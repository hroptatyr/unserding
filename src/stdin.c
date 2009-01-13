/*** stdin.c -- network and console handlers for the unserding client
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
#include <string.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#if defined HAVE_NETDB_H
# include <netdb.h>
#elif defined HAVE_LWRES_NETDB_H
# include <lwres/netdb.h>
#endif	/* NETDB */
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif
/* gnu readline */
#include <readline/readline.h>
/* our master include */
#include "unserding.h"
#include "unserding-private.h"
#include "protocore.h"

static int lsock __attribute__((used));
static ev_io __srv_watcher __attribute__((aligned(16)));
extern void ud_print_stdin(job_t j);


/* string goodies */
static void
handle_rl(char *line)
{
	size_t llen;
	job_t j;

	/* print newline */
	if (UNLIKELY(line == NULL)) {
		/* print newline */
		puts("");
		/* just remove the handler here */
		rl_callback_handler_remove();
		/* and finally kick the event loop */
		ev_unloop(EV_DEFAULT_ EVUNLOOP_ALL);
		return;
	}
	UD_DEBUG_STDIN("received line \"%s\"\n", line);
	/* enqueue t3h job and copy the input buffer over to
	 * the job's work space */
	if (UNLIKELY((llen = strlen(line)) > 4096)) {
		llen = 4096;
	}
	j = obtain_job(glob_jq);
	j->prntf = ud_print_stdin;
	memcpy(j->buf, line, llen);
	enqueue_job(glob_jq, j);
	/* now notify the slaves */
	ud_parse(j);
	return;
}


static int
stdin_listener_init(void)
{
	/* initialise the readline */
	rl_readline_name = "unserding";
	rl_attempted_completion_function = NULL;

	rl_basic_word_break_characters = "\t\n@$><=;|&{( ";

	/* the callback */
	rl_callback_handler_install("unserding> ", handle_rl);

	/* succeeded if == 0 */
	return STDIN_FILENO;
}

static void
stdin_listener_deinit(EV_P_ int sock)
{
	UD_DEBUG_STDIN("deinitialising readline\n");
	rl_callback_handler_remove();

	UD_DEBUG_STDIN("closing listening socket %d...\n", sock);
	shutdown(sock, SHUT_RDWR);
	close(sock);

#if 0
	UD_DEBUG_STDIN("kicking ctx %p :socket %d...\n", ctx, ctx->snk);
	/* kick the io handlers */
	ev_io_stop(EV_A_ ctx_rio(ctx));
	ev_io_stop(EV_A_ ctx_wio(ctx));

	/* finally, give the ctx struct a proper rinse */
	ctx->src = ctx->snk = -1;
	ctx->bidx = 0;
#endif
	return;
}


/* this callback is called when data is readable on one of the polled socks */
static void
stdin_traf_rcb(EV_P_ ev_io *w, int revents)
{
	rl_callback_read_char();
	return;
}

/* this callback is called when data is writable on one of the polled socks */
static void __attribute__((unused))
stdin_traf_wcb(EV_P_ ev_io *w, int revents)
{
#if 0
	conn_ctx_t ctx = ev_wio_ctx(w);
	outbuf_t obuf;

	UD_DEBUG_STDIN("writing buffer to %d\n", ctx->snk);
	lock_obring(&ctx->obring);
	/* obtain the current output buffer */
	obuf = curr_outbuf(&ctx->obring);
	if (LIKELY(obuf->obufidx < obuf->obuflen)) {
		const char *buf =
			(char*)((long int)obuf->obuf & ~1UL) + obuf->obufidx;
		size_t blen = obuf->obuflen - obuf->obufidx;
		/* the actual write */
		obuf->obufidx += write(w->fd, buf, blen);
	}
	/* it's likely that we can output all at once */
	if (LIKELY(obuf->obufidx >= obuf->obuflen)) {
		/* reset the timeout, we want activity on the remote side now */
		ctx->timeout = ev_now(EV_A) + STDIN_TIMEOUT;
		/* free the buffer */
		free_outbuf(obuf);
		/* wind to the next outbuf */
		ctx->obring.curr_idx = step_obring_idx(ctx->obring.curr_idx);

		if (LIKELY(outbuf_free_p(curr_outbuf(&ctx->obring)))) {
			/* if nothing's to be printed just turn it off */
			ev_io_stop(EV_A_ w);
		}
	}
	unlock_obring(&ctx->obring);
	return;
#endif
}


void
ud_print_stdin(job_t j)
{
	printf(j->buf);
	return;
}

int
ud_attach_stdin(EV_P)
{
	ev_io *srv_watcher = &__srv_watcher;

	/* get us a global sock */
	lsock = stdin_listener_init();

	/* initialise an io watcher, then start it */
	ev_io_init(srv_watcher, stdin_traf_rcb, lsock, EV_READ);
	ev_io_start(EV_A_ srv_watcher);
	return 0;
}

int
ud_detach_stdin(EV_P)
{
	stdin_listener_deinit(EV_A_ lsock);
	return 0;
}

/* stdin.c ends here */
