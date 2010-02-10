/*** uteseries.c -- tseries handling
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding/sushi.
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
#include <pthread.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <sushi/sl1tfile.h>
#include <sushi/m30.h>

/* our master include */
#include "unserding-nifty.h"

#define USE_FIXED_TSRNG		1
#define USE_FIXED_TKRNG		2
#define FIXATION		USE_FIXED_TKRNG

#define DEFAULT_PER		600
#define DEFAULT_TKSZ		262144

#if !defined time32_t
typedef int32_t time32_t;
#define time32_t	time32_t
#endif	/* !time32_t */

typedef struct ute_ctx_s *ute_ctx_t;
typedef struct tblister_s *tblister_t;

struct ute_ctx_s {
	/** the io guts */
	sl1t_fio_t fio;
	/** points to the inspection blister */
	tblister_t tbl;
};

static ute_ctx_t
open_ute_file(const char *fn)
{
	int fd;
	ute_ctx_t res = xnew(*res);

	if ((fd = open(fn, O_RDONLY, 0644)) < 0) {
		return NULL;
	} else if ((res->fio = make_sl1t_reader(fd)) == NULL) {
		close(fd);
		return NULL;
	}
	res->tbl = NULL;
	return res;
}

/* forw decl */
static void free_tblister(tblister_t t);

static void
close_ute_file(ute_ctx_t ctx)
{
	int fd;
	if (ctx == NULL) {
		return;
	}
	if (ctx->fio != NULL) {
		fd = sl1t_fio_fd(ctx->fio);
		free_sl1t_reader(ctx->fio);
		close(fd);
	}
	if (ctx->tbl != NULL) {
		free_tblister(ctx->tbl);
	}
	return;
}


struct tblister_s {
	time32_t bts;
	time32_t ets;
	uint32_t btk;
	uint32_t etk;

	uint16_t ttfbs[UTEHDR_MAX_SECS];
	uint32_t cnt[UTEHDR_MAX_SECS];

	/* now all the stuff that we do not plan on writing back to the disk */
	/* just to chain these guys, must stay behind ttfbs, as we have
	 * a copy operation that copies everything before that */
	tblister_t next;
	void *data;
};

static void
tbl_set(tblister_t tbl, uint16_t idx, uint16_t ttf)
{
	/* mask linked-mode candles */
	uint16_t ttfcl = (uint16_t)(ttf & 0x0f);
	/* obtain new bitset */
	tbl->ttfbs[idx] = (uint16_t)(tbl->ttfbs[idx] | (uint16_t)(1 << ttfcl));
	tbl->cnt[idx]++;
	return;
}

#define PROT_MEMMAP	(PROT_READ | PROT_WRITE)
#define MAP_MEMMAP	(MAP_ANONYMOUS | MAP_PRIVATE)

static const size_t tbl_sz = (sizeof(tblister_t) & ~4095) + 4096;

static tblister_t
make_tblister(void)
{
	return mmap(NULL, tbl_sz, PROT_MEMMAP, MAP_MEMMAP, 0, 0);
}

static void
free_tblister(tblister_t t)
{
	for (volatile tblister_t n; t; t = n) {
		n = t->next;
		munmap(t, tbl_sz);
	}
	return;
}

static __attribute__((unused)) void
cons_tblister(tblister_t tgt, tblister_t src)
{
/* chain src and tgt together */
	tblister_t res;
	for (res = tgt; res->next; res = res->next);
	res->next = src;
	return;
}

static void
tblister_copy_hdr(tblister_t tgt, tblister_t src)
{
	const size_t sz = offsetof(struct tblister_s, ttfbs);
	if (src == NULL) {
		/* fuck off early */
		return;
	}
	memcpy(tgt, src, sz);
	return;
}

static __attribute__((unused)) void
tblister_clear_tbls(tblister_t tbl)
{
	const size_t sz = sizeof(*tbl) - offsetof(struct tblister_s, ttfbs);
	memset(tbl->ttfbs, 0, sz);
	return;
}

static void
__prts(char *restrict tgt, size_t tsz, time_t ts)
{
	struct tm tm[1];
	memset(tm, 0, sizeof(*tm));
	gmtime_r(&ts, tm);
	strftime(tgt, tsz, "%F %T", tm);
	return;
}

