/*** unserding.c -- unserding network service (client)
 *
 * Copyright (C) 2008 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
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

/* our master include file */
#include "unserding.h"
/* our private bits */
#include "unserding-private.h"

FILE *logout;


typedef struct ud_worker_s *ud_worker_t;
typedef struct ud_ev_async_s ud_ev_async;

/* should that be properly public? */
extern void
stdin_print_async(ud_packet_t pkt, struct sockaddr_in *sa, socklen_t sal);


static index_t __attribute__((unused)) glob_idx = 0;

static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
//static ev_async ALGN16(__wakeup_watcher);
static ev_io ALGN16(__srv_watcher);
ev_async *glob_notify;

/* the global job queue */
static struct job_queue_s __glob_jq = {
	.ji = 0, .mtx = PTHREAD_MUTEX_INITIALIZER
};
job_queue_t glob_jq;

static struct ud_handle_s __hdl;

void
trigger_job_queue(void)
{
	return;
}


static const char emer_msg[] = "unserding has been shut down, cya mate!\n";

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


/* parser madness */
#include "unsercli-parser.h"
#include "unsercli-scanner.h"
#include "protocore.h"

extern int cli_yyparse(void *scanner, ud_handle_t);

void
ud_parse(const ud_packet_t pkt)
{
        yyscan_t scanner;
        YY_BUFFER_STATE buf;

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


static void
init_glob_jq(void)
{
	glob_jq = &__glob_jq;
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

	/* initialise global job q */
	init_glob_jq();

	/* get us some nice handle */
	make_unserding_handle(&__hdl);
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

/* unserding.c ends here */
