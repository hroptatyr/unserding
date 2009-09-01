/*** xdr-instr-private.h -- private bindings for instrument catalogue
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#if !defined INCLUDED_xdr_instr_private_h_
#define INCLUDED_xdr_instr_private_h_

#include <stdbool.h>
#include <pfack/instruments.h>
#include <pfack/tick.h>
#include "catalogue.h"
#include "protocore.h"
#include "seria.h"

#if !defined xnew
# define xnew(_x)	malloc(sizeof(_x))
#endif	/* !xnew */
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

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

#if !defined q32_t
# define q32_t	quantity32_t
#endif	/* !q32_t */
#if !defined m32_t
# define m32_t	monetary32_t
#endif	/* !m32_t */

extern cat_t instrs;

extern void dso_xdr_instr_LTX_init(void*);
extern void dso_xdr_instr_ticks_LTX_init(void*);
extern void dso_xdr_instr_mysql_LTX_init(void*);


/* inlines */
static inline void
clear_pkt(udpc_seria_t sctx, job_t rplj)
{
	memset(UDPC_PAYLOAD(rplj->buf), 0, UDPC_PLLEN);
	udpc_make_rpl_pkt(JOB_PACKET(rplj));
	udpc_seria_init(sctx, UDPC_PAYLOAD(rplj->buf), UDPC_PLLEN);
	return;
}

static inline void
copy_pkt(job_t tgtj, job_t srcj)
{
	memcpy(tgtj, srcj, sizeof(*tgtj));
	return;
}

static inline void
prep_pkt(udpc_seria_t sctx, job_t rplj, job_t srcj)
{
	copy_pkt(rplj, srcj);
	clear_pkt(sctx, rplj);
	return;
}

static inline void
send_pkt(udpc_seria_t sctx, job_t j)
{
	j->blen = UDPC_HDRLEN + udpc_seria_msglen(sctx);
	send_cl(j);
	return;
}

static inline void
hrclock_print(void)
{
	struct timespec tsp;
	clock_gettime(CLOCK_REALTIME, &tsp);
	fprintf(stderr, "%lu.%09u", tsp.tv_sec, (unsigned int)tsp.tv_nsec);
	return;
}

#endif	/* INCLUDED_xdr_instr_private_h_ */
