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


typedef struct my_ctx_s *my_ctx_t;
struct my_ctx_s {
	ute_ctx_t ctx;
	tblister_t tbl;
};

static size_t
FUCKING_FETCH(void **bla, tsc_key_t voo, void *doo)
{
	fprintf(stderr, "YAY\n");
	return 0;
}

static struct tsc_ops_s ute_ops[1] = {{
		.fetch_cb = FUCKING_FETCH,
	}};

static void
fill_cube(tscube_t c)
{
/* hardcoded as fuck */
	struct tsc_ce_s ce = {
		.key = {{
				.beg = 915148800,
				.end = 0x7fffffff,
				.ttf = SL1T_TTF_FIX,
				.msk = 1 | 2 | 4 | 8 | 16,
			}},
		.ops = ute_ops,
	};

	/* one such addition is EURUSD */
	ce.key->secu = su_secu(73380, 73381, 0);
	tsc_add(c, &ce);
	return;
}


static struct my_ctx_s my_ctx[1];
static const char my_hardcoded_file[] = "/home/freundt/.unserding/eur.ute";

void
fetch_urn_ute(void)
{
/* make me thread-safe and declare me */
	if (my_ctx->ctx == NULL) {
		return;
	}

	UD_DEBUG("inspecting sl1t ...");
	//fill_urns(my_ctx);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_init(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: loading ...");
	my_ctx->ctx = open_ute_file(my_hardcoded_file);
	fill_cube(gcube);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_ute_LTX_deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-ute: unloading ...");
	if (my_ctx->tbl != NULL) {
		free_tblister(my_ctx->tbl);
	}
	if (my_ctx->ctx != NULL) {
		close_ute_file(my_ctx->ctx);
	}
	UD_DBGCONT("done\n");
	return;
}

/* dso-tseries-ute.c ends here */
