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
%parse-param{ud_handle_t hdl}

%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "config.h"
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"
#include "catalogue.h"

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
extern int cli_yyparse(void *scanner, ud_handle_t hdl);

#define YYENABLE_NLS		0
#define YYLTYPE_IS_TRIVIAL	1

struct cli_yystype_s {
	long int slen;
	const char *sval;
};
#define YYSTYPE			struct cli_yystype_s


/* these delcarations are provided to suppress compiler warnings */
extern int cli_yylex();
extern int cli_yyget_lineno();
extern char *cli_yyget_text();
extern void cli_yyerror(void *scanner, ud_handle_t hdl, char const *errmsg);

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
cli_yyerror(void *scanner, ud_handle_t hdl, char const *s)
{
	puts("syntax error");
	return;
}

%}

%expect 0

%token <noval>
	TOK_HY
	TOK_CHEERS
	TOK_NVM
	TOK_WTF
	TOK_CYA
	TOK_SUP
	TOK_LS
	TOK_LC
	TOK_CAT
	TOK_IMPORT
	TOK_INTEGER

%token <sval>
	TOK_KEY
	TOK_VAL

%%


query:
hy_cmd {
	char buf[8];
	ud_packet_t pkt = BUF_PACKET(buf);
	udpc_make_pkt(pkt, hdl->convo++, 0, UDPC_PKT_HY);
	ud_send_raw(hdl, pkt);
	YYACCEPT;
} |
cheers_cmd {
	YYACCEPT;
} |
nvm_cmd {
	YYACCEPT;
} |
wtf_cmd {
	puts(help_rpl);
	YYACCEPT;
} |
cya_cmd {
	char buf[8];
	ud_packet_t pkt = BUF_PACKET(buf);
	udpc_make_pkt(pkt, hdl->convo++, 0, 0x7e54);
	ud_send_raw(hdl, pkt);
	YYACCEPT;
} |
sup_cmd {
	YYACCEPT;
} |
ls_cmd {
	udpc_make_pkt(hdl->pktchn[0], hdl->convo++, 0, UDPC_PKT_LS);
	ud_send_raw(hdl, hdl->pktchn[0]);

	/* the packet we're gonna send in raw shape */
	ud_fprint_pkt_raw(hdl->pktchn[0], stdout);

	YYACCEPT;
} |
lc_cmd {
	udpc_make_pkt(hdl->pktchn[0], hdl->convo++, 0, UDPC_PKT_LC);
	ud_send_raw(hdl, hdl->pktchn[0]);

	/* the packet we're gonna send in raw shape */
	ud_fprint_pkt_raw(hdl->pktchn[0], stdout);

	YYACCEPT;
} |
cat_cmd {
	udpc_make_pkt(hdl->pktchn[0], hdl->convo++, 0, UDPC_PKT_CAT);
	ud_send_raw(hdl, hdl->pktchn[0]);

	/* the packet we're gonna send in raw shape */
	ud_fprint_pkt_raw(hdl->pktchn[0], stdout);

	YYACCEPT;
} |
impo_cmd {
	udpc_make_pkt(hdl->pktchn[0], hdl->convo++, 0, 0x7e10);
	ud_send_raw(hdl, hdl->pktchn[0]);

	/* the packet we're gonna send in raw shape */
	ud_fprint_pkt_raw(hdl->pktchn[0], stdout);

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
TOK_LS |
TOK_LS /* in the middle */ {
	/* init the seqof counter */
	hdl->pktchn[0].pbuf[8] = UDPC_TYPE_SEQOF;
	/* set its initial value to naught */
	hdl->pktchn[0].pbuf[9] = 0;
	/* set the packet idx */
	hdl->pktchn[0].plen = 10;
} keyvals;

lc_cmd:
TOK_LC |
TOK_LC /* in the middle */ {
	/* init the seqof counter */
	hdl->pktchn[0].pbuf[8] = UDPC_TYPE_SEQOF;
	/* set its initial value to naught */
	hdl->pktchn[0].pbuf[9] = 0;
	/* set the packet idx */
	hdl->pktchn[0].plen = 10;
} keyvals;

cat_cmd:
TOK_CAT |
TOK_CAT /* in the middle */ {
	/* init the seqof counter */
	hdl->pktchn[0].pbuf[8] = UDPC_TYPE_SEQOF;
	/* set its initial value to naught */
	hdl->pktchn[0].pbuf[9] = 0;
	/* set the packet idx */
	hdl->pktchn[0].plen = 10;
} keyvals;

impo_cmd:
TOK_IMPORT;

keyvals:
keyval | keyvals keyval;

keyval:
key val;

key:
TOK_KEY {
	ud_tag_t t = ud_tag_from_s(yylval.sval, yylval.slen);
	hdl->pktchn[0].pbuf[hdl->pktchn[0].plen++] = t;
};

val:
TOK_VAL {
	char *restrict pbuf = &hdl->pktchn[0].pbuf[hdl->pktchn[0].plen];
	pbuf[0] = yylval.slen;
	memcpy(&pbuf[1], yylval.sval, yylval.slen);
	hdl->pktchn[0].plen += yylval.slen + 1;
	/* inc the seqof counter */
	hdl->pktchn[0].pbuf[9]++;
};

%%

/* parser.y ends here */
