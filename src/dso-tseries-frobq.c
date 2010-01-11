/*** dso-tseries-frobq.c -- tick frobbing
 *
 * Copyright (C) 2009 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <pfack/uterus.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "unserding-private.h"
#include "tseries-private.h"

/* our queuing backend */
#include "fsarrqueue.h"

/* maximum number of frobs on the queue */
#define NFROBS		32

#if !defined UD_DEBUG
# define UD_DEBUG(args...)
#endif	/* UNSERSRV */

typedef struct frobq_s *frobq_t;
typedef struct frobjob_s *frobjob_t;

struct frobjob_s {
	tseries_t tser;
	dse16_t beg;
	dse16_t end;
};

struct frobq_s {
	/* our job queue */
	fsarrpq_t q;
};

/* the global frobq, i like the reentrant version tho */
static struct frobq_s __gfrobq, *gfrobq = &__gfrobq;


/* useful addition to fsarrpq_t */
/**
 * Enqueue the cell pointed to by DATA into Q if not already in there. */
static bool
fsarrpq_enq_ifnot(fsarrpq_t q, void *data)
{
/* enqueue takes one item from the free list (dequeues there)
 * and puts it after tail */
	fsaqi_t qi;

	pthread_mutex_lock(&q->mtx);
	/* check if there's room in the queue */
	if (UNLIKELY(q->free == NULL)) {
		pthread_mutex_unlock(&q->mtx);
		return false;
	}
	/* have a glance at the queue so far, befriend with memcmp() */
	for (qi = q->head; qi; qi = qi->next) {
		if (memcmp(qi->data, data, q->csz) == 0) {
			pthread_mutex_unlock(&q->mtx);
			return false;
		}
	}
	/* otherwise, long way to go */
	/* dequeue from free list */
	qi = q->free;
	q->free = qi->next;
	qi->next = NULL;
	/* copy the cell */
	memcpy(qi->data, data, q->csz);
	/* enqueue to our real queue */
	if (q->tail) {
		q->tail->next = qi;
		qi->prev = q->tail;
	} else {
		/* head must be NULL also */
		q->head = qi;
		qi->prev = NULL;
	}
	q->tail = qi;

	pthread_mutex_unlock(&q->mtx);
	return true;
}

/* mark the original enqueuer unused */
static bool __attribute__((unused)) fsarrpq_enq(fsarrpq_t q, void *data);


/* queue handling */
static bool
frobq_deq(frobjob_t j, frobq_t q)
{
	return fsarrpq_deq(q->q, j);
}

static bool
frobq_enq(frobq_t q, tseries_t tser, dse16_t refds)
{
	struct frobjob_s j = {.tser = tser, .beg = refds, .end = refds + 13};

	return fsarrpq_enq_ifnot(q->q, &j);
}

/* the actual frobnication */
static void
__frobnicate(void *UNUSED(clo))
{
/* should now be in the worker thread */
	struct frobjob_s j;
	while (frobq_deq(&j, gfrobq)) {
#if 0
		struct tser_pkt_s pkt;
		if (j.tser->fetch_cb(&pkt, j.tser, j.beg, j.end) == 0) {
			/* we should massage the URN here so that this
			 * particular slot is never retried */
			return;
		}
		tseries_add(j.tser, j.beg, j.end, &pkt);
#else
		UD_DEBUG("stuff on the frobq ... doing, nothing\n");
#endif
	}
	return;
}


/* exports */
static void
init_frobq(frobq_t q)
{
	q->q = make_fsarrpq(NFROBS, sizeof(struct frobjob_s));
	return;
}

static void
free_frobq(frobq_t q)
{
	/* free our event queue */
	free_fsarrpq(q->q);
	return;
}

void
frobnicate(void)
{
	wpool_enq(gwpool, __frobnicate, NULL, true);
	return;
}

void
defer_frob(tseries_t tser, dse16_t refds, bool immediatep)
{
	frobq_enq(gfrobq, tser, refds);
	if (immediatep) {
		frobnicate();
	}
	return;
}


void
dso_tseries_frobq_LTX_init(void *UNUSED(clo))
{
	init_frobq(gfrobq);
	return;
}

void
dso_tseries_frobq_LTX_deinit(void *UNUSED(clo))
{
	free_frobq(gfrobq);
	return;
}

/* dso-tseries-frobq.c ends here */
