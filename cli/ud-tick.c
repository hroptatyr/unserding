/*** ud-tick.c -- convenience tool to obtain ticks by instruments
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
#include <time.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "tseries.h"
/* should be included somewhere */
#include <sushi/m30.h>

#include "clihelper.c"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;

static void
t1(scom_t t)
{
	const_sl1t_t tv = (const void*)t;

	fputc(' ', stdout);
	fputc(' ', stdout);
	switch (scom_thdr_ttf(t)) {
	case SL1T_TTF_BID:
		fputc('b', stdout);
		break;
	case SL1T_TTF_ASK:
		fputc('a', stdout);
		break;
	case SL1T_TTF_TRA:
		fputc('t', stdout);
		break;
	case SL1T_TTF_STL:
		fputc('x', stdout);
		break;
	case SL1T_TTF_FIX:
		fputc('f', stdout);
		break;
	case SL1T_TTF_UNK:
	default:
		fputc('@', stdout);
		break;
	}

	fprintf(stdout, ":%2.4f\n",
		ffff_m30_d(ffff_m30_get_ui32(tv->v[0])));
	return;
}

static void
ne(scom_t UNUSED(t))
{
	fputs("  v:does not exist\n", stdout);
	return;
}

static void
oh(scom_t UNUSED(t))
{
	fputs("  v:deferred\n", stdout);
	return;
}

static char
ttc(scom_t t)
{
	switch (scom_thdr_ttf(t)) {
	case SL1T_TTF_BID:
		return 'b';
	case SL1T_TTF_ASK:
		return 'a';
	case SL1T_TTF_TRA:
		return 't';
	case SL1T_TTF_STL:
		return 'x';
	case SL1T_TTF_FIX:
		return 'f';
	default:
		return 'u';
	}
}

static void
t_cb(su_secu_t s, scom_t t, void *UNUSED(clo))
{
	uint32_t qd = su_secu_quodi(s);
	int32_t qt = su_secu_quoti(s);
	uint16_t p = su_secu_pot(s);
	char ttf = ttc(t);
	int32_t ts = scom_thdr_sec(t);

	fprintf(stdout, "tick storm, ticks:1 ii:%u/%u@%hu tt:%c  ts:%i",
		qd, qt, p, ttf, ts);

	if (scom_thdr_nexist_p(t)) {
		ne(t);
	} else if (scom_thdr_onhold_p(t)) {
		oh(t);
	} else {
		t1(t);
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
	int n = 0;
	time_t ts[argc-1];
	uint32_t bs = \
		SL1T_TTF_BID | \
		SL1T_TTF_ASK | \
		SL1T_TTF_TRA | \
		SL1T_TTF_STL | \
		SL1T_TTF_FIX;

	if (argc <= 1) {
		fprintf(stderr, "Usage: ud-tick instr [date] [date] ...\n");
		exit(1);
	}

	if (argc == 2) {
		ts[0] = time(NULL);
	}

	for (int i = 2; i < argc; i++) {
		if ((ts[n] = parse_time(argv[i])) == 0) {
			fprintf(stderr, "invalid date format \"%s\", "
				"must be YYYY-MM-DDThh:mm:ss\n", argv[i]);
			exit(1);
		}
		n++;
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* just a test */
	cid = secu_from_str(hdl, argv[1]);
	/* now kick off the finder */
	ud_find_ticks_by_instr(hdl, t_cb, NULL, cid, bs, ts, n);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-tick.c ends here */
