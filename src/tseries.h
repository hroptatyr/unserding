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
#include <time.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"
/* comes from sushi */
#include <sushi/secu.h>
#include <sushi/scommon.h>
#include <sushi/sl1t.h>
#include <sushi/scdl.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* tick services */
#if !defined index_t
# define index_t	size_t
#endif	/* !index_t */

/**
 * Time series (per instrument) and market snapshots (per point in time)
 * need the best of both worlds, low latency on the one hand and small
 * memory footprint on the other, yet allowing for a rich variety of
 * gatherable information.
 * Specifically we want to answer questions like:
 * -
 * -
 **/

/**
 * Service 4220:
 * Find ticks of one time stamp over instruments (market snapshot).
 * This service can be used to get a succinct set of ticks, usually the
 * last ticks before a given time stamp, for several instruments.
 * The ticks to return can be specified in a bitset.
 *
 * sig: 4220(ui32 tsb, ui32 tse, ui32 types, (ui64 susecu)+)
 *   As a wildcard the null secu can be used.
 *
 * The TYPES parameter is a bitset made up of PFTB_* values as specified
 * in pfack/tick.h */
#define UD_SVC_TICK_BY_TS	0x4220

/**
 * Service 4222:
 * Find ticks of one instrument at several given points in time.
 *
 * sig: 4222(ui32 secu, ui32 fund, ui32 exch, ui32 types, (ui32 ts)+)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The TYPES parameter is a bitset made up of PFTB_* values as specified
 * in pfack/tick.h
 * This service is best used in conjunction with `ud_find_ticks_by_instr'
 * because several optimisation strategies apply. */
#define UD_SVC_TICK_BY_INSTR	0x4222

/**
 * Service 4224:
 * Get URNs by secu.
 *
 * sig: 4224(ui32 secu, ui32 fund, ui32 exch)
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The answer will be a rpl4224_s object, which is a tseries_s in disguise. */
#define UD_SVC_GET_URN		0x4224

/**
 * Service 4226:
 * Refetch URNs. */
#define UD_SVC_FETCH_URN	0x4226


/**
 * Service 4228:
 * Find ticks for one point in time given an instrument filter.
 * EXPERIMENTAL
 *
 * sig: 4228(ui32 ts [, ui64 susecu [, ui32 types]])
 *   As a wildcard for all funds or all exchanges 0x00000000 can be used.
 *
 * The TYPES parameter is a bitset made up of SL1T_TTF_* values as specified
 * in sushi/sl1t.h */
#define UD_SVC_MKTSNP		0x4228

/**
 * Adminstrative service 3f02.  List all cached boxes in the cube. */
#define UD_SVC_LIST_BOXES	0x3f02

typedef struct tslab_s *tslab_t;

typedef struct scom_secu_s *scom_secu_t;

typedef uint32_t date_t;

#define TICK_NA		((uint16_t)1023)
#define TICK_NE		((uint16_t)1022)
#define TICK_OLD	((uint16_t)1021)
#define TICK_SOON	((uint16_t)1020)

#define SCOM_ONHOLD	(1022)

/* type bitsets */
typedef uint32_t tbs_t;

/**
 * Tslabs, roughly metadata that describe the slabs of tseries. */
struct tslab_s {
	su_secu_t secu;
	time_t from, till;
	tbs_t types;
};

/**
 * Speical sparse tick when the symbol table is not delivered. */
struct scom_secu_s {
	su_secu_t s;
	const struct scom_thdr_s t[];
};


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
ud_find_one_price(ud_handle_t h, char *tgt, su_secu_t s, tbs_t bs, time_t ts);

/**
 * Deliver a packet storm of ticks for instruments specified by S at TS.
 * Call the callback function CB() for every tick in the packet storm. */
extern void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(su_secu_t, scom_t, void *clo), void *clo,
	su_secu_t *s, size_t slen,
	tbs_t bs, time_t ts);

/**
 * Deliver a packet storm of ticks for S at specified times TS.
 * Call the callback function CB() for every tick in the packet storm. */
