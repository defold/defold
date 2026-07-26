// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0
//
// Lua `host` module for embed hosts: host.poll() / host.send(type, payload).

#include "defold_embed_bus.h"

#include <stdlib.h>
#include <string.h>

#include <dmsdk/extension/extension.h>
#include <dmsdk/script/script.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lua.h>
}

namespace
{
static int Host_Poll(lua_State* L)
{
    char* json = 0;
    if (!DefoldEmbedBus_PollInbound(&json) || !json)
    {
        lua_pushnil(L);
        return 1;
    }

    size_t len = strlen(json);
    // JsonToLua throws a Lua error on parse failure — acceptable for host→engine bus.
    dmScript::JsonToLua(L, json, len);
    free(json);
    return 1;
}

static int Host_Send(lua_State* L)
{
    const char* type = luaL_checkstring(L, 1);
    if (lua_gettop(L) >= 2 && !lua_istable(L, 2) && !lua_isnil(L, 2))
        return luaL_error(L, "host.send(type, payload) expects a table payload");

    // Build { v=1, type=..., payload=... } on the stack then encode.
    lua_newtable(L);
    lua_pushinteger(L, 1);
    lua_setfield(L, -2, "v");
    lua_pushstring(L, type);
    lua_setfield(L, -2, "type");
    if (lua_istable(L, 2))
    {
        lua_pushvalue(L, 2);
        lua_setfield(L, -2, "payload");
    }
    else
    {
        lua_newtable(L);
        lua_setfield(L, -2, "payload");
    }

    char* json = 0;
    size_t json_len = 0;
    int table_index = lua_gettop(L);
    // options_index 0: no options table (out of range / ignored by cjson encode).
    if (dmScript::LuaToJson(L, table_index, 0, &json, &json_len) < 0 || !json)
    {
        lua_pop(L, 1);
        return luaL_error(L, "host.send: failed to encode JSON");
    }
    lua_pop(L, 1);
    DefoldEmbedBus_EnqueueOutbound(json);
    free(json);
    return 0;
}

static const luaL_reg Host_methods[] =
{
    { "poll", Host_Poll },
    { "send", Host_Send },
    { 0, 0 }
};

static dmExtension::Result HostInitialize(dmExtension::Params* params)
{
    lua_State* L = params->m_L;
    int top = lua_gettop(L);
    luaL_register(L, "host", Host_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
    return dmExtension::RESULT_OK;
}

static dmExtension::Result HostFinalize(dmExtension::Params* params)
{
    (void)params;
    return dmExtension::RESULT_OK;
}
} // namespace

DM_DECLARE_EXTENSION(DefoldEmbedHostExt, "DefoldEmbedHost", 0, 0, HostInitialize, 0, 0, HostFinalize)
