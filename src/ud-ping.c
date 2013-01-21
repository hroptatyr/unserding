/*** ud-ping.c -- ping/pong utility
 *
 * Copyright (C) 2009-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include "unserding.h"
#include "svc-pong.h"
#include "ud-time.h"
#include "unserding-nifty.h"


/* callbacks for libev */
static void
sub_cb(EV_P_ ev_io *w, int UNUSED(rev))
{
	ud_sock_t s = w->data;
	struct svc_ping_s po[1];

	if (ud_chck_ping(po, s) < 0) {
		/* don't care */
		return;
	}

	/* otherwise inspect packet */
	fprintf(stdout, "%jd\t%s\n", (intmax_t)po->pid, po->hostname);
	return;
}

static void
ptm_cb(EV_P_ ev_timer *w, int UNUSED(rev))
{
	ud_sock_t s = w->data;

	(void)ud_pack_pong(s, 0/*ping*/);
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(rev))
{
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "ud-ping-clo.h"
#include "ud-ping-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#elif defined __GNUC__
# pragma GCC diagnostic warning "-Wswitch"
# pragma GCC diagnostic warning "-Wswitch-enum"
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct ud_args_info argi[1];
	/* ev io */
	struct ev_loop *loop;
	ev_signal sigint_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_io sub[1];
	ev_timer ptm[1];
	/* unserding specific */
	ud_sock_t s;
	int res = 0;

	/* parse the command line */
	if (ud_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}
	/* obtain a new handle */
	if ((s = ud_socket((struct ud_sockopt_s){UD_PUBSUB})) == NULL) {
		perror("cannot initialise ud socket");
		return 1;
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigall_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigall_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);

	sub->data = s;
	ev_io_init(sub, sub_cb, s->fd, EV_READ);
	ev_io_start(EV_A_ sub);

	ptm->data = s;
	ev_timer_init(ptm, ptm_cb, 0., argi->interval_arg);
	ev_timer_start(EV_A_ ptm);

	/* classic mode */
	puts("ud-ping " UD_MCAST6_ADDR " (" UD_MCAST6_ADDR ") 8 bytes of data");

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	ev_io_stop(EV_A_ sub);
	ud_close(s);

	/* destroy the default evloop */
	ev_default_destroy();

out:
	ud_parser_free(argi);
	return res;
}

/* ud-tick.c ends here */
