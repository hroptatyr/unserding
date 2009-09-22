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

/* mysql conn, kept open */
static void *conn;

struct tser_pkt_idx_s {
	uint32_t i;
	tser_pkt_t pkt;
};


static void
qry_rowf(void **row, size_t nflds, void *clo)
{
	m32_t p = ffff_monetary32_get_s(row[1]);
	struct tser_pkt_idx_s *tmp = clo;
	tmp->pkt->t[tmp->i++] = p;
	return;
}

size_t
fetch_ticks_intv_mysql(tser_pktbe_t pkt, tick_by_instr_hdr_t hdr)
{
/* assumes eod ticks for now */
	char begs[16], ends[16];
	char qry[224];
	size_t len;
	struct tser_pkt_idx_s pi = {.i = 0, .pkt = &pkt->pkt};
	dse16_t beg = pkt->beg, end = pkt->end;

	print_ds_into(begs, sizeof(begs), dse_to_time(beg));
	print_ds_into(ends, sizeof(ends), dse_to_time(end));
	len = snprintf(
		qry, sizeof(qry),
		"SELECT `date`, `close` "
		"FROM `GAT_static`.`eod_interest_rates2` "
		"WHERE contractId = %d AND `date` BETWEEN '%s' AND '%s' "
		"ORDER BY 1",
		hdr->secu.instr, begs, ends);
	UD_DEBUG("querying: %s\n", qry);
	uddb_qry(conn, qry, len, qry_rowf, &pi);
	UD_DEBUG("got %u prices\n", pi.i);
	return pi.i;
}


/* overview and administrative bullshit */
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

static char *urns[64];
static size_t nurns = 0;

static const char*
find_urn(const char *urn)
{
	for (index_t i = 0; i < nurns; i++) {
		if (strcmp(urns[i], urn) == 0) {
			return urns[i];
		}
	}
	return urns[nurns++] = strdup(urn);
}

static void
ovqry_rowf(void **row, size_t nflds, void *UNUSED(clo))
{
	long unsigned int urn_id = strtoul(row[URN_ID], NULL, 10);
	struct secu_s secu;
	tseries_t tser;
	ts_anno_t tsa;

	secu.instr = strtoul(row[INSTR_ID], NULL, 10);
	secu.unit = 0;
	secu.pot = 0;

	switch (urn_id) {
	case 1 ... 3:
		UD_DEBUG("Once-A-Day tick for %u\n", secu.instr);
		if ((tser = find_tseries_by_secu(tscache, &secu)) == NULL) {
			struct tseries_s tmp = {.size = 0, .conses = NULL};
			tser = tscache_bang_series(tscache, &secu, &tmp);
		}
		tsa = tscache_tseries_annotation(tser);
		tsa->instr = secu.instr;
		tsa->tbl = find_urn(row[URN]);
		tsa->from = parse_time(row[MIN_DT]);
		tsa->to = parse_time(row[MAX_DT]);
		tsa->types = strtoul(row[TYPES_BS], NULL, 10);

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
	return;
}

/* dso-tseries-mysql.c */
