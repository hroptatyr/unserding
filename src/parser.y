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
%parse-param{ud_packet_t *pkt}

%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "config.h"
#include "unserding.h"
#include "protocore.h"

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
extern int cli_yyparse(void *scanner, ud_packet_t *pkt);

#define YYSTYPE			const char*
#define YYENABLE_NLS		0
#define YYLTYPE_IS_TRIVIAL	1


/* these delcarations are provided to suppress compiler warnings */
extern int cli_yylex();
extern int cli_yyget_lineno();
extern char *cli_yyget_text();
extern void cli_yyerror(void *scanner, ud_packet_t *pkt, char const *errmsg);

static const char help_rpl[] =
	"hy     send keep-alive message\n"
	"sup    list connected clients and neighbour servers\n"
	"help   this help screen\n"
	"cheers [time]  store last result for TIME seconds (default 86400)\n"
	"nvm    immediately flush last result\n"
	"ls     list catalogue entries of current directory\n"
	"spot <options> obtain spot price\n"
	;

void
cli_yyerror(void *scanner, ud_packet_t *pkt, char const *s)
{
	puts("syntax error");
	pkt->plen = 0;
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
hy_cmd {
	udpc_make_pkt(*pkt, 0, 0, UDPC_PKT_HY);
	pkt->plen = 8;
	YYACCEPT;
} |
cheers_cmd {
	pkt->plen = 0;
	YYACCEPT;
} |
nvm_cmd {
	pkt->plen = 0;
	YYACCEPT;
} |
wtf_cmd {
	puts(help_rpl);
	pkt->plen = 0;
	YYACCEPT;
} |
cya_cmd {
	udpc_make_pkt(*pkt, 0, 0, 0x7e54);
	pkt->plen = 8;
	YYACCEPT;
} |
sup_cmd {
	pkt->plen = 0;
	YYACCEPT;
} |
ls_cmd {
	pkt->plen = 0;
	YYACCEPT;
};

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
TOK_LS | TOK_LS keyvals;

keyvals:
keyval | keyvals keyval;

keyval:
key val;

key:
TOK_KEY;

val:
TOK_VAL;

%%

/* parser.y ends here */
