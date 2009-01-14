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
#include <stdint.h>

#if defined HAVE_EV_H
# include <ev.h>
#else  /* !HAVE_EV_H */
# error "We need an event loop, give us one."
#endif	/* HAVE_EV_H */

typedef size_t index_t;

#if defined UNSERSRV
#define UD_CRITICAL(args...)				\
	fprintf(stderr, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)			\
	fprintf(stderr, "[unserding] " args)
# define UD_CRITICAL_MCAST(args...)					\
	fprintf(stderr, "[unserding/input/mcast] CRITICAL " args)
# define UD_DEBUG_MCAST(args...)				\
	fprintf(stderr, "[unserding/input/mcast] " args)
# define UD_CRITICAL_STDIN(args...)				\
	fprintf(stderr, "[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)			\
	fprintf(stderr, "[unserding/stdin] " args)
# define UD_CRITICAL_PROTO(args...)				\
	fprintf(stderr, "[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)			\
	fprintf(stderr, "[unserding/proto] " args)
# define UD_CRITICAL_CAT(args...)				\
	fprintf(stderr, "[unserding/catalogue] CRITICAL " args)
# define UD_DEBUG_CAT(args...)				\
	fprintf(stderr, "[unserding/catalogue] " args)

#elif defined UNSERCLI
# define UD_CRITICAL(args...)				\
	fprintf(stderr, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)					\
	fprintf(stderr, "[unserding/input/mcast] CRITICAL " args)
# define UD_DEBUG_MCAST(args...)				\
	fprintf(stderr, "[unserding/input/mcast] " args)
# define UD_CRITICAL_STDIN(args...)			\
	fprintf(stderr, "[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)

#elif defined UNSERMON
# define UD_CRITICAL(args...)				\
	fprintf(stderr, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)
# define UD_DEBUG_MCAST(args...)
# define UD_UNSERMON_PKT(args...)	fprintf(stderr, "%02x:%06x: " args)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)				\
	fprintf(stderr, "[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)

#else  /* aux stuff */
# define UD_CRITICAL(args...)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)
# define UD_DEBUG_MCAST(args...)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)
#endif

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif
#define UNUSED(_x)	__attribute__((unused)) _x
#define ALGN16(_x)	__attribute__((aligned(16))) _x

#define countof(x)		(sizeof(x) / sizeof(*x))
#define countof_m1(x)		(countof(x) - 1)

/* The encoded parameter sizes will be rounded up to match pointer alignment. */
#define ROUND(s, a)		(a * ((s + a - 1) / a))
#define aligned_sizeof(t)	ROUND(sizeof(t), __alignof(void*))


/* job queue magic */
/* we use a fairly simplistic approach: one vector with two index pointers */
/* number of simultaneous jobs */
#define NJOBS		256
#define NO_JOB		((job_t)-1)

typedef struct job_queue_s *job_queue_t;
typedef struct job_s *job_t;
/**
 * Type for parse functions inside jobs. */
typedef void(*ud_parse_f)(job_t);
/**
 * Type for work functions inside jobs. */
typedef void(*ud_work_f)(job_t);
/**
 * Type for clean up functions inside jobs. */
typedef ud_work_f ud_free_f;
/**
 * Type for print functions inside jobs. */
typedef ud_work_f ud_prnt_f;

#define SIZEOF_JOB_S	4096
struct job_s {
	ud_work_f workf;
	ud_prnt_f prntf;
	ud_free_f freef;
	void *clo;
	/* set to 1 if job is ready to be processed */
	unsigned short int readyp;
	/** for udp based transports,
	 * use a union here to allow clients to use whatever struct they want */
	union {
		/* this is the size we want at least */
		char anon[32];
#if defined SA_STRUCT
		/* will be typically struct sockaddr_in6 */
		SA_STRUCT sa;
#endif
	};
	size_t blen;
	char ALGN16(buf)[];
} __attribute__((aligned(SIZEOF_JOB_S)));
#define JOB_BUF_SIZE						\
	SIZEOF_JOB_S - offsetof(struct job_s, buf)

struct job_queue_s {
	/* job index, always points to a free job */
	unsigned short int ji;
	/* en/de-queuing mutex */
	pthread_mutex_t mtx;
	/* the jobs vector */
	struct job_s jobs[NJOBS];
};

static inline index_t __attribute__((always_inline, gnu_inline))
__next_job(job_queue_t jq)
{
	index_t res = (jq->ji + 1) % NJOBS;

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
obtain_job(job_queue_t jq)
{
	job_t res;
	pthread_mutex_lock(&jq->mtx);
	res = &jq->jobs[jq->ji];
	jq->ji = __next_job(jq);
	res->readyp = 0;
	pthread_mutex_unlock(&jq->mtx);
	return res;
}

static inline void __attribute__((always_inline, gnu_inline))
enqueue_job(job_queue_t jq, job_t j)
{
	/* dont check if the queue is full, just go assume our pipes are
	 * always large enough */
	pthread_mutex_lock(&jq->mtx);
	j->readyp = 1;
	pthread_mutex_unlock(&jq->mtx);
	return;
}

#if 0
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
#endif	/* 0 */

static inline void __attribute__((always_inline, gnu_inline))
free_job(job_t j)
{
	if (UNLIKELY(j->freef != NULL)) {
		j->freef(j);
	}
	memset(j, 0, SIZEOF_JOB_S);
	return;
}

/**
 * Global job queue. */
extern job_queue_t glob_jq;

/**
 * Job that looks up the parser routine in ud_parsef(). */
extern void ud_proto_parse(job_t);
extern ud_parse_f ud_parsef[4096];

/* jobs */
extern void ud_hyrpl_job(job_t);
/* the old ascii parser */
extern void ud_parse(job_t);

#if defined UNSERCLI
extern bool cli_waiting_p;
#endif	/* UNSERCLI */

/* more socket goodness, defined in mcast4.c */
extern int ud_attach_mcast4(EV_P);
extern int ud_detach_mcast4(EV_P);
extern void send_m4(job_t);
extern void send_m46(job_t);
extern void send_m6(job_t);
extern void send_cl(job_t);

/* readline goodness, defined in stdin.c */
extern int ud_attach_stdin(EV_P);
extern int ud_detach_stdin(EV_P);
extern void ud_reset_stdin(EV_P);

/* jobs to browse the catalogue */
extern void ud_cat_ls_job(job_t);


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
