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

#if defined HAVE_MYSQL
# if defined HAVE_MYSQL_MYSQL_H
#  include <mysql/mysql.h>
# elif defined HAVE_MYSQL_H
#  include <mysql.h>
# endif
#endif	/* HAVE_MYSQL */

#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*x))
#endif	/* !countof */

/* mysql conn, kept open */
static void *conn;


static void*
db_connect(void *spec)
{
/* we assume that SPEC is a config_setting_t pointing to database mumbojumbo */
	MYSQL *res;
	const char *host = NULL;
	const char *user = NULL;
	const char *pass = NULL;
	const char *sche = NULL;
	const char dflt_sche[] = "freundt";

#if defined USE_LIBCONFIG
	/* try and read the stuff from the config file */
	config_setting_lookup_string(spec, "host", &host);
	config_setting_lookup_string(spec, "user", &user);
	config_setting_lookup_string(spec, "pass", &pass);
	config_setting_lookup_string(spec, "schema", &sche);
#endif	/* USE_LIBCONFIG */

	if (host == NULL || user == NULL || pass == NULL) {
		return conn = NULL;
	} else if (sche == NULL) {
		/* just assume the schema exists as we know it */
		sche = dflt_sche;
	}

	res = mysql_init(NULL);
	if (!mysql_real_connect(res, host, user, pass, sche, 0, NULL, 0)) {
		mysql_close(res);
		return conn = NULL;
	}
	return conn = res;
}

static void __attribute__((unused))
db_disconnect(void)
{
	(void)mysql_close(conn);
	conn = NULL;
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
	"`uiu`.`max_dt` "
	"FROM `freundt`.`ga_instr_urns` AS `uiu`";

static void
ovqry_rowf(void **row, size_t nflds)
{
	uint32_t urn_id = strtoul(row[URN_ID], NULL, 10);

	switch (urn_id) {
	case 1 ... 3:
		UD_DEBUG("Once-A-Day tick for %s\n", (char*)row[INSTR_ID]);
	default:
		break;
	}
	return;
}

static void
fetch_tick_overview(void)
{
	void *res;

	/* off we go */
	if (mysql_real_query(conn, ovqry, sizeof(ovqry)-1) != 0) {
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
			ovqry_rowf((void**)r, nflds);
		}
	}

	/* and free the result object */
	mysql_free_result(res);
	return;
}


/* initialiser code */
void
dso_tseries_mysql_LTX_init(void *clo)
{
	struct {ud_ctx_t ctx; void *spec;} *tmp = clo;

	UD_DEBUG("mod/tseries-mysql: connecting ...");
	if (db_connect(tmp->spec) == NULL) {
		UD_DBGCONT("failed\n");
		return;
	}
	UD_DBGCONT("done\n");

	UD_DEBUG("leeching overview ...");
	fetch_tick_overview();
	UD_DBGCONT("done\n");
	return;
}

/* dso-tseries-mysql.c */
