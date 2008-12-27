/*** unserding.c -- unserding network service
 *
 * Copyright (C) 2008 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <ev.h>

#if defined HAVE_SYS_SOCKET_H || 1
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H || 1
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H || 1
# include <arpa/inet.h>
#endif
#if defined HAVE_NETDB_H || 1
# include <netdb.h>
#endif
#if defined HAVE_SYS_UN_H || 1
# include <sys/un.h>
#endif
#if defined HAVE_ERRNO_H || 1
# include <errno.h>
#endif

#define INPUT_CRITICAL_TCPUDP(args...)			\
	fprintf(stderr, "[unserding/input/tcpudp] CRITICAL " args)
#define INPUT_DEBUG_TCPUDP(args...)			\
	fprintf(stderr, "[unserding/input/tcpudp] " args)
#define TCPUDP_TIMEOUT		5

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#define UNUSED(_x)	__attribute__((unused)) _x

#define countof(x)		(sizeof(x) / sizeof(*x))

/* The encoded parameter sizes will be rounded up to match pointer alignment. */
#define ROUND(s, a)		(a * ((s + a - 1) / a))
#define aligned_sizeof(t)	ROUND(sizeof(t), __alignof(void*))

typedef size_t index_t;
typedef struct conn_ctx_s *conn_ctx_t;
typedef struct outbuf_s *outbuf_t;

struct conn_ctx_s {
	struct ev_io __attribute__((aligned(16))) io;
	struct ev_timer __attribute__((aligned(16))) ti;
	int src;
	int snk;
	index_t bidx;
	char buf[1024 - 32 - sizeof(struct ev_timer) - sizeof(struct ev_io)]
	__attribute__((aligned(16)));
} __attribute__((aligned(1024)));

struct outbuf_s {
	struct ev_io __attribute__((aligned(16))) io;
	size_t buflen;
	const char *buffer;
};

static inline conn_ctx_t __attribute__((always_inline, gnu_inline))
ev_io_ctx(void *io)
{
	return (void*)io;
}

static inline conn_ctx_t __attribute__((always_inline, gnu_inline))
ev_timer_ctx(void *timer)
{
	return (void*)((char*)timer - aligned_sizeof(struct ev_io));
}

static inline void __attribute__((always_inline, gnu_inline)) *
ctx_io(conn_ctx_t ctx)
{
	return (void*)&ctx->io;
}

static inline void __attribute__((always_inline, gnu_inline)) *
ctx_timer(conn_ctx_t ctx)
{
	return (void*)&ctx->ti;
}

static index_t __attribute__((unused)) glob_idx = 0;
static struct conn_ctx_s glob_ctx[64];
static struct addrinfo glob_sa;
static struct sockaddr_in UNUSED(glob_mul);

static ev_io __srv_watcher __attribute__((aligned(16)));
static ev_io __srvmul_watcher __attribute__((aligned(16)));
static ev_signal __sigint_watcher __attribute__((aligned(16)));
static ev_signal __sigpipe_watcher __attribute__((aligned(16)));

/* protos */
static void tcpudp_listener_deinit(int sock);


static inline void __attribute__((always_inline))
tcpudp_kick_ctx(EV_P_ conn_ctx_t ctx)
{
	ev_io *evio = ctx_io(ctx);
	ev_timer *evti = ctx_timer(ctx);

	/* kick the timer */
	ev_timer_stop(EV_A_ evti);
	/* kick the io handler */
	ev_io_stop(EV_A_ evio);
	/* stop the bugger */
	tcpudp_listener_deinit(ctx->snk);
	/* finally, give the ctx struct a proper rinse */
	ctx->src = ctx->snk = -1;
	ctx->bidx = 0;
	return;
}

static inline void
tcpudp_ctx_renew_timer(EV_P_ conn_ctx_t ctx)
{
	ev_timer *evti = ctx_timer(ctx);

	ev_timer_stop(EV_A_ evti);
	ev_timer_set(evti, TCPUDP_TIMEOUT, 0.0);
	ev_timer_start(EV_A_ evti);
	return;
}

