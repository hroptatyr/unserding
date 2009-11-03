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
#include "xdr-instr-seria.h"

static struct ud_handle_s __hdl;
static ud_handle_t hdl = &__hdl;
static bool xmlp;

static void
in_cb(const char *buf, size_t len, void *UNUSED(clo))
{
	struct instr_s in;

	deser_instrument_into(&in, buf, len);
	fprintf(stderr, "d/l'd instrument: ");
	print_instr(stderr, &in);
	fputc('\n', stderr);
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
	/* vla */
	uint32_t cid[argc];
	const char *const *rest;
	int n = 0;

	/* parse the command line */
	rest = ud_parse_cl(argc, argv);

	/* read the UII's from the command line */
	if (rest == NULL) {
		/* no uii's at all */
		return 0;
	}
	for (const char *const *uii = rest; *uii; uii++) {
		if ((cid[n] = strtol(*uii, NULL, 10))) {
			n++;
		}
	}
	if (n == 0) {
		return 0;
	}
	/* obtain us a new handle */
	init_unserding_handle(hdl, PF_INET6, true);
	/* now kick off the finder */
	ud_find_many_instrs(hdl, in_cb, NULL, cid, n);
	/* and lose the handle again */
	free_unserding_handle(&__hdl);
	return 0;
}

/* ud-instr.c ends here */
