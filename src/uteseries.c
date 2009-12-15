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

#include <sushi/sl1tfile.h>

/* our master include */
#include "unserding-nifty.h"

typedef sl1t_fio_t ute_ctx_t;
typedef int32_t time32_t;

static ute_ctx_t
open_ute_file(const char *fn)
{
	int fd;
	ute_ctx_t res;

	if ((fd = open(fn, O_RDONLY, 0644)) < 0) {
		return NULL;
	} else if ((res = make_sl1t_reader(fd)) == NULL) {
		close(fd);
		return NULL;
	}
	return res;
}

static void
close_ute_file(ute_ctx_t ctx)
{
	int fd;
	if (ctx == NULL) {
		return;
	}
	fd = sl1t_fio_fd(ctx);
	free_sl1t_reader(ctx);
	close(fd);
	return;
}


typedef struct tblister_s *tblister_t;

struct tblister_s {
	time32_t bts;
	time32_t ets;
	uint32_t btk;
	uint32_t etk;
	uint64_t ttfbs[UTEHDR_MAX_SECS];
	uint32_t cnt[UTEHDR_MAX_SECS];
};

static void
tbl_set(tblister_t tbl, uint16_t idx, uint16_t ttf)
{
	tbl->ttfbs[idx] |= (1UL << ttf);
	tbl->cnt[idx]++;
	return;
}

static struct tblister_s gtbl[1];

static void
init_tblister_tbls(tblister_t tbl)
{
	const size_t sz = sizeof(*tbl) - offsetof(struct tblister_s, ttfbs);
	memset(tbl->ttfbs, 0, sz);
	return;
}

static void
init_tblister_hdr(tblister_t tbl)
{
	const size_t sz = offsetof(struct tblister_s, ttfbs);
	memset(tbl->ttfbs, 0, sz);
	return;
}

static void
init_tblister(tblister_t tbl)
{
	memset(tbl, 0, sizeof(*tbl));
	return;
}

static void
tblister_print(tblister_t tbl)
{
	fprintf(stderr, "blister %p  tk %i - %i (%i)  tk %u - %u (%u)\n",
		tbl,
		tbl->bts, tbl->ets, tbl->ets - tbl->bts,
		tbl->btk, tbl->etk, tbl->etk - tbl->btk);
	for (uint16_t i = 0; i < countof(tbl->ttfbs); i++) {
		if (tbl->ttfbs[i]) {
			fprintf(stderr, "  %hu: %lx %u\n",
				i, tbl->ttfbs[i], tbl->cnt[i]);
		}
	}
	return;
}

static __attribute__((unused)) void
__inspect_ts(const_sl1t_t t, size_t nticks)
{
	const_sl1t_t et = t + nticks;
	uint32_t per = gtbl->ets - gtbl->bts;

	while (t < et) {
		int32_t ets = gtbl->ets;
		int32_t ts;

		for (; t < et && (ts = sl1t_stmp_sec(t)) < ets; t++) {
			uint16_t idx = sl1t_tblidx(t);
			uint16_t ttf = sl1t_ttf(t);
			tbl_set(gtbl, idx, ttf);
			gtbl->etk++;
		}
		tblister_print(gtbl);
		init_tblister_tbls(gtbl);
		gtbl->bts = ts - ts % per;
		gtbl->ets = gtbl->bts + per;
		gtbl->btk = gtbl->etk;
	}
	return;
}

static __attribute__((unused)) void
__inspect_tk(const_sl1t_t t, size_t nticks)
{
	const_sl1t_t et = t + nticks;
	int32_t bts = sl1t_stmp_sec(t);

	init_tblister_tbls(gtbl);
	gtbl->bts = bts;
	gtbl->etk = gtbl->btk + nticks;
	for (; t < et; t++) {
		uint16_t idx = sl1t_tblidx(t);
		uint16_t ttf = sl1t_ttf(t);
		tbl_set(gtbl, idx, ttf);
	}
	gtbl->ets = sl1t_stmp_sec(t - 1);
	tblister_print(gtbl);
	gtbl->btk = gtbl->etk;
	return;
}

#define USE_FIXED_TSRNG		1
#define USE_FIXED_TKRNG		2
#define FIXATION		USE_FIXED_TSRNG

#define DEFAULT_PER		600
#define DEFAULT_TKSZ		262144

/* roughly look at what's in the file, keep track of secus and times */
static void
ute_inspect(ute_ctx_t ctx)
{
	const_sl1t_t t;

	if (ctx == NULL) {
		return;
	}

#if FIXATION == USE_FIXED_TSRNG
	init_tblister(gtbl);
	gtbl->bts = 0;
	gtbl->ets = DEFAULT_PER;
#elif FIXATION == USE_FIXED_TKRNG
	init_tblister_hdr(gtbl);
#endif
	for (size_t tidx = 0, nt;
	     (nt = sl1t_fio_read_ticks(ctx, &t, tidx,
#if FIXATION == USE_FIXED_TSRNG
				       -1UL
#elif FIXATION == USE_FIXED_TKRNG
				       tidx + DEFAULT_TKSZ
#endif
		     )) > 0;
	     tidx += nt) {
#if 0
		/* simple specs, avg. ticks per second */
		uint32_t bts = sl1t_stmp_sec(t);
		uint32_t ets = sl1t_stmp_sec(t + nt - 1);
		float tsptk = (float)(ets - bts) / (float)nt;
		fprintf(stderr, "%2.4f s/tk\n", tsptk);
#endif
#if FIXATION == USE_FIXED_TSRNG
		__inspect_ts(t, nt);
#elif FIXATION == USE_FIXED_TKRNG
		__inspect_tk(t, nt);
#endif
	}
	return;
}


#if defined TEST_MODE
int
main(int argc, const char *argv[])
{
	ute_ctx_t ctx = NULL;

	if (argc > 1) {
		ctx = open_ute_file(argv[1]);
	}

	ute_inspect(ctx);
	close_ute_file(ctx);
	return 0;
}
#endif	/* TEST_MODE */

/* uteseries.c ends here */
