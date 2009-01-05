/*** pfack-sql.h -- generic SQL bindings
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

#if !defined INCLUDED_pfack_sql_h_
#define INCLUDED_pfack_sql_h_

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdbool.h>
#include <stddef.h>
#include "unserding.h"
#include "unserding-private.h"

#if defined HAVE_MYSQL && defined WITH_MYSQL
# if !defined USE_MYSQL
#  define USE_MYSQL	1
# endif	 /* !USE_MYSQL */
#elif defined HAVE_PGSQL && defined WITH_PGSQL
# if !defined USE_PGSQL
#  define USE_PGSQL	1
# endif	 /* !USE_PGSQL */
#else
# error need at least one sql implementation
#endif	/*  */


/* connectors */
typedef enum pfack_proto_e pfack_proto_t;
typedef enum pfack_hop_e pfack_hop_t;
typedef struct connector_s *connector_t;

struct sql_connector_s {
	const char *host;
	const char *user;
	const char *password;
	short int port;
	const char *database;
	const char *schema;
	const char *table;
};

struct file_connector_s {
	const char *file;
	const char *schema;
};

enum pfack_proto_e {
	PP_UNKNOWN,
	PP_MYSQL,
	PP_PGSQL,
	PP_FILE,
};

enum pfack_hop_e {
	PH_UNKNOWN,
	PH_BINFIX,
	PH_PFACK,
};

/* is this something global? */
struct connector_s {
	pfack_proto_t proto;
#if 0
	/* a connector function */
	void*(*connf)(connector_t);
	/* a disconnector function */
	void(*discf)(connector_t, void*);
#endif
	/* la dee da */
	union {
		struct sql_connector_s sql;
		struct file_connector_s file;
	};
	pfack_hop_t hop;
	void *params;
};


typedef void *dbobj_t;
typedef void *dbconn_t;
typedef void *dbqry_t;
typedef void (*dbrow_f)(dbobj_t *row, size_t num_fields, void *closure);

extern inline dbconn_t __attribute__((always_inline, gnu_inline))
/* we cant do this one */
pfack_sql_connect(pfack_proto_t p, const char *host, const char *user,
		  const char *passwd, const char *dbname);
extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_close(dbconn_t conn);

extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_pgsql_p(const dbobj_t obj);
extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_mysql_p(const dbobj_t obj);
/** \private */
extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_get_obj(const dbobj_t obj);
/** \private */
extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_set_mysql(dbobj_t obj);
/** \private */
extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_set_pgsql(dbobj_t obj);

extern inline dbqry_t __attribute__((always_inline, gnu_inline))
pfack_sql_query(dbconn_t conn, const char *query, size_t length);
extern inline dbqry_t __attribute__((always_inline, gnu_inline))
pfack_sql_query_volatile(dbconn_t conn, const char *query, size_t length);
extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_free_query(dbqry_t qry);
extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_fetch(dbqry_t qry, index_t row, index_t col);
extern inline long int __attribute__((always_inline, gnu_inline))
pfack_sql_fetch_int(void *qry, index_t row, index_t col);
extern inline size_t __attribute__((always_inline, gnu_inline))
pfack_sql_nrows(dbqry_t qry);
/* to loop over a result set */
extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_rows(dbqry_t qry, dbrow_f rowf, void *clo);
extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_rows_max(dbqry_t qry, dbrow_f rowf, void *clo, size_t max_rows);
extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_rows_FETCH(dbconn_t c, const char *cursor_name,
		     dbrow_f rowf, void *clo, size_t nrows);


/* loadsa defines */
#if defined WITH_PGSQL && defined HAVE_PGSQL
# include "pfack-pgsql.h"
#endif	/* {WITH,HAVE}_PGSQL */

#if defined WITH_MYSQL && defined HAVE_MYSQL
# include "pfack-mysql.h"
#endif	/* {WITH,HAVE}_MYSQL */

#define REL_FLD(_tbl, _nam)	_tbl "." QUO_FLD(_nam)
#define REL_FLD_AS(_t, _n, _a)	_t "." QUO_FLD(_n) " AS " QUO_FLD(_a)

#define rela_deposit_state	SCHM_TBL("pfack","deposit_state")
#define rela_account		SCHM_TBL("pfack","account")
#define rela_depXac		SCHM_TBL("pfack","deposit_state_X_account")
#define view_last_depXac	SCHM_TBL("pfack","last_deposit_state_X_account")

#if defined USE_SQL_FUNS && USE_SQL_FUNS
# define PFACK_AC_SYM		"pfack_ac_sym"
# define PFACK_AC_ID		"pfack_ac_id"
# define PFACK_MAKE_STATE	"pfack_make_state"
#endif	/* USE_SQL_FUNS */

#define PFACK_SQL_BEGIN		"START TRANSACTION;"
#define PFACK_SQL_COMMIT	"COMMIT;"


