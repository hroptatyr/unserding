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
#include <pfack/instruments.h>
#include "xdr-instr-seria.h"

#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif

#define UD_SVC_TIMEOUT		50 /* milliseconds */

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
/* fixme, the retry cruft should be a parameter? */
	index_t rcvd = 0;
	index_t retry = 4;

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
			retry = 4;
		}
	} while (rcvd < len && retry > 0);
	return;
}


/* tick finders */
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

#if 0
static bool
fetch_slice(sl1tv_t tgt, index_t *k, ud_packet_t pkt, ud_convo_t cno)
{
	struct udpc_seria_s sctx;

	ud_recv_convo(hdl, &pkt, /* timeout */20, cno);
	if (pkt.plen == 0) {
		/* time out */
		return false;
	}
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), pkt.plen);
	/* fetch the gaid first */
	while (*k < tgt->len && udpc_seria_des_sl1tick(&tgt->vec[*k], &sctx)) {
		/* we're waiting for a TT_EOD (4) */
		print_tick(&tgt->vec[*k]);
		(*k)++;
	}
	return udpc_pktstorminess(pkt) == UDPC_PKTSTORMINESS_MANY;
}
#endif

void
ud_find_ticks_by_ts(
	ud_handle_t hdl,
	void(*cb)(sl1tick_t, void *clo), void *clo,
	secu_t s, size_t slen,
	uint32_t bs, time_t ts)
{
/* fixme, the retry cruft should be a parameter? */
	index_t rcvd = 0;
	index_t retry = 4;
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
#define FILL	(UDPC_PLLEN / (max_num_ticks(bs) * sizeof(struct sl1tick_s)))
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
			retry = 4;
		}
	} while (rcvd < slen && retry > 0);
	return;
}

/* libhelpers.c ends here */
