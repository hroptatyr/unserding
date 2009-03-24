/*** e123ify.c -- example client to reformat phone numbers compliant to E.123
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

#include "config.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#define HAVE_POPT
#include <popt.h>

#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"

/* could be used like so:
 * grep -o -e \
 *   "\"\\(\\+\\|(\\|\\[\\)[[:digit:]]\
 *   [- .()[:digit:]]\\{6,\\}[- /.,()[:alnum:]]*\"" \
 *   <infile> | e123ify -s */

/* our format structs */
typedef struct e123_fmt_s *e123_fmt_t;
typedef struct e123_fmtvec_s *e123_fmtvec_t;

struct e123_fmt_s {
	uint8_t idclen;
	uint8_t ndclen;
	uint8_t grplen[6];
	char idc[8];
	char ndc[8];
	uint8_t totlen;
	bool future;
	bool ndc_drops_naught;
};

struct e123_fmtvec_s {
	size_t nfmts;
	struct e123_fmt_s fmts[] __attribute__((aligned(16)));
};


static int sed_mode = 0;

#define TIMEOUT		100 /* milliseconds */

static bool
allowed_char_p(char c)
{
	/* likely chars first */
	switch (c) {
	case '0' ... '9':
	case '+':
	case 'a' ... 'z':
	case 'A' ... 'Z':
	case '/':
		return true;

	default:
		break;
	}
	return false;
}

static int
strip_number(char *restrict buf, const char *number)
{
	int len = 0;

	/* trivial cases first */
	if (number == NULL) {
		return 0;
	}

	/* strip stuff */
	for (int i = 0; number[i] != '\0'; i++) {
		/* only copy allowed characters */
		if (allowed_char_p(number[i])) {
			buf[len++] = number[i];
		}
	}
	return len;
}

static bool
usefulp(ud_packet_t pkt, void *closure)
{
	ud_convo_t cno = (ud_convo_t)(long unsigned int)closure;

	if (udpc_pkt_cno(pkt) != cno) {
		/* it's not our convo */
		return false;
	}
	if (pkt.pbuf[8] != UDPC_TYPE_SEQOF || pkt.pbuf[9] == 0x00) {
		/* answer is empty */
		return false;
	}
	return true;
}

static uint8_t
__ndigits(const char *number)
{
	uint8_t res = 0;

	for (const char *np = number; *np; np++) {
		if (*np >= '0' && *np <= '9') {
			res++;
		}
	}
	return res;
}


/* sender */
static ud_convo_t
e123ify_send(ud_handle_t hdl, const char *number)
{
	char buf[UDPC_SIMPLE_PKTLEN];
	char *restrict work_space;
	ud_packet_t pkt = {sizeof(buf), buf};
	ud_convo_t cno;
	uint8_t len;

	/* set up the packet, should be a fun? */
	memset(buf, 0, sizeof(buf));
	cno = ud_handle_convo(hdl);
	udpc_make_pkt(pkt, cno, /*pno*/0, UDPC_PKT_E123);

#define QRY_OFFSET	8
#define WS_OFFSET	QRY_OFFSET + 2
	/* add their args, the length is taken from the stripper */
	pkt.pbuf[QRY_OFFSET] = UDPC_TYPE_STRING;
	work_space = &pkt.pbuf[WS_OFFSET];
	len = pkt.pbuf[QRY_OFFSET + 1] = strip_number(work_space, number);

	/* set packet size and fill in any remaining args */
	pkt.plen = QRY_OFFSET + 2 + len;
	/* send him */
	ud_send_raw(hdl, pkt);
	hdl->convo++;
	return cno;
}

static char buf[UDPC_SIMPLE_PKTLEN];
static char *output = &buf[WS_OFFSET];

static uint8_t
__nfmts(const char *pbuf)
{
	/* return the number of fmt specs of the packet lying in buf */
	uint8_t res = pbuf[QRY_OFFSET + 1];
	/* check if it's maybe the empty fmt spec, as in future or unknown */
	if (res == 1 && pbuf[WS_OFFSET + 1] == 0) {
		return 0;
	}
	return res;
}

static uint8_t
__deser_e123_fmt(e123_fmt_t fmt, const char *pbuf)
{
	memset(fmt, 0, sizeof(*fmt));

#define IDC_OFFSET	3
#define NDC_OFFSET	IDC_OFFSET + 1 + fmt->idclen + 1
	fmt->idclen = pbuf[IDC_OFFSET + 1];
	fmt->ndclen = pbuf[NDC_OFFSET + 1];
	for (uint8_t i = 0, j = 4 + fmt->idclen + 2 + fmt->ndclen + 1;
	     j < pbuf[1] + 2; i++, j++) {
		fmt->grplen[i] = pbuf[j];
	}
	fmt->totlen = pbuf[2];

	/* copy the idc and ndc */
	memcpy(fmt->idc, &pbuf[IDC_OFFSET + 2], fmt->idclen);
	memcpy(fmt->ndc, &pbuf[NDC_OFFSET + 2], fmt->ndclen);

	return pbuf[1] + 2;
}

static void
__deser_e123_fmtvec(e123_fmtvec_t fmtvec, const char *pbuf)
{
	uint8_t offs = WS_OFFSET;

	/* store the number of format specs */
	fmtvec->nfmts = __nfmts(pbuf);
	/* and deserialise them one by one */
	for (uint8_t i = 0; i < fmtvec->nfmts; i++) {
		offs += __deser_e123_fmt(&fmtvec->fmts[i], &pbuf[offs]);
	}
	return;
}

