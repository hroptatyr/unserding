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

static bool
ttf_coincide_p(uint16_t tick_ttf, uint16_t blst_ttf)
{
#if defined CUBE_ENTRY_PER_TTF
	return (tick_ttf & 0x0f) == blst_ttf;
#else  /* !CUBE_ENTRY_PER_TTF */
	return (1 << (tick_ttf & 0x0f)) & blst_ttf;
#endif	/* CUBE_ENTRY_PER_TTF */
}

static const_sl1t_t
__find_bb(const_sl1t_t t, size_t nt, const_scom_thdr_t key)
{
	time32_t ts = scom_thdr_sec(key);
	uint16_t idx = scom_thdr_tblidx(key);
	uint16_t ttf = scom_thdr_ttf(key);
	/* to store the last seen ticks of each tick type 0 to 7 */
	const_sl1t_t lst[8] = {0};
	const_sl1t_t res;

	for (const_sl1t_t tmp = t; tmp < t + nt && sl1t_stmp_sec(tmp) <= ts; ) {
		uint16_t tkttf = sl1t_ttf(tmp);
		uint16_t tkidx = sl1t_tblidx(tmp);

		if (tkidx == idx && ttf_coincide_p(tkttf, ttf)) {
			/* keep track */
			lst[(tkttf & 0x0f)] = tmp;
		}
		tmp += scom_thdr_linked((const void*)(tmp)) ? 2 : 1;
	}
	/* find the minimum stamp out of the remaining ones */
	res = NULL;
	for (int i = 0; i < countof(lst); i++) {
		if ((volatile void*)(lst[i] - 1) < (volatile void*)(res - 1)) {
			res = lst[i];
		}
	}
	return res;
}

/* belongs in uteseries.c? */
static size_t
__cp_tk(const_sl1t_t *tgt, ute_ctx_t ctx, sl1t_t src)
{
	const_scom_thdr_t key = (const void*)src;
	time32_t ts = scom_thdr_sec(key);
	uint16_t idx = scom_thdr_tblidx(key);
	uint16_t ttf = scom_thdr_ttf(key);
	tblister_t tbl = __find_blister_by_ts(ctx->tbl, ts);
	size_t nt;
	const_sl1t_t t;

	UD_DEBUG("fetching %hu msk %x\n", ttf, tbl->ttfbs[idx]);
	if (UNLIKELY(tbl == NULL)) {
		return 0;
	} else if (!(tbl->ttfbs[idx] & ttf)) {
		return 0;
	}

	UD_DEBUG("fetching %u to %u for %i\n", tbl->btk, tbl->etk, ts);
	/* get the corresponding tick block */
	nt = sl1t_fio_read_ticks(ctx->fio, &t, tbl->btk, tbl->etk);
	/* this will give us a tick which is before the tick in question */
	*tgt = __find_bb(t, nt, key);
	return *tgt ? t + nt - *tgt : 0;
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
	UD_DEBUG("fine-grain over %zu ticks t[0]->ts %u\n",
		 nt, sl1t_stmp_sec(t[0]));
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
		} else if (tkidx == idx && ttf_coincide_p(tkttf, k->ttf)) {
			/* not thread-safe */
			memcpy(&tgt->sl1t[res * tgt->pad], &t[0][i],
			       sizeof(struct sl1t_s) * tgt->pad);
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

static void
fill_cube_by_bl_ttf(tscube_t c, tblister_t bl, ute_ctx_t ctx, uint16_t idx)
{
	uint64_t bs = bl->ttfbs[idx];
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
#if defined CUBE_ENTRY_PER_TTF
	bs >>= 1;
	/* go over all the tick types in the bitset BS */
	for (int j = 1; j < SL1T_TTF_VOL; j++, bs >>= 1) {
		if (bs & 1) {
			/* means tick type j is set */
			ce.key->ttf = j;
			tsc_add(c, &ce);
		}
	}
#else  /* !CUBE_ENTRY_PER_TTF */
/* try a ttf-less approach */
	ce.key->ttf = bs;
	tsc_add(c, &ce);
#endif	/* CUBE_ENTRY_PER_TTF */
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


#include <fts.h>

/* soft-code me */
static char ute_dir[] = "/home/freundt/.unserding/";
/* hardcoded too */
static char *const ute_dirs[] = {ute_dir, NULL};

static size_t nf;
static ute_ctx_t all[1024];

static void
fetch_urn_ute(job_t UNUSED(j))
{
	FTS *fts;
	FTSENT *ent;

	UD_DEBUG("inspecting directories ...");

	if (!(fts = fts_open(ute_dirs, FTS_LOGICAL, NULL))) {
		UD_DBGCONT("failed\n");
		return;
	}

	while ((ent = fts_read(fts)) != NULL) {
		ute_ctx_t my_ctx;
		if (ent->fts_info != FTS_F) {
			continue;
		}

		if ((my_ctx = open_ute_file(ent->fts_path))) {
			ute_inspect(my_ctx);
			all[nf++] = my_ctx;
			/* travel along the blister list and fill the cube */
			for (tblister_t t = my_ctx->tbl; t; t = t->next) {
				fill_cube_by_bl(gcube, t, my_ctx);
			}
		}
	}
	fts_close(fts);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_init(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: loading ...");
	/* announce our hook fun */
	add_hook(fetch_urn_hook, fetch_urn_ute);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: unloading ...");
	for (index_t i = 0; i < nf; i++) {
		close_ute_file(all[i]);
	}
	UD_DBGCONT("done\n");
	return;
}

/* dso-tseries-ute.c ends here */
