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

#include <pfack/instruments.h>
#include "catalogue.h"
#include "xdr-instr-private.h"

#if defined HAVE_MYSQL || 1
# include <mysql/mysql.h>
#endif	/* HAVE_MYSQL */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

static void *conn;
static const char host[] = "cobain";
static const char user[] = "GAT_user";
static const char pass[] = "EFGau5A4A5BLGAme";
static const char sche[] = "freundt";


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
make_currency(instr_t in, uint32_t id, void **rows, size_t nflds)
{
	uint16_t co = strtoul(rows[CCY_CODE], NULL, 10);
	make_tcnxxx_into(in, id, co);
	UD_DEBUG("made currency %s (%u)\n", instr_name(in), id);
	return;
}

static void
make_index(instr_t in, uint32_t id, void **rows, size_t nflds)
{
	make_tixxxx_into(in, id, rows[IDENT_GA]);
	UD_DEBUG("made index %s (%u)\n", instr_name(in), id);
	return;
}

static void
make_option(instr_t in, uint32_t id, void **rows, size_t nflds)
{
	uint32_t undl_id = strtoul(rows[OPT_UNDL], NULL, 10);
	const char *cfi = rows[OPT_CFI];
	char right = cfi[1];
	char exer = cfi[2];
	double multpl_dbl = strtod(rows[OPT_MULTPL], NULL);
	monetary32_t strike = ffff_monetary32_get_s(rows[OPT_STRIKE]);
	ratio17_t multpl = ffff_ratio17((uint32_t)multpl_dbl, 1);
	instr_t undl = find_instr_by_gaid(instrs, undl_id);

	make_oxxxxx_into(in, id, undl, right, exer, strike, multpl);
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
make_interest_rate(instr_t in, uint32_t id, void **rows, size_t nflds)
{
	uint16_t mmat = get_mmat(rows[FRA_MMAT]);
	uint16_t annu = get_annu(rows[FRA_ANNU]);
	uint16_t ccy = strtoul(rows[FRA_CCY], NULL, 10);

	make_trxxxx_into(in, id, rows[IDENT_GA], mmat, annu, ccy);
	UD_DEBUG("made irate %s (%u)\n", instr_name(in), id);
	return;
}

static void
iqry_rowf(void **row, size_t nflds)
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

static void
fetch_instrs(void)
{
	void *res;

	/* off we go */
	if (mysql_real_query(conn, iqry, sizeof(iqry)-1) != 0) {
		/* dont know */
		return;
	}
	/* otherwise fetch the result */
	if ((res = mysql_store_result(conn)) == NULL) {
		/* bummer */
		return;
	}
	/* process him */
	{
		size_t nflds = mysql_num_fields(res);
		MYSQL_ROW r;

		while ((r = mysql_fetch_row(res))) {
			iqry_rowf((void**)r, nflds);
		}
	}

	/* and free the result object */
	mysql_free_result(res);
	return;
}


/* db connectors */
static void*
db_connect(void)
{
	MYSQL *res;
	res = mysql_init(NULL);
	if (!mysql_real_connect(res, host, user, pass, sche, 0, NULL, 0)) {
		mysql_close(res);
		return conn = NULL;
	}
	return conn = res;
}

static void
db_disconnect(void)
{
	(void)mysql_close(conn);
	conn = NULL;
	return;
}


/* initialiser code */
void
dso_xdr_instr_mysql_LTX_init(void *UNUSED(clo))
{
	UD_DEBUG("mod/xdr-instr-mysql: loading ...");
	/* create the catalogue */
	if (instrs == NULL) {
		UD_DBGCONT("failed (no catalogue found)\n");
		return;
	}
	UD_DBGCONT("done\n");

	UD_DEBUG("connecting to database ...");
	if (db_connect() == NULL) {
		UD_DBGCONT("failed\n");
		return;
	}
	UD_DBGCONT("done\n");

	UD_DEBUG("leeching instruments ...");
	fetch_instrs();
	UD_DBGCONT("done\n");

	UD_DEBUG("kthxbye ...");
	db_disconnect();
	UD_DBGCONT("done\n");
	return;
}

/* dso-xdr-instr-mysql.c */
