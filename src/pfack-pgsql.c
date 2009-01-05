/*** pfack-pgsql.c -- postgres api bindings
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
#include "common.h"

#include "pfack-sql.h"
#if defined HAVE_PGSQL && defined WITH_PGSQL
# include <libpq-fe.h>
#endif


dbconn_t
pfack_pgsql_connect(const char *host, const char *user,
		    const char *pw, const char *db)
{
	PGconn *conn;
	if ((conn =
	     PQsetdbLogin(host, NULL, NULL, NULL, db, user, pw)) == NULL) {
		PFACK_ERROR("failed to connect\n");
	}
	if (UNLIKELY(PQstatus(conn) != CONNECTION_OK)) {
		PFACK_ERROR("failed to connect:\n[pgsql] %s",
			    PQerrorMessage(conn));
		PQfinish(conn);
		conn = NULL;
	}
	return pfack_sql_set_pgsql(conn);
}

void
pfack_pgsql_close(dbconn_t conn)
{
	void *tmp = pfack_sql_get_obj(conn);
	if (LIKELY(tmp != NULL)) {
		PQfinish(tmp);
	}
	return;
}

void
pfack_pgsql_changedb(dbconn_t conn, const char *db)
{
	conn = pfack_sql_get_obj(conn);
	if (conn == NULL ||
	    (conn =
	     PQsetdbLogin(
		     PQhost(conn), NULL, NULL, NULL,
		     db,
		     PQuser(conn),
		     PQpass(conn))) == NULL) {
		PFACK_ERROR("failed to change db\n");
	}
	conn = pfack_sql_set_pgsql(conn);
	return;
}

dbqry_t
pfack_pgsql_query(dbconn_t conn, const char *query, size_t length)
{
	void *res;
	conn = pfack_sql_get_obj(conn);
	res = PQexec(conn, query);
	if (UNLIKELY(PQresultStatus(res) != PGRES_COMMAND_OK)) {
		PFACK_ERROR("query failed:\n[pgsql] %s",
			    PQerrorMessage(conn));
		PQclear(res);
		return pfack_sql_set_pgsql(NULL);
	}
	return pfack_sql_set_pgsql(res);
}

void
pfack_pgsql_free_query(dbqry_t qry)
{
	PQclear(pfack_sql_get_obj(qry));
	return;
}

dbobj_t
pfack_pgsql_fetch(dbqry_t qry, index_t row, index_t col)
{
	return PQgetvalue(pfack_sql_get_obj(qry), row, col);
}


size_t
pfack_pgsql_nrows(dbqry_t qry)
{
	return PQntuples(pfack_sql_get_obj(qry));
}

/* to loop over a result set */
void
pfack_pgsql_rows(dbqry_t qry, dbrow_f rowf, void *clo)
{
	size_t num_fields = PQnfields(pfack_sql_get_obj(qry));
	size_t num_rows = PQntuples(pfack_sql_get_obj(qry));
	/* C99 we need you here */
	void *r[num_fields];

	/* hope he orders this differently */
	qry = pfack_sql_get_obj(qry);
	for (index_t i = 0; i < num_rows; i++) {
		for (index_t j = 0; j < num_fields; j++) {
			r[j] = PQgetvalue(qry, i, j);
		}
		rowf((void**)r, num_fields, clo);
	}
	return;
}

void
pfack_pgsql_rows_max(dbqry_t qry, dbrow_f rowf, void *clo, size_t max_rows)
{
	size_t num_fields = PQnfields(pfack_sql_get_obj(qry));
	size_t num_rows = PQntuples(pfack_sql_get_obj(qry));
	size_t nrows = num_rows < max_rows ? num_rows : max_rows;
	/* C99 we need you here */
	void *r[num_fields];

	PFACK_DEBUG("about to fetch min{%lu, %lu} = %lu rows\n",
		    num_rows, max_rows, nrows);
	/* hope he orders this differently */
	qry = pfack_sql_get_obj(qry);
	for (index_t i = 0; i < nrows; i++) {
		for (index_t j = 0; j < num_fields; j++) {
			r[j] = PQgetvalue(qry, i, j);
		}
		rowf((void**)r, num_fields, clo);
	}
	return;
}

void
pfack_pgsql_rows_FETCH(dbconn_t c, const char *cursor_name,
		       dbrow_f rowf, void *clo, size_t num_rows)
{
	char qry[64];
	size_t num_fields;
	void *res;

	/* unwrap the connexion object */
	if (UNLIKELY(PQstatus(c = pfack_sql_get_obj(c)) != CONNECTION_OK)) {
		return;
	}

	(void)snprintf(
		qry, countof(qry)-1,
		"FETCH %lu FROM %s",
		/* snprintf() args */
		num_rows, cursor_name);

	PFACK_DEBUG("doing a fetch using connexion %p\n%s\n", c, qry);
	/* kick of the query */
	res = PQexec(c, qry);
	if (UNLIKELY(PQresultStatus(res) != PGRES_TUPLES_OK)) {
		PFACK_ERROR("problem fetching:\n[pgsql] %s",
			    PQerrorMessage(c));
		PQclear(res);
		return;
	}
	/* fetch the number of rows and fields */
	num_fields = PQnfields(res);
	num_rows = PQntuples(res);

	PFACK_DEBUG("about to fetch %lu rows\n", num_rows);

	/* traverse the result set */
	for (index_t i = 0; i < num_rows; i++) {
		/* C99 we need you here */
		void *r[num_fields];

		for (index_t j = 0; j < num_fields; j++) {
			r[j] = PQgetvalue(res, i, j);
		}
		rowf((void**)r, num_fields, clo);
	}

	/* clean up the result handle */
	PQclear(res);
	return;
}

/* pfack-pgsql.c ends here */
