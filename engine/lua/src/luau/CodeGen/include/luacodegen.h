// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

// Can be used to reconfigure visibility/exports for public APIs
#ifndef LUACODEGEN_API
#define LUACODEGEN_API extern
#endif

typedef struct lua_State lua_State;

// returns 1 if Luau code generator is supported, 0 otherwise
LUACODEGEN_API int luau_codegen_supported(void);

// create an instance of Luau code generator. you must check that this feature is supported using luau_codegen_supported().
LUACODEGEN_API void luau_codegen_create(lua_State* L);

// build target function and all inner functions
LUACODEGEN_API void luau_codegen_compile(lua_State* L, int idx);
