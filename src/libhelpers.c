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

/* our master include */
#include "unserding.h"
#include "protocore.h"
#include "protocore-private.h"
#include "xdr-instr-seria.h"
#include "tseries.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif

#define UD_SVC_TIMEOUT	50	/* milliseconds */
/* backoff schema, read from bottom to top */
static int ud_backoffs[] = {
	12 * UD_SVC_TIMEOUT,
	8 * UD_SVC_TIMEOUT,
	4 * UD_SVC_TIMEOUT,
	2 * UD_SVC_TIMEOUT,
	2 * UD_SVC_TIMEOUT,
};
#define NRETRIES	(sizeof(ud_backoffs) / sizeof(*ud_backoffs))

size_t
ud_find_one_instr(ud_handle_t hdl, char *restrict tgt, uint32_t cont_id)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
	ud_convo_t cno = hdl->convo++;
	size_t len;
	char *out = NULL;

	memset(buf, 0, sizeof(buf));
	udpc_make_pkt(pkt, cno, 0, UD_SVC_INSTR_BY_ATTR);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	udpc_seria_add_si32(&sctx, cont_id);
	/* prepare packet for sending im off */
	pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
	ud_send_raw(hdl, pkt);

	pkt.plen = sizeof(buf);
	ud_recv_convo(hdl, &pkt, UD_SVC_TIMEOUT, cno);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);
	if ((len = udpc_seria_des_xdr(&sctx, (void*)&out)) > 0) {
		memcpy(tgt, out, len);
	}
	return len;
}

#define index_t		size_t
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
			udpc_seria_add_si32(&sctx, cont_id[i]);
		}
#undef FILL
		/* prepare packet for sending im off */
		pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
		ud_send_raw(hdl, pkt);

		pkt.plen = sizeof(buf);
		ud_recv_convo(hdl, &pkt, (5 - retry) * UD_SVC_TIMEOUT, cno);
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


/* tick finders */
#undef USE_TICK_BY_TS
#define USE_TICK_BY_INSTR

#if defined USE_TICK_BY_TS
size_t
ud_find_one_price(ud_handle_t hdl, char *tgt, secu_t s, uint32_t bs, time_t ts)
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
ud_find_one_price(ud_handle_t hdl, char *tgt, secu_t s, uint32_t bs, time_t ts)
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

	pkt.plen = sizeof(buf);
	ud_recv_convo(hdl, &pkt, UD_SVC_TIMEOUT, cno);
	if (UNLIKELY((len = pkt.plen) == 0)) {
		return 0;
	}
	memcpy(tgt, UDPC_PAYLOAD(pkt.pbuf), len - UDPC_HDRLEN);
	return len - UDPC_HDRLEN;
}
#endif	/* USE_TICK_BY_TS || USE_TICK_BY_INSTR */

void
ud_find_1oadt(ud_handle_t hdl, sl1oadt_t tgt, secu_t s, uint32_t bs, time_t ts)
{
	struct udpc_seria_s sctx;
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.plen = sizeof(buf), .pbuf = buf};
	ud_convo_t cno = hdl->convo++;

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

	pkt.plen = sizeof(buf);
	ud_recv_convo(hdl, &pkt, UD_SVC_TIMEOUT, cno);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);
	udpc_seria_des_sl1oadt(tgt, &sctx);
	return;
}

static size_t
max_num_ticks(uint32_t bitset)
{
#if defined __SSE4_2__
	__asm__("popcnt %0\n"
		: "=r" (bitset) : "rm" (bitset));
	return bitset;
#else
/* stolen from http://www-graphics.stanford.edu/~seander/bithacks.html */
	/* Magic Binary Numbers */
	static const int S[] = {1, 2, 4, 8, 16};
	static const int B[] = {
		0x55555555, 0x33333333, 0x0F0F0F0F, 0x00FF00FF, 0x0000FFFF};
	size_t cnt = bitset;

	cnt = cnt - ((cnt >> 1) & B[0]);
	cnt = ((cnt >> S[1]) & B[1]) + (cnt & B[1]);
	cnt = ((cnt >> S[2]) + cnt) & B[2];
	cnt = ((cnt >> S[3]) + cnt) & B[3];
	cnt = ((cnt >> S[4]) + cnt) & B[4];
	return cnt;
#endif	/* !SSE */
}

