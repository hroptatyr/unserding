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
#include <stddef.h>
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

#include <ev.h>

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

#define BRAG_RATE	26.0

static const char negostr[] = "<?scscp version=\"1.3\" ?>\n";
static const char bullshit[] = "                   \n<?scscp ack ?>\n"
	"<?scscp start ?>\n"
	"<?xml version=\"1.0\"?>\n"
	"<OM:OMOBJ xmlns:OM=\"http://www.openmath.org/OpenMath\"><OM:OMA><OMS cd=\"scscp2\" name=\"get_allowed_heads\"/></OM:OMA></OM:OMOBJ>\n"
	"<?scscp end ?>\n\n                                                                                                                                                                                                                                                                                                                                                                                                       \n";


typedef struct sock_ctx_s *sock_ctx_t;
struct sock_ctx_s {
	const char *host;
	const char *addr;
	int state;
	int sock;
	ev_io ALGN16(wio);
	ev_timer ALGN16(wtimer);
};

static struct sock_ctx_s issel;
static struct sock_ctx_s stirling;

static void init_spammer(EV_P_ sock_ctx_t ctx);

static inline sock_ctx_t
sock_ctx_from_evio(ev_io *io)
{
	return (void*)((char*)io - offsetof(struct sock_ctx_s, wio));
}

static inline sock_ctx_t
sock_ctx_from_evtimer(ev_timer *ti)
{
	return (void*)((char*)ti - offsetof(struct sock_ctx_s, wtimer));
}


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
scscp_connect(sock_ctx_t ctx)
{
	static struct sockaddr_storage srv;
	static socklen_t srvlen = sizeof(srv);
	struct sockaddr_in6 *s6 = (void*)&srv;
	int s = -1;

	/* wipe sockaddr structure */
	memset(&srv, 0, sizeof(srv));
	UD_DEBUG("(re)connecting to %s (%s)...", ctx->host, ctx->addr);
	/* get us a socket */
	if ((s = socket(PF_INET6, SOCK_STREAM, IPPROTO_IP)) < 0) {
		UD_DBGCONT("failed, socket() screwed up\n");
		return -1;
	}
	/* init him */
	s6->sin6_family = AF_INET6;
	s6->sin6_port = htons(26133);
	if (inet_pton(AF_INET6, ctx->addr, &s6->sin6_addr) < 0) {
		UD_DBGCONT("failed, inet_pton() screwed up\n");
		return -1;
	}
	if ((s6->sin6_scope_id = if_index(s, scscp_dev)) < 0) {
		UD_DBGCONT("failed, if_index() screwed up\n");
		return -1;
	}
	/* connect him */
	if (connect(s, (struct sockaddr*)(void*)&srv, srvlen) < 0) {
		UD_DBGCONT("failed, connect() screwed up\n");
		return -1;
	}
	UD_DBGCONT("succeeded\n");
	return s;
}

static void
inco_cb(EV_P_ ev_io *w, int revents)
{
	sock_ctx_t ctx = sock_ctx_from_evio(w);
	char buf[4096];
	ssize_t nread;

	UD_DEBUG("%s got back to us\n", ctx->host);
	if ((nread = read(w->fd, buf, sizeof(buf))) <= 0) {
		UD_DEBUG("no data, closing socket %s\n", ctx->host);
		ev_io_stop(EV_A_ w);
		close(w->fd);
		ctx->sock = -1;
		ctx->state = 0;
		return;
	}
	if (ctx->state == 0) {
		UD_DEBUG("negotiationg %s\n", ctx->host);
		write(w->fd, negostr, sizeof(negostr));
		ctx->state++;
	} else if (ctx->state == 1) {
		UD_DEBUG("nego succeeded for %s\n", ctx->host);
		ctx->state++;
	} else {
		/* nothing */
		;
	}
	return;
}

static void
ack_cb(EV_P_ ev_timer *w, int revents)
{
	sock_ctx_t ctx = sock_ctx_from_evtimer(w);

	if (ctx->sock >= 0) {
		UD_DEBUG("writing %s\n", ctx->host);
		write(ctx->sock, bullshit, sizeof(bullshit));
	} else {
		init_spammer(EV_A_ ctx);
	}
	return;
}

static void
init_spammer(EV_P_ sock_ctx_t ctx)
{
	ev_io *wio = &ctx->wio;

	if ((ctx->sock = scscp_connect(ctx)) < 0) {
		return;
	}
        /* initialise an io watcher, then start it */
        ev_io_init(wio, inco_cb, ctx->sock, EV_READ);
        ev_io_start(EV_A_ wio);
	return;
}

static void
init_timer(EV_P_ sock_ctx_t ctx)
{
	ev_timer *wtimer = &ctx->wtimer;

	/* init the timer */
        ev_timer_init(wtimer, ack_cb, BRAG_RATE, BRAG_RATE);
	ev_timer_start(EV_A_ wtimer);
	return;
}

static void
spam(EV_P_ sock_ctx_t ctx, const char *addr, const char *host)
{
	ctx->state = 0;
	ctx->host = host;
	ctx->addr = addr;
	ctx->sock = -1;
	init_spammer(EV_A, ctx);
	init_timer(EV_A, ctx);
	return;
}

static void
unspam(EV_P_ sock_ctx_t ctx)
{
	ev_timer_stop(EV_A_ &ctx->wtimer);
	ev_io_stop(EV_A_ &ctx->wio);
	close(ctx->sock);
	ctx->sock = -1;
	ctx->state = -1;
	return;
}


void
init(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG("mod/scscp: loading ...");
	/* connect to scscp and say ehlo */
	spam(ctx->mainloop, &issel, scscp_srv_issel, "issel");
	spam(ctx->mainloop, &stirling, scscp_srv_stirling, "stirling");
	UD_DBGCONT("loaded\n");
	return;
}

void
reinit(void *clo)
{
	UD_DEBUG("mod/scscp: reloading ...done\n");
	return;
}

void
deinit(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG("mod/scscp: unloading ...");
	unspam(ctx->mainloop, &issel);
	unspam(ctx->mainloop, &stirling);
	UD_DBGCONT("done\n");
	return;
}

/* dso-cli.c ends here */