static void
__print(tblister_t tbl)
{
	char btss[32], etss[32];

	__prts(btss, sizeof(btss), tbl->bts);
	__prts(etss, sizeof(etss), tbl->ets);

	fprintf(stderr, "blister %p  tk %i - %i (%i)  tk %u - %u (%u) %s %s\n",
		tbl,
		tbl->bts, tbl->ets, tbl->ets - tbl->bts,
		tbl->btk, tbl->etk, tbl->etk - tbl->btk,
		btss, etss);
	for (uint16_t i = 0; i < countof(tbl->ttfbs); i++) {
		if (tbl->ttfbs[i]) {
			fprintf(stderr, "  %hu: %hx %u\n",
				i, tbl->ttfbs[i], tbl->cnt[i]);
		}
	}
	return;
}

static inline void
tblister_print(tblister_t tbl)
{
	for (; tbl; tbl = tbl->next) {
		__print(tbl);
	}
	return;
}

static tblister_t
tblister_fork_new(tblister_t t)
{
	if (UNLIKELY(t == NULL)) {
		return make_tblister();
	} else if (UNLIKELY(t->btk == t->etk)) {
		return t;
	} else {
		tblister_t res = make_tblister();
		tblister_copy_hdr(res, t);
		/* cons them */
		res->next = t;
		return res;
	}
}

static __attribute__((unused)) tblister_t
__inspect_ts(tblister_t t, const_sl1t_t tk, size_t ntk)
{
	const_sl1t_t etk = tk + ntk;
	uint32_t per;
	int32_t ts;

	if (LIKELY(t != NULL)) {
		per = t->ets - t->bts;
	} else {
		ts = sl1t_stmp_sec(tk);
		per = DEFAULT_PER;

		/* god i hate myself, this is what we get when co-routinising
		 * the shit */
	ugly:
		t = tblister_fork_new(t);
		t->bts = ts - ts % per;
		t->ets = t->bts + per;
		t->btk = t->etk;
	moreso:
		;
	}

	if (tk >= etk) {
		return t;
	} else if ((ts = sl1t_stmp_sec(tk)) >= t->ets) {
		goto ugly;
	} else {
		uint16_t idx = sl1t_tblidx(tk);
		uint16_t ttf = sl1t_ttf(tk);
		tbl_set(t, idx, ttf);
		t->etk++;
		tk++;
		goto moreso;
	}
	/* not reached */
}

static __attribute__((unused)) tblister_t
__inspect_tk(tblister_t t, const_sl1t_t tk, size_t ntk)
{
	const_sl1t_t et = tk + ntk;
	int32_t bts = sl1t_stmp_sec(tk);
	uint32_t old_etk = t ? t->etk : 0;

	t = tblister_fork_new(t);
	t->bts = bts;
	t->btk = old_etk;
	t->etk = t->btk + ntk;
	for (; tk < et; tk++) {
		uint16_t idx = sl1t_tblidx(tk);
		uint16_t ttf = sl1t_ttf(tk);
		tbl_set(t, idx, ttf);
		if (scom_thdr_linked((const void*)tk)) {
			tk++;
		}
	}
	if (scom_thdr_linked((const void*)(tk - 2))) {
		t->ets = sl1t_stmp_sec(tk - 2);
	} else {
		t->ets = sl1t_stmp_sec(tk - 1);
	}
	return t;
}

#if FIXATION == USE_FIXED_TSRNG
# define UPPER_FETCH		(-1UL)
# define __inspect		__inspect_ts
#elif FIXATION == USE_FIXED_TKRNG
# define UPPER_FETCH		(tidx + DEFAULT_TKSZ)
# define __inspect		__inspect_tk
#endif

/* roughly look at what's in the file, keep track of secus and times */
static void
ute_inspect(ute_ctx_t ctx)
{
	const_sl1t_t t;

	if (ctx == NULL || ctx->fio == NULL) {
		return;
	}

	/* look if there's a thing in there already */
	if (ctx->tbl != NULL) {
		free_tblister(ctx->tbl);
		ctx->tbl = NULL;
	}
	for (size_t tidx = 0, nt;
	     (nt = sl1t_fio_read_ticks(ctx->fio, &t, tidx, UPPER_FETCH)) > 0;
	     tidx += nt) {
		ctx->tbl = __inspect(ctx->tbl, t, nt);
	}
	return;
}