void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(sl1tick_t, void *clo), void *clo,
	secu_t s, size_t slen,
	uint32_t bs, time_t ts)
{
	index_t rcvd = 0;
	index_t retry = NRETRIES;
	struct tick_by_ts_hdr_s hdr = {.ts = ts, .types = bs};

	do {
		struct udpc_seria_s sctx;
		struct sl1tick_s t;
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
			udpc_seria_add_secu(&sctx, &s[i]);
		}
#undef FILL
		/* prepare packet for sending im off */
		pkt.plen = udpc_seria_msglen(&sctx) + UDPC_HDRLEN;
		ud_send_raw(hdl, pkt);

		pkt.plen = sizeof(buf);
		ud_recv_convo(hdl, &pkt, (5 - retry) * UD_SVC_TIMEOUT, cno);
		udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);

		/* we assume that instrs are sent in the same order as
		 * requested *inside* the packet */
		while (udpc_seria_des_sl1tick(&t, &sctx)) {
			/* marking-less approach, so we could make s[] const */
			if (sl1tick_instr(&t) == s[rcvd].instr) {
				rcvd++;
			}
			/* callback */
			cb(&t, clo);
			retry = NRETRIES;
		}
	} while (rcvd < slen && retry > 0);
	return;
}


/* by instr aka 4222 */
typedef struct ftbi_ctx_s *ftbi_ctx_t;
struct ftbi_ctx_s {
	ud_handle_t hdl;
	uint8_t retry;
	uint8_t rcvd;
	ud_convo_t cno;
	char buf[UDPC_PKTLEN];
	struct sl1oadt_s oadt;
	struct udpc_seria_s sctx;
	secu_t secu;
	uint32_t types;
	ud_packet_t pkt;
	/* hope this still works on 32b systems
	 * oh oh, this implicitly encodes NFILL (which is 64 at the mo) */
	uint64_t seen;
	void(*cb)(sl1oadt_t, void *clo);
	void *clo;
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
whereis(sl1oadt_t t, time_t ts[], size_t nts)
{
/* look if the tick in t has been asked for and return the index */
	for (index_t i = 0; i < nts; i++) {
		if (sl1oadt_dse(t) == time_to_dse(ts[i])) {
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
	memset(&bictx->oadt, 0, sizeof(bictx->oadt));
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
	return;
}

static void
send_stamps(ftbi_ctx_t bictx)
{
	bictx->pkt.plen = udpc_seria_msglen(&bictx->sctx) + UDPC_HDRLEN;
	ud_send_raw(bictx->hdl, bictx->pkt);
	return;
}

static void
recv_ticks(ftbi_ctx_t bc)
{
	int to = bc->retry >= NRETRIES
		? UD_SVC_TIMEOUT : ud_backoffs[bc->retry];
	bc->pkt.plen = sizeof(bc->buf);
	ud_recv_convo(bc->hdl, &bc->pkt, to, bc->cno);
	udpc_seria_init(&bc->sctx, UDPC_PAYLOAD(bc->pkt.pbuf), bc->pkt.plen);
	return;
}

static void
frob_ticks(ftbi_ctx_t bictx, time_t ts[], size_t nts)
{
	while (udpc_seria_des_sl1oadt(&bictx->oadt, &bictx->sctx)) {
		index_t where;
		if ((where = whereis(&bictx->oadt, ts, nts)) < nts) {
			if (bictx->oadt.value[0] != OADT_ONHOLD) {
				bictx->rcvd++;
				/* mark it, use our mark vector */
				set_seen(bictx, where);
			}
		}
		/* callback */
		(*bictx->cb)(&bictx->oadt, bictx->clo);
		bictx->retry = NRETRIES;
	}
	return;
}

static inline void
lodge_closure(ftbi_ctx_t bictx, void(*cb)(sl1oadt_t, void *clo), void *clo)
{
	bictx->cb = cb;
	bictx->clo = clo;
	return;
}

static inline void
lodge_ihdr(ftbi_ctx_t bictx, secu_t secu, uint32_t types)
{
	bictx->secu = secu;
	bictx->types = types;
	return;
}

void
ud_find_ticks_by_instr(
	ud_handle_t hdl,
	void(*cb)(sl1oadt_t, void *clo), void *clo,
	secu_t s, uint32_t bs,
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
		/* eval, possibly marking them */
		frob_ticks(bictx, ts, tslen);
	} while (bictx->rcvd < tslen && bictx->retry > 0);
	return;
}

/* libhelpers.c ends here */
