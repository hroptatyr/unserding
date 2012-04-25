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

struct xmit_s {
	ud_chan_t ud;
	utectx_t ute;
	float speed;
	bool restampp;
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
	udpc_make_pkt(pkt, 0, pno++, 0x2000);			\
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
			/* and sleep */
			slp = ((stmp - reft) * 1000 + msec - refm) * speed;
			for (unsigned int i = slp, j = 0; i; i /= 2, j++) {
				XMIT_STUP('0' + (j % 10));
			}
			XMIT_STUP('\n');
			usleep(slp);
			refm = msec;
			reft = stmp;
		}
		/* add the scom in question to the pool */
		XMIT_STUP('+');
		{
			struct sndwch_s sto[4];
			size_t bs = scom_byte_size(ti);

			/* copy the scom, not strictly necessary but ah well */
			memcpy(sto, ti, bs);

			if (ctx->restampp) {
				struct timeval now[1];

				gettimeofday(now, NULL);
				/* replace the time stamp */
				AS_SCOM_THDR(sto)->sec = now->tv_sec;
				AS_SCOM_THDR(sto)->msec = now->tv_usec / 1000;
			}
			/* add the copied guy */
			udpc_seria_add_data(ser, sto, bs);
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
	static struct ud_chan_s hdl;
	struct gengetopt_args_info argi[1];
	struct xmit_s ctx[1];
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
	hdl = ud_chan_init(8584);

	/* the actual work */
	switch (setjmp(jb)) {
	case 0:
		ctx->ud = &hdl;
		ctx->speed = argi->speed_arg;
		ctx->restampp = argi->restamp_given;
		work(ctx);
	case SIGINT:
	default:
		printf("sent %zu ticks in %u packets\n", nt, pno);
		break;	
	}

	/* and lose the handle again */
	ud_chan_fini(hdl);

	/* and close the file */
	ute_close(ctx->ute);

fr_out:
	/* free up command line parser resources */
	cmdline_parser_free(argi);
out:
	return res;
}

/* ud-xmit.c ends here */
