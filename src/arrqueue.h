/*** arrqueue.h -- helper for AoS queues
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

#if !defined INCLUDED_arrqueue_h_
#define INCLUDED_arrqueue_h_

#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>


#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */
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
typedef struct fsarrq_s *fsarrq_t;
/* ordinary doubly-linked queue with data from an array */
typedef struct dlarrq_s *dlarrq_t;
/* ordinary pointer based queue with data from an array */
typedef struct arrpq_s *arrpq_t;

struct fsarrq_s {
	/* queue head */
	short unsigned int head;
	/* queue tail */
	short unsigned int tail;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* slot size */
	unsigned int slsz;
	/* queue size */
	unsigned int qsz;
	/* data vector */
	char ALGN16(data[]);
};

struct arrpq_s {
	/* queue head */
	unsigned int head;
	/* queue tail */
	unsigned int tail;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* queue size */
	size_t size;
	/* the jobs vector */
	void *queue[] __attribute__((aligned(16)));
};


extern arrpq_t make_arrpq(size_t size);
extern void free_arrpq(arrpq_t q);

/**
 * Return the allocated size of the queue Q. */
static inline size_t __attribute__((always_inline, gnu_inline))
arrpq_alloc_size(arrpq_t q)
{
	return q->size;
}

/**
 * Return the number of elements held in the queue Q. */
static inline unsigned int __attribute__((always_inline, gnu_inline))
arrpq_size(arrpq_t q)
{
	unsigned int res;
	pthread_mutex_lock(&q->mtx);
	res = (unsigned int)(((int)q->head - (int)q->tail) % q->size);
	pthread_mutex_unlock(&q->mtx);
	return res;
}

static bool __attribute__((unused))
arrpq_enqueue(arrpq_t q, void *data)
{
	pthread_mutex_lock(&q->mtx);
	/* check if there's room in the queue */
	if (UNLIKELY((q->head + 1 == q->tail) ||
		     (q->head + 1 == q->size && q->tail == 0))) {
		pthread_mutex_unlock(&q->mtx);
		return false;
	}
	/* lodge data */
	q->queue[q->head] = data;
	/* step the head */
	q->head = (q->head + 1) % q->size;
	pthread_mutex_unlock(&q->mtx);
	return true;
}

static void __attribute__((unused))*
arrpq_dequeue(arrpq_t q)
{
	void *res = NULL;
	pthread_mutex_lock(&q->mtx);
	if (LIKELY(q->tail != q->head)) {
		/* fetch data */
		res = q->queue[q->tail];
		/* put a NULL into it */
		q->queue[q->tail] = NULL;
		/* step the tail */
		q->tail = (q->tail + 1) % q->size;
	}
	pthread_mutex_unlock(&q->mtx);
	return res;
}

#endif	/* INCLUDED_arrqueue_h_ */
