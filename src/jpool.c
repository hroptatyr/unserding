/*** jpool.c -- unserding job pool, it's just a queue basically
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

#include <pthread.h>
#include "unserding-nifty.h"
#include "wpool.h"
#if !defined UD_DEBUG
# define UD_DEBUG(args...)
#endif	/* UNSERSRV */

/* fixed-size AoS queue, data inside. */
typedef struct __jpool_s *__jpool_t;
/* navigator cell */
typedef struct jpi_s *jpi_t;

struct jpi_s {
	jpool_t pool;
	jpi_t next;
	char ALGN16(data[]);
};

struct __jpool_s {
	/* free queue */
	jpi_t free;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* cell size */
	unsigned int csz;
	/* data vector */
	char ALGN16(data[]);
};

/* super hack */
static inline jpi_t
jpi_from_job(job_t j)
{
	return (void*)((char*)j - offsetof(struct jpi_s, data));
}

static inline jpool_t
jpool_from_job(job_t j)
{
	jpi_t jpi = jpi_from_job(j);
	return jpi->pool;
}


jpool_t
make_jpool(int njobs, int job_size)
{
	size_t nav_size = sizeof(struct jpi_s) + job_size;
	__jpool_t res = xmalloc(sizeof(*res) + njobs * nav_size);

	res->csz = job_size;
	pthread_mutex_init(&res->mtx, NULL);
	/* initialise the free queue */
	for (size_t i = 1; i < njobs; i++) {
		jpi_t qi = (void*)(res->data + nav_size * (i - 1));
		jqi_t qn = (void*)(res->data + nav_size * i);
		qi->pool = res;
		qi->next = qn;
	}
	{
		/* last cell */
		jpi_t qi = (void*)(res->data + nav_size * (njobs - 1));
		qi->pool = res;
		qi->next = NULL;
	}
	/* let the free list head point to the first element */
	res->free = (void*)res->data;
	return res;
}

void
free_jpool(jpool_t q)
{
	pthread_mutex_lock(&AS_JPOOL(q)->mtx);
	pthread_mutex_unlock(&AS_JPOOL(q)->mtx);
	pthread_mutex_destroy(&AS_JPOOL(q)->mtx);
	xfree(q);
	return;
}

/**
 * Acquire a job from our pool. */
static job_t
jpool_acquire(jpool_t jp)
{
	__jpool_t q = jp;
	jpi_t qi;

	pthread_mutex_lock(&q->mtx);
	/* check if there's room in the queue */
	if (UNLIKELY(q->free == NULL)) {
		pthread_mutex_unlock(&q->mtx);
		return NO_JOB;
	}
	/* dequeue from free list */
	qi = q->free;
	q->free = qi->next;
	qi->next = NULL;
	pthread_mutex_unlock(&q->mtx);
	return qi->data;
}

/**
 * Put a previously acquired job back on the queue. */
void
jpool_release(job_t j)
{
	jpi_t qi;
	__jpool_t q;

	if (j == NO_JOB) {
		return;
	}
	/* otherwise */
	qi = jpi_from_job(j);
	q = qi->pool;

	/* clean up */
	memset(j, 0, q->csz);

	pthread_mutex_lock(&q->mtx);
	/* enqueue to our free queue */
	qi->next = q->free;
	q->free = qi;
	pthread_mutex_unlock(&q->mtx);
	return;
}

/* jpool.c ends here */
