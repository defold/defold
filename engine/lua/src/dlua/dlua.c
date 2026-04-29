#include <dmsdk/dlua/dlua.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

typedef const char* (*dlua_Reader)(dlua_State* L, void* ud, size_t* sz);
typedef int (*dlua_Writer)(dlua_State* L, const void* p, size_t sz, void* ud);
typedef void* (*dlua_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

#define DLUA_L(L) ((lua_State*)(L))
#define DLUA_DL(L) ((dlua_State*)(L))
#define DLUA_AR(ar) ((lua_Debug*)(ar))
#define DLUA_DAR(ar) ((dlua_Debug*)(ar))
#define DLUA_BUFFER(B) ((luaL_Buffer*)(B))
#define DLUA_REG(l) ((const luaL_Reg*)(l))

dlua_State* dlua_newstate(dlua_Alloc f, void* ud)
{
    return DLUA_DL(lua_newstate(f, ud));
}

void dlua_close(dlua_State* L)
{
    lua_close(DLUA_L(L));
}

dlua_State* dlua_newthread(dlua_State* L)
{
    return DLUA_DL(lua_newthread(DLUA_L(L)));
}

dlua_CFunction dlua_atpanic(dlua_State* L, dlua_CFunction panicf)
{
    return lua_atpanic(DLUA_L(L), panicf);
}

dlua_State* dlua_open(void)
{
    return dluaL_newstate();
}

int dlua_gettop(dlua_State* L)
{
    return lua_gettop(DLUA_L(L));
}

void dlua_settop(dlua_State* L, int idx)
{
    lua_settop(DLUA_L(L), idx);
}

void dlua_pushvalue(dlua_State* L, int idx)
{
    lua_pushvalue(DLUA_L(L), idx);
}

void dlua_remove(dlua_State* L, int idx)
{
    lua_remove(DLUA_L(L), idx);
}

void dlua_insert(dlua_State* L, int idx)
{
    lua_insert(DLUA_L(L), idx);
}

void dlua_replace(dlua_State* L, int idx)
{
    lua_replace(DLUA_L(L), idx);
}

int dlua_checkstack(dlua_State* L, int sz)
{
    return lua_checkstack(DLUA_L(L), sz);
}

void dlua_xmove(dlua_State* from, dlua_State* to, int n)
{
    lua_xmove(DLUA_L(from), DLUA_L(to), n);
}

int dlua_isnumber(dlua_State* L, int idx)
{
    return lua_isnumber(DLUA_L(L), idx);
}

int dlua_isstring(dlua_State* L, int idx)
{
    return lua_isstring(DLUA_L(L), idx);
}

int dlua_iscfunction(dlua_State* L, int idx)
{
    return lua_iscfunction(DLUA_L(L), idx);
}

int dlua_isuserdata(dlua_State* L, int idx)
{
    return lua_isuserdata(DLUA_L(L), idx);
}

int dlua_type(dlua_State* L, int idx)
{
    return lua_type(DLUA_L(L), idx);
}

const char* dlua_typename(dlua_State* L, int tp)
{
    return lua_typename(DLUA_L(L), tp);
}

int dlua_equal(dlua_State* L, int idx1, int idx2)
{
    return lua_equal(DLUA_L(L), idx1, idx2);
}

int dlua_rawequal(dlua_State* L, int idx1, int idx2)
{
    return lua_rawequal(DLUA_L(L), idx1, idx2);
}

int dlua_lessthan(dlua_State* L, int idx1, int idx2)
{
    return lua_lessthan(DLUA_L(L), idx1, idx2);
}

dlua_Number dlua_tonumber(dlua_State* L, int idx)
{
    return lua_tonumber(DLUA_L(L), idx);
}

dlua_Integer dlua_tointeger(dlua_State* L, int idx)
{
    return lua_tointeger(DLUA_L(L), idx);
}

int dlua_toboolean(dlua_State* L, int idx)
{
    return lua_toboolean(DLUA_L(L), idx);
}

const char* dlua_tolstring(dlua_State* L, int idx, size_t* len)
{
    return lua_tolstring(DLUA_L(L), idx, len);
}

size_t dlua_objlen(dlua_State* L, int idx)
{
    return lua_objlen(DLUA_L(L), idx);
}

dlua_CFunction dlua_tocfunction(dlua_State* L, int idx)
{
    return lua_tocfunction(DLUA_L(L), idx);
}

void* dlua_touserdata(dlua_State* L, int idx)
{
    return lua_touserdata(DLUA_L(L), idx);
}

dlua_State* dlua_tothread(dlua_State* L, int idx)
{
    return DLUA_DL(lua_tothread(DLUA_L(L), idx));
}

const void* dlua_topointer(dlua_State* L, int idx)
{
    return lua_topointer(DLUA_L(L), idx);
}

void dlua_pushnil(dlua_State* L)
{
    lua_pushnil(DLUA_L(L));
}

void dlua_pushnumber(dlua_State* L, dlua_Number n)
{
    lua_pushnumber(DLUA_L(L), n);
}

void dlua_pushinteger(dlua_State* L, dlua_Integer n)
{
    lua_pushinteger(DLUA_L(L), n);
}

void dlua_pushlstring(dlua_State* L, const char* s, size_t l)
{
    lua_pushlstring(DLUA_L(L), s, l);
}

void dlua_pushstring(dlua_State* L, const char* s)
{
    lua_pushstring(DLUA_L(L), s);
}

const char* dlua_pushvfstring(dlua_State* L, const char* fmt, va_list argp)
{
    return lua_pushvfstring(DLUA_L(L), fmt, argp);
}

const char* dlua_pushfstring(dlua_State* L, const char* fmt, ...)
{
    const char* result;
    va_list     argp;
    va_start(argp, fmt);
    result = lua_pushvfstring(DLUA_L(L), fmt, argp);
    va_end(argp);
    return result;
}

void dlua_pushcclosure(dlua_State* L, dlua_CFunction fn, int n)
{
    lua_pushcclosure(DLUA_L(L), fn, n);
}

void dlua_pushboolean(dlua_State* L, int b)
{
    lua_pushboolean(DLUA_L(L), b);
}

void dlua_pushlightuserdata(dlua_State* L, void* p)
{
    lua_pushlightuserdata(DLUA_L(L), p);
}

int dlua_pushthread(dlua_State* L)
{
    return lua_pushthread(DLUA_L(L));
}

void dlua_gettable(dlua_State* L, int idx)
{
    lua_gettable(DLUA_L(L), idx);
}

void dlua_getfield(dlua_State* L, int idx, const char* k)
{
    lua_getfield(DLUA_L(L), idx, k);
}

void dlua_rawget(dlua_State* L, int idx)
{
    lua_rawget(DLUA_L(L), idx);
}

void dlua_rawgeti(dlua_State* L, int idx, int n)
{
    lua_rawgeti(DLUA_L(L), idx, n);
}

void dlua_createtable(dlua_State* L, int narr, int nrec)
{
    lua_createtable(DLUA_L(L), narr, nrec);
}

void* dlua_newuserdata(dlua_State* L, size_t sz)
{
    return lua_newuserdata(DLUA_L(L), sz);
}

int dlua_getmetatable(dlua_State* L, int objindex)
{
    return lua_getmetatable(DLUA_L(L), objindex);
}

void dlua_getfenv(dlua_State* L, int idx)
{
    lua_getfenv(DLUA_L(L), idx);
}

void dlua_settable(dlua_State* L, int idx)
{
    lua_settable(DLUA_L(L), idx);
}

void dlua_setfield(dlua_State* L, int idx, const char* k)
{
    lua_setfield(DLUA_L(L), idx, k);
}

void dlua_rawset(dlua_State* L, int idx)
{
    lua_rawset(DLUA_L(L), idx);
}

void dlua_rawseti(dlua_State* L, int idx, int n)
{
    lua_rawseti(DLUA_L(L), idx, n);
}

int dlua_setmetatable(dlua_State* L, int objindex)
{
    return lua_setmetatable(DLUA_L(L), objindex);
}

int dlua_setfenv(dlua_State* L, int idx)
{
    return lua_setfenv(DLUA_L(L), idx);
}

void dlua_call(dlua_State* L, int nargs, int nresults)
{
    lua_call(DLUA_L(L), nargs, nresults);
}

int dlua_pcall(dlua_State* L, int nargs, int nresults, int errfunc)
{
    return lua_pcall(DLUA_L(L), nargs, nresults, errfunc);
}

int dlua_cpcall(dlua_State* L, dlua_CFunction func, void* ud)
{
    return lua_cpcall(DLUA_L(L), func, ud);
}

int dlua_load(dlua_State* L, dlua_Reader reader, void* dt, const char* chunkname)
{
    return lua_load(DLUA_L(L), reader, dt, chunkname);
}

int dlua_dump(dlua_State* L, dlua_Writer writer, void* data)
{
    return lua_dump(DLUA_L(L), writer, data);
}

int dlua_yield(dlua_State* L, int nresults)
{
    return lua_yield(DLUA_L(L), nresults);
}

int dlua_resume(dlua_State* L, int narg)
{
    return lua_resume(DLUA_L(L), narg);
}

int dlua_status(dlua_State* L)
{
    return lua_status(DLUA_L(L));
}

int dlua_gc(dlua_State* L, int what, int data)
{
    return lua_gc(DLUA_L(L), what, data);
}

int dlua_error(dlua_State* L)
{
    return lua_error(DLUA_L(L));
}

int dlua_next(dlua_State* L, int idx)
{
    return lua_next(DLUA_L(L), idx);
}

void dlua_concat(dlua_State* L, int n)
{
    lua_concat(DLUA_L(L), n);
}

dlua_Alloc dlua_getallocf(dlua_State* L, void** ud)
{
    return lua_getallocf(DLUA_L(L), ud);
}

void dlua_setallocf(dlua_State* L, dlua_Alloc f, void* ud)
{
    lua_setallocf(DLUA_L(L), f, ud);
}

void dlua_setlevel(dlua_State* from, dlua_State* to)
{
    lua_setlevel(DLUA_L(from), DLUA_L(to));
}

int dlua_getstack(dlua_State* L, int level, dlua_Debug* ar)
{
    return lua_getstack(DLUA_L(L), level, DLUA_AR(ar));
}

int dlua_getinfo(dlua_State* L, const char* what, dlua_Debug* ar)
{
    return lua_getinfo(DLUA_L(L), what, DLUA_AR(ar));
}

const char* dlua_getlocal(dlua_State* L, const dlua_Debug* ar, int n)
{
    return lua_getlocal(DLUA_L(L), (const lua_Debug*)ar, n);
}

const char* dlua_setlocal(dlua_State* L, const dlua_Debug* ar, int n)
{
    return lua_setlocal(DLUA_L(L), (const lua_Debug*)ar, n);
}

const char* dlua_getupvalue(dlua_State* L, int funcindex, int n)
{
    return lua_getupvalue(DLUA_L(L), funcindex, n);
}

const char* dlua_setupvalue(dlua_State* L, int funcindex, int n)
{
    return lua_setupvalue(DLUA_L(L), funcindex, n);
}

int dlua_sethook(dlua_State* L, dlua_Hook func, int mask, int count)
{
    return lua_sethook(DLUA_L(L), (lua_Hook)func, mask, count);
}

dlua_Hook dlua_gethook(dlua_State* L)
{
    return (dlua_Hook)lua_gethook(DLUA_L(L));
}

int dlua_gethookmask(dlua_State* L)
{
    return lua_gethookmask(DLUA_L(L));
}

int dlua_gethookcount(dlua_State* L)
{
    return lua_gethookcount(DLUA_L(L));
}

void dluaL_register(dlua_State* L, const char* libname, const dluaL_Reg* l)
{
    luaL_register(DLUA_L(L), libname, DLUA_REG(l));
}

void dluaL_openlib(dlua_State* L, const char* libname, const dluaL_Reg* l, int nup)
{
    luaL_openlib(DLUA_L(L), libname, DLUA_REG(l), nup);
}

int dluaL_getmetafield(dlua_State* L, int obj, const char* e)
{
    return luaL_getmetafield(DLUA_L(L), obj, e);
}

int dluaL_callmeta(dlua_State* L, int obj, const char* e)
{
    return luaL_callmeta(DLUA_L(L), obj, e);
}

int dluaL_typerror(dlua_State* L, int narg, const char* tname)
{
    return luaL_typerror(DLUA_L(L), narg, tname);
}

int dluaL_argerror(dlua_State* L, int numarg, const char* extramsg)
{
    return luaL_argerror(DLUA_L(L), numarg, extramsg);
}

const char* dluaL_checklstring(dlua_State* L, int numArg, size_t* l)
{
    return luaL_checklstring(DLUA_L(L), numArg, l);
}

const char* dluaL_optlstring(dlua_State* L, int numArg, const char* def, size_t* l)
{
    return luaL_optlstring(DLUA_L(L), numArg, def, l);
}

dlua_Number dluaL_checknumber(dlua_State* L, int numArg)
{
    return luaL_checknumber(DLUA_L(L), numArg);
}

dlua_Number dluaL_optnumber(dlua_State* L, int nArg, dlua_Number def)
{
    return luaL_optnumber(DLUA_L(L), nArg, def);
}

dlua_Integer dluaL_checkinteger(dlua_State* L, int numArg)
{
    return luaL_checkinteger(DLUA_L(L), numArg);
}

dlua_Integer dluaL_optinteger(dlua_State* L, int nArg, dlua_Integer def)
{
    return luaL_optinteger(DLUA_L(L), nArg, def);
}

void dluaL_checkstack(dlua_State* L, int sz, const char* msg)
{
    luaL_checkstack(DLUA_L(L), sz, msg);
}

void dluaL_checktype(dlua_State* L, int narg, int t)
{
    luaL_checktype(DLUA_L(L), narg, t);
}

void dluaL_checkany(dlua_State* L, int narg)
{
    luaL_checkany(DLUA_L(L), narg);
}

int dluaL_newmetatable(dlua_State* L, const char* tname)
{
    return luaL_newmetatable(DLUA_L(L), tname);
}

void* dluaL_checkudata(dlua_State* L, int ud, const char* tname)
{
    return luaL_checkudata(DLUA_L(L), ud, tname);
}

void dluaL_where(dlua_State* L, int lvl)
{
    luaL_where(DLUA_L(L), lvl);
}

int dluaL_error(dlua_State* L, const char* fmt, ...)
{
    va_list argp;
    va_start(argp, fmt);
    luaL_where(DLUA_L(L), 1);
    lua_pushvfstring(DLUA_L(L), fmt, argp);
    va_end(argp);
    lua_concat(DLUA_L(L), 2);
    return lua_error(DLUA_L(L));
}

int dluaL_checkoption(dlua_State* L, int narg, const char* def, const char* const lst[])
{
    return luaL_checkoption(DLUA_L(L), narg, def, lst);
}

int dluaL_ref(dlua_State* L, int t)
{
    return luaL_ref(DLUA_L(L), t);
}

void dluaL_unref(dlua_State* L, int t, int ref)
{
    luaL_unref(DLUA_L(L), t, ref);
}

int dluaL_loadfile(dlua_State* L, const char* filename)
{
    return luaL_loadfile(DLUA_L(L), filename);
}

int dluaL_loadbuffer(dlua_State* L, const char* buff, size_t sz, const char* name)
{
    return luaL_loadbuffer(DLUA_L(L), buff, sz, name);
}

int dluaL_loadstring(dlua_State* L, const char* s)
{
    return luaL_loadstring(DLUA_L(L), s);
}

dlua_State* dluaL_newstate(void)
{
    return DLUA_DL(luaL_newstate());
}

const char* dluaL_gsub(dlua_State* L, const char* s, const char* p, const char* r)
{
    return luaL_gsub(DLUA_L(L), s, p, r);
}

const char* dluaL_findtable(dlua_State* L, int idx, const char* fname, int szhint)
{
    return luaL_findtable(DLUA_L(L), idx, fname, szhint);
}

void dluaL_buffinit(dlua_State* L, dluaL_Buffer* B)
{
    luaL_buffinit(DLUA_L(L), DLUA_BUFFER(B));
}

char* dluaL_prepbuffer(dluaL_Buffer* B)
{
    return luaL_prepbuffer(DLUA_BUFFER(B));
}

void dluaL_addlstring(dluaL_Buffer* B, const char* s, size_t l)
{
    luaL_addlstring(DLUA_BUFFER(B), s, l);
}

void dluaL_addstring(dluaL_Buffer* B, const char* s)
{
    luaL_addstring(DLUA_BUFFER(B), s);
}

void dluaL_addvalue(dluaL_Buffer* B)
{
    luaL_addvalue(DLUA_BUFFER(B));
}

void dluaL_pushresult(dluaL_Buffer* B)
{
    luaL_pushresult(DLUA_BUFFER(B));
}

void dluaL_openlibs(dlua_State* L)
{
    luaL_openlibs(DLUA_L(L));
}

int dluaopen_base(dlua_State* L)
{
    return luaopen_base(DLUA_L(L));
}

int dluaopen_table(dlua_State* L)
{
    return luaopen_table(DLUA_L(L));
}

int dluaopen_io(dlua_State* L)
{
    return luaopen_io(DLUA_L(L));
}

int dluaopen_os(dlua_State* L)
{
    return luaopen_os(DLUA_L(L));
}

int dluaopen_string(dlua_State* L)
{
    return luaopen_string(DLUA_L(L));
}

int dluaopen_math(dlua_State* L)
{
    return luaopen_math(DLUA_L(L));
}

int dluaopen_debug(dlua_State* L)
{
    return luaopen_debug(DLUA_L(L));
}

int dluaopen_package(dlua_State* L)
{
    return luaopen_package(DLUA_L(L));
}
