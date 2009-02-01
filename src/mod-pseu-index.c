/*** mod-pseu-index.c -- unserding module to obtain bollocks
 *
 * Copyright (C) 2008 Sebastian Freundt
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "catalogue.h"

#if defined HAVE_FFFF_FFFF_H
/* to get a default set of aliases */
# define USE_MONETARY64
# include <ffff/ffff.h>
#else  /* !FFFF */
/* posix */
# include <math.h>
#endif	/* FFFF */

typedef long int timestamptz_t;
extern void mod_pseu_index_LTX_init(void);
extern void mod_pseu_index_LTX_deinit(void);

#define PFACK_DEBUG		UD_DEBUG
#define PFACK_CRITICAL		UD_CRITICAL

/** struct for end of day data */
struct eod_ser_s {
	monetary32_t pri;
};

static time_series_t idx_PSEU;
static time_series_t idx_BUGG;
static ud_catobj_t c_pseu, c_bugg;

static inline timestamptz_t __attribute__((always_inline))
YYYY_MM_DD_to_ts(char *restrict buf)
{
	struct tm tm;
	/* now for real, parse the expiry date string */
	memset(&tm, 0, sizeof(tm));
	(void)strptime(buf, "%Y-%m-%d", &tm);
	return (timestamptz_t)mktime(&tm);
}

static void
obtain_pri_PSEU(void)
{
	idx_PSEU = malloc(sizeof(struct time_series_s) +
			  16384 * sizeof(monetary32_t));
	idx_PSEU->lo = 0;
	idx_PSEU->hi = idx_PSEU->ser_len = 16384;
	idx_PSEU->nser_prices = 1;
	idx_PSEU->aosp = 0;
	/* now the prices */
	for (int i = 0; i < 16384; ++i) {
		idx_PSEU->prices[i] =
			ffff_monetary32_get_d(2.01 + (double)i/16384.0);
	}
	return;
}

static void
obtain_pri_BUGG(void)
{
	idx_BUGG = malloc(sizeof(struct time_series_s) +
			  8192 * sizeof(monetary32_t));
	idx_BUGG->lo = 0;
	idx_BUGG->hi = idx_BUGG->ser_len = 8192;
	idx_BUGG->nser_prices = 1;
	idx_BUGG->aosp = 0;
	/* fill in prices */
	for (int i = 0; i < 8192; ++i) {
		idx_BUGG->prices[i] =
			ffff_monetary32_get_d(4.2 - (double)i/8192.0);
	}
	return;
}

void
mod_pseu_index_LTX_init(void)
{
	obtain_pri_PSEU();
	obtain_pri_BUGG();

	c_pseu = ud_make_catobj(
		UD_MAKE_CLASS("instrument"),
		UD_MAKE_CLASS("index"),
		UD_MAKE_NAME("PSEU"));

	c_bugg = ud_make_catobj(
		UD_MAKE_CLASS("instrument"),
		UD_MAKE_CLASS("index"),
		UD_MAKE_NAME("BUGG"));

	ud_cat_add_obj(c_pseu);
	ud_cat_add_obj(c_bugg);

	return;
}

void
mod_pseu_index_LTX_deinit(void)
{
	return;
}

/* mod-pseu-index.c ends here */
