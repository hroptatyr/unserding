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
#include "tseries.h"
#include "tseries-private.h"

/* corking/uncorking */
#include "ud-sock.h"
#include <errno.h>

#if defined UNSERSRV && defined TSER_DEBUG_FLAG
# define UD_DEBUG_TSER(args...)					\
	fprintf(logout, "[unserding/tseries] " args)
#else  /* !UNSERSRV || !TSER_DEBUG_FLAG */
# define UD_DEBUG_TSER(args...)
#endif	/* UNSERSRV && TSER_DEBUG_FLAG */

tscube_t gcube = NULL;
struct hook_s __fetch_urn_hook[1], *fetch_urn_hook = __fetch_urn_hook;

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


/* mktsnp getter */
#include "dccp.h"
#define NRETRIES	3
#define BOX_CHUNK_SIZE	1024

static volatile int gport;

struct bcb_clo_s {
	int sock;
	int pktcnt;
	tsc_key_t key;
	udpc_seria_t sctx;
	job_t rplj;
};

static void
rearm_cannon(struct bcb_clo_s *clo)
{
	/* prepare the reply packet ... */
	clear_pkt(clo->sctx, clo->rplj);
	return;
}

static void
fire_cannon(struct bcb_clo_s *clo)
{
	clo->rplj->blen = UDPC_HDRLEN + udpc_seria_msglen(clo->sctx);
	dccp_send(clo->sock, clo->rplj->buf, clo->rplj->blen);
	clo->pktcnt++;
	return;
}

static void
maybe_yield(struct bcb_clo_s *clo)
{
	if (udpc_seria_msglen(clo->sctx) >= UDPC_PLLEN - 32) {
		/* fire the old cannon */
		fire_cannon(clo);
		/* rearm the cannon */
		rearm_cannon(clo);
	}
	return;
}

static void
box_cb(tsc_box_t b, su_secu_t s, void *clo)
{
	struct bcb_clo_s *bcbclo = clo;
#if defined TSER_DEBUG_FLAG
	int sock = bcbclo->sock;
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);
#endif	/* TSER_DEBUG_FLAG */
	const_sl1t_t t, lim;

	UD_DEBUG_TSER("found match %u/%i@%hu %p  -> %i\n", qd, qt, p, b, sock);
	/* sequential scan, skip all stuff before the beg in question */
	lim = b->sl1t + b->nt * b->skip;
	for (t = b->sl1t;
	     t < lim && (time32_t)sl1t_stmp_sec(t) < bcbclo->key->beg;
	     t += b->skip);
	/* no copy all stuff that matches */
	for (;
	     t < lim && (time32_t)sl1t_stmp_sec(t) <= bcbclo->key->end;
	     t += b->skip) {
		udpc_seria_add_scom_secu(bcbclo->sctx, s, (scom_t)t);
		maybe_yield(bcbclo);
	}
	return;
}

static void
instr_tick_by_ts_svc(job_t j)
{
	struct udpc_seria_s sctx[1];
	struct udpc_seria_s rplsctx[1];
	struct job_s rplj[1];
	/* the key we need */
	struct tsc_key_s key = {
		/* make sure we let the system know what we want */
		.msk = 1 | 2 | 4,
	};
	uint16_t port;
	int s, res;
	/* allow to filter for 64 instruments at once */
	//su_secu_t filt[64];
	struct bcb_clo_s clo[1] = {{.key = &key}};

	/* prepare the iterator for the incoming packet,
	 * sig: 4220(ui32 tsb, ui32 tse, ui32 types, ui64 secu, ui64 secu, ...) */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the timestamp */
	key.beg = udpc_seria_des_ui32(sctx);
	/* read the timestamp */
	key.end = udpc_seria_des_ui32(sctx);
	/* read the types */
	key.ttf = udpc_seria_des_tbs(sctx);
	/* read the key secu, we read the first one */
	key.secu = udpc_seria_des_secu(sctx);
#if 0
	/* choose a random port */
	port = 8650 + (gport++ % 50);
#else
/* great thought, how's the client gonna know? */
	port = UD_NETWORK_SERVICE;
#endif
	UD_DEBUG("0x%04x (UD_SVC_TICK_BY_TS): %s at %i, %x mask (->%hu)\n",
		 UD_SVC_TICK_BY_TS, secbugger(key.secu), key.beg, key.ttf, port);

	/* use dccp here? */
	clo->sock = s = dccp_open();
	/* fill in the rest of the closure */
	clo->rplj = rplj;
	clo->sctx = rplsctx;
	clo->key = &key;
	copy_pkt(clo->rplj, j);
	clo->pktcnt = 0;
	if ((res = dccp_connect(s, j->sa, port, 4000 /*msecs*/)) >= 0) {
		/* prepare the reply packet ... */
		rearm_cannon(clo);
		tsc_find_cb(gcube, &key, box_cb, clo);
		fire_cannon(clo);
	} else {
		UD_DEBUG("sock %i timed out, res %i: %s\n",
			 s, res, strerror(errno));
	}
	UD_DEBUG("sent %i packets to sock %i\n", clo->pktcnt, s);
	dccp_close(s);
	return;
}


