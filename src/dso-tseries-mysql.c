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

#include <pfack/instruments.h>
/* our master include */
#include "unserding.h"
#include "module.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-ctx.h"
#include "ud-time.h"

#include "urn.h"
#include "tscache.h"
#include "tscoll.h"
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
#define DEFINE_GORY_STUFF
#include <sushi/m30.h>

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */
#if !defined xmalloc
# define xmalloc	malloc
#endif	/* !xmalloc */

#if defined DEBUG_FLAG
# define UD_DEBUG_SQL(args...)			\
	fprintf(logout, "[unserding/tseries] " args)
#endif	/* DEBUG_FLAG */

/* mysql conn, kept open */
static void *conn = NULL;

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

static const char*
dupfld(const char *r)
{
	size_t len = strlen(r);
	char *res = xmalloc(len + 2 + 1);
	res[0] = '`';
	res[len + 1] = '`';
	memcpy(res + 1, r, len);
	res[len + 2] = '\0';
	return res;
}

static m30_t
get_m30(const char *s)
{
	if (UNLIKELY(s == NULL)) {
		return ffff_m30_get_ui32(0);
	}
	return ffff_m30_get_s(&s);
}


struct qrclo_s {
	uint32_t cnt;
	uint32_t max;
	sl1t_t box;
};

static void
qry_rowf(void **row, size_t nflds, void *clo)
{
	struct qrclo_s *qrclo = clo;
	time_t ts = parse_time(row[0]);
	uint32_t cnt;

	if (UNLIKELY(qrclo->cnt >= qrclo->max)) {
		/* stop caching */
		return;
	}
	cnt = qrclo->cnt++;
	/* brilliantly hard-coded bollocks */
	if (nflds == 2 || nflds == 3) {
		m30_t p = get_m30(row[1]);
		m30_t q = nflds == 3 ? get_m30(row[2]) : ffff_m30_get_ui32(0);
		scom_thdr_t th = (void*)(qrclo->box + cnt);

		UD_DEBUG("putting %s %2.4f into slot %u\n",
			 (char*)row[0], ffff_m30_d(p), cnt);
		/* bang bang */
		scom_thdr_set_sec(th, ts);
		scom_thdr_set_msec(th, 0);
		scom_thdr_set_ttf(th, SL1T_TTF_FIX);
		scom_thdr_set_tblidx(th, 0);
		qrclo->box[cnt].v[0] = ffff_m30_ui32(p);
		qrclo->box[cnt].v[1] = ffff_m30_ui32(q);

	} else {
		abort();
#if 0
		/* full BATOMCFX candles */
		struct ohlcv_p_s cdl;

		/* bid */
		cdl.o = get_m32(row[1]);
		cdl.h = get_m32(row[2]);
		cdl.l = get_m32(row[3]);
		cdl.c = get_m32(row[4]);
		UD_DEBUG("putting %s B-OHLC candle into slot %d\n",
			 (char*)row[0], iip);
		ute_bang_ohlcv_p(&pkt->t[iip], PFTT_BID, ds, &cdl);

		/* ask */
		cdl.o = get_m32(row[6]);
		cdl.h = get_m32(row[7]);
		cdl.l = get_m32(row[8]);
		cdl.c = get_m32(row[9]);
		UD_DEBUG("putting %s A-OHLC candle into slot %d\n",
			 (char*)row[0], iip);
		ute_bang_ohlcv_p(&pkt->t[iip], PFTT_ASK, ds, &cdl);

		/* tra/spot */
		cdl.o = get_m32(row[11]);
		cdl.h = get_m32(row[12]);
		cdl.l = get_m32(row[13]);
		cdl.c = get_m32(row[14]);
		UD_DEBUG("putting %s T-OHLC candle into slot %d\n",
			 (char*)row[0], iip);
		ute_bang_ohlcv_p(&pkt->t[iip], PFTT_TRA, ds, &cdl);
#endif
	}
	return;
}

