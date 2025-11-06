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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <dlib/log.h>
#include <dlib/image.h>
#include <extension/extension.hpp>

#include "script_buffer.h"

#include "../gamesys.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    /*# Image API documentation
     *
     * Functions for creating image objects.
     *
     * @document
     * @name Image
     * @namespace image
     * @language Lua
     */

    #define LIB_NAME "image"

    /*# RGB image type
     *
     * @name image.TYPE_RGB
     * @constant
     */

    /*# RGBA image type
     *
     * @name image.TYPE_RGBA
     * @constant
     */

    /*# luminance image type
     *
     * @name image.TYPE_LUMINANCE
     * @constant
     */

    /*# luminance image type
     *
     * @name image.TYPE_LUMINANCE_ALPHA
     * @constant
     */

    static void PushImageParameters(lua_State* L, dmImage::Image image)
    {
        lua_pushliteral(L, "width");
        lua_pushinteger(L, image.m_Width);
        lua_rawset(L, -3);

        lua_pushliteral(L, "height");
        lua_pushinteger(L, image.m_Height);
        lua_rawset(L, -3);

        lua_pushliteral(L, "type");
        switch (image.m_Type) {
            case dmImage::TYPE_RGB:
                lua_pushliteral(L, "rgb");
                break;
            case dmImage::TYPE_RGBA:
                lua_pushliteral(L, "rgba");
                break;
            case dmImage::TYPE_LUMINANCE:
                lua_pushliteral(L, "l");
                break;
            case dmImage::TYPE_LUMINANCE_ALPHA:
                lua_pushliteral(L, "la");
                break;
            default:
                assert(false);
        }
        lua_rawset(L, -3);
    }

    /*# load image from buffer
    * Load image (PNG or JPEG) from buffer.
    *
    * @name image.load
    * @param buffer [type:string] image data buffer
    * @param [options] [type:table] An optional table containing parameters for loading the image. Supported entries:
    *
    * `premultiply_alpha`
    * : [type:boolean] True if alpha should be premultiplied into the color components. Defaults to `false`.
    *
    * `flip_vertically`
    * : [type:boolean] True if the image contents should be flipped vertically. Defaults to `false`.
    *
    * @return image [type:table|nil] object or `nil` if loading fails. The object is a table with the following fields:
    *
    * - [type:number] `width`: image width
    * - [type:number] `height`: image height
    * - [type:constant] `type`: image type
    *     - `image.TYPE_RGB`
    *     - `image.TYPE_RGBA`
    *     - `image.TYPE_LUMINANCE`
    *     - `image.TYPE_LUMINANCE_ALPHA`
    * - [type:string] `buffer`: the raw image data
    *
    * @examples
    *
    * How to load an image from an URL and create a GUI texture from it:
    *
    * ```lua
    * local imgurl = "http://www.site.com/image.png"
    * http.request(imgurl, "GET", function(self, id, response)
    *         local img = image.load(response.response)
    *         local tx = gui.new_texture("image_node", img.width, img.height, img.type, img.buffer)
    *     end)
    * ```
    */
    int Image_Load(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TSTRING);
        size_t buffer_len = 0;
        const char* buffer = lua_tolstring(L, 1, &buffer_len);

        bool premult = false;
        bool flip_vertically = false;

        if (top >= 2)
        {
            // Parse as options table
            if (lua_istable(L, 2))
            {
                lua_pushvalue(L, 2);

                lua_getfield(L, -1, "premultiply_alpha");
                if (!lua_isnil(L, -1))
                    premult = dmScript::CheckBoolean(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "flip_vertically");
                if (!lua_isnil(L, -1))
                    flip_vertically = dmScript::CheckBoolean(L, -1);
                lua_pop(L, 1);

                lua_pop(L, 1);
            }
            // backwards compatability
            else
            {
                premult = dmScript::CheckBoolean(L, 2);
            }
        }

        dmImage::Image image;
        dmImage::Result r = dmImage::Load(buffer, buffer_len, premult, flip_vertically, &image);
        if (r == dmImage::RESULT_OK) {

            int bytes_per_pixel = dmImage::BytesPerPixel(image.m_Type);
            if (bytes_per_pixel == 0) {
                dmImage::Free(&image);
                luaL_error(L, "unknown image type %d", image.m_Type);
            }

            lua_newtable(L);

            PushImageParameters(L, image);

            lua_pushliteral(L, "buffer");
            lua_pushlstring(L, (const char*) image.m_Buffer, bytes_per_pixel * image.m_Width * image.m_Height);
            lua_rawset(L, -3);

            dmImage::Free(&image);
        }
        else
        {
            dmLogWarning("failed to load image (%d)", r);
            lua_pushnil(L);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# load image from a string into a buffer object
    * Load image (PNG or JPEG) from a string buffer.
    *
    * @name image.load_buffer
    * @param buffer [type:string] image data buffer
    * @param [options] [type:table] An optional table containing parameters for loading the image. Supported entries:
    *
    * `premultiply_alpha`
    * : [type:boolean] True if alpha should be premultiplied into the color components. Defaults to `false`.
    *
    * `flip_vertically`
    * : [type:boolean] True if the image contents should be flipped vertically. Defaults to `false`.
    *
    * @return image [type:table|nil] object or `nil` if loading fails. The object is a table with the following fields:
    *
    * - [type:number] `width`: image width
    * - [type:number] `height`: image height
    * - [type:constant] `type`: image type
    *     - `image.TYPE_RGB`
    *     - `image.TYPE_RGBA`
    *     - `image.TYPE_LUMINANCE`
    *     - `image.TYPE_LUMINANCE_ALPHA`
    * - [type:buffer] `buffer`: the script buffer that holds the decompressed image data. See [ref:buffer.create] how to use the buffer.
    *
    * @examples
    *
    * Load an image from an URL as a buffer and create a texture resource from it:
    *
    * ```lua
    * local imgurl = "http://www.site.com/image.png"
    * http.request(imgurl, "GET", function(self, id, response)
    *         local img = image.load_buffer(response.response, { flip_vertically = true })
    *         local tparams = {
    *             width  = img.width,
    *             height = img.height,
    *             type   = graphics.TEXTURE_TYPE_2D,
    *             format = graphics.TEXTURE_FORMAT_RGBA }
    *
    *         local my_texture_id = resource.create_texture("/my_custom_texture.texturec", tparams, img.buffer)
    *         -- Apply the texture to a model
    *         go.set("/go1#model", "texture0", my_texture_id)
    *     end)
    * ```
    *
    */
    static int Image_LoadBuffer(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TSTRING);
        size_t buffer_len = 0;
        const char* buffer = lua_tolstring(L, 1, &buffer_len);

        bool premult = false;
        bool flip_vertically = false;

        if (top >= 2)
        {
            // Parse as options table
            if (lua_istable(L, 2))
            {
                lua_pushvalue(L, 2);

                lua_getfield(L, -1, "premultiply_alpha");
                if (!lua_isnil(L, -1))
                    premult = dmScript::CheckBoolean(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "flip_vertically");
                if (!lua_isnil(L, -1))
                    flip_vertically = dmScript::CheckBoolean(L, -1);
                lua_pop(L, 1);

                lua_pop(L, 1);
            }
            // backwards compatability
            else
            {
                premult = dmScript::CheckBoolean(L, 2);
            }
        }

        dmImage::Image image;
        dmImage::Result r = dmImage::Load(buffer, buffer_len, premult, flip_vertically, &image);
        if (r == dmImage::RESULT_OK) {

            uint8_t bytes_per_pixel = dmImage::BytesPerPixel(image.m_Type);
            if (bytes_per_pixel == 0) {
                dmImage::Free(&image);
                luaL_error(L, "unknown image type %d", image.m_Type);
            }

            lua_newtable(L);

            PushImageParameters(L, image);

            uint32_t imagesize = image.m_Width * image.m_Height;
            uint32_t datasize = bytes_per_pixel * imagesize;

            lua_pushliteral(L, "buffer");

            dmBuffer::StreamDeclaration streams_decl[] = {
                { dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, bytes_per_pixel }
            };

            dmBuffer::HBuffer buffer = 0;
            dmBuffer::Create(imagesize, streams_decl, 1, &buffer);

            uint8_t* buffer_data     = 0;
            uint32_t buffer_datasize = 0;
            dmBuffer::GetBytes(buffer, (void**)&buffer_data, &buffer_datasize);
            memcpy(buffer_data, image.m_Buffer, datasize);

            dmScript::LuaHBuffer luabuf(buffer, dmScript::OWNER_LUA);
            dmScript::PushBuffer(L, luabuf);

            lua_rawset(L, -3);

            dmImage::Free(&image);
        }
        else
        {
            dmLogWarning("failed to load image (%d)", r);
            lua_pushnil(L);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }


    /*# get the header of an .astc buffer
    * get the header of an .astc buffer
    *
    * @name image.get_astc_header
    * @param buffer [type:string] .astc file data buffer
    *
    * @return table [type:table|nil] header or `nil` if buffer is not a valid .astc. The header has these fields:
    *
    * - [type:number] `width`: image width
    * - [type:number] `height`: image height
    * - [type:number] `depth`: image depth
    * - [type:number] `block_size_x`: block size x
    * - [type:number] `block_size_y`: block size y
    * - [type:number] `block_size_z`: block size z
    *
    * @examples
    *
    * How to get the block size and dimensions from a .astc file
    *
    * ```lua
    * local s = sys.load_resource("/assets/cat.astc")
    * local header = image.get_astc_header(s)
    * pprint(s)
    * ```
    */
    int Image_GetAstcHeader(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_checktype(L, 1, LUA_TSTRING);
        size_t data_len = 0;
        const char* data = lua_tolstring(L, 1, &data_len);

        uint32_t width, height, depth;
        if (!dmImage::GetAstcDimensions((void*)data, (uint32_t)data_len, &width, &height, &depth))
        {
            return luaL_error(L, "Data is not a valid .astc file");
        }

        uint32_t block_size_x, block_size_y, block_size_z;
        if (!dmImage::GetAstcBlockSize((void*)data, (uint32_t)data_len, &block_size_x, &block_size_y, &block_size_z))
        {
            return luaL_error(L, "Data is not a valid .astc file");
        }

        lua_newtable(L);

            lua_pushinteger(L, width);
            lua_setfield(L, -2, "width");
            lua_pushinteger(L, height);
            lua_setfield(L, -2, "height");
            lua_pushinteger(L, depth);
            lua_setfield(L, -2, "depth");

            lua_pushinteger(L, block_size_x);
            lua_setfield(L, -2, "block_size_x");
            lua_pushinteger(L, block_size_y);
            lua_setfield(L, -2, "block_size_y");
            lua_pushinteger(L, block_size_z);
            lua_setfield(L, -2, "block_size_z");

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg ScriptImage_methods[] =
    {
        {"load",            Image_Load},
        {"load_buffer",     Image_LoadBuffer},
        {"get_astc_header", Image_GetAstcHeader},
        {0, 0}
    };

    static void ScriptImageRegister(lua_State* L)
    {
        int top = lua_gettop(L);

        luaL_register(L, LIB_NAME, ScriptImage_methods);

#define SETCONSTANT(name, val) \
        lua_pushliteral(L, val); \
        lua_setfield(L, -2, #name);\

        SETCONSTANT(TYPE_RGB, "rgb")
        SETCONSTANT(TYPE_RGBA, "rgba")
        SETCONSTANT(TYPE_LUMINANCE, "l")
        SETCONSTANT(TYPE_LUMINANCE_ALPHA, "la")
#undef SETCONSTANT

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    static dmExtension::Result ScriptImageInitialize(dmExtension::Params* params)
    {
        lua_State* L = params->m_L;
        ScriptImageRegister(L);
        return dmExtension::RESULT_OK;
    }


    static dmExtension::Result ScriptImageFinalize(dmExtension::Params* params)
    {
        return dmExtension::RESULT_OK;
    }


    DM_DECLARE_EXTENSION(ScriptImageExt, "ScriptImage", 0, 0, ScriptImageInitialize, 0, 0, ScriptImageFinalize)
}
