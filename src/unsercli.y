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

%}

%expect 0

%token <noval>
	TOK_WTF
	TOK_CYA

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
}

wtf_cmd:
TOK_WTF;

cya_cmd:
TOK_CYA;

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

cmd_t commands[] = {
	{ "wtf", "Change to directory DIR" },
	{ "help", "Delete FILE" },
	{ "kthx", "Display this text" },
	{ "quit", "Synonym for `help'" },
	{ "bye", "List files in DIR" },
	{ "logout", "List files in DIR" },
	{ NULL, NULL }
};

extern char *cmd_generator(const char *text, int state);
char*
cmd_generator(const char *text, int state)
{
	static int list_index, len;
	char *name;

	/* If this is a new word to complete, initialize now.  This
	   includes saving the length of TEXT for efficiency, and
	   initializing the index variable to 0. */
	if (!state) {
		list_index = 0;
		len = strlen(text);
	}

	/* Return the next name which partially matches from the
	   command list. */
	while ((name = commands[list_index].name)) {
		list_index++;

		if (strncmp(name, text, len) == 0) {
			return strdup(name);
		}
	}

	/* If no names matched, then return NULL. */
	return NULL;
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
