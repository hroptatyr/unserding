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

#if defined HAVE_POPT_H || 1
# include <popt.h>
#endif

#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"
#include "protocore-private.h"
#include "cli-common.h"
#include "unsercli-scanner.h"
#include "stdin.h"

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

/* popt helper */
static int prefer4 = 0;
static uint16_t port = UD_NETWORK_SERVICE;
/* whether to display a hex dump of incoming traffic */
static int hexp = 0;

static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_io ALGN16(__srv_watcher);
ev_async *glob_notify;

/* our global handle space, closured because */
static struct ud_handle_s __hdl;
static char __pktbuf[UDPC_PKTLEN];
static ud_packet_t __pkt = {.plen = countof(__pktbuf), .pbuf = __pktbuf};

/* these delcarations are provided to suppress compiler warnings */
extern int cli_yylex();
extern int cli_yyget_lineno();
extern char *cli_yyget_text();
extern void cli_yyerror(void *scanner, ud_handle_t hdl, char const *errmsg);


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
static struct udpc_seria_s sctx;

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

static void
rem_cmd(const char *name, size_t nlen)
{
	if (nlen == 0) {
		nlen = strlen(name);
	}
	if (nlen > 15) {
		nlen = 15;
	}

	for (cli_cmd_t c = cmd_list, o = NULL; c; o = c, c = c->next) {
		if (strncmp(c->name, name, nlen) == 0) {
			/* found him */
			if (o == NULL) {
				cmd_list = c->next;
			} else {
				o->next = c->next;
			}
			free(c);
			return;
		}
	}
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

static uint8_t
parse_ui8h(const char *tok, size_t len)
{
/* format is 0xnn or 0xn */
	uint8_t res = 0;
	uint8_t tmp;

	if ((tmp = hexdig_to_nibble(tok[2])) < 16) {
		if (len == 4) {
			res |= tmp << 4;
		} else {
			return tmp;
		}
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[3])) < 16) {
		res |= tmp;
	} else {
		goto out;
	}
	return res;
out:
	return 0;
}

static uint16_t
parse_ui16h(const char *tok, size_t len)
{
/* format is 0xnnn or 0xnnnn */
	uint16_t res = 0;
	uint8_t tmp;

	/* try the hex reader */
	if ((tmp = hexdig_to_nibble(tok[2])) < 16) {
		if (len == 6) {
			res |= tmp << 12;
		} else {
			res |= tmp << 8;
		}
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[3])) < 16) {
		if (len == 6) {
			res |= tmp << 8;
		} else {
			res |= tmp << 4;
		}
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[4])) < 16) {
		if (len == 6) {
			res |= tmp << 4;
		} else {
			return res | tmp;
		}
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[5])) < 16) {
		res |= tmp << 0;
	} else {
		goto out;
	}
	return res;
out:
	return 0;
}

static uint32_t
parse_ui32h(const char *tok, size_t len)
{
/* format is 0xnnnnn or 0xnnnnnn or 0xnnnnnnn or 0xnnnnnnnn */
	uint32_t res = 0;
	uint8_t tmp;

	/* try the hex reader */
	if ((tmp = hexdig_to_nibble(tok[2])) < 16) {
		res |= tmp << ((len - 3) * 4);
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[3])) < 16) {
		res |= tmp << ((len - 4) * 4);
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[4])) < 16) {
		res |= tmp << ((len - 5) * 4);
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[5])) < 16) {
		res |= tmp << ((len - 6) * 4);
	} else {
		goto out;
	}
	for (int i = 6; i < 10; i++) {
		if ((tmp = hexdig_to_nibble(tok[i])) < 16) {
			res |= tmp << ((len - i - 1) * 4);
			if (len - i - 1 == 0) {
				return res;
			}
		} else {
			goto out;
		}
	}
	return res;
out:
	return 0;
}

