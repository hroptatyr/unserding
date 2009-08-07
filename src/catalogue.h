/*** catalogue.h -- simple catalogue of instruments
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

#if !defined INCLUDED_catalogue_h_
#define INCLUDED_catalogue_h_

#include "unserding.h"
#include <stdarg.h>
#include <stddef.h>
#include <pfack/instruments.h>

/**
 * The catalogue data type, just a husk. */
typedef void *cat_t;

/**
 * The catalogue data structure.
 * Naive, just an array of instruments. */
struct cat_s {
	size_t ninstrs;
	size_t alloc_sz;
	void *instrs;
	pthread_mutex_t mtx;
};

extern cat_t make_cat(void);
extern void free_cat(cat_t cat);

extern size_t cat_size(cat_t cat);
extern void cat_add_instr(cat_t cat, instr_t i);

extern instr_t cat_obtain_instr(cat_t cat);
extern instr_t cat_bang_instr(cat_t cat, instr_t i);

extern instr_t find_instr_by_gaid(cat_t cat, gaid_t gaid);
extern instr_t find_instr_by_name(cat_t cat, const char *name);

#endif	/* INCLUDED_catalogue_h_ */