static inline size_t
print_qry(char *tgt, size_t len, tsc_key_t k, urn_t urn, time32_t b, time32_t e)
{
	char begs[16], ends[16];

	print_ds_into(begs, sizeof(begs), b);
	print_ds_into(ends, sizeof(ends), e);

#if defined __INTEL_COMPILER
# pragma warning	(disable:981)
#endif	/* __INTEL_COMPILER */
	switch (urn_type(urn)) {
	case URN_OAD_C:
	case URN_OAD_OHLC:
	case URN_OAD_OHLCV:
		len = snprintf(
			tgt, len,
			"SELECT "
			"%s AS `stamp`, "
			"%s AS `c`, %s AS `v` "
			"FROM %s "
			"WHERE %s = %u AND %s BETWEEN '%s' AND '%s' "
			"ORDER BY 1",
			urn_fld_date(urn),
			urn_fld_tcp(urn),
			urn_fld_tv(urn) ? urn_fld_tv(urn) : "0",
			urn_fld_dbtbl(urn),
			urn_fld_id(urn),
			su_secu_quodi(k->secu),
			urn_fld_date(urn), begs, ends);
		break;
	case URN_UTE_CDL:
#if 0
		len = snprintf(
			tgt, len,
			"SELECT %s AS `stamp`, "
			"%s `bo`, %s `bh`, %s `bl`, %s `bc`, %s `bv`, "
			"%s `ao`, %s `ah`, %s `al`, %s `ac`, %s `av`, "
			"%s `to`, %s `th`, %s `tl`, %s `tc`, %s `tv`  "
			"FROM %s "
			"WHERE %s = %u AND %s BETWEEN '%s' AND '%s' "
			"ORDER BY 1",
			urn_fld_date(tser->urn),

			urn_fld_bop(tser->urn),
			urn_fld_bhp(tser->urn),
			urn_fld_blp(tser->urn),
			urn_fld_bcp(tser->urn),
			urn_fld_bv(tser->urn) ? urn_fld_tv(tser->urn) : "0",

			urn_fld_aop(tser->urn),
			urn_fld_ahp(tser->urn),
			urn_fld_alp(tser->urn),
			urn_fld_acp(tser->urn),
			urn_fld_av(tser->urn) ? urn_fld_tv(tser->urn) : "0",

			urn_fld_top(tser->urn),
			urn_fld_thp(tser->urn),
			urn_fld_tlp(tser->urn),
			urn_fld_tcp(tser->urn),
			urn_fld_tv(tser->urn) ? urn_fld_tv(tser->urn) : "0",

			urn_fld_dbtbl(tser->urn),
			urn_fld_id(tser->urn),
			su_secu_quodi(tser->secu),
			urn_fld_date(tser->urn), begs, ends);
#else
		len = 0;
#endif
		break;

	case URN_L1_TICK:
	case URN_L1_BAT:
	case URN_L1_PEG:
	case URN_L1_BATPEG:
	default:
		len = 0;
	}
#if defined __INTEL_COMPILER
# pragma warning	(default:981)
#endif	/* __INTEL_COMPILER */
	return len;
}

