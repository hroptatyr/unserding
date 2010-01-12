/*** dso-tseries-ute.c -- ute tick files
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>

#include <sushi/sl1t.h>
#include <sushi/sl1tfile.h>
/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"
#include "unserding-private.h"
/* for higher level packet handling */
#include "seria-proto-glue.h"
#include "tscube.h"
#define NO_LEGACY
#include "tseries-private.h"


/* ugly */
#include "uteseries.c"


static inline uint16_t
__fhdr_nsecs(utehdr_t fhdr)
{
/* assumed that fhdr contains secs, this is not checked */
	uint32_t ploff = utehdr_payload_offset(fhdr);
	uint32_t sz1 = sizeof(su_secu_t) + sizeof(struct time_range_s);
	return (uint16_t)(ploff / sz1 - UTEHDR_FIRST_SECIDX);
}

static uint16_t
find_secu_idx(ute_ctx_t ctx, su_secu_t sec)
{
/* hardcoded as fuck */
	utehdr_t fhdr = ctx->fio->fhdr;
	for (index_t i = UTEHDR_FIRST_SECIDX;
	     i < __fhdr_nsecs(fhdr) + UTEHDR_FIRST_SECIDX; i++) {
		if (fhdr->sec[i].mux == sec.mux) {
			return i;
		}
	}
	return 0;
}

/* belongs in uteseries.c? */
static size_t
__cp_tk(const_sl1t_t *tgt, ute_ctx_t ctx, sl1t_t src)
{
	const_scom_thdr_t scsrc = (const void*)src;
	time32_t ts = scom_thdr_sec(scsrc);
	uint16_t idx = scom_thdr_tblidx(scsrc);
	uint16_t ttf = scom_thdr_ttf(scsrc);
	tblister_t tbl = __find_blister_by_ts(ctx->tbl, ts);
	size_t nt;
	const_sl1t_t t;

	if (UNLIKELY(tbl == NULL)) {
		return 0;
	} else if (!(tbl->ttfbs[idx] & (1 << ttf))) {
		return 0;
	}

	/* get the corresponding tick block */
	nt = sl1t_fio_read_ticks(ctx->fio, &t, tbl->btk, tbl->etk);
	/* assume ascending order */
	for (const_sl1t_t tk = &t[nt - 1]; tk >= t; tk--) {
		uint16_t tkidx = sl1t_tblidx(tk);
		uint16_t tkttf = sl1t_ttf(tk);

		if (tkidx == idx && tkttf == ttf && sl1t_stmp_sec(tk) <= ts) {
			/* not thread-safe */
			*tgt = tk;
			return t + nt - tk;
		}
	}
	return 0;
}

static size_t
fetch_tick(
	sl1t_t tgt, size_t tsz, tsc_key_t k, void *uval,
	time32_t beg, time32_t end)
{
	ute_ctx_t ctx = uval;
	uint16_t idx = find_secu_idx(ctx, k->secu);
	const_sl1t_t t[1];
	size_t nt, res = 0;

	/* different keying now */
	sl1t_set_stmp_sec(tgt, beg);
	/* use the global secu -> tblidx map */
	sl1t_set_tblidx(tgt, idx);
	sl1t_set_ttf(tgt, k->ttf);

	if ((nt = __cp_tk(t, ctx, tgt)) == 0) {
		return 0;
	}
	/* otherwise iterate */
	for (const_sl1t_t tp = t[0]; tp < t[0] + nt && res < tsz; tp++) {
		uint16_t tkidx = sl1t_tblidx(tp);
		uint16_t tkttf = sl1t_ttf(tp);

		/* assume ascending order */
		if (sl1t_stmp_sec(tp) > end) {
			break;
		} else if (tkidx == idx && tkttf == k->ttf) {
			/* not thread-safe */
			*tgt++ = *tp;
			res++;
		}
	}
	return res;
}

static void
fetch_urn(tsc_key_t UNUSED(key), void *uval)
{
/* make me thread-safe and declare me */
	if (uval == NULL) {
		return;
	}

	UD_DEBUG("inspecting sl1t ...");
	//fill_urns(my_ctx);
	UD_DBGCONT("done\n");
	return;
}

static struct tsc_ops_s ute_ops[1] = {{
		.fetch_cb = fetch_tick,
		.urn_refetch_cb = fetch_urn,
	}};

struct cb_clo_s {
	tscube_t c;
	ute_ctx_t ctx;
};

static void
fill_cube_cb(uint16_t UNUSED(idx), su_secu_t sec, void *clo)
{
/* hardcoded as fuck */
	struct cb_clo_s *fcclo = clo;
	tscube_t c = fcclo->c;
	ute_ctx_t ctx = fcclo->ctx;
	struct tsc_ce_s ce = {
		.key = {{
				.beg = 915148800,
				.end = 0x7fffffff,
				.ttf = SL1T_TTF_FIX,
				.msk = 1 | 2 | 4 | 8 | 16,
				.secu = sec,
			}},
		.ops = ute_ops,
	};

	ce.uval = ctx;
	tsc_add(c, &ce);
	return;
}


static ute_ctx_t my_ctx;
static const char my_hardcoded_file[] = "/home/freundt/.unserding/eur.ute";

void
dso_tseries_ute_LTX_init(void *UNUSED(clo))
{
	struct cb_clo_s cbclo = {.c = gcube, .ctx = my_ctx};

	UD_DEBUG("mod/tseries-ute: loading ...");
	my_ctx = open_ute_file(my_hardcoded_file);
	ute_inspect(my_ctx);
	sl1t_fio_trav_stbl(my_ctx->fio, fill_cube_cb, NULL, &cbclo);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: unloading ...");
	close_ute_file(my_ctx);
	my_ctx = NULL;
	UD_DBGCONT("done\n");
	return;
}

/* dso-tseries-ute.c ends here */
