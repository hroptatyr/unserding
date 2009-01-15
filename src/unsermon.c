/*** unsermon.c -- unserding network monitor
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

FILE *logout;


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


static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

/* worker magic */
/* round robin var */
static index_t rr_wrk = 0;

/* the global job queue */
static struct job_queue_s __glob_jq = {
	.ji = 0, .mtx = PTHREAD_MUTEX_INITIALIZER
};
job_queue_t glob_jq;


inline void __attribute__((always_inline, gnu_inline))
trigger_job_queue(void)
{
	ev_async_send(EV_DEFAULT_ glob_notify);
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


static void
worker_cb(EV_P_ ev_async *w, int revents)
{
	job_t j;

	for (unsigned short int i = 0; i < NJOBS; i++) {
		pthread_mutex_lock(&glob_jq->mtx);
		j = &glob_jq->jobs[i];
		if (LIKELY(j->readyp != 0)) {
			j->readyp = 0;
		}
		pthread_mutex_unlock(&glob_jq->mtx);
		/* race condition!!! */
		if (LIKELY(j->workf != NULL)) {
			j->workf(j);
		}
		if (LIKELY(j->prntf != NULL)) {
			j->prntf(j);
		}
		free_job(j);
	}
	return;
}


/* interests module husk */
extern void __attribute__((weak)) init_interests(void);
void __attribute__((weak))
init_interests(void)
{
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

	/* where to log */
	logout = stderr;

	/* initialise global job q */
	init_glob_jq();

	/* attach a multicast listener */
	ud_attach_mcast4(EV_A);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a wakeup handler */
	glob_notify = &__wakeup_watcher;
	ev_async_init(glob_notify, worker_cb);
	ev_async_start(EV_A_ glob_notify);

	/* reset the round robin var */
	rr_wrk = 0;
	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* close the socket */
	ud_detach_mcast4(EV_A);

	/* destroy the default evloop */
	ev_default_destroy();

	/* close log file */
	fflush(logout);
	fclose(logout);
	/* unloop was called, so exit */
	return 0;
}

/* unsermon.c ends here */
