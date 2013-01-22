/*** unserding-dbg.h -- debugging and logging mumbo jumbo
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

#if !defined INCLUDED_unserding_dbg_h_
#define INCLUDED_unserding_dbg_h_

#if defined HAVE_CONFIG_H
# include "config.h"
#endif
#if defined HAVE_PTHREAD_H
# include <pthread.h>
#endif
#if defined HAVE_STRING_H
# include <string.h>
#endif
#if defined HAVE_STDINT_H
# include <stdint.h>
#endif
#if defined HAVE_STDARG_H
# include <stdarg.h>
#endif
#if defined HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdio.h>

/* our own logger facility */
#include "ud-logger.h"


/* logging */
#define UD_LOGOUT(args...)	fprintf(logout, args)

#if defined UNSERSRV && defined DEBUG_FLAG
# define UD_DEBUG(args...)	UD_LOGOUT("[unserding] " args)
# define UD_CRITICAL_STDIN(args...)				\
	UD_LOGOUT("[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)			\
	UD_LOGOUT("[unserding/stdin] " args)
# define UD_CRITICAL_PROTO(args...)				\
	UD_LOGOUT("[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)			\
	UD_LOGOUT("[unserding/proto] " args)
# define UD_CRITICAL_CAT(args...)				\
	UD_LOGOUT("[unserding/catalogue] CRITICAL " args)
# define UD_DEBUG_CAT(args...)				\
	UD_LOGOUT("[unserding/catalogue] " args)
# define UD_DBGCONT(args...)			\
	UD_LOGOUT(args)

#elif defined UNSERCLI && defined DEBUG_FLAG
# define UD_DEBUG(args...)				\
	UD_LOGOUT("[unserding] " args)
# define UD_CRITICAL_STDIN(args...)			\
	UD_LOGOUT("[unserding/stdin] CRITICAL " args)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)
# define UD_DBGCONT(args...)			\
	UD_LOGOUT(args)

#elif defined UNSERMON && defined DEBUG_FLAG
# define UD_DEBUG(args...)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)				\
	UD_LOGOUT("[unserding/proto] CRITICAL " args)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)
# define UD_DBGCONT(args...)

#else  /* aux stuff */
# define UD_DEBUG(args...)
# define UD_CRITICAL_STDIN(args...)
# define UD_DEBUG_STDIN(args...)
# define UD_CRITICAL_PROTO(args...)
# define UD_DEBUG_PROTO(args...)
# define UD_CRITICAL_CAT(args...)
# define UD_DEBUG_CAT(args...)
# define UD_DBGCONT(args...)
#endif

#define UD_CRITICAL(args...)					\
	do {							\
		UD_LOGOUT("[unserding] CRITICAL " args);	\
		UD_SYSLOG(LOG_CRIT, "CRITICAL " args);		\
	} while (0)

#if defined UNSERMON
# define UD_UNSERMON_PKT(args...)	UD_LOGOUT("%02x:%06x: " args)
#endif	/* UNSERMON */

#endif	/* INCLUDED_unserding_dbg_h_ */
