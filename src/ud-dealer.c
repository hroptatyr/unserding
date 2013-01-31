/*** ud-dealer.c -- unserding router-dealer
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
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
#include "unserding.h"
#include "ud-nifty.h"
#include "ud-sock.h"
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
	ud_sock_t dst;

	enum {
		PROTO_UDP,
		PROTO_TCP,
	} proto;
	const char *host;
	const char *port;
};

struct dccp_conn_s {
	ev_io io[1];
	socklen_t sz;
	struct sockaddr_storage sa[1];
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

	if ((port = strrchr(conn, ':')) == NULL) {
		/* no host name, just the port */
		ctx->host = NULL;
		ctx->port = conn;
	} else if (strchr(port, ']') != NULL) {
		fprintf(stderr, "no port specified\n");
		return -1;
	} else {
		/* otherwise */
		*port++ = '\0';
		ctx->host = conn;
		ctx->port = port;
	}
	return 0;
}


static int
try_bind(struct addrinfo **aires)
{
	struct addrinfo *ai = *aires;

	for (int s; ai != NULL; ai = ai->ai_next, close(s), s = -1) {
		if ((s = socket(ai->ai_family, ai->ai_socktype, 0)) >= 0 &&
		    (setsock_reuseaddr(s), setsock_reuseport(s),
		     bind(s, ai->ai_addr, ai->ai_addrlen)) >= 0) {
			*aires = ai;
			return s;
		}
	}
	return -1;
}

static int
ud_dealer_socket(ctx_t ctx)
{
        struct addrinfo *aires;
        struct addrinfo hints = {0};
	const char *h;
	int s = -1;

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

	if (LIKELY((h = ctx->host) == NULL)) {
		h = "::";
	}

	if (getaddrinfo(h, ctx->port, &hints, &aires) < 0) {
		goto out;
	} else if (UNLIKELY((s = try_bind(&aires)) < 0)) {
		goto out;
	} else if (aires->ai_socktype == SOCK_DGRAM) {
		/* skip the listening step */
		;
	} else if (UNLIKELY(listen(s, MAX_DCCP_CONNECTION_BACK_LOG) < 0)) {
		close(s);
		s = -1;
		goto out;
	}

out:
	freeaddrinfo(aires);
	return s;
}


static void
dlr_data_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
#define MAX_RETR	(3U)
#define RETR_SLEEP	(4U)
	char buf[ETH_MTU];
	ctx_t ctx = w->data;
	ud_sock_t s = ctx->dst;
	int dst = s->fd;
	ssize_t nrd;

	UD_DEBUG("dlr_data_cb\n");
	if ((nrd = recv(w->fd, buf, sizeof(buf), 0)) <= 0) {
		/* don't even bother */
		goto clo;
	}

	/* redirect packet as is, no encapsulation or anything */
	for (size_t i = 0; i < MAX_RETR && send(dst, buf, nrd, 0) != nrd; i++) {
		usleep(RETR_SLEEP);
	}
clo:
	if (UNLIKELY(ctx->proto == PROTO_TCP)) {
		ev_io_shut(EV_A_ w);
	}
	return;
}

static void
dlr_tcp_cb(EV_P_ ev_io *w, int rev)
{
	static struct dccp_conn_s conns[8];
	static size_t next = 0;
	ctx_t ctx = w->data;
	int s;

	UD_DEBUG("dlr_tcp_cb\n");
	if (UNLIKELY(rev & EV_CUSTOM)) {
		/* going down */
		for (size_t i = 0; i < countof(conns); i++) {
			if (conns[i].io->fd > 0) {
				ev_io_shut(EV_A_ conns[i].io);
			}
		}
		return;
	}

	/* otherwise find us a free slot */
	if (conns[next].io->fd > 0) {
		ev_io_shut(EV_A_ conns[next].io);
	}

	conns[next].sz = sizeof(*conns[next].sa);
	if ((s = accept(w->fd, (void*)&conns[next].sa, &conns[next].sz)) < 0) {
		perror("cannot accept incoming connection");
		return;
	}

	conns[next].io->data = ctx;
	ev_io_init(conns[next].io, dlr_data_cb, s, EV_READ);
	ev_io_start(EV_A_ conns[next].io);
	if (++next >= countof(conns)) {
		next = 0;
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
#include "ud-dealer-clo.h"
#include "ud-dealer-clo.c"
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
 	ev_io dlr[1];
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

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigall_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	ev_signal_init(sigterm_watcher, sigall_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);

	/* set up the publishing end in the unserding network */
	if ((ctx->dst = ud_socket((struct ud_sockopt_s){
				UD_PUB,
				.port = (uint16_t)argi->beef_arg,
			})) == NULL) {
		perror("cannot initialise unserding socket");
		res = 1;
		goto out;
	}

	/* set up the dealer socket */
	{
		int s;

		if ((s = ud_dealer_socket(ctx)) < 0) {
			perror("cannot obtain unserding dealer socket");
			res = 1;
			goto clos;
		}

		dlr->data = ctx;
		switch (ctx->proto) {
		case PROTO_UDP:
			ev_io_init(dlr, dlr_data_cb, s, EV_READ);
			break;
		case PROTO_TCP:
			ev_io_init(dlr, dlr_tcp_cb, s, EV_READ);
			break;
		default:
			abort();
		}
		ev_io_start(EV_A_ dlr);
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	/* and off */
	ev_io_shut(EV_A_ dlr);

clos:
	ud_close(ctx->dst);

	/* destroy the default evloop */
	ev_default_destroy();

out:
	ud_parser_free(argi);
	return res;
}

/* ud-dealer.c ends here */
