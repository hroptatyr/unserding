/*** ud-time.h -- time and clock goodness
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

#if !defined INCLUDED_ud_time_h_
#define INCLUDED_ud_time_h_

/* this is just a convenience header to overcome differences between the
 * intel and the gcc compiler */

/* allow for the intel compiler to use time.h features we need */
#if !defined __USE_POSIX
# define __USE_POSIX
#endif	/* !__USE_POSIX */
#if !defined __USE_MISC
# define __USE_MISC
#endif	/* !__USE_MISC */
#if !defined __USE_POSIX199309
# define __USE_POSIX199309
#endif	/* !__USE_POSIX199309 */
#if !defined __USE_XOPEN
# define __USE_XOPEN
#endif	/* !__USE_XOPEN */

#include <time.h>

/* returns the current CLOCK_REALTIME time stamp */
static inline struct timespec
__stamp(void)
{
	struct timespec res;
	clock_gettime(CLOCK_REALTIME, &res);
	return res;
}

/* given a stamp THEN, returns the difference between __stamp() and THEN. */
static inline struct timespec
__lapse(struct timespec then)
{
	struct timespec now, res;
	clock_gettime(CLOCK_REALTIME, &now);
	if (now.tv_nsec < then.tv_nsec) {
		res.tv_sec = now.tv_sec - then.tv_sec - 1;
		res.tv_nsec = 1000000000 + now.tv_nsec - then.tv_nsec;
	} else {
		res.tv_sec = now.tv_sec - then.tv_sec;
		res.tv_nsec = now.tv_nsec - then.tv_nsec;
	}
	return res;
}

static inline double
__as_f(struct timespec src)
{
/* return time as float in milliseconds */
	return src.tv_sec * 1000.f + src.tv_nsec / 1000000.f;
}

#endif	/* INCLUDED_ud_time_h_ */
