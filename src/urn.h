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

#if !defined INCLUDED_urn_h_
#define INCLUDED_urn_h_

typedef enum urn_type_e urn_type_t;
typedef struct urn_s *urn_t;
typedef const struct urn_s *const_urn_t;

enum urn_type_e {
	URN_UNK,
	URN_OAD_C,
	URN_OAD_OHLC,
	URN_OAD_OHLCV,
	/* just a tick, the meaning is encoded in a tick-type slot */
	URN_L1_TICK,
	/* bid-ask-trade as one vector */
	URN_L1_BAT,
	/* peg tick over all bourses */
	URN_L1_PEG,
	/* peg snap over all bourses and bid-ask-trade */
	URN_L1_BATPEG,
	/* uterus candle */
	URN_UTE_CDL,
};

struct urn_unk_s {
	const char *fld_id;
	const char *fld_date;
};

struct urn_batfx_ohlcv_s {
	const char *fld_id;
	const char *fld_date;
	const char *fld_bo;
	const char *fld_bh;
	const char *fld_bl;
	const char *fld_bc;
	const char *fld_bv;
	const char *fld_ao;
	const char *fld_ah;
	const char *fld_al;
	const char *fld_ac;
	const char *fld_av;
	const char *fld_to;
	const char *fld_th;
	const char *fld_tl;
	const char *fld_tc;
	const char *fld_tv;
	const char *fld_f;
	const char *fld_x;
};

union fld_names_u {
	struct urn_unk_s unk;
	struct urn_batfx_ohlcv_s batfx_ohlcv;
};

struct urn_s {
	urn_type_t type;
	char *dbtbl;
	union fld_names_u flds;
};


/* inlines */
static inline void
init_urn(urn_t __attribute__((unused)) urn)
{
	return;
}

static inline urn_type_t
urn_type(const_urn_t urn)
{
	return urn->type;
}

static inline const char*
urn_fld_id(const_urn_t urn)
{
	return urn->flds.unk.fld_id;
}

static inline const char*
urn_fld_date(const_urn_t urn)
{
	return urn->flds.unk.fld_date;
}

static inline const char*
urn_fld_bop(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_bo;
}

static inline const char*
urn_fld_bhp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_bh;
}

static inline const char*
urn_fld_blp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_bl;
}

static inline const char*
urn_fld_bcp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_bc;
}

static inline const char*
urn_fld_bv(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_bv;
}

static inline const char*
urn_fld_aop(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_ao;
}

static inline const char*
urn_fld_ahp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_ah;
}

static inline const char*
urn_fld_alp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_al;
}

static inline const char*
urn_fld_acp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_ac;
}

static inline const char*
urn_fld_av(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_av;
}

static inline const char*
urn_fld_top(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_to;
}

static inline const char*
urn_fld_thp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_th;
}

static inline const char*
urn_fld_tlp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_tl;
}

static inline const char*
urn_fld_tcp(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_tc;
}

static inline const char*
urn_fld_tv(const_urn_t urn)
{
	return urn->flds.batfx_ohlcv.fld_tv;
}

static inline const char*
urn_fld_dbtbl(const_urn_t urn)
{
	return urn->dbtbl;
}

#endif	/* INCLUDED_urn_h_ */
