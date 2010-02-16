/*** libhelpers.c -- helpers that do some of the services
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>

#include <pfack/instruments.h>
#include <pfack/uterus.h>
/* our master include */
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "protocore-private.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif

#define NRETRIES	2
#define USE_SUBSCR
/* when schedflo is in use we do not try to guess if there's more
 * data to come, instead we will evaluate the storminess flag of the
 * packet and if it's ONE it's guaranteed to be the last packet */
#define USE_SCHEDFLO

struct f1i_clo_s {
	ud_convo_t cno;
	size_t len;
	const void **tgt;
};

static bool
__f1i_cb(const ud_packet_t pkt, ud_const_sockaddr_t UNUSED(sa), void *clo)
{
	struct f1i_clo_s *bc = clo;
	struct udpc_seria_s sctx;

	if (UDPC_PKT_INVALID_P(pkt)) {
		bc->len = 0;
		return false;
	} else if (udpc_pkt_cno(pkt) != bc->cno) {
		/* we better ask for another packet */
		bc->len = 0;
		return true;
	}
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	if ((bc->len = udpc_seria_des_xdr(&sctx, bc->tgt)) == 0) {
		/* what? just wait a bit */
		return true;
	}
	/* no more packets please */
	return false;
}

size_t
ud_find_one_instr(ud_handle_t hdl, const void **tgt, uint32_t cont_id)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	ud_convo_t cno = hdl->convo++;
	struct f1i_clo_s __f1i_clo;

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_INSTR_BY_ATTR);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	udpc_seria_add_ui32(&sctx, cont_id);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	/* use timeout of 0, letting the mart system decide */
	__f1i_clo.cno = cno;
	__f1i_clo.tgt = tgt;
	ud_subscr_raw(hdl, 0, __f1i_cb, &__f1i_clo);
	return __f1i_clo.len;
}

size_t
ud_find_one_isym(ud_handle_t hdl, const void **tgt, const char *sym, size_t len)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	ud_convo_t cno = hdl->convo++;
	struct f1i_clo_s __f1i_clo;

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_INSTR_BY_ATTR);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	udpc_seria_add_str(&sctx, sym, len);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	/* use timeout of 0, letting the mart system decide */
	__f1i_clo.cno = cno;
	__f1i_clo.tgt = tgt;
	ud_subscr_raw(hdl, 0, __f1i_cb, &__f1i_clo);
	return __f1i_clo.len;
}

void
ud_find_many_instrs(
	ud_handle_t hdl,
	void(*cb)(const char *tgt, size_t len, void *clo), void *clo,
	uint32_t cont_id[], size_t len)
{
	index_t rcvd = 0;
	index_t retry = NRETRIES;

	do {
		struct udpc_seria_s sctx;
		char buf[UDPC_PKTLEN];
		ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
		char *out = NULL;
		ud_convo_t cno = hdl->convo++;
		size_t nrd;

		retry--;
		memset(buf, 0, sizeof(buf));
		udpc_make_pkt(pkt, cno, 0, UD_SVC_INSTR_BY_ATTR);
		udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
#define FILL	(UDPC_PLLEN / sizeof(struct instr_s))
		for (index_t j = 0, i = rcvd; j < FILL && i < len; i++, j++) {
			udpc_seria_add_ui32(&sctx, cont_id[i]);
		}
#undef FILL
		/* prepare packet for sending im off */
		pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
		ud_send_raw(hdl, pkt);

		/* let the mart system decide, or use backoffs? */
		pkt.plen = sizeof(buf);
		ud_recv_convo(hdl, &pkt, 0, cno);
		udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);

		/* we assume that instrs are sent in the same order as
		 * requested *inside* the packet */
		while ((nrd = udpc_seria_des_xdr(&sctx, (void*)&out)) > 0) {
			cb(out, nrd, clo);
			rcvd++;
			retry = NRETRIES;
		}
	} while (rcvd < len && retry > 0);
	return;
}


