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

#define USE_LUA
/* our master include file */
#include "unserding.h"
/* context goodness, passed around internally */
#include "unserding-ctx.h"
#include "unserding-cfg.h"
/* our private bits */
#include "unserding-private.h"
/* proto stuff */
#include "protocore.h"
/* module handling */
#include "module.h"
/* worker pool */
#include "wpool.h"

#define UD_VERSION		"v" PACKAGE_VERSION

FILE *logout;


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


/* module services */
struct ev_cb_clo_s {
	union {
		ev_idle idle;
		ev_timer timer;
	};
	void(*cb)(void*);
	void *clo;
};

static void
std_idle_cb(EV_P_ ev_idle *ie, int UNUSED(revents))
{
	struct ev_cb_clo_s *clo = (void*)ie;

	/* stop the idle event */
	ev_idle_stop(EV_A_ ie);

	/* call the call back */
	clo->cb(clo->clo);

	/* clean up */
	free(clo);
	return;
}

static void
std_timer_once_cb(EV_P_ ev_timer *te, int UNUSED(revents))
{
	struct ev_cb_clo_s *clo = (void*)te;

	/* stop the timer event */
	ev_timer_stop(EV_A_ te);

	/* call the call back */
	clo->cb(clo->clo);

	/* clean up */
	free(clo);
	return;
}

static void
std_timer_every_cb(EV_P_ ev_timer *te, int UNUSED(revents))
{
	struct ev_cb_clo_s *clo = (void*)te;

	/* call the call back */
	clo->cb(clo->clo);
	return;
}

void
schedule_once_idle(void *ctx, void(*cb)(void *clo), void *clo)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = xnew(struct ev_cb_clo_s);

	/* make a closure from cb and clo */
	f->cb = cb;
	f->clo = clo;

	ev_idle_init((ev_idle*)f, std_idle_cb);
	ev_idle_start(ud_ctx->mainloop, (void*)f);
	return;
}

void
schedule_timer_once(void *ctx, void(*cb)(void *clo), void *clo, double in)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = xnew(struct ev_cb_clo_s);

	/* make a closure from cb and clo */
	f->cb = cb;
	f->clo = clo;

	ev_timer_init((ev_timer*)f, std_timer_once_cb, in, 0.0);
	ev_timer_start(ud_ctx->mainloop, (void*)f);
	return;
}

void*
schedule_timer_every(void *ctx, void(*cb)(void *clo), void *clo, double every)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = xnew(struct ev_cb_clo_s);

	/* make a closure from cb and clo */
	f->cb = cb;
	f->clo = clo;

	ev_timer_init((ev_timer*)f, std_timer_every_cb, every, every);
	ev_timer_start(ud_ctx->mainloop, (void*)f);
	return f;
}

void
unsched_timer(void *ctx, void *timer)
{
	ud_ctx_t ud_ctx = ctx;
	struct ev_cb_clo_s *f = timer;

	ev_timer_stop(ud_ctx->mainloop, timer);
	f->cb = NULL;
	f->clo = NULL;
	free(timer);
	return;
}


static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sighup_watcher);
static ev_signal ALGN16(__sigterm_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_signal ALGN16(__sigusr2_watcher);
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
sigusr2_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	open_aux("dso-cli", NULL);
	ud_mod_dump(logout);
	return;
}


static void
triv_cb(EV_P_ ev_async *UNUSED(w), int UNUSED(revents))
{
	return;
}


/* helper for daemon mode */
static bool daemonisep = 0;
static bool prefer6p = 0;
static char *cf = NULL;

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
		UD_SYSLOG(LOG_NOTICE, "Successfully bore a squaller: %d\n", pid);
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
	logout = fopen("/dev/null", "w");
	return 0;
}


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


#define GLOB_CFG_PRE	"/etc/unserding"
#if !defined MAX_PATH_LEN
# define MAX_PATH_LEN	64
#endif	/* !MAX_PATH_LEN */

/* do me properly */
static const char cfg_glob_prefix[] = GLOB_CFG_PRE;

#if defined USE_LUA
static const char cfg_file_name[] = "unserding.lua";

static void
ud_expand_user_cfg_file_name(char *tgt)
{
	char *p;

	/* get the user's home dir */
	p = stpcpy(tgt, getenv("HOME"));
	*p++ = '/';
	*p++ = '.';
	strncpy(p, cfg_file_name, sizeof(cfg_file_name));
	return;
}

static void
ud_expand_glob_cfg_file_name(char *tgt)
{
	char *p;

	/* get the user's home dir */
	strncpy(tgt, cfg_glob_prefix, sizeof(cfg_glob_prefix));
	p = tgt + sizeof(cfg_glob_prefix);
	*p++ = '/';
	strncpy(p, cfg_file_name, sizeof(cfg_file_name));
	return;
}

