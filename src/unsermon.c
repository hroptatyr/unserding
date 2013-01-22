/*** unsermon.c -- unserding network monitor
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
#include <errno.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
/* for clock_gettime() */
#include <time.h>
/* to get a take on them m30s and m62s */
# define DEFINE_GORY_STUFF
#if defined HAVE_UTERUS_UTERUS_H
# include <uterus/uterus.h>
# include <uterus/m30.h>
# include <uterus/m62.h>
# define HAVE_UTERUS
#elif defined HAVE_UTERUS_H
# include <uterus.h>
# include <m30.h>
# include <m62.h>
# define HAVE_UTERUS
#endif	/* HAVE_UTERUS_UTERUS_H || HAVE_UTERUS_H */

/* our master include file */
#include "unserding.h"
#include "unserding-nifty.h"
#include "ud-logger.h"
#include "boobs.h"

#define USE_COROUTINES		1

#if defined DEBUG_FLAG
# include <stdio.h>
# define UDEBUG(args...)	fprintf(stderr, args)
# else	/* !DEBUG_FLAG */
# define UDEBUG(args...)
#endif	/* DEBUG_FLAG */

#define UD_SYSLOG(x, args...)	ud_logout(x, errno, args)

static FILE *monout;


typedef struct ud_ev_async_s ud_ev_async;

/* our version of the async event, cdr-coding */
struct ud_ev_async_s {
	struct ev_async super;
};

struct ud_loopclo_s {
	/** loop lock */
	pthread_mutex_t lolo;
	/** just a cond */
	pthread_cond_t loco;
};


#if defined HAVE_UTERUS
/* helpers */
static inline size_t
pr_tsmstz(char *restrict buf, time_t sec, uint32_t msec, char sep)
{
	struct tm *tm;

	tm = gmtime(&sec);
	strftime(buf, 32, "%F %T", tm);
	buf[10] = sep;
	buf[19] = '.';
	buf[20] = (char)(((msec / 100) % 10) + '0');
	buf[21] = (char)(((msec / 10) % 10) + '0');
	buf[22] = (char)(((msec / 1) % 10) + '0');
	buf[23] = '+';
	buf[24] = '0';
	buf[25] = '0';
	buf[26] = ':';
	buf[27] = '0';
	buf[28] = '0';
	return 29;
}

static inline size_t
pr_ttf(char *restrict buf, uint16_t ttf)
{
	switch (ttf & ~(SCOM_FLAG_LM | SCOM_FLAG_L2M)) {
	case SL1T_TTF_BID:
		*buf = 'b';
		break;
	case SL1T_TTF_ASK:
		*buf = 'a';
		break;
	case SL1T_TTF_TRA:
		*buf = 't';
		break;
	case SL1T_TTF_FIX:
		*buf = 'f';
		break;
	case SL1T_TTF_STL:
		*buf = 'x';
		break;
	case SL1T_TTF_AUC:
		*buf = 'k';
		break;

	case SBAP_FLAVOUR:
		*buf++ = 'b';
		*buf++ = 'a';
		*buf++ = 'p';
		return 3;

	case SCOM_TTF_UNK:
	default:
		*buf++ = '?';
		break;
	}
	return 1;
}

static size_t
__pr_snap(char *tgt, scom_t st)
{
	const_ssnp_t snp = (const void*)st;
	char *p = tgt;

	/* bid price */
	p += ffff_m30_s(p, (m30_t)snp->bp);
	*p++ = '\t';
	/* ask price */
	p += ffff_m30_s(p, (m30_t)snp->ap);
	if (scom_thdr_ttf(st) == SSNP_FLAVOUR) {
		/* real snaps reach out further */
		*p++ = '\t';
		/* bid quantity */
		p += ffff_m30_s(p, (m30_t)snp->bq);
		*p++ = '\t';
		/* ask quantity */
		p += ffff_m30_s(p, (m30_t)snp->aq);
		*p++ = '\t';
		/* volume-weighted trade price */
		p += ffff_m30_s(p, (m30_t)snp->tvpr);
		*p++ = '\t';
		/* trade quantity */
		p += ffff_m30_s(p, (m30_t)snp->tq);
	}
	return p - tgt;
}

