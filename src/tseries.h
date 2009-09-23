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

#include <stdbool.h>
#include <pfack/tick.h>
#include <time.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"
#include "urn.h"

/* tick services */
#if !defined index_t
# define index_t	size_t
#endif	/* !index_t */

/**
 * Service 4220:
 * Find ticks of one time stamp over instruments (market snapshot).
 * This service can be used to get a succinct set of ticks, usually the
 * last ticks before a given time stamp, for several instruments.
 * The ticks to return can be specified in a bitset.
 *
 * sig: 4220(ui32 ts, ui32 types, (ui32 secu, ui32 fund, ui32 exch)+)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The TYPES parameter is a bitset made up of PFTB_* values as specified
 * in pfack/tick.h */
#define UD_SVC_TICK_BY_TS	0x4220

/**
 * Time series (per instrument) and market snapshots (per point in time)
 * need the best of both worlds, low latency on the one hand and small
 * memory footprint on the other, yet allowing for a rich variety of
 * gatherable information.
 * Specifically we want to answer questions like:
 * -
 * -
 **/
#define UD_SVC_TICK_BY_INSTR	0x4222

/* points to the tick-type of the day */
#define sl1tick_s		sl1tp_s
#define sl1tick_t		sl1tp_t
#define fill_sl1tick_shdr	fill_sl1tp_shdr
#define fill_sl1tick_tick	fill_sl1tp_tick
#define sl1tick_value		sl1tp_value
#define sl1tick_tick_type	sl1tp_tt
#define sl1tick_timestamp	sl1tp_ts
#define sl1tick_msec		sl1tp_msec
#define sl1tick_instr		sl1tp_inst
#define sl1tick_unit		sl1tp_unit
#define sl1tick_pot		sl1tp_exch

/* migrate to ffff tseries */
typedef struct tseries_s *tseries_t;
typedef struct tser_cons_s *tser_cons_t;
typedef struct tser_qry_intv_s *tser_qry_intv_t;
typedef struct tser_pktbe_s *tser_pktbe_t;

typedef struct tser_pkt_s *tser_pkt_t;

typedef struct secu_s *secu_t;
typedef struct tick_by_ts_hdr_s *tick_by_ts_hdr_t;
typedef struct tick_by_instr_hdr_s *tick_by_instr_hdr_t;
typedef struct sl1tp_s *sl1tp_t;
typedef struct sl1t_s *sl1t_t;

/** (semi)sparse once-a-day level 1 ticks */
typedef struct sl1oadt_s *sl1oadt_t;
/** days since epoch type, goes till ... */
typedef uint16_t dse16_t;

typedef union time_dse_u udate_t;
typedef uint32_t date_t;

/**
 * Condensed version of:
 *   // upper 10 bits
 *   uint10_t millisec_flags;
 *   // lower 6 bits
 *   uint6_t tick_type;
 * where the millisec_flags slot uses the values 0 to 999 if it
 * is a valid available tick and denotes the milliseconds part
 * of the timestamp and special values 1000 to 1023 if it's not,
 * whereby:
 * - 1023 TICK_NA denotes a tick that is not available, as in it
 *   is unknown to the system whether or not it exists
 * - 1022 TICK_NE denotes a tick that is known not to exist
 * - 1021 TICK_OLD denotes a tick that is too old and hence
 *   meaningless in the current context
 * - 1020 TICK_SOON denotes a tick that is known to exist but
 *   is out of reach at the moment, a packet retransmission will
 *   be necessary
 * In either case the actual timestamp and value slot of the tick
 * structure has become meaningless, therefore it is possible to
 * transfer even shorter, denser versions of the packet in such
 * cases, saving 64 bits, at the price of non-uniformity.
 **/
typedef uint16_t l1t_auxinfo_t;

#define TICK_NA		((uint16_t)1023)
#define TICK_NE		((uint16_t)1022)
#define TICK_OLD	((uint16_t)1021)
#define TICK_SOON	((uint16_t)1020)


/**
 * Sparse level 1 ticks, packed. */
