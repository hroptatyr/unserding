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

#include <readline/readline.h>

/* our master include file */
#include "unserding.h"
/* our private bits */
#include "unserding-private.h"


typedef struct ud_worker_s *ud_worker_t;
typedef struct ud_ev_async_s ud_ev_async;

/* our version of the async event, cdr-coding */
struct ud_ev_async_s {
	struct ev_async super;
};


static index_t __attribute__((unused)) glob_idx = 0;
static struct conn_ctx_s glob_ctx[1];

static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

/* worker magic */
#define NWORKERS		4
/* round robin var */
static index_t rr_wrk = 0;

/* the global job queue */
static struct job_queue_s __glob_jq = {
	.ri = 0, .wi = 0, .mtx = PTHREAD_MUTEX_INITIALIZER
};
job_queue_t glob_jq;


static const char emer_msg[] = "unserding has been shut down, cya mate!\n";

static void
sigint_cb(EV_P_ ev_signal *w, int revents)
{
	UD_DEBUG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

conn_ctx_t
find_ctx(void)
{
/* returns the next free context */
	for (index_t res = 0; res < countof(glob_ctx); res++) {
		if (glob_ctx[res].snk == -1) {
			return &glob_ctx[res];
		}
	}
	return NULL;
}


static void
init_glob_ctx(void)
{
	for (index_t i = 0; i < countof(glob_ctx); i++) {
		glob_ctx[i].snk = -1;
	}
	return;
}

static void
init_glob_jq(void)
{
	glob_jq = &__glob_jq;
	return;
}

static void
init_readline(void)
{
	rl_readline_name = "unserding";
	rl_attempted_completion_function = NULL;

	rl_basic_word_break_characters = "\t\n@$><=;|&{( ";
	return;
}

int
main (void)
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop = ev_default_loop(0);
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	char *test;

	/* initialise the global context */
	init_glob_ctx();
	/* initialise global job q */
	init_glob_jq();
	/* initialise readline */
	init_readline();

	/* attach a tcp listener */
	ud_attach_stdin(EV_A);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);

	test = readline("unserding> ");

	/* reset the round robin var */
	rr_wrk = 0;
	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* close the socket */
	ud_detach_stdin(EV_A);

	/* destroy the default evloop */
	ev_default_destroy();

	/* unloop was called, so exit */
	return 0;
}

/* unserding.c ends here */