extern void
ud_find_ticks_by_instr(
	ud_handle_t hdl,
	void(*cb)(su_secu_t, scom_t, void *clo), void *clo,
	su_secu_t s, tbs_t bs,
	time_t *ts, size_t tslen);

/**
 * Deliver an entire market snapshot at TS, optionally filtering for
 * secus specified by S and tick types BS.  For each hit CB will be
 * called as specified. */
void
ud_find_mktsnp(
	ud_handle_t hdl,
	void(*cb)(su_secu_t, scom_t, void *clo), void *clo,
	time_t ts, su_secu_t s, tbs_t bs);


/* (de)serialisers */
static inline void
udpc_seria_add_secu(udpc_seria_t sctx, su_secu_t secu)
{
/* we assume sizeof(su_secu_t) == 8 */
	udpc_seria_add_ui64(sctx, su_secu_ui64(secu));
	return;
}

static inline su_secu_t
udpc_seria_des_secu(udpc_seria_t sctx)
{
/* we assume sizeof(su_secu_t) == 8 */
	uint64_t wire = udpc_seria_des_ui64(sctx);
	return su_secu_set_ui64(wire);
}

static inline void
udpc_seria_add_scom_secu(udpc_seria_t sctx, su_secu_t s, scom_t t)
{
/* this is an almost copy of udpc_seria_add_data() */
	size_t ssz = sizeof(s);
	size_t tsz = scom_byte_size(t);
	sctx->msg[sctx->msgoff + 0] = UDPC_TYPE_DATA;
	sctx->msg[sctx->msgoff + 1] = (uint8_t)(ssz + tsz);
	memcpy(&sctx->msg[sctx->msgoff + DATA_HDR_LEN], &s, ssz);
	memcpy(&sctx->msg[sctx->msgoff + DATA_HDR_LEN + ssz], t, tsz);
	sctx->msgoff += DATA_HDR_LEN + (ssz + tsz);
	return;
}

static inline uint8_t
udpc_seria_des_scom_secu(udpc_seria_t sctx, const struct scom_secu_s **tgt)
{
	return udpc_seria_des_data(sctx, (const void**)tgt);
}

static inline void
udpc_seria_add_tbs(udpc_seria_t sctx, tbs_t bs)
{
	udpc_seria_add_ui32(sctx, bs);
	return;
}

static inline tbs_t
udpc_seria_des_tbs(udpc_seria_t sctx)
{
	return udpc_seria_des_ui32(sctx);
}

static inline void
udpc_seria_add_sl1t(udpc_seria_t sctx, const_sl1t_t t)
{
	udpc_seria_add_data(sctx, t, sizeof(*t));
	return;
}

static inline bool
udpc_seria_des_sl1t(sl1t_t t, udpc_seria_t sctx)
{
	/* have to use a bigger size here */
	return udpc_seria_des_data_into(t, sizeof(struct scdl_s), sctx) > 0;
}

static inline void
udpc_seria_add_scdl(udpc_seria_t sctx, const_scdl_t t)
{
	udpc_seria_add_data(sctx, t, sizeof(*t));
	return;
}

static inline bool
udpc_seria_des_scdl(scdl_t t, udpc_seria_t sctx)
{
	/* have to use a bigger size here */
	return udpc_seria_des_data_into(t, sizeof(*t), sctx) > 0;
}

/* Attention, a tseries_t object gets transferred as tslab_t,
 * the corresponding udpc_seria_add_tseries() is in tscoll.h. */
static inline bool
udpc_seria_des_tslab(tslab_t ts, udpc_seria_t sctx)
{
	return udpc_seria_des_data_into(ts, sizeof(*ts), sctx) > 0;
}

static inline __attribute__((pure)) bool
scom_thdr_onhold_p(const_scom_thdr_t h)
{
	return scom_thdr_msec(h) == SCOM_ONHOLD;
}

static inline __attribute__((pure)) bool
sl1t_onhold_p(const_sl1t_t h)
{
	return scom_thdr_onhold_p(h->hdr);
}

static inline void
scom_thdr_mark_onhold(scom_thdr_t t)
{
	scom_thdr_set_msec(t, SCOM_ONHOLD);
	return;
}


/* our sl1tfile like (de)serialiser */


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_tseries_h_ */
