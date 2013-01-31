/*** ud-router.c -- unserding router-dealer
 *
 * Copyright (C) 2013 Sebastian Freundt
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
#include <stdio.h>
#include <string.h>
#include <time.h>
#if defined HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */
#if defined HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif	/* HAVE_SYS_SOCKET_H */
#if defined HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif	/* HAVE_NETINET_IN_H */
#if defined HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif	/* HAVE_ARPA_INET_H */
#if defined HAVE_NETDB_H
# include <netdb.h>
#endif	/* HAVE_NETDB_H */
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif	/* HAVE_ERRNO_H */
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include "unserding.h"
#include "ud-nifty.h"
#include "ud-sock.h"
#include "ud-logger.h"
#include "daemonise.h"

#if defined DEBUG_FLAG && !defined BENCHMARK
# include <assert.h>
# define UD_DEBUG(args...)	fprintf(stderr, args)
# define MAYBE_NOINLINE		__attribute__((noinline))
# define MAYBE_UNUSED		UNUSED
#else  /* !DEBUG_FLAG */
# define assert(...)
# define UD_DEBUG(args...)
# define MAYBE_UNUSED		UNUSED
# define MAYBE_NOINLINE
#endif	/* DEBUG_FLAG */

#if !defined ETH_MTU
/* mtu for ethernet */
# define ETH_MTU		(1492U)
#endif	/* !ETH_MTU */

typedef struct ctx_s *ctx_t;

struct ctx_s {
	/* socket used for forwarding */
	int dst;

	enum {
		PROTO_UDP,
		PROTO_TCP,
	} proto;
	const char *host;
	const char *port;

	/* router identity of the form RTR_xxx */
	uint32_t ident;
};


static void
ev_io_shut(EV_P_ ev_io *w)
{
	int fd = w->fd;

	ev_io_stop(EV_A_ w);

	shutdown(fd, SHUT_RDWR);
	close(fd);
	w->fd = -1;
	w->data = NULL;
	return;
}

static int
massage_conn(ctx_t ctx, const char *conn)
{
	char *port;

	ctx->proto = PROTO_UDP;
	for (char *p; (p = strstr(conn, "://")) != NULL;) {
		*p = '\0';

		if (!strcmp(conn, "udp")) {
			ctx->proto = PROTO_UDP;
		} else if (!strcmp(conn, "tcp")) {
			ctx->proto = PROTO_TCP;
		} else {
			fprintf(stderr, "cannot handle protocol %s\n", conn);
			return -1;
		}

		conn = p + 3;
		break;
	}

	if ((port = strrchr(conn, ':')) == NULL || strchr(port, ']') != NULL) {
		fprintf(stderr, "no port specified\n");
		return -1;
	}
	/* otherwise */
	*port++ = '\0';

	/* mash it all up */
	ctx->host = conn;
	ctx->port = port;
	return 0;
}

static uint32_t
make_router_id(void)
{
	uint32_t res = 0U;
	int s;

	/* mangle buf and put hex repr into ctx->ident */
	if ((s = open("/dev/urandom", O_RDONLY)) < 0) {

	} else if (read(s, &res, sizeof(res)) < (ssize_t)sizeof(res)) {

	} else {
		close(s);
	}
	return res;
}


static int
try_connect(struct addrinfo **aires)
{
	struct addrinfo *ai = *aires;

	for (int s; ai != NULL; ai = ai->ai_next, close(s), s = -1) {
		if ((s = socket(ai->ai_family, ai->ai_socktype, 0)) >= 0 &&
		    connect(s, ai->ai_addr, ai->ai_addrlen) >= 0) {
			*aires = ai;
			return s;
		}
	}
	return -1;
}

static int
ud_router_socket(ctx_t ctx)
{
        struct addrinfo *aires;
        struct addrinfo hints = {0};
	int s = 0;

	/* set up hints for gai */
        hints.ai_family = AF_UNSPEC;
	switch (ctx->proto) {
	case PROTO_UDP:
		hints.ai_socktype = SOCK_DGRAM;
		break;
	case PROTO_TCP:
		hints.ai_socktype = SOCK_STREAM;
		break;
	default:
		abort();
	}
        hints.ai_flags = 0;
#if defined AI_ADDRCONFIG
        hints.ai_flags |= AI_ADDRCONFIG;
#endif  /* AI_ADDRCONFIG */
#if defined AI_V4MAPPED
        hints.ai_flags |= AI_V4MAPPED;
#endif  /* AI_V4MAPPED */
        hints.ai_protocol = 0;

	if (getaddrinfo(ctx->host, ctx->port, &hints, &aires) < 0) {
		goto out;
	} else if (UNLIKELY((s = try_connect(&aires)) < 0)) {
		goto out;
	}

out:
	freeaddrinfo(aires);
	return s;
}


