/*** mod-interests.c -- unserding module to obtain interest rates
 *
 * Copyright (C) 2008 Sebastian Freundt
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "pfack-sql.h"
#include "catalogue.h"

#if defined HAVE_FFFF_FFFF_H
/* to get a default set of aliases */
# define USE_MONETARY64
# include <ffff/ffff.h>
#else  /* !FFFF */
/* posix */
# include <math.h>
#endif	/* FFFF */

typedef long int timestamptz_t;
extern void init_interests(void);

static void *loc_conn;
static struct connector_s c;

#define PFACK_DEBUG		UD_DEBUG
#define PFACK_CRITICAL		UD_CRITICAL

/** struct for end of day data */
struct eod_ser_s {
	monetary32_t pri;
};

static monetary32_t *intr_FFD;
static monetary32_t *intr_EONIA;
static ud_catobj_t c_ffd, c_eonia;

static inline timestamptz_t __attribute__((always_inline))
YYYY_MM_DD_to_ts(char *restrict buf)
{
	struct tm tm;
	/* now for real, parse the expiry date string */
	memset(&tm, 0, sizeof(tm));
	(void)strptime(buf, "%Y-%m-%d", &tm);
	return (timestamptz_t)mktime(&tm);
}

static char iir_qry[] =
	"SELECT "
	REL_FLD("iir", "date1") ", "
	REL_FLD("iir", "close") " "
	"FROM " "GAT_static.gfd_data" " AS " QUO_FLD("iir") " "
	"WHERE "
	REL_FLD("iir", "id") " = " /* overwrite me */ "00000";

static void
store_iir(void **cols, size_t ncols, void *closure)
{
	short int idx = YYYY_MM_DD_to_ts((char*)cols[0]) % 86400;
	monetary32_t *arr = closure;
	arr[idx] = ffff_monetary32_get_s((char*)cols[1]);
	return;
}

static void
obtain_iir_FFD(void)
{
/* USA Fed Funds Official */
	void *res;
	size_t len = countof_m1(iir_qry);

	/* build the query */
	iir_qry[len-4] = '3';
	iir_qry[len-3] = '3';
	iir_qry[len-2] = '1';
	iir_qry[len-1] = '5';

	PFACK_DEBUG("querying\n%s\n", iir_qry);
	/* off we go */
	if (UNLIKELY((res = pfack_sql_query(loc_conn, iir_qry, len)) == NULL)) {
		/* no statements? */
		PFACK_CRITICAL("No interest rates available\n");
		return;
	} else if (UNLIKELY(pfack_sql_nrows(res) == 0UL)) {
		/* no statements? */
		PFACK_CRITICAL("No interest rates available\n");
		pfack_sql_free_query(res);
		return;
	}
	/* otherwise process 'em */
	intr_FFD = malloc(16384 * sizeof(*intr_FFD));
	/* copy the result */
	pfack_sql_rows(res, store_iir, &intr_FFD);
	pfack_sql_free_query(res);
}

static void
obtain_iir_EONIA(void)
{
/* EONIA */
	void *res;
	size_t len = countof_m1(iir_qry);

	/* build the query */
	iir_qry[len-4] = '5';
	iir_qry[len-3] = '6';
	iir_qry[len-2] = '4';
	iir_qry[len-1] = '5';

	PFACK_DEBUG("querying\n%s\n", iir_qry);
	/* off we go */
	if (UNLIKELY((res = pfack_sql_query(loc_conn, iir_qry, len)) == NULL)) {
		/* no statements? */
		PFACK_CRITICAL("No interest rates available\n");
		return;
	} else if (UNLIKELY(pfack_sql_nrows(res) == 0UL)) {
		/* no statements? */
		PFACK_CRITICAL("No interest rates available\n");
		pfack_sql_free_query(res);
		return;
	}
	/* otherwise process 'em */
	intr_EONIA = malloc(16384 * sizeof(*intr_EONIA));
	/* copy the data */
	pfack_sql_rows(res, store_iir, &intr_EONIA);
	pfack_sql_free_query(res);
	return;
}

void
init_interests(void)
{
	/* where do we get that fucking connector info from? */
	c.proto = PP_MYSQL;
	c.hop = PH_PFACK;
	c.sql.host = "cobain";
	c.sql.user = "GAT_user";
	c.sql.password = "EFGau5A4A5BLGAme";
	c.sql.database = "GAT_static";
	c.sql.schema = "GAT_static";
	c.sql.table = "gfd_data";

	/* we assume that the mysql connector function must work */
	if ((loc_conn = pfack_mysql_connect(
		     c.sql.host, c.sql.user,
		     c.sql.password, c.sql.database)) == NULL) {
		UD_CRITICAL("bingo, connexion failed! "
			    "I'll have a big sleep in now\n");
		return;
	}

	obtain_iir_FFD();
	obtain_iir_EONIA();

#if 0
	/* escrow the fuckers */
	c_ffd = ud_cat_add_child(intrs, "FFD", UD_CF_SPOTTABLE);
	c_eonia = ud_cat_add_child(intrs, "EONIA", UD_CF_SPOTTABLE);
#else
	c_ffd = ud_make_catobj(
		MAKE_CLASS("instrument"),
		MAKE_CLASS("interest"),
		MAKE_NAME("FFD"));

	c_eonia = ud_make_catobj(
		MAKE_CLASS("instrument"),
		MAKE_CLASS("interest"),
		MAKE_NAME("EONIA"));
#endif
	ud_cat_add_obj(c_ffd);
	ud_cat_add_obj(c_eonia);
	return;
}

/* mod-interests.c ends here */
