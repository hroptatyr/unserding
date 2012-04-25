/*** unsermon.c -- unserding network monitor
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
/* context goodness, passed around internally */
#include "unserding-ctx.h"
/* our private bits */
#include "unserding-private.h"
#include "protocore-private.h"

#define USE_COROUTINES		1

#if defined DEBUG_FLAG
# define UD_CRITICAL_MCAST(args...)					\
	do {								\
		UD_LOGOUT("[unserding/input/mcast] CRITICAL " args);	\
		UD_SYSLOG(LOG_CRIT, "[input/mcast] CRITICAL " args);	\
	} while (0)
# define UD_DEBUG_MCAST(args...)					\
	do {								\
		UD_LOGOUT("[unserding/input/mcast] " args);		\
		UD_SYSLOG(LOG_INFO, "[input/mcast] " args);		\
	} while (0)
# define UD_INFO_MCAST(args...)						\
	do {								\
		UD_LOGOUT("[unserding/input/mcast] " args);		\
		UD_SYSLOG(LOG_INFO, "[input/mcast] " args);		\
	} while (0)
#else  /* !DEBUG_FLAG */
# define UD_CRITICAL_MCAST(args...)				\
	UD_SYSLOG(LOG_CRIT, "[input/mcast] CRITICAL " args)
# define UD_INFO_MCAST(args...)					\
	UD_SYSLOG(LOG_INFO, "[input/mcast] " args)
# define UD_DEBUG_MCAST(args...)
#endif	/* DEBUG_FLAG */

FILE *logout;
static FILE *monout;

#define UD_LOG_CRIT(args...)						\
	do {								\
		UD_LOGOUT("[unsermon] CRITICAL " args);		\
		UD_SYSLOG(LOG_CRIT, "CRITICAL " args);			\
	} while (0)
#define UD_LOG_INFO(args...)						\
	do {								\
		UD_LOGOUT("[unsermon] " args);				\
		UD_SYSLOG(LOG_INFO, args);				\
	} while (0)
#define UD_LOG_ERR(args...)						\
	do {								\
		UD_LOGOUT("[unsermon] ERROR " args);			\
		UD_SYSLOG(LOG_ERR, "ERROR " args);			\
	} while (0)
#define UD_LOG_NOTI(args...)						\
	do {								\
		UD_LOGOUT("[unsermon] NOTICE " args);			\
		UD_SYSLOG(LOG_NOTICE, args);				\
	} while (0)


typedef struct ud_ev_async_s ud_ev_async;

/* our version of the async event, cdr-coding */
struct ud_ev_async_s {
	struct ev_async super;
};

struct ud_loopclo_s {
	/** loop lock */
	pthread_mutex_t lolo;
	/** just a cond */
	pthread_cond_t loco;
};


/* the actual worker function, exec'd in a different thread */
static void
mon_pkt_cb(job_t j)
{
	static char buf[8192];
	size_t psz;

	if ((psz = ud_sprint_pkt_pretty(buf, JOB_PACKET(j)))) {
		buf[psz] = '\0';
		fputs(buf, logout);
	} else {
		fputs("NAUGHT message\n", logout);
	}
	return;
}

static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ssize_t nread;
	char buf[INET6_ADDRSTRLEN];
	/* the address in human readable form */
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	/* a job */
	struct job_s j[1];
	socklen_t lsa = sizeof(j->sa);

	j->sock = w->fd;
	nread = recvfrom(w->fd, j->buf, JOB_BUF_SIZE, 0, &j->sa.sa, &lsa);
	/* obtain the address in human readable form */
	a = inet_ntop(
		ud_sockaddr_fam(&j->sa), ud_sockaddr_addr(&j->sa),
		buf, sizeof(buf));
	p = ud_sockaddr_port(&j->sa);
	UD_INFO_MCAST(
		":sock %d connect :from [%s]:%d  "
		":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
		w->fd, a, p,
		(unsigned int)nread,
		udpc_pkt_cno(JOB_PACKET(j)),
		udpc_pkt_pno(JOB_PACKET(j)),
		udpc_pkt_cmd(JOB_PACKET(j)),
		ntohs(((const uint16_t*)j->buf)[3]));

	/* handle the reading */
	if (UNLIKELY(nread < 0)) {
		UD_CRITICAL_MCAST("could not handle incoming connection\n");
		goto out_revok;
	} else if (nread == 0) {
		/* no need to bother */
		goto out_revok;
	}

	j->blen = nread;

	/* enqueue t3h job and copy the input buffer over to
	 * the job's work space, also trigger the lazy bastards */
	(void)mon_pkt_cb(j);
out_revok:
	return;
}


static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sighup_watcher);
static ev_signal ALGN16(__sigterm_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

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


static void
triv_cb(EV_P_ ev_async *UNUSED(w), int UNUSED(revents))
{
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "unsermon-clo.h"
#include "unsermon-clo.c"
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
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sighup_watcher = &__sighup_watcher;
	ev_signal *sigterm_watcher = &__sigterm_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	ev_io *beef = NULL;
	size_t nbeef = 0;
	struct ud_ctx_s __ctx;
	struct ud_handle_s __hdl;
	/* args */
	struct gengetopt_args_info argi[1];

	/* whither to log */
	logout = stderr;
	monout = stdout;
	/* wipe stack pollution */
	memset(&__ctx, 0, sizeof(__ctx));

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);
	__ctx.mainloop = loop;

	/* initialise the lib handle */
	init_unserding_handle(&__hdl, PF_INET6, true);
	__ctx.hdl = &__hdl;

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

	/* initialise a wakeup handler */
	glob_notify = &__wakeup_watcher;
	ev_async_init(glob_notify, triv_cb);
	ev_async_start(EV_A_ glob_notify);

	/* make some room for the control channel and the beef chans */
	nbeef = argi->beef_given + 1;
	beef = malloc(nbeef * sizeof(*beef));

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	{
		int s = ud_mcast_init(UD_NETWORK_SERVICE);
		ev_io_init(beef, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef);
	}

	/* go through all beef channels */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		int s = ud_mcast_init(argi->beef_arg[i]);
		ev_io_init(beef + i + 1, mon_beef_cb, s, EV_READ);
		ev_io_start(EV_A_ beef + i + 1);
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	UD_LOG_NOTI("shutting down unsermon\n");

	/* detaching beef channels */

	for (unsigned int i = 0; i < nbeef; i++) {
		int s = beef[i].fd;
		ev_io_stop(EV_A_ beef + i);
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

/* unsermon.c ends here */
