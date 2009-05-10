/*** dllqueue.c -- helper for AoS queues
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
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "dllqueue.h"

typedef struct __pqi_s *pqi_t;
struct __pqi_s {
	void *next;
	void *data;
};


dllpq_t
make_dllpq(size_t size __attribute__((unused)))
{
	dllpq_t res = xmalloc(sizeof(struct dllpq_s));
	res->head = res->tail = NULL;
	res->size = 0;
	pthread_mutex_init(&res->mtx, NULL);
	return res;
}

void
free_dllpq(dllpq_t q)
{
	pthread_mutex_lock(&q->mtx);
	for (pqi_t i = q->head, j; i; i = j) {
		j = i->next;
		xfree(i);
	}
	pthread_mutex_unlock(&q->mtx);
	pthread_mutex_destroy(&q->mtx);
	xfree(q);
	return;
}


void
dllpq_enqueue(dllpq_t q, void *data)
{
	pqi_t qi = xmalloc(sizeof(struct __pqi_s));

	pthread_mutex_lock(&q->mtx);
	qi->next = NULL;
	qi->data = data;

	if (q->tail) {
		pqi_t qt = q->tail;
		qt->next = qi;
	}
	q->tail = qi;
	pthread_mutex_unlock(&q->mtx);
	return;
}

void*
dllpq_dequeue(dllpq_t q)
{
	void *res = NULL;
	pqi_t qi;

	pthread_mutex_lock(&q->mtx);
	if ((qi = q->head) != NULL) {
		res = qi->data;
		q->head = qi->next;
		xfree(qi);
	}
	pthread_mutex_unlock(&q->mtx);
	return res;
}


/* arrqueue.c ends here */
