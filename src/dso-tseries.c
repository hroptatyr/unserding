/*** dso-tseries.c -- ticks of instruments
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"
#include "unserding-private.h"

#include "xdr-instr-seria.h"
#include "xdr-instr-private.h"

/* later to be decoupled from the actual source */
#if defined HAVE_MYSQL
# if defined HAVE_MYSQL_MYSQL_H
#  include <mysql/mysql.h>
# elif defined HAVE_MYSQL_H
#  include <mysql.h>
# endif
#endif	/* HAVE_MYSQL */

/* tick services */
#define index_t	size_t
typedef struct spitfire_ctx_s *spitfire_ctx_t;
typedef enum spitfire_res_e spitfire_res_t;

typedef struct tser_pkt_s *tser_pkt_t;
typedef struct tser_qry_intv_s *tser_qry_intv_t;

/* packet of 10 ticks */
struct tser_pkt_s {
	monetary32_t t[10];
};

struct tser_qry_intv_s {
	time_t beg;
	time_t end;
};

struct spitfire_ctx_s {
	secu_t secu;
	size_t slen;
	index_t idx;
	time_t ts;
	uint32_t types;
};

enum spitfire_res_e {
	NO_TICKS,
	OUT_OF_SPACE,
	OUT_OF_TICKS,
};

static spitfire_res_t
spitfire(spitfire_ctx_t sfctx, udpc_seria_t sctx)
{
	struct sl1tick_s t;
	size_t trick = 1;

	/* start out with one tick per instr */
	while (sfctx->idx < sfctx->slen &&
	       sctx->msgoff < sctx->len - /*yuck*/7*8) {
		secu_t s = &sfctx->secu[sfctx->idx];

		if (trick && (1 << (PFTT_EOD))/* bollocks */ & sfctx->types) {
			gaid_t i = s->instr;
			gaid_t u = s->unit ? s->unit : 73380;
			gaid_t p = s->pot ? s->pot : 4;

			fill_sl1tick_shdr(&t, i, u, p);
			fill_sl1tick_tick(&t, sfctx->ts, 0, PFTT_EOD, 10000);
			udpc_seria_add_sl1tick(sctx, &t);
		}
		if (!trick && (1 << (PFTT_STL))/* bollocks */ & sfctx->types) {
			gaid_t i = s->instr;
			gaid_t u = s->unit ? s->unit : 73380;
			gaid_t p = s->pot ? s->pot : 4;

			fill_sl1tick_shdr(&t, i, u, p);
			fill_sl1tick_tick(&t, sfctx->ts, 0, PFTT_STL, 15000);
			udpc_seria_add_sl1tick(sctx, &t);
		}
		sfctx->idx += (trick ^= 1);
	}
	/* return false if this packet is meant to be the last one */
	return sfctx->idx < sfctx->slen;
}

static void
init_spitfire(spitfire_ctx_t ctx, secu_t secu, size_t slen, tick_by_ts_hdr_t t)
{
	ctx->secu = secu;
	ctx->slen = slen;
	ctx->idx = 0;
	ctx->ts = t->ts;
	ctx->types = t->types;
	return;
}

static void
instr_tick_by_ts_svc(job_t j)
{
	struct udpc_seria_s sctx;
	struct udpc_seria_s rplsctx;
	struct job_s rplj;
	/* in args */
	struct tick_by_ts_hdr_s hdr;
	/* allow to filter for 64 instruments at once */
	struct secu_s filt[64];
	unsigned int nfilt = 0;
	struct spitfire_ctx_s sfctx;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	udpc_seria_des_tick_by_ts_hdr(&hdr, &sctx);

	/* triples of instrument identifiers */
	while (udpc_seria_des_secu(&filt[nfilt], &sctx) &&
	       ++nfilt < countof(filt));

	UD_DEBUG("0x4220: ts:%d filtered for %u instrs\n", (int)hdr.ts, nfilt);
	/* initialise the spit state context */
	init_spitfire(&sfctx, filt, nfilt, &hdr);
	copy_pkt(&rplj, j);
	for (bool moar = true; moar;) {
		/* prepare the reply packet ... */
		clear_pkt(&rplsctx, &rplj);
		/* serialise some ticks */
		if ((moar = spitfire(&sfctx, &rplsctx))) {
			udpc_set_immed_frag_pkt(JOB_PACKET(&rplj));
		}
		/* send what we've got */
		send_pkt(&rplsctx, &rplj);
	}
	return;
}