/* this callback is called when data is readable on one of the polled socks */
static void
tcpudp_traf_rcb(EV_P_ ev_io *w, int revents)
{
	size_t nread;
	/* our brilliant type pun */
	struct conn_ctx_s *ctx = (void*)w;

	INPUT_DEBUG_TCPUDP("traffic on %d\n", w->fd);
	nread = read(w->fd, &ctx->buf[ctx->bidx],
		     countof(ctx->buf) - ctx->bidx);
	if (UNLIKELY(nread == 0)) {
		tcpudp_kick_ctx(EV_A_ ctx);
		return;
	}
	/* prolongate the timer a wee bit */
	tcpudp_ctx_renew_timer(EV_A_ ctx);
	/* wind the buffer index */
	ctx->bidx += nread;
	return;
}

/* this callback is called when data is writable on one of the polled socks */
static void __attribute__((unused))
tcpudp_traf_wcb(EV_P_ ev_io *w, int revents)
{
	outbuf_t ob = (void*)w;
	write(w->fd, ob->buffer, ob->buflen);
	ev_io_stop(EV_A_ w);
	return;
}

static const char idle_msg[] = "Oi shitface, stop wasting my time, will ya!\n";
static const char emer_msg[] = "unserding has been shut down, cya mate!\n";

static void
tcpudp_idleto_cb(EV_P_ ev_timer *w, int revents)
{
	/* our brilliant type pun */
	struct conn_ctx_s *ctx = ev_timer_ctx(w);

	INPUT_DEBUG_TCPUDP("bitching back at the eejit on %d\n", ctx->snk);
	write(ctx->snk, idle_msg, countof(idle_msg));

	/* just finish him off */
	tcpudp_kick_ctx(EV_A_ ctx);
	return;
}

static void
sigint_cb(EV_P_ ev_signal *w, int revents)
{
	INPUT_DEBUG_TCPUDP("C-c caught, unrolling everything\n");
	/* kill all open connexions */
	for (index_t res = 0; res < countof(glob_ctx); res++) {
		if (LIKELY(glob_ctx[res].snk == -1)) {
			continue;
		}
		INPUT_DEBUG_TCPUDP("letting %d know\n", glob_ctx[res].snk);
		write(glob_ctx[res].snk, emer_msg, countof(emer_msg));
		tcpudp_kick_ctx(EV_A_ &glob_ctx[res]);
	}
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static inline index_t
find_ctx(void)
{
/* returns the next free context */
	for (index_t res = 0; res < countof(glob_ctx); res++) {
		if (glob_ctx[res].snk == -1) {
			return res;
		}
	}
	return -1;
}

/* this callback is called when data is readable on the main server socket */
static void
tcpudp_inco_cb(EV_P_ ev_io *w, int revents)
{
	volatile int ns;
	struct sockaddr_in6 sa;
	socklen_t sa_size = sizeof(sa);
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	index_t widx;
	ev_io *watcher;
	ev_timer *to;

	INPUT_DEBUG_TCPUDP("incoming connection\n");

	ns = accept(w->fd, (struct sockaddr *)&sa, &sa_size);
	if (ns < 0) {
		INPUT_CRITICAL_TCPUDP("could not handle incoming connection\n");
		return;
	}
	/* obtain the address in human readable form */
	a = inet_ntop(sa.sin6_family, &sa.sin6_addr, buf, sizeof(buf));
	p = ntohs(sa.sin6_port);

	INPUT_DEBUG_TCPUDP("Server: connect from host %s, port %d.\n", a, p);

	/* initialise an io watcher, then start it */
	widx = find_ctx();

	/* escrow the socket poller */
	watcher = ctx_io(&glob_ctx[widx]);
	ev_io_init(watcher, tcpudp_traf_rcb, ns, EV_READ);
	ev_io_start(EV_A_ watcher);

	/* escrow the connexion idle timeout */
	to = ctx_timer(&glob_ctx[widx]);
	ev_timer_init(to, tcpudp_idleto_cb, TCPUDP_TIMEOUT, 0.);
	ev_timer_start(EV_A_ to);

	/* put the src and snk sock into glob_ctx */
	glob_ctx[widx].src = w->fd;
	glob_ctx[widx].snk = ns;
	return;
}

static void __attribute__((unused))
timeout_cb(EV_P_ ev_timer *w, int revents)
{
	puts("timeout");
	/* this causes the innermost ev_loop to stop iterating */
	ev_unloop(EV_A_ EVUNLOOP_ONE);
}

static void __attribute__((unused))
idle_cb(EV_P_ ev_idle *w, int revents)
{
	puts("idle");
}

static inline void
__linger_sock(int sock)
{
#if defined SO_LINGER || 1
	struct linger lng;

	lng.l_onoff = 1;	/* 1 == on */
	lng.l_linger = 1;	/* linger time in seconds */

	INPUT_DEBUG_TCPUDP("setting option SO_LINGER for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) < 0) {
		INPUT_CRITICAL_TCPUDP("setsockopt(SO_LINGER) failed\n");
	}
#endif
	return;
}

static inline void
__reuse_sock(int sock)
{
	const int one = 1;

#if defined SO_REUSEADDR || 1
	INPUT_DEBUG_TCPUDP("setting option SO_REUSEADDR for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
		INPUT_CRITICAL_TCPUDP("setsockopt(SO_REUSEADDR) failed\n");
	}
#else
# error "Go away!"
#endif
#if defined SO_REUSEPORT
	INPUT_DEBUG_TCPUDP("setting option SO_REUSEPORT for sock %d\n", sock);
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0) {
		INPUT_CRITICAL_TCPUDP("setsockopt(SO_REUSEPORT) failed\n");
	}
#endif
	return;
}


