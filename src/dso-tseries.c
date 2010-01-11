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

#include <pfack/uterus.h>
#include <pfack/instruments.h>
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

/* tseries stuff, to be replaced with ffff */
#include "tscube.h"
#include "tscache.h"
#include "tscoll.h"
#include "tseries.h"
#include "tseries-private.h"

tscube_t gcube = NULL;
struct hook_s __fetch_urn_hook[1], *fetch_urn_hook = __fetch_urn_hook;

#if defined DEBUG_FLAG
# define UD_DEBUG_TSER(args...)			\
	fprintf(logout, "[unserding/tseries] " args)
#endif	/* DEBUG_FLAG */


#if 0
/* deactivated for a mo */
typedef struct spitfire_ctx_s *spitfire_ctx_t;
typedef enum spitfire_res_e spitfire_res_t;

struct spitfire_ctx_s {
	su_secu_t *secu;
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
		su_secu_t s = sfctx->secu[sfctx->idx];

		if (trick && (1 << (PFTT_EOD))/* bollocks */ & sfctx->types) {
			gaid_t i = su_secu_quodi(s);
			gaid_t u = su_secu_quoti(s);
			gaid_t p = su_secu_pot(s);

			u = u ? u : 73380;
			p = p ? p : 4;
			fill_sl1tick_shdr(&t, i, u, p);
			fill_sl1tick_tick(&t, sfctx->ts, 0, PFTT_EOD, 10000);
			udpc_seria_add_sl1tick(sctx, &t);
		}
		if (!trick && (1 << (PFTT_STL))/* bollocks */ & sfctx->types) {
			gaid_t i = su_secu_quodi(s);
			gaid_t u = su_secu_quoti(s);
			gaid_t p = su_secu_pot(s);

			u = u ? u : 73380;
			p = p ? p : 4;
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
init_spitfire(
	spitfire_ctx_t ctx, su_secu_t *secu, size_t slen, tick_by_ts_hdr_t t)
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
	struct udpc_seria_s sctx[1];
	struct udpc_seria_s rplsctx;
	struct job_s rplj;
	/* in args */
	uint32_t ts;
	tbs_t tbs;
	/* allow to filter for 64 instruments at once */
	su_secu_t filt[64];
	unsigned int nfilt = 0;
	struct spitfire_ctx_s sfctx;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	ts = udpc_seria_des_ui32(sctx);
	tbs = udpc_seria_des_tbs(sctx);

	/* triples of instrument identifiers */
	while ((filt[nfilt].mux = udpc_seria_des_secu(&sctx).mux) &&
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
#endif


/* number of stamps we can process at once */
#define NFILT	64

typedef struct oadt_ctx_s *oadt_ctx_t;
struct oadt_ctx_s {
	udpc_seria_t sctx;
	su_secu_t secu;
	tscoll_t coll;
	time32_t filt[NFILT];
	size_t nfilt;
	tbs_t tbs;
};

/* we sort the list of requested time stamps to collapse contiguous ones */
static index_t
selsort_minidx(time32_t arr[], size_t narr, index_t offs)
{
	index_t minidx = offs;

	for (index_t i = offs+1; i < narr; i++) {
		if (arr[i] < arr[minidx]) {
			minidx = i;
		}
	}
	return minidx;
}

static inline void
selsort_swap(time32_t arr[], index_t i1, index_t i2)
{
	time32_t tmp;
	tmp = arr[i1];
	arr[i1] = arr[i2];
	arr[i2] = tmp;
	return;
}

static void
selsort_in_situ(time32_t arr[], size_t narr)
{
	for (index_t i = 0; i < narr-1; i++) {
		index_t minidx;
		minidx = selsort_minidx(arr, narr, i);
		selsort_swap(arr, i, minidx);
	}
	return;
}

static size_t
snarf_times(udpc_seria_t sctx, time32_t ts[], size_t nts)
{
	size_t nfilt = 0;
	while ((ts[nfilt] = udpc_seria_des_ui32(sctx)) && ++nfilt < nts);
	if (LIKELY(nfilt > 0)) {
		selsort_in_situ(ts, nfilt);
	}
	return nfilt;
}

static __attribute__((unused)) const char*
tsbugger(time_t ts)
{
	static char buf[32];
	struct tm tm;
	memset(&tm, 0, sizeof(tm));
	(void)gmtime_r(&ts, &tm);
	(void)strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &tm);
	return buf;
}

static __attribute__((unused)) const char*
secbugger(su_secu_t s)
{
	static char buf[64];
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);

	snprintf(buf, sizeof(buf), "%u/%i@%hu", qd, qt, p);
	return buf;
}

static inline bool
__tbs_has_p(tbs_t tbs, uint16_t ttf)
{
	return tbs & (1UL << ttf);
}


static __attribute__((unused)) void
__bang(oadt_ctx_t octx, tser_pkt_t pkt, uint8_t idx)
{
	udpc_seria_add_secu(octx->sctx, octx->secu);
	udpc_seria_add_sl1t(octx->sctx, &pkt->t[idx]);
	return;
}

static void
__bang_nexist(oadt_ctx_t octx, time_t refts, uint16_t ttf)
{
	struct sl1t_s tgt[1];
	scom_thdr_t th = (void*)tgt;

	scom_thdr_set_sec(th, refts);
	scom_thdr_mark_nexist(th);
	scom_thdr_set_ttf(th, ttf);
	scom_thdr_set_tblidx(th, 0);

	udpc_seria_add_secu(octx->sctx, octx->secu);
	udpc_seria_add_sl1t(octx->sctx, tgt);
	return;
}

static void
__bang_onhold(oadt_ctx_t octx, time_t refts, uint16_t ttf)
{
	struct sl1t_s tgt[1];
	scom_thdr_t th = (void*)tgt;

	scom_thdr_set_sec(th, refts);
	scom_thdr_mark_onhold(th);
	scom_thdr_set_ttf(th, ttf);
	scom_thdr_set_tblidx(th, 0);

	udpc_seria_add_secu(octx->sctx, octx->secu);
	udpc_seria_add_sl1t(octx->sctx, tgt);
	return;
}

static __attribute__((unused)) void
__bang_all_nexist(oadt_ctx_t octx, time_t refts)
{
	tbs_t tbs = octx->tbs;

#define BANG_IF_TBS_HAS(_x)				\
	if (__tbs_has_p(tbs, (_x))) {			\
		__bang_nexist(octx, refts, (_x));	\
	} else

	BANG_IF_TBS_HAS(SL1T_TTF_BID);
	BANG_IF_TBS_HAS(SL1T_TTF_ASK);
	BANG_IF_TBS_HAS(SL1T_TTF_TRA);
	BANG_IF_TBS_HAS(SL1T_TTF_FIX);
	BANG_IF_TBS_HAS(SL1T_TTF_STL);

#undef BANG_IF_TBS_HAS
	return;
}

static __attribute__((unused)) void
__bang_all_onhold(oadt_ctx_t octx, time_t refts)
{
	tbs_t tbs = octx->tbs;

#define BANG_IF_TBS_HAS(_x)				\
	if (__tbs_has_p(tbs, (_x))) {			\
		__bang_onhold(octx, refts, (_x));	\
	} else

	BANG_IF_TBS_HAS(SL1T_TTF_BID);
	BANG_IF_TBS_HAS(SL1T_TTF_ASK);
	BANG_IF_TBS_HAS(SL1T_TTF_TRA);
	BANG_IF_TBS_HAS(SL1T_TTF_FIX);
	BANG_IF_TBS_HAS(SL1T_TTF_STL);

#undef BANG_IF_TBS_HAS
	return;
}

#if 0
static void
proc_one(oadt_ctx_t octx, time_t ts)
{
	tseries_t tser;
	tser_pkt_t pkt;
	uint8_t idx;

	/* this is the 10-ticks per 2 weeks fragment, so dont bother
	 * looking up saturdays and sundays */
#define TICKS_PER_FORTNIGHT	10
	if ((idx = index_in_pkt(ts)) >= TICKS_PER_FORTNIGHT) {
		/* leave a note in the packet? */
		UD_DEBUG_TSER("week end tick (%s %s)\n",
			      secbugger(octx->secu), tsbugger(ts));
		__bang_all_nexist(octx, ts);

	} else if ((tser = tscoll_find_series(octx->coll, ts)) == NULL) {
		/* no way of obtaining ticks */
		UD_DEBUG_TSER("No suitable URN found (%s %s)\n",
			      secbugger(octx->secu), tsbugger(ts));
		__bang_all_nexist(octx, ts);

	} else if ((pkt = tseries_find_pkt(tser, ts)) == NULL) {
		UD_DEBUG_TSER("URN not cached, deferring (%s %s)\n",
			      secbugger(octx->secu), tsbugger(ts));
		defer_frob(tser, time_to_dse(ts) - idx, false);
		__bang_all_onhold(octx, ts);

	} else {
		/* bother the cache */
		UD_DEBUG("yay, cached\n");
		__bang(octx, pkt, idx);
	}
	return;
}
#else
static void
proc_one(oadt_ctx_t octx, time32_t ts)
{
	struct tsc_key_s k = {
		.secu = octx->secu,
		.beg = ts, .end = ts,
		/* just noughtify the rest */
		.ttf = 0,
		/* make sure we let the system know what we want */
		.msk = 1 | 2 | 4,
	};
	struct tser_pkt_s pkt[1];
	size_t ntk;

	ntk = tsc_find1(pkt->t, countof(pkt->t), gcube, &k);
	UD_DEBUG("found %zu\n", ntk);
	for (index_t i = 0; i < ntk; i++) {
		udpc_seria_add_sl1t(octx->sctx, &pkt->t[i]);
	}
	return;
}
#endif

static inline bool
one_moar_p(oadt_ctx_t octx)
{
	size_t cur = udpc_seria_msglen(octx->sctx);
	size_t add = (sizeof(struct sparse_Dute_s) + 2) * 5;
	return cur + add < UDPC_PLLEN;
}

static index_t
proc_some(oadt_ctx_t octx, index_t i)
{
	udpc_seria_add_secu(octx->sctx, octx->secu);
	for (; i < octx->nfilt && one_moar_p(octx); i++) {
		proc_one(octx, octx->filt[i]);
	}
	return i;
}

static void
instr_tick_by_instr_svc(job_t j)
{
	struct udpc_seria_s sctx[1];
	struct udpc_seria_s rplsctx[1];
	struct oadt_ctx_s oadtctx[1];
	struct job_s rplj;
	/* in args */
	su_secu_t sec;
	tbs_t tbs;
	/* allow to filter for 64 time stamps at once */
	unsigned int nfilt = 0;
	bool moarp = true;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	sec = udpc_seria_des_secu(sctx);
	tbs = udpc_seria_des_tbs(sctx);
	/* get all the timestamps */
	if ((nfilt = snarf_times(sctx, oadtctx->filt, NFILT)) == 0) {
		return;
	}
	UD_DEBUG("0x%04x (UD_SVC_TICK_BY_INSTR): secu %s for %u time stamps\n",
		 UD_SVC_TICK_BY_INSTR, secbugger(sec), nfilt);

	/* initialise our context */
	oadtctx->sctx = rplsctx;
	oadtctx->secu = sec;
	oadtctx->nfilt = nfilt;
	oadtctx->tbs = tbs;

	for (index_t i = 0; moarp;) {
		/* prepare the reply packet ... */
		copy_pkt(&rplj, j);
		clear_pkt(rplsctx, &rplj);
		/* process some time stamps, this fills the packet */
		moarp = (i = proc_some(oadtctx, i)) < oadtctx->nfilt;
		/* send what we've got so far */
		send_pkt(rplsctx, &rplj);
	} while (moarp);

	/* we cater for any frobnication desires now */
	frobnicate();
	return;
}


/* urn getter */
static void
get_urn_cb(tsc_ce_t ce, void *clo)
{
	udpc_seria_t sctx = clo;
	UD_DEBUG("series for %s from %i till %i\n",
		 secbugger(ce->key->secu), ce->key->beg, ce->key->end);
	udpc_seria_add_data(sctx, ce, sizeof(*ce));
	return;
}

static void
instr_urn_svc(job_t j)
{
	struct udpc_seria_s sctx[1];
	/* in args */
	su_secu_t secu;
	/* to query the cube */
	struct tsc_key_s k;

	/* prepare the iterator for the incoming packet */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the header off of the wire */
	secu = udpc_seria_des_secu(sctx);
	UD_DEBUG("0x%04x (UD_SVC_GET_URN): %s\n",
		 UD_SVC_GET_URN, secbugger(secu));

	/* prepare the key */
	k.secu = secu;
	k.msk = 4 /* only secu is set */;

	/* reuse buf and sctx, just traverse the cube and send the bugger */
	clear_pkt(sctx, j);
	tsc_trav(gcube, &k, get_urn_cb, sctx);
	send_pkt(sctx, j);
	return;
}


/* fetch urn svc */
static void
fetch_urn_svc(job_t j)
{
	UD_DEBUG("0x%04x (UD_SVC_FETCH_URN)\n", UD_SVC_FETCH_URN);
	ud_set_service(UD_SVC_FETCH_URN, NULL, NULL);
	for (index_t i = 0; i < fetch_urn_hook->nf; i++) {
		fetch_urn_hook->f[i](j);
	}
	ud_set_service(UD_SVC_FETCH_URN, fetch_urn_svc, NULL);
	return;
}


void
dso_tseries_LTX_init(void *clo)
{
	UD_DEBUG("mod/tseries: loading ...");
	/* create the catalogue */
	gcube = make_tscube();
	/* tick service */
	//ud_set_service(UD_SVC_TICK_BY_TS, instr_tick_by_ts_svc, NULL);
	ud_set_service(UD_SVC_TICK_BY_INSTR, instr_tick_by_instr_svc, NULL);
	ud_set_service(UD_SVC_GET_URN, instr_urn_svc, NULL);
	ud_set_service(UD_SVC_FETCH_URN, fetch_urn_svc, NULL);
	UD_DBGCONT("done\n");

	/* have the frobq initialised */
	dso_tseries_frobq_LTX_init(clo);

	/* now kick off a fetch-URN job, dont bother about the
	 * job slot, it's unused anyway */
	wpool_enq(gwpool, (wpool_work_f)fetch_urn_svc, NULL, true);
	return;
}

void
dso_tseries_LTX_deinit(void *clo)
{
	/* have the frobq initialised */
	dso_tseries_frobq_LTX_deinit(clo);
	free_tscube(gcube);
	return;
}

/* dso-tseries.c */
