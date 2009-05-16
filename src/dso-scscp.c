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
#include "unserding-ctx.h"
#define UNSERSRV
#include "unserding-dbg.h"
#include "unserding-nifty.h"

#include <ev.h>

static int sock;
static struct sockaddr_storage srv;
static socklen_t srvlen = sizeof(srv);

/* issel */
//static const char scscp_srv[] = "fe80::216:17ff:feb3:5eaa";
/* muck */
//static const char scscp_srv[] = "fe80::219:dbff:fed1:4da8";
/* stirling */
static const char scscp_srv[] = "fe80::219:dbff:fe63:577a";
static const char scscp_dev[] = "lan0";

#define BRAG_RATE	0.2

static ev_timer ALGN16(__wtimer);
static ev_io ALGN16(__wio);


static int
if_index(int s, const char *if_name)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

	if (ioctl(s, SIOCGIFINDEX, &ifr) < 0) {
		close(sock);
		return -1;
	}
	return ifr.ifr_ifindex;
}

static int
scscp_connect(void)
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
	if ((s6->sin6_scope_id = if_index(s, scscp_dev)) < 0) {
		UD_DBGCONT("failed, if_index() screwed up\n");
		return -1;
	}
	/* connect him */
	if (connect(s, (struct sockaddr*)(void*)&srv, srvlen) < 0) {
		UD_DBGCONT("failed, connect() screwed up\n");
		return -1;
	}
	return s;
}

static int negop = 0;
static const char negostr[] = "<?scscp version=\"1.3\" ?>\n";
static const char bullshit[] = "<?scscp info text=\"debug\" ?>\n";
static const char ackmsg[] = "<?scscp ack ?>\n";

static void
inco_cb(EV_P_ ev_io *w, int revents)
{
	ev_timer *wtimer = &__wtimer;
	char buf[4096];
	ssize_t nread;

	if ((nread = read(sock, buf, sizeof(buf))) <= 0) {
		UD_DEBUG("no data, closing socket\n");
		ev_io_stop(EV_A_ w);
		close(w->fd);
		return;
	}
	if (negop == 0) {
		write(sock, negostr, sizeof(negostr));
		negop++;
	} else if (negop == 1) {
		ev_timer_start(EV_A_ wtimer);
		negop++;
	} else {
		write(sock, bullshit, sizeof(bullshit));
	}
	return;
}

static void
ack_cb(EV_P_ ev_timer *w, int revents)
{
	write(sock, ackmsg, sizeof(ackmsg));
	return;
}

static void
init_watchers(EV_P_ int s)
{
	ev_io *wio = &__wio;
	ev_timer *wtimer = &__wtimer;

        /* initialise an io watcher, then start it */
        ev_io_init(wio, inco_cb, s, EV_READ);
        ev_io_start(EV_A_ wio);

        /* init the timer which is sending scscp acks all along */
        ev_timer_init(wtimer, ack_cb, 0.0, BRAG_RATE);
	/* not starting the timer just yet */
	return;
}


void
init(void *clo)
{
	ud_ctx_t ctx = clo;

	UD_DEBUG("mod/scscp: loading ...");
	/* connect to scscp and say ehlo */
	if ((sock = scscp_connect()) < 0) {
		return;
	}
	/* set up the IO watcher and timer */
	init_watchers(ctx->mainloop, sock);

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
	UD_DEBUG("mod/scscp: unloading ...");
	close(sock);
	sock = -1;
	UD_DBGCONT("done\n");
	return;
}

/* dso-cli.c ends here */