/* find tslabs, aka urns */
struct f1tsl_clo_s {
	ud_convo_t cno;
	size_t len;
	const void **tgt;
	void(*cb)(const void *d);
};

static bool
__f1tsl_cb(const ud_packet_t pkt, ud_const_sockaddr_t UNUSED(sa), void *clo)
{
	struct f1tsl_clo_s *bc = clo;
	struct udpc_seria_s sctx;
	const void *tgt[1];

	if (UDPC_PKT_INVALID_P(pkt)) {
		/* finish the subscription */
		return false;
	} else if (udpc_pkt_cno(pkt) != bc->cno) {
		/* we better ask for another packet */
		return true;
	}
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	if (bc->cb == NULL) {
		/* old behaviour, subject to disappear */
		if ((bc->len = udpc_seria_des_data(&sctx, bc->tgt)) == 0) {
			/* what? just wait a bit */
			return true;
		}
		/* no more packets please */
		return false;
	}
	/* otherwise */
	while (udpc_seria_des_data(&sctx, tgt)) {
		/* tgt[0] should be cast to const struct tsc_ce_s* */
		bc->cb(tgt[0]);
		bc->len++;
	}
	return true;
}

size_t
ud_find_one_tslab(ud_handle_t hdl, const void **tgt, su_secu_t s)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	ud_convo_t cno = hdl->convo++;
	struct f1tsl_clo_s __f1t_clo[1];

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_GET_URN);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* dispatch the secu */
	udpc_seria_add_secu(&sctx, s);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	/* use timeout of 0, letting the mart system decide */
	__f1t_clo->cno = cno;
	__f1t_clo->tgt = tgt;
	__f1t_clo->len = 0;
	ud_subscr_raw(hdl, 0, __f1tsl_cb, __f1t_clo);
	return __f1t_clo->len;
}

size_t
ud_find_tslabs(ud_handle_t hdl, su_secu_t s, void(*cb)(const void*))
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	ud_convo_t cno = hdl->convo++;
	struct f1tsl_clo_s __f1t_clo[1];

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_GET_URN);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* dispatch the secu */
	udpc_seria_add_secu(&sctx, s);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	/* use timeout of 0, letting the mart system decide */
	__f1t_clo->cno = cno;
	__f1t_clo->cb = cb;
	__f1t_clo->len = 0;
	ud_subscr_raw(hdl, 0, __f1tsl_cb, __f1t_clo);
	return __f1t_clo->len;
}


/* tick finders */
#undef USE_TICK_BY_TS
#define USE_TICK_BY_INSTR

#if defined USE_TICK_BY_TS
size_t
ud_find_one_price(
	ud_handle_t hdl, char *tgt, su_secu_t s, uint32_t bs, time_t ts)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
	ud_convo_t cno = hdl->convo++;
	struct tick_by_ts_hdr_s hdr = {.ts = ts, .types = bs};
	size_t len;

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_TICK_BY_TS);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* 4220(ts, tick_bitset, triples-of-instrs) */
	udpc_seria_add_tick_by_ts_hdr(&sctx, &hdr);
	udpc_seria_add_secu(&sctx, s);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	pkt.plen = sizeof(buf);
	ud_recv_convo(hdl, &pkt, UD_SVC_TIMEOUT, cno);
	if (UNLIKELY((len = pkt.plen) == 0)) {
		return 0;
	}
	memcpy(tgt, UDPC_PAYLOAD(pkt.pbuf), len - UDPC_HDRLEN);
	return len - UDPC_HDRLEN;
}
#elif defined USE_TICK_BY_INSTR
size_t
ud_find_one_price(
	ud_handle_t hdl, char *tgt, su_secu_t s, uint32_t bs, time_t ts)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
	ud_convo_t cno = hdl->convo++;
	size_t len;

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_TICK_BY_INSTR);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* 4222(secu, tick_bitset, ts, ts, ts, ...) */
	udpc_seria_add_secu(&sctx, s);
	udpc_seria_add_tbs(&sctx, bs);
	udpc_seria_add_ui32(&sctx, ts);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	/* let the mart system decide */
	pkt.plen = sizeof(buf);
	ud_recv_convo(hdl, &pkt, 0, cno);
	if (UNLIKELY((len = pkt.plen) == 0)) {
		return 0;
	}
	memcpy(tgt, UDPC_PAYLOAD(pkt.pbuf), len - UDPC_HDRLEN);
	return len - UDPC_HDRLEN;
}
#endif	/* USE_TICK_BY_TS || USE_TICK_BY_INSTR */