/* number of stamps we can process at once */
#define NFILT	64

typedef struct oadt_ctx_s *oadt_ctx_t;
struct oadt_ctx_s {
	udpc_seria_t sctx;
	su_secu_t secu;
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

static inline bool
__tbs_has_p(tbs_t tbs, uint16_t ttf)
{
	return tbs & (1UL << ttf);
}


static void
__bang_nexist(oadt_ctx_t octx, time32_t refts, uint16_t ttf)
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
	struct sl1t_s t[16];
	su_secu_t s[countof(t)];
	size_t ntk;
	time32_t last_seen;

	ntk = tsc_find(t, s, countof(t), gcube, &k);
	UD_DEBUG("found %zu for secu %s\n", ntk, secbugger(octx->secu));
	if (ntk == 0) {
		__bang_nexist(octx, ts, SL1T_TTF_UNK);
		return;
	}
	/* assume ascending order */
	for (index_t i = 0; i < ntk; i++) {
		sl1t_t this = t + i;
		scom_thdr_t hdr = this->hdr;
		last_seen = scom_thdr_sec(hdr);
		udpc_seria_add_secu(octx->sctx, s[scom_thdr_tblidx(hdr)]);
		if (!scom_thdr_linked(hdr)) {
			udpc_seria_add_sl1t(octx->sctx, this);
		} else {
			udpc_seria_add_scdl(octx->sctx, (const void*)this);
			i++;
		}
	}
	/* now if the last tick is not exactly our timestamp,
	 * issue a nexist as well */
	if (last_seen < ts) {
		__bang_nexist(octx, ts, SL1T_TTF_UNK);
	}
	return;
}

static inline bool
one_moar_p(oadt_ctx_t octx)
{
	size_t cur = udpc_seria_msglen(octx->sctx);
	size_t add = (sizeof(struct sl1t_s) + 2) * 5;
	return cur + add < UDPC_PLLEN;
}

static index_t
proc_some(oadt_ctx_t octx, index_t i)
{
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
	struct oadt_ctx_s octx[1];
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
	if ((nfilt = snarf_times(sctx, octx->filt, NFILT)) == 0) {
		return;
	}

	/* initialise our context */
	octx->sctx = rplsctx;
	octx->secu = sec;
	octx->nfilt = nfilt;
	octx->tbs = tbs;

	for (index_t i = 0; moarp;) {
		/* prepare the reply packet ... */
		copy_pkt(&rplj, j);
		clear_pkt(rplsctx, &rplj);
		/* process some time stamps, this fills the packet */
		moarp = (i = proc_some(octx, i)) < octx->nfilt;
		/* send what we've got so far */
		send_pkt(rplsctx, &rplj);
	} while (moarp);

	/* we cater for any frobnication desires now */
	frobnicate();
	return;
}


/* mktsnp getter, essentially like tick-by-ts but returns the last prices before
 * a given time stamp */
#include "dccp.h"
#define NRETRIES	3
#define BOX_CHUNK_SIZE	1024

/* use the bcb_clo_s above */

static inline bool
ttf_coincide_p(uint16_t tick_ttf, tsc_key_t key)
{
#if defined CUBE_ENTRY_PER_TTF
	return (tick_ttf & 0x0f) == key->ttf;
#else  /* !CUBE_ENTRY_PER_TTF */
	return (1 << (tick_ttf & 0x0f)) & key->ttf || (key->msk & 8) == 0;
#endif	/* CUBE_ENTRY_PER_TTF */
}

