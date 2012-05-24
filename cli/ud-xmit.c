/*** ud-xmit.c -- transmission of ute files through unserding
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

#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>
/* for gettimeofday() */
#include <sys/time.h>
#include <sys/epoll.h>
#include <uterus.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect(!!(_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect(!!(_x), 0)
#endif
#if !defined countof
# define countof(x)	(sizeof(x) / sizeof(*(x)))
#endif	/* !countof */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#if defined DEBUG_FLAG
# define XMIT_DEBUG(args...)	fprintf(stderr, args)
# define XMIT_STUP(arg)		fputc(arg, stderr)
#else  /* !DEBUG_FLAG */
# define XMIT_DEBUG(args...)
# define XMIT_STUP(arg)
#endif	/* DEBUG_FLAG */

#define UD_CMD_QMETA	(0x7572)
#define PKT(x)		(ud_packet_t){sizeof(x), x}

struct xmit_s {
	ud_chan_t ud;
	utectx_t ute;
	float speed;
	bool restampp;
	int epfd;
	int mcfd;
};

static jmp_buf jb;


static void
handle_sigint(int signum)
{
	longjmp(jb, signum);
	return;
}

static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	fputs("[ud-xmit]: ", stderr);
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

#if !defined UTE_ITER
# define UTE_ITER(i, __ctx)						\
	for (size_t __i = 0, __tsz;					\
	     __i < ute_nticks(__ctx); __i += __tsz)			\
		for (scom_t i = ute_seek(__ctx, __i); i;		\
		     __tsz = scom_tick_size(i), i = 0)
#endif	/* !UTE_ITER */

static unsigned int pno = 0;
static size_t nt = 0;

static inline void
udpc_seria_add_scom(udpc_seria_t sctx, scom_t s, size_t len)
{
	memcpy(sctx->msg + sctx->msgoff, s, len);
	sctx->msgoff += len;
	return;
}

static useconds_t
tv_diff(struct timeval *t1, struct timeval *t2)
{
	useconds_t res = (t2->tv_sec - t1->tv_sec) * 1000000;
	res += (t2->tv_usec - t1->tv_usec);
	return res;
}

static void
party_deser(const struct xmit_s *ctx, ud_packet_t pkt)
{
	struct udpc_seria_s ser[2];
	char rpl[UDPC_PKTLEN];
	size_t nsyms = ute_nsyms(ctx->ute);

	/* make a reply packet */
	memcpy(rpl, pkt.pbuf, pkt.plen);
	udpc_make_rpl_pkt(PKT(rpl));

	/* get the serialisers and deserialisers ready */
	udpc_seria_init(ser, UDPC_PAYLOAD(pkt.pbuf), UDPC_PAYLLEN(pkt.plen));
	udpc_seria_init(ser + 1, UDPC_PAYLOAD(rpl), UDPC_PAYLLEN(sizeof(rpl)));

	for (uint8_t tag; (tag = udpc_seria_tag(ser)); ) {
		switch (tag) {
		case UDPC_TYPE_UI16: {
			uint16_t idx = udpc_seria_des_ui16(ser);
			const char *sym;

			if ((sym = ute_idx2sym(ctx->ute, idx))) {
				udpc_seria_add_ui16(ser + 1, idx);
				udpc_seria_add_str(ser + 1, sym, strlen(sym));
				XMIT_STUP('!');
			}
			break;
		}
		case UDPC_TYPE_STR: {
			char sym[64];
			uint16_t idx;
			size_t len;

			len = udpc_seria_des_str_into(sym, sizeof(sym), ser);
			if ((idx = ute_sym2idx(ctx->ute, sym)) <= nsyms) {
				udpc_seria_add_ui16(ser + 1, idx);
				udpc_seria_add_str(ser + 1, sym, len);
				XMIT_STUP('!');
			}
			break;
		}
		default:
			break;
		}
	}
	if (udpc_seria_msglen(ser + 1)) {
		ud_packet_t rplpkt = {
			.pbuf = rpl,
			.plen = udpc_seria_msglen(ser + 1) + UDPC_HDRLEN,
		};
		ud_chan_send(ctx->ud, rplpkt);
	}
	return;
}

static int
__qmetap(ud_packet_t pkt)
{
	return udpc_pkt_cmd(pkt) == UD_CMD_QMETA;
}

static void
party(const struct xmit_s *ctx, useconds_t tm)
{
	struct epoll_event ev[1];
	struct timeval tv[2];
	int mil = tm / 1000;
	int mic = tm % 1000;

	gettimeofday(tv + 0, NULL);
	while (epoll_wait(ctx->epfd, ev, 1, mil) > 0) {
		char inq[UDPC_PKTLEN];
		ssize_t nrd;
		useconds_t elps;

		/* otherwise be nosey and look at the packet */
		if ((ev->events & EPOLLIN) == 0) {
			/* must be HUP or ERR, don't bother reading it */
			;
		} else if ((nrd = read(ev->data.fd, inq, sizeof(inq))) <= 0) {
			;
		} else if (!udpc_pkt_valid_p((ud_packet_t){nrd, inq})) {
			;
		} else if (!__qmetap((ud_packet_t){nrd, inq})) {
			;
		} else {
			XMIT_STUP('?');
			party_deser(ctx, (ud_packet_t){nrd, inq});
			XMIT_STUP('\n');
		}

		/* check how long it took us */
		gettimeofday(tv + 1, NULL);
		elps = tv_diff(tv + 0, tv + 1);
		if (tm > elps + 1000) {
			mil = (tm - elps) / 1000 - 1;
			mic = (tm - elps) % 1000;
		} else if (tm > elps) {
			mil = 0;
			mic = (tm - elps);
		}
	}
	usleep(mic);
	return;
}

