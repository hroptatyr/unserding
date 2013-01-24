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
#include <string.h>
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
#include <sys/mman.h>
#if defined HAVE_EV_H
# include <ev.h>
# undef EV_P
# define EV_P  struct ev_loop *loop __attribute__((unused))
#endif	/* HAVE_EV_H */
/* for clock_gettime() */
#include <time.h>

/* our master include file */
#include "unserding.h"
#include "ud-sockaddr.h"
#include "ud-private.h"
#include "unserding-nifty.h"
#include "ud-logger.h"
#include "ud-module.h"
#include "boobs.h"
#include "unsermon.h"

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


/* decoders */
#include "svc-pong.h"

/* we organise decoders in channels
 * every second channel gets a decoder map */
typedef ud_mondec_f *__decmap_t;

static __decmap_t decmap[128];

int
ud_mondec_reg(ud_svc_t svc, ud_mondec_f cb)
{
	/* singleton valley */
	unsigned int chn = UD_CHN(svc);
	__decmap_t dm;

	if ((dm = decmap[chn / 2]) == NULL) {
		size_t z = 512 * sizeof(cb);
		void *p;

		p = mmap(NULL, z, PROT_MEM, MAP_MEM, -1, 0);
		if (UNLIKELY(p == MAP_FAILED)) {
			return -1;
		}
		dm = decmap[chn / 2] = p;
	}
	/* first 9 bits */
	dm[svc & ((1U << 9U) - 1)] = cb;
	return 0;
}

int
ud_mondec_dereg(ud_svc_t svc)
{
	/* singleton valley */
	unsigned int chn = UD_CHN(svc);
	__decmap_t dm;

	if ((dm = decmap[chn / 2]) == NULL) {
		return -1;
	} else if (dm[svc & ((1U << 9U) - 1)] == NULL) {
		return -1;
	}
	dm[svc & ((1U << 9U) - 1)] = NULL;
	return 0;
}

static ud_mondec_f
__mondec(ud_svc_t svc)
{
	unsigned int chn = UD_CHN(svc);
	__decmap_t dm;
	ud_mondec_f res;

	if ((dm = decmap[chn / 2]) == NULL) {
		res = NULL;
	} else if ((res = dm[svc & ((1U << 9U) - 1)]) == NULL) {
		;
	}
	return res;
}

static inline size_t
hrclock_print(char *buf, size_t len)
{
	struct timespec tsp;
	clock_gettime(CLOCK_REALTIME, &tsp);
	return snprintf(buf, len, "%ld.%09li", tsp.tv_sec, tsp.tv_nsec);
}


/* the actual packet decoder */
static void
mon_pkt_cb(ud_sock_t s, const struct ud_msg_s msg[static 1])
{
	char buf[8192], *epi = buf;
	struct ud_auxmsg_s aux[1];
	ud_mondec_f cb;

	/* print a time stamp */
	epi += hrclock_print(buf, sizeof(buf));
	*epi++ = '\t';

	if (ud_chck_aux(aux, s) < 0) {
		/* uh oh */
		*epi++ = '?';
		goto bang;
	}

	/* otherwise */
	{
		/* obtain the address in human readable form */
		ud_const_sockaddr_t udsa = &aux->src->sa;
		int fam = ud_sockaddr_fam(udsa);
		const struct sockaddr *sa = ud_sockaddr_addr(udsa);
		uint16_t port = ud_sockaddr_port(udsa);

		*epi++ = '[';
		if (inet_ntop(fam, sa, epi, 128)) {
			epi += strlen(epi);
		}
		*epi++ = ']';
		epi += snprintf(epi, 16, ":%hu", port);
	}

	/* next up, size */
	epi += snprintf(
		epi, 256, "\t%04x\t%04hx\t%04hx\t",
		aux->len, aux->pno, aux->svc);

	/* go for the actual message */
	if ((cb = __mondec(aux->svc)) != NULL) {
		epi += cb(epi, 512, aux->svc, msg);
	} else {
		/* intercept special channels */
		switch (aux->svc) {
		case UD_CTRL_SVC(UD_SVC_CMD):
			epi += snprintf(epi, 256, "CMD request");
			break;
		case UD_CTRL_SVC(UD_SVC_CMD + 1):
			epi += snprintf(epi, 256, "CMD announce");
			break;

		case UD_CTRL_SVC(UD_SVC_TIME):
			epi += snprintf(epi, 256, "TIME request");
			break;

		case UD_CTRL_SVC(UD_SVC_TIME + 1):
			epi += snprintf(epi, 256, "TIME announce");
			break;

		case UD_CTRL_SVC(UD_SVC_PING):
			epi += snprintf(epi, 256, "PING");
			break;

		case UD_CTRL_SVC(UD_SVC_PING + 1):
			epi += snprintf(epi, 256, "PONG");
			break;

		default:
			break;
		}
	}

bang:
	/* print the whole shebang */
	*epi++ = '\n';
	*epi = '\0';
	{
		size_t bsz = epi - buf;
		if (fwrite(buf, sizeof(char), bsz, monout) < bsz) {
			/* uh oh */
			abort();
		}
	}
	return;
}

static int
mon_beef_peek(int fd)
{
#if defined MON_BEEF_PEEK
	/* the address in human readable form */
	char buf[INET6_ADDRSTRLEN];
	const char *a;
	/* the port (in host-byte order) */
	uint16_t p;
	union ud_sockaddr_u sa;
	socklen_t lsa = sizeof(sa);
	uint16_t peek[4];
	ssize_t nrd;

	/* for the debugging */
	if ((nrd = recvfrom(
		     fd, peek, sizeof(peek), MSG_PEEK, &sa.sa, &lsa)) < 0) {
		/* no need to bother our deserialiser */
		return -1;
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
		fd, a, p, nrd,
		peek[0], peek[1], peek[2], peek[3]);
#else  /* !MON_BEEF_PEEK */
	if (UNLIKELY(fd < 0)) {
		return -1;
	}
#endif	/* MON_BEEF_PEEK */
	return 0;
}

