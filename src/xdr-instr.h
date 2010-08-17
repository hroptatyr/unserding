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

#if !defined INCLUDED_xdr_instr_h_
#define INCLUDED_xdr_instr_h_

#include <stdbool.h>
#include <time.h>
#include "secu.h"
#include "unserding.h"
#include "protocore.h"
#include "seria.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
 *   encoded instrument can be unbanged from
 * \param inst_id the GA instrument identifier
 * \return the length of the XDR encoding in TGT
 * The instrument is delivered in encoded form and can be decoded
 * with libpfack's deser_instrument().
 **/
extern size_t
ud_find_one_instr(ud_handle_t h, const void **tgt, uint32_t inst_id);
/* same but for symbols */
extern size_t
ud_find_one_isym(ud_handle_t h, const void **tgt, const char *sym, size_t len);

/**
 * Deliver a list of tslabs known to the network for the instrument
 * specified by S.
 * \param hdl the unserding handle to use
 * \param tgt a pointer which will point to a tseries_s object.
 *   Do not dereference pointers from this structure.
 * \param s the GA instrument identifier
 * \return the length of the tseries buffer.
 **/
extern size_t
ud_find_one_tslab(ud_handle_t hdl, const void **tgt, su_secu_t s);

/**
 * Deliver a list of tslabs known to the network for the instrument
 * specified by S.
 * \param hdl the unserding handle to use
 * \param s the GA instrument identifier
 * \param cb callback called for each tslab found, the const void*
 *   should be cast to a const struct tsc_ce_s*
 * \return the number of tslabs found in total
 **/
extern size_t
ud_find_tslabs(ud_handle_t hdl, su_secu_t s, void(*cb)(const void*));

/**
 * Query a bunch of instruments at once, calling CB() on each result. */
extern void
ud_find_many_instrs(
	ud_handle_t hdl,
	void(*cb)(const char *tgt, size_t len, void *clo), void *clo,
	uint32_t cont_id[], size_t len);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* INCLUDED_xdr_instr_h_ */