static void
rtr_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	char buf[1280];
	ssize_t nrd;
	ctx_t ctx = w->data;

	UD_DEBUG("rtr_cb\n");
	if ((nrd = recv(w->fd, buf, sizeof(buf), 0)) <= 0) {
		switch (ctx->proto) {
		case PROTO_UDP:
			break;
		case PROTO_TCP:
			ctx->dst = -1;
			ev_io_shut(EV_A_ w);
			break;
		default:
			abort();
		}
	}
	return;
}

static void
sub_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
#define MAX_RETR	(3)
#define RETR_SLEEP	(4U)
	char buf[ETH_MTU];
	ud_sock_t s = w->data;
	ctx_t ctx = s->data;
	int dst = ctx->dst;
	ssize_t nrd;

	UD_DEBUG("sub_cb\n");
	if ((nrd = recv(w->fd, buf, sizeof(buf), 0)) <= 0) {
		/* don't even bother */
		return;
	}
	/* redirect packet as is, no encapsulation or anything */
	for (size_t i = 0; i < MAX_RETR && send(dst, buf, nrd, 0) != nrd; i++) {
		usleep(RETR_SLEEP);
	}
	return;
}

static void
chk_cb(EV_P_ ev_check *w, int rev)
{
	static ev_io rtr[1];
	static time_t last_reco = 0U;
	ctx_t ctx = w->data;
	time_t now;

	UD_DEBUG("chk_cb\n");
	if (UNLIKELY(rev & EV_CUSTOM)) {
		/* we're going down :( */
		ev_io_shut(EV_A_ rtr);
		return;
	} else if (UNLIKELY(ctx->dst < 0 && (now = time(NULL)) > last_reco)) {
		/* just to make sure we don't reconnect too often */
		last_reco = now;

		if ((ctx->dst = ud_router_socket(ctx)) < 0) {
			perror("cannot obtain unserding router socket");
			return;
		}

		rtr->data = ctx;
		ev_io_init(rtr, rtr_cb, ctx->dst, EV_READ);
		ev_io_start(EV_A_ rtr);
	}
	return;
}

static void
sigall_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
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
#include "ud-router-clo.h"
#include "ud-router-clo.c"
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
	/* args */
	struct ud_args_info argi[1];
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_signal sigint_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_io *beef = NULL;
	size_t nbeef;
	ev_check chk[1];
	/* context we pass around */
	struct ctx_s ctx[1];
	/* business logic */
	int res = 0;
 
	/* parse the command line */
	if (ud_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1U) {
		ud_parser_print_help();
		res = 1;
		goto out;
	} else if (massage_conn(ctx, argi->inputs[0]) < 0) {
		res = 1;
		goto out;
	} else if (argi->daemonise_given && detach() < 0) {
		perror("daemonisation failed");
		res = 1;
		goto out;
	}

	/* open the log file */
	ud_openlog(argi->log_arg);

	/* fill in the rest of ctx */
	ctx->ident = make_router_id();

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigall_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigall_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);

	/* make some room for the control channel and the beef chans */
	nbeef = (argi->beef_given);
	beef = calloc(nbeef + 1, sizeof(*beef));

	{
		ud_sock_t s;

		if ((s = ud_socket((struct ud_sockopt_s){UD_SUB})) != NULL) {
			beef[nbeef].data = s;
			s->data = ctx;
			ev_io_init(beef + nbeef, sub_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef + nbeef);
		}
	}

	/* set up the one end, sub to unserding network */
	for (unsigned int i = 0; i < argi->beef_given; i++) {
		uint16_t port = (uint16_t)argi->beef_arg[i];
		ud_sock_t s;

		if ((s = ud_socket((struct ud_sockopt_s){
					UD_SUB,
					.port = port})) == NULL) {
			error(errno, "\
cannot initialise unserding socket, channel %hu", port);
			continue;
		}
		/* otherwise */
		beef[i].data = s;
		s->data = ctx;
		ev_io_init(beef + i, sub_cb, s->fd, EV_READ);
		ev_io_start(EV_A_ beef + i);
	}

	/* set up preparation */
	ctx->dst = -1;
	chk->data = ctx;
	ev_check_init(chk, chk_cb);
	ev_check_start(EV_A_ chk);

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* close the routed-to socket */
	chk_cb(EV_A_ chk, EV_CUSTOM);
	ev_check_stop(EV_A_ chk);

	/* detaching beef channels */
	for (unsigned int i = 0; i <= nbeef; i++) {
		ud_sock_t s;

		if (LIKELY((s = beef[i].data) != NULL)) {
			ev_io_stop(EV_A_ beef + i);
			ud_close(s);
		}
	}
	/* free beef resources */
	free(beef);

	/* destroy the default evloop */
	ev_default_destroy();

	/* close log resources */
	ud_closelog();
out:
	ud_parser_free(argi);
	return res;
}

/* ud-router.c ends here */