static size_t
__attribute__((noinline))
__pr_cdl(char *tgt, scom_t st)
{
	const_scdl_t cdl = (const void*)st;
	char *p = tgt;

	/* h(igh) */
	p += ffff_m30_s(p, (m30_t)cdl->h);
	*p++ = '\t';
	/* l(ow) */
	p += ffff_m30_s(p, (m30_t)cdl->l);
	*p++ = '\t';
	/* o(pen) */
	p += ffff_m30_s(p, (m30_t)cdl->o);
	*p++ = '\t';
	/* c(lose) */
	p += ffff_m30_s(p, (m30_t)cdl->c);
	*p++ = '\t';
	/* start of the candle */
	p += sprintf(p, "%08x", cdl->sta_ts);
	*p++ = '|';
	p += pr_tsmstz(p, cdl->sta_ts, 0, 'T');
	*p++ = '\t';
	/* event count in candle, print 3 times */
	p += sprintf(p, "%08x", cdl->cnt);
	*p++ = '|';
	p += ffff_m30_s(p, (m30_t)cdl->cnt);
	return p - tgt;
}
#endif	/* HAVE_UTERUS */

static inline size_t
hrclock_print(char *buf, size_t len)
{
	struct timespec tsp;
	clock_gettime(CLOCK_REALTIME, &tsp);
	return snprintf(buf, len, "%ld.%09li", tsp.tv_sec, tsp.tv_nsec);
}


/* the actual worker function */
#if 0
static void
mon_pkt_cb(job_t j)
{
	char buf[8192], *epi = buf;

	/* print a time stamp */
	epi += hrclock_print(buf, sizeof(buf));
	*epi++ = '\t';

	/* obtain the address in human readable form */
	{
		int fam = ud_sockaddr_fam(&j->sa);
		const struct sockaddr *sa = ud_sockaddr_addr(&j->sa);
		uint16_t port = ud_sockaddr_port(&j->sa);

		*epi++ = '[';
		if (inet_ntop(fam, sa, epi, 128)) {
			epi += strlen(epi);
		}
		*epi++ = ']';
		epi += snprintf(epi, 16, ":%hu", port);
	}
	*epi++ = '\t';

	{
		unsigned int nrd = j->blen;
		unsigned int cno = udpc_pkt_cno(JOB_PACKET(j));
		unsigned int pno = udpc_pkt_pno(JOB_PACKET(j));
		unsigned int cmd = udpc_pkt_cmd(JOB_PACKET(j));
		unsigned int mag = ntohs(((const uint16_t*)j->buf)[3]);

		epi += snprintf(
			epi, 256,
			/*len*/"%04x\t"
			/*cno*/"%02x\t"
			/*pno*/"%06x\t"
			/*cmd*/"%04x\t"
			/*mag*/"%04x\t",
			nrd, cno, pno, cmd, mag);
	}

	/* intercept special channels */
	switch (udpc_pkt_cmd(JOB_PACKET(j))) {
	case 0x7574:
	case 0x7575: {
#if defined HAVE_UTERUS
		char *pbuf = UDPC_PAYLOAD(JOB_PACKET(j).pbuf);
		size_t plen = UDPC_PAYLLEN(JOB_PACKET(j).plen);

		for (scom_t sp = (void*)pbuf, ep = (void*)(pbuf + plen);
		     sp < ep;
		     sp += scom_tick_size(sp) *
			     (sizeof(struct sndwch_s) / sizeof(*sp))) {
			uint32_t sec = scom_thdr_sec(sp);
			uint16_t msec = scom_thdr_msec(sp);
			uint16_t ttf = scom_thdr_ttf(sp);
			char *p = epi;

			p += pr_tsmstz(p, sec, msec, 'T');
			*p++ = '\t';
			/* index into the sym table */
			p += sprintf(p, "%x", scom_thdr_tblidx(sp));
			*p++ = '\t';
			/* tick type */
			p += pr_ttf(p, ttf);
			*p++ = '\t';
			switch (ttf) {
				const_sl1t_t l1t;

			case SL1T_TTF_BID:
			case SL1T_TTF_ASK:
			case SL1T_TTF_TRA:
			case SL1T_TTF_FIX:
			case SL1T_TTF_STL:
			case SL1T_TTF_AUC:
				l1t = (const void*)sp;
				/* price value */
				p += ffff_m30_s(p, (m30_t)l1t->v[0]);
				*p++ = '\t';
				/* size value */
				p += ffff_m30_s(p, (m30_t)l1t->v[1]);
				break;
			case SL1T_TTF_VOL:
			case SL1T_TTF_VPR:
			case SL1T_TTF_OI:
				/* just one huge value, will there be a m62? */
				l1t = (const void*)sp;
				p += ffff_m62_s(p, (m62_t)l1t->w[0]);
				break;

				/* snaps */
			case SSNP_FLAVOUR:
			case SBAP_FLAVOUR:
				p += __pr_snap(p, sp);
				break;

				/* candles */
			case SL1T_TTF_BID | SCOM_FLAG_LM:
			case SL1T_TTF_ASK | SCOM_FLAG_LM:
			case SL1T_TTF_TRA | SCOM_FLAG_LM:
			case SL1T_TTF_FIX | SCOM_FLAG_LM:
			case SL1T_TTF_STL | SCOM_FLAG_LM:
			case SL1T_TTF_AUC | SCOM_FLAG_LM:
				p += __pr_cdl(p, sp);
				break;

			case SCOM_TTF_UNK:
			default:
				break;
			}
			*p++ = '\n';
			*p = '\0';
			fwrite(buf, sizeof(char), p - buf, monout);
		}
#else  /* !HAVE_UTERUS */
		fwrite(buf, sizeof(char), epi - buf, monout);
		fputs("UTE/le message, no decoding support\n", monout);
#endif	/* HAVE_UTERUS */
		break;
	}
	case 0x5554:
	case 0x5555:
		fwrite(buf, sizeof(char), epi - buf, monout);
		fputs("UTE/be message, no decoding support\n", monout);
		break;
	default:
		if (ud_sprint_pkt_pretty(epi, JOB_PACKET(j))) {
			for (char *p = epi, *np;
			     (np = strchr(p, '\n'));
			     p = np + 1) {
				fwrite(buf, sizeof(char), epi - buf, monout);
				fwrite(p, sizeof(char), np - p + 1, monout);
			}
		} else {
			fwrite(buf, sizeof(char), epi - buf, monout);
			fputs("NAUGHT message\n", monout);
		}
		break;
	}
	return;
}
#endif

