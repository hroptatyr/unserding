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
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <popt.h>
#include "unserding.h"
#include <ffff/monetary.h>
#include <pfack/instruments.h>
#include "protocore.h"

typedef struct ictx_s *ictx_t;
typedef struct ga_spec_s *ga_spec_t;

struct ictx_s {
	int outfd;
	struct ud_handle_s hdl;
	void (*wrf)(ictx_t ctx, const char *buf, size_t bsz);
};

struct ga_spec_s {
	long unsigned int gaid;
	long unsigned int specid;
	double strike;
	int right;
	time_t expiry;
};


/* underlyers */
static struct instr_s __pfi_idx_dax, *pfi_idx_dax = &__pfi_idx_dax;
static struct instr_s __pfi_idx_esx, *pfi_idx_esx = &__pfi_idx_esx;
static struct instr_s __pfi_idx_cac, *pfi_idx_cac = &__pfi_idx_cac;
static struct instr_s __pfi_idx_ftse, *pfi_idx_ftse = &__pfi_idx_ftse;
static struct instr_s __pfi_idx_djia, *pfi_idx_djia = &__pfi_idx_djia;
static struct instr_s __pfi_idx_ndx, *pfi_idx_ndx = &__pfi_idx_ndx;
static struct instr_s __pfi_idx_rut, *pfi_idx_rut = &__pfi_idx_rut;
static struct instr_s __pfi_idx_spx, *pfi_idx_spx = &__pfi_idx_spx;
static struct instr_s __pfi_idx_xeo, *pfi_idx_xeo = &__pfi_idx_xeo;
static struct instr_s __pfi_idx_k200, *pfi_idx_k200 = &__pfi_idx_k200;

static struct instr_s __pfi_ccy_eur, *pfi_ccy_eur = &__pfi_ccy_eur;
static struct instr_s __pfi_ccy_usd, *pfi_ccy_usd = &__pfi_ccy_usd;
static struct instr_s __pfi_ccy_gbp, *pfi_ccy_gbp = &__pfi_ccy_gbp;
static struct instr_s __pfi_ccy_krw, *pfi_ccy_krw = &__pfi_ccy_krw;
static struct instr_s __pfi_ccy_cad, *pfi_ccy_cad = &__pfi_ccy_cad;

static void
init_indices(void)
{
	make_tixxxx_into(pfi_idx_dax, 1, "DAX");
	make_tixxxx_into(pfi_idx_esx, 2, "Stoxx50");
	make_tixxxx_into(pfi_idx_cac, 3, "CAC40");
	make_tixxxx_into(pfi_idx_ftse, 4, "FTSE");
	make_tixxxx_into(pfi_idx_djia, 5, "DJIA");
	make_tixxxx_into(pfi_idx_ndx, 6, "NDX");
	make_tixxxx_into(pfi_idx_rut, 7, "RUT");
	make_tixxxx_into(pfi_idx_spx, 8, "SPX");
	make_tixxxx_into(pfi_idx_xeo, 9, "XEO");
	make_tixxxx_into(pfi_idx_k200, 10, "K200");
	return;
}

static void
init_currencies(void)
{
	make_tcnxxx_into(pfi_ccy_eur, 73380, PFACK_4217_EUR_IDX);
	make_tcnxxx_into(pfi_ccy_usd, 73381, PFACK_4217_USD_IDX);
	make_tcnxxx_into(pfi_ccy_gbp, 73382, PFACK_4217_GBP_IDX);
	make_tcnxxx_into(pfi_ccy_krw, 73383, PFACK_4217_KRW_IDX);
	make_tcnxxx_into(pfi_ccy_cad, 73384, PFACK_4217_CAD_IDX);
	return;
}

static const_instr_t
udl(long unsigned int specid)
{
	switch (specid) {
	case 1:
	case 2:
		return pfi_idx_dax;
	case 3:
	case 4:
		return pfi_idx_esx;
	case 5:
	case 6:
		return pfi_idx_cac;
	case 7:
	case 8:
		return pfi_idx_ftse;
	case 9:
	case 10:
		return pfi_idx_djia;
	case 11:
	case 12:
		return pfi_idx_ndx;
	case 13:
	case 14:
		return pfi_idx_rut;
	case 15:
	case 16:
		return pfi_idx_spx;
	case 17:
	case 18:
		return pfi_idx_xeo;
	case 19:
	case 20:
		return pfi_idx_k200;
	default:
		/* bugger off */
		return NULL;
	}
}


static void
ordinary_write(ictx_t ctx, const char *buf, size_t bsz)
{
	write(ctx->outfd, buf, bsz);
	return;
}

static void
unserding_write(ictx_t ctx, const char *buf, size_t bsz)
{
	char pbuf[UDPC_PKTLEN];
	ud_packet_t pkt = {.pbuf = pbuf, .plen = sizeof(pbuf)};
	struct udpc_seria_s sctx;

	/* init the seria structure and prepare the pkt */
	udpc_make_pkt(pkt, 0, 0, 0x4216);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(pbuf), UDPC_PLLEN);
	/* send it off */
	udpc_seria_add_xdr(&sctx, buf, bsz);
	ud_send_raw(&ctx->hdl, pkt);
	return;
}


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

static void
dump_option(ictx_t ctx, ga_spec_t sp)
{
	const_instr_t refi = udl(sp->specid);
	struct instr_s i;
	size_t ssz;
	static char sbuf[4096];

	make_oxxxxx_into(
		&i, sp->gaid, refi, sp->right ? 'C' : 'P',
		/* exer style */ 'E',
		ffff_monetary32_get_d(sp->strike), 0);

	ssz = seria_instrument(sbuf, sizeof(sbuf), &i);
	ctx->wrf(ctx, sbuf, ssz);
	return;
}

