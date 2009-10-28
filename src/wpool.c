/*** wpool.c -- unserding worker pool
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#include <pthread.h>
#include "unserding-nifty.h"
#include "wpool.h"
#include "arrqueue.h"

typedef struct __worker_s *__worker_t;

struct __worker_s {
	pthread_t ALGN16(thread);
	/** the loop we're talking */
	struct ev_loop *loop;
} __attribute__((aligned(16)));

struct __wpool_s {
	/* the queue for the workers */
	arrpq_t wq;
};
#define AS_WPOOL(_x)	((struct __wpool_s*)(_x))

/* round robin var */
static index_t rr_wrk = 0;
/* the workers array */
static struct __worker_s __attribute__((aligned(16))) workers[MAX_WORKERS];


/* the gory stuff */
static void*
worker(void *wk)
{
	long int UNUSED(self) = pthread_self();
#if 0
	void *loop = worker_loop(wk);
	size_t iter = 0;

	lolo_lock(EV_A);
	UD_DEBUG("starting worker thread %lx, loop %p\n", self, loop);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	ev_loop(EV_A_ 0);
	UD_DEBUG("quitting worker thread %lx, loop %p\n", self, loop);
	lolo_unlock(EV_A);
#endif
#if 0
	wpjob_t j = wpool_deq();
	j->workf(j->closure);
#endif
	return NULL;
}


static void
add_worker(struct ev_loop *loop)
{
	pthread_attr_t attr;
	__worker_t wk = &workers[rr_wrk++];

	/* load off our loop */
	wk->loop = loop;
	/* initialise thread attributes */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* start the thread now */
	pthread_create(&wk->thread, &attr, worker, wk);

	/* destroy locals */
	pthread_attr_destroy(&attr);
	return;
}

static void
kill_worker(__worker_t wk)
{
	/* send a lethal signal to the workers and detach */
	ev_async_send(worker_loop(wk), worker_killw(wk));
	return;
}


/* exports */
wpool_t
make_wpool(int nworkers, int njobs)
{
	//AS_WPOOL(res)->wq = make_arrpq(njobs);
	return NULL;
}

void
kill_wpool(wpool_t p)
{
	return;
}


/* queue handling */
void
wpool_enq(wpool_t p, wpjob_t j, bool triggerp)
{
	/* enqueue in the worker queue */
	arrpq_enqueue(AS_WPOOL(p)->wq, j);
	return;
}

void
wpool_trigger(wpool_t p)
{
	return;
}

#if 0
static void*
init_secondary(void)
{
	ev_async *evw = &work_watcher;
	ev_async *evk = &kill_watcher;
	struct ev_loop *res = ev_loop_new(EVFLAG_AUTO);
	static struct ud_loopclo_s clo;

	/* prepare the closure */
	pthread_mutex_init(&clo.lolo, 0);
	pthread_cond_init(&clo.loco, 0);
	/* announce our cruft */
	ev_set_userdata(res, &clo);
	ev_set_loop_release_cb(res, lolo_unlock, lolo_lock);
	//ev_set_invoke_pending_cb(res, worker_invoke);
	/* prepare watchers */
#if 0
	ev_async_init(evw, worker_cb);
	ev_async_start(res, evw);
#endif
	ev_async_init(evk, kill_cb);
	ev_async_start(res, evk);
	return res;
}

for (int i = 0; i < nworkers; i++) {
		UD_DEBUG("killing worker %lu\n", (long unsigned int)i - 1);
		add_worker(&workers[i-1]);
	}


	/* kill the workers along with their secondary loops */
	for (int i = nworkers; i > 0; i--) {
		UD_DEBUG("killing worker %lu\n", (long unsigned int)i - 1);
		kill_worker(&workers[i-1]);
	}
	for (int i = nworkers; i > 0; i--) {
		UD_DEBUG("gathering worker %lu\n", (long unsigned int)i - 1);
		pthread_join(workers[i-1].thread, NULL);
	}
	/* destroy the secondary loop */
	ev_loop_destroy(secl);

#endif

/* wpool.c ends here */
