/*** svc-pong.c -- pong service goodies
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

#include "unserding.h"
#include "unserding-nifty.h"
#include "svc-pong.h"
#include "seria-proto-glue.h"

typedef struct __clo_s {
	ud_handle_t hdl;
	ud_pong_set_t seen;
	ud_convo_t cno;
	struct timespec rtref;
} *__clo_t;

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

void
ud_svc_update_mart(ud_handle_t hdl, struct timespec then)
{
	struct timespec rtts = __lapse(then);
	if (rtts.tv_sec != 0) {
		/* more than a second difference?! ignore */
		return;
	}
	/* update hdl->mart */
	hdl->mart = 1 + (hdl->mart + rtts.tv_nsec / 1000000) / 2;
	return;
}

ud_pong_score_t
ud_svc_nego_score(ud_handle_t hdl, int timeout)
{
	struct __clo_s clo;

	/* fill in the closure */
	clo.seen = ud_empty_pong_set();
	clo.rtref = __stamp();
	clo.hdl = hdl;
	/* send off the bugger */
	clo.cno = ud_send_simple(hdl, UD_SVC_PING);
	/* wait for replies */
	ud_subscr_raw(hdl, timeout, cb, &clo);
	/* after they're all through, try and get a proper score */
	return hdl->score = ud_find_score(clo.seen);
}

/* svc-pong.c ends here */
