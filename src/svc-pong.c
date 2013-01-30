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
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "unserding.h"
#include "svc-pong.h"
#include "ud-nifty.h"
#include "ud-private.h"
#include "boobs.h"

#if defined UD_COMPAT
# include "seria-proto-glue.h"
# include "ud-time.h"
#endif	/* UD_COMPAT */
#if defined UNSERMON_DSO
# include "unsermon.h"
#endif	/* UNSERMON_DSO */

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
#if !defined UNSERMON_DSO
static union {
	struct __ping_s wire;
	char buf[64];
} __msg;

#define MAX_HNZ		((uint8_t)(sizeof(__msg) - 6U))

int
ud_pack_ping(ud_sock_t sock, const struct svc_ping_s msg[static 1])
{
	ud_svc_t cmd;

	switch (msg->what) {
	case SVC_PING_PING:
		cmd = UD_CTRL_SVC(UD_SVC_PING);
		break;
	case SVC_PING_PONG:
		cmd = UD_CTRL_SVC(UD_SVC_PING + 1/*reply*/);
		break;
	default:
		return -1;
	}

	/* 4 bytes for the pid */
	__msg.wire.pid = htobe32((uint32_t)msg->pid);
	if ((__msg.wire.hnz = (uint8_t)msg->hostnlen) > MAX_HNZ) {
		__msg.wire.hnz = sizeof(__msg) - 5;
	}
	memcpy(__msg.wire.hn, msg->hostname, __msg.wire.hnz);

	(void)ud_flush(sock);
	return ud_pack_cmsg(sock, (struct ud_msg_s){
			.svc = cmd,
			.data = __msg.buf,
			.dlen = sizeof(__msg.buf),
		});
}

int
ud_pack_pong(ud_sock_t sock, unsigned int pongp)
{
/* PINGs can't be packed. */
	static struct svc_ping_s po;

	if (UNLIKELY(po.hostnlen == 0U)) {
		static char hname[HOST_NAME_MAX];
		if (gethostname(hname, sizeof(hname)) < 0) {
			return -1;
		}
		hname[HOST_NAME_MAX - 1] = '\0';
		po.hostnlen = strlen(hname);
		po.hostname = hname;
		po.pid = getpid();
	}

	if (pongp) {
		po.what = SVC_PING_PONG;
	} else {
		po.what = SVC_PING_PING;
	}
	return ud_pack_ping(sock, &po);
}

int
ud_chck_ping(struct svc_ping_s *restrict tgt, ud_sock_t sock)
{
	struct ud_msg_s msg[1];

	if (ud_chck_msg(msg, sock) < 0) {
		return -1;
	} else if (msg->dlen > sizeof(__msg)) {
		return -1;
	} else if ((msg->svc & ~0x01) != UD_CTRL_SVC(UD_SVC_PING)) {
		/* not a PING nor a PONG */
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
	tgt->what = msg->svc & 0x01 ? SVC_PING_PONG : SVC_PING_PING;
	/* as a service, \nul terminate the hostname */
	__msg.wire.hn[__msg.wire.hnz] = '\0';
	return 0;
}
#endif	/* UNSERMON_DSO */

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
	clo.cno = ud_send_simple(hdl, UD_SVC_PING);
	/* wait for replies */
	ud_subscr_raw(hdl, timeout, cb, &clo);
	/* after they're all through, try and get a proper score */
	return hdl->score = ud_find_score(clo.seen);
}
#endif	/* UD_COMPAT */


/* monitor service */
#if defined UNSERMON_DSO
static size_t
mon_dec_ping(
	char *restrict p, size_t z, ud_svc_t svc,
	const struct ud_msg_s m[static 1])
{
	static const char ping[] = "PING";
	static const char pong[] = "PONG";
	char *restrict q = p;

	switch (svc) {
	case UD_CTRL_SVC(UD_SVC_PING):
		memcpy(q, ping, sizeof(ping));
		break;
	case UD_CTRL_SVC(UD_SVC_PING + 1):
		memcpy(q, pong, sizeof(pong));
		break;
	default:
		return 0UL;
	}
	(q += sizeof(ping))[-1] = '\t';

	/* decipher the actual message */
	const union {
		struct __ping_s wire;
		char buf[64];
	} *pm;

	if (UNLIKELY((pm = m->data) == NULL || m->dlen != sizeof(*pm))) {
		*q++ = '?';
	} else {
		q += snprintf(q, z - (q - p), "%u\t", be32toh(pm->wire.pid));
		memcpy(q, pm->wire.hn, pm->wire.hnz);
		q += pm->wire.hnz;
	}
	return q - p;
}

int
ud_mondec_init(void)
{
	ud_mondec_reg(UD_CTRL_SVC(UD_SVC_PING), mon_dec_ping);
	ud_mondec_reg(UD_CTRL_SVC(UD_SVC_PING + 1), mon_dec_ping);
	return 0;
}
#endif	/* UNSERMON_DSO */

/* svc-pong.c ends here */
