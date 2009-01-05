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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>

#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif

/* our master include file */
#include "unserding.h"
/* our private bits */
#include "unserding-private.h"

#define USE_COROUTINES		1


typedef struct ud_worker_s *ud_worker_t;
typedef struct ud_ev_async_s ud_ev_async;

/* our version of the async event, cdr-coding */
struct ud_ev_async_s {
	struct ev_async super;
};

struct ud_worker_s {
	pthread_t ALGN16(thread);
#if !USE_COROUTINES
	/* the loop we live on */
	struct ev_loop *loop;
	/* a watcher for worker jobs */
	struct ev_async ALGN16(work_watcher);
	/* a watcher for harakiri orders */
	struct ev_async ALGN16(kill_watcher);
#endif	/* !USE_COROUTINES */
} __attribute__((aligned(16)));


static index_t __attribute__((unused)) glob_idx = 0;
static struct conn_ctx_s glob_ctx[64];

static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

/* worker magic */
#define NWORKERS		4
/* round robin var */
static index_t rr_wrk = 0;
/* the workers array */
static struct ud_worker_s __attribute__((aligned(16))) workers[NWORKERS];

#if USE_COROUTINES
static struct ev_loop *secl;
/* a watcher for worker jobs */
struct ev_async ALGN16(work_watcher);
/* a watcher for harakiri orders */
struct ev_async ALGN16(kill_watcher);
#endif	/* USE_COROUTINES */

static inline struct ev_loop __attribute__((always_inline, gnu_inline)) *
worker_loop(ud_worker_t wk)
{
#if USE_COROUTINES
	return secl;
#else  /* !USE_COROUTINES */
	return wk->loop;
#endif	/* USE_COROUTINES */
}

static inline struct ev_async __attribute__((always_inline, gnu_inline)) *
worker_workw(ud_worker_t wk)
{
#if USE_COROUTINES
	return &work_watcher;
#else  /* !USE_COROUTINES */
	return &wk->work_watcher;
#endif	/* USE_COROUTINES */
}

static inline struct ev_async __attribute__((always_inline, gnu_inline)) *
worker_killw(ud_worker_t wk)
{
#if USE_COROUTINES
	return &kill_watcher;
#else  /* !USE_COROUTINES */
	return &wk->kill_watcher;
#endif	/* USE_COROUTINES */
}

/* the global job queue */
static struct job_queue_s __glob_jq = {
	.ri = 0, .wi = 0, .mtx = PTHREAD_MUTEX_INITIALIZER
};
job_queue_t glob_jq;


inline void __attribute__((always_inline, gnu_inline))
trigger_job_queue(void)
{
	/* look what we can do */
#if USE_COROUTINES
	/* easy */
	ev_async_send(secl, &work_watcher);
#else  /* !USE_COROUTINES */
	/* resort to our round robin */
	ev_async_send(workers[rr_wrk].loop, &workers[rr_wrk].work_watcher);
	/* step the round robin */
	rr_wrk = (rr_wrk + 1) % NWORKERS;
#endif
	return;
}

static const char emer_msg[] = "unserding has been shut down, cya mate!\n";

