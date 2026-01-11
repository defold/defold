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

const static dmhash_t EXT_HASH_FONTC = dmHashString64("fontc");

dmResource::HFactory g_ResourceFactory = 0;

struct CallbackContext
{
    dmScript::LuaCallbackInfo* m_Callback;
    int                        m_Request;
};


/*#
 * associates a ttf resource to a .fontc file.
 * @note The ttf font is loaded via the resource system. There are a few ways it can be accessed:
 *     - It was already loaded in the resource system
 *     - It is bundled via our game data
 *     - It is accessible via a live update mount
 *
 * @note The reference count will increase for the .ttf font
 *
 *
 * @name font.add_font
 * @param fontc [type:string|hash] The path to the .fontc resource
 * @param ttf [type:string|hash] The path to the .ttf resource
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
    // TODO: If it's a string, pass a string to the function to allow for explicit loading of the resource
    const char* ttf_path = 0;
    dmhash_t ttf_path_hash = 0;
    if (lua_isstring(L, 2))
    {
        ttf_path = luaL_checkstring(L, 2);
    }
    else
    {
        ttf_path_hash = dmScript::CheckHash(L, 2);
    }

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::GetWithExt(g_ResourceFactory, fontc_path_hash, EXT_HASH_FONTC, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    if (ttf_path)
        r = dmGameSystem::ResFontAddFontByPath(g_ResourceFactory, resource, ttf_path);
    else
        r = dmGameSystem::ResFontAddFontByPathHash(g_ResourceFactory, resource, ttf_path_hash);
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
 *
 * @examples
 *
 * ```lua
 * local font_hash = hash("/assets/fonts/roboto.fontc")
 * local ttf_hash = hash("/assets/fonts/Roboto/Roboto-Bold.ttf")
 * font.remove_font(font_hash, ttf_hash)
 * ```
 */
static int RemoveFont(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);
    dmhash_t ttf_path_hash = dmScript::CheckHashOrString(L, 2);

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::GetWithExt(g_ResourceFactory, fontc_path_hash, EXT_HASH_FONTC, (void**)&resource);
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
 * local font_hash = hash("/assets/fonts/roboto.fontc")
 * font.prewarm_text(font_hash, "Some text", function (self, request_id, result, errstring)
 *         -- cache is warm, show the text!
 *     end)
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

    dmGameSystem::FPrewarmTextCallback callback = 0;
    CallbackContext* cbk_ctx = 0;
    if (luacbk)
    {
        callback = PrewarmTextCallback;
        cbk_ctx = new CallbackContext;
        cbk_ctx->m_Callback  = luacbk;
        cbk_ctx->m_Request = request_id;
    }

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::GetWithExt(g_ResourceFactory, fontc_path_hash, EXT_HASH_FONTC, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    r = dmGameSystem::ResFontPrewarmText(resource, text, callback, cbk_ctx);
    if (dmResource::RESULT_OK != r)
    {
        dmResource::Release(g_ResourceFactory, resource);
        return DM_LUA_ERROR("Failed to add glyphs to font %s", dmHashReverseSafe64(fontc_path_hash));
    }

    dmResource::Release(g_ResourceFactory, resource);
    lua_pushinteger(L, request_id);
    return 1;
}

/*#
 * Gets information about a font, such as the associated font files
 *
 * @name font.get_info
 * @param fontc [type:string|hash] The path to the .fontc resource
 * @return info [type:table] the information table contains these fields:
 *
 * `path`
 * : [type:hash] The path hash of the current file.
 *
 * `fonts`
 * : [type:table] An array of associated font (e.g. .ttf) files. Each item is a table that contains:
 *
 *      `path`
 *      : [type:string] The path of the font file
 *
 *      `path_hash`
 *      : [type:hash] The path of the font file
 *
 */
static int GetFontInfo(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    dmhash_t fontc_path_hash = dmScript::CheckHashOrString(L, 1);

    dmGameSystem::FontResource* resource;
    dmResource::Result r = dmResource::GetWithExt(g_ResourceFactory, fontc_path_hash, EXT_HASH_FONTC, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        return DM_LUA_ERROR("Failed to get font %s: %d", dmHashReverseSafe64(fontc_path_hash), r);
    }

    lua_createtable(L,0,0);

    {
        dmScript::PushHash(L, fontc_path_hash);
        lua_setfield(L, -2, "path");

        HFontCollection fontcollection = ResFontGetFontCollection(resource);
        uint32_t num_fonts = FontCollectionGetFontCount(fontcollection);

        lua_newtable(L);

        {
            for (uint32_t i = 0; i < num_fonts; ++i)
            {
                HFont font = FontCollectionGetFont(fontcollection, i);
                dmhash_t font_path_hash = ResFontGetPathHashFromFont(resource, font);

                lua_newtable(L);

                lua_pushstring(L, FontGetPath(font));
                lua_setfield(L, -2, "path");

                dmScript::PushHash(L, font_path_hash);
                lua_setfield(L, -2, "path_hash");

                lua_rawseti(L, -2, i + 1);
            }
        }

        lua_setfield(L, -2, "fonts");
    }

    dmResource::Release(g_ResourceFactory, resource);

    return 1;
}




// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"add_font", AddFont},
    {"remove_font", RemoveFont},
    {"prewarm_text", PrewarmText},
    {"get_info", GetFontInfo},
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

DM_DECLARE_EXTENSION(ScriptFont, "ScriptFont", 0, 0, ScriptFontInitialize, 0, 0, ScriptFontFinalize)

} // namespace

