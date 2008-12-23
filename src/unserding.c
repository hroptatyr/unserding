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
#define UNLIKELY

ev_io stdin_watcher;
ev_timer timeout_watcher;
ev_idle idle_watcher;

struct addrinfo glob_sa;


/* dealers */
static void
tcpudp_handle_new(int sock)
{
	volatile int ns;
	struct sockaddr_in6 sa;
	socklen_t sa_size = sizeof(sa);
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;

	ns = accept(sock, (struct sockaddr *)&sa, &sa_size);
	if (ns < 0) {
		INPUT_CRITICAL_TCPUDP("could not handle incoming connection\n");
		return;
	}
	/* obtain the address in human readable form */
	a = inet_ntop(sa.sin6_family, &sa.sin6_addr, buf, sizeof(buf));
	p = ntohs(sa.sin6_port);

	INPUT_DEBUG_TCPUDP("Server: connect from host %s, port %d.\n", a, p);
	return;
}


/* this callback is called when data is readable on stdin */
static void
stdin_cb(EV_P_ ev_io *w, int revents)
{
	INPUT_DEBUG_TCPUDP("incoming connection\n");
	return;
}

static void
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
	volatile int s, port = 0;
	int retval;

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

	if (port == 0) {
		int gni;
		char servbuf[NI_MAXSERV];

		gni = getnameinfo(lres->ai_addr,
				  lres->ai_addrlen, NULL,
				  0, servbuf,
				  sizeof(servbuf),
				  NI_NUMERICSERV);

		if (gni == 0) {
			port = atoi(servbuf);
		}
	}
	INPUT_DEBUG_TCPUDP("listening on port %d\n", port);
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


int
main (void)
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop = ev_default_loop(0);
	int lsock = tcpudp_listener_init();

	/* initialise an io watcher, then start it
	 * this one will watch for stdin to become readable */
	ev_io_init(&stdin_watcher, stdin_cb, lsock, EV_READ);
	ev_io_start(loop, &stdin_watcher);

	/* initialise a timer watcher, then start it
	 * simple non-repeating 15.5 second timeout */
	ev_timer_init(&timeout_watcher, timeout_cb, 15.5, 0.);
	ev_timer_start(loop, &timeout_watcher);

#if 0
	/* initialise an idler */
	ev_idle_init(&idle_watcher, idle_cb);
	ev_idle_start(loop, &idle_watcher);
#endif

	/* now wait for events to arrive */
	ev_loop(loop, 0);

	/* close the socket */
	tcpudp_listener_deinit(lsock);

	/* unloop was called, so exit */
	return 0;
}

/* unserding.c ends here */
