/*** testcli.c -- unserding network service (client)
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
#include "protocore.h"
#include "catalogue.h"

#include <pfack/instruments.h>

#if defined TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else  /* !TIME_WITH_SYS_TIME */
# if defined HAVE_SYS_TIME_H
#  include <sys/time.h>
# elif defined HAVE_TIME_H
#  include <time.h>
# endif
#endif

static inline ud_convo_t __attribute__((always_inline, gnu_inline))
udpc_pkt_cno(const ud_packet_t pkt)
{
	const uint32_t *tmp = (void*)pkt.pbuf;
	uint32_t all = ntohl(tmp[0]);
	/* yes ntoh conversion!!! */
	return (ud_convo_t)(all >> 24);
}

static inline ud_pkt_no_t __attribute__((always_inline, gnu_inline))
udpc_pkt_pno(const ud_packet_t pkt)
{
	const uint32_t *tmp = (void*)pkt.pbuf;
	uint32_t all = ntohl(tmp[0]);
	/* yes ntoh conversion!!! */
	return (ud_pkt_no_t)(all & (0xffffff));
}

static inline ud_pkt_cmd_t __attribute__((always_inline, gnu_inline))
udpc_pkt_cmd(const ud_packet_t pkt)
{
	const uint16_t *tmp = (void*)pkt.pbuf;
	/* yes ntoh conversion!!! */
	return ntohs(tmp[2]);
}

static inline void __attribute__((always_inline, gnu_inline))
udpc_print_pkt(const ud_packet_t pkt)
{
	printf(":len %04x :cno %02x :pno %06x :cmd %04x :mag %04x\n",
	       (unsigned int)pkt.plen,
	       udpc_pkt_cno(pkt), udpc_pkt_pno(pkt), udpc_pkt_cmd(pkt),
	       ntohs(((const uint16_t*)pkt.pbuf)[3]));
	return;
}

static inline void
duration(struct timespec *res, struct timespec *sta, struct timespec *end)
{
	if (end->tv_nsec >= sta->tv_nsec) {
		res->tv_nsec = end->tv_nsec - sta->tv_nsec;
		res->tv_sec = end->tv_sec - sta->tv_sec;
	} else {
		res->tv_nsec = 1000000000 + end->tv_nsec - sta->tv_nsec;
		res->tv_sec = end->tv_sec - sta->tv_sec - 1;
	}
}

int
main (void)
{
	struct ud_handle_s __hdl;
	struct timespec sta, end, diff;
#if 0
	char buf[UDPC_SIMPLE_PKTLEN];
	ud_packet_t pkt = {sizeof(buf), buf};
	ud_convo_t cno;

	/* obtain us a new handle */
	init_unserding_handle(&__hdl);

	for (int i = 0; i < 1000; i++) {
		cno = ud_send_simple(&__hdl, UDPC_PKT_HY);
		clock_gettime(CLOCK_REALTIME, &sta);
		
		pkt.plen = sizeof(buf);
		ud_recv_convo(&__hdl, &pkt, 200, cno);
		clock_gettime(CLOCK_REALTIME, &end);

		duration(&diff, &sta, &end);
		fprintf(stderr, "wallclock %lu.%09ld\n",
			diff.tv_sec, diff.tv_nsec);
	}

	/* free the handle */
	free_unserding_handle(&__hdl);

	return 0;
#elif 1
	/* obtain us a new handle */
	init_unserding_handle(&__hdl);

	for (int i = 0; i < 1000; i++) {
		void *bar;

		clock_gettime(CLOCK_REALTIME, &sta);
		bar = ud_cat_ls_by_gaid(&__hdl, 2147483797);
		clock_gettime(CLOCK_REALTIME, &end);

		duration(&diff, &sta, &end);
		fprintf(stderr, "wallclock %lu.%09ld  :instr ",
			diff.tv_sec, diff.tv_nsec);
		print_instr(stderr, bar);
		fputc('\n', stderr);
	}

	/* free the handle */
	free_unserding_handle(&__hdl);
	return 0;

#elif 1
	volatile int a = 0;

	for (int i = 0; i < 10000000; i++) {
		a += ud_tag_from_s(":class");
		a += ud_tag_from_s(":name");
		a += ud_tag_from_s(":price");
		a += ud_tag_from_s(":attr");
	}
	return a;
#endif
}

/* unserding.c ends here */
