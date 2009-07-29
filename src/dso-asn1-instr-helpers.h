/*** dso-asn1-instr-helpers.h -- unserding helpers for ASN.1 catalogue
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

#if !defined INCLUDED_dso_asn1_instr_helpers_h_
#define INCLUDED_dso_asn1_instr_helpers_h_

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "Instrument.h"

typedef struct Instrument *instr_t;
typedef struct Ident *ident_t;
typedef long unsigned int gaid_t;

typedef struct OCTET_STRING *ostring_t;

static inline bool
ostring_equal_p(ostring_t os1, ostring_t os2)
{
	return os1->size == os2->size &&
		memcmp(os1->buf, os2->buf, os1->size) == 0;
}

static inline bool
gaid_equal_p(gaid_t gaid1, gaid_t gaid2)
{
	return gaid1 == gaid2;
}


/* ident accessors */
static inline ostring_t
ident_name(ident_t i)
{
	return &i->name;
}

static inline gaid_t
ident_gaid(ident_t i)
{
	return i->gaid;
}

static inline bool
ident_name_equal_p(ident_t i1, ident_t i2)
{
	return ostring_equal_p(ident_name(i1), ident_name(i2));
}

static inline bool
ident_gaid_equal_p(ident_t i1, ident_t i2)
{
	return gaid_equal_p(ident_gaid(i1), ident_gaid(i2)) && i1 != 0;
}


/* instr accessors */
static inline ident_t
instr_ident(instr_t i)
{
	return &i->ident;
}

static inline bool
instr_name_equal_p(instr_t i1, instr_t i2)
{
	return ident_name_equal_p(instr_ident(i1), instr_ident(i2));
}

#endif	/* INCLUDED_dso_asn1_instr_helpers_h_ */
