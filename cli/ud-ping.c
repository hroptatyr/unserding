/*** ud-ping.c -- ping/pong utility
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

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "svc-pong.h"

typedef struct clks_s {
	struct timespec rt;
	struct timespec pt;
} *clks_t;

/* this is the callback closure for the classic mode */
typedef struct ccb_clo_s {
	struct clks_s clks;
	long int seen;
	ud_convo_t cno;
} *ccb_clo_t;

static size_t cnt;
static long unsigned int timeout;
static void(*mode)(ud_handle_t);


/* field retrievers */
static void
fetch_hnname(udpc_seria_t sctx, char *buf, size_t bsz)
{
	const char *p;
	size_t s;
	memset(buf, 0, bsz);
	s = udpc_seria_des_str(sctx, &p);
	memcpy(buf, p, s);
	return;
}


/* modes we can do, classic mode */
static bool
cb(ud_packet_t pkt, ud_const_sockaddr_t sa, void *clo)
{
	ccb_clo_t ccb = clo;
	struct timespec lap;
	struct udpc_seria_s sctx;
	static char hnname[16];
	ud_pong_score_t score;

	if (pkt.plen == 0 || pkt.plen > UDPC_PKTLEN) {
		if (ccb->seen == 0) {
			printf("From " UD_MCAST6_ADDR "  convo=%i  "
			       "no servers on the network\n",
			       ccb->cno);
		}
		return false;
	}
	/* otherwise the packet is meaningful */
	lap = __lapse(ccb->clks.rt);
	/* count this response */
	ccb->seen++;
	/* fetch fields */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	/* fetch the host name */
	fetch_hnname(&sctx, hnname, sizeof(hnname));
	/* skip the next two */
	(void)udpc_seria_des_ui32(&sctx);
	(void)udpc_seria_des_ui32(&sctx);
	score = udpc_seria_des_byte(&sctx);
	/* print */
	{
		double lapf = __as_f(lap);
		char psa[INET6_ADDRSTRLEN];

		ud_sockaddr_ntop(psa, sizeof(psa), sa);
		printf("%zu bytes from %s (%s): "
		       "convo=%i score=%i time=%2.3f ms\n",
		       pkt.plen, hnname, psa, udpc_pkt_cno(pkt), score, lapf);
	}
	return true;
}

static void
handle_sigint(int UNUSED(signum))
{
	/* just set cnt to naught and we will exit the main loop */
	cnt = 0;
	return;
}

static int
ping1(ud_handle_t hdl)
{
	/* referential stamp */
	struct ccb_clo_s clo;

	/* record the current time */
	clo.clks.rt = __stamp();
	/* and we've seen noone yet */
	clo.seen = 0;
	/* send off the bugger */
	clo.cno = ud_send_simple(hdl, 0x0004);
	/* wait for replies */
	ud_subscr_raw(hdl, timeout, cb, &clo);
	return 0;
}

static void
classic_mode(ud_handle_t hdl)
{
	/* install sig handler */
	(void)signal(SIGINT, handle_sigint);
	/* to mimic ping(8) even more */
	puts("ud-ping " UD_MCAST6_ADDR " (" UD_MCAST6_ADDR ") 8 bytes of data");
	/* enter the `main loop' */
	while (cnt-- > 0) {
		ping1(hdl);
	}
	return;
}


/* another mode, negotiation mode, this will go to libunserding one day */
typedef struct nego_clo_s {
	struct timespec rt;
	ud_pong_set_t seen;
} *nego_clo_t;

static bool
ncb(ud_packet_t pkt, ud_const_sockaddr_t UNUSED(sa), void *clo)
{
	nego_clo_t nclo = clo;
	struct udpc_seria_s sctx;
	static char hnname[16];
	struct timespec us, em;
	uint8_t score;

	if (pkt.plen == 0 || pkt.plen > UDPC_PKTLEN) {
		return false;
	}
	/* otherwise the packet is meaningful */
	us = __lapse(nclo->rt);

	/* fetch fields */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	/* fetch the host name */
	fetch_hnname(&sctx, hnname, sizeof(hnname));
	em.tv_sec = udpc_seria_des_ui32(&sctx);
	em.tv_nsec = udpc_seria_des_ui32(&sctx);
	score = udpc_seria_des_byte(&sctx);
	/* compute their stamp */
	em = __lapse(em);
	/* keep track of their score */
	nclo->seen = ud_pong_set(nclo->seen, score);
	/* print */
	{
		double usf = __as_f(us);
		double emf = __as_f(em);
		printf("name %s  roundtrip %2.3f ms  clkskew %2.3f ms  "
		       "score %u\n",
		       hnname, usf, emf, (unsigned int)score);
	}
	return true;
}

static int
nego1(ud_handle_t hdl)
{
	/* referential stamp */
	struct nego_clo_s nclo;
	ud_pong_score_t s;

	/* record the current time, set wipe `seen' bitset */
	nclo.rt = __stamp();
	nclo.seen = ud_empty_pong_set();
	/* send off the bugger */
	(void)ud_send_simple(hdl, 0x0004);
	/* wait for replies */
	ud_subscr_raw(hdl, timeout, ncb, &nclo);
	/* after they're all through, try and get a proper score */
	s = ud_find_score(nclo.seen);
	printf("score would be %d\n", s);
	return 0;
}

static void
nego_mode(ud_handle_t hdl)
{
	puts("ud-ping negotiating in " UD_MCAST6_ADDR);
	/* enter the `main loop' */
	nego1(hdl);
	return;
}


static void
libnego_mode(ud_handle_t hdl)
{
	ud_pong_score_t s;
	puts("ud-ping negotiating in " UD_MCAST6_ADDR);
	s = ud_svc_nego_score(hdl, timeout);
	printf("lib nego'd %d\n", s);
	printf("mart %d\n", hdl->mart);
	return;
}


static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr, "\
Usage: ud-ping [-c count] [-n|--negotiation] [-i interval]\n");
	exit(1);
}

static void
parse_args(int argc, const char *argv[])
{
	/* set some defaults */
	timeout = 1000;
	cnt = -1UL;
	mode = &classic_mode;

	if (argc <= 0) {
		usage();
	}
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			usage();
		}
		switch (argv[i][1]) {
		case 'n':
			timeout = 250;
			mode = nego_mode;
			break;
		case 'l':
			/* secret switch */
			timeout = 250;
			mode = libnego_mode;
			break;
		case 'i':
			if (argv[i+1]) {
				timeout = strtol(argv[i+1], NULL, 10);
			}
			i++;
			break;
		case 'c':
			if (argv[i+1]) {
				cnt = strtol(argv[i+1], NULL, 10);
			}
			i++;
			break;
		default:
			usage();
			break;
		}
	}
	return;
}

int
main(int argc, const char *argv[])
{
	static struct ud_handle_s __hdl;

	/* look what the luser wants */
	parse_args(argc, argv);
	/* obtain a new handle */
	init_unserding_handle(&__hdl, PF_INET6, false);
	/* call the mode function */
	(void)(*mode)(&__hdl);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
