/*** ud-snap.c -- convenience tool to obtain a market snapshot at a given time
 *
 * Copyright (C) 2009, 2010 Sebastian Freundt
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

#include <stdio.h>
#include <stdbool.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "tseries.h"
#include <sushi/m30.h>

#include "ud-time.h"
#include "clihelper.c"
#include "dccp.h"

static struct ud_handle_s hdl[1];

static void
t_cb(su_secu_t s, scom_t th, void *UNUSED(clo))
{
	const_sl1t_t t = (const void*)th;
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);
	uint16_t ttf = scom_thdr_ttf(th);
	time_t ts = scom_thdr_sec(th);
	uint16_t ms = scom_thdr_msec(th);
	double v = ffff_m30_d(ffff_m30_get_ui32(t->v[0]));

	fprintf(stdout, "ii:%u/%i@%hu  tt:%d ts:%ld.%03hd v:%2.4f\n",
		qd, qt, p, ttf, ts, ms, v);
	return;
}

static time_t
parse_time(const char *t)
{
	struct tm tm;
	char *on;

	memset(&tm, 0, sizeof(tm));
	on = strptime(t, "%Y-%m-%d", &tm);
	if (on == NULL) {
		return 0;
	}
	if (on[0] == ' ' || on[0] == 'T' || on[0] == '\t') {
		on++;
	}
	(void)strptime(on, "%H:%M:%S", &tm);
	return timegm(&tm);
}

int
main(int argc, const char *argv[])
{
	/* vla */
	su_secu_t cid;
	time_t ts;
	uint32_t bs =
		(1 << SL1T_TTF_BID) |
		(1 << SL1T_TTF_ASK) |
		(1 << SL1T_TTF_TRA) |
		(1 << SL1T_TTF_STL) |
		(1 << SL1T_TTF_FIX);
	/* ud nonsense */
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = {.pbuf = buf, .plen = sizeof(buf)};
	struct udpc_seria_s sctx[1];

	if (argc <= 1) {
		fprintf(stderr, "Usage: ud-snap instr [date]\n");
		exit(1);
	}

	if (argc == 2) {
		ts = time(NULL);
	} else if ((ts = parse_time(argv[2])) == 0) {
		fprintf(stderr, "invalid date format \"%s\", "
			"must be YYYY-MM-DDThh:mm:ss\n", argv[2]);
		exit(1);
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* just a test */
	cid = secu_from_str(hdl, argv[1]);
	/* now kick off the finder */
	udpc_make_pkt(pkt, 0, 0, UD_SVC_MKTSNP);
	udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* ts first */
	udpc_seria_add_ui32(sctx, ts);
	/* secu and bs */
	udpc_seria_add_secu(sctx, cid);
	udpc_seria_add_tbs(sctx, bs);
	/* the dccp port we expect */
	udpc_seria_add_ui16(sctx, UD_NETWORK_SERVICE);
	pkt.plen = udpc_seria_msglen(sctx) + UDPC_HDRLEN;
	{
		int s = dccp_open(), res;

		fprintf(stderr, "socket %i\n", s);
		ud_send_raw(hdl, pkt);
		/* listen for traffic */
		res = dccp_accept(s, UD_NETWORK_SERVICE);
		fprintf(stderr, "listen %i\n", res);
		dccp_close(res);
		dccp_close(s);
	}
	/* and lose the handle again */
	free_unserding_handle(hdl);
	return 0;
}

/* ud-snap.c ends here */
