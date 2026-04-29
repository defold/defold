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

#ifndef DMSDK_DLUA_H
#define DMSDK_DLUA_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef DLUA_API
#define DLUA_API extern
#endif

#define DLUA_NUMBER_FMT "%.14g"

#define DLUA_MULTRET (-1)
#define DLUA_REGISTRYINDEX (-10000)
#define DLUA_GLOBALSINDEX (-10002)
#define dlua_upvalueindex(i) (DLUA_GLOBALSINDEX - (i))

#define DLUA_YIELD 1
#define DLUA_ERRRUN 2
#define DLUA_ERRMEM 4

#define DLUA_TNONE (-1)
#define DLUA_TNIL 0
#define DLUA_TBOOLEAN 1
#define DLUA_TLIGHTUSERDATA 2
#define DLUA_TNUMBER 3
#define DLUA_TSTRING 4
#define DLUA_TTABLE 5
#define DLUA_TFUNCTION 6
#define DLUA_TUSERDATA 7
#define DLUA_TTHREAD 8

#define DLUA_GCCOLLECT 2
#define DLUA_GCCOUNT 3

#define DLUA_MASKCALL (1 << 0)

#define DLUA_NOREF (-2)
#define DLUA_REFNIL (-1)

#define DLUAL_BUFFERSIZE BUFSIZ
#define DLUA_IDSIZE 60

    typedef struct lua_State  dlua_State;
    typedef struct dlua_Debug dlua_Debug;

    typedef double            dlua_Number;
    typedef ptrdiff_t         dlua_Integer;

    typedef int (*dlua_CFunction)(dlua_State* L);
    typedef void (*dlua_Hook)(dlua_State* L, dlua_Debug* ar);

    typedef struct dluaL_Reg
    {
        const char*    name;
        dlua_CFunction func;
    } dluaL_Reg;

    typedef dluaL_Reg dluaL_reg;

    typedef struct dluaL_Buffer
    {
        char*       p;
        int         lvl;
        dlua_State* L;
        char        buffer[DLUAL_BUFFERSIZE];
    } dluaL_Buffer;

    struct dlua_Debug
    {
        int         event;
        const char* name;
        const char* namewhat;
        const char* what;
        const char* source;
        int         currentline;
        int         nups;
        int         linedefined;
        int         lastlinedefined;
        char        short_src[DLUA_IDSIZE];
        int         i_ci;
    };

    DLUA_API dlua_State*    dlua_open(void);
    DLUA_API dlua_State*    dluaL_newstate(void);
    DLUA_API void           dlua_close(dlua_State* L);
    DLUA_API dlua_CFunction dlua_atpanic(dlua_State* L, dlua_CFunction panicf);

    DLUA_API int            dlua_gettop(dlua_State* L);
    DLUA_API void           dlua_settop(dlua_State* L, int idx);
    DLUA_API void           dlua_pushvalue(dlua_State* L, int idx);
    DLUA_API void           dlua_remove(dlua_State* L, int idx);
    DLUA_API void           dlua_insert(dlua_State* L, int idx);
    DLUA_API void           dlua_replace(dlua_State* L, int idx);
    DLUA_API int            dlua_checkstack(dlua_State* L, int sz);

    DLUA_API int            dlua_isnumber(dlua_State* L, int idx);
    DLUA_API int            dlua_isstring(dlua_State* L, int idx);
    DLUA_API int            dlua_isuserdata(dlua_State* L, int idx);
    DLUA_API int            dlua_type(dlua_State* L, int idx);
    DLUA_API const char*    dlua_typename(dlua_State* L, int tp);
    DLUA_API int            dlua_rawequal(dlua_State* L, int idx1, int idx2);
    DLUA_API dlua_Number    dlua_tonumber(dlua_State* L, int idx);
    DLUA_API dlua_Integer   dlua_tointeger(dlua_State* L, int idx);
    DLUA_API int            dlua_toboolean(dlua_State* L, int idx);
    DLUA_API const char*    dlua_tolstring(dlua_State* L, int idx, size_t* len);
    DLUA_API size_t         dlua_objlen(dlua_State* L, int idx);
    DLUA_API void*          dlua_touserdata(dlua_State* L, int idx);
    DLUA_API dlua_State*    dlua_tothread(dlua_State* L, int idx);
    DLUA_API const void*    dlua_topointer(dlua_State* L, int idx);

    DLUA_API void           dlua_pushnil(dlua_State* L);
    DLUA_API void           dlua_pushnumber(dlua_State* L, dlua_Number n);
    DLUA_API void           dlua_pushinteger(dlua_State* L, dlua_Integer n);
    DLUA_API void           dlua_pushlstring(dlua_State* L, const char* s, size_t l);
    DLUA_API void           dlua_pushstring(dlua_State* L, const char* s);
    DLUA_API const char*    dlua_pushvfstring(dlua_State* L, const char* fmt, va_list argp);
