/*** xdr-instr-seria.h -- unserding serialisation for xdr instruments
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

#if !defined INCLUDED_xdr_instr_seria_h_
#define INCLUDED_xdr_instr_seria_h_

#include <stdbool.h>
#include <time.h>
#include <pfack/instruments.h>
#include "unserding.h"
#include "protocore.h"
#include "seria.h"

/**
 * Service 421a:
 * Get instrument definitions.
 * sig: 421a((si32 gaid)*)
 *   Returns the instruments whose ids match the given ones.  In the
 *   special case that no id is given, all instruments are dumped.
 **/
#define UD_SVC_INSTR_BY_ATTR	0x421a


/* helper funs, to simplify the unserding access */
/**
 * Deliver the guts of the instrument specified by CONT_ID via XDR.
 * \param hdl the unserding handle to use
 * \param tgt a pointer which will point to a buffer where the XDR
 *   encoded instrument can be unbanged from,
 * \param inst_id the GA instrument identifier
 * \return the number of bytes copied into TGT.
 * The instrument is delivered in encoded form and can be decoded
 * with libpfack's deser_instrument().
 **/
extern size_t
ud_find_one_instr(ud_handle_t hdl, const void **tgt, uint32_t inst_id);

/**
 * Query a bunch of instruments at once, calling CB() on each result. */
extern void
ud_find_many_instrs(
	ud_handle_t hdl,
	void(*cb)(const char *tgt, size_t len, void *clo), void *clo,
	uint32_t cont_id[], size_t len);

#endif	/* INCLUDED_xdr_instr_seria_h_ */
