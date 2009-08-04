/*** instrumenstruation -- fiddling with instruments
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@math.tu-berlin.de>
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
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#if defined HAVE_POPT_H || 1
# include <popt.h>
#endif
#include "unserding.h"
#include <ffff/monetary.h>
#include <pfack/instruments.h>

typedef struct ga_spec_s *ga_spec_t;

struct ga_spec_s {
	long unsigned int gaid;
	long unsigned int specid;
	double strike;
	int right;
	time_t expiry;
};

#define outfile		stdout

static inline bool
properp(ga_spec_t sp)
{
	return sp->expiry > 0;
}

static time_t
parse_tstamp(const char *buf, char **on)
{
	struct tm tm;

	memset(&tm, 0, sizeof(tm));
	*on = strptime(buf, "%Y-%m-%d", &tm);
	if (*on != NULL) {
		return timegm(&tm);
	} else {
		return 0;
	}
}

static const_instr_t
udl(long unsigned int specid)
{
	return NULL;
}

static void
dump_option(ga_spec_t sp)
{
	struct instr_s i;
	size_t ssz;
	static char sbuf[4096];

	make_oxxxxx_into(
		&i, sp->gaid, udl(sp->specid), sp->right ? 'C' : 'P',
		/* exer style */ 'E',
		ffff_monetary32_get_d(sp->strike), 0);
	ssz = seria_instrument(sbuf, sizeof(sbuf), &i);
	fwrite(sbuf, 1, ssz, outfile);
	return;
}

static void
instrumentify(const char *buf, size_t bsz)
{
/* format goes cid - specid - strike - right - expiry */
	struct ga_spec_s sp;
	char *on;

	sp.gaid = strtoul(buf, &on, 10);
	while (*on++ != '\t');

	sp.specid = strtoul(on, &on, 10);
	while (*on++ != '\t');

	sp.strike = strtod(on, &on);
	while (*on++ != '\t');

	if (on[0] == '-' && on[1] == '1') {
		sp.right = -1;
	} else if (on[0] == '1') {
		sp.right = 1;
	} else {
		sp.right = 0;
	}
	while (*on++ != '\t');

	sp.expiry = parse_tstamp(on, &on);

	if (!properp(&sp)) {
		return;
	}
	/* otherwise create and dump the instrument */
	dump_option(&sp);
	return;
}

static void
rdlns(FILE *fp)
{
	size_t lbuf_sz = 256;
	char *lbuf = malloc(lbuf_sz);
	ssize_t sz;

	while ((sz = getline(&lbuf, &lbuf_sz, fp)) > 0) {
		instrumentify(lbuf, sz);
	}

	free(lbuf);
	return;
}

static void
process(const char *infile)
{
	FILE *fp = fopen(infile, "r");

	rdlns(fp);
	fclose(fp);
	return;
}

static void
usage(void)
{
	printf("instrumenstruation ga-contracts-file\n");
	return;
}

int
main(int argc, const char *argv[])
{
	if (argc > 0 && argv[1][0] != '-') {
		process(argv[1]);
	} else if ((argc > 0 && argv[1][1] == '\0') || (argc == 0)) {
		process("-");
	} else {
		usage();
	}
	return 0;
}

/* instrumenstruation.c ends here */
