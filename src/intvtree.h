/*** intvtree.h -- interval trees based on red-black trees
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

#if !defined INCLUDED_intvtree_h_
#define INCLUDED_intvtree_h_

#include <stdint.h>
#include <stdbool.h>

typedef struct itree_s *itree_t;
typedef struct it_node_s *it_node_t;
typedef void(*it_trav_f)(uint32_t lo, uint32_t hi, void *data, void *clo);

extern itree_t make_itree(void);
extern void free_itree(itree_t);
extern it_node_t itree_add(itree_t it, uint32_t lo, uint32_t hi, void *data);
extern void *itree_del_node(itree_t it, it_node_t z);
extern it_node_t itree_succ_of(itree_t it, it_node_t x);
extern it_node_t itree_pred_of(itree_t it, it_node_t x);

/* iterators */
extern void itree_trav_in_order(itree_t it, it_trav_f cb, void *clo);
extern void itree_find_point(itree_t it, uint32_t p, it_trav_f cb, void *clo);

/* subject to sudden extinction */
extern void itree_print(itree_t it);

#endif	/* INCLUDED_intvtree_h_ */
