/*** mysql-helpers.h -- some static routines used frequently in the DSOs
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

#if !defined INCLUDED_mysql_helpers_h_
#define INCLUDED_mysql_helpers_h_

/* this defines serious stuff, don't include if you don't want it. */
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#if defined HAVE_MYSQL
# if defined HAVE_MYSQL_MYSQL_H
#  include <mysql/mysql.h>
# elif defined HAVE_MYSQL_H
#  include <mysql.h>
# endif
#endif	/* HAVE_MYSQL */
/* for the config stuff and the context */
#include "unserding-ctx.h"

typedef void *ud_conn_t;
typedef void(*ud_row_f)(void**, size_t, void *clo);

static ud_conn_t __attribute__((unused))
uddb_connect(ud_ctx_t ctx, ud_cfgset_t spec)
{
/* we assume that SPEC is a config_setting_t pointing to database mumbojumbo */
	MYSQL *res;
	const char *host = NULL;
	const char *user = NULL;
	const char *pass = NULL;
	const char *sche = NULL;
	/* fucking amazing huh? */
	const char dflt_sche[] = "freundt";

	udcfg_tbl_lookup_s(&host, ctx, spec, "host");
	udcfg_tbl_lookup_s(&user, ctx, spec, "user");
	udcfg_tbl_lookup_s(&pass, ctx, spec, "pass");
	udcfg_tbl_lookup_s(&sche, ctx, spec, "schema");

	if (host == NULL || user == NULL || pass == NULL) {
		return NULL;
	} else if (sche == NULL) {
		/* just assume the schema exists as we know it */
		sche = dflt_sche;
	}

	res = mysql_init(NULL);
	if (!mysql_real_connect(res, host, user, pass, sche, 0, NULL, 0)) {
		mysql_close(res);
		return NULL;
	}
	return res;
}

static void __attribute__((unused))
uddb_disconnect(ud_conn_t conn)
{
	(void)mysql_close(conn);
	return;
}

static size_t __attribute__((unused))
uddb_qry(ud_conn_t conn, const char *qry, size_t len, ud_row_f cb, void *clo)
{
	void *res;
	size_t nres = 0;

	/* off we go */
	if (mysql_real_query(conn, qry, len) != 0) {
		/* dont know */
		return 0;
	}
	/* otherwise fetch the result */
	if ((res = mysql_store_result(conn)) == NULL) {
		/* bummer */
		return 0;
	}
	/* process him */
	{
		size_t nflds = mysql_num_fields(res);
		MYSQL_ROW r;

		while ((r = mysql_fetch_row(res))) {
			(*cb)((void**)r, nflds, clo);
			nres++;
		}
	}

	/* and free the result object */
	mysql_free_result(res);
	return nres;
}

#endif	/* INCLUDED_mysql_helpers_h_ */