/* for __popcnt */
#include "aux.h"

static size_t
max_num_ticks(uint32_t bitset)
{
	return __popcnt(bitset);
}

static inline void
udpc_seria_add_tick_by_ts_hdr(udpc_seria_t sctx, time_t ts, tbs_t bs)
{
	udpc_seria_add_ui32(sctx, ts);
	udpc_seria_add_tbs(sctx, bs);
	return;
}

static inline void
udpc_seria_add_tick_by_instr_hdr(udpc_seria_t sctx, su_secu_t s, tbs_t bs)
{
	udpc_seria_add_secu(sctx, s);
	udpc_seria_add_tbs(sctx, bs);
	return;
}

void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(su_secu_t, scom_t, void *clo), void *clo,
	su_secu_t *s, size_t slen,
	tbs_t bs, time_t ts)
{
	index_t rcvd = 0;
	index_t retry = NRETRIES;

	do {
		struct udpc_seria_s sctx[1];
		struct sl1t_s t[1];
		char buf[UDPC_PKTLEN];
		ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
		ud_convo_t cno = hdl->convo++;
		su_secu_t sec;

		retry--;
		memset(buf, 0, sizeof(buf));
		udpc_make_pkt(pkt, cno, 0, UD_SVC_TICK_BY_TS);
		udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
/* compute me! */
#define FILL	(48 / max_num_ticks(bs))
		/* 4220(ts, tick_bitset, triples-of-instrs) */
		udpc_seria_add_tick_by_ts_hdr(sctx, ts, bs);
		for (index_t j = 0, i = rcvd; j < FILL && i < slen; i++, j++) {
			udpc_seria_add_secu(sctx, s[i]);
		}
#undef FILL
		/* prepare packet for sending im off */
		pkt.plen = udpc_seria_msglen(sctx) + UDPC_HDRLEN;
		ud_send_raw(hdl, pkt);

		/* let the mart system decide */
		pkt.plen = sizeof(buf);
		ud_recv_convo(hdl, &pkt, 0, cno);
		udpc_seria_init(sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);

		/* we assume that instrs are sent in the same order as
		 * requested *inside* the packet */
		while (su_secu_ui64(sec = udpc_seria_des_secu(sctx)) &&
		       udpc_seria_des_sl1t(t, sctx)) {
			/* marking-less approach, so we could make s[] const */
			if (su_secu_match_p(s[rcvd], sec)) {
				rcvd++;
			}
			/* callback */
			cb(sec, (const void*)t, clo);
			retry = NRETRIES;
		}
	} while (rcvd < slen && retry > 0);
	return;
}


/* find ticks by instr aka 4222 */
typedef struct ftbi_ctx_s *ftbi_ctx_t;
struct ftbi_ctx_s {
	ud_handle_t hdl;
	uint8_t retry;
	uint8_t rcvd;
	ud_convo_t cno;
	char buf[UDPC_PKTLEN];
	/* leave a bit of space here */
	struct sl1t_s sl1t[4];
	struct udpc_seria_s sctx[1];
	su_secu_t secu;
	tbs_t tbs;
	ud_packet_t pkt;
#if defined USE_SCHEDFLO
	size_t offs;
#else  /* !USE_SCHEDFLO */
	/* hope this still works on 32b systems
	 * oh oh, this implicitly encodes NFILL (which is 64 at the mo) */
	uint64_t seen;
#endif	/* USE_SCHEDFLO */
	void(*cb)(su_secu_t, scom_t, void *clo);
	void *clo;
	time_t *ts;
	size_t nts;
};

