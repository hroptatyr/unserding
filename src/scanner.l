/*** cli.l -- unserding CLI scanner  -*- C -*-
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

%{
#define YY_EXTRA_TYPE	void*
%}

%option reentrant
%option noyywrap
%option warn
%option yylineno
%option nounput
%option noinput
%option bison-bridge
%option header-file="lex.yy.h"
%option outfile="lex.yy.c"
%option pointer
%option nodebug

%{

#include <stdlib.h>
#include <string.h>

struct cli_yystype_s {
	long int slen;
	const char *sval;
};
#define YYSTYPE			struct cli_yystype_s

#include "unsercli-parser.h"

extern int cli_yyget_column(yyscan_t);
extern void cli_yyset_column(int, yyscan_t);

#define RETURN_TOKEN(_x)			\
	do {					\
		fprintf(stderr, #_x "\n");	\
		return _x;			\
	} while (0)

%}

ws		[ \t\f\n\r]+
geq		(>=)|(=>)
leq		(<=)|(=<)
neq		(<>)|(!=)
less		<
grtr		>
eq		[:=]?|is
neg		!

key		:[a-z]+
val		[A-Za-z0-9,.-]+

int		[-+]?[0-9]+

abs		\|

%x BINOP
%x RVAL

%%

hy		{ RETURN_TOKEN(TOK_HY); }
oi		{ RETURN_TOKEN(TOK_HY); }
hey		{ RETURN_TOKEN(TOK_HY); }
lo		{ RETURN_TOKEN(TOK_HY); }
ls		{ RETURN_TOKEN(TOK_LS); }
cheers		{ RETURN_TOKEN(TOK_CHEERS); }
ta		{ RETURN_TOKEN(TOK_CHEERS); }
thx		{ RETURN_TOKEN(TOK_CHEERS); }
help		{ RETURN_TOKEN(TOK_WTF); }
wtf		{ RETURN_TOKEN(TOK_WTF); }
cya		{ RETURN_TOKEN(TOK_CYA); }
bye		{ RETURN_TOKEN(TOK_CYA); }
kthx		{ RETURN_TOKEN(TOK_CYA); }
sup		{ RETURN_TOKEN(TOK_SUP); }
alright		{ RETURN_TOKEN(TOK_SUP); }
nvm		{ RETURN_TOKEN(TOK_NVM); }
bugger		{ RETURN_TOKEN(TOK_NVM); }
impo(rt)?	{ RETURN_TOKEN(TOK_IMPORT); }
cat		{ RETURN_TOKEN(TOK_CAT); }
e123ify		{ RETURN_TOKEN(TOK_E123); }

 /* don't do that at the moment, we can parse the string based on the context
  * ourselves, and much more efficiently */
 /*{int}		{ RETURN_TOKEN(TOK_INTEGER); }*/
{key}		{
	yylval->slen = cli_yyget_leng(yyscanner);
	yylval->sval = cli_yyget_text(yyscanner);
	RETURN_TOKEN(TOK_KEY);
}
{val}		{
	yylval->slen = cli_yyget_leng(yyscanner);
	yylval->sval = cli_yyget_text(yyscanner);
	RETURN_TOKEN(TOK_VAL);
}

 /* more ignored shit, whitespace et al */
{ws}		{ /* ignore */ }

%%

/* cli.l ends here */