static e123_fmtvec_t
e123ify_recv(ud_handle_t hdl, ud_convo_t cno)
{
	ud_packet_t pkt = {sizeof(buf), buf};
	void *clo = (void*)(long unsigned int)cno;
	e123_fmtvec_t res;
	uint8_t nfmts;

	/* clean the packet space */
	memset(pkt.pbuf, 0, UDPC_SIMPLE_PKTLEN);
	ud_recv_pred(hdl, &pkt, TIMEOUT, usefulp, clo);

	if (!usefulp(pkt, clo)) {
		return NULL;
	}
	/* otherwise just deserialise the data into a fmtvec structure */
	if ((nfmts = __nfmts(pkt.pbuf)) == 0) {
		return NULL;
	}
	/* make room for the format specs */
	res = malloc(aligned_sizeof(struct e123_fmtvec_s) +
		     aligned_sizeof(struct e123_fmt_s));
	/* deserialise */
	__deser_e123_fmtvec(res, pkt.pbuf);
	return res;
}

#if 0
/* reformat the inbuf */
static uint8_t
__e123ify_1(char *restrict resbuf, const char *inbuf, e123_fmt_t fmt)
{
	size_t inblen = strlen(inbuf);
	char tmpin[256], *mp = match;
	uint8_t ii = 0, ri = 0;

	if (UNLIKELY(match == NULL)) {
		return 0;
	}
	if (*mp++ != '[') {
		return 0;
	}

#define WILDCARD	'#'
	/* buffer inbuf temporarily, assume stripped string */
	memset(tmpin, WILDCARD, sizeof(tmpin));
	memcpy(tmpin, inbuf, inblen);

	/* go through the format specs */
	/* copy the initial '+' */
	resbuf[ri++] = tmpin[ii++];

	do {
		for (int i = 0, n = *mp++ - '0'; i < n; i++) {
			resbuf[ri++] = tmpin[ii++];
		}
		resbuf[ri++] = ' ';
		++mp;
	} while ((*mp >= '0' && *mp <= '9') ||
		 /* always false */
		 (resbuf[--ri] = '\0'));
	/* copy the rest */
	while (ii < inblen) {
		resbuf[ri++] = tmpin[ii++];
	}
	return ri;
}
#endif
static char missing_char = '#';
static char excess_char = ' ';

static void
__e123_apply(e123_fmt_t fmt, uint8_t ndigs, const char *arg)
{
	fputc('+', stdout);
	fputs(fmt->idc, stdout);
	fputc(' ', stdout);
	fputs(fmt->ndc, stdout);
	fputc('\n', stdout);
	return;
	/* otherwise care about the output */
	if (!sed_mode) {
		fputs(output, stdout);
		fputc('\n', stdout);
	} else {
		fputc('s', stdout);
		fputc('/', stdout);
		fputs(arg, stdout);
		fputc('/', stdout);
		fputs(output, stdout);
		fputc('/', stdout);
		fputc('\n', stdout);
	}
	return;
}

static void
__e123ify(ud_handle_t hdl, const char *arg)
{
	ud_convo_t cno;	
	e123_fmtvec_t fv;
	uint8_t ndigs;
	int8_t fvi;

	/* query the bugger */
	cno = e123ify_send(hdl, arg);
	/* receive */
	if ((fv = e123ify_recv(hdl, cno)) == NULL) {
		fprintf(stderr, "No answers\n");
		return;
	}
	/* otherwise care about the output */
	ndigs = __ndigits(arg);
	/* find the right template to apply */
	for (fvi = fv->nfmts - 1; fvi >= 0; fvi--) {
		if (fv->fmts[fvi].totlen == ndigs) {
			break;
		}
	}
	/* apply */
	__e123_apply(&fv->fmts[fvi], ndigs, arg);
	return;
}


/* modes */
static void
cli_mode(ud_handle_t hdl, const char *const argv[])
{
	const char *arg;

	while ((arg = *argv++) != NULL) {
		__e123ify(hdl, arg);
	}
	return;
}

static void
stdio_mode(ud_handle_t hdl)
{
	ssize_t nread;
	size_t llen = 256;
	char *lbuf;

	/* get a line buffer */
	lbuf = malloc(llen);
	/* process line by line */
	while ((nread = getline(&lbuf, &llen, stdin)) > 0) {
		__e123ify(hdl, lbuf);
	}
	/* free the line buffer */
	free(lbuf);
	return;
}


#if defined HAVE_POPT
static const struct poptOption const __opts[] = {
        {"sed-mode", 's', POPT_ARG_NONE,
	 &sed_mode, 0,
	 "Print the original and the reformatted string like "
	 "\"s/original/reformatted/\"", NULL },
        POPT_AUTOHELP
        POPT_TABLEEND
};
#endif  /* HAVE_POPT */

static const char *const*
parse_cl(size_t argc, const char *argv[])
{
#if defined HAVE_POPT
        int rc;
        poptContext opt_ctx;

        opt_ctx = poptGetContext(NULL, argc, argv, __opts, 0);
        poptSetOtherOptionHelp(
		opt_ctx,
		"[reconciliation-options] "
		"module [module [...]]");

        /* auto-do */
        while ((rc = poptGetNextOpt(opt_ctx)) > 0) {
                /* Read all the options ... */
                ;
        }
        return poptGetArgs(opt_ctx);
#else  /* !HAVE_POPT */
# error "Get popt or get a life ..."
#endif  /* HAVE_POPT */
}


int
main(int argc, const char *argv[])
{
	struct ud_handle_s __hdl;
	const char *const *rest;

	/* obtain us a new handle */
	init_unserding_handle(&__hdl);

	/* parse cli options */
	rest = parse_cl(argc, argv);

	if (rest != NULL) {
		cli_mode(&__hdl, rest);
	} else {
		stdio_mode(&__hdl);
	}

	/* free the handle */
	free_unserding_handle(&__hdl);
	return 0;
}

/* e123ify.c ends here */
