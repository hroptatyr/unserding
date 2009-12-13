/*** dso-tseries-sl1t.c -- sl1t tick files
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
#include "tseries-private.h"

#if defined DEBUG_FLAG
# define UD_DEBUG_TSER(args...)			\
	fprintf(logout, "[unserding/tseries] " args)
#endif	/* DEBUG_FLAG */

typedef struct tssl1t_s *tssl1t_t;

struct tssl1t_s {
	int infd;
	void *rdr;
};


static size_t
fetch_ticks(tser_pkt_t pkt, tseries_t tser, dse16_t beg, dse16_t end)
{
	return 0;
}


static void
fill_urn_sec(uint16_t idx, su_secu_t sec, void *UNUSED(clo))
{
	tscoll_t tsc;
	struct tseries_s tser[1];
	const uint32_t tbs = 0x01 | 0x02;

	if (sl1t_stbl_sec_slot_free_p(sec)) {
		return;
	} else {
		uint32_t quodi = su_secu_quodi(sec);
		int32_t quoti = su_secu_quoti(sec);
		uint16_t pot = su_secu_pot(sec);
		fprintf(stdout, "%hu: %u/%i@%hu\n", idx, quodi, quoti, pot);
	}

	/* create us one of these nifty ts entries
	 * this should be really done by the lib itself */
	tsc = find_tscoll_by_secu_crea(tscache, sec);
	tser->urn = (void*)(long int)idx;
	/* hardcoded */
	tser->from = 915148800;
	tser->to = 0x7fffffff;
	tser->types = tbs;
	/* our cb */
	tser->fetch_cb = fetch_ticks;
	/* add to the collection of time stamps */
	tscoll_add(tsc, tser);
	return;
}

static void
fill_urns(tssl1t_t ctx)
{
	void *rdr = ctx->rdr;
	sl1t_fio_trav_stbl(rdr, fill_urn_sec, NULL /* punish */, rdr);
	return;
}


/* we expect a .ute file with secus and tt 4 (TTF_FIX) */
static const char fxsl1tf[] = "/home/freundt/.unserding/eur.ute";
static struct tssl1t_s my_ctx[1];

static void
load_sl1t_file(tssl1t_t ctx)
{
	UD_DEBUG("opening sl1t (%s) ...", fxsl1tf);
	if ((ctx->infd = open(fxsl1tf, O_RDONLY, 0644)) < 0) {
		/* file not found */
		ctx->rdr = NULL;
		UD_DBGCONT("failed\n");
		return;
	}
	ctx->rdr = make_sl1t_reader(ctx->infd);
	UD_DBGCONT("done\n");
	return;
}

void
fetch_urn_sl1t(void)
{
/* make me thread-safe and declare me */
	if (my_ctx->rdr == NULL) {
		return;
	}

	UD_DEBUG("inspecting sl1t ...");
	fill_urns(my_ctx);
	UD_DBGCONT("done\n");
	return;
}


void
dso_tseries_sl1t_LTX_init(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-sl1t: loading ...");
	load_sl1t_file(my_ctx);
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_sl1t_LTX_deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/tseries-sl1t: unloading ...");
	if (my_ctx->rdr != NULL) {
		free_sl1t_reader(my_ctx->rdr);
	}
	close(my_ctx->infd);
	UD_DBGCONT("done\n");
	return;
}

/* dso-sl1t.c */
