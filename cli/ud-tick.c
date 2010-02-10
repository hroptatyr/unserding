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

#include "ud-time.h"
#include "clihelper.c"
#include "utehelper.c"

static struct ud_handle_s hdl[1];

static void
t_cb(su_secu_t s, scom_t t, void *UNUSED(clo))
{
	pretty_print_secu(s);
	pretty_print_scom(t);
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
	uint32_t bs =
		(1 << SL1T_TTF_BID) |
		(1 << SL1T_TTF_ASK) |
		(1 << SL1T_TTF_TRA) |
		(1 << SL1T_TTF_STL) |
		(1 << SL1T_TTF_FIX);

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
	free_unserding_handle(hdl);
	return 0;
}

/* ud-tick.c ends here */
