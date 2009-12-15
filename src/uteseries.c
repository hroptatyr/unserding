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

/* our master include */
#include "unserding-nifty.h"

#define USE_FIXED_TSRNG		1
#define USE_FIXED_TKRNG		2
#define FIXATION		USE_FIXED_TKRNG

#define DEFAULT_PER		600
#define DEFAULT_TKSZ		262144

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

	/* just to chain these guys, must stay behind ttfbs, as we have
	 * a copy operation that copies everything before that */
	tblister_t next;
};

static void
tbl_set(tblister_t tbl, uint16_t idx, uint16_t ttf)
{
	tbl->ttfbs[idx] |= (1UL << ttf);
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

static void
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

static void
tblister_clear_tbls(tblister_t tbl)
{
	const size_t sz = sizeof(*tbl) - offsetof(struct tblister_s, ttfbs);
	memset(tbl->ttfbs, 0, sz);
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
__inspect_ts(tblister_t t, const_sl1t_t tk, size_t ntk)
{
	const_sl1t_t etk = tk + ntk;
	uint32_t per = t->ets - t->bts;

	while (tk < etk) {
		int32_t ets = t->ets;
		int32_t ts;

		for (; tk < etk && (ts = sl1t_stmp_sec(tk)) < ets; tk++) {
			uint16_t idx = sl1t_tblidx(tk);
			uint16_t ttf = sl1t_ttf(tk);
			tbl_set(t, idx, ttf);
			t->etk++;
		}
		tblister_print(t);
		tblister_clear_tbls(t);
		t->bts = ts - ts % per;
		t->ets = t->bts + per;
		t->btk = t->etk;
	}
	return;
}

static __attribute__((unused)) void
__inspect_tk(tblister_t t, const_sl1t_t tk, size_t ntk)
{
	const_sl1t_t et = tk + ntk;
	int32_t bts = sl1t_stmp_sec(tk);

	tblister_clear_tbls(t);
	t->bts = bts;
	t->etk = t->btk + ntk;
	for (; tk < et; tk++) {
		uint16_t idx = sl1t_tblidx(tk);
		uint16_t ttf = sl1t_ttf(tk);
		tbl_set(t, idx, ttf);
	}
	t->ets = sl1t_stmp_sec(tk - 1);
	tblister_print(t);
	t->btk = t->etk;
	return;
}

#if FIXATION == USE_FIXED_TSRNG
# define UPPER_FETCH		(-1UL)
# define __inspect		__inspect_ts
#elif FIXATION == USE_FIXED_TKRNG
# define UPPER_FETCH		(tidx + DEFAULT_TKSZ)
# define __inspect		__inspect_tk
#endif

/* roughly look at what's in the file, keep track of secus and times */
static tblister_t
ute_inspect(ute_ctx_t ctx)
{
	const_sl1t_t t;
	tblister_t res = NULL;

	if (ctx == NULL) {
		return NULL;
	}

	for (size_t tidx = 0, nt;
	     (nt = sl1t_fio_read_ticks(ctx, &t, tidx, UPPER_FETCH)) > 0;
	     tidx += nt) {
		/* store old result */
		tblister_t tmp = make_tblister();

		tblister_copy_hdr(tmp, res);
		__inspect(tmp, t, nt);
		cons_tblister(tmp, res);
		res = tmp;
	}
	return res;
}

#if 0
/* simple specs, avg. ticks per second */
		uint32_t bts = sl1t_stmp_sec(t);
		uint32_t ets = sl1t_stmp_sec(t + nt - 1);
		float tsptk = (float)(ets - bts) / (float)nt;
		fprintf(stderr, "%2.4f s/tk\n", tsptk);
#endif


#if defined TEST_MODE
int
main(int argc, const char *argv[])
{
	ute_ctx_t ctx = NULL;
	tblister_t res;

	if (argc > 1) {
		ctx = open_ute_file(argv[1]);
	}

	res = ute_inspect(ctx);

	free_tblister(res);
	close_ute_file(ctx);
	return 0;
}
#endif	/* TEST_MODE */

/* uteseries.c ends here */
