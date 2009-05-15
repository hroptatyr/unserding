/*** unserdingd.c -- unserding network service daemon
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif
#if defined HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if defined HAVE_STDIO_H
# include <stdio.h>
#endif
#if defined HAVE_STDDEF_H
# include <stddef.h>
#endif
#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif
#if defined HAVE_STDBOOL_H
# include <stdbool.h>
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
#if defined HAVE_SYS_UN_H
# include <sys/un.h>
#endif
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif
#if defined HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if defined HAVE_POPT_H || 1
# include <popt.h>
#endif

/* our master include file */
#include "unserding.h"
/* our private bits */
#include "unserding-private.h"
/* context stuff */
#include "unserding-ctx.h"
/* proto stuff */
#include "protocore.h"
/* module handling */
#include "module.h"

FILE *logout;


typedef struct ud_worker_s *ud_worker_t;
typedef struct ud_ev_async_s ud_ev_async;

/* our version of the async event, cdr-coding */
struct ud_ev_async_s {
	struct ev_async super;
};

struct ud_worker_s {
	pthread_t ALGN16(thread);
} __attribute__((aligned(16)));


static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sighup_watcher);
static ev_signal ALGN16(__sigterm_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_signal ALGN16(__sigusr2_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

/* worker magic */
#define NWORKERS		4
/* round robin var */
static index_t rr_wrk = 0;
/* the workers array */
static struct ud_worker_s __attribute__((aligned(16))) workers[NWORKERS];

static struct ev_loop *secl;
/* a watcher for worker jobs */
struct ev_async ALGN16(work_watcher);
/* a watcher for harakiri orders */
struct ev_async ALGN16(kill_watcher);

static inline struct ev_loop __attribute__((always_inline, gnu_inline)) *
worker_loop(ud_worker_t wk)
{
	return secl;
}

static inline struct ev_async __attribute__((always_inline, gnu_inline)) *
worker_workw(ud_worker_t wk)
{
	return &work_watcher;
}

static inline struct ev_async __attribute__((always_inline, gnu_inline)) *
worker_killw(ud_worker_t wk)
{
	return &kill_watcher;
}

/* the global job queue */
static struct job_queue_s __glob_jq;
job_queue_t glob_jq;


inline void __attribute__((always_inline, gnu_inline))
trigger_job_queue(void)
{
	/* look what we can do */
	/* easy */
	ev_async_send(secl, &work_watcher);
	return;
}

static const char emer_msg[] = "unserding has been shut down, cya mate!\n";

static void
sigint_cb(EV_P_ ev_signal *w, int revents)
{
	UD_DEBUG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sighup_cb(EV_P_ ev_signal *w, int revents)
{
	UD_DEBUG("SIGHUP caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigusr2_cb(EV_P_ ev_signal *w, int revents)
{
	open_aux("dso-cli", NULL);
	ud_mod_dump(logout);
	return;
}


static void
triv_cb(EV_P_ ev_async *w, int revents)
{
	return;
}

static void
kill_cb(EV_P_ ev_async *w, int revents)
{
	long int UNUSED(self) = (long int)pthread_self();
	UD_DEBUG("SIGQUIT caught in %lx\n", self);
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
worker_cb(EV_P_ ev_async *w, int revents)
{
	long int UNUSED(self) = (long int)pthread_self();
	job_t j;

	while ((j = dequeue_job(glob_jq)) != NO_JOB) {
		UD_DEBUG("thread/loop %lx/%p doing work %p\n", self, loop, j);
		ud_proto_parse(j);
#if 0
/* we took precautions to send off the packet that's now in j
 * based on the transmission flags, however, that's additional bollocks
 * atm so we let the proto funs deal with that themselves */
		send_cl(j);
#endif
		free_job(j);
	}

	UD_DEBUG("no more jobs %lx/%p\n", self, loop);
	return;
}

static void*
worker(void *wk)
{
	long int UNUSED(self) = pthread_self();
	void *loop = worker_loop(wk);
	UD_DEBUG("starting worker thread %lx, loop %p\n", self, loop);
	ev_loop(EV_A_ 0);
	UD_DEBUG("quitting worker thread %lx, loop %p\n", self, loop);
	return NULL;
}

static void
add_worker(struct ev_loop *loop)
{
	pthread_attr_t attr;
	ud_worker_t wk = &workers[rr_wrk++];

	/* initialise thread attributes */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	/* start the thread now */
	pthread_create(&wk->thread, &attr, worker, wk);

	/* destroy locals */
	pthread_attr_destroy(&attr);
	return;
}

static void
kill_worker(ud_worker_t wk)
{
	/* send a lethal signal to the workers and detach */
	ev_async_send(worker_loop(wk), worker_killw(wk));
	return;
}


/* helper for daemon mode */
static bool daemonisep = 0;

static int
daemonise(void)
{
	int fd;
	pid_t pid;

	switch (pid = fork()) {
	case -1:
		return false;
	case 0:
		break;
	default:
		UD_DEBUG("Successfully bore a squaller: %d\n", pid);
		exit(0);
	}

	if (setsid() == -1) {
		return false;
	}
	for (int i = getdtablesize(); i>=0; --i) {
		/* close all descriptors */
		close(i);
	}
	if (LIKELY((fd = open("/dev/null", O_RDWR, 0)) >= 0)) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > STDERR_FILENO) {
			(void)close(fd);
		}
	}
	logout = fopen("/tmp/unserding.log", "w");
	return 0;
}


/* the popt helper */
static struct poptOption srv_opts[] = {
	{ "daemon", 'd', POPT_ARG_NONE | POPT_ARGFLAG_SHOW_DEFAULT,
	  &daemonisep, 0,
	  "Detach from tty and run as daemon.", NULL },
        POPT_TABLEEND
};

static const struct poptOption const ud_opts[] = {
#if 1
        { NULL, '\0', POPT_ARG_INCLUDE_TABLE, srv_opts, 0,
          "Server Options", NULL },
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
        opt_ctx = poptGetContext(NULL, argc, argv, ud_opts, 0);
        poptSetOtherOptionHelp(
		opt_ctx,
		"[server-options] "
		"module [module [...]]");

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
	struct ev_loop *loop;
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sighup_watcher = &__sighup_watcher;
	ev_signal *sigterm_watcher = &__sigterm_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	ev_signal *sigusr2_watcher = &__sigusr2_watcher;
	const char *const *rest;
	struct ud_ctx_s __ctx;

	/* whither to log */
	logout = stderr;

	/* parse the command line */
	rest = ud_parse_cl(argc, argv);

	/* run as daemon, do me properly */
	if (daemonisep) {
		daemonise();
	}
	/* initialise the main loop */
	loop = ev_default_loop(0);
	__ctx.mainloop = loop;

	/* initialise global job q */
	init_glob_jq(&__glob_jq);

	/* initialise the proto core */
	init_proto();

	/* initialise modules */
	ud_init_modules(rest, &__ctx);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigint_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sighup_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);
	/* initialise a SIGUSR2 handler */
	ev_signal_init(sigusr2_watcher, sigusr2_cb, SIGUSR2);
	ev_signal_start(EV_A_ sigusr2_watcher);

	/* initialise a wakeup handler */
	glob_notify = &__wakeup_watcher;
	ev_async_init(glob_notify, triv_cb);
	ev_async_start(EV_A_ glob_notify);

	/* create one loop for all threads */
	secl = ev_loop_new(0);
	{
		ev_async *eva = &work_watcher;
		ev_async_init(eva, worker_cb);
		ev_async_start(secl, eva);
	}
	{
		ev_async *eva = &kill_watcher;
		ev_async_init(eva, kill_cb);
		ev_async_start(secl, eva);
	}
	/* set up the worker threads along with their secondary loops */
	for (index_t i = 0; i < NWORKERS; i++) {
		add_worker(secl);
	}

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	ud_attach_mcast4(EV_A);

	/* reset the round robin var */
	rr_wrk = 0;
	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* deinitialise modules */
	ud_deinit_modules(&__ctx);

	/* close the socket */
	ud_detach_mcast4(EV_A);

	/* kill the workers along with their secondary loops */
	for (index_t i = NWORKERS; i > 0; i--) {
		UD_DEBUG("killing worker %lu\n", (long unsigned int)i - 1);
		kill_worker(&workers[i-1]);
	}
	for (index_t i = NWORKERS; i > 0; i--) {
		UD_DEBUG("killing worker %lu\n", (long unsigned int)i - 1);
		kill_worker(&workers[i-1]);
		usleep(10000);
	}
	for (index_t i = NWORKERS; i > 0; i--) {
		UD_DEBUG("gathering worker %lu\n", (long unsigned int)i - 1);
		pthread_join(workers[i-1].thread, NULL);
	}
	/* destroy the secondary loop */
	ev_loop_destroy(secl);

	/* destroy the default evloop */
	ev_default_destroy();

	/* close our log output */	
	fflush(logout);
	fclose(logout);
	/* unloop was called, so exit */
	return 0;
}

/* unserdingd.c ends here */
