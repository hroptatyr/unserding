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
#define __USE_XOPEN
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "seria.h"
#include "svc-pong.h"

/* this is a bunch of time stamps */
typedef struct clks_s {
	struct timespec rt;
	struct timespec pt;
} *clks_t;

static size_t cnt;
static long unsigned int timeout;
static void(*mode)(ud_handle_t);


static void
__stamp(clockid_t cid, struct timespec *tgt)
{
	clock_gettime(cid, tgt);
	return;
}

static void
__diff(struct timespec *tgt, struct timespec *beg, struct timespec *end)
{
	if ((end->tv_nsec - beg->tv_nsec) < 0) {
		tgt->tv_sec = end->tv_sec - beg->tv_sec - 1;
		tgt->tv_nsec = 1000000000 + end->tv_nsec - beg->tv_nsec;
	} else {
		tgt->tv_sec = end->tv_sec - beg->tv_sec;
		tgt->tv_nsec = end->tv_nsec - beg->tv_nsec;
	}
	return;
}

static float
__as_f(struct timespec *src)
{
/* return time as float in milliseconds */
	return src->tv_sec * 1000.f + src->tv_nsec / 1000000.f;
}

/* higher level clock stuff */
static void
hrclock_stamp(clks_t tgt)
{
	__stamp(CLOCK_REALTIME, &tgt->rt);
	__stamp(CLOCK_PROCESS_CPUTIME_ID, &tgt->pt);
	return;
}

static void
hrclock_diff(clks_t tgt, clks_t beg, clks_t end)
{
	__diff(&tgt->rt, &beg->rt, &end->rt);
	__diff(&tgt->pt, &beg->pt, &end->pt);
	return;
}


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
cb(ud_packet_t pkt, void *clo)
{
	clks_t then = clo;
	struct clks_s now;
	struct udpc_seria_s sctx;
	static char hnname[16];

	if (pkt.plen == 0 || pkt.plen > UDPC_PKTLEN) {
		return false;
	}
	/* otherwise the packet is meaningful */
	hrclock_stamp(&now);
	hrclock_diff(&now, then, &now);

	/* fetch fields */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	/* fetch the host name */
	fetch_hnname(&sctx, hnname, sizeof(hnname));
	/* print */
	printf("%zu bytes from %s (...): "
	       "convo=%i time=%2.3f ms\n",
	       pkt.plen, hnname, udpc_pkt_cno(pkt), __as_f(&now.rt));
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
	ud_convo_t cno;
	/* referential stamp */
	struct clks_s clks;

	/* record the current time */
	hrclock_stamp(&clks);
	/* send off the bugger */
	cno = ud_send_simple(hdl, 0x0004);
	/* wait for replies */
	ud_subscr_raw(hdl, timeout, cb, &clks);
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
ncb(ud_packet_t pkt, void *clo)
{
	nego_clo_t nclo = clo;
	struct udpc_seria_s sctx;
	static char hnname[16];
	struct timespec now, us, em;
	uint8_t score;

	if (pkt.plen == 0 || pkt.plen > UDPC_PKTLEN) {
		return false;
	}
	/* otherwise the packet is meaningful */
	__stamp(CLOCK_REALTIME, &now);
	__diff(&us, &nclo->rt, &now);

	/* fetch fields */
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	/* fetch the host name */
	fetch_hnname(&sctx, hnname, sizeof(hnname));
	em.tv_sec = udpc_seria_des_ui32(&sctx);
	em.tv_nsec = udpc_seria_des_ui32(&sctx);
	score = udpc_seria_des_byte(&sctx);
	/* compute their stamp */
	__diff(&em, &nclo->rt, &em);
	/* keep track of their score */
	nclo->seen = ud_pong_set(nclo->seen, score);
	/* print */
	printf("name %s  roundtrip %2.3f ms  clkskew %2.3f ms  score %u\n",
	       hnname, __as_f(&us), __as_f(&em), score);
	return true;
}

static int
nego1(ud_handle_t hdl)
{
	ud_convo_t cno;
	/* referential stamp */
	struct nego_clo_s nclo;
	ud_pong_score_t s;

	/* record the current time, set wipe `seen' bitset */
	__stamp(CLOCK_REALTIME, &nclo.rt);
	nclo.seen = ud_empty_pong_set();
	/* send off the bugger */
	cno = ud_send_simple(hdl, 0x0004);
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
	printf("mart %ld\n", hdl->mart);
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
	init_unserding_handle(&__hdl, PF_INET6);
	/* call the mode function */
	(void)(*mode)(&__hdl);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