#if 0
/* simple specs, avg. ticks per second */
		uint32_t bts = sl1t_stmp_sec(t);
		uint32_t ets = sl1t_stmp_sec(t + nt - 1);
		float tsptk = (float)(ets - bts) / (float)nt;
		fprintf(stderr, "%2.4f s/tk\n", tsptk);
#endif


/* search functions using an existing blister */
static tblister_t
__find_blister_by_ts(tblister_t tbl, time_t ts)
{
	for (; tbl; tbl = tbl->next) {
		if (ts >= tbl->bts && ts < tbl->ets) {
			return tbl;
		}
	}
	return NULL;
}

/* assumes the time is correct in this blister, so no next slots will be
 * visited */
/**
 * Given the details in TGTSRC (time, tblidx and ttf will be eval'd) find
 * the nearest match in the blister TBL. */
static __attribute__((unused)) const_sl1t_t
__find_tk(ute_ctx_t ctx, tblister_t tbl, sl1t_t tgtsrc)
{
	const_scom_thdr_t src = (const void*)tgtsrc;
	time32_t ts = scom_thdr_sec(src);
	uint16_t idx = scom_thdr_tblidx(src);
	uint16_t ttf = scom_thdr_ttf(src);
	size_t nt;
	const_sl1t_t t;

	if (UNLIKELY(tbl == NULL)) {
		return NULL;
	} else if (!(tbl->ttfbs[idx] & (1 << ttf))) {
		return NULL;
	}

	/* get the corresponding tick block */
	nt = sl1t_fio_read_ticks(ctx->fio, &t, tbl->btk, tbl->etk);
	/* assume ascending order */
	for (const_sl1t_t tk = &t[nt - 1]; tk >= t; tk--) {
		uint16_t tkidx = sl1t_tblidx(tk);
		uint16_t tkttf = sl1t_ttf(tk);

		if (tkidx == idx && tkttf == ttf &&
		    (time32_t)sl1t_stmp_sec(tk) <= ts) {
			*tgtsrc = *tk;
			return tgtsrc;
		}
	}
	return NULL;
}


#if defined TEST_MODE
static time_t
__parse_ts(const char *str)
{
	struct tm tm[1];

	memset(tm, 0, sizeof(*tm));
	strptime(str, "%F %T", tm);
	return timegm(tm);
}

int
main(int argc, const char *argv[])
{
	ute_ctx_t ctx = NULL;

	if (argc > 1) {
		ctx = open_ute_file(argv[1]);
	}

	ute_inspect(ctx);

	if (argc > 2) {
		time_t ts = __parse_ts(argv[2]);
		tblister_t tmp = __find_blister_by_ts(ctx->tbl, ts);
		uint16_t idx = 1;

		if (argc > 3) {
			idx = (uint16_t)strtoul(argv[3], NULL, 10);
		}

		if (tmp) {
			struct sl1t_s t[1];

			sl1t_set_stmp_sec(t, ts);
			sl1t_set_tblidx(t, idx);
			sl1t_set_ttf(t, SL1T_TTF_TRA);
			/* given the template above, try n find the bugger */
			if (__find_tk(ctx, tmp, t)) {
				m30_t v0 = ffff_m30_get_ui32(t->v[0]);
				m30_t v1 = ffff_m30_get_ui32(t->v[1]);
				char stmp[32];
				double v0d = ffff_m30_d(v0);
				double v1d = ffff_m30_d(v1);

				__prts(stmp, sizeof(stmp), sl1t_stmp_sec(t));
				fprintf(stderr, "%s %2.4f %2.4f\n",
					stmp, v0d, v1d);
			}
		}
	} else {
		/* otherwise just print the bugger */
		tblister_print(ctx->tbl);
	}

	close_ute_file(ctx);
	return 0;
}
#endif	/* TEST_MODE */

/* uteseries.c ends here */
