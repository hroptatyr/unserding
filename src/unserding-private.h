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
#include <string.h>

#if defined HAVE_EV_H
# include <ev.h>
#else  /* !HAVE_EV_H */
# error "We need an event loop, give us one."
#endif	/* HAVE_EV_H */

typedef size_t index_t;

#define UD_CRITICAL(args...)				\
	fprintf(stderr, "[unserding] CRITICAL " args)
#define UD_DEBUG(args...)			\
	fprintf(stderr, "[unserding] " args)
#define UD_CRITICAL_TCPUDP(args...)					\
	fprintf(stderr, "[unserding/input/tcpudp] CRITICAL " args)
#define UD_DEBUG_TCPUDP(args...)				\
	fprintf(stderr, "[unserding/input/tcpudp] " args)
#define UD_CRITICAL_STDIN(args...)					\
	fprintf(stderr, "[unserding/stdin] CRITICAL " args)
#define UD_DEBUG_STDIN(args...)				\
	fprintf(stderr, "[unserding/stdin] " args)
#define UD_CRITICAL_PROTO(args...)				\
	fprintf(stderr, "[unserding/proto] CRITICAL " args)
#define UD_DEBUG_PROTO(args...)				\
	fprintf(stderr, "[unserding/proto] " args)
#define UD_CRITICAL_CAT(args...)				\
	fprintf(stderr, "[unserding/catalogue] CRITICAL " args)
#define UD_DEBUG_CAT(args...)				\
	fprintf(stderr, "[unserding/catalogue] " args)

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
typedef struct outbuf_s *outbuf_t;
typedef struct outbuf_ring_s *outbuf_ring_t;

/** Structure for output buffers */
struct outbuf_s {
	/** Length of the output buffer. */
	size_t obuflen;
	/** Index of ... */
	index_t obufidx;
	/** Output buffer, | with 1 if buffer must be freed after output. */
	const char *obuf;
};

#define NOUTBUFS		16
struct outbuf_ring_s {
	pthread_mutex_t mtx;
	index_t curr_idx;
	struct outbuf_s obufs[NOUTBUFS];
};

