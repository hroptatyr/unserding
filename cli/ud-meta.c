/*** ud-meta.c -- meta querying
 *
 * Copyright (C) 2012 Sebastian Freundt
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

#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "unserding.h"
#include "protocore.h"

#if defined DEBUG_FLAG
# define META_DEBUG(args...)	fprintf(stderr, "[ud-meta]: " args)
# define META_STUP(arg)		fputc(arg, stderr)
#else  /* !DEBUG_FLAG */
# define META_DEBUG(args...)
# define META_STUP(arg)
#endif	/* DEBUG_FLAG */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define UD_CMD_QMETA	(0x7572)

struct meta_s {
	ud_chan_t ud;
	int epfd;
	int mcfd;
	int timeo;
};


static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("ud-meta: ", stderr);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

static void
prnt(const struct meta_s *UNUSED(ctx), ud_packet_t pkt)
{
	struct udpc_seria_s ser[1];

	/* reply packet should look like UI16, STR */
	udpc_seria_init(ser, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	while (1) {
		switch (udpc_seria_tag(ser)) {
			uint16_t idx;
		case UDPC_TYPE_UI16:
			idx = udpc_seria_des_ui16(ser);
			continue;
		case UDPC_TYPE_STR: {
			const char *str;
			size_t len;
			len = udpc_seria_des_str(ser, &str);
			fprintf(stdout, "%hu\t", idx);
			fwrite(str, sizeof(*str), len, stdout);
			fputc('\n', stdout);
			break;
		}
		default:
			goto out;
		}
	}
out:
	return;
}

static int
tv_diff(struct timeval *t1, struct timeval *t2)
{
	useconds_t res = (t2->tv_sec - t1->tv_sec) * 1000000;
	res += (t2->tv_usec - t1->tv_usec);
	return res / 1000;
}

static int
__qmeta_rpl_p(ud_packet_t pkt)
{
	return udpc_pkt_cmd(pkt) == UDPC_PKT_RPL(UD_CMD_QMETA);
}

static void
wait(const struct meta_s *ctx)
{
	struct epoll_event ev[1];
	struct timeval tv[2];
	char rpl[UDPC_PKTLEN];
	ssize_t nrd;

	gettimeofday(tv + 0, NULL);
	for (int mil = ctx->timeo;
	     mil >= 0 && epoll_wait(ctx->epfd, ev, 1, mil) > 0;
	     gettimeofday(tv + 1, NULL), mil -= tv_diff(tv + 0, tv + 1)) {

		while ((ev->events & EPOLLIN) &&
		       (nrd = read(ev->data.fd, rpl, sizeof(rpl))) > 0 &&
		       udpc_pkt_valid_p((ud_packet_t){nrd, rpl}) &&
		       __qmeta_rpl_p((ud_packet_t){nrd, rpl})) {
			prnt(ctx, (ud_packet_t){nrd, rpl});
		}
	}
	return;
}

static int
work(const struct meta_s *ctx, const char *const inp[], size_t len)
{
	char __pkt[UDPC_PKTLEN];
	struct udpc_seria_s ser[1];
	ud_packet_t pkt = {.pbuf = __pkt, .plen = sizeof(__pkt)};
	int pno = 0;

#define SEND_PACK						\
	pkt.plen = udpc_seria_msglen(ser) + UDPC_HDRLEN;	\
	ud_chan_send(ctx->ud, pkt)
#define MAKE_PACK							\
	udpc_make_pkt(pkt, 0, pno++, UD_CMD_QMETA);			\
	udpc_seria_init(ser, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen))

	MAKE_PACK;
	for (size_t i = 0; i < len; i++) {
		const char *sym = inp[i];
		long unsigned int idx;
		char *on;

		if ((idx = strtoul(sym, &on, 10)) && idx < 65536 && !*on) {
			/* query an index */
			if (udpc_seria_msglen(ser) + 4 > UDPC_PLLEN) {
				SEND_PACK;
				MAKE_PACK;
			}
			META_DEBUG("QI %lu\n", idx);
			udpc_seria_add_ui16(ser, idx);
		} else {
			/* query a symbol */
			size_t ssz = strlen(sym);

			if (udpc_seria_msglen(ser) + 2 + ssz > UDPC_PLLEN) {
				SEND_PACK;
				MAKE_PACK;
			}
			META_DEBUG("QS %s\n", sym);
			udpc_seria_add_str(ser, sym, ssz);
		}
	}
	/* send the final pack */
	SEND_PACK;

	/* wait for replies */
	wait(ctx);
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "ud-meta-clo.h"
#include "ud-meta-clo.c"
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
	struct meta_args_info argi[1];
	struct meta_s ctx[1];
	int res = 0;

	if (meta_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}
	/* make me an argument */
	ctx->timeo = argi->timeout_arg;

	/* obtain a new handle, somehow we need to use the port number innit? */
	ctx->ud = ud_chan_init(argi->beef_given ? argi->beef_arg : 8584);

	/* also accept connections on that socket */
	if ((ctx->mcfd = ud_chan_init_mcast(ctx->ud)) < 0) {
		error(0, "cannot instantiate mcast for meta data queries");
	} else if ((ctx->epfd = epoll_create(1)) < 0) {
		error(0, "cannot instantiate epoll on ud-meta socket");
	} else {
		struct epoll_event ev[1];

		ev->events = EPOLLIN;
		ev->data.fd = ctx->mcfd;
		epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->mcfd, ev);
	}

	/* init the serialiser */
	if (work(ctx, argi->inputs, argi->inputs_num) < 0) {
		res = 1;
	}

	/* close epoll */
	close(ctx->epfd);

	/* and the unserding beef handle */
	ud_chan_fini(ctx->ud);

out:
	/* free up command line parser resources */
	meta_parser_free(argi);
	return res;
}

/* ud-meta.c ends here */