static void
sigint_cb(EV_P_ ev_signal *w, int revents)
{
	UD_DEBUG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

conn_ctx_t
find_ctx(void)
{
/* returns the next free context */
	for (index_t res = 0; res < countof(glob_ctx); res++) {
		if (glob_ctx[res].snk == -1) {
			return &glob_ctx[res];
		}
	}
	return NULL;
}


/* connexion goodness */
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
		UD_CRITICAL("Unable to open multicast socket\n");
		return -1;
	}

	saddr.sin_family = PF_INET;
	saddr.sin_port = htons(8653);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(s, (struct sockaddr*)&saddr, sizeof(struct sockaddr_in));

	if (retval < 1) {
		UD_CRITICAL("Unable to bind multicast socket\n");
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
triv_cb(EV_P_ ev_async *w, int revents)
{
	return;
}

static void
kill_cb(EV_P_ ev_async *w, int revents)
{
	long int self = (long int)pthread_self();
	UD_DEBUG("SIGQUIT caught in %lx\n", self);
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
worker_cb(EV_P_ ev_async *w, int revents)
{
	void *self = (void*)(long int)pthread_self();
	job_t j;

	while ((j = dequeue_job(glob_jq)) != NO_JOB) {
		UD_DEBUG("thread/loop %p/%p doing work %p\n", self, loop, j);
		if (UNLIKELY(j == NULL)) {
			continue;
		}
		j->workf(j);
		free_job(j);
	}
	UD_DEBUG("no more jobs %p/%p\n", self, loop);
	return;
}

static void*
worker(void *wk)
{
	long int self = pthread_self();
	void *loop = worker_loop(wk);
	UD_DEBUG("starting worker thread %lx, loop %p\n", self, loop);
	ev_loop(EV_A_ 0);
	UD_DEBUG("quitting worker thread %lx, loop %p\n", self, loop);
	return NULL;
}

static void
add_worker(struct ev_loop *loop)
{
	pthread_attr_t attr;
	ud_worker_t wk = &workers[rr_wrk++];

	/* initialise thread attributes */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* use the existing  */
#if !USE_COROUTINES
	wk->loop = loop;
	{
		ev_async *eva = &wk->work_watcher;
		ev_async_init(eva, worker_cb);
		ev_async_start(wk->loop, eva);
	}
	{
		ev_async *eva = &wk->kill_watcher;
		ev_async_init(eva, kill_cb);
		ev_async_start(wk->loop, eva);
	}
#endif	/* !USE_COROUTINES */

	/* start the thread now */
	pthread_create(&wk->thread, &attr, worker, wk);

	/* destroy locals */
	pthread_attr_destroy(&attr);
	return;
}

static void
kill_worker(ud_worker_t wk)
{
	/* send a lethal signal to the workers and detach */
	ev_async_send(worker_loop(wk), worker_killw(wk));
#if !USE_COROUTINES
	pthread_join(wk->thread, NULL);
	ev_loop_destroy(worker_loop(wk));
#endif
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

static void
init_glob_jq(void)
{
	glob_jq = &__glob_jq;
	return;
}

int
main (void)
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop = ev_default_loop(0);
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;

	/* initialise the global context */
	init_glob_ctx();
	/* initialise global job q */
	init_glob_jq();

	/* attach a tcp listener */
	ud_attach_tcp6(EV_A);

	/* initialise instruments */
	init_instr();
	/* initialise interests module */
	(void)init_interests();

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a wakeup handler */
	glob_notify = &__wakeup_watcher;
	ev_async_init(glob_notify, triv_cb);
	ev_async_start(EV_A_ glob_notify);

#if USE_COROUTINES
	/* create one loop for all threads */
	secl = ev_loop_new(0);
	{
		ev_async *eva = &work_watcher;
		ev_async_init(eva, worker_cb);
		ev_async_start(secl, eva);
	}
	{
		ev_async *eva = &kill_watcher;
		ev_async_init(eva, kill_cb);
		ev_async_start(secl, eva);
	}
#endif	/* USE_COROUTINES */
	/* set up the worker threads along with their secondary loops */
	for (index_t i = 0; i < NWORKERS; i++) {
#if !USE_COROUTINES
		/* in case of no coroutines create the secondary loop here */
		struct ev_loop *secl = ev_loop_new(0);
#endif	/* !USE_COROUTINES */
		add_worker(secl);
	}

	/* reset the round robin var */
	rr_wrk = 0;
	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* kill all open connexions */
	for (index_t res = 0; res < countof(glob_ctx); res++) {
		if (LIKELY(glob_ctx[res].snk == -1)) {
			continue;
		}
		UD_DEBUG("letting %d know\n", glob_ctx[res].snk);
		write(glob_ctx[res].snk, emer_msg, countof(emer_msg) - 1);
		ud_kick_tcp6(EV_A_ &glob_ctx[res]);
	}

	/* close the socket */
	ud_detach_tcp6(EV_A);

	/* kill the workers along with their secondary loops */
	for (index_t i = NWORKERS; i > 0; i--) {
		UD_DEBUG("killing worker %lu\n", (long unsigned int)i - 1);
		kill_worker(&workers[i-1]);
	}
#if USE_COROUTINES
	for (index_t i = NWORKERS; i > 0; i--) {
		UD_DEBUG("killing worker %lu\n", (long unsigned int)i - 1);
		kill_worker(&workers[i-1]);
		usleep(10000);
	}
	for (index_t i = NWORKERS; i > 0; i--) {
		UD_DEBUG("gathering worker %lu\n", (long unsigned int)i - 1);
		pthread_join(workers[i-1].thread, NULL);
	}
	/* destroy the secondary loop */
	ev_loop_destroy(secl);
#endif	/* USE_COROUTINES */

	/* destroy the default evloop */
	ev_default_destroy();

	/* unloop was called, so exit */
	return 0;
}

/* unserding.c ends here */
