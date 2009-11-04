/*** lua-config.c -- unserding configuration via lua
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

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include <lua.h>
#include <lauxlib.h>

#include "lua-config.h"


/* inlines */
static inline void*
lc_ref(void *L)
{
	int r = luaL_ref(L, LUA_REGISTRYINDEX);
	return (void*)(long int)r;
}

static inline void
lc_deref(void *L, void *ref)
{
	int r = (long int)ref;
	lua_rawgeti(L, LUA_REGISTRYINDEX, r);
	return;
}

static inline void
lc_freeref(void *L, void *ref)
{
	int r = (long int)ref;
	luaL_unref(L, LUA_REGISTRYINDEX, r);
}


/* config setting fetchers */
void*
lc_cfgtbl_lookup(void *L, void *s, const char *name)
{
	void *res;

	/* push the value of S on the stack */
	lc_deref(L, s);
	if (lua_istable(L, -1)) {
		/* get the NAME slot */
		lua_getfield(L, -1, name);
		/* ref the result for later use, fucking memleak */
		if (!lua_isnoneornil(L, -1)) {
			res = lc_ref(L);
		} else {
			res = NULL;
		}
	} else {
		/* dont bother passing NILs around */
		res = NULL;
	}
	/* pop S */
	lua_pop(L, 1);
	return res;
}

void
lc_cfgtbl_free(void *L, void *s)
{
	lc_freeref(L, s);
	return;
}

size_t
lc_cfgtbl_lookup_s(const char **res, void *L, void *s, const char *name)
{
	size_t len;

	lc_deref(L, s);
	lua_getfield(L, -1, name);
	*res = lua_tolstring(L, -1, &len);
	lua_pop(L, 1);
	return len;
}

size_t
lc_globcfg_lookup_s(const char **res, void *L, const char *name)
{
	size_t len;

	lua_getglobal(L, name);
	*res = lua_tolstring(L, -1, &len);
	lua_pop(L, 1);
	return len;
}

bool
lc_globcfg_lookup_b(void *L, const char *name)
{
	bool res;

	lua_getglobal(L, name);
	res = lua_toboolean(L, -1);
	lua_pop(L, 1);
	return res;
}


/* i'm too lazy to include module.h */
extern void ud_defer_dso(const char *name, void *cfgset);

static int
lc_load_module(lua_State *L)
{
	const char *p;
	void *cfgset;

	if (!lua_istable(L, 1)) {
		fprintf(stderr, "need a table you fuckwit\n");
		return 0;
	}
	cfgset = lc_ref(L);

	lc_cfgtbl_lookup_s(&p, L, cfgset, "file");
	ud_defer_dso(p, cfgset);
	return 0;
}

static void
register_funs(lua_State *L)
{
	lua_register(L, "load_module", lc_load_module);
	return;
}

bool
read_lua_config(void *L, const char *file)
{
	int res;

	/* eval the input file */
	res = luaL_dofile(L, file);
	return res == 0;
}

void
lua_config_init(void **state)
{
	*state = lua_open();
	/* add our bindings */
	register_funs(*state);
	return;
}

void
lua_config_deinit(void **state)
{
	lua_close(*state);
	return;
}

#if defined STANDALONE
int
main(int argc, const char *argv[])
{
	read_lua_config(argv[1]);
	return 0;
}
#endif	/* STANDALONE */

/* lua-config.c ends here */
