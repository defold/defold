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
    /*# Font API documentation
     *
     * Functions, messages and properties used to manipulate font resources.
     *
     * @document
     * @name Font
     * @namespace font
     * @language Lua
     */

    //////////////////////////////////////////////////////////////////////////////

dmResource::HFactory g_ResourceFactory = 0;

struct CallbackContext
{
    dmScript::LuaCallbackInfo* m_Callback;
    int                        m_Request;
};

// TODO: Determine more about our actual use case!
// /*#
//  * Adds a ttf resource to a .fontc file
//  *
//  * @name font.add_source
//  * @param fontc [type:string|hash] The path to the .fontc resource
//  * @param ttf [type:string|hash] The path to the .ttf resource
//  * @param codepoint_min [type:number] The minimum codepoint range (inclusive)
//  * @param codepoint_max [type:number] The maximum codepoint range (inclusive)
//  *
//  * @examples
//  *
//  * ```lua
//  * local font_hash = hash("/assets/fonts/roboto.fontc")
//  * local ttf_hash = hash("/assets/fonts/Roboto/Roboto-Bold.ttf")
//  * local codepoint_min = 0x00000041 -- A
//  * local codepoint_max = 0x0000005A -- Z
//  * font.add_source(font_hash, ttf_hash, codepoint_min, codepoint_max)
//  * ```
//  */
// static int AddSource(lua_State* L)
// {
//     DM_LUA_STACK_CHECK(L, 0);

//     dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
//     dmhash_t ttf_path_hash = dmScript::CheckHashOrString(L, 2);
//     int codepoint_min = luaL_checkinteger(L, 3);
//     int codepoint_max = luaL_checkinteger(L, 4);

//     dmResource::Result r = dmGameSystem::ResFontAddGlyphSource(g_ResourceFactory, fontc_path_hash, ttf_path_hash, codepoint_min, codepoint_max);
//     if (dmResource::RESULT_OK != r)
//     {
//         return DM_LUA_ERROR("Failed to add glyph source '%s' for font '%s'", dmHashReverseSafe64(ttf_path_hash), dmHashReverseSafe64(fontc_path_hash));
//     }

//     return 0;
// }

// /*#
//  * Removes the .ttf resource from a .fontc file
//  * Removes all ranges associated with the .ttf resource
//  *
//  * @name font.add_source
//  * @param fontc [type:string|hash] The path to the .fontc resource
//  * @param ttf [type:string|hash] The path to the .ttf resource
//  */
// static int RemoveSource(lua_State* L)
// {
//     DM_LUA_STACK_CHECK(L, 0);

//     dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
//     dmhash_t ttf_path_hash = dmScript::CheckHashOrString(L, 2);

//     dmResource::Result r = dmGameSystem::ResFontRemoveGlyphSource(g_ResourceFactory, fontc_path_hash, ttf_path_hash);
//     if (dmResource::RESULT_OK != r)
//     {
//         return DM_LUA_ERROR("Failed to remove glyph source '%s' from font '%s'", dmHashReverseSafe64(ttf_path_hash), dmHashReverseSafe64(fontc_path_hash));
//     }
//     return 0;
// }

static void AddGlyphsCallback(dmJobThread::HJob job, uint64_t tag, void* _ctx, int result, const char* errmsg)
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

/*# adds more glyphs to a .fontc resource
 * Asynchronoously adds more glyphs to a .fontc resource
 *
 * @note The generated glyph bitmaps are stored in memory, for easy transition into the glyph cache texture.
 * You can call `font.remove_glyphs()` to remove them from memory.
 *
 * @note A glyph residing in the glyph cache will stay there until it's evicted.
 * It will not be removed from the glyph cache by removing the loaded glyphs from the .fontc resource.
 *
 * @name font.add_glyphs
 *
 * @param path [type:string|hash] The path to the .fontc resource
 * @param text [type:string] A string with unique unicode characters to be loaded
 * @param [callback] [type:function(self, request_id, result, errstring)] (optional) A callback function that is called after the request is finished
 *
 * `self`
 * : [type:object] The current object.
 *
 * `request_id`
 * : [type:number] The request id
 *
 * `result`
 * : [type:boolean] True if request was succesful
 *
 * `errstring`
 * : [type:string] `nil` if the request was successful
 *
 * @return request_id [type:number] Returns the asynchronous request id
 *
 * @examples
 *
 * ```lua
 * -- Add glyphs
 * local requestid = font.add_glyphs("/path/to/my.fontc", "abcABC123", function (self, request, result, errstring)
 *         -- make a note that all the glyphs are loaded
 *         -- and we're ready to present the text
 *         self.dialog_text_ready = true
 *     end)
 * ```
 *
 * ```lua
 * -- Remove glyphs
 * local requestid = font.remove_glyphs("/path/to/my.fontc", "abcABC123")
 * ```
 */

static int AddGlyphs(lua_State* L)
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
        callback = AddGlyphsCallback;
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

    if (!dmGameSystem::FontGenAddGlyphs(resource, text, false, callback, cbk_ctx))
    {
        dmResource::Release(g_ResourceFactory, resource);
        return DM_LUA_ERROR("Failed to add glyphs to font %s", dmHashReverseSafe64(fontc_path_hash));
    }

    dmResource::Release(g_ResourceFactory, resource);
    lua_pushinteger(L, request_id);
    return 1;
}

/*# removes glyphs from the font
 * Removes glyphs from the font
 *
 * @name font.remove_glyphs
 * @param path [type:string|hash] The path to the .fontc resource
 * @param text [type:string] A string with unique unicode characters to be removed
 */
static int RemoveGlyphs(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    const char* text = luaL_checkstring(L, 2);

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::Get(g_ResourceFactory, fontc_path_hash, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    if (!dmGameSystem::FontGenRemoveGlyphs(resource, text))
    {
        dmResource::Release(g_ResourceFactory, resource);
        return DM_LUA_ERROR("Failed to remove glyphs from font %s", dmHashReverseSafe64(fontc_path_hash));
    }

    dmResource::Release(g_ResourceFactory, resource);
    return 0;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    // {"add_source", AddSource},
    // {"remove_source", RemoveSource},

    {"add_glyphs", AddGlyphs},
    {"remove_glyphs", RemoveGlyphs},
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

