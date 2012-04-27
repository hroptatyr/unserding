/*** umod-example.c -- monitor 8584 channel and pick up certain quotes
 *
 * Copyright (C) 2012 Sebastian Freundt
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
/* context goodness, passed around internally */
#include "unserding-ctx.h"
/* our private bits */
#include "unserding-private.h"
#include "protocore-private.h"

#define USE_COROUTINES		1

#if defined DEBUG_FLAG
# define UD_CRITICAL_MCAST(args...)					\
	do {								\
		UD_LOGOUT("[umod/beef] CRITICAL " args);		\
		UD_SYSLOG(LOG_CRIT, "[umod/beef] CRITICAL " args);	\
	} while (0)
# define UD_DEBUG_MCAST(args...)
# define UD_INFO_MCAST(args...)						\
	do {								\
		UD_LOGOUT("[umod/beef] " args);				\
		UD_SYSLOG(LOG_INFO, "[umod/beef] " args);		\
	} while (0)
#else  /* !DEBUG_FLAG */
# define UD_CRITICAL_MCAST(args...)				\
	UD_SYSLOG(LOG_CRIT, "[umod/beef] CRITICAL " args)
# define UD_INFO_MCAST(args...)					\
	UD_SYSLOG(LOG_INFO, "[umod/beef] " args)
# define UD_DEBUG_MCAST(args...)
#endif	/* DEBUG_FLAG */

FILE *logout;
static FILE *monout;


static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	static char pkt[UDPC_PKTLEN];
	ssize_t nrd;
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	union ud_sockaddr_u sa;
	socklen_t nsa = sizeof(sa);

	nrd = recvfrom(w->fd, pkt, sizeof(pkt), 0, &sa.sa, &nsa);
	/* obtain the address in human readable form */
	a = inet_ntop(AF_INET6, ud_sockaddr_addr(&sa), buf, sizeof(buf));
	p = ud_sockaddr_port(&sa);
	UD_INFO_MCAST(
		":sock %d connect :from [%s]:%d  "
		":len %04zx :cno %02x :pno %06x :cmd %04x :mag %04x\n",
		w->fd, a, p, nrd,
		udpc_pkt_cno((ud_packet_t){nrd, pkt}),
		udpc_pkt_pno((ud_packet_t){nrd, pkt}),
		udpc_pkt_cmd((ud_packet_t){nrd, pkt}),
		ntohs(((const uint16_t*)pkt)[3]));

	/* handle the reading */
	if (UNLIKELY(nrd < 0)) {
		UD_CRITICAL_MCAST("could not handle incoming connection\n");
		goto out_revok;
	} else if (nrd == 0) {
		/* no need to bother */
		goto out_revok;
	}

out_revok:
	return;
}


static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("SIGPIPE caught, doing nothing\n");
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("SIGHUP caught, unrolling everything\n");
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
#include "umod-example-clo.h"
#include "umod-example-clo.c"
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
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_io beef[2];
	/* args */
	struct gengetopt_args_info argi[1];

	/* whither to log */
	logout = stderr;
	monout = stdout;

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sighup_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	{
		int s = ud_mcast_init(UD_NETWORK_SERVICE);
		ev_io_init(beef + 0, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + 0);
	}

	if (argi->beef_given) {
		int s = ud_mcast_init(argi->beef_arg);
		ev_io_init(beef + 1, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + 1);
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* detaching beef channels */
	if (argi->beef_given) {
		int s = beef[1].fd;
		ev_io_stop(EV_A_ beef + 1);
		ud_mcast_fini(s);
	}
	{
		int s = beef[0].fd;
		ev_io_stop(EV_A_ beef + 0);
		ud_mcast_fini(s);
	}

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	cmdline_parser_free(argi);

	/* close our log output */
	fflush(monout);
	fflush(logout);
	fclose(monout);
	fclose(logout);
	/* unloop was called, so exit */
	return 0;
}

/* umod-example.c ends here */