#if defined(__GNUC__)
    DLUA_API const char* dlua_pushfstring(dlua_State* L, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#else
DLUA_API const char* dlua_pushfstring(dlua_State* L, const char* fmt, ...);
#endif
    DLUA_API void         dlua_pushcclosure(dlua_State* L, dlua_CFunction fn, int n);
    DLUA_API void         dlua_pushboolean(dlua_State* L, int b);
    DLUA_API void         dlua_pushlightuserdata(dlua_State* L, void* p);
    DLUA_API int          dlua_pushthread(dlua_State* L);

    DLUA_API void         dlua_gettable(dlua_State* L, int idx);
    DLUA_API void         dlua_getfield(dlua_State* L, int idx, const char* k);
    DLUA_API void         dlua_rawget(dlua_State* L, int idx);
    DLUA_API void         dlua_rawgeti(dlua_State* L, int idx, int n);
    DLUA_API void         dlua_createtable(dlua_State* L, int narr, int nrec);
    DLUA_API void*        dlua_newuserdata(dlua_State* L, size_t sz);
    DLUA_API int          dlua_getmetatable(dlua_State* L, int objindex);

    DLUA_API void         dlua_settable(dlua_State* L, int idx);
    DLUA_API void         dlua_setfield(dlua_State* L, int idx, const char* k);
    DLUA_API void         dlua_rawset(dlua_State* L, int idx);
    DLUA_API void         dlua_rawseti(dlua_State* L, int idx, int n);
    DLUA_API int          dlua_setmetatable(dlua_State* L, int objindex);

    DLUA_API void         dlua_call(dlua_State* L, int nargs, int nresults);
    DLUA_API int          dlua_pcall(dlua_State* L, int nargs, int nresults, int errfunc);
    DLUA_API int          dlua_cpcall(dlua_State* L, dlua_CFunction func, void* ud);

    DLUA_API int          dlua_status(dlua_State* L);
    DLUA_API int          dlua_gc(dlua_State* L, int what, int data);
    DLUA_API int          dlua_error(dlua_State* L);
    DLUA_API int          dlua_next(dlua_State* L, int idx);
    DLUA_API void         dlua_concat(dlua_State* L, int n);

    DLUA_API int          dlua_getstack(dlua_State* L, int level, dlua_Debug* ar);
    DLUA_API int          dlua_getinfo(dlua_State* L, const char* what, dlua_Debug* ar);
    DLUA_API int          dlua_sethook(dlua_State* L, dlua_Hook func, int mask, int count);

    DLUA_API void         dluaL_openlib(dlua_State* L, const char* libname, const dluaL_Reg* l, int nup);
    DLUA_API void         dluaL_register(dlua_State* L, const char* libname, const dluaL_Reg* l);
    DLUA_API int          dluaL_callmeta(dlua_State* L, int obj, const char* e);
    DLUA_API int          dluaL_typerror(dlua_State* L, int narg, const char* tname);
    DLUA_API int          dluaL_argerror(dlua_State* L, int numarg, const char* extramsg);
    DLUA_API const char*  dluaL_checklstring(dlua_State* L, int numArg, size_t* l);
    DLUA_API const char*  dluaL_optlstring(dlua_State* L, int numArg, const char* def, size_t* l);
    DLUA_API dlua_Number  dluaL_checknumber(dlua_State* L, int numArg);
    DLUA_API dlua_Number  dluaL_optnumber(dlua_State* L, int nArg, dlua_Number def);
    DLUA_API dlua_Integer dluaL_checkinteger(dlua_State* L, int numArg);
    DLUA_API void         dluaL_checkstack(dlua_State* L, int sz, const char* msg);
    DLUA_API void         dluaL_checktype(dlua_State* L, int narg, int t);
    DLUA_API void         dluaL_checkany(dlua_State* L, int narg);
    DLUA_API int          dluaL_newmetatable(dlua_State* L, const char* tname);
    DLUA_API void*        dluaL_checkudata(dlua_State* L, int ud, const char* tname);
    DLUA_API void         dluaL_where(dlua_State* L, int lvl);
#if defined(__GNUC__)
    DLUA_API int dluaL_error(dlua_State* L, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#else
DLUA_API int dluaL_error(dlua_State* L, const char* fmt, ...);
#endif
    DLUA_API int   dluaL_checkoption(dlua_State* L, int narg, const char* def, const char* const lst[]);
    DLUA_API int   dluaL_ref(dlua_State* L, int t);
    DLUA_API void  dluaL_unref(dlua_State* L, int t, int ref);
    DLUA_API int   dluaL_loadfile(dlua_State* L, const char* filename);
    DLUA_API int   dluaL_loadbuffer(dlua_State* L, const char* buff, size_t sz, const char* name);
    DLUA_API int   dluaL_loadstring(dlua_State* L, const char* s);
    DLUA_API void  dluaL_buffinit(dlua_State* L, dluaL_Buffer* B);
    DLUA_API char* dluaL_prepbuffer(dluaL_Buffer* B);
    DLUA_API void  dluaL_addlstring(dluaL_Buffer* B, const char* s, size_t l);
    DLUA_API void  dluaL_addstring(dluaL_Buffer* B, const char* s);
    DLUA_API void  dluaL_pushresult(dluaL_Buffer* B);
    DLUA_API void  dluaL_openlibs(dlua_State* L);

#define dlua_pop(L, n) dlua_settop((L), -(n) - 1)
#define dlua_newtable(L) dlua_createtable((L), 0, 0)
#define dlua_register(L, n, f) (dlua_pushcfunction((L), (f)), dlua_setglobal((L), (n)))
#define dlua_pushcfunction(L, f) dlua_pushcclosure((L), (f), 0)
#define dlua_strlen(L, i) dlua_objlen((L), (i))
#define dlua_isfunction(L, n) (dlua_type((L), (n)) == DLUA_TFUNCTION)
#define dlua_istable(L, n) (dlua_type((L), (n)) == DLUA_TTABLE)
#define dlua_isnil(L, n) (dlua_type((L), (n)) == DLUA_TNIL)
#define dlua_isboolean(L, n) (dlua_type((L), (n)) == DLUA_TBOOLEAN)
#define dlua_isthread(L, n) (dlua_type((L), (n)) == DLUA_TTHREAD)
#define dlua_isnone(L, n) (dlua_type((L), (n)) == DLUA_TNONE)
#define dlua_isnoneornil(L, n) (dlua_type((L), (n)) <= 0)
#define dlua_pushliteral(L, s) dlua_pushlstring((L), "" s, (sizeof(s) / sizeof(char)) - 1)
#define dlua_setglobal(L, s) dlua_setfield((L), DLUA_GLOBALSINDEX, (s))
#define dlua_getglobal(L, s) dlua_getfield((L), DLUA_GLOBALSINDEX, (s))
#define dlua_tostring(L, i) dlua_tolstring((L), (i), NULL)

#define dluaL_argcheck(L, cond, numarg, extramsg) ((void)((cond) || dluaL_argerror((L), (numarg), (extramsg))))
#define dluaL_checkstring(L, n) dluaL_checklstring((L), (n), NULL)
#define dluaL_optstring(L, n, d) dluaL_optlstring((L), (n), (d), NULL)
#define dluaL_checkint(L, n) ((int)dluaL_checkinteger((L), (n)))
#define dluaL_typename(L, i) dlua_typename((L), dlua_type((L), (i)))
#define dluaL_dofile(L, fn) (dluaL_loadfile((L), (fn)) || dlua_pcall((L), 0, DLUA_MULTRET, 0))
#define dluaL_dostring(L, s) (dluaL_loadstring((L), (s)) || dlua_pcall((L), 0, DLUA_MULTRET, 0))
#define dluaL_getmetatable(L, n) dlua_getfield((L), DLUA_REGISTRYINDEX, (n))
#define dluaL_addchar(B, c) ((void)((B)->p < ((B)->buffer + DLUAL_BUFFERSIZE) || dluaL_prepbuffer((B))), (*(B)->p++ = (char)(c)))

#define dlua_ref(L, lock) ((lock) ? dluaL_ref((L), DLUA_REGISTRYINDEX) : (dlua_pushstring((L), "unlocked references are obsolete"), dlua_error((L)), 0))

#ifdef __cplusplus
}
#endif

#endif DMSDK_DLUA_H
