/*** dso-xdr-instr-ticks.c -- ticks of instruments
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

#include <pfack/instruments.h>
#include "catalogue.h"
#include "xdr-instr-seria.h"
#include "xdr-instr-private.h"

/* later to be decoupled from the actual source */
#if defined HAVE_MYSQL
# include <mysql/mysql.h>
#endif	/* HAVE_MYSQL */

static void db_connect(void *UNUSED());
static void db_disconnect(void);

/* tick services */
#define index_t	size_t
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

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	udpc_seria_des_tick_by_instr_hdr(&hdr, &sctx);

	/* triples of instrument identifiers */
	while ((filt[nfilt] = udpc_seria_des_ui32(&sctx)) &&
	       ++nfilt < countof(filt));

	UD_DEBUG("0x4222: %u/%u@%u filtered for %u time stamps\n",
		 hdr.secu.instr, hdr.secu.unit, hdr.secu.pot, nfilt);
	/* prepare the reply packet ... */
	copy_pkt(&rplj, j);
	clear_pkt(&rplsctx, &rplj);
	/* let the luser know we deliver our shit later on */
	udpc_set_defer_fina_pkt(JOB_PACKET(&rplj));
	/* send what we've got */
	send_pkt(&rplsctx, &rplj);
	return;
}


/* db connectors */
static const char *host = NULL;
static const char *user = NULL;
static const char *pass = NULL;
static const char *sche = NULL;
/* mysql conn, kept open */
static void *conn;

static void
db_connect(void *UNUSED(clo))
{
/* we assume that SPEC is a config_setting_t pointing to database mumbojumbo */
	MYSQL *res;
	const char dflt_sche[] = "freundt";

	UD_DEBUG("deferred tick loader: loading ...");
	/* have we got a catalogue? */
	if (instrs == NULL) {
		UD_DBGCONT("failed (no catalogue found)\n");
		return;
	} else if (conn != NULL) {
		UD_DBGCONT("failed (already connected)\n");
		return;
	}
	UD_DBGCONT("done\n");

	UD_DEBUG("connecting to database ...");

	if (host == NULL || user == NULL || pass == NULL) {
		conn = NULL;
		UD_DBGCONT("failed\n");
		return;
	} else if (sche == NULL) {
		/* just assume the schema exists as we know it */
		sche = dflt_sche;
	}

	res = mysql_init(NULL);
	if (!mysql_real_connect(res, host, user, pass, sche, 0, NULL, 0)) {
		mysql_close(res);
		conn = NULL;
		UD_DBGCONT("failed\n");
		return;
	}
	conn = res;
	UD_DBGCONT("done\n");
	return;
}

static void __attribute__((unused))
db_disconnect(void)
{
	(void)mysql_close(conn);
	conn = NULL;
	return;
}

static void
frob_db_specs(void *clo)
{
	/* try and read the stuff from the config file */
	config_setting_lookup_string(clo, "host", &host);
	config_setting_lookup_string(clo, "user", &user);
	config_setting_lookup_string(clo, "pass", &pass);
	config_setting_lookup_string(clo, "schema", &sche);
}


#define DEFERRAL_TIME	10.0

void
dso_xdr_instr_ticks_LTX_init(void *clo)
{
	struct {ud_ctx_t ctx; void *spec;} *tmp = clo;

	UD_DEBUG("mod/xdr-instr-ticks: loading ...");
	/* tick service */
	ud_set_service(UD_SVC_TICK_BY_TS, instr_tick_by_ts_svc, NULL);
	ud_set_service(UD_SVC_TICK_BY_INSTR, instr_tick_by_instr_svc, NULL);
	UD_DBGCONT("done\n");

	/* store the config file settings for later use */
	frob_db_specs(tmp->spec);
	schedule_timer_once(tmp->ctx, db_connect, NULL, DEFERRAL_TIME);
	return;
}

/* dso-xdr-instr-ticks.c */