static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
#if defined DEBUG_FLAG
	/* the address in human readable form */
	char buf[INET6_ADDRSTRLEN];
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	union ud_sockaddr_u sa;
	socklen_t lsa = sizeof(sa);
	uint16_t peek[4];
	ssize_t nrd;
#endif	/* DEBUG_FLAG */
	ud_sock_t s = w->data;
	struct ud_msg_s msg[1];

#if defined DEBUG_FLAG
	/* for the debugging */
	if ((nrd = recvfrom(
		     w->fd, peek, sizeof(peek), MSG_PEEK, &sa.sa, &lsa)) < 0) {
		/* no need to bother our deserialiser */
		return;
	}
	/* de-big-endian-ify the peek */
	peek[0] = be16toh(peek[0]);
	peek[1] = be16toh(peek[1]);
	peek[2] = be16toh(peek[2]);
	peek[3] = be16toh(peek[3]);
	/* obtain the address in human readable form */
	{
		int fam = ud_sockaddr_fam(&sa);
		a = inet_ntop(fam, ud_sockaddr_addr(&sa), buf, sizeof(buf));
		p = ud_sockaddr_port(&sa);
	}
	/* in debugging mode give it full blast */
	UDEBUG(
		":sock %d connect :from [%s]:%hu  "
		":len %04zd :ini %04hx :pno %04hx :cmd %04hx :mag %04hx\n",
		w->fd, a, p, nrd,
		peek[0], peek[1], peek[2], peek[3]);
#endif	/* DEBUG_FLAG */

	/* otherwise just report activity */
	{
		static time_t last_act;
		static time_t last_rpt;
		time_t this_act = time(NULL);

		if (last_act + 1 < this_act) {
			logger(LOG_INFO, ":sock %d shows activity", s->fd);
			last_rpt = this_act;
		} else if (last_rpt + 60 < this_act) {
			logger(LOG_INFO, 
				":sock %d (still) shows activity ... "
				"1 minute reminder\n", s->fd);
			last_rpt = this_act;
		}
		last_act = this_act;
	}

	/* handle the reading */
	if (ud_chck_msg(msg, s) < 0) {
		/* buggered */
		return;
	}
#if 0
	if (UNLIKELY(nread < 0)) {
		UD_CRITICAL_MCAST("could not handle incoming connection\n");
		goto out_revok;
	} else if (nread == 0) {
		/* no need to bother */
		goto out_revok;
	}

	/* decode the guy */
	(void)mon_pkt_cb(j);
