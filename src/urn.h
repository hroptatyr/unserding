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
};

struct urn_unk_s {
	char *fld_id;
	char *fld_date;
};

struct urn_oad_c_s {
	char *fld_id;
	char *fld_date;
	char *fld_close;
};

struct urn_oad_ohlc_s {
	char *fld_id;
	char *fld_date;
	char *fld_open;
	char *fld_high;
	char *fld_low;
	char *fld_close;
};

struct urn_oad_ohlcv_s {
	char *fld_id;
	char *fld_date;
	char *fld_open;
	char *fld_high;
	char *fld_low;
	char *fld_close;
	char *fld_volume;
};

union fld_names_u {
	struct urn_unk_s unk;
	struct urn_oad_c_s oad_c;
	struct urn_oad_ohlc_s oad_ohlc;
	struct urn_oad_ohlcv_s oad_ohlcv;
};

struct urn_s {
	urn_type_t type;
	char *dbtbl;
	union fld_names_u flds;
};


/* inlines */
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
urn_fld_close(const_urn_t urn)
{
	switch (urn->type) {
	case URN_OAD_C:
		return urn->flds.oad_c.fld_close;
	case URN_OAD_OHLC:
		return urn->flds.oad_ohlc.fld_close;
	case URN_OAD_OHLCV:
		return urn->flds.oad_ohlcv.fld_close;
	case URN_UNK:
	case URN_L1_TICK:
	case URN_L1_BAT:
	case URN_L1_PEG:
	case URN_L1_BATPEG:
	default:
		return NULL;
	}
}

static inline const char*
urn_fld_dbtbl(const_urn_t urn)
{
	return urn->dbtbl;
}

#endif	/* INCLUDED_urn_h_ */
