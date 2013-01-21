/*** svc-pong.c -- pong service goodies
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#include "unserding.h"
#include "unserding-nifty.h"
#include "svc-pong.h"
#include "boobs.h"

#if defined UD_COMPAT
#include "seria-proto-glue.h"
#include "ud-time.h"
#endif	/* UD_COMPAT */

struct __ping_s {
	uint32_t pid;
	uint8_t hnz;
	char hn[];
};

#if defined UD_COMPAT
typedef struct __clo_s {
	ud_handle_t hdl;
	ud_pong_set_t seen;
	ud_convo_t cno;
	struct timeval rtref;
} *__clo_t;
#endif	/* UD_COMPAT */


#if defined UD_COMPAT
static void
seria_skip_str(udpc_seria_t sctx)
{
	const char *p;
	(void)udpc_seria_des_str(sctx, &p);
	return;
}

static void
seria_skip_ui32(udpc_seria_t sctx)
{
	(void)udpc_seria_des_ui32(sctx);
	return;
}

static void
ud_svc_update_mart(ud_handle_t hdl, struct timeval then)
{
	struct timeval rtts = __ulapse(then);
	if (rtts.tv_sec != 0) {
		/* more than a second difference?! ignore */
		return;
	}
	/* update hdl->mart */
	hdl->mart = 1 + (hdl->mart + rtts.tv_usec / 1000U) / 2;
	return;
}
#endif	/* UD_COMPAT */


/* packing service */
static union {
	struct __ping_s wire;
	char buf[64];
} __msg;

#define MAX_HNZ		((uint8_t)(sizeof(__msg) - 6U))

int
ud_pack_ping(ud_sock_t sock, const struct svc_ping_s msg[static 1])
{
	/* 4 bytes for the pid */
	__msg.wire.pid = htobe32((uint32_t)msg->pid);
	if ((__msg.wire.hnz = (uint8_t)msg->hostnlen) > MAX_HNZ) {
		__msg.wire.hnz = sizeof(__msg) - 5;
	}
	memcpy(__msg.wire.hn, msg->hostname, __msg.wire.hnz);
	return ud_pack_msg(sock, &(struct ud_msg_s){
			.data = __msg.buf,
			.dlen = sizeof(__msg.buf),
			});
}

int
ud_chck_ping(struct svc_ping_s *restrict tgt, ud_sock_t sock)
{
	struct ud_msg_s msg[1];

	if (ud_chck_msg(msg, sock) < 0) {
		return -1;
	} else if (msg->dlen > sizeof(__msg)) {
		return -1;
	}
	/* otherwise memcpy to static buffer for inspection */
	memcpy(__msg.buf, msg->data, msg->dlen);
	if (__msg.wire.hnz > MAX_HNZ) {
		return -1;
	}
	tgt->hostnlen = __msg.wire.hnz;
	tgt->hostname = __msg.wire.hn;
	tgt->pid = be32toh(__msg.wire.pid);
	/* as a service, \nul terminate the hostname */
	__msg.wire.hn[__msg.wire.hnz] = '\0';
	return 0;
}

#if defined UD_COMPAT
/* conforms to ud_subscr_f */
static bool
cb(ud_packet_t pkt, ud_const_sockaddr_t UNUSED(sa), void *clo)
{
	__clo_t nclo = clo;
	struct udpc_seria_s sctx;
	uint8_t score;

	if (pkt.plen == 0 || pkt.plen > UDPC_PKTLEN) {
		/* means we've seen a timeout or a signal */
		return false;
	}
	/* otherwise the packet is meaningful */
	ud_svc_update_mart(nclo->hdl, nclo->rtref);
	/* fetch fields */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	/* first thing is a string which contains the hostname, just skip it */
	seria_skip_str(&sctx);
	/* next is a remote time stamp, skip it too */
	seria_skip_ui32(&sctx);
	seria_skip_ui32(&sctx);
	/* finally! */
	score = udpc_seria_des_byte(&sctx);
	/* keep track of seen scores */
	nclo->seen = ud_pong_set(nclo->seen, score);
	return true;
}

ud_pong_score_t
ud_svc_nego_score(ud_handle_t hdl, int timeout)
{
	struct __clo_s clo;

	/* fill in the closure */
	clo.seen = ud_empty_pong_set();
	clo.rtref = __ustamp();
	clo.hdl = hdl;
	/* send off the bugger */
	clo.cno = ud_send_simple(hdl, UD_SVC_PING.svcu);
	/* wait for replies */
	ud_subscr_raw(hdl, timeout, cb, &clo);
	/* after they're all through, try and get a proper score */
	return hdl->score = ud_find_score(clo.seen);
}
#endif	/* UD_COMPAT */

/* svc-pong.c ends here */
