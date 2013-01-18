/*** libstuff.c -- unserding library definitions, old API
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
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
#if defined HAVE_ERRNO_H
# include <errno.h>
#endif	/* HAVE_ERRNO_H */

/* our master include */
#include "unserding.h"
#include "unserding-nifty.h"
#include "protocore.h"
#include "protocore-private.h"
#include "ud-sock.h"
#include "epoll-helpers.h"

#include "svc-pong.h"

#if defined DEBUG_FLAG
# define UD_DEBUG_SENDRECV(args...)		\
	fprintf(stderr, "[libunserding/sendrecv] " args)
/* we could define the PKTTRAF debug flag but it's seriously slow */
#else  /* !DEBUG_FLAG */
# define UD_DEBUG_SENDRECV(args...)
# undef DEBUG_PKTTRAF_FLAG
#endif	/* DEBUG_FLAG */

/**
 * Timeout in milliseconds. */
#define UD_SENDRECV_TIMEOUT	10
#define UDP_MULTICAST_TTL	16
#define SOCK_INVALID		(int)0xffffff

static inline void
ud_sockaddr_set_port(ud_sockaddr_t sa, uint16_t port)
{
	sa->sa6.sin6_port = htons(port);
	return;
}


static inline void
ud_chan_set_port(struct ud_chan_s *c, uint16_t port)
{
	ud_sockaddr_set_port(&c->sa, port);
	return;
}

static inline void
ud_chan_set_svc(struct ud_chan_s *c)
{
	c->sa.sa6.sin6_family = AF_INET6;
	/* we pick link-local here for simplicity */
	inet_pton(AF_INET6, UD_MCAST6_LINK_LOCAL, &c->sa.sa6.sin6_addr);
	/* set the flowinfo */
	c->sa.sa6.sin6_flowinfo = 0;
	return;
}

static void
fiddle_with_mtu(int __attribute__((unused)) s)
{
#if defined IPV6_PATHMTU
	struct ip6_mtuinfo mtui;
	socklen_t mtuilen = sizeof(mtui);
#endif	/* IPV6_PATHMTU */

#if defined IPV6_USE_MIN_MTU
	/* use minimal mtu */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU, &opt, sizeof(opt));
#endif
#if defined IPV6_DONTFRAG
	/* rather drop a packet than to fragment it */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_DONTFRAG, &opt, sizeof(opt));
#endif
#if defined IPV6_RECVPATHMTU
	/* obtain path mtu to send maximum non-fragmented packet */
	opt = 1;
	setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU, &opt, sizeof(opt));
#endif
#if defined IPV6_PATHMTU
	/* obtain current pmtu */
	if (getsockopt(s, IPPROTO_IPV6, IPV6_PATHMTU, &mtui, &mtuilen) < 0) {
		perror("could not obtain pmtu");
	} else {
		fprintf(stderr, "pmtu is %d\n", mtui.ip6m_mtu);
	}
#endif
	return;
}

static int
mcast6_init(struct ud_chan_s *tgt)
{
#if defined IPPROTO_IPV6
	int opt = 0;

	/* try v6 first */
	if ((tgt->sock = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP)) < 0) {
		tgt->sock = SOCK_INVALID;
		return -1;
	}

#if defined IPV6_V6ONLY
	opt = 1;
	setsockopt(tgt->sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif	/* IPV6_V6ONLY */

	fiddle_with_mtu(tgt->sock);
	ud_chan_set_svc(tgt);
	return 0;

#else  /* !IPPROTO_IPV6 */
out:
	return -1;
#endif	/* IPPROTO_IPV6 */
}

/* pseudo constructor, singleton */
static ep_ctx_t
epoll_guts(void)
{
	static struct ep_ctx_s gepg = FLEX_EP_CTX_INITIALISER(4);

	if (UNLIKELY(gepg.sock == -1)) {
		init_ep_ctx(&gepg, gepg.nev);
	}
	return &gepg;
}

static void
free_epoll_guts(void)
{
	ep_ctx_t epg = epoll_guts();
	free_ep_ctx(epg);
	return;
}

typedef struct __convo_flt_s {
	ud_packet_t *pkt;
	ud_convo_t cno;
} *__convo_flt_t;

static bool
__pkt_our_convo_p(const ud_packet_t pkt, ud_const_sockaddr_t UNUSED(s), void *c)
{
	__convo_flt_t flt = c;
	if (UDPC_PKT_INVALID_P(pkt)) {
		return false;
	} else if (udpc_pkt_cno(pkt) == flt->cno) {
		size_t sz = pkt.plen < flt->pkt->plen
			? pkt.plen
			: flt->pkt->plen;
		memcpy(flt->pkt->pbuf, pkt.pbuf, sz);
		return false;
	}
	return true;
}

static int
fiddle_with_timeout(ud_handle_t hdl, int timeout)
{
	if (timeout > 0) {
		return timeout;
	}
	return 2 * hdl->mart ?: 1;
}