static inline void
init_bictx(ftbi_ctx_t bictx, ud_handle_t hdl)
{
	bictx->hdl = hdl;
	bictx->retry = NRETRIES;
	bictx->rcvd = 0;
#if defined USE_SCHEDFLO
	bictx->offs = 0;
#else  /* !USE_SCHEDFLO */
	bictx->seen = 0;
#endif	/* USE_SCHEDFLO */
	return;
}

#if !defined USE_SCHEDFLO
static index_t
whereis(const_sl1t_t t, time_t ts[], size_t nts)
{
/* look if the tick in t has been asked for and return the index */
	for (index_t i = 0; i < nts; i++) {
		if ((time_t)sl1t_stmp_sec(t) == ts[i]) {
			return i;
		}
	}
	return nts;
}
#endif	/* !USE_SCHEDFLO */

static void
new_convo(ftbi_ctx_t bictx)
{
	bictx->cno = bictx->hdl->convo++;
	bictx->pkt.plen = sizeof(bictx->buf);
	bictx->pkt.pbuf = bictx->buf;

	bictx->retry--;
	memset(bictx->buf, 0, sizeof(bictx->buf));
	memset(bictx->sl1t, 0, sizeof(*bictx->sl1t));
	udpc_make_pkt(bictx->pkt, bictx->cno, 0, UD_SVC_TICK_BY_INSTR);
	udpc_seria_init(bictx->sctx, UDPC_PAYLOAD(bictx->buf), UDPC_PLLEN);
	return;
}

#if !defined USE_SCHEDFLO
/* obsoleted by the use of the sched/flow system */
static inline bool
seenp(ftbi_ctx_t bictx, index_t i)
{
	return bictx->seen & (1 << i);
}

static inline bool
seen_all_p(ftbi_ctx_t bc)
{
	uint64_t msk = (1 << bc->nts) - 1;
	return (bc->seen & msk) == msk;
}

static inline void
set_seen(ftbi_ctx_t bictx, index_t i)
{
	bictx->seen |= (1 << i);
	return;
}
#endif	/* !USE_SCHEDFLO */

#define FILL(X)	(48 / max_num_ticks(X))
#if defined USE_SCHEDFLO
static void
feed_stamps(ftbi_ctx_t bictx)
{
	index_t i = bictx->offs;
	size_t flim = FILL(bictx->tbs) + i;

	if (UNLIKELY(flim > bictx->nts)) {
		flim = bictx->nts;
	}

	/* send the secu and the desired ticks */
	udpc_seria_add_tick_by_instr_hdr(bictx->sctx, bictx->secu, bictx->tbs);

	for (; i < flim; i++) {
		udpc_seria_add_ui32(bictx->sctx, bictx->ts[i]);
	}
	bictx->offs = i;
	return;
}

#else  /* !USE_SCHEDFLO */
static void
feed_stamps(ftbi_ctx_t bictx, time_t ts[], size_t nts)
{
#undef FILL
	/* send the secu and the desired ticks */
	udpc_seria_add_tick_by_instr_hdr(bictx->sctx, bictx->secu, bictx->tbs);
	for (index_t j = 0, i = 0; j < FILL(bictx->tbs) && i < nts; i++) {
		if (!seenp(bictx, i)) {
			udpc_seria_add_ui32(bictx->sctx, ts[i]);
			j++;
		}
	}
#if defined USE_SUBSCR
	bictx->ts = ts;
	bictx->nts = nts;
#endif	/* USE_SUBSCR */
	return;
}
#endif	/* USE_SCHEDFLO */
#undef FILL

static void
send_stamps(ftbi_ctx_t bictx)
{
	bictx->pkt.plen = udpc_seria_msglen(bictx->sctx) + UDPC_HDRLEN;
	ud_send_raw(bictx->hdl, bictx->pkt);
	return;
}

