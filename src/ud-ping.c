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
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "seria.h"

#define MAX_TIMEOUT	500

/* this is a bunch of time stamps */
typedef struct clks_s {
	struct timespec rt;
	struct timespec pt;
} *clks_t;

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;

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


static bool
cb(ud_packet_t pkt, void *clo)
{
	clks_t then = clo;
	struct clks_s now;

	if (pkt.plen == 0) {
		return false;
	}
	/* otherwise the packet is meaningful */
	hrclock_stamp(&now);
	hrclock_diff(&now, then, &now);
	printf("%2.6f ms-rt  %2.6f ms-pt\n", __as_f(&now.rt), __as_f(&now.pt));
	return true;
}

int
main(int argc, const char *UNUSED(argv[]))
{
	ud_convo_t cno;
	/* referential stamp */
	struct clks_s clks;

	if (argc <= 0) {
		fprintf(stderr, "Usage: ud-ping\n");
		exit(1);
	}

	/* obtain a new handle */
	init_unserding_handle(hdl, PF_INET6);
	/* record the current time */
	hrclock_stamp(&clks);
	/* send off the bugger */
	cno = ud_send_simple(hdl, 0x0004);
	/* wait */
	ud_subscr_raw(hdl, MAX_TIMEOUT, cb, &clks);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