struct sl1tp_s {
	uint32_t inst;
	uint32_t unit;
	uint16_t exch;
	l1t_auxinfo_t auxinfo;
	/* these are actually optional */
	uint32_t ts;
	uint32_t val;
};

struct tick_by_ts_hdr_s {
	time_t ts;
	uint32_t types;
};

struct secu_s {
	uint32_t instr;
	uint32_t unit;
	uint16_t pot;
};

struct tick_by_instr_hdr_s {
	struct secu_s secu;
	uint32_t types;
};

struct sl1t_s {
	struct secu_s secu;
	struct l1tick_s tick;
};

struct tseries_s {
	size_t size;
	/** vector of cons cells, nope, it's a list at the mo */
	tser_cons_t conses;
};

/* packet of 10 ticks, fuck ugly */
struct tser_pkt_s {
	monetary32_t t[10];
};

union time_dse_u {
	time_t time;
	dse16_t dse16;
};

struct tser_pktbe_s {
	date_t beg;
	date_t end;
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

struct sl1oadt_s {
	uint32_t instr;
	uint32_t unit;
	/** consists of 10 bits for pot, 6 bits for tt */
	uint16_t mux;
	/** number of values in the vector below */
	uint8_t nticks;
	/** days since epoch */
	dse16_t dse;
	uint32_t value[252];
};


/* the global tscache container */
/**
 * The time series cache data type, just a husk. */
typedef void *tscache_t;

extern tscache_t make_tscache(void);
extern void free_tscache(tscache_t tsc);
extern size_t tscache_size(tscache_t tsc);

extern tseries_t tscache_bang_series(tscache_t tsc, secu_t s, tseries_t ts);
extern tseries_t find_tseries_by_secu(tscache_t tsc, secu_t secu);


/**
 * Deliver a tick packet for S at TS.
 * \param hdl the unserding handle to use
 * \param tgt a buffer that holds the tick packet, should be at least
 *   UDPC_PLLEN bytes long
 * \param s the security to look up
 * \param bs a bitset to filter for ticks
 * \param ts the timestamp at which S shall be valuated
 * \return the length of the tick packet
 **/
extern size_t
ud_find_one_price(ud_handle_t hdl, char *tgt, secu_t s, uint32_t bs, time_t ts);

/**
 * Deliver a packet storm of ticks for instruments specified by S at TS.
 * Call the callback function CB() for every tick in the packet storm. */
extern void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(sl1tick_t, void *clo), void *clo,
	secu_t s, size_t slen,
	uint32_t bs, time_t ts);

/**
 * Deliver a packet storm of ticks for S at specified times TS.
 * Call the callback function CB() for every tick in the packet storm. */
extern void
ud_find_ticks_by_instr(
	ud_handle_t hdl,
	void(*cb)(sl1oadt_t, void *clo), void *clo,
	secu_t s, uint32_t bs,
	time_t *ts, size_t tslen);

/**
 * Annotation space inside the cache. */
typedef struct ts_anno_s {
	uint32_t instr;
	const_urn_t urn;
	time_t from, to;
	uint32_t types;
} *ts_anno_t;

/**
 * Like tseries but with annotation goodness. */
typedef struct anno_tseries_s *anno_tseries_t;
struct anno_tseries_s {
	struct tseries_s tseries;
	struct ts_anno_s anno;
};

/**
 * Return the annotation of a tseries ts from within a tscache object. */
static inline ts_anno_t
tscache_tseries_annotation(tseries_t i)
{
	return &(((anno_tseries_t)(void*)i)->anno);
}


/* inlines, type (de)muxers */
static inline l1t_auxinfo_t
l1t_auxinfo(uint16_t msec, uint8_t tt)
{
	return (msec << 6) | (tt & 0x3f);
}

static inline l1t_auxinfo_t
l1t_auxinfo_set_msec(l1t_auxinfo_t src, uint16_t msec)
{
	return (src & 0x3f) | (msec << 6);
}

static inline l1t_auxinfo_t
l1t_auxinfo_set_tt(l1t_auxinfo_t src, uint8_t tt)
{
	return (src & 0xffc0) | (tt & 0x3f);
}

static inline uint16_t
l1t_auxinfo_msec(l1t_auxinfo_t ai)
{
	return (ai >> 6);
}

