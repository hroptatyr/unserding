/*** unsercli.y -- unserding CLI parser  -*- C -*-
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
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif

#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"
#include "protocore-private.h"
#include "cli-common.h"
#include "unsercli-scanner.h"

#if 0
#if !defined xmalloc
# define xmalloc        GC_MALLOC
#endif	/* !xmalloc */
#if !defined !xmalloc_atomic
# define xmalloc_atomic GC_MALLOC_ATOMIC
#endif	/* !xmalloc_atomic */
#if !defined !xrealloc
# define xrealloc       GC_REALLOC
#endif	/* !xrealloc */

#else  /* !0 */

#if !defined xmalloc
# define xmalloc        malloc
#endif	/* !xmalloc */
#if !defined xmalloc_atomic
# define xmalloc_atomic malloc
#endif	/* !xmalloc_atomic */
#if !defined xrealloc
# define xrealloc       realloc
#endif	/* !xrealloc */
#endif	/* 0 */

/* declarations */
extern int cli_yyparse(void *scanner, ud_handle_t hdl);


typedef struct ud_worker_s *ud_worker_t;
typedef struct ud_ev_async_s ud_ev_async;


FILE *logout;

static const char help_rpl[] =
	"help   this help screen\n"
	"quit   leave the command line interface\n"
	;

static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_io ALGN16(__srv_watcher);
ev_async *glob_notify;

/* our global handle space, closured because */
static struct ud_handle_s __hdl;
static char __pktbuf[UDPC_SIMPLE_PKTLEN];
static ud_packet_t __pkt = {.plen = countof(__pktbuf), .pbuf = __pktbuf};

/* these delcarations are provided to suppress compiler warnings */
extern int cli_yylex();
extern int cli_yyget_lineno();
extern char *cli_yyget_text();
extern void cli_yyerror(void *scanner, ud_handle_t hdl, char const *errmsg);

/* should that be properly public? */
extern void
stdin_print_async(ud_packet_t pkt, struct sockaddr_in *sa, socklen_t sal);


void
cli_yyerror(void *scanner, ud_handle_t hdl, char const *s)
{
	fputs("syntax error\n", logout);
	return;
}

#define YYENABLE_NLS		0
#define YYLTYPE_IS_TRIVIAL	1
#define YYSTACK_USE_ALLOCA	1


/* command handling */
typedef struct cli_cmd_s *cli_cmd_t;
struct cli_cmd_s {
	char name[16];
	ud_pkt_cmd_t cmd;
	cli_cmd_t next;
};

static cli_cmd_t cmd_list = NULL;

static void
add_cmd(const char *name, size_t nlen, ud_pkt_cmd_t cmd)
{
	cli_cmd_t c = malloc(sizeof(struct cli_cmd_s));

	if (nlen == 0) {
		nlen = strlen(name);
	}
	if (nlen > 15) {
		nlen = 15;
	}
	strncpy(c->name, name, nlen);
	c->name[nlen] = '\0';
	c->cmd = cmd;
	/* cons with global list */
	c->next = cmd_list;
	cmd_list = c;
	return;
}

static inline uint8_t
hexdig_to_nibble(char c)
{
	switch (c) {
	case '0' ... '9':
		return (uint8_t)(c - '0');
	case 'A' ... 'F':
		return (uint8_t)(c - '0' - 7);
	case 'a' ... 'f':
		return (uint8_t)(c - '0' - 39);
	default:
		return (uint8_t)(0xff);
	}
}

static ud_pkt_cmd_t
resolve_tok(const char *tok, size_t len)
{
	ud_pkt_cmd_t res = 0;
	uint8_t tmp;

	if (len == 0) {
		len = strlen(tok);
	}
	if (len > 16) {
		len = 16;
	}
	for (cli_cmd_t c = cmd_list; c; c = c->next) {
		if (strncmp(c->name, tok, len) == 0) {
			return c->cmd;
		}
	}
	if (len != 4) {
		goto out;
	}
	/* try the hex reader */
	if ((tmp = hexdig_to_nibble(tok[0])) < 16) {
		res |= tmp << 12;
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[1])) < 16) {
		res |= tmp << 8;
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[2])) < 16) {
		res |= tmp << 4;
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[3])) < 16) {
		res |= tmp << 0;
	} else {
		goto out;
	}
	return res;
out:
	return 0xffff;
}

%}

%expect 0

%token <noval>
	TOK_WTF
	TOK_CYA
	TOK_ALI

%token <sval>
	TOK_KEY
	TOK_VAL
	TOK_STRING

%%