static void
instr_tick_by_instr_svc(job_t j)
{
	struct udpc_seria_s sctx;
	struct udpc_seria_s rplsctx;
	struct job_s rplj;
	/* in args */
	struct tick_by_instr_hdr_s hdr;
	/* allow to filter for 64 time stamps at once */
	time_t filt[64];
	unsigned int nfilt = 0;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	udpc_seria_des_tick_by_instr_hdr(&hdr, &sctx);

	/* triples of instrument identifiers */
	while ((filt[nfilt] = udpc_seria_des_ui32(&sctx)) &&
	       ++nfilt < countof(filt));

	UD_DEBUG("0x4222: %u/%u@%u filtered for %u time stamps\n",
		 hdr.secu.instr, hdr.secu.unit, hdr.secu.pot, nfilt);

	if (nfilt == 0) {
		return;
	}
	/* obtain the time intervals we need */
	{
		struct tser_qry_intv_s intv;

		intv.beg = __last_monday(filt[0]);
		intv.end = intv.beg + 13 * 86400;
		fetch_ticks_intv_mysql(&hdr, intv.beg, intv.end);
	}

	/* prepare the reply packet ... */
	copy_pkt(&rplj, j);
	clear_pkt(&rplsctx, &rplj);
	/* let the luser know we deliver our shit later on */
	udpc_set_defer_fina_pkt(JOB_PACKET(&rplj));
	/* send what we've got */
	send_pkt(&rplsctx, &rplj);
	return;
}


static void*
cfgspec_get_source(void *ctx, void *spec)
{
#define CFG_SOURCE	"source"
	return udcfg_tbl_lookup(ctx, spec, CFG_SOURCE);
}

typedef enum {
	CST_UNK,
	CST_MYSQL,
} cfgsrc_type_t;

static cfgsrc_type_t
cfgsrc_type(void *ctx, void *spec)
{
#define CFG_TYPE	"type"
	const char *type = NULL;
	udcfg_tbl_lookup_s(&type, ctx, spec, CFG_TYPE);

	UD_DEBUG("type %s %p\n", type, spec);
	if (type == NULL) {
		return CST_UNK;
	} else if (memcmp(type, "mysql", 5) == 0) {
		return CST_MYSQL;
	}
	return CST_UNK;
}

static void
load_ticks_fetcher(void *clo, void *spec)
{
	void *src = cfgspec_get_source(clo, spec);

	/* pass along the src settings */
	udctx_set_setting(clo, src);

	/* find out about its type */
	switch (cfgsrc_type(clo, src)) {
	case CST_MYSQL:
#if defined HAVE_MYSQL
		/* fetch some instruments by sql */
		dso_tseries_mysql_LTX_init(clo);
#endif	/* HAVE_MYSQL */
		break;

	case CST_UNK:
	default:
		/* do fuckall */
		break;
	}

	/* clean up */
	udctx_set_setting(clo, NULL);
	udcfg_tbl_free(clo, src);
	return;
}


void
dso_tseries_LTX_init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	UD_DEBUG("mod/tseries: loading ...");
	/* tick service */
	ud_set_service(UD_SVC_TICK_BY_TS, instr_tick_by_ts_svc, NULL);
	ud_set_service(UD_SVC_TICK_BY_INSTR, instr_tick_by_instr_svc, NULL);
	UD_DBGCONT("done\n");

	if ((settings = udctx_get_setting(ctx)) != NULL) {
		load_ticks_fetcher(clo, settings);
		/* be so kind as to unref the settings */
		udcfg_tbl_free(ctx, settings);
	}
	/* clean up */
	udctx_set_setting(ctx, NULL);
	return;
}

/* dso-tseries.c */
