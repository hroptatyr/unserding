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

static void *conn;
static const char host[] = "cobain";
static const char user[] = "GAT_user";
static const char pass[] = "EFGau5A4A5BLGAme";
static const char sche[] = "freundt";

static const char iqry[] =
	"SELECT "
	"`uiu`.`ga_instr_id`, "
	"`uiu`.`ga_instr_type_id`, "
	"`uiu`.`name`, "
	"`uiu`.`cfi`, "
	"`uiu`.`spec_urn`, "
	/* currency fields */
	"`uiu`.`ccy_iso4217`, "
	"`uiu`.`ccy_iso4217_code`, "
	/* option fields */
	"`uiu`.`opt_underlying`, "
	"`uiu`.`opt_strike`, "
	"`uiu`.`opt_delivery`, "
	"`uiu`.`opt_cont_multiplier`, "
	"`uiu`.`opt_cfi`, "
	"`uiu`.`opt_default_ccy`, "
	"`uiu`.`opt_opol`, "
	/* interest rate fields */
	"`uiu`.`fra_maturity`, "
	"`uiu`.`fra_dcc`, "
	/* aliases */
	"`uiu`.`description`, "
	"`uiu`.`gaid`, "
	"`uiu`.`ric`, "
	"`uiu`.`bbg`, "
	"`uiu`.`isin` "
	"FROM `freundt`.`ud_instr_unrolled` AS uiu";


static void
iqry_rowf(void **row, size_t nflds)
{
	UD_DEBUG(" instr %lu ", (long unsigned int)atoi(row[0]));
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