static uint64_t
parse_ui64h(const char *tok, size_t len)
{
/* format is ... ya know */
	uint32_t res = 0;
	uint8_t tmp;

	/* try the hex reader */
	if ((tmp = hexdig_to_nibble(tok[2])) < 16) {
		res |= tmp << ((len - 3) * 4);
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[3])) < 16) {
		res |= tmp << ((len - 4) * 4);
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[4])) < 16) {
		res |= tmp << ((len - 5) * 4);
	} else {
		goto out;
	}
	if ((tmp = hexdig_to_nibble(tok[5])) < 16) {
		res |= tmp << ((len - 6) * 4);
	} else {
		goto out;
	}
	for (int i = 6; i < 18; i++) {
		if ((tmp = hexdig_to_nibble(tok[i])) < 16) {
			res |= tmp << ((len - i - 1) * 4);
			if (len - i - 1 == 0) {
				return res;
			}
		} else {
			goto out;
		}
	}
	return res;
out:
	return 0;
}

static int32_t
parse_int(const char *tok, size_t len)
{
	long int res;
	char *on;

	res = strtol(tok, &on, 10);
	if (on != 0) {
		return (int32_t)res;
	}
	return 0;
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
	TOK_HEXCMD
	TOK_UI64H
	TOK_UI32H
	TOK_UI16H
	TOK_UI8H
	TOK_INT
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
gen_cmd tvs {
	hdl->pktchn[0].plen = UDPC_HDRLEN + udpc_seria_msglen(&sctx);
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
	rem_cmd($<sval>2, $<slen>2);
	YYACCEPT;
} |
TOK_ALI TOK_VAL TOK_VAL {
	ud_pkt_cmd_t cmd = resolve_tok($<sval>3, $<slen>3);

	add_cmd($<sval>2, $<slen>2, cmd);
	YYACCEPT;
};

gen_cmd:
TOK_HEXCMD {
	ud_pkt_cmd_t cmd = resolve_tok(yylval.sval, yylval.slen);
	if (cmd == 0xffff) {
		fprintf(logout, "no such command: \"%s\"\n", yylval.sval);
		YYABORT;
	}
	udpc_make_pkt(hdl->pktchn[0], hdl->convo++, 0, cmd);
	udpc_seria_init(&sctx, UDPC_PAYLOAD(hdl->pktchn[0].pbuf), UDPC_PLLEN);
};

tvs:
/* nothing */ |
tv tvs
;

tv:
TOK_STRING {
	udpc_seria_add_str(&sctx, yylval.sval, yylval.slen);
} |
TOK_UI8H {
	uint8_t val = (uint8_t)parse_ui8h(yylval.sval, yylval.slen);
	udpc_seria_add_byte(&sctx, val);
} |
TOK_UI16H {
	uint16_t val = (uint16_t)parse_ui16h(yylval.sval, yylval.slen);
	udpc_seria_add_ui16(&sctx, val);
} |
TOK_UI32H {
	uint32_t val = (uint32_t)parse_ui32h(yylval.sval, yylval.slen);
	udpc_seria_add_ui32(&sctx, val);
} |
TOK_UI64H {
	uint64_t val = (uint64_t)parse_ui64h(yylval.sval, yylval.slen);
	udpc_seria_add_ui64(&sctx, val);
} |
TOK_INT {
	int32_t val = (int32_t)parse_int(yylval.sval, yylval.slen);
	udpc_seria_add_si32(&sctx, val);
};

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

static size_t
rplpkt_pr_src(char *restrict outbuf, const ud_sockaddr_t *sa)
{
	static const char srcstr[] = "packet from ";
	static const char unkstr[] = "unknown";
	size_t res = sizeof(srcstr) - 1;
	/* the port (in host-byte order) */
	uint16_t p;
	int fam = ud_sockaddr_fam(sa);

	/* copy the introductory text */
	memcpy(outbuf, srcstr, res);
	/* obtain the address in human readable form */
	switch (fam) {
	case PF_INET6:
		outbuf[res++] = '[';
		inet_ntop(fam, ud_sockaddr_addr(sa), &outbuf[res], 256);
		res += strlen(&outbuf[res]);
		outbuf[res++] = ']';
		break;
	case PF_INET:
		inet_ntop(fam, ud_sockaddr_addr(sa), &outbuf[res], 256);
		res += strlen(&outbuf[res]);
		break;
	default:
		memcpy(&outbuf[res], unkstr, sizeof(unkstr));
		return res + sizeof(unkstr) - 1;
	}
	outbuf[res++] = ':';
	p = ud_sockaddr_port(sa);
	res += sprintf(&outbuf[res], "%d", p);
	return res;
}

static void
rplpkt_cb(EV_P_ ev_io *w, int revents)
{
	ssize_t nread;
	ud_sockaddr_t sa;
	socklen_t lsa = sizeof(sa);
	char res[UDPC_PKTLEN];
	/* the print buffer */
	static char prbuf[8 * UDPC_PKTLEN];
	size_t len = 0;

	nread = recvfrom(w->fd, res, countof(res), 0, (void*)&sa, &lsa);
	len = rplpkt_pr_src(prbuf, &sa);
	prbuf[len++] = ' ';

	/* now the header */
	len += ud_sprint_pkthdr(&prbuf[len], PACKET(nread, res));
	/* the raw packet */
	if (hexp) {
		len += ud_sprint_pkt_raw(&prbuf[len], PACKET(nread, res));
	}
	/* the packet in pretty */
	len += ud_sprint_pkt_pretty(&prbuf[len], PACKET(nread, res));
	prbuf[len] = '\0';

	/* print him */
	stdin_print_async(prbuf, len);
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


static void
ud_parse(const char *line, size_t len)
{
        yyscan_t scanner;
        YY_BUFFER_STATE buf;

	/* set up our packet, just take the obligatory header for granted */
	__pkt.plen = 8;
	/* set up the lexer */
        cli_yylex_init(&scanner);
        buf = cli_yy_scan_string(line, scanner);
	/* parse him */
        (void)cli_yyparse(scanner, &__hdl);
        cli_yylex_destroy(scanner);
	return;
}


static struct poptOption srv_opts[] = {
	{ NULL, '4', POPT_ARG_NONE,
	  &prefer4, 1,
	  "Prefer IPv4 multicasting to IPv6.", NULL },
	{ "port", 'p', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT,
	  &port, 0,
	  "Multicast port.", NULL },
        POPT_TABLEEND
};

static struct poptOption dpy_opts[] = {
	{ NULL, 'x', POPT_ARG_NONE,
	  &hexp, 1,
	  "Include hex dump of incoming traffic.", NULL },
        POPT_TABLEEND
};

static const struct poptOption const uc_opts[] = {
#if 1
        { NULL, '\0', POPT_ARG_INCLUDE_TABLE, srv_opts, 0,
          "Server Options", NULL },
        { NULL, '\0', POPT_ARG_INCLUDE_TABLE, dpy_opts, 0,
          "Display Options", NULL },
#endif
        POPT_AUTOHELP
        POPT_TABLEEND
};

static const char *const*
ud_parse_cl(size_t argc, const char *argv[])
{
        int rc;
        poptContext opt_ctx;

        UD_DEBUG("parsing command line options\n");
        opt_ctx = poptGetContext(NULL, argc, argv, uc_opts, 0);
        poptSetOtherOptionHelp(opt_ctx, "-4x -p <port>");

        /* auto-do */
        while ((rc = poptGetNextOpt(opt_ctx)) > 0) {
                /* Read all the options ... */
                ;
        }
        return poptGetArgs(opt_ctx);
}


int
main(int argc, const char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop = ev_default_loop(0);
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	ev_io *srv_watcher = &__srv_watcher;
	const char *const *rest;

	/* where to log */
	logout = stderr;

	/* parse the command line */
	rest = ud_parse_cl(argc, argv);
	for (const char *const *r = rest; r && *r; r++) {
		UD_DEBUG("unknown option \"%s\"\n", *r);
	}

	/* get us some nice handle */
	if (!prefer4) {
		init_unserding_handle(&__hdl, PF_UNSPEC);
	} else {
		init_unserding_handle(&__hdl, PF_INET);
	}
	if (port != UD_NETWORK_SERVICE) {
		ud_handle_set_port(&__hdl, port);
	}
	/* store our global packet */
	__hdl.pktchn = &__pkt;
	/* attach the stdinlistener, inits readline too */
	ud_attach_stdin(EV_A_ &ud_parse);

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
