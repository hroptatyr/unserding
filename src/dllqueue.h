/*** dllqueue.h -- helper for AoS queues
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

#if !defined INCLUDED_dllqueue_h_
#define INCLUDED_dllqueue_h_

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
typedef struct fsdllq_s *fsdllq_t;
/* ordinary doubly-linked queue with data from an array */
typedef struct dlarrq_s *dlarrq_t;
/* ordinary pointer based queue with data from an array */
typedef struct dllpq_s *dllpq_t;

struct dllpq_s {
	/* queue head */
	void *head;
	/* queue tail */
	void *tail;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* queue size */
	size_t size;
	/* free containers */
	void *freeq[];
};


extern dllpq_t make_dllpq(size_t size);
extern void free_dllpq(dllpq_t q);

/**
 * Return the number of elements held in the queue Q. */
static inline unsigned int __attribute__((always_inline, gnu_inline))
dllpq_size(dllpq_t q)
{
	unsigned int res;
	pthread_mutex_lock(&q->mtx);
	res = (unsigned int)q->size;
	pthread_mutex_unlock(&q->mtx);
	return res;
}

extern void dllpq_enqueue(dllpq_t q, void *data);
extern void *dllpq_dequeue(dllpq_t q);

#endif	/* INCLUDED_dllqueue_h_ */
