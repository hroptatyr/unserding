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

/* and the instr version */
typedef struct anno_instr_s *anno_instr_t;

/**
 * Just a reverse lookup structure. */
struct keyval_s {
	gaid_t key;
	uint32_t val;
};

/**
 * Annotation structure for annotated instruments. */
typedef struct cat_anno_s {
	;
} *cat_anno_t;

/**
 * Use annotated instruments, this is the class to do so.  Actually,
 * libpfack provides this already in form of URNs, however we are
 * not overly certain of how the API shall look like.
 * To get ahead, we just store the one resource we're interested in
 * directly. */
struct anno_instr_s {
	struct instr_s instr;
	struct cat_anno_s anno;
};

/**
 * The catalogue data structure.
 * Naive, just an array of instruments. */
struct cat_s {
	size_t ninstrs;
	size_t alloc_sz;
	struct anno_instr_s *instrs;
	struct keyval_s *keys;
	pthread_mutex_t mtx;
};

extern cat_t make_cat(void);
extern void free_cat(cat_t cat);

extern size_t cat_size(cat_t cat);

/**
 * Check if I is known, if not add it to the catalogue CAT.
 * If known replace the existing instrument in CAT by I. */
extern instr_t cat_bang_instr(cat_t cat, instr_t i);

/**
 * Annotate I.
 * I is assumed to stem from within the catalogue structure as data
 * is appended beyond its natural extent. */
static inline void
cat_annotate_instr(cat_t c, instr_t i, const char *tbl, time_t from, time_t to)
{
	return;
}

/**
 * Return the annotation of an instrument I from within the catalogue. */
static inline cat_anno_t
cat_instr_annotation(instr_t i)
{
	return &(((anno_instr_t)(void*)i)->anno);
}

extern instr_t find_instr_by_gaid(cat_t cat, gaid_t gaid);
extern instr_t find_instr_by_name(cat_t cat, const char *name);

#endif	/* INCLUDED_catalogue_h_ */
