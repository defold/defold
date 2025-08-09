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

#include <stdio.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/utf8.h>
#include <dmsdk/script/script.h>

#include <dmsdk/gamesys/resources/res_font.h>
#include <dmsdk/gamesys/resources/res_ttf.h>

#include "gamesys/fontgen/fontgen.h"

namespace dmGameSystem
{
    //////////////////////////////////////////////////////////////////////////////

dmResource::HFactory g_ResourceFactory = 0;

struct CallbackContext
{
    dmScript::LuaCallbackInfo* m_Callback;
    int                        m_Request;
};


/*#
 * associates a ttf resource to a .fontc file.
 * @note The reference count will increase for the .ttf font
 *
 * @name font.add_font
 * @param fontc [type:string|hash] The path to the .fontc resource
 * @param ttf [type:string|hash] The path to the .ttf resource
 * @param codepoint_min [type:number] The minimum codepoint range (inclusive)
 * @param codepoint_max [type:number] The maximum codepoint range (inclusive)
 *
 * @examples
 *
 * ```lua
 * local font_hash = hash("/assets/fonts/roboto.fontc")
 * local ttf_hash = hash("/assets/fonts/Roboto/Roboto-Bold.ttf")
 * font.add_font(font_hash, ttf_hash)
 * ```
 */
static int AddFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    dmhash_t ttf_path_hash = dmScript::CheckHashOrString(L, 2);

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::Get(g_ResourceFactory, fontc_path_hash, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    r = dmGameSystem::ResFontAddFont(g_ResourceFactory, resource, ttf_path_hash);
    dmResource::Release(g_ResourceFactory, resource);

    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to add font '%s' to font collection '%s'", dmHashReverseSafe64(ttf_path_hash), dmHashReverseSafe64(fontc_path_hash));
    }

    return 0;
}

/*#
 * associates a ttf resource to a .fontc file
 * @note The reference count will decrease for the .ttf font
 *
 * @name font.remove_font
 * @param fontc [type:string|hash] The path to the .fontc resource
 * @param ttf [type:string|hash] The path to the .ttf resource
 * @param codepoint_min [type:number] The minimum codepoint range (inclusive)
 * @param codepoint_max [type:number] The maximum codepoint range (inclusive)
 *
 * @examples
 *
 * ```lua
 * local font_hash = hash("/assets/fonts/roboto.fontc")
 * local ttf_hash = hash("/assets/fonts/Roboto/Roboto-Bold.ttf")
 * font.add_font(font_hash, ttf_hash)
 * ```
 */
static int RemoveFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    dmhash_t ttf_path_hash = dmScript::CheckHashOrString(L, 2);

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::Get(g_ResourceFactory, fontc_path_hash, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    r = dmGameSystem::ResFontRemoveFont(g_ResourceFactory, resource, ttf_path_hash);
    dmResource::Release(g_ResourceFactory, resource);

    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to add font '%s' to font collection '%s'", dmHashReverseSafe64(ttf_path_hash), dmHashReverseSafe64(fontc_path_hash));
    }

    return 0;
}

static void PrewarmTextCallback(void* _ctx, int result, const char* errmsg)
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


/*#
 * prepopulates the font glyph cache with rasterised glyphs
 *
 * @name font.prewarm_text
 * @param fontc [type:string|hash] The path to the .fontc resource
 * @param text [type:string] The text to layout
 *
 * @examples
 *
 * ```lua
 * local font_hash = hash("/assets/fonts/roboto.fontc")
 * font.prewarm_text(font_hash, "Some text")
 * ```
 */
static int PrewarmText(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    int top = lua_gettop(L);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    const char* text = luaL_checkstring(L, 2);

    dmScript::LuaCallbackInfo* luacbk = 0;
    if (top > 2 && lua_isfunction(L, 3))
    {
        luacbk = dmScript::CreateCallback(L, 3);
    }

    static int requests = 1;
    int request_id = requests++;

    dmGameSystem::FGlyphCallback callback = 0;
    CallbackContext* cbk_ctx = 0;
    if (luacbk)
    {
        callback = PrewarmTextCallback;
        cbk_ctx = new CallbackContext;
        cbk_ctx->m_Callback  = luacbk;
        cbk_ctx->m_Request = request_id;
    }

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::Get(g_ResourceFactory, fontc_path_hash, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    r = dmGameSystem::ResFontPrewarmText(resource, text, false, callback, cbk_ctx);
    if (dmResource::RESULT_OK != r)
    {
        dmResource::Release(g_ResourceFactory, resource);
        return DM_LUA_ERROR("Failed to add glyphs to font %s", dmHashReverseSafe64(fontc_path_hash));
    }

    dmResource::Release(g_ResourceFactory, resource);
    lua_pushinteger(L, request_id);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"add_font", AddFont},
    {"remove_font", RemoveFont},
    {"prewarm_text", PrewarmText},
    {0, 0}
};

static dmExtension::Result ScriptFontInitialize(dmExtension::Params* params)
{
    g_ResourceFactory = params->m_ResourceFactory;

    lua_State* L = params->m_L;
    luaL_register(L, "font", Module_methods);
    lua_pop(L, 1); // pop the lua module

    return dmGameSystem::FontGenInitialize(params);
}

static dmExtension::Result ScriptFontFinalize(dmExtension::Params* params)
{
    g_ResourceFactory = 0;
    return dmGameSystem::FontGenFinalize(params);
}

static dmExtension::Result ScriptFontUpdate(dmExtension::Params* params)
{
    return dmGameSystem::FontGenUpdate(params);
}

DM_DECLARE_EXTENSION(ScriptFont, "ScriptFont", 0, 0, ScriptFontInitialize, ScriptFontUpdate, 0, ScriptFontFinalize)

} // namespace