static void
mon_beef_actvty(ud_sock_t s)
{
	static time_t last_act;
	static time_t last_rpt;
	time_t this_act = time(NULL);
	/* pretty printing */
	char a[INET6_ADDRSTRLEN];
	uint16_t p;
	ud_const_sockaddr_t sa;

	if (LIKELY(last_rpt + 60 > this_act || last_act + 1 > this_act)) {
		goto out;
	}

	if (LIKELY((sa = ud_socket_addr(s)) != NULL)) {
		ud_sockaddr_ntop(a, sizeof(a), sa);
		p = ud_sockaddr_port(sa);
	}

	/* otherwise just report activity */
	if (last_act + 1 < this_act) {
		if (LIKELY(sa != NULL)) {
			logger(LOG_INFO,
				"network [%s]:%hu shows activity", a, p);
		} else {
			logger(LOG_INFO,
				"network associated with %d shows activity",
				s->fd);
		}
		last_rpt = this_act;
	} else if (last_rpt + 60 < this_act) {
		if (LIKELY(sa != NULL)) {
			logger(LOG_INFO,
				"network [%s]:%hu (still) shows activity ... "
				"1 minute reminder\n", a, p);
		} else {
			logger(LOG_INFO,
				"network associated with %d (still) "
				"shows activity ... 1 minute reminder", s->fd);
		}
		last_rpt = this_act;
	}
out:
	last_act = this_act;
	return;
}

static void
log_rgstr(ud_sock_t s)
{
	/* pretty printing */
	char a[INET6_ADDRSTRLEN];
	uint16_t p;
	ud_const_sockaddr_t sa;

	if (UNLIKELY((sa = ud_socket_addr(s)) == NULL)) {
		return;
	}
	/* otherwise ... */
	ud_sockaddr_ntop(a, sizeof(a), sa);
	p = ud_sockaddr_port(sa);

	logger(LOG_INFO, "monitoring network [%s]:%hu", a, p);
	return;
}

static void
log_dergstr(ud_sock_t s)
{
	/* pretty printing */
	char a[INET6_ADDRSTRLEN];
	uint16_t p;
	ud_const_sockaddr_t sa;

	if (UNLIKELY((sa = ud_socket_addr(s)) == NULL)) {
		return;
	}
	/* otherwise ... */
	ud_sockaddr_ntop(a, sizeof(a), sa);
	p = ud_sockaddr_port(sa);

	logger(LOG_INFO, "monitoring of network [%s]:%hu halted", a, p);
	return;
}


/* EV callbacks */
static void
mon_beef_cb(EV_P_ ev_io *w, int UNUSED(revents))
{
	ud_sock_t s = w->data;

	/* see if we need peeking */
	(void)mon_beef_peek(w->fd);

	/* report activity */
	mon_beef_actvty(s);

	/* handle the reading */
	for (struct ud_msg_s msg[1]; ud_chck_msg(msg, s) == 0;) {
		mon_pkt_cb(s, msg);
	}
	return;
}

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

#include "svc-pong.h"
#if defined HAVE_UTERUS_H || defined HAVE_UTERUS_UTERUS_H
# include "svc-uterus.h"
#endif	/* HAVE_UTERUS_H || HAVE_UTERUS_UTERUS_H */

int
main(int argc, char *argv[])
{
	/* use the default event loop unless you have special needs */
	struct ev_loop *loop;
	ev_signal sigint_watcher[1];
	ev_signal sighup_watcher[1];
	ev_signal sigterm_watcher[1];
	ev_signal sigpipe_watcher[1];
	ev_async wakeup_watcher[1];
	ev_io *beef = NULL;
	size_t nbeef;
	/* args */
	struct gengetopt_args_info argi[1];

	/* whither to log */
	monout = stdout;

	/* parse the command line */
	if (cmdline_parser(argc, argv, argi)) {
		exit(1);
	}

	/* open the log file */
	ud_openlog(argi->log_arg);

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
			log_rgstr(s);
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
		log_rgstr(s);
		nbeef = j;
	}

	svc_pong_LTX_ud_mondec_init();
#if defined HAVE_UTERUS_H || defined HAVE_UTERUS_UTERUS_H
	svc_uterus_LTX_ud_mondec_init();
#endif	/* HAVE_UTERUS_H || HAVE_UTERUS_UTERUS_H */

	/* load DSOs */
	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		ud_mod_t m;
		ud_mod_f inif;

		if ((m = ud_mod_open(argi->inputs[i])) == NULL) {
			;
		} else if ((inif = ud_mod_sym(m, "ud_mondec_init")) == NULL) {
			;
		} else {
			/* everything in order, call the initter */
			(void)((int(*)(void))inif)();
		}
	}

	/* now wait for events to arrive */
	ev_loop(EV_A_ 0);

	logger(LOG_NOTICE, "shutting down unsermon");

	/* detaching beef channels */
	for (unsigned int i = 0; i <= nbeef; i++) {
		ud_sock_t s = beef[i].data;

		log_dergstr(s);
		ev_io_stop(EV_A_ beef + i);
		ud_close(s);
	}

	/* clearing decmap */
	for (size_t i = 0; i < countof(decmap); i++) {
		if (decmap[i] != NULL) {
			munmap(decmap[i], 512 * sizeof(ud_mondec_f));
		}
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
