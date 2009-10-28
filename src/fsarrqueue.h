/*** fsarrqueue.h -- helper for AoS queues
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

#if !defined INCLUDED_fsarrqueue_h_
#define INCLUDED_fsarrqueue_h_

#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined ALGN16
# define ALGN16(_x)	__attribute__((aligned(16))) _x
#endif	/* !ALGN16 */

#if !defined countof
# define countof(x)		(sizeof(x) / sizeof(*x))
#endif	/* countof */

#if !defined xmalloc
# define xmalloc(_x)	malloc(_x)
#endif	/* !xmalloc */
#if !defined xfree
# define xfree(_x)	free(_x)
#endif	/* !free */


/* since we need it so bad we provide an array based queue with fixed size
 * slots, thread-safe of course. */

/* fixed-size AoS queue, data inside. */
typedef struct fsarrpq_s *fsarrpq_t;
/* navigator cell */
typedef struct fsaqi_s *fsaqi_t;

struct fsaqi_s {
	fsaqi_t next;
	fsaqi_t prev;
	char ALGN16(data[]);
};

struct fsarrpq_s {
	/* queue head */
	fsaqi_t head;
	/* queue tail */
	fsaqi_t tail;
	/* free queue head */
	fsaqi_t free;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* cell size */
	unsigned int csz;
	/* queue size */
	unsigned int qsz;
	/* alloc size */
	unsigned int asz;
	/* data vector */
	char ALGN16(data[]);
};


static fsarrpq_t
make_fsarrpq(size_t queue_size, size_t cell_size)
{
	size_t nav_size = sizeof(struct fsaqi_s) + cell_size;
	fsarrpq_t res = xmalloc(sizeof(*res) + queue_size * nav_size);
	res->head = res->tail = NULL;
	res->csz = cell_size;
	res->qsz = 0;
	res->asz = queue_size;
	pthread_mutex_init(&res->mtx, NULL);
	/* initialise the free queue */
	for (size_t i = 1; i < queue_size; i++) {
		fsaqi_t qi = (void*)(res->data + nav_size * (i - 1));
		fsaqi_t qn = (void*)(res->data + nav_size * i);
		qi->next = qn;
	}
	{
		/* last cell */
		fsaqi_t qi = (void*)(res->data + nav_size * (queue_size - 1));
		qi->next = NULL;
	}
	res->free = (void*)res->data;
	return res;
}

static void
free_fsarrpq(fsarrpq_t q)
{
	pthread_mutex_lock(&q->mtx);
	pthread_mutex_unlock(&q->mtx);
	pthread_mutex_destroy(&q->mtx);
	xfree(q);
	return;
}

/**
 * Return the allocated size of the queue Q. */
static inline size_t
fsarrpq_alloc_size(fsarrpq_t q)
{
	return q->asz;
}

/**
 * Return the number of elements held in the queue Q. */
static inline size_t
fsarrpq_size(fsarrpq_t q)
{
	unsigned int res;
	pthread_mutex_lock(&q->mtx);
	res = q->qsz;
	pthread_mutex_unlock(&q->mtx);
	return res;
}

/**
 * Enqueue the cell pointed to by DATA into Q.
 * Return true if the cell could be enqueued. */
static bool
fsarrpq_enq(fsarrpq_t q, void *data)
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

static bool
fsarrpq_deq(fsarrpq_t q, void *data)
{
/* dequeue takes one item from the queue and prepends it to the free list */
	fsaqi_t qi;

	pthread_mutex_lock(&q->mtx);
	/* check if there's room in the queue */
	if (UNLIKELY(q->head == NULL)) {
		pthread_mutex_unlock(&q->mtx);
		return false;
	}
	/* dequeue from our list */
	qi = q->head;
	q->head = qi->next;
	if (q->head) {
		q->head->prev = NULL;
	} else {
		/* tail must be NULL also */
		q->tail = NULL;
	}
	/* copy the cell */
	memcpy(data, qi->data, q->csz);
	/* enqueue to our free queue */
	qi->next = q->free;
	q->free = qi;

	pthread_mutex_unlock(&q->mtx);
	return true;
}

#endif	/* INCLUDED_arrqueue_h_ */