static inline void __attribute__((always_inline, gnu_inline))
init_outbuf(outbuf_t obuf)
{
	obuf->obuflen = obuf->obufidx = 0UL, obuf->obuf = NULL;
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
deinit_outbuf(outbuf_t obuf)
{
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
lock_obring(outbuf_ring_t obring)
{
	pthread_mutex_lock(&obring->mtx);
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
unlock_obring(outbuf_ring_t obring)
{
	pthread_mutex_unlock(&obring->mtx);
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
init_obring(outbuf_ring_t obring)
{
	pthread_mutex_init(&obring->mtx, NULL);
	obring->curr_idx = 0;
	for (index_t i = 0; i < NOUTBUFS; i++) {
		init_outbuf(&obring->obufs[i]);
	}
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
deinit_obring(outbuf_ring_t obring)
{
	lock_obring(obring);
	unlock_obring(obring);
	pthread_mutex_destroy(&obring->mtx);
	return;
}

static inline outbuf_t __attribute__((always_inline, gnu_inline))
curr_outbuf(outbuf_ring_t obring)
{
	return &obring->obufs[obring->curr_idx];
}

static inline index_t __attribute__((always_inline, gnu_inline))
step_obring_idx(index_t idx)
{
	return (idx + 1) % NOUTBUFS;
}

static inline outbuf_t __attribute__((always_inline, gnu_inline))
next_outbuf(outbuf_ring_t obring)
{
	index_t idx;
	for (idx = obring->curr_idx;
	     obring->obufs[idx].obuf != NULL;
	     idx = step_obring_idx(idx));
	return &obring->obufs[idx];
}

static inline void __attribute__((always_inline, gnu_inline))
free_outbuf(outbuf_t obuf)
{
	if ((long int)obuf->obuf & 1UL) {
		free((void*)((long int)obuf->obuf & ~1UL));
	}
	/* reset the output buffer */
	obuf->obuf = NULL;
	return;
}

static inline bool __attribute__((always_inline, gnu_inline))
outbuf_free_p(outbuf_t obuf)
{
	return obuf->obuf == NULL;
}

/** Size of the entire connexion context structure, VLA at the end. */
#define SIZEOF_CONN_CTX_S	2048
struct conn_ctx_s {
	/* read io */
	struct ev_io ALGN16(rio);
	/* write io */
	struct ev_io ALGN16(wio);
	/* timer */
	struct ev_timer ALGN16(ti);
	int src;
	int snk;
	ev_tstamp timeout;
	/** current pwd */
	ud_cat_t pwd;
	/** output buffer */
	struct outbuf_ring_s obring;
	/** input buffer */
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


/* socket goodness, defined in tcp6.c */
extern int ud_attach_tcp6(EV_P);
extern int ud_detach_tcp6(EV_P);
extern void ud_print_tcp6(EV_P_ conn_ctx_t ctx, const char *m, size_t mlen);
extern void ud_kick_tcp6(EV_P_ conn_ctx_t ctx);

/* readline goodness, defined in stdin.c */
extern int ud_attach_stdin(EV_P);
extern int ud_detach_stdin(EV_P);


/* job queue magic */
/* we use a fairly simplistic approach: one vector with two index pointers */
/* number of simultaneous jobs */
#define NJOBS		256
#define NO_JOB		((job_t)-1)

typedef struct job_queue_s *job_queue_t;
typedef struct job_s *job_t;
/**
 * Type for work functions inside jobs. */
typedef void(*ud_work_f)(job_t);
/**
 * Type for clean up functions inside jobs. */
typedef void(*ud_free_f)(job_t);

struct job_s {
	ud_work_f workf;
	ud_free_f freef;
	void *clo;
	char ALGN16(work_space)[];
} __attribute__((aligned(1024)));

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

static inline index_t __attribute__((always_inline, gnu_inline))
__next_job(job_queue_t jq)
{
	index_t res = (jq->wi + 1) % NJOBS;

	if (LIKELY(jq->jobs[res].workf == NULL)) {
		return res;
	}
	for (; jq->jobs[res].workf == NULL; ) {
		if (LIKELY(++res < NJOBS));
		else {
			res = 0;
		}
	}
	return res;
}

static inline job_t __attribute__((always_inline, gnu_inline))
enqueue_job(job_queue_t jq, ud_work_f workf, void *clo)
{
	job_t res;
	pthread_mutex_lock(&jq->mtx);
	res = &jq->jobs[jq->wi];
	/* dont check if the queue is full, just go assume our pipes are
	 * always large enough */
	res->workf = workf;
	res->clo = clo;
	jq->wi = __next_job(jq);
	pthread_mutex_unlock(&jq->mtx);
	return res;
}

static inline job_t __attribute__((always_inline, gnu_inline))
enqueue_job_cp_ws(job_queue_t jq, ud_work_f workf, void *clo,
		  const void *stu, size_t len)
{
/* enqueue the job and copy stuff over to the job's work space */
	job_t res;
	pthread_mutex_lock(&jq->mtx);
	res = &jq->jobs[jq->wi];
	/* dont check if the queue is full, just go assume our pipes are
	 * always large enough */
	res->workf = workf;
	res->clo = clo;
	/* copy over the baloney in stu
	 * we dont check the length here, brilliant aye? */
	memcpy(&res->work_space, stu, len);
	res->work_space[len] = '\0';
	/* inc the job counter */
	jq->wi = __next_job(jq);
	pthread_mutex_unlock(&jq->mtx);
	return res;
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

/* some special work functions */
extern void ud_parse(job_t);
/* jobs to browse the catalogue */
extern void ud_cat_ls_job(job_t);
extern void ud_cat_pwd_job(job_t);
extern void ud_cat_cd_job(job_t);


/* worker magic */
/* think we keep this private */
extern void trigger_job_queue(void);
/* global notification signal */
extern ev_async *glob_notify;
/* notify the global event loop */
extern inline void __attribute__((gnu_inline)) trigger_evloop(EV_P);

extern inline void __attribute__((always_inline, gnu_inline))
trigger_evloop(EV_P)
{
	ev_async_send(EV_A_ glob_notify);
	return;
}

#endif	/* INCLUDED_unserding_private_h_ */
