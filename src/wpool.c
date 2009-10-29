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
#if !defined UD_DEBUG
# define UD_DEBUG(args...)
#endif	/* UNSERSRV */

/* our queuing backend */
#include "fsarrqueue.h"

typedef struct __wpool_s *__wpool_t;
typedef struct __worker_s *__worker_t;
typedef struct wpjob_s *wpjob_t;

struct wpjob_s {
	wpool_work_f cb;
	void *clo;
};

struct __worker_s {
	pthread_t ALGN16(thread);
	wpool_t pool;
} __attribute__((aligned(16)));

struct __wpool_s {
	/* our job queue */
	fsarrpq_t q;
	/* worker signal */
	pthread_mutex_t mtx;
	pthread_cond_t sem;
	/* workers array */
	int nworkers;
	struct __worker_s __attribute__((aligned(16))) workers[];
};
#define AS_WPOOL(_x)	((struct __wpool_s*)(_x))


/* queue handling */
static inline bool
wpool_deq(wpjob_t j, wpool_t p)
{
	return fsarrpq_deq(AS_WPOOL(p)->q, j);
}

bool
wpool_enq(wpool_t p, wpool_work_f cb, void *clo, bool triggerp)
{
	struct wpjob_s j = {.cb = cb, .clo = clo};
	bool res;

	res = fsarrpq_enq(AS_WPOOL(p)->q, &j);
	if (triggerp || !res) {
		wpool_trigger(p);
	}
	return res;
}

void
wpool_trigger(wpool_t p)
{
	pthread_mutex_lock(&AS_WPOOL(p)->mtx);
	pthread_cond_signal(&AS_WPOOL(p)->sem);
	pthread_mutex_unlock(&AS_WPOOL(p)->mtx);
	return;
}


/* the gory stuff */
static void*
worker(void *clo)
{
	long int UNUSED(self) = pthread_self();
	__worker_t wk = clo;
	__wpool_t wp = wk->pool;

	UD_DEBUG("starting worker thread %lx %p\n", self, clo);
	while (true) {
		struct wpjob_s wpj;
		pthread_mutex_lock(&wp->mtx);
		pthread_cond_wait(&wp->sem, &wp->mtx);
		pthread_mutex_unlock(&wp->mtx);
		UD_DEBUG("working %lx %p\n", self, clo);
		while (wpool_deq(&wpj, wp)) {
			if (wpj.cb == NULL && wpj.clo == (void*)0xdead) {
				/* we were ordered to die */
				goto bingo;
			}
			/* otherwise call the callback */
			wpj.cb(wpj.clo);
		}
		UD_DEBUG("work finished %lx %p\n", self, clo);
	}
bingo:
	UD_DEBUG("quitting worker thread %lx %p\n", self, clo);
	pthread_exit(NULL);
}


static void
add_worker(wpool_t wp, __worker_t wk)
{
	pthread_attr_t attr;

	/* initialise thread attributes */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* let the worker now about the pool */
	wk->pool = wp;
	/* start the thread now */
	pthread_create(&wk->thread, &attr, worker, wk);

	/* destroy locals */
	pthread_attr_destroy(&attr);
	return;
}

static void
kill_worker(wpool_t wp, __worker_t __attribute__((unused)) wk)
{
	/* send a lethal signal to the workers and detach */
	wpool_enq(wp, NULL, (void*)0xdead, true);
	return;
}


/* exports */
wpool_t
make_wpool(int nworkers, int njobs)
{
	size_t asz = nworkers * sizeof(struct __worker_s);
	wpool_t res = malloc(sizeof(struct __wpool_s) + asz);

	AS_WPOOL(res)->nworkers = nworkers;
	AS_WPOOL(res)->q = make_fsarrpq(njobs, sizeof(struct wpjob_s));

	pthread_mutex_init(&AS_WPOOL(res)->mtx, 0);
	pthread_cond_init(&AS_WPOOL(res)->sem, 0);
	for (int i = 0; i < nworkers; i++) {
		__worker_t wk = &AS_WPOOL(res)->workers[i];
		add_worker(res, wk);
	}
	return res;
}

void
kill_wpool(wpool_t p)
{
	/* kill the workers along with their secondary loops */
	for (int i = 0; i < AS_WPOOL(p)->nworkers; i++) {
		UD_DEBUG("killing worker %d\n", i);
		kill_worker(p, &AS_WPOOL(p)->workers[i]);
	}
	for (int i = 0; i < AS_WPOOL(p)->nworkers; i++) {
		UD_DEBUG("gathering worker %d\n", i);
		pthread_join(AS_WPOOL(p)->workers[i].thread, NULL);
	}
	/* destroy mutex and semaphore */
	pthread_mutex_lock(&AS_WPOOL(p)->mtx);
	pthread_mutex_unlock(&AS_WPOOL(p)->mtx);
	pthread_cond_destroy(&AS_WPOOL(p)->sem);
	pthread_mutex_destroy(&AS_WPOOL(p)->mtx);
	/* free our event queue */
	free_fsarrpq(AS_WPOOL(p)->q);
	return;
}

/* wpool.c ends here */