static inline uint8_t
l1t_auxinfo_tt(l1t_auxinfo_t ai)
{
	return (ai & 0x3f);
}

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

static inline date_t
tseries_beg(tseries_t tser)
{
	if (tser->size > 0) {
		return tser->conses->pktbe.beg;
	}
}

static inline date_t
tseries_end(tseries_t tser)
{
	if (tser->size > 0) {
		return tseries_last(tser)->pktbe.end;
	}
}

/* the sl1tp packed tick, consumes 12 or 20 bytes */
static inline void
fill_sl1tp_shdr(sl1tp_t l1t, uint32_t secu, uint32_t fund, uint16_t exch)
{
	l1t->inst = secu;
	l1t->unit = fund;
	l1t->exch = exch;
	return;
}

static inline void
fill_sl1tp_tick(sl1tp_t l1t, time_t ts, uint16_t msec, uint8_t tt, uint32_t v)
{
	l1t->auxinfo = l1t_auxinfo(msec, tt);
	l1t->ts = ts;
	l1t->val = v;
	return;
}

static inline uint32_t
sl1tp_value(sl1tp_t t)
{
	return t->val;
}

static inline uint8_t
sl1tp_tt(sl1tp_t t)
{
	return l1t_auxinfo_tt(t->auxinfo);
}

static inline uint16_t
sl1tp_msec(sl1tp_t t)
{
	return l1t_auxinfo_msec(t->auxinfo);
}

static inline uint32_t
sl1tp_ts(sl1tp_t t)
{
	return t->ts;
}

static inline uint32_t
sl1tp_inst(sl1tp_t t)
{
	return t->inst;
}

static inline uint32_t
sl1tp_unit(sl1tp_t t)
{
	return t->unit;
}

static inline uint16_t
sl1tp_exch(sl1tp_t t)
{
	return t->exch;
}

/* them old sl1t ticks, consumes 28 bytes */
static inline void
fill_sl1t_secu(sl1t_t l1t, uint32_t secu, uint32_t fund, uint16_t exch)
{
	l1t->secu.instr = secu;
	l1t->secu.unit = fund;
	l1t->secu.pot = exch;
	return;
}

static inline void
fill_sl1t_tick(sl1t_t l1t, time_t ts, uint16_t msec, uint8_t tt, uint32_t v)
{
	l1t->tick.ts = ts;
	l1t->tick.nsec = msec * 1000000;
	l1t->tick.tt = tt;
	l1t->tick.value = v;
	return;
}

/* sl1oadt accessors */
static inline dse16_t
time_to_dse(time_t ts)
{
	return (dse16_t)(ts / 86400);
}

static inline time_t
dse_to_time(dse16_t ts)
{
	return (time_t)(ts * 86400);
}

static inline uint32_t
sl1oadt_instr(sl1oadt_t t)
{
	return t->instr;
}

static inline uint32_t
sl1oadt_unit(sl1oadt_t t)
{
	return t->unit;
}

static inline uint16_t
sl1oadt_pot(sl1oadt_t t)
{
	return (uint16_t)(t->mux >> 6);
}

static inline uint8_t
sl1oadt_tick_type(sl1oadt_t t)
{
	return (uint8_t)(t->mux & 0x3f);
}

static inline dse16_t
sl1oadt_dse(sl1oadt_t t)
{
	return t->dse;
}

static inline uint8_t
find_index_in_pkt(dse16_t dse)
{
/* find the index of the date encoded in dse inside a tick bouquet */
	dse16_t anchor = time_to_dse(442972800);
	uint8_t res = (dse - anchor) % 14;
	if (res > 0) {
		return res - 1;
	} else {
		return 13;
	}
}

static inline dse16_t
tser_pkt_beg_dse(dse16_t dse)
{
	uint8_t sub = find_index_in_pkt(dse);
	return dse - sub;
}

static inline uint32_t
sl1oadt_value(sl1oadt_t oadt, uint8_t idx)
{
	return oadt->value[idx];
}

/**
 * Instantiate a sparse level 1 once-a-day tick for HDR. */
