/*** pfack-mysql.h -- mysql api bindings
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

#if !defined INCLUDED_pfack_mysql_h_
#define INCLUDED_pfack_mysql_h_

#if defined USE_MYSQL
# define QUO_FLD(_fld)		"`" _fld "`"
# define SQL_TS2UNIX(_ts)	"unix_timestamp(" _ts ")"
# define SQL_UNIX2TS(_ts)	"from_unixtime(" _ts ")"
# define SCHM_TBL(_schem, _tbl)	_tbl
#endif	/* USE_MYSQL */

extern dbconn_t
pfack_mysql_connect(const char *host, const char *user,
		    const char *passwd, const char *dbname);
extern void
pfack_mysql_close(dbconn_t conn);
extern void
pfack_mysql_changedb(dbconn_t conn, const char *db);

extern void*
pfack_mysql_query(dbconn_t conn, const char *query, size_t length);
extern void*
pfack_mysql_query_volatile(dbconn_t conn, const char *query, size_t length);
extern void
pfack_mysql_free_query(dbqry_t qry);
extern void*
pfack_mysql_fetch(dbqry_t qry, index_t row, index_t col);
extern inline long int __attribute__((always_inline, gnu_inline))
pfack_mysql_fetch_int(void *qry, index_t row, index_t col);
extern size_t
pfack_mysql_nrows(dbqry_t qry);

/* to loop over a result set */
extern void
pfack_mysql_rows(dbqry_t qry, dbrow_f rowf, void *clo);
/* to loop over a result set but only MAX_ROWS times at most */
extern void
pfack_mysql_rows_max(dbqry_t qry, dbrow_f rowf, void *clo, size_t max_rows);


extern inline long int __attribute__((always_inline, gnu_inline))
pfack_mysql_fetch_int(dbqry_t qry, index_t row, index_t col)
{
	void *r = pfack_mysql_fetch(qry, row, col);
	return strtol((char*)r, NULL, 0);
}

#endif	/* INCLUDED_pfack_mysql_h_ */
