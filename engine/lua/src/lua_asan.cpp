// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

extern "C"
{
#ifdef LUA_ANSI
#undef LUA_ANSI
#define LUA_ANSI
#endif
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
#undef lua_error
#undef luaL_error

extern "C" void __asan_handle_no_return();

extern "C" int dm_lua_error_asan(lua_State* L)
{
    __asan_handle_no_return();
    return DM_LUA_RENAME(lua_error)(L);
}

extern "C" int dm_luaL_error_asan(lua_State* L, const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    luaL_where(L, 1);
    lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    lua_concat(L, 2);
    return dm_lua_error_asan(L);
}
#endif
