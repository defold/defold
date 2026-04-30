#include <dmsdk/dlua/dlua.h>

#include "lua.h"
#if defined(DM_DLUA_BACKEND_LUAU)
#include "lualib.h"
#else
#include "lauxlib.h"
#include "lualib.h"
#endif

#if defined(DM_DLUA_BACKEND_LUAU)
#include "luacode.h"
#if defined(DM_DLUA_LUAU_CODEGEN)
#include "luacodegen.h"
#endif
#include <stdlib.h>
#include <string.h>
#endif

typedef const char* (*dlua_Reader)(dlua_State* L, void* ud, size_t* sz);
typedef int (*dlua_Writer)(dlua_State* L, const void* p, size_t sz, void* ud);
typedef void* (*dlua_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

#define DLUA_L(L) ((lua_State*)(L))
#define DLUA_DL(L) ((dlua_State*)(L))
#define DLUA_AR(ar) ((lua_Debug*)(ar))
#define DLUA_DAR(ar) ((dlua_Debug*)(ar))
#define DLUA_BUFFER(B) ((luaL_Buffer*)(B))
#define DLUA_REG(l) ((const luaL_Reg*)(l))

#if defined(DM_DLUA_BACKEND_LUAU)
static void dlua_CopyLuauDebug(dlua_Debug* to, const lua_Debug* from, int level)
{
    memset(to, 0, sizeof(*to));

    to->name            = from->name ? from->name : "";
    to->namewhat        = from->name ? "function" : "";
    to->what            = from->what ? from->what : "";
    to->source          = from->source ? from->source : "";
    to->currentline     = from->currentline;
    to->nups            = from->nupvals;
    to->linedefined     = from->linedefined;
    to->lastlinedefined = from->linedefined;
    to->i_ci            = level;

    if (from->short_src)
    {
        strncpy(to->short_src, from->short_src, sizeof(to->short_src) - 1);
        to->short_src[sizeof(to->short_src) - 1] = '\0';
    }

    if (strcmp(to->what, "Lua") == 0 && to->linedefined == 0)
    {
        to->what = "main";
    }
}

static void dlua_CopyLua51DebugOptions(const char* from, char* to, size_t to_size)
{
    size_t index = 0;
    for (; *from && index + 1 < to_size; ++from)
    {
        char option = *from;
        if (option == 'S')
        {
            option = 's';
        }

        if (option == 'L')
        {
            continue;
        }

        to[index++] = option;
    }
    to[index] = '\0';
}

static int dlua_LuauAbsIndex(dlua_State* L, int idx)
{
    return idx > 0 || idx <= DLUA_REGISTRYINDEX ? idx : dlua_gettop(L) + idx + 1;
}

static int dlua_ToLuaType(int type)
{
    switch ((dlua_Type)type)
    {
    case DLUA_TNONE:          return LUA_TNONE;
    case DLUA_TNIL:           return LUA_TNIL;
    case DLUA_TBOOLEAN:       return LUA_TBOOLEAN;
    case DLUA_TLIGHTUSERDATA: return LUA_TLIGHTUSERDATA;
    case DLUA_TNUMBER:        return LUA_TNUMBER;
    case DLUA_TSTRING:        return LUA_TSTRING;
    case DLUA_TTABLE:         return LUA_TTABLE;
    case DLUA_TFUNCTION:      return LUA_TFUNCTION;
    case DLUA_TUSERDATA:      return LUA_TUSERDATA;
    case DLUA_TTHREAD:        return LUA_TTHREAD;
    }
    return type;
}

static dlua_Type dlua_FromLuaType(int type)
{
    switch (type)
    {
    case LUA_TNONE:          return DLUA_TNONE;
    case LUA_TNIL:           return DLUA_TNIL;
    case LUA_TBOOLEAN:       return DLUA_TBOOLEAN;
    case LUA_TLIGHTUSERDATA: return DLUA_TLIGHTUSERDATA;
    case LUA_TNUMBER:        return DLUA_TNUMBER;
    case LUA_TINTEGER:       return DLUA_TNUMBER;
    case LUA_TVECTOR:        return DLUA_TUSERDATA;
    case LUA_TSTRING:        return DLUA_TSTRING;
    case LUA_TTABLE:         return DLUA_TTABLE;
    case LUA_TFUNCTION:      return DLUA_TFUNCTION;
    case LUA_TUSERDATA:      return DLUA_TUSERDATA;
    case LUA_TTHREAD:        return DLUA_TTHREAD;
    case LUA_TBUFFER:        return DLUA_TUSERDATA;
    }
    return (dlua_Type)type;
}

#if defined(DM_DLUA_LUAU_CODEGEN)
static int dlua_LuauCodeGenSupported(void)
{
    static int supported = -1;
    if (supported < 0)
    {
        supported = luau_codegen_supported();
    }
    return supported;
}

static void dlua_LuauCodeGenCreate(lua_State* L)
{
    if (L != 0 && dlua_LuauCodeGenSupported())
    {
        luau_codegen_create(L);
    }
}

static void dlua_LuauCodeGenCompile(lua_State* L, int idx)
{
    if (dlua_LuauCodeGenSupported())
    {
        luau_codegen_compile(L, idx);
    }
}
#else
static void dlua_LuauCodeGenCreate(lua_State* L)
{
    (void)L;
}

static void dlua_LuauCodeGenCompile(lua_State* L, int idx)
{
    (void)L;
    (void)idx;
}
#endif
#endif

dlua_State* dlua_newstate(dlua_Alloc f, void* ud)
{
    lua_State* L = lua_newstate(f, ud);
#if defined(DM_DLUA_BACKEND_LUAU)
    dlua_LuauCodeGenCreate(L);
#endif
    return DLUA_DL(L);
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
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    (void)panicf;
    return 0;
#else
    return lua_atpanic(DLUA_L(L), panicf);
#endif
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

dlua_Type dlua_type(dlua_State* L, int idx)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    return dlua_FromLuaType(lua_type(DLUA_L(L), idx));
#else
    return (dlua_Type)lua_type(DLUA_L(L), idx);
#endif
}

const char* dlua_typename(dlua_State* L, int tp)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    return lua_typename(DLUA_L(L), dlua_ToLuaType(tp));
#else
    return lua_typename(DLUA_L(L), tp);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    lua_pushcclosure(DLUA_L(L), fn, "dlua", n);
#else
    lua_pushcclosure(DLUA_L(L), fn, n);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)reader;
    (void)dt;
    lua_pushfstring(DLUA_L(L), "%s is not supported by the Luau backend", "dlua_load");
    return LUA_ERRRUN;