#if defined USE_SUBSCR
/* we combine recv_ticks() and frob_ticks() in here */
#define usa	UNUSED(sa)
static bool
__recv_tick_cb(const ud_packet_t pkt, ud_const_sockaddr_t usa, void *clo)
{
	ftbi_ctx_t bc = clo;

	if (UDPC_PKT_INVALID_P(pkt)) {
		return false;
	} else if (udpc_pkt_cno(pkt) != bc->cno) {
		/* we better ask for another packet */
		return true;
	}
	udpc_seria_init(
		bc->sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	while ((bc->secu = udpc_seria_des_secu(bc->sctx)).mux &&
	       udpc_seria_des_sl1t(bc->sl1t, bc->sctx)) {
#if !defined USE_SCHEDFLO
		index_t where;
		if ((where = whereis(bc->sl1t, bc->ts, bc->nts)) < bc->nts) {
			if (!sl1t_onhold_p(bc->sl1t)) {
				bc->rcvd++;
				/* mark it, use our mark vector */
				set_seen(bc, where);
			}
		}
#endif	/* !USE_SCHEDFLO */
		/* callback */
		bc->cb(bc->secu, (const void*)bc->sl1t, bc->clo);
		bc->retry = NRETRIES;
	}
	/* ask for more, true will keep the subscription */
#if defined USE_SCHEDFLO
	return udpc_pktstorminess(pkt) == UDPC_PKTSTORMINESS_MANY;
#else  /* !USE_SCHEDFLO */
	return !seen_all_p(bc);
#endif	/* USE_SCHEDFLO */
}

static void
recv_ticks(ftbi_ctx_t bc)
{
	/* we use a timeout of 0 to let the mart thingie decide */
	bc->pkt.plen = sizeof(bc->buf);
	ud_subscr_raw(bc->hdl, 0, __recv_tick_cb, bc);
	return;
}

#else  /* !USE_SUBSCR */
/* the old system, we receive exactly one packet of our convo, then call
 * frob_ticks() */
static void
recv_ticks(ftbi_ctx_t bc)
{
	/* we use a timeout of 0 to let the mart thingie decide */
	bc->pkt.plen = sizeof(bc->buf);
	ud_recv_convo(bc->hdl, &bc->pkt, 0, bc->cno);
	udpc_seria_init(bc->sctx, UDPC_PAYLOAD(bc->pkt.pbuf), bc->pkt.plen);
	return;
}

static void
frob_ticks(ftbi_ctx_t bictx)
{
	time_t *ts = bictx->ts;
	size_t nts = bictx->nts;

	/* we don't really need this, do we? */
	udpc_seria_des_secu(bictx->sctx);
	while (udpc_seria_des_sl1t(bictx->sl1t, bictx->sctx)) {
		index_t where;
		if ((where = whereis(bictx->sl1t, ts, nts)) < nts) {
			if (!sl1t_onhold_p(bictx->sl1t)) {
				bictx->rcvd++;
				/* mark it, use our mark vector */
				set_seen(bictx, where);
			}
		}
		/* callback */
		bictx->cb(bictx->secu, (const void*)bictx->sl1t, bictx->clo);
		bictx->retry = NRETRIES;
	}
	return;
}
#endif	/* USE_SUBSCR */

static inline void
lodge_closure(ftbi_ctx_t b, void(*cb)(su_secu_t, scom_t, void *clo), void *clo)
{
	b->cb = cb;
	b->clo = clo;
	return;
}

static inline void
lodge_stamps(ftbi_ctx_t b, time_t *ts, size_t tslen)
{
	b->ts = ts;
	b->nts = tslen;
	return;
}

static inline void
lodge_ihdr(ftbi_ctx_t bictx, su_secu_t secu, tbs_t tbs)
{
	bictx->secu = secu;
	bictx->tbs = tbs;
	return;
}

static inline bool
moarp(ftbi_ctx_t bictx)
{
#if defined USE_SCHEDFLO
	return bictx->offs < bictx->nts && bictx->retry > 0;
#else  /* !USE_SCHEDFLO */
	return bictx->rcvd < tslen && bictx->retry > 0;
#endif	/* USE_SCHEDFLO */
}

void
ud_find_ticks_by_instr(
	ud_handle_t hdl,
	void(*cb)(su_secu_t, scom_t, void *clo), void *clo,
	su_secu_t s, tbs_t bs,
	time_t *ts, size_t tslen)
{
/* fixme, the retry cruft should be a parameter? */
	struct ftbi_ctx_s bictx[1];

	init_bictx(bictx, hdl);
	lodge_ihdr(bictx, s, bs);
	lodge_closure(bictx, cb, clo);
	lodge_stamps(bictx, ts, tslen);

	do {
		/* initiate new conversation */
		new_convo(bictx);
		/* 4222(instr-triple, tick_bitset, ts, ts, ts, ...) */
		feed_stamps(bictx);
		/* prepare packet for sending im off */
		send_stamps(bictx);

		/* receive one answer, linear back off */
		recv_ticks(bictx);
#if !defined USE_SUBSCR
		/* eval, possibly marking them */
		frob_ticks(bictx);
#endif	/* !USE_SUBSCR */
	} while (moarp(bictx));
	return;
}


/* SVC_MKTSNP: 4228(ui32 ts, ui64 secu, ui32 types) */
#include "tscube.h"
#include "dccp.h"

typedef struct unwrbox_clo_s {
	void(*cb)(su_secu_t, scom_t, void *clo);
	void *clo;
} *unwrbox_clo_t;

static void
unwrap_box(char *buf, size_t bsz, void *clo)
{
	unwrbox_clo_t ub = clo;
	const struct scom_secu_s *st;
	struct udpc_seria_s sctx[1];

	udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(bsz));

	while (udpc_seria_des_scom_secu(sctx, &st)) {
		ub->cb(st->s, st->t, ub->clo);
	}
	return;
}

