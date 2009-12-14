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
//#define USE_SUBSCR

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
};

static bool
__f1tsl_cb(const ud_packet_t pkt, ud_const_sockaddr_t UNUSED(sa), void *clo)
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
	if ((bc->len = udpc_seria_des_data(&sctx, bc->tgt)) == 0) {
		/* what? just wait a bit */
		return true;
	}
	/* no more packets please */
	return false;
}

size_t
ud_find_one_tslab(ud_handle_t hdl, const void **tgt, uint32_t cont_id)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	ud_convo_t cno = hdl->convo++;
	struct f1tsl_clo_s __f1t_clo;

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_GET_URN);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* dispatch the quodi */
	udpc_seria_add_ui32(&sctx, cont_id);
	/* all quotis */
	udpc_seria_add_ui32(&sctx, 0);
	/* all pots */
	udpc_seria_add_ui32(&sctx, 0);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	/* use timeout of 0, letting the mart system decide */
	__f1t_clo.cno = cno;
	__f1t_clo.tgt = tgt;
	ud_subscr_raw(hdl, 0, __f1tsl_cb, &__f1t_clo);
	return __f1t_clo.len;
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
	udpc_seria_add_ui32(&sctx, bs);
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

void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(scom_t, void *clo), void *clo,
	su_secu_t *s, size_t slen,
	uint32_t bs, time_t ts)
{
	index_t rcvd = 0;
	index_t retry = NRETRIES;
	struct tick_by_ts_hdr_s hdr = {.ts = ts, .types = bs};

	do {
		struct udpc_seria_s sctx;
		struct sl1t_s t;
		char buf[UDPC_PKTLEN];
		ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
		ud_convo_t cno = hdl->convo++;

		retry--;
		memset(buf, 0, sizeof(buf));
		udpc_make_pkt(pkt, cno, 0, UD_SVC_TICK_BY_TS);
		udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
/* compute me! */
#define FILL	(48 / max_num_ticks(bs))
		/* 4220(ts, tick_bitset, triples-of-instrs) */
		udpc_seria_add_tick_by_ts_hdr(&sctx, &hdr);
		for (index_t j = 0, i = rcvd; j < FILL && i < slen; i++, j++) {
			udpc_seria_add_secu(&sctx, s[i]);
		}
#undef FILL
		/* prepare packet for sending im off */
		pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
		ud_send_raw(hdl, pkt);

		/* let the mart system decide */
		pkt.plen = sizeof(buf);
		ud_recv_convo(hdl, &pkt, 0, cno);
		udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);

		/* we assume that instrs are sent in the same order as
		 * requested *inside* the packet */
		while (udpc_seria_des_sl1t(&t, &sctx)) {
			/* marking-less approach, so we could make s[] const */
			if (sl1tick_instr(&t) == su_secu_quodi(s[rcvd])) {
				rcvd++;
			}
			/* callback */
			cb(&t, clo);
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
	struct sparse_Dute_s Dute;
	struct udpc_seria_s sctx;
	su_secu_t secu;
	uint32_t types;
	ud_packet_t pkt;
	/* hope this still works on 32b systems
	 * oh oh, this implicitly encodes NFILL (which is 64 at the mo) */
	uint64_t seen;
	void(*cb)(spDute_t, void *clo);
	void *clo;
#if defined USE_SUBSCR
	time_t *ts;
	size_t nts;
#endif	/* USE_SUBSCR */
};

static inline void
init_bictx(ftbi_ctx_t bictx, ud_handle_t hdl)
{
	bictx->hdl = hdl;
	bictx->retry = NRETRIES;
	bictx->rcvd = 0;
	bictx->seen = 0;
	return;
}

static index_t
whereis(spDute_t t, time_t ts[], size_t nts)
{
/* look if the tick in t has been asked for and return the index */
	for (index_t i = 0; i < nts; i++) {
		if (t->pivot == time_to_dse(ts[i])) {
			return i;
		}
	}
	return nts;
}

static void
new_convo(ftbi_ctx_t bictx)
{
	bictx->cno = bictx->hdl->convo++;
	bictx->pkt.plen = sizeof(bictx->buf);
	bictx->pkt.pbuf = bictx->buf;

	bictx->retry--;
	memset(bictx->buf, 0, sizeof(bictx->buf));
	memset(&bictx->Dute, 0, sizeof(bictx->Dute));
	udpc_make_pkt(bictx->pkt, bictx->cno, 0, UD_SVC_TICK_BY_INSTR);
	udpc_seria_init(&bictx->sctx, UDPC_PAYLOAD(bictx->buf), UDPC_PLLEN);
	return;
}

static inline bool
seenp(ftbi_ctx_t bictx, index_t i)
{
	return bictx->seen & (1 << i);
}

static inline void
set_seen(ftbi_ctx_t bictx, index_t i)
{
	bictx->seen |= (1 << i);
	return;
}

static void
feed_stamps(ftbi_ctx_t bictx, time_t ts[], size_t nts)
{
#define FILL(X)	(48 / max_num_ticks(X))
	udpc_seria_add_secu(&bictx->sctx, bictx->secu);
	udpc_seria_add_ui32(&bictx->sctx, bictx->types);
	for (index_t j = 0, i = 0; j < FILL(bictx->types) && i < nts; i++) {
		if (!seenp(bictx, i)) {
			udpc_seria_add_ui32(&bictx->sctx, ts[i]);
			j++;
		}
	}
#undef FILL
#if defined USE_SUBSCR
	bictx->ts = ts;
	bictx->nts = nts;
#endif	/* USE_SUBSCR */
	return;
}

static void
send_stamps(ftbi_ctx_t bictx)
{
	bictx->pkt.plen = udpc_seria_msglen(&bictx->sctx) + UDPC_HDRLEN;
	ud_send_raw(bictx->hdl, bictx->pkt);
	return;
}

#if defined USE_SUBSCR
/* we combine recv_ticks() and frob_ticks() in here */
static bool
__recv_tick_cb(const ud_packet_t pkt, void *clo)
{
	ftbi_ctx_t bc = clo;

	if (UDPC_PKT_INVALID_P(pkt)) {
		return false;
	} else if (udpc_pkt_cno(pkt) != bc->cno) {
		/* we better ask for another packet */
		return true;
	}
	udpc_seria_init(
		&bc->sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	while (udpc_seria_des_spDute(&bc->Dute, &bc->sctx)) {
		index_t where;
		if ((where = whereis(&bc->Dute, bc->ts, bc->nts)) < bc->nts) {
			if (!spDute_onhold_p(&bc->Dute)) {
				bc->rcvd++;
				/* mark it, use our mark vector */
				set_seen(bc, where);
			}
		}
		/* callback */
		(*bc->cb)(&bc->Dute, bc->clo);
		bc->retry = NRETRIES;
	}
	/* ask for more */
	return true;
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
	udpc_seria_init(&bc->sctx, UDPC_PAYLOAD(bc->pkt.pbuf), bc->pkt.plen);
	return;
}

static void
frob_ticks(ftbi_ctx_t bictx, time_t ts[], size_t nts)
{
	while (udpc_seria_des_spDute(&bictx->Dute, &bictx->sctx)) {
		index_t where;
		if ((where = whereis(&bictx->Dute, ts, nts)) < nts) {
			if (!spDute_onhold_p(&bictx->Dute)) {
				bictx->rcvd++;
				/* mark it, use our mark vector */
				set_seen(bictx, where);
			}
		}
		/* callback */
		(*bictx->cb)(&bictx->Dute, bictx->clo);
		bictx->retry = NRETRIES;
	}
	return;
}
#endif	/* USE_SUBSCR */

static inline void
lodge_closure(ftbi_ctx_t bictx, void(*cb)(spDute_t, void *clo), void *clo)
{
	bictx->cb = cb;
	bictx->clo = clo;
	return;
}

static inline void
lodge_ihdr(ftbi_ctx_t bictx, su_secu_t secu, uint32_t types)
{
	bictx->secu = secu;
	bictx->types = types;
	return;
}

void
ud_find_ticks_by_instr(
	ud_handle_t hdl,
	ud_find_ticks_by_instr_cb_f cb, void *clo,
	su_secu_t s, uint32_t bs,
	time_t *ts, size_t tslen)
{
/* fixme, the retry cruft should be a parameter? */
	struct ftbi_ctx_s __bictx, *bictx = &__bictx;

	init_bictx(bictx, hdl);
	lodge_ihdr(bictx, s, bs);
	lodge_closure(bictx, cb, clo);

	do {
		/* initiate new conversation */
		new_convo(bictx);
		/* 4222(instr-triple, tick_bitset, ts, ts, ts, ...) */
		feed_stamps(bictx, ts, tslen);
		/* prepare packet for sending im off */
		send_stamps(bictx);

		/* receive one answer, linear back off */
		recv_ticks(bictx);
#if !defined USE_SUBSCR
		/* eval, possibly marking them */
		frob_ticks(bictx, ts, tslen);
#endif	/* !USE_SUBSCR */
	} while (bictx->rcvd < tslen && bictx->retry > 0);
	return;
}

/* libhelpers.c ends here */
