/*** seria.h -- unserding serialisation
 *
 * Copyright (C) 2008, 2009 Sebastian Freundt
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

#if !defined INCLUDED_seria_h_
#define INCLUDED_seria_h_

#define UDPC_TYPE_SEQ	'a'
#define UDPC_TYPE_VAR	'v'


/* one byte sequences */
#define UDPC_TYPE_UNK	0x00
#define UDPC_TYPE_BYTE	0x01
#define UDPC_TYPE_UI16	0x02
#define UDPC_TYPE_SI16	(UDPC_TYPE_UI16 | UDPC_SGN_MASK)
#define UDPC_TYPE_UI32	0x04
#define UDPC_TYPE_SI32	(UDPC_TYPE_UI32 | UDPC_SGN_MASK)
#define UDPC_TYPE_UI64	0x08
#define UDPC_TYPE_SI64	(UDPC_TYPE_UI64 | UDPC_SGN_MASK)

#define UDPC_TYPE_FLTH	(UDPC_TYPE_UI16 | UDPC_FLT_MASK)
#define UDPC_TYPE_FLTS	(UDPC_TYPE_UI32 | UDPC_FLT_MASK)
#define UDPC_TYPE_FLTD	(UDPC_TYPE_UI64 | UDPC_FLT_MASK)

#define UDPC_TYPE_STR	(UDPC_TYPE_BYTE | UDPC_SEQ_MASK)

/* multi byte sigs */
#define UDPC_TYPE_REC	0x0f	/* + number of slots + slot sigs */

/* masks */
#define UDPC_SGN_MASK	0x10
#define UDPC_FLT_MASK	0x20
#define UDPC_SEQ_MASK	0x80	/* + number of array elements */


/* inlines */
static inline uint16_t
udpc_msg_size(const char *sig)
{
	uint16_t res = 0;
	return res;
}

#endif	/* INCLUDED_seria_h_ */
