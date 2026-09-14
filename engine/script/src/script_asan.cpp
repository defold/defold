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

#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}

#undef lua_error
#undef luaL_error

extern "C" void __asan_handle_no_return();

// Keep the sanitizer wrappers in their own object so linking them doesn't
// pull in script initialization.
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

#if defined(DM_ASAN_WRAP_UNWIND)
#include <unwind.h>

extern "C" _Unwind_Reason_Code __real__Unwind_RaiseException(_Unwind_Exception* exception_object);

extern "C" _Unwind_Reason_Code __wrap__Unwind_RaiseException(_Unwind_Exception* exception_object)
{
    // LuaJIT's internal argument errors (e.g. luaL_checktype) bypass our
    // lua_error/luaL_error wrappers and reach _Unwind_RaiseException.
    // With NDK 27, static libunwind.a supplies a local, hidden symbol, so
    // ASAN's runtime interceptor cannot intercept this call. Restore the
    // __asan_handle_no_return() call that the interceptor normally makes
    // before unwinding. Otherwise, discarded stack redzones remain poisoned
    // and can cause false ASAN reports when the stack is reused.
    __asan_handle_no_return();
    return __real__Unwind_RaiseException(exception_object);
}
#endif
#endif
