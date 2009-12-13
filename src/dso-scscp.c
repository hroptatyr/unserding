/*** dso-scscp.c -- redirect to scscp servers
 *
 * Copyright (C) 2009 Sebastian Freundt
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

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "module.h"
#include "unserding.h"
#include "protocore.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-nifty.h"
#include "unserding-ctx.h"

#include <ev.h>
#undef EV_P
#define EV_P	struct ev_loop *loop __attribute__((unused))

static int sock_i, sock_s;
static struct sockaddr_storage srv;
static socklen_t srvlen = sizeof(srv);

/* link local network */
#define V6PFX			"fe80:"
//#define V6PFX			"2001:470:9dd3:ca05"

/* issel */
static const char scscp_srv_issel[] = V6PFX ":216:17ff:feb3:5eaa";
/* muck */
//static const char scscp_srv[] = V6PFX ":219:dbff:fed1:4da8";
/* stirling */
static const char scscp_srv_stirling[] = V6PFX ":219:dbff:fe63:577a";
static const char scscp_dev[] = "lan0";

#define BRAG_RATE	6.0

static ev_timer ALGN16(__wtimer);
static ev_io ALGN16(__wio);


static int
if_index(int s, const char *if_name)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		close(s);
		return -1;
	}
	return ifr.ifr_ifindex;
}

static int
scscp_connect(const char *scscp_srv)
{
	struct sockaddr_in6 *s6 = (void*)&srv;
	int s = -1;

	/* wipe sockaddr structure */
	memset(&srv, 0, sizeof(srv));
	/* get us a socket */
	if ((s = socket(PF_INET6, SOCK_STREAM, IPPROTO_IP)) < 0) {
		UD_DBGCONT("failed, socket() screwed up\n");
		return -1;
	}
	/* init him */
	s6->sin6_family = AF_INET6;
	s6->sin6_port = htons(26133);
	if (inet_pton(AF_INET6, scscp_srv, &s6->sin6_addr) < 0) {
		UD_DBGCONT("failed, inet_pton() screwed up\n");
		return -1;
	}
	s6->sin6_scope_id = if_index(s, scscp_dev);
	/* connect him */
	if (connect(s, (struct sockaddr*)(void*)&srv, srvlen) < 0) {
		UD_DBGCONT("failed, connect() screwed up\n");
		return -1;
	}
	return s;
}

static int negop = 0;
static const char negostr[] = "<?scscp version=\"1.3\" ?>\n";
static const char bullshit[] = "                   \n<?scscp ack ?>\n<?scscp start ?><OMOBJ/><?scscp end ?>\n\n                                                                                                                                                                                                                                                                                                                                                                                                       \n";
//static const char ackmsg[] = "                                                                                                                                                                                                                                                                                                                                \n<?scscp ack ?>\n";
static char ackmsg[4096];
//static const char ackmsg[] = "                   \n<?scscp ack ?>\n<?scscp start ?><OMOBJ/><?scscp end ?>\n\n                                                                                                                                                                                                                                                                                                                                                                                                       \n";

static void
inco_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ev_timer *wtimer = &__wtimer;
	char buf[4096];

	UD_DEBUG("they got back to us\n");
	if (read(w->fd, buf, sizeof(buf)) <= 0) {
		UD_DEBUG("no data, closing socket\n");
		ev_io_stop(EV_A_ w);
		if (w->fd == sock_i) {
			sock_i = -1;
		} else if (w->fd == sock_s) {
			sock_s = -1;
		}
		close(w->fd);
		return;
	}
	if (negop == 0) {
		write(w->fd, negostr, sizeof(negostr));
		negop++;
	} else if (negop == 1) {
		ev_timer_start(EV_A_ wtimer);
		negop++;
	} else {
		write(w->fd, bullshit, sizeof(bullshit));
	}
	return;
}

static void
ack_cb(EV_P_ ev_timer *UNUSED(w), int UNUSED(revents))
{
	UD_DEBUG("writing\n");
	if (sock_i >= 0) {
		write(sock_i, ackmsg, sizeof(ackmsg));
	} else {
		sock_i = scscp_connect(scscp_srv_issel);
	}
	if (sock_s >= 0) {
		write(sock_s, ackmsg, sizeof(ackmsg));
	} else {
		sock_s = scscp_connect(scscp_srv_stirling);
	}
	return;
}

static void
init_watchers(EV_P_ int s)
{
	ev_io *wio = &__wio;

	if (s < 0) {
		return;
	}

        /* initialise an io watcher, then start it */
        ev_io_init(wio, inco_cb, s, EV_READ);
        ev_io_start(EV_A_ wio);
	return;
}

static void
init_timer(EV_P)
{
	ev_timer *wtimer = &__wtimer;

        /* init the timer which is sending scscp acks all along */
        ev_timer_init(wtimer, ack_cb, 0.0, BRAG_RATE);
	/* not starting the timer just yet */

	//memset(ackmsg, 32, sizeof(ackmsg));
	//memcpy(ackmsg, init_watchers, 402);
	for (size_t i = 0; i < sizeof(ackmsg); i++) {
		ackmsg[i] = (char)rand();
	}
	return;
}


void
init(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG("mod/scscp: loading ...");
	/* connect to scscp and say ehlo */
	sock_i = scscp_connect(scscp_srv_issel);
	sock_s = scscp_connect(scscp_srv_stirling);
	/* set up the IO watcher and timer */
	init_watchers(ctx->mainloop, sock_i);
	init_watchers(ctx->mainloop, sock_s);

	init_timer(ctx->mainloop);
	UD_DBGCONT("loaded\n");
	return;
}

void
reinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/scscp: reloading ...done\n");
	return;
}

void
deinit(void *UNUSED(clo))
{
	UD_DEBUG("mod/scscp: unloading ...");
	close(sock_i);
	close(sock_s);
	sock_i = -1;
	sock_s = -1;
	UD_DBGCONT("done\n");
	return;
}

/* dso-scscp.c ends here */