/* public funs */
ssize_t
ud_send_raw(ud_handle_t hdl, ud_packet_t pkt)
{
	int s = ud_handle_sock(hdl);

	if (sizeof(hdl->sa) != sizeof(hdl->sa.sas)) {
		abort();
	}

	/* always send to the mcast addresses */
	return sendto(s, pkt.pbuf, pkt.plen, 0, &hdl->sa.sa, sizeof(hdl->sa));
}

ssize_t
ud_recv_raw(ud_handle_t hdl, ud_packet_t pkt, int timeout)
{
	int s = ud_handle_sock(hdl);
	int nfds;
	ep_ctx_t epg = epoll_guts();

	if (UNLIKELY(UDPC_PKT_INVALID_P(pkt))) {
		return -1;
	}

	/* wait for events */
	ep_prep_reader(epg, s);
	timeout = fiddle_with_timeout(hdl, timeout);
	nfds = ep_wait(epg, timeout);
	/* no need to loop atm, nfds can be 0 or 1 */
	if (LIKELY(nfds != 0)) {
	/* otherwise NFDS was 1 and it MUST be our socket */
		pkt.plen = recvfrom(s, pkt.pbuf, pkt.plen, 0, NULL, 0);
#if defined DEBUG_PKTTRAF_FLAG
		udpc_print_pkt(BUF_PACKET(buf));
#endif	/* DEBUG_PKTTRAF_FLAG */
	} else {
		/* nothing received */
		pkt.plen = 0;
	}
	ep_fini(epg, s);
	return pkt.plen;
}

void
ud_recv_convo(ud_handle_t hdl, ud_packet_t *pkt, int to, ud_convo_t cno)
{
	struct __convo_flt_s flt = {.pkt = pkt, .cno = cno};
	ud_subscr_raw(hdl, to, __pkt_our_convo_p, &flt);
	return;
}

/* subscriptions, basically receivers with callbacks */
void
ud_subscr_raw(ud_handle_t hdl, int timeout, ud_subscr_f cb, void *clo)
{
	int s = ud_handle_sock(hdl);
	int nfds;
	ssize_t nread;
	static __thread char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);
	union ud_sockaddr_u __sa;
	socklen_t lsa = sizeof(__sa);
	struct sockaddr *sa = (void*)&__sa.sa;
	ep_ctx_t epg = epoll_guts();

	/* wait for events */
	ep_prep_reader(epg, s);
	timeout = fiddle_with_timeout(hdl, timeout);
	do {
		nfds = ep_wait(epg, timeout);
		/* no need to loop atm, nfds can be 0 or 1 */
		if (UNLIKELY(nfds == 0)) {
			/* nothing received */
			memset(buf, 0, sizeof(buf));
			pkt.plen = 0;
		} else {
			/* otherwise NFDS was 1 and it MUST be our socket */
			nread = recvfrom(s, buf, sizeof(buf), 0, sa, &lsa);
			pkt.plen = nread;
		}
	} while ((*cb)(pkt, (void*)sa, clo));
	ep_fini(epg, s);
	return;
}


/* protocol funs */
ud_convo_t
ud_send_simple(ud_handle_t hdl, ud_pkt_cmd_t cmd)
{
	ud_convo_t cno = ud_handle_convo(hdl);
	char buf[UDPC_PKTLEN];
	ud_packet_t pkt = BUF_PACKET(buf);

	udpc_make_pkt(pkt, cno, /*pno*/0, cmd);
	ud_send_raw(hdl, pkt);
	hdl->convo++;
	return cno;
}


void
init_unserding_handle(ud_handle_t hdl, int pref_fam, bool negop)
{
	struct ud_chan_s c[1];

	/* initialise our sockaddr structure upfront */
	memset(&c->sa, 0, sizeof(c->sa));
	/* transport protocol independence */
	switch (pref_fam) {
	default:
	case PF_UNSPEC:
		if (mcast6_init(c) != SOCK_INVALID) {
			break;
		}
	case PF_INET:
		hdl->sock = -1;
		break;
	case PF_INET6:
		hdl->sock = mcast6_init(c);
		break;
	}
	/* set UD_NETWORK_SVC as channel */
	ud_chan_set_port(c, UD_NETWORK_SERVICE);
	/* operate in non-blocking mode */
	setsock_nonblock(c->sock);

	/* copy from C to HDL */
	hdl->sock = c->sock;
	hdl->sa = c->sa;
	hdl->convo = 0;
	if (negop) {
		/* fiddle with the mart slot */
		hdl->mart = UD_SENDRECV_TIMEOUT;
		ud_svc_nego_score(hdl, UD_SENDRECV_TIMEOUT);
	} else {
		hdl->mart = UD_SENDRECV_TIMEOUT;
	}
	return;
}

void
free_unserding_handle(ud_handle_t hdl)
{
	int s = ud_handle_sock(hdl);

	if (LIKELY(s != SOCK_INVALID)) {
		/* and kick the socket */
		shutdown(s, SHUT_RDWR);
		close(s);
		hdl->sock = SOCK_INVALID;
	}
	/* also free the epoll cruft */
	free_epoll_guts();
	return;
}

/* libstuff.c ends here */
