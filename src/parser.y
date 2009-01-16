/*** cli.y -- unserding CLI parser  -*- C -*-
 *
 * Copyright (C) 2002-2008 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@math.tu-berlin.de>
 *
 * This file is part of QaoS.
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

%defines
%output="y.tab.c"
%pure-parser
%name-prefix="cli_yy"
%lex-param{void *scanner}
%parse-param{void *scanner}
%parse-param{qaos_query_ctx_t ctx}

%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "config.h"
#include "unserding.h"

typedef struct qaos_query_ctx_s *qaos_query_ctx_t;
struct qaos_query_ctx_s {
        char *statements;
        size_t statements_size;
        const char *cur_field;
        const char *cur_op;
        char cur_statement[256];
        size_t cur_stmt_size;
        bool absp;
};

#if defined HAVE_BDWGC
# define xmalloc        GC_MALLOC
# define xmalloc_atomic GC_MALLOC_ATOMIC
# define xrealloc       GC_REALLOC
#else  /* !BDWGC */
# define xmalloc        malloc
# define xmalloc_atomic malloc
# define xrealloc       realloc
#endif  /* BDWGC */

/* declarations */
extern int cli_yyparse(void *scanner, qaos_query_ctx_t ctx);

#define YYSTYPE		const char*

#define STMT_AND	"AND"
#define STMT_AND_SIZE	sizeof(STMT_AND)-1

#define YYENABLE_NLS		0
#define YYLTYPE_IS_TRIVIAL	1


/* these delcarations are provided to suppress compiler warnings */
extern int cli_yylex();
extern int cli_yyget_lineno();
extern char *cli_yyget_text();
extern void
cli_yyerror(void *scanner, qaos_query_ctx_t ctx, char const *s);

void
cli_yyerror(void *scanner, qaos_query_ctx_t ctx, char const *s)
{
	fprintf(stderr, "error in line %d: %s\n",
		cli_yyget_lineno(scanner), s);
	return;
}

static inline void
__concat(qaos_query_ctx_t ctx)
{
	strncpy(&ctx->statements[ctx->statements_size],
		ctx->cur_statement, ctx->cur_stmt_size);
	ctx->statements_size += ctx->cur_stmt_size;
	return;
}

%}

%token <ival>
	TOK_HY
	TOK_CHEERS
	TOK_NVM
	TOK_WTF
	TOK_CYA
	TOK_SUP
	TOK_LS
	TOK_INTEGER
	TOK_KEY
	TOK_VAL
	TOK_SPACE

%%


query:
hy_cmd | cheers_cmd | nvm_cmd | wtf_cmd | cya_cmd | sup_cmd | ls_cmd;

hy_cmd:
TOK_HY;

cheers_cmd:
TOK_CHEERS;

nvm_cmd:
TOK_NVM;

wtf_cmd:
TOK_WTF;

cya_cmd:
TOK_CYA;

sup_cmd:
TOK_SUP;

ls_cmd:
TOK_LS | TOK_LS TOK_SPACE keyvals;

keyvals:
keyval | keyvals TOK_SPACE keyval;

keyval:
key TOK_SPACE val;

key:
TOK_KEY;

val:
TOK_VAL;

%%

/* parser.y ends here */
