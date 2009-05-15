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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif
#if defined HAVE_PTHREAD_H
# include <pthread.h>
#endif
#if defined HAVE_STRING_H
# include <string.h>
#endif
#if defined HAVE_STDINT_H
# include <stdint.h>
#endif
#if defined HAVE_STDARG_H
# include <stdarg.h>
#endif
#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined HAVE_EV_H
# include <ev.h>
#else  /* !HAVE_EV_H */
# error "We need an event loop, give us one."
#endif	/* HAVE_EV_H */

#include "unserding.h"


#include "unserding-nifty.h"


#include "unserding-dbg.h"


#define USE_ARRPQ	0

#if USE_ARRPQ
# include "arrqueue.h"
#else
# include "dllqueue.h"
#endif

/* job queue magic */
/* we use a fairly simplistic approach: one vector with two index pointers */
/* number of simultaneous jobs */
#define NJOBS		256
#define NO_JOB		((job_t)0)

typedef struct job_queue_s *job_queue_t;
typedef struct job_s *job_t;
#define TYPEDEFD_job_t
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
	/**
	 * bits 0-1 is job state:
	 * set to 0 if job is free,
	 * set to 1 if job is being loaded
	 * set to 2 if job is ready to be processed
	 * bits 2-3 is transmission state:
	 *
	 * */
	long unsigned int flags;

	size_t blen;
	char ALGN16(buf)[];
} __attribute__((aligned(SIZEOF_JOB_S)));
#define JOB_BUF_SIZE						\
	SIZEOF_JOB_S - offsetof(struct job_s, buf)

struct job_queue_s {
#if USE_ARRPQ
	/* the queue for the workers */
	arrpq_t wq;
	/* the queue for free jobs */
	arrpq_t fq;
#else  /* !USE_ARRPQ */
	/* the queue for the workers */
	dllpq_t wq;
	/* the queue for free jobs */
	dllpq_t fq;
#endif	/* USE_ARRPQ */
	/* the jobs vector */
	struct job_s jobs[NJOBS] __attribute__((aligned(16)));
};

static inline uint8_t __attribute__((always_inline, gnu_inline))
__job_ready_bits(job_t j)
{
/* the bits of the flags slot that deal with the readiness of a job */
	return j->flags & 0x3;
}

static inline uint8_t __attribute__((always_inline, gnu_inline))
__job_trans_bits(job_t j)
{
/* the bits of the flags slot that deal with the transmission of a job */
	return j->flags & 0xc;
}

static inline bool __attribute__((always_inline, gnu_inline))
__job_notransp(job_t j)
{
/* return true iff job is not to be transmitted back */
	return __job_trans_bits(j) == 0;
}

static inline void __attribute__((always_inline, gnu_inline))
__job_set_notrans(job_t j)
{
/* make job not be transmitted back */
	j->flags &= ~0xc;
	return;
}

static inline bool __attribute__((always_inline, gnu_inline))
__job_transp(job_t j)
{
/* return true iff job is to be transmitted back to the origin */
	return __job_trans_bits(j) == 3;
}

static inline void __attribute__((always_inline, gnu_inline))
__job_set_trans(job_t j)
{
/* make job be transmitted back to whence it came */
	j->flags |= 0xc;
	return;
}

/**
 * Global job queue. */
extern job_queue_t glob_jq;

static inline job_t __attribute__((always_inline, gnu_inline))
make_job(void)
{
	/* dequeue from the free queue */
#if USE_ARRPQ
	return arrpq_dequeue(glob_jq->fq);
#else
	return dllpq_dequeue(glob_jq->fq);
#endif
}

static inline void __attribute__((always_inline, gnu_inline))
free_job(job_t j)
{
	/* enqueue in the free queue */
	memset(j, 0, SIZEOF_JOB_S);
#if USE_ARRPQ
	arrpq_enqueue(glob_jq->fq, j);
#else
	dllpq_enqueue(glob_jq->fq, j);
#endif
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
enqueue_job(job_queue_t jq, job_t j)
{
	/* enqueue in the worker queue */
#if USE_ARRPQ
	arrpq_enqueue(jq->wq, j);
#else
	dllpq_enqueue(jq->wq, j);
#endif
	return;
}

static inline job_t __attribute__((always_inline, gnu_inline))
dequeue_job(job_queue_t jq)
{
	/* dequeue from the worker queue */
#if USE_ARRPQ
	return arrpq_dequeue(jq->wq);
#else
	return dllpq_dequeue(jq->wq);
#endif
}

/* helper macro to use a job as packet */
#define JOB_PACKET(j)	((ud_packet_t){.plen = j->blen, .pbuf = j->buf})
/* helper macro to use a char buffer as packet */
#define BUF_PACKET(b)	((ud_packet_t){.plen = countof(b), .pbuf = b})
#define PACKET(a, b)	((ud_packet_t){.plen = a, .pbuf = b})

static void __attribute__((unused))
init_glob_jq(job_queue_t q)
{
	glob_jq = q;
#if USE_ARRPQ
	glob_jq->wq = make_arrpq(NJOBS);
	glob_jq->fq = make_arrpq(NJOBS);
#else
	glob_jq->wq = make_dllpq(NJOBS);
	glob_jq->fq = make_dllpq(NJOBS);
#endif
	/* enqueue all jobs in the free queue */
	for (int i = 0; i < NJOBS; i++) {
#if USE_ARRPQ
		arrpq_enqueue(glob_jq->fq, &glob_jq->jobs[i]);
#else
		dllpq_enqueue(glob_jq->fq, &glob_jq->jobs[i]);
#endif
	}
	return;
}

/**
 * Job that looks up the parser routine in ud_parsef(). */
extern void ud_proto_parse(job_t);

/* jobs */
extern void ud_hyrpl_job(job_t);
/* the old ascii parser */
extern void ud_parse(ud_packet_t);

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