/* connection fiddling */
extern inline dbconn_t __attribute__((always_inline, gnu_inline))
pfack_sql_connect(pfack_proto_t p, const char *host, const char *user,
		  const char *pw, const char *db)
{
	switch (p) {
	case PP_MYSQL:
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
		return pfack_mysql_connect(host, user, pw, db);
#endif
		break;

	case PP_PGSQL:
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
		return pfack_pgsql_connect(host, user, pw, db);
#endif
		break;
	case PP_UNKNOWN:
	case PP_FILE:
	default:
		break;
	}
	return NULL;
}

extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_close(dbconn_t conn)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(conn))) {
		pfack_mysql_close((conn));
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(conn))) {
		pfack_pgsql_close((conn));
#endif
	}
	return;
}

extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_pgsql_p(dbobj_t obj)
{
	return ((long unsigned int)obj & 1UL) == 0;
}

extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_mysql_p(dbobj_t obj)
{
	return ((long unsigned int)obj & 1UL) == 1;
}

/* not for public consumption */
extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_get_obj(dbobj_t obj)
{
	return (void*)((long unsigned int)obj & -2UL);
}

extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_set_pgsql(dbobj_t obj)
{
	return obj;
}

extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_set_mysql(dbobj_t obj)
{
	return (void*)((long unsigned int)obj | 1);
}


/* queries and shite */
extern inline dbqry_t __attribute__((always_inline, gnu_inline))
pfack_sql_query(dbconn_t conn, const char *query, size_t qlen)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(conn))) {
		return pfack_mysql_query((conn), query, qlen);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(conn))) {
		return pfack_pgsql_query((conn), query, qlen);
#endif
	}
	return NULL;
}

extern inline dbqry_t __attribute__((always_inline, gnu_inline))
pfack_sql_query_volatile(dbconn_t conn, const char *query, size_t qlen)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(conn))) {
		return pfack_mysql_query_volatile((conn), query, qlen);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(conn))) {
		return pfack_pgsql_query((conn), query, qlen);
#endif
	}
	return NULL;
}

extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_free_query(dbqry_t qry)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(qry))) {
		pfack_mysql_free_query((qry));
		return;
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(qry))) {
		pfack_pgsql_free_query((qry));
		return;
#endif
	}
	return;
}

extern inline dbobj_t __attribute__((always_inline, gnu_inline))
pfack_sql_fetch(dbqry_t qry, index_t row, index_t col)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(qry))) {
		return pfack_mysql_fetch((qry), row, col);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(qry))) {
		return pfack_pgsql_fetch((qry), row, col);
#endif
	}
	return NULL;
}

extern inline long int __attribute__((always_inline, gnu_inline))
pfack_sql_fetch_int(void *qry, index_t row, index_t col)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(qry))) {
		return pfack_mysql_fetch_int((qry), row, col);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(qry))) {
		return pfack_pgsql_fetch_int((qry), row, col);
#endif
	}
	return -1;
}

extern inline size_t __attribute__((always_inline, gnu_inline))
pfack_sql_nrows(dbqry_t qry)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(qry))) {
		return pfack_mysql_nrows((qry));
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(qry))) {
		return pfack_pgsql_nrows((qry));
#endif
	}
	return 0;
}

extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_rows(dbqry_t qry, dbrow_f rowf, void *clo)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(qry))) {
		return pfack_mysql_rows((qry), rowf, clo);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(qry))) {
		return pfack_pgsql_rows((qry), rowf, clo);
#endif
	}
	return;
}

extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_rows_max(dbqry_t qry, dbrow_f rowf, void *clo, size_t max_rows)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL
	} else if (LIKELY(pfack_sql_mysql_p(qry))) {
		return pfack_mysql_rows_max((qry), rowf, clo, max_rows);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(qry))) {
		return pfack_pgsql_rows_max((qry), rowf, clo, max_rows);
#endif
	}
	return;
}

extern inline void __attribute__((always_inline, gnu_inline))
pfack_sql_rows_FETCH(dbconn_t c, const char *cursor_name,
		     dbrow_f rowf, void *clo, size_t nrows)
{
	if (false) {
#if defined WITH_MYSQL && defined HAVE_MYSQL && defined USE_MYSQL && 0
	} else if (LIKELY(pfack_sql_mysql_p(c))) {
		return pfack_mysql_rows_FETCH(c, rowf, clo, nrows);
#endif
#if defined WITH_PGSQL && defined HAVE_PGSQL && defined USE_PGSQL
	} else if (LIKELY(pfack_sql_pgsql_p(c))) {
		return pfack_pgsql_rows_FETCH(c, cursor_name, rowf, clo, nrows);
#endif
	}
	return;
}


/* specific SQL commands */
extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_cmd_BEGIN(dbconn_t c);
extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_cmd_COMMIT(dbconn_t c);

extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_cmd_BEGIN(dbconn_t c)
{
	return true;
}

extern inline bool __attribute__((always_inline, gnu_inline))
pfack_sql_cmd_COMMIT(dbconn_t c)
{
	return true;
}

#endif	/* INCLUDED_pfack_sql_h_ */
