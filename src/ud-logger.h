/*** ud-logger.h -- unserding logging service
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
#if !defined INCLUDED_ud_logger_h_
#define INCLUDED_ud_logger_h_

#include <stdarg.h>
#include <syslog.h>

#if defined __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* this really is a FILE* */
extern void *logout;

/**
 * Open the logfile with pathname LOGFN. */
extern int ud_openlog(const char *logfn);

/**
 * Close logging facility. */
extern int ud_closelog(void);

/**
 * Rotate the log file.
 * This actually just closes and opens the file again. */
extern void ud_rotlog(void);

/**
 * Like perror() but for our log file. */
extern __attribute__((format(printf, 3, 4))) void
ud_logout(int facil, int eno, const char *fmt, ...);

/**
 * Like perror() but for our log file. */
#if !defined __cplusplus
# define error(eno, args...)	ud_logout(LOG_ERR, eno, args)
#else  /* __cplusplus */
# define error(eno, args...)	::ud_logout(LOG_ERR, eno, args)
#endif	/* __cplusplus */

/**
 * For generic logging without errno indication. */
#if !defined __cplusplus
# define logger(facil, args...)	ud_logout(facil, 0, args)
#else  /* __cplusplus */
# define logger(facil, args...)	::ud_logout(facil, 0, args)
#endif	/* __cplusplus */

#if defined __cplusplus
}
#endif	/* __cplusplus */

#endif	/* INCLUDED_ud_logger_h_ */