static void
ud_read_config(ud_ctx_t ctx)
{
	char cfgf[MAX_PATH_LEN];

        UD_DEBUG("reading configuration from config file ...");
	lua_config_init(&ctx->cfgctx);

	/* we prefer the user's config file, then fall back to the
	 * global config file if that's not available */
	if (cf && read_lua_config(ctx->cfgctx, cf)) {
		UD_DBGCONT("done\n");
		return;
	}

	ud_expand_user_cfg_file_name(cfgf);
	if (read_lua_config(ctx->cfgctx, cfgf)) {
		UD_DBGCONT("done\n");
		return;
	}
	/* otherwise there must have been an error */
	ud_expand_glob_cfg_file_name(cfgf);
	if (read_lua_config(ctx->cfgctx, cfgf)) {
		UD_DBGCONT("done\n");
		return;
	}
	UD_DBGCONT("failed\n");
	return;
}

static void
ud_free_config(ud_ctx_t ctx)
{
	lua_config_deinit(&ctx->cfgctx);
	return;
}
#endif

static void
write_pidfile(const char *pidfile)
{
	char str[32];
	pid_t pid;
	size_t len;
	int fd;

	if ((pid = getpid()) &&
	    (len = snprintf(str, sizeof(str) - 1, "%d\n", pid)) &&
	    (fd = open(pidfile, O_RDWR | O_CREAT | O_TRUNC, 0644)) >= 0) {
		write(fd, str, len);
		close(fd);
	}
	return;
}


/* static module loader */
static void
ud_init_statmods(void *clo)
{
	dso_pong_LTX_init(clo);
	return;
}

static void
ud_deinit_statmods(void *clo)
{
	dso_pong_LTX_deinit(clo);
	return;
}


#include "unserdingd-clo.h"
#include "unserdingd-clo.c"

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sighup_watcher = &__sighup_watcher;
	ev_signal *sigterm_watcher = &__sigterm_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	ev_signal *sigusr2_watcher = &__sigusr2_watcher;
	struct ud_ctx_s __ctx;
	struct ud_handle_s __hdl;
	/* args */
	struct gengetopt_args_info argi[1];

	/* whither to log */
	logout = stderr;
	ud_openlog();
	/* wipe stack pollution */
	memset(&__ctx, 0, sizeof(__ctx));
	/* obtain the number of cpus */
	nworkers = get_num_proc();

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}
	/* evaluate argi */
	daemonisep |= argi->daemon_flag;
	cf = argi->config_arg;

	/* try and read the context file */
	ud_read_config(&__ctx);

	daemonisep |= udcfg_glob_lookup_b(&__ctx, "daemonise");
	prefer6p |= udcfg_glob_lookup_b(&__ctx, "prefer_ipv6");

	/* run as daemon, do me properly */
	if (daemonisep) {
		daemonise();
	}
	/* check if nworkers is not too large */
	if (nworkers > MAX_WORKERS) {
		nworkers = MAX_WORKERS;
	}
	/* write the pid file */
	if (argi->pidfile_given) {
		write_pidfile(argi->pidfile_arg);
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

	/* initialise the proto core (no-op at the mo) */
	init_proto();
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
	/* initialise a SIGUSR2 handler */
	ev_signal_init(sigusr2_watcher, sigusr2_cb, SIGUSR2);
	ev_signal_start(EV_A_ sigusr2_watcher);

	/* initialise a wakeup handler */
	glob_notify = &__wakeup_watcher;
	ev_async_init(glob_notify, triv_cb);
	ev_async_start(EV_A_ glob_notify);

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	ud_attach_mcast(EV_A_ prefer6p);

	/* attach the tcp/unix service */
	ud_attach_tcp_unix(EV_A_ prefer6p);

	/* initialise modules */
	ud_init_modules(argi->inputs, argi->inputs_num, &__ctx);

	/* static modules */
	ud_init_statmods(&__ctx);

	/* rock the wpool queue to trigger anything on there */
	wpool_trigger(gwpool);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	UD_SYSLOG(LOG_NOTICE, "shutting down unserdingd");

	/* deinitialise modules */
	ud_deinit_modules(&__ctx);

	/* pong service */
	ud_deinit_statmods(&__ctx);

	/* close the tcp/unix service */
	ud_detach_tcp_unix(EV_A);

	/* close the socket */
	ud_detach_mcast(EV_A);

	/* destroy the default evloop */
	ev_default_destroy();

	/* kill our slaves */
	kill_wpool(gwpool);
	/* kill our buckets */
	free_jpool(gjpool);

	/* kick the config context */
	ud_free_config(&__ctx);
	cmdline_parser_free(argi);

	/* close our log output */	
	fflush(logout);
	fclose(logout);
	ud_closelog();
	/* unloop was called, so exit */
	return 0;
}

/* unserdingd.c ends here */
