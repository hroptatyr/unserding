/*** dso-tseries-mysql.c -- ticks from mysql databases
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

#include "xdr-instr-private.h"
#include "xdr-instr-seria.h"
#include "urn.h"
#include "tscache.h"
#include "tscoll.h"
#include "tseries.h"
#include "tseries-private.h"

#if defined HAVE_MYSQL
# if defined HAVE_MYSQL_MYSQL_H
#  include <mysql/mysql.h>
# elif defined HAVE_MYSQL_H
#  include <mysql.h>
# endif
#endif	/* HAVE_MYSQL */
/* some common routines */
#include "mysql-helpers.h"
#include "tseries.h"

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

#if defined DEBUG_FLAG
# define UD_DEBUG_SQL(args...)			\
	fprintf(logout, "[unserding/tseries] " args)
#endif	/* DEBUG_FLAG */

/* mysql conn, kept open */
static void *conn;

struct tser_pkt_idx_s {
	uint32_t i;
	tser_pkt_t pkt;
};

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


static void
qry_rowf(void **row, size_t nflds, void *clo)
{
	dse16_t ds = time_to_dse(parse_time(row[0]));
	tser_pkt_t pkt = clo;
	uint8_t iip = index_in_pkt(ds);
	m32_t p;

	if (UNLIKELY(row[1] == NULL)) {
		/* do not cahe NULL prices */
		return;
	}
	p = ffff_monetary32_get_s(row[1]);
	if (UNLIKELY(iip >= countof(pkt->t))) {
		/* do not cache weekend `prices' */
		return;
	}
	UD_DEBUG("putting %s %2.4f into slot %d\n",
		 (char*)row[0], ffff_monetary32_d(p), iip);
	pkt->t[iip].t.c.p = p;
	return;
}

size_t
fetch_ticks_intv_mysql(tser_pkt_t pkt, tseries_t tser, dse16_t beg, dse16_t end)
{
/* assumes eod ticks for now,
 * i wonder if it's wise to have all the intelligence in here
 * to go through various different tsa's as they are now chained
 * together */
	char begs[16], ends[16];
	char qry[224];
	size_t len;
	size_t nres;

	memset(pkt, 0, sizeof(*pkt));
	print_ds_into(begs, sizeof(begs), dse_to_time(beg));
	print_ds_into(ends, sizeof(ends), dse_to_time(end));
	len = snprintf(
		qry, sizeof(qry),
		"SELECT `%s`, `%s` "
		"FROM %s "
		"WHERE `%s` = %d AND `%s` BETWEEN '%s' AND '%s' "
		"ORDER BY 1",
		urn_fld_date(tser->urn), urn_fld_close(tser->urn),
		urn_fld_dbtbl(tser->urn),
		urn_fld_id(tser->urn), tser->secu->instr,
		urn_fld_date(tser->urn), begs, ends);
	UD_DEBUG_SQL("querying: %s\n", qry);
	nres = uddb_qry(conn, qry, len, qry_rowf, pkt);
	UD_DEBUG("got %lu prices\n", (long unsigned int)nres);
	return nres;
}


/* urn bollocks */
static struct urn_s urns[64];
static size_t nurns = 0;

static bool
fld_id_p(const char *row_name)
{
	return strcmp(row_name, "contractId") == 0 ||
		strcmp(row_name, "ga_instr_id") == 0;
}

static bool
fld_date_p(const char *row_name)
{
	return strcmp(row_name, "date") == 0 ||
		strcmp(row_name, "stamp") == 0;
}

