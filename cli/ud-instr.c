/*** ud-instr.c -- fetch instruments and time series intervals
 *
 * Copyright (C) 2005 - 2009 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <stdbool.h>
#include <popt.h>
#include <pfack/instruments.h>
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "ud-time.h"
#include "tscoll.h"

static bool xmlp;
static bool tslabp;

static inline size_t
print_ts_into(char *restrict tgt, size_t len, time_t ts)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	(void)gmtime_r(&ts, &tm);
	return strftime(tgt, len, "%Y-%m-%d %H:%M:%S", &tm);
}

static void
find_them_instrs(ud_handle_t hdl, const char *const *insv)
{
	if (insv == NULL) {
		/* no uii's at all */
		return;
	}
	/* read the UII's from the command line */
	for (const char *const *uii = insv; *uii; uii++) {
		uint32_t cid = strtol(*uii, NULL, 10);
		const void *data;
		size_t len;

		if (cid == 0) {
			continue;
		}
		if ((len = ud_find_one_instr(hdl, &data, cid)) > 0) {
			struct instr_s in;
			/* data here points to an xdr-encoded instr */
			deser_instrument_into(&in, data, len);
			print_instr(stdout, &in);
			fputc('\n', stdout);
		} else {
			fprintf(stdout, "%u unknown\n", cid);
		}
		if ((len = ud_find_one_tslab(hdl, &data, cid)) > 0) {
			/* data hereby points to a tseries object */
			const struct tseries_s *p = data;
			char los[32], his[32];
			/* debugging mumbo jumbo */
			print_ts_into(los, sizeof(los), p->from);
			print_ts_into(his, sizeof(his), p->to);
			fprintf(stdout, "  tslab %d %s..%s\n",
				p->types, los, his);
		} else {
			fputs("  no tslabs yet\n", stdout);
		}
	}
	return;
}


/* option magic */
typedef enum poptCallbackReason __attribute__((unused)) cbr_t;
typedef poptContext c_t;

static void
help_cb(
	poptContext con, enum poptCallbackReason UNUSED(foo),
	struct poptOption *key, UNUSED(const char *arg), UNUSED(void *data))
{
	if (key->shortName == 'h') {
		poptPrintHelp(con, stdout, 0);
	} else if (key->shortName == 'V') {
		fprintf(stdout, "ud-instr " PACKAGE_VERSION "\n");
	} else {
		poptPrintUsage(con, stdout, 0);
	}
	exit(0);
	return;
}

static struct poptOption out_opts[] = {
	{"xml", 'x', POPT_ARG_NONE,
	 &xmlp, 0,
	 "Output instrument specs in XML.", NULL},
	{"tslab", 't', POPT_ARG_NONE,
	 &tslabp, 0,
	 "Request tick slab for each instrument.", NULL},
	POPT_TABLEEND
};

static struct poptOption help_opts[] = {
	{NULL, '\0', POPT_ARG_CALLBACK, (void*)help_cb, 0, NULL, NULL},
	{"help", 'h', 0, NULL, '?', "Show this help message", NULL},
	{"version", 'V', 0, NULL, 'V', "Print version string and exit.", NULL},
	{"usage", '\0', 0, NULL, 'u', "Display brief usage message", NULL},
	POPT_TABLEEND
};

static struct poptOption porn_opts[] = {
	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, out_opts, 0,
	 "Output options", NULL},
	{NULL, '\0', POPT_ARG_INCLUDE_TABLE, help_opts, 0,
	 "Help options", NULL},
        POPT_TABLEEND
};

static const char *const*
ud_parse_cl(size_t argc, const char *argv[])
{
        int rc;
        poptContext opt_ctx;

        opt_ctx = poptGetContext(NULL, argc, argv, porn_opts, 0);
        poptSetOtherOptionHelp(
		opt_ctx,
		" "
		"uii [uii [...]]");

        /* auto-do */
        while ((rc = poptGetNextOpt(opt_ctx)) > 0) {
                /* Read all the options ... */
                ;
        }
        return poptGetArgs(opt_ctx);
}


int
main(int argc, const char *argv[])
{
	struct ud_handle_s __hdl;
	ud_handle_t hdl = &__hdl;
	const char *const *rest;

	/* parse the command line */
	rest = ud_parse_cl(argc, argv);
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* now kick off the finder */
	find_them_instrs(hdl, rest);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-instr.c ends here */
