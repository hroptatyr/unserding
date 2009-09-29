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
/* tseries stuff, to be replaced with ffff */
#include "tscache.h"
#include "tscoll.h"
#include "tseries.h"
#include "tseries-private.h"

tscache_t tscache = NULL;


#if 0
static tser_pkt_t
find_tser_pkt(tseries_t tser, date_t ts)
{
	for (tser_cons_t res = tser->conses; res; res = res->next) {
		if (ts < res->pktbe.beg) {
			continue;
		} else if (ts <= res->pktbe.end) {
			return &res->pktbe.pkt;
		}
	}
	return NULL;
}

static void
add_tser_pktbe(tseries_t tser, tser_pktbe_t pktbe)
{
	tser_cons_t res, c;

	if (tser->conses == NULL || pktbe->beg <= tser->conses->pktbe.beg) {
		/* we're the first, sort is trivial */
		c = xnew(*c);
		c->next = tser->conses;
		c->cache_expiry = -1UL;
		c->pktbe = *pktbe;
		/* prepend to tser->conses */
		tser->conses = c;
		UD_DEBUG("added in front\n");
		return;
	}
	/* otherwise find the right place first */
	for (res = tser->conses; res && res->next; res = res->next) {
		if (pktbe->beg <= res->next->pktbe.beg) {
			c = xnew(*c);
			c->next = res->next;
			c->cache_expiry = -1UL;
			c->pktbe = *pktbe;
			/* prepend to res */
			res->next = c;
			UD_DEBUG("added after %p\n", res);
			return;
		}
	}
	/* if we reach this, we have to append the fucker */
	c = xnew(*c);
	c->next = NULL;
	c->cache_expiry = -1UL;
	c->pktbe = *pktbe;
	/* prepend to res */
	res->next = c;
	UD_DEBUG("added at the end\n");
	return;
}
#endif

typedef struct spitfire_ctx_s *spitfire_ctx_t;
typedef enum spitfire_res_e spitfire_res_t;

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
	tscoll_t tsc;
	tseries_t tser;
	tser_pkt_t pkt;
	struct sl1oadt_s oadt;

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

	/* get us the tseries we're talking about */
	if ((tsc = find_tscoll_by_secu(tscache, &hdr.secu)) == NULL) {
		/* means we have no means of fetching */
		/* we could issue a packet saying so */
		UD_DEBUG("No way of fetching stuff\n");
		return;
	}
	if ((tser = tscoll_find_series(tsc, filt[0])) == NULL) {
		/* no way of obtaining ticks */
		UD_DEBUG("Found instr but no suitable URN\n");
		return;
	}

	/* prepare the reply packet ... */
	copy_pkt(&rplj, j);
	clear_pkt(&rplsctx, &rplj);

	/* ugly, but we have to loop-ify this anyway */
	dse16_t refts = time_to_dse(filt[0]);
	uint8_t idx = find_index_in_pkt(refts);
	/* obtain the time intervals we need */
	if ((pkt = tseries_find_pkt(tser, refts)) == NULL) {
		struct tser_pktbe_s p;

		/* let the luser know we deliver our shit later on */
		udpc_set_defer_fina_pkt(JOB_PACKET(&rplj));
		/* send what we've got */
		send_pkt(&rplsctx, &rplj);

		/* now care about fetching the bugger */
		p.beg = refts - idx;
		p.end = p.beg + 13;

		if (fetch_ticks_intv_mysql(&p, tser) == 0) {
			/* we should send something like quote invalid or so */
			return;
		}
		tseries_add(tser, &p);

		/* reset the packet */
		clear_pkt(&rplsctx, &rplj);
		fill_sl1oadt_1(&oadt, &hdr, PFTT_EOD, refts, p.pkt.t[idx]);
		udpc_seria_add_sl1oadt(&rplsctx, &oadt);
	} else {
		m32_t pri = pkt->t[idx];

		UD_DEBUG("yay, cached\n");
		fill_sl1oadt_1(&oadt, &hdr, PFTT_EOD, refts, pri);
		udpc_seria_add_sl1oadt(&rplsctx, &oadt);
	}
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
	/* create the catalogue */
	tscache = make_tscache();
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
