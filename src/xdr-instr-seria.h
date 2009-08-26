/*** xdr-instr-seria.h -- unserding serialisation for xdr instruments
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

#if !defined INCLUDED_xdr_instr_seria_h_
#define INCLUDED_xdr_instr_seria_h_

#include <stdbool.h>
#include <pfack/tick.h>
#include <time.h>
#include "seria.h"

typedef struct sl1tick_s *sl1tick_t;
typedef struct secu_s *secu_t;
typedef struct tick_by_ts_hdr_s *tick_by_ts_hdr_t;
typedef struct tick_by_instr_hdr_s *tick_by_instr_hdr_t;

struct tick_by_ts_hdr_s {
	time_t ts;
	uint32_t types;
};

struct tick_by_instr_hdr_s {
	gaid_t instr;
	uint32_t types;
};

struct secu_s {
	gaid_t instr;
	gaid_t unit;
	gaid_t pot;
};

struct sl1tick_s {
	struct secu_s secu;
	struct l1tick_s tick;
};


/* (de)serialisers */
static inline void
udpc_seria_add_tick_by_ts_header(udpc_seria_t sctx, tick_by_ts_hdr_t t)
{
	udpc_seria_add_ui32(sctx, t->ts);
	udpc_seria_add_ui32(sctx, t->types);
	return;
}

static inline void
udpc_seria_des_tick_by_ts_hdr(tick_by_ts_hdr_t t, udpc_seria_t sctx)
{
	t->ts = udpc_seria_des_ui32(sctx);
	t->types = udpc_seria_des_ui32(sctx);
	return;
}

static inline void
udpc_seria_add_secu(udpc_seria_t sctx, secu_t secu)
{
	udpc_seria_add_ui32(sctx, secu->instr);
	udpc_seria_add_ui32(sctx, secu->unit);
	udpc_seria_add_ui32(sctx, secu->pot);
	return;
}

static inline bool
udpc_seria_des_secu(secu_t t, udpc_seria_t sctx)
{
	if ((t->instr = udpc_seria_des_ui32(sctx)) == 0) {
		/* no gaid? fuck off early */
		return false;
	}
	/* currency */
	t->unit = udpc_seria_des_ui32(sctx);
	/* exchange */	
	t->pot = udpc_seria_des_ui32(sctx);
	return true;
}

static inline void
udpc_seria_add_sl1tick(udpc_seria_t sctx, sl1tick_t t)
{
	udpc_seria_add_ui32(sctx, (int32_t)t->secu.instr);
	udpc_seria_add_ui32(sctx, (int32_t)t->secu.unit);
	udpc_seria_add_ui32(sctx, (int32_t)t->secu.pot);
	udpc_seria_add_ui32(sctx, (uint32_t)t->tick.ts);
	udpc_seria_add_ui32(sctx, (uint32_t)t->tick.nsec);
	udpc_seria_add_byte(sctx, (uint8_t)t->tick.tt);
	udpc_seria_add_ui32(sctx, (uint32_t)t->tick.value);
	return;
}

static inline bool
udpc_seria_des_sl1tick(sl1tick_t t, udpc_seria_t sctx)
{
	if (!udpc_seria_des_secu(&t->secu, sctx)) {
		/* the security deserialisation got cunted, fuck off early */
		return false;
	}
	/* now the meaningful fields */
	t->tick.ts = udpc_seria_des_ui32(sctx);
	t->tick.nsec = udpc_seria_des_ui32(sctx);
	t->tick.tt = udpc_seria_des_byte(sctx);
	t->tick.value = udpc_seria_des_ui32(sctx);
	return true;
}

#endif	/* INCLUDED_seria_h_ */
