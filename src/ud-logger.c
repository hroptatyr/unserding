/*** ud-logger.c -- unserding logging service
 *
 * Copyright (C) 2011-2013 Sebastian Freundt
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
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "ud-logger.h"
#include "unserding-nifty.h"

#define UD_LOG_FLAGS		(LOG_PID | LOG_NDELAY)
#define UD_FACILITY		(LOG_LOCAL4)
#define UD_MAKEPRI(x)		(x)
#define UD_SYSLOG(x, args...)	syslog(UD_MAKEPRI(x), args)

static const char *glogfn;
void *logout;

static int
__open_logout(const char logfn[static 1])
{
	if (UNLIKELY((logout = fopen(logfn, "a")) == NULL)) {
		int errno_sv = errno;

		/* backup plan */
		logout = fopen("/dev/null", "w");
		errno = errno_sv;
		return -1;
	}
	return 0;
}

static void
__close_logout(void)
{
	if (logout != NULL) {
		fflush(logout);
		fclose(logout);
	}
	logout = NULL;
	return;
}

int
ud_openlog(const char *logfn)
{
	int res = 0;

	if (((glogfn = logfn) == NULL && (logout = stderr)) ||
	    (res = __open_logout(logfn)) < 0) {
		;
	} else {
		atexit(__close_logout);
	}
	return res;
}

int
ud_closelog(void)
{
	__close_logout();
	return 0;
}

void
ud_rotlog(void)
{
	if (glogfn == NULL) {
		return;
	}
	/* otherwise close and open again */
	__close_logout();
	(void)__open_logout(glogfn);
	return;
}

__attribute__((format(printf, 2, 3))) void
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(logout, fmt, vap);
	va_end(vap);
	if (eno) {
		fputc(':', logout);
		fputc(' ', logout);
		fputs(strerror(eno), logout);
	}
	fputc('\n', logout);
	return;
}

/* logger.c ends here */