#else
    return lua_load(DLUA_L(L), reader, dt, chunkname);
#endif
}

int dlua_dump(dlua_State* L, dlua_Writer writer, void* data)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)writer;
    (void)data;
    lua_pushfstring(DLUA_L(L), "%s is not supported by the Luau backend", "dlua_dump");
    return 1;
#else
    return lua_dump(DLUA_L(L), writer, data);
#endif
}

int dlua_yield(dlua_State* L, int nresults)
{
    return lua_yield(DLUA_L(L), nresults);
}

int dlua_resume(dlua_State* L, int narg)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    return lua_resume(DLUA_L(L), 0, narg);
#else
    return lua_resume(DLUA_L(L), narg);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    lua_error(DLUA_L(L));
    return 0;
#else
    return lua_error(DLUA_L(L));
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    (void)f;
    (void)ud;
#else
    lua_setallocf(DLUA_L(L), f, ud);
#endif
}

void dlua_setlevel(dlua_State* from, dlua_State* to)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)from;
    (void)to;
#else
    lua_setlevel(DLUA_L(from), DLUA_L(to));
#endif
}

int dlua_getstack(dlua_State* L, int level, dlua_Debug* ar)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    lua_Debug luau_ar;
    memset(&luau_ar, 0, sizeof(luau_ar));
    if (!lua_getinfo(DLUA_L(L), level, "", &luau_ar))
    {
        return 0;
    }

    memset(ar, 0, sizeof(*ar));
    ar->name     = "";
    ar->namewhat = "";
    ar->what     = "";
    ar->source   = "";
    ar->i_ci     = level;
    return 1;
#else
    return lua_getstack(DLUA_L(L), level, DLUA_AR(ar));
#endif
}

int dlua_getinfo(dlua_State* L, const char* what, dlua_Debug* ar)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    char luau_what[16];
    int inspect_stack_function = what[0] == '>';
    int level = inspect_stack_function ? -1 : ar->i_ci;
    lua_Debug luau_ar;

    dlua_CopyLua51DebugOptions(inspect_stack_function ? what + 1 : what, luau_what, sizeof(luau_what));
    memset(&luau_ar, 0, sizeof(luau_ar));
    int result = lua_getinfo(DLUA_L(L), level, luau_what, &luau_ar);
    if (inspect_stack_function)
    {
        if (result && strchr(luau_what, 'f'))
        {
            dlua_remove(L, -2);
        }
        else
        {
            dlua_pop(L, 1);
        }
    }

    if (!result)
    {
        return 0;
    }

    dlua_CopyLuauDebug(ar, &luau_ar, level);
    return 1;
#else
    return lua_getinfo(DLUA_L(L), what, DLUA_AR(ar));
#endif
}

const char* dlua_getlocal(dlua_State* L, const dlua_Debug* ar, int n)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    (void)ar;
    (void)n;
    return 0;
#else
    return lua_getlocal(DLUA_L(L), (const lua_Debug*)ar, n);
#endif
}

const char* dlua_setlocal(dlua_State* L, const dlua_Debug* ar, int n)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    (void)ar;
    (void)n;
    return 0;
#else
    return lua_setlocal(DLUA_L(L), (const lua_Debug*)ar, n);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    (void)func;
    (void)mask;
    (void)count;
    return 0;
#else
    return lua_sethook(DLUA_L(L), (lua_Hook)func, mask, count);
#endif
}

dlua_Hook dlua_gethook(dlua_State* L)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    return 0;
#else
    return (dlua_Hook)lua_gethook(DLUA_L(L));
#endif
}