/* tick fetching revised */
static size_t
fetch_tick(
	sl1t_t tgt, size_t tsz, tsc_key_t k, void *uval,
	time32_t beg, time32_t end)
{
	char qry[480];
	size_t len;
	size_t nres;
	struct qrclo_s qrclo[1];

	len = print_qry(qry, sizeof(qry), k, uval, beg, end);
	UD_DEBUG_SQL("querying: %s\n", qry);
	qrclo->box = tgt;
	qrclo->max = tsz;
	qrclo->cnt = 0;
	nres = uddb_qry(conn, qry, len, qry_rowf, qrclo);
	UD_DEBUG("got %u/%zu prices\n", qrclo->cnt, nres);
	return qrclo->cnt;
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

#define MAKE_PRED(_x, args...)						\
	static char *__##_x##_pred[] = {args};				\
	static inline bool						\
	fld_##_x##_p(const char *r)					\
	{								\
		for (unsigned int i = 0; i < countof(__##_x##_pred); i++) { \
			if (strcmp(r, __##_x##_pred[i]) == 0) {		\
				return true;				\
			}						\
		}							\
		return false;						\
	}

#if defined __INTEL_COMPILER
# pragma warning	(disable:424)
#endif
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
MAKE_PRED(to, "top", "o", "open");
MAKE_PRED(th, "thp", "h", "high", "hi");
MAKE_PRED(tl, "tlp", "l", "low", "lo");
MAKE_PRED(tc, "tcp", "c", "close");
MAKE_PRED(tv, "tv", "v", "volume", "volu", "vol");
MAKE_PRED(f, "f", "fix");
MAKE_PRED(x, "x", "set");

#define fld_open_p	fld_to_p
#define fld_high_p	fld_th_p
#define fld_low_p	fld_tl_p
#define fld_close_p	fld_tc_p
#define fld_volume_p	fld_tv_p

static urn_t
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
fill_batfx_ohlcv(urn_t urn, const char *r)
{
#define APPEND(_urn, _x, _r)						\
	if (fld_##_x##_p(_r)) {						\
		/*UD_DBGCONT("=> " #_x "\n");*/				\
		_urn->flds.batfx_ohlcv.fld_##_x = dupfld(_r);		\
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
	//UD_DBGCONT("unknown column: \"%s\"\n", r);
#undef APPEND
	return;
}

static void
urnqry_rowf(void **row, size_t UNUSED(nflds), void *clo)
{
	urn_t urn = clo;

	//UD_DEBUG("column %s %s ... ", urn->dbtbl, (char*)row[0]);
	if (urn->flds.unk.fld_id == NULL && fld_id_p(row[0])) {
		//UD_DBGCONT("=> id\n");
		urn->flds.unk.fld_id = dupfld(row[0]);
	} else if (urn->flds.unk.fld_date == NULL && fld_date_p(row[0])) {
		//UD_DBGCONT("=> stamp\n");
		urn->flds.unk.fld_date = dupfld(row[0]);
	} else {
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
#define QUODI_ID	0
	"`uiu`.`ga_instr_id`, "
#define QUOTI_ID	1
	"`uiu`.`quoti_id`, "
#define POT_ID		2
	"`uiu`.`pot_id`, "
#define URN_ID		3
	"`uiu`.`ga_urn_id`, "
#define URN		4
	"`uiu`.`urn`, "
#define MIN_DT		5
	"`uiu`.`min_dt`, "
#define MAX_DT		6
	"`uiu`.`max_dt`, "
#define TYPES_BS	7
	"`uiu`.`types_bitset` "
	"FROM `freundt`.`ga_instr_urns` AS `uiu`";

/* type_bs stuff */
/* if it is a once-a-day tick */
#define TBS_OAD		0x01
/* if TBS_OAD is set, this indicates if there are 5 days in a week */
#define TBS_5DW		0x02

/* forward decl */
static void fetch_urn_mysql(job_t UNUSED(j));

static struct tsc_ops_s mysql_ops[1] = {{
		.fetch_cb = fetch_tick,
		.urn_refetch_cb = NULL,
	}};

static void
ovqry_rowf(void **row, size_t UNUSED(nflds), void *UNUSED(clo))
{
	tscube_t c = gcube;
	time32_t beg = parse_time(row[MIN_DT]);
	time32_t end = UNLIKELY(row[MAX_DT] == NULL)
		? 0x7fffffff
		: parse_time(row[MAX_DT]);
	uint32_t qd = strtoul(row[QUODI_ID], NULL, 10);
	int32_t qt = strtol(row[QUOTI_ID], NULL, 10);
	uint16_t p = strtoul(row[POT_ID], NULL, 10);
	/* urn voodoo */
	uint32_t urn_id = strtoul(row[URN_ID], NULL, 10);
	urn_t urn = find_urn((urn_type_t)urn_id, row[URN]);
	/* the final cube entry */
	struct tsc_ce_s ce = {
		.key = {{
				.beg = beg,
				.end = end,
				.ttf = SL1T_TTF_FIX,
				.msk = 1 | 2 | 4 | 8 | 16,
				.secu = su_secu(qd, qt, p),
			}},
		.ops = mysql_ops,
		.uval = urn,
	};
	/* add to the cube */
	tsc_add(c, &ce);
	return;
}

static void
fetch_urn_mysql(job_t UNUSED(j))
{
/* make me thread-safe and declare me */
	if (conn == NULL) {
		return;
	}

	UD_DEBUG("leeching overview ...");
	uddb_qry(conn, ovqry, sizeof(ovqry)-1, ovqry_rowf, NULL);
	UD_DBGCONT("done\n");

	UD_DEBUG("inspecting URNs ...");
	fill_urns();
	UD_DBGCONT("done\n");
	return;
}


static void*
cfgspec_get_source(void *ctx, void *spec)
{
#define CFG_SOURCE	"source"
	return udcfg_tbl_lookup(ctx, spec, CFG_SOURCE);
}


typedef enum {
	CST_UNK,
	CST_MYSQL,
} cfgsrc_type_t;

static cfgsrc_type_t
cfgsrc_type(void *ctx, void *spec)
{
#define CFG_TYPE	"type"
	const char *type = NULL;

	if (spec == NULL) {
		UD_DEBUG("mod/tseries: no source specified\n");
		return CST_UNK;
	}
	udcfg_tbl_lookup_s(&type, ctx, spec, CFG_TYPE);

	if (type == NULL) {
		return CST_UNK;
	} else if (memcmp(type, "mysql", 5) == 0) {
		return CST_MYSQL;
	}
	return CST_UNK;
}

static void
load_ticks_fetcher(void *clo, void *spec)
{
	void *src;

	if ((src = cfgspec_get_source(clo, spec)) == NULL) {
		return;
	}

	/* pass along the src settings */
	udctx_set_setting(clo, src);

	/* find out about its type */
	switch (cfgsrc_type(clo, src)) {
	case CST_MYSQL:
		if ((conn = uddb_connect(clo, src)) == NULL) {
			UD_DBGCONT("failed\n");
		}

	case CST_UNK:
	default:
		/* do fuckall */
		break;
	}

	/* clean up */
	udctx_set_setting(clo, NULL);
	udcfg_tbl_free(clo, src);
	return;
}


/* initialiser code */
void
dso_tseries_mysql_LTX_init(void *clo)
{
	ud_ctx_t ctx = clo;
	void *settings;

	UD_DEBUG("mod/tseries-mysql: connecting ...");
	if ((settings = udctx_get_setting(ctx)) != NULL) {
		load_ticks_fetcher(clo, settings);
		/* be so kind as to unref the settings */
		udcfg_tbl_free(ctx, settings);
	}
	/* clean up */
	udctx_set_setting(ctx, NULL);
	UD_DBGCONT("done\n");
	/* announce our hook fun */
	add_hook(fetch_urn_hook, fetch_urn_mysql);
	return;
}

void
dso_tseries_mysql_LTX_deinit(void *UNUSED(clo))
{
	if (conn != NULL) {
		uddb_disconnect(conn);
	}
	return;
}

/* dso-tseries-mysql.c */
