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


typedef struct tsblister_s *tsblister_t;
typedef struct tkblister_s *tkblister_t;

struct tsblister_s {
	int32_t bts;
	int32_t per;
	uint64_t ttfbs[UTEHDR_MAX_SECS];
};

struct tkblister_s {
	uint32_t tk;
	uint32_t sz;
	uint64_t ttfbs[UTEHDR_MAX_SECS];
};

static void
tsbl_set(tsblister_t tsbl, uint16_t idx, uint16_t ttf)
{
	tsbl->ttfbs[idx] |= (1UL << ttf);
	return;
}

static struct tsblister_s gtsbl[1];
static struct tkblister_s gtkbl[1];

static void
init_tsblister(tsblister_t tsbl)
{
	memset(tsbl, 0, sizeof(*tsbl));
	return;
}

static void
tsblister_print(tsblister_t tsbl)
{
	fprintf(stderr, "blister %p  asof %i for %i\n",
		tsbl, tsbl->bts, tsbl->per);
	for (uint16_t i = 0; i < countof(tsbl->ttfbs); i++) {
		if (tsbl->ttfbs[i]) {
			fprintf(stderr, "  %hu: %lx\n", i, tsbl->ttfbs[i]);
		}
	}
	return;
}

static void
__inspect(const_sl1t_t ticks, size_t nticks)
{
	const_sl1t_t et = ticks + nticks;

	init_tsblister(gtsbl);
	gtsbl->bts = sl1t_stmp_sec(ticks);
	gtsbl->per = sl1t_stmp_sec(et - 1);
	for (; ticks < et; ticks++) {
		uint16_t idx = sl1t_tblidx(ticks);
		uint16_t ttf = sl1t_ttf(ticks);
		tsbl_set(gtsbl, idx, ttf);
	}
	tsblister_print(gtsbl);
	return;
}

/* roughly look at what's in the file, keep track of secus and times */
static void
ute_inspect(ute_ctx_t ctx)
{
	const_sl1t_t t;

	if (ctx == NULL) {
		return;
	}

	for (size_t tidx = 0, nt;
	     (nt = sl1t_fio_read_ticks(ctx, &t, tidx, -1UL)) > 0;
	     tidx += nt) {
		/* simple linear search */
		__inspect(t, nt);
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
