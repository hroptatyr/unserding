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
# undef EV_P
# define EV_P	struct ev_loop *loop __attribute__((unused))
#else  /* !HAVE_EV_H */
# error "We need an event loop, give us one."
#endif	/* HAVE_EV_H */

#include "unserding.h"
#include "protocore.h"
#include "wpool.h"
#include "jpool.h"

#include "unserding-nifty.h"
#include "unserding-dbg.h"


/* job queue magic */
/* we use a fairly simplistic approach: one vector with two index pointers */
/* number of simultaneous jobs */
#define NJOBS		256

/**
 * Type for work functions inside jobs. */
typedef void(*ud_work_f)(job_t);
/**
 * Type for clean up functions inside jobs. */
typedef ud_work_f ud_free_f;
/**
 * Type for print functions inside jobs. */
typedef ud_work_f ud_prnt_f;

static inline uint8_t __attribute__((always_inline))
__job_ready_bits(job_t j)
{
/* the bits of the flags slot that deal with the readiness of a job */
	return j->flags & 0x3;
}

static inline uint8_t __attribute__((always_inline))
__job_trans_bits(job_t j)
{
/* the bits of the flags slot that deal with the transmission of a job */
	return j->flags & 0xc;
}

static inline bool __attribute__((always_inline))
__job_notransp(job_t j)
{
/* return true iff job is not to be transmitted back */
	return __job_trans_bits(j) == 0;
}

static inline void __attribute__((always_inline))
__job_set_notrans(job_t j)
{
/* make job not be transmitted back */
	j->flags &= ~0xc;
	return;
}

static inline bool __attribute__((always_inline))
__job_transp(job_t j)
{
/* return true iff job is to be transmitted back to the origin */
	return __job_trans_bits(j) == 3;
}

static inline void __attribute__((always_inline))
__job_set_trans(job_t j)
{
/* make job be transmitted back to whence it came */
	j->flags |= 0xc;
	return;
}

/**
 * Global job queue. */
extern jpool_t gjpool;

/**
 * Global worker pool, contains the job queue. */
extern wpool_t gwpool;

#if defined UNSERCLI
extern bool cli_waiting_p;
#endif	/* UNSERCLI */

/* more socket goodness, defined in mcast4.c */
extern int ud_attach_mcast(EV_P_ bool prefer_ipv6_p);
extern int ud_detach_mcast(EV_P);

/* more mainloop magic */
extern void
schedule_once_idle(void *ctx, void(*cb)(void *clo), void *clo);
extern void
schedule_timer_once(void *ctx, void(*cb)(void *clo), void *clo, double in);
extern void*
schedule_timer_every(void *ctx, void(*cb)(void *clo), void *clo, double every);
extern void
unsched_timer(void *ctx, void *timer);


/* specific services */
extern void dso_pong_LTX_init(void*);
extern void dso_pong_LTX_deinit(void*);

#endif	/* INCLUDED_unserding_private_h_ */
