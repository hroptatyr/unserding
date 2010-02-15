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
#include <stddef.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "tseries.h"
#include <sushi/m30.h>
#include <errno.h>

#include "ud-time.h"
#include "clihelper.c"
#include "dccp.h"
#include "tscube.h"
#include "utehelper.c"

static struct ud_handle_s hdl[1];

struct boxpkt_s {
	uint32_t boxno;
	uint32_t chunkno;
	char box[];
};


static void
t_cb(su_secu_t s, scom_t t, void *UNUSED(clo))
{
	pretty_print_secu(s);
	pretty_print_scom(t);
	return;
}

#if 0
/* could be in sushi */
static size_t
scom_size(scom_t t)
{
	return !scom_thdr_linked(t) ? 1 : 2;
}
#endif

static void
unwrap_box(char *buf, size_t bsz, void *clo)
{
	const void *t;
	struct udpc_seria_s sctx[1];

	udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PAYLLEN(bsz));
	while (udpc_seria_des_data(sctx, &t)) {
		t_cb(su_secu(0, 0, 0), t, clo);
	}
	return;
}

static void
recv_mktsnp(int s)
{
	int res;

	fprintf(stderr, "socket %i\n", s);
	/* listen for traffic */
	if ((res = dccp_accept(s, 4000)) > 0) {
		char b[UDPC_PKTLEN];
		ssize_t sz;

		while ((sz = dccp_recv(res, b, sizeof(b))) > 0) {
			fprintf(stderr, "pkt sz %zi\n", sz);
			unwrap_box(b, sz, NULL);
		}
		dccp_close(res);
	}
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
	int s;

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
	udpc_make_pkt(pkt, 0, 0, UD_SVC_TICK_BY_TS);
	udpc_seria_init(sctx, UDPC_PAYLOAD(buf), UDPC_PLLEN);
	/* ts first */
	udpc_seria_add_ui32(sctx, ts - 300);
	udpc_seria_add_ui32(sctx, ts);
	/* secu and bs */
	udpc_seria_add_tbs(sctx, bs);
	udpc_seria_add_secu(sctx, cid);
#if 0
/* that's gonna be a problem with variadic secus */
	/* the dccp port we expect */
	udpc_seria_add_ui16(sctx, UD_NETWORK_SERVICE);
#endif
	pkt.plen = udpc_seria_msglen(sctx) + UDPC_HDRLEN;

	/* open a dccp socket and make it listen */
	s = dccp_open();
	if (dccp_listen(s, UD_NETWORK_SERVICE) >= 0) {
		/* send the packet */
		ud_send_raw(hdl, pkt);
		/* ... and receive the answers */
		recv_mktsnp(s);
		/* and we're out */
		dccp_close(s);
	}

	/* and lose the handle again */
	free_unserding_handle(hdl);
	return 0;
}

/* ud-snap.c ends here */