int dlua_gethookmask(dlua_State* L)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    return 0;
#else
    return lua_gethookmask(DLUA_L(L));
#endif
}

int dlua_gethookcount(dlua_State* L)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    return 0;
#else
    return lua_gethookcount(DLUA_L(L));
#endif
}

void dluaL_register(dlua_State* L, const char* libname, const dluaL_Reg* l)
{
    luaL_register(DLUA_L(L), libname, DLUA_REG(l));
}

void dluaL_openlib(dlua_State* L, const char* libname, const dluaL_Reg* l, int nup)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)nup;
    luaL_register(DLUA_L(L), libname, DLUA_REG(l));
#else
    luaL_openlib(DLUA_L(L), libname, DLUA_REG(l), nup);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    luaL_typeerror(DLUA_L(L), narg, tname);
    return 0;
#else
    return luaL_typerror(DLUA_L(L), narg, tname);
#endif
}

int dluaL_argerror(dlua_State* L, int numarg, const char* extramsg)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    luaL_argerror(DLUA_L(L), numarg, extramsg);
    return 0;
#else
    return luaL_argerror(DLUA_L(L), numarg, extramsg);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    luaL_checktype(DLUA_L(L), narg, dlua_ToLuaType(t));
#else
    luaL_checktype(DLUA_L(L), narg, t);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    lua_error(DLUA_L(L));
    return 0;
#else
    return lua_error(DLUA_L(L));
#endif
}

int dluaL_checkoption(dlua_State* L, int narg, const char* def, const char* const lst[])
{
    return luaL_checkoption(DLUA_L(L), narg, def, lst);
}

int dluaL_ref(dlua_State* L, int t)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    if (dlua_isnil(L, -1))
    {
        dlua_pop(L, 1);
        return -1;
    }

    t = dlua_LuauAbsIndex(L, t);
    dlua_rawgeti(L, t, 0);
    int ref = (int)dlua_tointeger(L, -1);
    dlua_pop(L, 1);

    if (ref != 0)
    {
        dlua_rawgeti(L, t, ref);
        dlua_rawseti(L, t, 0);
    }
    else
    {
        ref = (int)dlua_objlen(L, t) + 1;
    }

    dlua_rawseti(L, t, ref);
    return ref;
#else
    return luaL_ref(DLUA_L(L), t);
#endif
}

void dluaL_unref(dlua_State* L, int t, int ref)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    if (ref < 0)
    {
        return;
    }

    t = dlua_LuauAbsIndex(L, t);
    dlua_rawgeti(L, t, 0);
    dlua_rawseti(L, t, ref);
    dlua_pushinteger(L, ref);
    dlua_rawseti(L, t, 0);
#else
    luaL_unref(DLUA_L(L), t, ref);
#endif
}

int dluaL_loadfile(dlua_State* L, const char* filename)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    lua_pushfstring(DLUA_L(L), "%s is not supported by the Luau backend", "dluaL_loadfile");
    (void)filename;
    return LUA_ERRRUN;
#else
    return luaL_loadfile(DLUA_L(L), filename);
#endif
}

int dluaL_loadbuffer(dlua_State* L, const char* buff, size_t sz, const char* name)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    size_t bytecode_size = 0;
    lua_CompileOptions options;
    memset(&options, 0, sizeof(options));
    options.optimizationLevel = 2;
    options.debugLevel        = 1;
#if defined(DM_DLUA_LUAU_CODEGEN)
    if (dlua_LuauCodeGenSupported())
    {
        options.typeInfoLevel = 1;
    }
#endif

    char* bytecode = luau_compile(buff, sz, &options, &bytecode_size);
    int result = luau_load(DLUA_L(L), name, bytecode, bytecode_size, 0);
    free(bytecode);
    if (result == 0)
    {
        dlua_LuauCodeGenCompile(DLUA_L(L), -1);
    }
    return result;
#else
    return luaL_loadbuffer(DLUA_L(L), buff, sz, name);
#endif
}

int dluaL_loadstring(dlua_State* L, const char* s)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    return dluaL_loadbuffer(L, s, strlen(s), "=string");
#else
    return luaL_loadstring(DLUA_L(L), s);
#endif
}

dlua_State* dluaL_newstate(void)
{
    lua_State* L = luaL_newstate();
#if defined(DM_DLUA_BACKEND_LUAU)
    dlua_LuauCodeGenCreate(L);
#endif
    return DLUA_DL(L);
}

const char* dluaL_gsub(dlua_State* L, const char* s, const char* p, const char* r)
{
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    (void)s;
    (void)p;
    (void)r;
    return 0;
#else
    return luaL_gsub(DLUA_L(L), s, p, r);
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    return luaL_prepbuffsize(DLUA_BUFFER(B), LUA_BUFFERSIZE);
#else
    return luaL_prepbuffer(DLUA_BUFFER(B));
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    return 0;
#else
    return luaopen_io(DLUA_L(L));
#endif
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
#if defined(DM_DLUA_BACKEND_LUAU)
    (void)L;
    return 0;
#else
    return luaopen_package(DLUA_L(L));
#endif
}