static char out[16777216];

static void
prepare_dump(ictx_t ctx)
{
	/* dump index instruments here */
	size_t ssz;
	static char sbuf[4096];

#define SERIA_IDX(_x)					\
	ssz = seria_instrument(sbuf, sizeof(sbuf), _x);	\
	ctx->wrf(ctx, sbuf, ssz)

	SERIA_IDX(pfi_idx_dax);
	SERIA_IDX(pfi_idx_esx);
	SERIA_IDX(pfi_idx_cac);
	SERIA_IDX(pfi_idx_ftse);
	SERIA_IDX(pfi_idx_djia);
	SERIA_IDX(pfi_idx_ndx);
	SERIA_IDX(pfi_idx_rut);
	SERIA_IDX(pfi_idx_spx);
	SERIA_IDX(pfi_idx_xeo);
	SERIA_IDX(pfi_idx_k200);

#define SERIA_CCY(_x)					\
	ssz = seria_instrument(sbuf, sizeof(sbuf), _x);	\
	ctx->wrf(ctx, sbuf, ssz)

	SERIA_CCY(pfi_ccy_eur);
	SERIA_CCY(pfi_ccy_usd);
	SERIA_CCY(pfi_ccy_gbp);
	SERIA_CCY(pfi_ccy_krw);
	SERIA_CCY(pfi_ccy_cad);
	return;
}

static void
finish_dump(ictx_t unused)
{
	return;
}

static void
instrumentify(ictx_t ctx, const char *buf, size_t bsz)
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
	dump_option(ctx, &sp);
	return;
}

static void
rdlns(ictx_t ctx, FILE *fp)
{
	size_t lbuf_sz = 256;
	char *lbuf = malloc(lbuf_sz);
	ssize_t sz;

	/* pre-hook */
	prepare_dump(ctx);
	while ((sz = getline(&lbuf, &lbuf_sz, fp)) > 0) {
		instrumentify(ctx, lbuf, sz);
	}

	/* post-hook */
	finish_dump(ctx);
	free(lbuf);
	return;
}

static void
rdxdrs(FILE *fp)
{
	ssize_t nrd;

	if ((nrd = fread(out, 1, sizeof(out), fp)) > 0) {
		char *buf = out;
		size_t res = 0;

		fprintf(stderr, "deco %lu bytes\n", (long unsigned int)nrd);
		while (nrd > 0) {
			XDR hdl;
			struct instr_s this;

			xdrmem_create(&hdl, buf, nrd, XDR_DECODE);
			if (xdr_instr_s(&hdl, &this)) {
				res = xdr_getpos(&hdl);
			}
			xdr_destroy(&hdl);
			fprintf(stderr, "read %lu bytes\n",
				(long unsigned int)res);
			buf += res;
			nrd -= res;

			fprintf(stderr, "found instr %u ref %u\n",
				instr_gaid(&this),
				this.instance.instance_s_u.option.underlyer);
		}
	}
	return;
}


static void
process(ictx_t ctx, const char *infile)
{
	FILE *fp;

	if (infile != NULL) {
		fp = fopen(infile, "r");
	} else {
		fp = stdin;
	}
	rdlns(ctx, fp);
	fclose(fp);
	return;
}

static void
decipher(const char *infile)
{
	FILE *fp;

	if (infile != NULL) {
		fp = fopen(infile, "r");
	} else {
		fp = stdin;
	}
	rdxdrs(fp);
	fclose(fp);
	return;
}

static void
init_udnet(ictx_t ctx)
{
	ctx->outfd = -1;
	init_unserding_handle(&ctx->hdl, PF_INET6);
	ctx->wrf = unserding_write;
	return;
}

static void
init_outfile(ictx_t ctx, const char *outfile)
{
	if (outfile != NULL) {
		ctx->outfd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	} else {
		ctx->outfd = STDOUT_FILENO;
	}
	ctx->wrf = ordinary_write;
	return;
}

static void
deinit(ictx_t ctx)
{
	if (ctx->outfd > 0) {
		close(ctx->outfd);
	} else {
		free_unserding_handle(&ctx->hdl);
	}
	return;
}


/* the popt helper */
static int decipherp = 0;
static int uploadp = 0;
static char *outfile = NULL;

static const struct poptOption my_opts[] = {
	{"decipher", 'd', POPT_ARG_NONE, &decipherp, 0,
	 "Read instruments file and show its contents.", NULL },
	{"output", 'o', POPT_ARG_STRING, &outfile, 0,
	 "Output encoded/decoded data to OUTFILE, default stdout.",
	 "OUTFILE"},
	{"upload", 'u', POPT_ARG_NONE, &uploadp, 0,
	 "Upload instruments to unserding network.", NULL},
        POPT_TABLEEND
};

static const char *const*
parse_cl(size_t argc, const char *argv[])
{
        int rc;
        poptContext opt_ctx;

        opt_ctx = poptGetContext(NULL, argc, argv, my_opts, 0);
        poptSetOtherOptionHelp(opt_ctx, "[-d] [-o outfile] contracts");

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
	const char *const *rest;
	const char *infile = NULL;
	struct ictx_s __ctx, *ctx = &__ctx;

	/* parse the command line */
	if ((rest = parse_cl(argc, argv)) != NULL) {
		infile = rest[0];
	}

	if (uploadp) {
		init_udnet(ctx);
	} else {
		init_outfile(ctx, outfile);
	}

	if (!decipherp) {
		init_indices();
		init_currencies();
		process(ctx, infile);
	} else {
		decipher(infile);
	}
	/* close n free our resources */
	deinit(ctx);
	return 0;
}

/* instrumenstruation.c ends here */