/* connexion goodness */
static int
_tcpudp_listener_try(volatile struct addrinfo *lres)
{
	volatile int s;
	int retval;
	char servbuf[NI_MAXSERV];

	s = socket(lres->ai_family, SOCK_STREAM, 0);
	if (s < 0) {
		INPUT_CRITICAL_TCPUDP("socket() failed, whysoever\n");
		return s;
	}
	__reuse_sock(s);

	/* we used to retry upon failure, but who cares */
	retval = bind(s, lres->ai_addr, lres->ai_addrlen);
	if (retval >= 0 ) {
		retval = listen(s, 5);
	}
	if (UNLIKELY(retval == -1)) {
		INPUT_CRITICAL_TCPUDP("bind() failed, whysoever\n");
		if (errno != EISCONN) {
			close(s);
			return -1;
		}
	}

	if (getnameinfo(lres->ai_addr, lres->ai_addrlen, NULL,
			0, servbuf, sizeof(servbuf), NI_NUMERICSERV) == 0) {
		INPUT_DEBUG_TCPUDP("listening on port %s\n", servbuf);
	}
	return s;
}

static int
tcpudp_listener_init(void)
{
	struct addrinfo *res;
	const struct addrinfo hints = {
		.ai_family = AF_INET6,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		/* specify to whom we listen */
		.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ALL,
	};
	int retval;
	volatile int s;

#if defined HAVE_GETADDRINFO || 1
	retval = getaddrinfo("::", "8653", &hints, &res);
#else
# error How the fuck did you reach this?!
#endif
	if (retval != 0) {
		/* abort(); */
		return -1;
	}
	for (struct addrinfo *lres = res; lres; lres = lres->ai_next) {
		if ((s = _tcpudp_listener_try(lres)) >= 0) {
			INPUT_DEBUG_TCPUDP("found service %d on sock %d\n",
					   lres->ai_protocol, s);
			memcpy(&glob_sa, lres, sizeof(struct addrinfo));
			break;
		}
	}

	freeaddrinfo(res);
	/* succeeded if > 0 */
	return s;
}

#if 0
static int
tcpudp_mlistener_init(void)
{
	struct addrinfo *res;
	const struct addrinfo hints = {
		.ai_family = AF_INET6,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
		/* specify to whom we listen */
		.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ALL,
	};
	int retval;
	volatile int s;
	struct sockaddr_in saddr;
	struct ip_mreq imreq;

	/* set content of struct saddr and imreq to zero */
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	memset(&imreq, 0, sizeof(struct ip_mreq));

	/* open a UDP socket */
	s = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (s < 0) {
		INPUT_CRITICAL_TCPUDP("Unable to open multicast socket\n");
		return -1;
	}

	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(8653);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(s, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));

	if (retval < 1) {
		INPUT_CRITICAL_TCPUDP("Unable to bind multicast socket\n");
		return -1;
	}

	imreq.imr_multiaddr.s_addr = inet_addr("224.0.0.1");
	imreq.imr_interface.s_addr = INADDR_ANY;

	/* set TTL of multicast packet */
	retval = setsockopt(s,
			    saddr.ai_family == PF_INET6 ? IPPROTO_IPV6 : IPPROTO_IP,
			    saddr.ai_family == PF_INET6 ? IPV6_MULTICAST_HOPS : IP_MULTICAST_TTL,
			    (char*) &multicastTTL, sizeof(multicastTTL));

	if (retval != 0) {
		abort();
	}

	/* succeeded if > 0 */
	return s;
}
#endif

