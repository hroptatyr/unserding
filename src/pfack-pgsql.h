/*** pfack-pgsql.h -- pgsql api bindings
 *
 * Copyright (C) 2008 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <hroptatyr@gna.org>
 *
 * This file is part of pfack.
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

#if !defined INCLUDED_pfack_pgsql_h_
#define INCLUDED_pfack_pgsql_h_

#if defined USE_PGSQL
# define QUO_FLD(_fld)		"\"" _fld "\""
# define SQL_UNIX2TS(_ts)	"TIMESTAMP 'epoch' + " _ts " * INTERVAL '1 s'"
# define SQL_TS2UNIX(_ts)	"TIMESTAMP 'epoch' + " _ts " * INTERVAL '1 s'"
# define SCHM_TBL(_schem, _tbl)	_schema "." _tbl
#endif	/* USE_PGSQL */

extern void pfack_init_pgsql(void);
extern void pfack_deinit_pgsql(void);

extern dbconn_t
pfack_pgsql_connect(const char *host, const char *user,
		    const char *passwd, const char *dbname);
extern void
pfack_pgsql_close(dbconn_t conn);
extern void
pfack_pgsql_changedb(dbconn_t conn, const char *db);
extern dbqry_t
pfack_pgsql_query(dbconn_t conn, const char *query, size_t length);
extern void
pfack_pgsql_free_query(void *qry);
extern void*
pfack_pgsql_fetch(void *qry, index_t row, index_t col);
extern inline long int
pfack_pgsql_fetch_int(void *qry, index_t row, index_t col);

/* to loop over a result set */
extern void
pfack_pgsql_rows(dbqry_t qry, dbrow_f rowf, void *clo);
extern void
pfack_pgsql_rows_max(dbqry_t qry, dbrow_f rowf, void *clo, size_t max_rows);
extern size_t
pfack_pgsql_nrows(void *qry);
extern void
pfack_pgsql_rows_FETCH(dbconn_t c, const char *cursor_name,
		       dbrow_f rowf, void *clo, size_t nrows);


extern inline long int
pfack_pgsql_fetch_int(void *qry, index_t row, index_t col)
{
	void *r = pfack_pgsql_fetch(qry, row, col);
	return strtol((char*)r, NULL, 0);
}

#endif	/* INCLUDED_pfack_pgsql_h_ */