static void
mktsnp_cb(tsc_box_t b, su_secu_t s, void *clo)
{
	struct bcb_clo_s *bcbclo = clo;
#if defined TSER_DEBUG_FLAG
	int sock = bcbclo->sock;
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);
#endif	/* TSER_DEBUG_FLAG */
	const_sl1t_t lst[8] = {0};
	const_sl1t_t t, lim;

	UD_DEBUG_TSER("found match %u/%i@%hu %p  -> %i\n", qd, qt, p, b, sock);
	/* sequential scan, skip all stuff before the beg in question */
	lim = b->sl1t + b->nt * b->skip;
	for (t = b->sl1t;
	     t < lim && (time32_t)sl1t_stmp_sec(t) <= bcbclo->key->end;
	     t += b->skip) {
		uint16_t tkttf = sl1t_ttf(t);
		/* keep track */
		if (ttf_coincide_p(tkttf, bcbclo->key)) {
			lst[(tkttf & 0x0f)] = t;
		}
	}
	for (int i = 0; i < (int)countof(lst); i++) {
		if (lst[i] == 0) {
			continue;
		}
		udpc_seria_add_scom_secu(bcbclo->sctx, s, (scom_t)lst[i]);
		maybe_yield(bcbclo);
	}
	return;
}

static void
fetch_mktsnp_svc(job_t j)
{
	struct udpc_seria_s sctx[1];
	struct udpc_seria_s rplsctx[1];
	struct job_s rplj[1];
	/* the key we need */
	struct tsc_key_s key = {
		/* make sure we let the system know what we want */
		.msk = 1 | 2 | 4,
	};
	uint16_t port;
	int s, res;
	struct bcb_clo_s clo[1] = {{.key = &key}};

	/* prepare the iterator for the incoming packet
	 * sig: 4228(ui32 tsb, ui64 secu, ui32 types, ui16 port) */
	udpc_seria_init(sctx, UDPC_PAYLOAD(j->buf), UDPC_PLLEN);
	/* read the timestamp */
	key.beg = key.end = udpc_seria_des_ui32(sctx);
	/* read the key secu */
	key.secu = udpc_seria_des_secu(sctx);
	/* read the types */
	key.ttf = udpc_seria_des_tbs(sctx);
	port = udpc_seria_des_ui16(sctx);

	UD_DEBUG("0x%04x (UD_SVC_MKTSNP): %s at %i, %x mask (->%hu)\n",
		 UD_SVC_MKTSNP, secbugger(key.secu), key.beg, key.ttf, port);

	/* prepare the closure structure and initialise dccp */
	clo->sock = s = dccp_open();
	/* fill in the rest of the closure */
	clo->rplj = rplj;
	clo->sctx = rplsctx;
	clo->key = &key;
	copy_pkt(clo->rplj, j);
	clo->pktcnt = 0;

	if ((res = dccp_connect(s, j->sa, port, 4000 /*msecs*/)) >= 0) {
		UD_DEBUG("sock %i res %i\n", s, res);
		/* prepare the reply packet ... */
		rearm_cannon(clo);
		/* do the search dance */
		tsc_find_cb(gcube, &key, mktsnp_cb, clo);
		/* final fire */
		fire_cannon(clo);
	} else {
		UD_DEBUG("timed out, res %i: %s\n", res, strerror(errno));
	}
	UD_DEBUG("sent %i packets\n", clo->pktcnt);
	dccp_close(s);
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

static void
list_boxes_svc(job_t UNUSED(j))
{
	UD_DEBUG("0x%04x (UD_SVC_LIST_BOXES)\n", UD_SVC_LIST_BOXES);
	tsc_list_boxes(gcube);
	return;
}

static void
sched_prune_caches(void *clo)
{
	UD_DEBUG("pruning caches\n");
	tsc_prune_caches(clo);
	return;
}


void
dso_tseries_LTX_init(void *clo)
{
	UD_DEBUG("mod/tseries: loading ...");
	/* create the catalogue */
	gcube = make_tscube();
	/* tick service */
	ud_set_service(UD_SVC_TICK_BY_TS, instr_tick_by_ts_svc, NULL);
	ud_set_service(UD_SVC_TICK_BY_INSTR, instr_tick_by_instr_svc, NULL);
	ud_set_service(UD_SVC_GET_URN, instr_urn_svc, NULL);
	ud_set_service(UD_SVC_FETCH_URN, fetch_urn_svc, NULL);
	ud_set_service(UD_SVC_MKTSNP, fetch_mktsnp_svc, NULL);
	/* administrative services */
	ud_set_service(UD_SVC_LIST_BOXES, list_boxes_svc, NULL);
	/* cache cleaning */
	schedule_timer_every(clo, sched_prune_caches, gcube, 20.0);
	UD_DBGCONT("done\n");

	/* have the frobq initialised */
	dso_tseries_frobq_LTX_init(clo);

	/* now kick off a fetch-URN job, dont bother about the
	 * job slot, it's unused anyway */
	wpool_enq(gwpool, (wpool_work_f)fetch_urn_svc, NULL, false);
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