#define MAKE_PRED(_x, _f1)			\
	static inline bool			\
	fld_##_x##_p(const char *r)		\
	{					\
		return strcmp(r, #_f1) == 0;	\
	}
#define MAKE_PRED2(_x, _f1, _f2)		\
	static inline bool			\
	fld_##_x##_p(const char *r)		\
	{					\
		return strcmp(r, #_f1) == 0 ||	\
			strcmp(r, #_f2) == 0;	\
	}

MAKE_PRED(bo, "bop");
MAKE_PRED(bh, "bhp");
MAKE_PRED(bl, "blp");
MAKE_PRED(bc, "bcp");
MAKE_PRED(bv, "bv");
MAKE_PRED(ao, "aop");
MAKE_PRED(ah, "ahp");
MAKE_PRED(al, "alp");
MAKE_PRED(ac, "acp");
MAKE_PRED(av, "av");
MAKE_PRED2(to, "top", "o");
MAKE_PRED2(th, "thp", "h");
MAKE_PRED2(tl, "tlp", "l");
MAKE_PRED2(tc, "tcp", "c");
MAKE_PRED2(tv, "tv", "v");
MAKE_PRED2(f, "f", "fix");
MAKE_PRED2(x, "x", "set");

#define fld_open_p	fld_to_p
#define fld_high_p	fld_th_p
#define fld_low_p	fld_tl_p
#define fld_close_p	fld_tc_p
#define fld_volume_p	fld_tv_p

static const_urn_t
find_urn(urn_type_t type, const char *urn)
{
	index_t idx;
	for (index_t i = 0; i < nurns; i++) {
		if (strcmp(urns[i].dbtbl, urn) == 0) {
			return &urns[i];
		}
	}
	idx = nurns++;
	memset(&urns[idx], 0, sizeof(urns[idx]));
	urns[idx].type = type;
	urns[idx].dbtbl = strdup(urn);
	return &urns[idx];
}

static void
fill_f_c(urn_t urn, const char *r)
{
	if (urn->flds.oad_c.fld_close == NULL && fld_close_p(r)) {
		urn->flds.oad_c.fld_close = strdup(r);
	}
	return;
}

static void
fill_f_ohlcv(urn_t urn, const char *r)
{
	if (urn->flds.oad_ohlcv.fld_open == NULL && fld_open_p(r)) {
		urn->flds.oad_ohlcv.fld_open = strdup(r);

	} else if (urn->flds.oad_ohlcv.fld_high == NULL && fld_high_p(r)) {
		urn->flds.oad_ohlcv.fld_high = strdup(r);

	} else if (urn->flds.oad_ohlcv.fld_low == NULL && fld_low_p(r)) {
		urn->flds.oad_ohlcv.fld_low = strdup(r);

	} else if (urn->flds.oad_ohlcv.fld_close == NULL && fld_close_p(r)) {
		urn->flds.oad_ohlcv.fld_close = strdup(r);

	} else if (urn->flds.oad_ohlcv.fld_volume == NULL && fld_volume_p(r)) {
		urn->flds.oad_ohlcv.fld_volume = strdup(r);
	}
	return;
}

static void
fill_batfx_ohlcv(urn_t urn, const char *r)
{
#define APPEND(_urn, _x, _r)						\
	if (_urn->flds.batfx_ohlcv.fld_##_x == NULL && fld_##_x##_p(_r)) { \
		_urn->flds.batfx_ohlcv.fld_##_x = strdup(_r);		\
		return;							\
	}

	APPEND(urn, bo, r);
	APPEND(urn, bh, r);
	APPEND(urn, bl, r);
	APPEND(urn, bc, r);
	APPEND(urn, bv, r);
	APPEND(urn, ao, r);
	APPEND(urn, ah, r);
	APPEND(urn, al, r);
	APPEND(urn, ac, r);
	APPEND(urn, av, r);
	APPEND(urn, to, r);
	APPEND(urn, th, r);
	APPEND(urn, tl, r);
	APPEND(urn, tc, r);
	APPEND(urn, tv, r);
	APPEND(urn, f, r);
	APPEND(urn, x, r);

#undef APPEND
	return;
}

static void
urnqry_rowf(void **row, size_t nflds, void *clo)
{
	urn_t urn = clo;

	UD_DEBUG("column %s %s\n", urn->dbtbl, (char*)row[0]);
	if (urn->flds.unk.fld_id == NULL && fld_id_p(row[0])) {
		urn->flds.unk.fld_id = strdup(row[0]);
	} else if (urn->flds.unk.fld_date == NULL && fld_date_p(row[0])) {
		urn->flds.unk.fld_date = strdup(row[0]);
	} else if (urn->type == URN_OAD_C) {
		fill_f_c(urn, row[0]);
	} else if (urn->type == URN_OAD_OHLC ||
		   urn->type == URN_OAD_OHLCV) {
		fill_f_ohlcv(urn, row[0]);
	} else if (urn->type == URN_UTE_CDL) {
		fill_batfx_ohlcv(urn, row[0]);
	}
	return;
}

static void
fill_urn(urn_t urn)
{
	char qry[224];
	size_t len;

	len = snprintf(qry, sizeof(qry), "SHOW COLUMNS FROM %s", urn->dbtbl);
	uddb_qry(conn, qry, len, urnqry_rowf, urn);
	return;
}

static void
fill_urns(void)
{
/* fetch field info */
	for (index_t i = 0; i < nurns; i++) {
		fill_urn(&urns[i]);
	}
	return;
}


/* overview and administrative bullshit */
static const char ovqry[] =
	"SELECT "
#define INSTR_ID	0
	"`uiu`.`ga_instr_id`, "
#define URN_ID		1
	"`uiu`.`ga_urn_id`, "
#define URN		2
	"`uiu`.`urn`, "
#define MIN_DT		3
	"`uiu`.`min_dt`, "
#define MAX_DT		4
	"`uiu`.`max_dt`, "
#define TYPES_BS	5
	"`uiu`.`types_bitset` "
	"FROM `freundt`.`ga_instr_urns` AS `uiu`";

/* type_bs stuff */
/* if it is a once-a-day tick */
#define TBS_OAD		0x01
/* if TBS_OAD is set, this indicates if there are 5 days in a week */
#define TBS_5DW		0x02

static void
ovqry_rowf(void **row, size_t nflds, void *UNUSED(clo))
{
	uint32_t urn_id = strtoul(row[URN_ID], NULL, 10);
	struct secu_s secu;
	tscoll_t tsc;
	struct tseries_s tser;
	uint32_t tbs = strtoul(row[TYPES_BS], NULL, 10);

	secu.instr = strtoul(row[INSTR_ID], NULL, 10);
	secu.unit = 0;
	secu.pot = 0;

	switch (urn_id) {
	case 1 ... 3:
	case 8:
		/* once-a-day, 5-a-week */
		UD_DEBUG("OAD/5DW tick for %u\n", secu.instr);
		tsc = find_tscoll_by_secu(tscache, &secu);

		tser.urn = find_urn(urn_id, row[URN]);
		if (UNLIKELY(row[MIN_DT] == NULL)) {
			/* i'm not entirely sure about the meaning of this */
			break;
		}
		tser.from = parse_time(row[MIN_DT]);
		if (UNLIKELY(row[MAX_DT] == NULL)) {
			/* we agreed on saying that this means up-to-date
			 * so consequently we assign the maximum key */
			tser.to = 0x7fffffff;
		} else {
			tser.to = parse_time(row[MAX_DT]);
		}
		tser.types = tbs;
		/* add to the collection of time stamps */
		tscoll_add(tsc, &tser);

	default:
		break;
	}
	return;
}

/* initialiser code */
void
dso_tseries_mysql_LTX_init(void *clo)
{
	void *spec = udctx_get_setting(clo);

	UD_DEBUG("mod/tseries-mysql: connecting ...");
	if ((conn = uddb_connect(clo, spec)) == NULL) {
		UD_DBGCONT("failed\n");
		return;
	}
	UD_DBGCONT("done\n");

	UD_DEBUG("leeching overview ...");
	uddb_qry(conn, ovqry, sizeof(ovqry)-1, ovqry_rowf, NULL);
	UD_DBGCONT("done\n");

	UD_DEBUG("inspecting URNs ...");
	fill_urns();
	UD_DBGCONT("done\n");
	return;
}

void
dso_tseries_mysql_LTX_deinit(void *clo)
{
	uddb_disconnect(conn);
	return;
}

/* dso-tseries-mysql.c */
