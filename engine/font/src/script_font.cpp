// Copyright 2020-2025 The Defold Foundation
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

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/script/script.h>

#include "fontgen.h"

#define MODULE_NAME "font"


static int LoadFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 2);

    const char* fontc_path = luaL_checkstring(L, 1); // dmScript::CheckHash(L, 1);
    const char* ttf_path = luaL_checkstring(L, 2); //dmScript::CheckHash(L, 2);

    if (!dmFontGen::LoadFont(fontc_path, ttf_path))
    {
        lua_pushnil(L); // No font
        lua_pushfstring(L, "Failed to load one of fonts: %s / %s", fontc_path, ttf_path);
    }
    else
    {
        dmScript::PushHash(L, dmHashString64(fontc_path));
        lua_pushnil(L); // no error
    }
    return 2;
}

static int UnloadFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    if (!dmFontGen::UnloadFont(fontc_path_hash))
        return luaL_error(L, "Failed to unload font %s", dmHashReverseSafe64(fontc_path_hash));

    return 0;
}

struct CallbackContext
{
    dmScript::LuaCallbackInfo* m_Callback;
    int                        m_Request;
};

static void AddGlyphsCallback(void* _ctx, int result, const char* errmsg)
{
    CallbackContext* ctx = (CallbackContext*)_ctx;
    dmScript::LuaCallbackInfo* cbk = ctx->m_Callback;

    lua_State* L = dmScript::GetCallbackLuaContext(cbk);
    DM_LUA_STACK_CHECK(L, 0);

    if (dmScript::SetupCallback(cbk))
    {
        int nargs = 3;
        lua_pushinteger(L, (int)ctx->m_Request);
        lua_pushboolean(L, result != 0);
        if (0 != errmsg)
            lua_pushstring(L, errmsg);
        else
            lua_pushnil(L);

        dmScript::PCall(L, 1 + nargs, 0); // self + # user arguments

        dmScript::TeardownCallback(cbk);
    }
    dmScript::DestroyCallback(cbk); // only do this if you're not using the callback again
    delete ctx;
}

static int AddGlyphs(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    int top = lua_gettop(L);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    const char* text = luaL_checkstring(L, 2);

    dmScript::LuaCallbackInfo* luacbk = 0;
    if (top > 2 && !lua_isnil(L, 3)) {
        luacbk = dmScript::CreateCallback(L, 3);
    }

    static int requests = 1;
    int request_id = requests++;

    dmFontGen::FGlyphCallback callback = 0;
    CallbackContext* cbk_ctx = 0;
    if (luacbk)
    {
        callback = AddGlyphsCallback;
        cbk_ctx = new CallbackContext;
        cbk_ctx->m_Callback  = luacbk;
        cbk_ctx->m_Request = request_id;
    }

    if (!dmFontGen::AddGlyphs(fontc_path_hash, text, callback, cbk_ctx))
    {
        return luaL_error(L, "Failed to add glyphs to font %s", dmHashReverseSafe64(fontc_path_hash));
    }

    lua_pushinteger(L, request_id);
    return 1;
}

static int RemoveGlyphs(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    const char* text = luaL_checkstring(L, 2);

    if (!dmFontGen::RemoveGlyphs(fontc_path_hash, text))
        return luaL_error(L, "Failed to remove glyphs from font %s", dmHashReverseSafe64(fontc_path_hash));

    return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"load_font", LoadFont},
    {"unload_font", UnloadFont},
    {"add_glyphs", AddGlyphs},
    {"remove_glyphs", RemoveGlyphs},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeFontGen(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeFontGen(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s extension", MODULE_NAME);

    dmFontGen::Initialize(params);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeFontGen(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeFontGen(dmExtension::Params* params)
{
    dmFontGen::Finalize(params);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(ScriptFontExt, MODULE_NAME, AppInitializeFontGen, AppFinalizeFontGen, InitializeFontGen, 0, 0, FinalizeFontGen)
