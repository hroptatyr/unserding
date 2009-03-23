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
#include <stdarg.h>
#include <unistd.h>

#if defined HAVE_EV_H
# include <ev.h>
#else  /* !HAVE_EV_H */
# error "We need an event loop, give us one."
#endif	/* HAVE_EV_H */

#include "unserding.h"

typedef size_t index_t;

#if defined UNSERSRV && defined DEBUG_FLAG
#define UD_CRITICAL(args...)				\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)			\
	fprintf(logout, "[unserding] " args)
# define UD_CRITICAL_MCAST(args...)					\
	fprintf(logout, "[unserding/input/mcast] CRITICAL " args)
# define UD_DEBUG_MCAST(args...)				\
	fprintf(logout, "[unserding/input/mcast] " args)
# define UD_CRITICAL_STDIN(args...)				\
	fprintf(logout, "[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)			\
	fprintf(logout, "[unserding/stdin] " args)
# define UD_CRITICAL_PROTO(args...)				\
	fprintf(logout, "[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)			\
	fprintf(logout, "[unserding/proto] " args)
# define UD_CRITICAL_CAT(args...)				\
	fprintf(logout, "[unserding/catalogue] CRITICAL " args)
# define UD_DEBUG_CAT(args...)				\
	fprintf(logout, "[unserding/catalogue] " args)

#elif defined UNSERCLI && defined DEBUG_FLAG
# define UD_CRITICAL(args...)				\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)					\
	fprintf(logout, "[unserding/input/mcast] CRITICAL " args)
# define UD_DEBUG_MCAST(args...)				\
	fprintf(logout, "[unserding/input/mcast] " args)
# define UD_CRITICAL_STDIN(args...)			\
	fprintf(logout, "[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)

#elif defined UNSERMON && defined DEBUG_FLAG
# define UD_CRITICAL(args...)				\
	fprintf(logout, "[unserding] CRITICAL " args)
# define UD_DEBUG(args...)
# define UD_CRITICAL_MCAST(args...)
# define UD_DEBUG_MCAST(args...)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)				\
	fprintf(logout, "[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)

#else  /* aux stuff */
# define UD_CRITICAL(args...)			\
	fprintf(logout, "[unserding] CRITICAL " args)
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

#if defined UNSERMON
# define UD_UNSERMON_PKT(args...)	fprintf(logout, "%02x:%06x: " args)
#endif	/* UNSERMON */

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


/* logging */
extern FILE *logout;

#if defined UNSERSRV
# if defined TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else  /* !TIME_WITH_SYS_TIME */
#  if defined HAVE_SYS_TIME_H
#   include <sys/time.h>
#  endif
#  if defined HAVE_TIME_H
#   include <time.h>
#  endif
# endif

extern inline void __attribute__((always_inline, gnu_inline,format(printf,1,0)))
__ud_log(const char *restrict fmt, ...);
# define UD_LOG(args...)	__ud_log("%lu.%09u [unserding] " args)
# define UD_LOG_MCAST(args...)	__ud_log("%lu.%09u [unserding/mcast] " args)

# if __GNUC_PREREQ(4,3)
/* methinks this is fuck ugly, but kinda cool */
extern inline void __attribute__((always_inline, gnu_inline,format(printf,1,0)))
__ud_log(const char *restrict fmt, ...)
{
#  if defined HAVE_CLOCK_GETTIME || 1 /* check for me */
	struct timespec n;
#  else	 /* !HAVE_CLOCK_GETTIME */
#   error "WTF?!"
#  endif  /* HAVE_CLOCK_GETTIME */
	clock_gettime(CLOCK_REALTIME, &n);
	/* there must be %lu.%09u in the format string */
	fprintf(logout, fmt, n.tv_sec, n.tv_nsec, __builtin_va_arg_pack());
	fflush(logout);
	return;
}
# else	/* !4.3 */
#  error "Use gcc 4.3 or give me va_arg_pack() implementation!"
# endif	 /* >= 4.3 */
#else
# define UD_LOG(args...)
# define UD_LOG_MCAST(args...)
#endif	/* UNSERSRV */


#include "arrqueue.h"

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
	/* the queue for the workers */
	arrpq_t wq;
	/* the queue for free jobs */
	arrpq_t fq;
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
	return arrpq_dequeue(glob_jq->fq);
}

static inline void __attribute__((always_inline, gnu_inline))
free_job(job_t j)
{
	/* enqueue in the free queue */
	memset(j, 0, SIZEOF_JOB_S);
	arrpq_enqueue(glob_jq->fq, j);
	return;
}

static inline void __attribute__((always_inline, gnu_inline))
enqueue_job(job_queue_t jq, job_t j)
{
	/* enqueue in the worker queue */
	arrpq_enqueue(jq->wq, j);
	return;
}

static inline job_t __attribute__((always_inline, gnu_inline))
dequeue_job(job_queue_t jq)
{
	/* dequeue from the worker queue */
	return arrpq_dequeue(jq->wq);
}

/* helper macro to use a job as packet */
#define JOB_PACKET(j)	((ud_packet_t){.plen = j->blen, .pbuf = j->buf})
/* helper macro to use a char buffer as packet */
#define BUF_PACKET(b)	((ud_packet_t){.plen = countof(b), .pbuf = b})
#define PACKET(a, b)	((ud_packet_t){.plen = a, .pbuf = b})

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