query:
wtf_cmd {
	puts(help_rpl);
	YYACCEPT;
} |
cya_cmd {
	ud_detach_stdin(EV_DEFAULT);
	ev_unloop(EV_DEFAULT_ EVUNLOOP_ALL);
	YYACCEPT;
} |
gen_cmd keyvals {
	ud_send_raw(hdl, hdl->pktchn[0]);
	YYACCEPT;
} |
ali_cmd;

wtf_cmd:
TOK_WTF;

cya_cmd:
TOK_CYA;

ali_cmd:
TOK_ALI TOK_VAL {
	fputs("unaliasing commands not yet supported\n", logout);
	YYERROR;
} |
TOK_ALI TOK_VAL TOK_VAL {
	ud_pkt_cmd_t cmd = resolve_tok($<sval>3, $<slen>3);

	add_cmd($<sval>2, $<slen>2, cmd);
	YYACCEPT;
};

gen_cmd:
TOK_VAL {
	ud_pkt_cmd_t cmd = resolve_tok(yylval.sval, yylval.slen);
	if (cmd == 0xffff) {
		fprintf(logout, "no such command: \"%s\"\n", yylval.sval);
		YYABORT;
	}
	udpc_make_pkt(hdl->pktchn[0], hdl->convo++, 0, cmd);
};

keyvals:
;

%%


static void
sigint_cb(EV_P_ ev_signal *w, int revents)
{
	ud_reset_stdin(EV_A);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *w, int revents)
{
	return;
}

static void
rplpkt_cb(EV_P_ ev_io *w, int revents)
{
	ssize_t nread;
	struct sockaddr_in6 sa;
	socklen_t lsa = sizeof(sa);
	char res[UDPC_SIMPLE_PKTLEN];

	nread = recvfrom(w->fd, res, countof(res), 0, &sa, &lsa);
	stdin_print_async(PACKET(nread, res), (void*)&sa, lsa);
	return;
}


/* completers */
/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
	/* User printable name of the function. */
	char *name;
	/* Documentation for this function.  */
	char *doc;
} cmd_t;

extern char *cmd_generator(const char *text, int state);
char*
cmd_generator(const char *text, int state)
{
	static size_t len;
	static cli_cmd_t last = NULL;

	/* if this is a new word to complete, initialize now.  This
	   includes saving the length of TEXT for efficiency, and
	   initializing the index variable to 0. */
	if (!state) {
		last = cmd_list;
		len = strlen(text);
	}

	/* return the next name which partially matches from the
	   command list. */
	while (last) {
		char *name = last->name;

		last = last->next;
		if (strncmp(name, text, len) == 0) {
			return strdup(name);
		}
	}

	/* if no names matched, then return NULL. */
	return NULL;
}

static void
init_cmd_list(void)
{
	add_cmd("wtf", 0, 0xffff);
	add_cmd("quit", 0, 0xffff);
	add_cmd("logout", 0, 0xffff);
	add_cmd("help", 0, 0xffff);
	add_cmd("kthx", 0, 0xffff);
	add_cmd("kthxbye", 0, 0xffff);
	add_cmd("bye", 0, 0xffff);
	add_cmd("alias", 0, 0xffff);
}


void
ud_parse(const ud_packet_t pkt)
{
        yyscan_t scanner;
        YY_BUFFER_STATE buf;

	/* set up our packet, just take the obligatory header for granted */
	__pkt.plen = 8;
	/* set up the lexer */
        cli_yylex_init(&scanner);
        buf = cli_yy_scan_string(pkt.pbuf, scanner);
	/* parse him */
        (void)cli_yyparse(scanner, &__hdl);
        cli_yylex_destroy(scanner);
	/* free the input line */
	free(pkt.pbuf);
	return;
}


int
main (void)
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop = ev_default_loop(0);
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	ev_io *srv_watcher = &__srv_watcher;

	/* where to log */
	logout = stderr;

	/* get us some nice handle */
	init_unserding_handle(&__hdl);
	/* store our global packet */
	__hdl.pktchn = &__pkt;
	/* attach the stdinlistener, inits readline too */
	ud_attach_stdin(EV_A);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise an io watcher, then start it */
	ev_io_init(srv_watcher, rplpkt_cb, ud_handle_sock(&__hdl), EV_READ);
	ev_io_start(EV_A_ srv_watcher);

	/* initialise command system */
	init_cmd_list();

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* close the socket */
	ud_detach_stdin(EV_A);

	/* destroy the default evloop */
	ev_default_destroy();

	/* free the handle */
	free_unserding_handle(&__hdl);
	/* close our log output */
	fflush(logout);
	fclose(logout);
	/* unloop was called, so exit */
	return 0;
}

/* parser.y ends here */