out_revok:
#endif
	return;
}


static ev_signal ALGN16(__sigint_watcher);
static ev_signal ALGN16(__sighup_watcher);
static ev_signal ALGN16(__sigterm_watcher);
static ev_signal ALGN16(__sigpipe_watcher);
static ev_async ALGN16(__wakeup_watcher);
ev_async *glob_notify;

static void
sigint_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UDEBUG("C-c caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
sigpipe_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UDEBUG("SIGPIPE caught, doing nothing\n");
	return;
}

static void
sighup_cb(EV_P_ ev_signal *UNUSED(w), int UNUSED(revents))
{
	UDEBUG("SIGHUP caught, unrolling everything\n");
	ev_unloop(EV_A_ EVUNLOOP_ALL);
	return;
}

static void
triv_cb(EV_P_ ev_async *UNUSED(w), int UNUSED(revents))
{
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wswitch-enum"
#endif /* __INTEL_COMPILER */
#include "unsermon-clo.h"
#include "unsermon-clo.c"
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
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_signal *sigint_watcher = &__sigint_watcher;
	ev_signal *sighup_watcher = &__sighup_watcher;
	ev_signal *sigterm_watcher = &__sigterm_watcher;
	ev_signal *sigpipe_watcher = &__sigpipe_watcher;
	ev_io *beef = NULL;
	size_t nbeef;
	/* args */
	struct gengetopt_args_info argi[1];

	/* whither to log */
	monout = stdout;
	ud_openlog(NULL);

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	/* initialise the main loop */
	loop = ev_default_loop(EVFLAG_AUTO);

	/* initialise a sig C-c handler */
	ev_signal_init(sigint_watcher, sigint_cb, SIGINT);
	ev_signal_start(EV_A_ sigint_watcher);
	/* initialise a sig C-c handler */
	ev_signal_init(sigpipe_watcher, sigpipe_cb, SIGPIPE);
	ev_signal_start(EV_A_ sigpipe_watcher);
	/* initialise a SIGTERM handler */
	ev_signal_init(sigterm_watcher, sighup_cb, SIGTERM);
	ev_signal_start(EV_A_ sigterm_watcher);
	/* initialise a SIGHUP handler */
	ev_signal_init(sighup_watcher, sighup_cb, SIGHUP);
	ev_signal_start(EV_A_ sighup_watcher);

	/* initialise a wakeup handler */
	glob_notify = &__wakeup_watcher;
	ev_async_init(glob_notify, triv_cb);
	ev_async_start(EV_A_ glob_notify);

	/* make some room for the control channel and the beef chans */
	nbeef = (argi->beef_given + 1);
	beef = malloc(nbeef * sizeof(*beef));

	/* attach a multicast listener
	 * we add this quite late so that it's unlikely that a plethora of
	 * events has already been injected into our precious queue
	 * causing the libev main loop to crash. */
	{
		ud_sock_t s;

		if ((s = ud_socket((struct ud_sockopt_s){UD_SUB})) != NULL) {
			beef->data = s;
			ev_io_init(beef, mon_beef_cb, s->fd, EV_READ);
			ev_io_start(EV_A_ beef);
		}
	}

	/* go through all beef channels */
	for (unsigned int i = 0, j = 0; i < argi->beef_given; i++) {
		ud_sock_t s;

		if ((s = ud_socket((struct ud_sockopt_s){
				UD_SUB,
				.port = (uint16_t)argi->beef_arg[i],
				})) == NULL) {
			continue;
		}
		beef[++j].data = s;
		ev_io_init(beef + j, mon_beef_cb, s->fd, EV_READ);
		ev_io_start(EV_A_ beef + j);
		nbeef = j;
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	logger(LOG_NOTICE, "shutting down unsermon\n");

	/* detaching beef channels */

	for (unsigned int i = 0; i <= nbeef; i++) {
		ud_sock_t s = beef[i].data;
		ev_io_stop(EV_A_ beef + i);
		ud_close(s);
	}

	/* destroy the default evloop */
	ev_default_destroy();

	/* kick the config context */
	cmdline_parser_free(argi);

	/* close our log output */
	fflush(monout);
	fclose(monout);
	ud_closelog();
	/* unloop was called, so exit */
	return 0;
}

/* unsermon.c ends here */