static void
tcpudp_listener_deinit(int sock)
{
	/* linger the sink sock */
	__linger_sock(sock);
	INPUT_DEBUG_TCPUDP("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return;
}


#define TICK_CYCLES		256
#define NWORKER_THREADS		4
static index_t nworkers = 0;
static pthread_t workers[NWORKER_THREADS];
static struct ev_loop *loops[NWORKER_THREADS];
/* the ticker runs TICK_CYCLES times as fast as the main server tick */
static ev_periodic worker_watcher[NWORKER_THREADS];
static ev_async wksig_watcher[NWORKER_THREADS];

static void
wksig_cb(EV_P_ ev_async *w, int revents)
{
	void *self = (void*)(long int)pthread_self();
	INPUT_DEBUG_TCPUDP("SIGQUIT caught in %p\n", self);
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
worker_cb(EV_P_ ev_periodic *w, int revents)
{
	void *self = (void*)(long int)pthread_self();
	INPUT_DEBUG_TCPUDP("thread %p doing work\n", self);
	return;
}

static void*
worker(void *loop)
{
	void *self = (void*)(long int)pthread_self();
	INPUT_DEBUG_TCPUDP("starting worker thread %p, loop %p\n", self, loop);
	ev_loop(EV_A_ 0);
	INPUT_DEBUG_TCPUDP("quitting worker thread %p, loop %p\n", self, loop);
	return NULL;
}

static void
add_worker(void)
{
	pthread_attr_t attr;
	void *loop;

	/* initialise thread attributes */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* new thread-local loop */
	loop = loops[nworkers] = ev_loop_new(0);

	ev_periodic_init(&worker_watcher[nworkers],
			 worker_cb, 0, TICK_CYCLES, 0);
	ev_periodic_start(loop, &worker_watcher[nworkers]);

	ev_async_init(&wksig_watcher[nworkers], wksig_cb);
	ev_async_start(loop, &wksig_watcher[nworkers]);

	/* start the thread now */
	pthread_create(&workers[nworkers++], &attr, worker, loop);

	/* destroy locals */
	pthread_attr_destroy(&attr);
	return;
}

static void
kill_worker(index_t w)
{
	void *ignore;
	ev_async_send(loops[w], wksig_watcher);
	pthread_join(workers[w], &ignore);
	return;
}


static void
init_glob_ctx(void)
{
	for (index_t i = 0; i < countof(glob_ctx); i++) {
		glob_ctx[i].snk = -1;
	}
	return;
}

int
main (void)
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop = ev_default_loop(0);
	int lsock = tcpudp_listener_init();
#if 0
	int msock = tcpudp_mlistener_init();
#endif
	ev_io *srv_watcher = &__srv_watcher;
	ev_io *UNUSED(srvmul_watcher) = &__srvmul_watcher;
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;

	/* initialise the global context */
	init_glob_ctx();

	/* initialise an io watcher, then start it */
	ev_io_init(srv_watcher, tcpudp_inco_cb, lsock, EV_READ);
	ev_io_start(EV_A_ srv_watcher);
#if 0
	/* initialise a multicast watcher */
	ev_io_init(srvmul_watcher, tcpudp_inco_cb, msock, EV_READ);
	ev_io_start(EV_A_ srvmul_watcher);
#endif

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);

	/* set up the worker threads along with their secondary loops */
	for (index_t i = 0; i < NWORKER_THREADS; i++) {
		add_worker();
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* close the socket */
	tcpudp_listener_deinit(lsock);

	/* kill the workers along with their secondary loops */
	for (index_t i = 0; i < NWORKER_THREADS; i++) {
		kill_worker(i);
	}

	/* unloop was called, so exit */
	return 0;
}

/* unserding.c ends here */
