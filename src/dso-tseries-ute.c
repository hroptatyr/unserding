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
	size_t nsecs = __fhdr_nsecs(fhdr);
	for (index_t i = UTEHDR_FIRST_SECIDX;
	     i < nsecs + UTEHDR_FIRST_SECIDX; i++) {
		if (fhdr->sec[i].mux == sec.mux) {
			return i;
		}
	}
	return 0;
}

static su_secu_t
find_idx_secu(ute_ctx_t ctx, uint16_t idx)
{
/* hardcoded as fuck */
	utehdr_t fhdr = ctx->fio->fhdr;
	return fhdr->sec[idx];
}

static const_sl1t_t
__find_bb(const_sl1t_t t, size_t nt, time32_t ts)
{
	/* assume ascending order and that once linked mode is used, it's
	 * always used throughout the blister */
	index_t i, hi = nt, lo = 0;
	int fiddle = (scom_thdr_linked((const void*)(t))) ? 2 : 1;

	do {
		/* make sure i is even to cope with candles */
		i = ((hi + lo) / 2 & -fiddle);

		if (sl1t_stmp_sec(t + i) <= ts &&
		    sl1t_stmp_sec(t + i + fiddle) > ts) {
			/* was: return ar + i; so i points to the right index */
			return t + i;
		} else if (sl1t_stmp_sec(t + i) > ts) {
			/* prefer lower half */
			hi = i - fiddle;
		} else {
			/* strictly less, prefer upper half */
			lo = i + fiddle;
		}
	} while (true);
	/* not reached */
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
	const_sl1t_t t, res;

	UD_DEBUG("fetching %hu msk %x\n", ttf, tbl->ttfbs[idx]);
	if (UNLIKELY(tbl == NULL)) {
		return 0;
	} else if (!(tbl->ttfbs[idx] & (1 << (ttf & 0x0f)))) {
		return 0;
	}

	UD_DEBUG("fetching %u to %u\n", tbl->btk, tbl->etk);
	/* get the corresponding tick block */
	nt = sl1t_fio_read_ticks(ctx->fio, &t, tbl->btk, tbl->etk);
	/* this will give us a tick that is before the tick in question */
	if ((res = __find_bb(t, nt, ts)) == NULL) {
		return 0;
	}
	*tgt = res;
	return t + nt - res;
}

static size_t
fetch_tick(
	tsc_box_t tgt, size_t tsz, tsc_key_t k, void *uval,
	time32_t beg, time32_t end)
{
	ute_ctx_t ctx = uval;
	uint16_t idx;
	const_sl1t_t t[1];
	size_t nt, res = 0, i;
	struct sl1t_s sl1key[1];

	UD_DEBUG("fetching %i %i %x\n", beg, end, k->ttf);
	/* different keying now */
	sl1t_set_stmp_sec(sl1key, beg);
	/* use the global secu -> tblidx map */
	idx = find_secu_idx(ctx, k->secu);
	sl1t_set_tblidx(sl1key, idx);
	sl1t_set_ttf(sl1key, k->ttf);

	if ((nt = __cp_tk(t, ctx, sl1key)) == 0) {
		return 0;
	}
	UD_DEBUG("fine-grain over %zu ticks t[0]->ts %u\n", nt, sl1t_stmp_sec(t[0]));
	/* otherwise iterate */
	if (!scom_thdr_linked((const void*)(t[0]))) {
		tgt->pad = 1;
	} else {
		tsz /= (tgt->pad = 2);
	}

	for (i = res = 0; i < nt && res < tsz; i += tgt->pad) {
		uint16_t tkidx = sl1t_tblidx(&t[0][i]);
		uint16_t tkttf = sl1t_ttf(&t[0][i]) & 0x0f;
		time32_t tkts = sl1t_stmp_sec(&t[0][i]);

		/* assume ascending order */
		if (UNLIKELY(tkts > end)) {
			/* dont know if this is a good idea */
			tgt->end = tkts - 1;
			break;
		} else if (tkidx == idx && tkttf == k->ttf) {
			/* not thread-safe */
#if 0
			switch (tgt->pad) {
			case 1:
				tgt->sl1t[res] = t[0][i];
				break;
			case 2:
				tgt->scdl[res] = *(const_scdl_t)(&t[0][i]);
				break;
			default:
				break;
			}
#else
			memcpy(&tgt->sl1t[res * tgt->pad], &t[0][i],
			       sizeof(struct sl1t_s) * tgt->pad);
#endif
			res++;
		}
	}
	if (tgt->end == 0) {
		tgt->end = sl1t_stmp_sec(&tgt->sl1t[(res - 1) * tgt->pad]);
	}
	/* absolute count, candles or sl1t's */
	tgt->nt = res;
	tgt->beg = sl1t_stmp_sec(tgt->sl1t);
	UD_DEBUG("srch %u: tgt->beg %i  tgt->end %i\n",
		 sl1t_stmp_sec(sl1key), tgt->beg, tgt->end);
	return res * tgt->pad;
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

static void
fill_cube_by_bl_ttf(tscube_t c, tblister_t bl, ute_ctx_t ctx, uint16_t idx)
{
	uint64_t bs = bl->ttfbs[idx] >> 1;
	struct tsc_ce_s ce;

	if (bs == 0) {
		return;
	}
	/* otherwise */
	ce.key->beg = bl->bts;
	ce.key->end = bl->ets;
	ce.key->msk = 1 | 2 | 4 | 8 | 16;
	ce.key->secu = find_idx_secu(ctx, idx);
	ce.ops = ute_ops;
	ce.uval = ctx;
	/* go over all the tick types in the bitset BS */
	for (int j = 1; j < SL1T_TTF_VOL; j++, bs >>= 1) {
		if (bs & 1) {
			/* means tick type j is set */
			ce.key->ttf = j;
			tsc_add(c, &ce);
		}
	}
	return;
}

static void
fill_cube_by_bl(tscube_t c, tblister_t bl, ute_ctx_t ctx)
{
	for (uint16_t i = 0; i < countof(bl->ttfbs); i++) {
		fill_cube_by_bl_ttf(c, bl, ctx, i);
	}
	return;
}


static ute_ctx_t my_ctx;
static const char my_hardcoded_file[] = "/home/freundt/.unserding/eur.ute";

static ute_ctx_t my_ctx2;
static const char my_hardcoded_file2[] =
	"/home/freundt/.unserding/ute_IBk200.2009.5m.scdl";

void
dso_tseries_ute_LTX_init(void *UNUSED(clo))
{
	struct cb_clo_s cbclo = {.c = gcube};

	UD_DEBUG("mod/tseries-ute: loading ...");
	my_ctx = open_ute_file(my_hardcoded_file);
	ute_inspect(my_ctx);
	cbclo.ctx = my_ctx;
	sl1t_fio_trav_stbl(my_ctx->fio, fill_cube_cb, NULL, &cbclo);

	my_ctx2 = open_ute_file(my_hardcoded_file2);
	ute_inspect(my_ctx2);
	/* travel along the blister list and fill the cube */
	for (tblister_t tmp = my_ctx2->tbl; tmp; tmp = tmp->next) {
		fill_cube_by_bl(gcube, tmp, my_ctx2);
	}
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
