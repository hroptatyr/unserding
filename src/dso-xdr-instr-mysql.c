/*** dso-xdr-instr-mysql.c -- instrumentsfrom mysql databases
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>

/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"

#include <pfack/instruments.h>
#include "catalogue.h"
#include "xdr-instr-private.h"

#if defined HAVE_MYSQL
# if defined HAVE_MYSQL_MYSQL_H
#  include <mysql/mysql.h>
# elif defined HAVE_MYSQL_H
#  include <mysql.h>
# endif
#endif	/* HAVE_MYSQL */
/* some common routines */
#include "mysql-helpers.h"

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

/* for the mo, we don't need debugging output here */
#define UD_DEBUG_XDR(args...)

static void *conn = NULL;


static const char iqry[] =
	"SELECT "
#define GAID		0
	"`uiu`.`ga_instr_id`, "
#define INSTTY		1
	"`uiu`.`ga_instr_type_id`, "
#define TYNAME		2
	"`uiu`.`name`, "
#define CFI		3
	"`uiu`.`cfi`, "
#define SPEC_URN	4
	"`uiu`.`spec_urn`, "
	/* currency fields */
#define CCY_4217	5
	"`uiu`.`ccy_iso4217`, "
#define CCY_CODE	6
	"`uiu`.`ccy_iso4217_code`, "
	/* option fields */
#define OPT_UNDL	7
	"`uiu`.`opt_underlying`, "
#define OPT_STRIKE	8
	"`uiu`.`opt_strike`, "
#define OPT_DELIV	9
	"`uiu`.`opt_delivery`, "
#define OPT_MULTPL	10
	"`uiu`.`opt_cont_multiplier`, "
#define OPT_CFI		11
	"`uiu`.`opt_cfi`, "
#define OPT_DEFLT_CCY	12
	"`uiu`.`opt_default_ccy`, "
#define OPT_OPOL	13
	"`uiu`.`opt_opol`, "
	/* interest rate fields */
#define FRA_MMAT	14
	"`uiu`.`fra_maturity`, "
#define FRA_ANNU	15
	"`uiu`.`fra_dcc`, "
#define FRA_CCY		16
	"`uiu`.`fra_ccy`, "
	/* aliases */
	"`uiu`.`description`, "
#define IDENT_GA	18
	"`uiu`.`GA name`, "
#define IDENT_RIC	19
	"`uiu`.`RIC`, "
#define IDENT_BBG	20
	"`uiu`.`BBG`, "
#define IDENT_ISIN	21
	"`uiu`.`ISIN`, "
#define IDENT_GFD	22
	"`uiu`.`gfdsym` "
	"FROM `freundt`.`ud_instr_unrolled` AS uiu";

static void
make_currency(instr_t in, uint32_t id, void **rows, size_t UNUSED(nflds))
{
	uint16_t co = strtoul(rows[CCY_CODE], NULL, 10);
	make_tcnxxx_into(in, id, co);
	UD_DEBUG_XDR("made currency %s (%u)\n", instr_name(in), id);
	return;
}

static void
make_index(instr_t in, uint32_t id, void **rows, size_t UNUSED(nflds))
{
	make_tixxxx_into(in, id, rows[IDENT_GA]);
	UD_DEBUG_XDR("made index %s (%u)\n", instr_name(in), id);
	return;
}

static time_t
parse_tstamp(const char *buf)
{
	struct tm tm;
	char *on;

	memset(&tm, 0, sizeof(tm));
	on = strptime(buf, "%Y-%m-%d", &tm);
	if (on != NULL) {
		return timegm(&tm);
	} else {
		return 0;
	}
}

static double
parse_multiplier(const char *data)
{
	if (LIKELY(data != NULL)) {
		return strtod(data, NULL);
	}
	/* what's a good default here? */
	return 1.0;
}

static m30_t
parse_strike(const char *data)
{
	if (LIKELY(data != NULL)) {
		return ffff_m30_get_s(&data);
	}
	/* grrr */
	return ffff_m30_get_ui32(0);
}

static uint32_t
parse_undl(const char *data)
{
	return strtoul(data, NULL, 10);
}

static void
make_option(instr_t in, uint32_t id, void **rows, size_t UNUSED(nflds))
{
	uint32_t undl_id = parse_undl(rows[OPT_UNDL]);
	const char *cfi = rows[OPT_CFI];
	char right = cfi[1];
	char exer = cfi[2];
	double multpl_dbl = parse_multiplier(rows[OPT_MULTPL]);
	m30_t strike = parse_strike(rows[OPT_STRIKE]);
	ratio17_t multpl = ffff_ratio17((uint32_t)multpl_dbl, 1);
	instr_t undl = find_instr_by_gaid(instrs, undl_id);

	make_oxxxxx_into(in, id, undl, right, exer, strike, multpl);

	/* fiddle with the delivery group ... it's a joke anyway at the mo */
	option_t o = instr_option(in);
	o->ds.issue = 0;
	o->ds.expiry = parse_tstamp(rows[OPT_DELIV]);
	o->ds.settle = o->ds.expiry + 3 * 86400;
	return;
}