static inline void
fill_sl1oadt_1(
	sl1oadt_t oadt, tick_by_instr_hdr_t hdr,
	uint8_t tt, dse16_t dse, uint32_t pri)
{
	oadt->instr = hdr->secu.instr;
	oadt->unit = hdr->secu.unit;
	oadt->mux = ((hdr->secu.pot & 0x3ff) << 6) | (tt & 0x3f);
	oadt->dse = dse;
	oadt->nticks = 1;
	oadt->value[0] = pri;
	return;
}

/**
 * Instantiate a sequence of level 1 once-a-day ticks for HDR. */
static inline void
fill_sl1oadt(
	sl1oadt_t oadt, tick_by_instr_hdr_t hdr,
	uint8_t tt, dse16_t dse, uint32_t pri[], uint8_t nticks)
{
	oadt->instr = hdr->secu.instr;
	oadt->unit = hdr->secu.unit;
	oadt->mux = ((hdr->secu.pot & 0x3ff) << 6) | (tt & 0x3f);
	oadt->dse = dse;
	oadt->nticks = nticks;
	for (uint8_t i = 0; i < nticks; i++) {
		oadt->value[i] = pri[i];
	}
	return;
}


/* (de)serialisers */
static inline void
udpc_seria_add_tick_by_ts_hdr(udpc_seria_t sctx, tick_by_ts_hdr_t t)
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
		t->unit = 0;
		t->pot = 0;
		return false;
	}
	/* currency */
	t->unit = udpc_seria_des_ui32(sctx);
	/* exchange */	
	t->pot = udpc_seria_des_ui32(sctx);
	return true;
}

static inline void
udpc_seria_add_tick_by_instr_hdr(udpc_seria_t sctx, tick_by_instr_hdr_t h)
{
	udpc_seria_add_secu(sctx, &h->secu);
	udpc_seria_add_ui32(sctx, h->types);
	return;
}

static inline void
udpc_seria_des_tick_by_instr_hdr(tick_by_instr_hdr_t h, udpc_seria_t sctx)
{
	udpc_seria_des_secu(&h->secu, sctx);
	h->types = udpc_seria_des_ui32(sctx);
	return;
}

static inline void
udpc_seria_add_sl1tick(udpc_seria_t sctx, sl1tick_t t)
{
	udpc_seria_add_data(sctx, t, sizeof(*t));
	return;
}

static inline bool
udpc_seria_des_sl1tick(sl1tick_t t, udpc_seria_t sctx)
{
	return udpc_seria_des_data_into(t, sizeof(*t), sctx) > 0;
}

static inline void
udpc_seria_add_sl1oadt(udpc_seria_t sctx, sl1oadt_t oadt)
{
	/* we send of the first 3 slots, 32b 32b 16b, as secu */
	udpc_seria_add_ui32(sctx, sl1oadt_instr(oadt));
	udpc_seria_add_ui32(sctx, sl1oadt_unit(oadt));
	udpc_seria_add_ui16(sctx, oadt->mux);
	/* now comes the semi-sparse entry */
	udpc_seria_add_byte(sctx, oadt->nticks);
	udpc_seria_add_ui16(sctx, oadt->dse);
	/* and the vector */
	for (uint8_t i = 0; i < oadt->nticks; i++) {
		udpc_seria_add_ui32(sctx, oadt->value[i]);
	}
	return;
}

static inline size_t
udpc_seria_des_sl1oadt(sl1oadt_t oadt, udpc_seria_t sctx)
{
	/* we send of the first 3 slots, 32b 32b 16b, as secu */
	oadt->instr = udpc_seria_des_ui32(sctx);
	oadt->unit = udpc_seria_des_ui32(sctx);
	oadt->mux = udpc_seria_des_ui16(sctx);
	/* now comes the semi-sparse entry */
	oadt->nticks = udpc_seria_des_byte(sctx);
	oadt->dse = udpc_seria_des_ui16(sctx);
	/* and the vector */
	for (uint8_t i = 0; i < oadt->nticks; i++) {
		oadt->value[i] = udpc_seria_des_ui32(sctx);
	}
	return oadt->nticks;
}

#endif	/* INCLUDED_tseries_h_ */
