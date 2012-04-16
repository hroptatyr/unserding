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
/* worker pool */
#include "wpool.h"

#define USE_COROUTINES		1

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
	fputs("ACK\n", monout);
	jpool_release(j);
	return;
}


static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sighup_watcher);
static ev_signal ALGN16(__sigterm_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

/* worker magic */
static int nworkers = 1;

/* the global job queue */
jpool_t gjpool;
/* holds worker pool */
wpool_t gwpool;

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


/* helper for daemon mode */
static bool prefer6p = true;


/* helper function for the worker pool */
static int
get_num_proc(void)
{
#if defined HAVE_PTHREAD_AFFINITY_NP
	long int self = pthread_self();
	cpu_set_t cpuset;

	if (pthread_getaffinity_np(self, sizeof(cpuset), &cpuset) == 0) {
		int ret = cpuset_popcount(&cpuset);
		if (ret > 0) {
			return ret;
		} else {
			return 1;
		}
	}
#endif	/* HAVE_PTHREAD_AFFINITY_NP */
#if defined _SC_NPROCESSORS_ONLN
	return sysconf(_SC_NPROCESSORS_ONLN);
#else  /* !_SC_NPROCESSORS_ONLN */
/* any ideas? */
	return 1;
#endif	/* _SC_NPROCESSORS_ONLN */
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
	struct ud_ctx_s __ctx;
	struct ud_handle_s __hdl;
	/* args */
	struct gengetopt_args_info argi[1];

	/* whither to log */
	logout = stderr;
	monout = stdout;
	/* wipe stack pollution */
	memset(&__ctx, 0, sizeof(__ctx));
	/* obtain the number of cpus */
	nworkers = get_num_proc();

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	/* check if nworkers is not too large */
	if (nworkers > MAX_WORKERS) {
		nworkers = MAX_WORKERS;
	}
	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);
	__ctx.mainloop = loop;

	/* create the job pool, here because we may want to offload stuff
	 * the name job pool is misleading, it's a bucket pool with
	 * equally sized buckets of memory */
	gjpool = make_jpool(NJOBS, sizeof(struct job_s));
	/* create the worker pool */
	gwpool = make_wpool(nworkers, NJOBS);

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

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	ud_attach_mcast(EV_A_ mon_pkt_cb, prefer6p);

	/* rock the wpool queue to trigger anything on there */
	wpool_trigger(gwpool);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	UD_LOG_NOTI("shutting down unsermon\n");

	/* close the socket */
	ud_detach_mcast(EV_A);

	/* destroy the default evloop */
	ev_default_destroy();

	/* kill our buckets */
	free_jpool(gjpool);

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
