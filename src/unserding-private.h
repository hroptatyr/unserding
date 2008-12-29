/*** unserding-private.h -- backend goodies
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

#if !defined INCLUDED_unserding_private_h_
#define INCLUDED_unserding_private_h_

#include <pthread.h>
#if defined HAVE_EV_H
# include <ev.h>
#else  /* !HAVE_EV_H */
# error "We need an event loop, give us one."
#endif	/* HAVE_EV_H */

typedef size_t index_t;

#define UD_CRITICAL_TCPUDP(args...)					\
	fprintf(stderr, "[unserding/input/tcpudp] CRITICAL " args)
#define UD_DEBUG_TCPUDP(args...)					\
	fprintf(stderr, "[unserding/input/tcpudp] " args)

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#define UNUSED(_x)	__attribute__((unused)) _x
#define ALGN16(_x)	__attribute__((aligned(16))) _x

#define countof(x)		(sizeof(x) / sizeof(*x))

/* The encoded parameter sizes will be rounded up to match pointer alignment. */
#define ROUND(s, a)		(a * ((s + a - 1) / a))
#define aligned_sizeof(t)	ROUND(sizeof(t), __alignof(void*))


/* connexion contexts */
typedef struct conn_ctx_s *conn_ctx_t;

#define SIZEOF_CONN_CTX_S	1024
struct conn_ctx_s {
	/* read io */
	struct ev_io ALGN16(rio);
	/* write io */
	struct ev_io ALGN16(wio);
	/* timer */
	struct ev_timer ALGN16(ti);
	int src;
	int snk;
	/* output buffer */
	size_t obuflen;
	index_t obufidx;
	const char *obuf;
	/* the morning-after pill */
	void *after_wio_j;
	/* input buffer */
	index_t bidx;
	char ALGN16(buf)[];
} __attribute__((aligned(SIZEOF_CONN_CTX_S)));
#define CONN_CTX_BUF_SIZE					\
	SIZEOF_CONN_CTX_S - offsetof(struct conn_ctx_s, buf)

static inline conn_ctx_t __attribute__((always_inline, gnu_inline))
ev_rio_ctx(void *rio)
{
	return (void*)((char*)rio - offsetof(struct conn_ctx_s, rio));
}

static inline conn_ctx_t __attribute__((always_inline, gnu_inline))
ev_wio_ctx(void *wio)
{
	return (void*)((char*)wio - offsetof(struct conn_ctx_s, wio));
}

static inline conn_ctx_t __attribute__((always_inline, gnu_inline))
ev_timer_ctx(void *timer)
{
	return (void*)((char*)timer - offsetof(struct conn_ctx_s, ti));
}

static inline ev_io __attribute__((always_inline, gnu_inline)) *
ctx_rio(conn_ctx_t ctx)
{
	return (void*)&ctx->rio;
}

static inline ev_io __attribute__((always_inline, gnu_inline)) *
ctx_wio(conn_ctx_t ctx)
{
	return (void*)&ctx->wio;
}

static inline ev_timer __attribute__((always_inline, gnu_inline)) *
ctx_timer(conn_ctx_t ctx)
{
	return (void*)&ctx->ti;
}

extern conn_ctx_t find_ctx(void);


/* socket goodness */
extern int ud_attach_tcp6(EV_P);
extern int ud_detach_tcp6(EV_P);


/* job queue magic */
/* we use a fairly simplistic approach: one vector with two index pointers */
/* number of simultaneous jobs */
#define NJOBS		256
#define NO_JOB		((job_t)-1)

typedef struct job_queue_s *job_queue_t;
typedef struct job_s *job_t;

struct job_s {
	void(*workf)(void*);
	void *clo;
};

struct job_queue_s {
	/* read index, where to read the next job */
	index_t ri;
	/* write index, where to put the next job */
	index_t wi;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* the jobs vector */
	struct job_s jobs[NJOBS];
};

static inline void __attribute__((always_inline, gnu_inline))
enqueue_job(job_queue_t jq, job_t job)
{
	pthread_mutex_lock(&jq->mtx);
	/* dont check if the queue is full, just go assume our pipes are
	 * always large enough */
	if (LIKELY(job != NULL)) {
		jq->jobs[jq->wi] = *job;
	} else {
		memset(&jq->jobs[jq->wi], 0, sizeof(*job));
	}
	jq->wi = (jq->wi + 1) % NJOBS;
	pthread_mutex_unlock(&jq->mtx);
	return;
}

static inline job_t __attribute__((always_inline, gnu_inline))
dequeue_job(job_queue_t jq)
{
	volatile job_t res = NO_JOB;
	pthread_mutex_lock(&jq->mtx);
	if (UNLIKELY(jq->ri != jq->wi)) {
		if (UNLIKELY((res = &jq->jobs[jq->ri])->workf == NULL)) {
			res = NULL;
		}
		jq->ri = (jq->ri + 1) % NJOBS;
	}
	pthread_mutex_unlock(&jq->mtx);
	return res;
}

static inline void __attribute__((always_inline, gnu_inline))
free_job(job_t j)
{
	j->workf = NULL;
	j->clo = NULL;
	return;
}

extern job_queue_t glob_jq;


/* worker magic */
/* think we keep this private */
extern void trigger_job_queue(void);

#endif	/* INCLUDED_unserding_private_h_ */