static uint16_t
get_mmat(const char *val)
{
	static struct {const char *name; uint16_t val;} lu[] = {
		{"ON", PFMM_ON},
		{"SW", PFMM_SW},
		{"2W", PFMM_2W},
		{"3W", PFMM_3W},
		{"1M", PFMM_1M},
		{"2M", PFMM_2M},
		{"3M", PFMM_3M},
		{"4M", PFMM_4M},
		{"5M", PFMM_5M},
		{"6M", PFMM_6M},
		{"7M", PFMM_7M},
		{"8M", PFMM_8M},
		{"9M", PFMM_9M},
		{"10M", PFMM_10M},
		{"11M", PFMM_11M},
		{"1Y", PFMM_1Y}
	};

	for (size_t i = 0; i < countof(lu); i++) {
		if (strcasecmp(lu[i].name, val) == 0) {
			return lu[i].val;
		}
	}
	return PFMM_UNK;
}

static uint16_t
get_annu(const char *val)
{
	static struct {const char *name; uint16_t val;} lu[] = {
		{"act/360", PFAN_ACT_360},
		{"act/365", PFAN_ACT_365},
		{"act/act", PFAN_ACT_ACT},
	};

	for (size_t i = 0; i < countof(lu); i++) {
		if (strcasecmp(lu[i].name, val) == 0) {
			return lu[i].val;
		}
	}
	return PFAN_UNK;
}

static void
make_interest_rate(instr_t in, uint32_t id, void **rows, size_t UNUSED(nflds))
{
	mmaturity_t mmat = (mmaturity_t)get_mmat(rows[FRA_MMAT]);
	annuity_t annu = (annuity_t)get_annu(rows[FRA_ANNU]);
	uint16_t ccy = strtoul(rows[FRA_CCY], NULL, 10);

	make_trxxxx_into(in, id, rows[IDENT_GA], mmat, annu, ccy);
	UD_DEBUG_XDR("made irate %s (%u)\n", instr_name(in), id);
	return;
}

static void
iqry_rowf(void **row, size_t nflds, void *UNUSED(clo))
{
	uint32_t id = strtoul(row[GAID], NULL, 10);
	uint16_t ty = strtoul(row[INSTTY], NULL, 10);
	struct instr_s in;

	switch (ty) {
	case 1:
		/* currency */
		make_currency(&in, id, row, nflds);
		break;
	case 2:
		/* index */
		make_index(&in, id, row, nflds);
		break;
	case 3:
		/* options */
		make_option(&in, id, row, nflds);
		break;
	case 4:
		/* interest rate */
		make_interest_rate(&in, id, row, nflds);
		break;
	default:
		/* unknown */
		abort();
	}

	/* add i to the catalogue */
	cat_bang_instr(instrs, &in);
	return;
}


void
fetch_instr_mysql(void)
{
/* make me thread-safe and declare me */
	if (conn == NULL) {
		return;
	}

	UD_DEBUG("leeching instruments ...");
	uddb_qry(conn, iqry, sizeof(iqry)-1, iqry_rowf, NULL);
	UD_DBGCONT("done\n");

	UD_DEBUG("kthxbye ...");
	uddb_disconnect(conn);
	conn = NULL;
	UD_DBGCONT("done\n");
	return;
}


/* initialiser code */
void
dso_xdr_instr_mysql_LTX_init(void *clo)
{
	void *spec = udctx_get_setting(clo);

	UD_DEBUG("mod/xdr-instr-mysql: loading ...");
	/* create the catalogue */
	if (instrs == NULL) {
		UD_DBGCONT("failed (no catalogue found)\n");
		return;
	}
	UD_DBGCONT("done\n");

	UD_DEBUG("connecting to database ...");
	if ((conn = uddb_connect(clo, spec)) == NULL) {
		UD_DBGCONT("failed\n");
		return;
	}
	UD_DBGCONT("done\n");
	return;
}

void
dso_xdr_instr_mysql_LTX_deinit(void *UNUSED(clo))
{
	if (conn != NULL) {
		uddb_disconnect(conn);
	}
	return;
}

/* dso-xdr-instr-mysql.c */