/* ute services come in 2 flavours little endian "ut" and big endian "UT" */
#define UTE_CMD_LE	0x7574
#define UTE_CMD_BE	0x5554
#if defined WORDS_BIGENDIAN
# define UTE_CMD	UTE_CMD_BE
#else  /* !WORDS_BIGENDIAN */
# define UTE_CMD	UTE_CMD_LE
#endif	/* WORDS_BIGENDIAN */

static void
work(const struct xmit_s *ctx)
{
	struct udpc_seria_s ser[1];
	static char buf[UDPC_PKTLEN];
	static ud_packet_t pkt = {0, buf};
	time_t reft = 0;
	unsigned int refm = 0;
	const unsigned int speed = (unsigned int)(1000 * ctx->speed);

#define RESET_SER						\
	udpc_make_pkt(pkt, 0, pno++, UTE_CMD);			\
	udpc_seria_init(ser, UDPC_PAYLOAD(buf), UDPC_PLLEN)

	/* initial set up of pkt */
	RESET_SER;

	UTE_ITER(ti, ctx->ute) {
		time_t stmp = scom_thdr_sec(ti);
		unsigned int msec = scom_thdr_msec(ti);
		size_t plen;
		char status;

		if (UNLIKELY(!reft)) {
			/* singleton */
			reft = stmp;
		}
		/* disseminate */
		plen = udpc_seria_msglen(ser);
		if (((stmp > reft || msec > refm) && (status = '!', plen)) ||
		    (status = '/', plen + scom_byte_size(ti) > UDPC_PLLEN)) {
			pkt.plen = UDPC_HDRLEN + plen;
			XMIT_STUP(status);
			ud_chan_send(ctx->ud, pkt);
			XMIT_STUP('\n');
			/* reset ser */
			RESET_SER;
		}
		/* sleep, well maybe */
		if (stmp > reft || msec > refm) {
			useconds_t slp;

			/* and party hard for some microseconds */
			slp = ((stmp - reft) * 1000 + msec - refm) * speed;
			for (unsigned int i = slp, j = 0; i; i /= 2, j++) {
				XMIT_STUP('0' + (j % 10));
			}
			XMIT_STUP('\n');

			party(ctx, slp);

			refm = msec;
			reft = stmp;
		}
		/* add the scom in question to the pool */
		XMIT_STUP('+');
		{
			scom_thdr_t sto = (void*)(ser->msg + ser->msgoff);
			size_t bs = scom_byte_size(ti);

			/* add the original guy */
			udpc_seria_add_scom(ser, ti, bs);

			if (ctx->restampp) {
				struct timeval now[1];

				gettimeofday(now, NULL);
				/* replace the time stamp */
				sto->sec = now->tv_sec;
				sto->msec = now->tv_usec / 1000;
			}
		}
		nt++;
	}
	if (udpc_seria_msglen(ser)) {
		pkt.plen = UDPC_HDRLEN + udpc_seria_msglen(ser);
		XMIT_STUP('/');
		ud_chan_send(ctx->ud, pkt);
		XMIT_STUP('\n');
	}
	return;
}

static int
pre_work(const struct xmit_s *ctx)
{
	return 0;
}

static int
post_work(const struct xmit_s *ctx)
{
	return 0;
}

static int
rebind_chan(ud_chan_t ch)
{
	union ud_sockaddr_u sa;
	socklen_t len = sizeof(sa);
	getsockname(ch->sock, &sa.sa, &len);
	return bind(ch->sock, &sa.sa, sizeof(sa));
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "ud-xmit-clo.h"
#include "ud-xmit-clo.c"
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
	struct gengetopt_args_info argi[1];
	struct xmit_s ctx[1];
	short unsigned int port = 8584;
	int res = 0;

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		error(0, "need input file");
		res = 1;
		goto fr_out;
	} else if ((ctx->ute = ute_open(argi->inputs[0], UO_RDONLY)) == NULL) {
		error(0, "cannot open file '%s'", argi->inputs[0]);
		res = 1;
		goto fr_out;
	}

	/* set signal handler */
	signal(SIGINT, handle_sigint);

	/* obtain a new handle, somehow we need to use the port number innit? */
	ctx->ud = ud_chan_init(port);

	/* also accept connections on that socket and the mcast network */
	if ((ctx->epfd = epoll_create(2)) < 0) {
		error(0, "cannot instantiate epoll on ud-xmit socket");
	} else {
		struct epoll_event ev[1];

		if (rebind_chan(ctx->ud) < 0) {
			error(0, "cannot bind ud-xmit socket");
		} else {
			ev->events = EPOLLIN;
			ev->data.fd = ctx->ud->sock;
			epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->ud->sock, ev);
		}

		if ((ctx->mcfd = ud_mcast_init(port)) < 0) {
			error(0, "cannot instantiate mcast listener");
		} else {
			ev->events = EPOLLIN;
			ev->data.fd = ctx->mcfd;
			epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, ctx->mcfd, ev);
		}
	}

	/* the actual work */
	switch (setjmp(jb)) {
	case 0:
		ctx->speed = argi->speed_arg;
		ctx->restampp = argi->restamp_given;
		if (pre_work(ctx) == 0) {
			/* do the actual work */
			work(ctx);
		}
	case SIGINT:
	default:
		if (post_work(ctx) < 0) {
			res = 1;
		}
		printf("sent %zu ticks in %u packets\n", nt, pno);
		break;	
	}

	/* close epoll */
	close(ctx->epfd);

	/* and lose the handle again */
	ud_mcast_fini(ctx->mcfd);
	ud_chan_fini(ctx->ud);

	/* and close the file */
	ute_close(ctx->ute);

fr_out:
	/* free up command line parser resources */
	cmdline_parser_free(argi);
out:
	return res;
}

/* ud-xmit.c ends here */