/* currently assembled box */
struct boxpkt_s {
	uint32_t boxno;
	uint32_t chunkno;
	char box[];
};

/* called when dccp packets arrive */
static void
recv_mktsnp(int s, void *clo)
{
	int res;

	/* listen for traffic */
	if ((res = dccp_accept(s, 4000 /*msecs*/)) > 0) {
		char b[UDPC_PKTLEN];
		ssize_t sz;

		while ((sz = dccp_recv(res, b, sizeof(b))) > 0) {
			unwrap_box(b, sz, clo);
		}
		dccp_close(res);
	}
	return;
}

void
ud_find_mktsnp(
	ud_handle_t hdl,
	void(*cb)(su_secu_t, scom_t, void *clo), void *clo,
	time_t ts, su_secu_t s, tbs_t bs)
{
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.pbuf = buf, .plen = sizeof(buf)};
	struct udpc_seria_s sctx[1];
	struct unwrbox_clo_s ubclo[1] = {{.cb = cb, .clo = clo}};
	int sock;

	/* now kick off the finder */
	udpc_make_pkt(pkt, 0, 0, UD_SVC_MKTSNP);
	udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* ts first */
	udpc_seria_add_ui32(sctx, ts);
	/* secu and bs */
	udpc_seria_add_secu(sctx, s.mux);
	udpc_seria_add_tbs(sctx, bs);
	/* the dccp port we expect */
	udpc_seria_add_ui16(sctx, UD_NETWORK_SERVICE);
	pkt.plen = udpc_seria_msglen(sctx) + UDPC_HDRLEN;

	/* open a dccp socket and make it listen */
	sock = dccp_open();
	if (dccp_listen(sock, UD_NETWORK_SERVICE) >= 0) {
		/* send the packet */
		ud_send_raw(hdl, pkt);
		/* ... and receive the answers */
		recv_mktsnp(sock, ubclo);
		/* and we're out */
		dccp_close(sock);
	}
	return;
}

/* libhelpers.c ends here */
