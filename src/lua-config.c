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

static int
lc_load_module(lua_State *L)
{
	size_t len;
	const char *p;

	if (!lua_istable(L, 1)) {
		fprintf(stderr, "need a table you fuckwit\n");
		return 0;
	}

	lua_getfield(L, 1, "file");
	p = lua_tolstring(L, 2, &len);
	fprintf(stderr, "loading %s\n", p);
	return 0;
}

static void
register_funs(lua_State *L)
{
	lua_register(L, "load_module", lc_load_module);
	return;
}

void
read_lua_config(const char *file)
{
	lua_State *L = lua_open();

	/* add our bindings */
	register_funs(L);
	/* eval the input file */
	(void)luaL_dofile(L, file);
	/* and fuck off */
	lua_close(L);
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
