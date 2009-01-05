/*** pfack-mysql.c -- mysql api bindings
 *
 * Copyright (C) 2008 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <hroptatyr@gna.org>
 *
 * This file is part of libpfack.
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

#include <stdlib.h>
#include <stdio.h>
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */

#include "pfack-sql.h"
#if defined HAVE_MYSQL && defined WITH_MYSQL
#include <mysql/mysql.h>
#endif

#define PFACK_MYSQL_ERROR(args...)	UD_CRITICAL("[mysql] " args)


dbconn_t
pfack_mysql_connect(const char *host, const char *user,
		    const char *pw, const char *db)
{
	MYSQL *conn;
	conn = mysql_init(NULL);
	if (!mysql_real_connect(conn, host, user, pw, db, 0, NULL, 0)) {
		PFACK_MYSQL_ERROR("failed to connect\n");
		mysql_close(conn);
		return NULL;
	}
	return pfack_sql_set_mysql(conn);
}

void
pfack_mysql_close(dbconn_t conn)
{
	conn = pfack_sql_get_obj(conn);
	(void)mysql_close(conn);
	return;
}

void
pfack_mysql_changedb(dbconn_t conn, const char *db)
{
	conn = pfack_sql_get_obj(conn);
	if (!mysql_select_db(conn, db)) {
		PFACK_MYSQL_ERROR("failed to change database\n");
	}
	return;
}

dbqry_t
pfack_mysql_query(dbconn_t conn, const char *query, size_t length)
{
	MYSQL_RES *res;

	conn = pfack_sql_get_obj(conn);
	if (UNLIKELY(length == 0)) {
		length = strlen(query);
	}
	if (UNLIKELY(mysql_real_query(conn, query, length) != 0)) {
		return NULL;
	}
	if ((res = mysql_store_result(conn)) == NULL) {
		/* coulda been an INSERTion */
		;
	}
	return pfack_sql_set_mysql(res);
}

dbqry_t
pfack_mysql_query_volatile(dbconn_t conn, const char *query, size_t length)
{
	MYSQL_RES *res;

	conn = pfack_sql_get_obj(conn);
	if (UNLIKELY(length == 0)) {
		length = strlen(query);
	}
	if (UNLIKELY(mysql_real_query(conn, query, length) != 0)) {
		return NULL;
	}
	if ((res = mysql_use_result(conn)) == NULL) {
		/* coulda been an INSERTion */
		;
	}
	return pfack_sql_set_mysql(res);
}

void
pfack_mysql_free_query(dbqry_t qry)
{
	qry = pfack_sql_get_obj(qry);
	mysql_free_result(qry);
	return;
}

dbobj_t
pfack_mysql_fetch(dbqry_t qry, index_t row, index_t col)
{
	MYSQL_ROW r;
	qry = pfack_sql_get_obj(qry);
	mysql_data_seek(qry, row);
	r = mysql_fetch_row(qry);
	return r[col];
}

void
pfack_mysql_rows(dbqry_t qry, dbrow_f rowf, void *clo)
{
	MYSQL_ROW r;
	size_t num_fields;

	qry = pfack_sql_get_obj(qry);
	num_fields = mysql_num_fields(qry);
	while ((r = mysql_fetch_row(qry))) {
		rowf((void**)r, num_fields, clo);
	}
	return;
}

void
pfack_mysql_rows_max(dbqry_t qry, dbrow_f rowf, void *clo, size_t max_rows)
{
/* like pfack_mysql_rows but processes at most MAX_ROWS */
	MYSQL_ROW r;
	size_t num_fields;

	qry = pfack_sql_get_obj(qry);
	num_fields = mysql_num_fields(qry);
	while (max_rows-- > 0UL && (r = mysql_fetch_row(qry))) {
		rowf((void**)r, num_fields, clo);
	}
	return;
}

size_t
pfack_mysql_nrows(dbqry_t qry)
{
	return mysql_num_rows(pfack_sql_get_obj(qry));
}

/* pfack-mysql.c ends here */
