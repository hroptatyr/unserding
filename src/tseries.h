/*** tseries.h -- stuff that is soon to be replaced by ffff's tseries
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

#if !defined INCLUDED_tseries_h_
#define INCLUDED_tseries_h_

/* migrate to ffff tseries */
typedef struct tseries_s *tseries_t;
typedef struct tser_cons_s *tser_cons_t;
typedef struct tser_qry_intv_s *tser_qry_intv_t;
typedef struct tser_pktbe_s *tser_pktbe_t;

typedef struct tser_pkt_s *tser_pkt_t;

struct tseries_s {
	size_t size;
	/** vector of cons cells, nope, it's a list at the mo */
	tser_cons_t conses;
};

/* packet of 10 ticks, fuck ugly */
struct tser_pkt_s {
	monetary32_t t[10];
};

struct tser_pktbe_s {
	time_t beg;
	time_t end;
	struct tser_pkt_s pkt;
};

struct tser_cons_s {
	tser_cons_t next;
	time_t cache_expiry;
	struct tser_pktbe_s pktbe;
};


struct tser_qry_intv_s {
	time_t beg;
	time_t end;
};

extern void
fetch_ticks_intv_mysql(tser_pkt_t, tick_by_instr_hdr_t, time_t beg, time_t end);


/* inlines */
static inline tser_cons_t
tseries_first(tseries_t tser)
{
	if (tser->size > 0) {
		return tser->conses;
	}
	return NULL;
}

static inline tser_cons_t
tseries_last(tseries_t tser)
{
	tser_cons_t res;
	for (res = tser->conses; res && res->next; res = res->next);
	return res;
}

static inline time_t
tseries_beg(tseries_t tser)
{
	if (tser->size > 0) {
		return tser->conses->pktbe.beg;
	}
}

static inline time_t
tseries_end(tseries_t tser)
{
	if (tser->size > 0) {
		return tseries_last(tser)->pktbe.end;
	}
}

#endif	/* INCLUDED_tseries_h_ */
