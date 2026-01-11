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

#include <stdint.h>
#include <string.h>

#include "script.h"

#include <graphics/graphics.h>
#include <graphics/graphics_ddf.h>

extern "C"
{
    #include <lua/lauxlib.h>
    #include <lua/lualib.h>
}

namespace dmScript
{
    #define SCRIPT_LIB_NAME "graphics"

    /*# Graphics API documentation
     *
     * Graphics functions and constants.
     *
     * @document
     * @name Graphics
     * @namespace graphics
     * @language Lua
     */

    /*#
     * @name graphics.BUFFER_TYPE_COLOR0_BIT
     * @constant
     */

    /*#
     * @name graphics.BUFFER_TYPE_DEPTH_BIT
     * @constant
     */

    /*#
     * @name graphics.BUFFER_TYPE_STENCIL_BIT
     * @constant
     */

    /*#
     * May be nil if multitarget rendering isn't supported
     * @name graphics.BUFFER_TYPE_COLOR1_BIT
     * @constant
     */

    /*#
     * May be nil if multitarget rendering isn't supported
     * @name graphics.BUFFER_TYPE_COLOR2_BIT
     * @constant
     */

    /*#
     * May be nil if multitarget rendering isn't supported
     * @name graphics.BUFFER_TYPE_COLOR3_BIT
     * @constant
     */

    /*#
     * @name graphics.STATE_DEPTH_TEST
     * @constant
     */

    /*#
     * @name graphics.STATE_SCISSOR_TEST
     * @constant
     */

    /*#
     * @name graphics.STATE_STENCIL_TEST
     * @constant
     */

    /*#
     * @name graphics.STATE_ALPHA_TEST
     * @constant
     */

    /*#
     * @name graphics.STATE_BLEND
     * @constant
     */

    /*#
     * @name graphics.STATE_CULL_FACE
     * @constant
     */

    /*#
     * @name graphics.STATE_POLYGON_OFFSET_FILL
     * @constant
     */

    /*#
     * @name graphics.STATE_ALPHA_TEST_SUPPORTED
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ZERO
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_SRC_COLOR
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_SRC_COLOR
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_DST_COLOR
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_DST_COLOR
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_SRC_ALPHA
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_DST_ALPHA
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_DST_ALPHA
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_SRC_ALPHA_SATURATE
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_CONSTANT_COLOR
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_CONSTANT_ALPHA
     * @constant
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_NEVER
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_LESS
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_LEQUAL
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_GREATER
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_GEQUAL
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_EQUAL
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_NOTEQUAL
     * @constant
     */

    /*#
     * @name graphics.COMPARE_FUNC_ALWAYS
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_KEEP
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_ZERO
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_REPLACE
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_INCR
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_INCR_WRAP
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_DECR
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_DECR_WRAP
     * @constant
     */

    /*#
     * @name graphics.STENCIL_OP_INVERT
     * @constant
     */

    /*#
     * @name graphics.FACE_TYPE_FRONT
     * @constant
     */

    /*#
     * @name graphics.FACE_TYPE_BACK
     * @constant
     */

    /*#
     * @name graphics.FACE_TYPE_FRONT_AND_BACK
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_TYPE_2D
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_TYPE_2D_ARRAY
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_TYPE_CUBE_MAP
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_TYPE_IMAGE_2D
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_TYPE_3D
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_TYPE_IMAGE_3D
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_DEFAULT
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_NEAREST
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_LINEAR
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_SAMPLE
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_MEMORYLESS
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_STORAGE
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_INPUT
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_COLOR
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_WRAP_CLAMP_TO_BORDER
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_WRAP_CLAMP_TO_EDGE
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_WRAP_MIRRORED_REPEAT
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_WRAP_REPEAT
     * @constant
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_DEFAULT
     * @constant
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_WEBP
     * @constant
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_WEBP_LOSSY
     * @constant
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_BASIS_UASTC
     * @constant
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_BASIS_ETC1S
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FORMAT_DEPTH
     * @constant
     */

    /*#
     * @name graphics.TEXTURE_FORMAT_STENCIL
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_LUMINANCE
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_LUMINANCE_ALPHA
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_16BPP
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_16BPP
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_ETC1
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R_ETC2
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG_ETC2
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_ETC2
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_ASTC_4X4
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_BC1
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_BC3
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R_BC4
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG_BC5
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_BC7
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB16F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB32F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA16F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA32F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R16F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG16F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R32F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG32F
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA32UI
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_BGRA8U
     * @constant
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R32UI
     * @constant
     */

    static const luaL_reg ScriptGraphics_methods[] =
    {
        {0, 0}
    };

    void InitializeGraphics(lua_State* L, dmGraphics::HContext graphics_context)
    {
        DM_LUA_STACK_CHECK(L, 0);

        luaL_register(L, SCRIPT_LIB_NAME, ScriptGraphics_methods);

    #define SET_GRAPHICS_ENUM(name) \
        lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
        lua_setfield(L, -2, #name);

    #define SET_GRAPHICS_ENUM_NAMED(enum_name, name) \
        lua_pushnumber(L, (lua_Number) dmGraphics:: enum_name); \
        lua_setfield(L, -2, #name);

    #define SET_TEXTUREFORMAT_IF_SUPPORTED(name) \
        if (graphics_context && dmGraphics::IsTextureFormatSupported(graphics_context, dmGraphics::name)) \
        { \
            lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
            lua_setfield(L, -2, #name); \
        }

        // Buffer
        SET_GRAPHICS_ENUM(BUFFER_TYPE_COLOR0_BIT);
        SET_GRAPHICS_ENUM(BUFFER_TYPE_DEPTH_BIT);
        SET_GRAPHICS_ENUM(BUFFER_TYPE_STENCIL_BIT);

        if (graphics_context && dmGraphics::IsContextFeatureSupported(graphics_context, dmGraphics::CONTEXT_FEATURE_MULTI_TARGET_RENDERING))
        {
            SET_GRAPHICS_ENUM(BUFFER_TYPE_COLOR1_BIT);
            SET_GRAPHICS_ENUM(BUFFER_TYPE_COLOR2_BIT);
            SET_GRAPHICS_ENUM(BUFFER_TYPE_COLOR3_BIT);
        }

        // State
        SET_GRAPHICS_ENUM(STATE_DEPTH_TEST);
        SET_GRAPHICS_ENUM(STATE_SCISSOR_TEST);
        SET_GRAPHICS_ENUM(STATE_STENCIL_TEST);
        SET_GRAPHICS_ENUM(STATE_ALPHA_TEST);
        SET_GRAPHICS_ENUM(STATE_BLEND);
        SET_GRAPHICS_ENUM(STATE_CULL_FACE);
        SET_GRAPHICS_ENUM(STATE_POLYGON_OFFSET_FILL);
        SET_GRAPHICS_ENUM(STATE_ALPHA_TEST_SUPPORTED);

        // BlendFactor
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ZERO);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_SRC_COLOR);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE_MINUS_SRC_COLOR);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_DST_COLOR);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE_MINUS_DST_COLOR);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_SRC_ALPHA);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_DST_ALPHA);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE_MINUS_DST_ALPHA);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_SRC_ALPHA_SATURATE);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_CONSTANT_COLOR);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_CONSTANT_ALPHA);
        SET_GRAPHICS_ENUM(BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA);

        // CompareFunc
        SET_GRAPHICS_ENUM(COMPARE_FUNC_NEVER);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_LESS);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_LEQUAL);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_GREATER);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_GEQUAL);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_EQUAL);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_NOTEQUAL);
        SET_GRAPHICS_ENUM(COMPARE_FUNC_ALWAYS);

        // StencilOP
        SET_GRAPHICS_ENUM(STENCIL_OP_KEEP);
        SET_GRAPHICS_ENUM(STENCIL_OP_ZERO);
        SET_GRAPHICS_ENUM(STENCIL_OP_REPLACE);
        SET_GRAPHICS_ENUM(STENCIL_OP_INCR);
        SET_GRAPHICS_ENUM(STENCIL_OP_INCR_WRAP);
        SET_GRAPHICS_ENUM(STENCIL_OP_DECR);
        SET_GRAPHICS_ENUM(STENCIL_OP_DECR_WRAP);
        SET_GRAPHICS_ENUM(STENCIL_OP_INVERT);

        // FaceType
        SET_GRAPHICS_ENUM(FACE_TYPE_FRONT);
        SET_GRAPHICS_ENUM(FACE_TYPE_BACK);
        SET_GRAPHICS_ENUM(FACE_TYPE_FRONT_AND_BACK);

        // TextureType
        SET_GRAPHICS_ENUM(TEXTURE_TYPE_2D);
        SET_GRAPHICS_ENUM(TEXTURE_TYPE_2D_ARRAY);
        SET_GRAPHICS_ENUM(TEXTURE_TYPE_IMAGE_2D);
        SET_GRAPHICS_ENUM(TEXTURE_TYPE_CUBE_MAP);

        if (graphics_context && dmGraphics::IsContextFeatureSupported(graphics_context, dmGraphics::CONTEXT_FEATURE_3D_TEXTURES))
        {
            SET_GRAPHICS_ENUM(TEXTURE_TYPE_3D);
            SET_GRAPHICS_ENUM(TEXTURE_TYPE_IMAGE_3D);
        }

        // TextureFilter
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_DEFAULT);
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_NEAREST);
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_LINEAR);
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST);
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR);
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST);
        SET_GRAPHICS_ENUM(TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR);

        // TextureUsageFlag
        SET_GRAPHICS_ENUM(TEXTURE_USAGE_FLAG_SAMPLE);
        SET_GRAPHICS_ENUM(TEXTURE_USAGE_FLAG_MEMORYLESS);
        SET_GRAPHICS_ENUM(TEXTURE_USAGE_FLAG_STORAGE);
        SET_GRAPHICS_ENUM(TEXTURE_USAGE_FLAG_INPUT);
        SET_GRAPHICS_ENUM(TEXTURE_USAGE_FLAG_COLOR);

        // TextureWrap
        SET_GRAPHICS_ENUM(TEXTURE_WRAP_CLAMP_TO_BORDER);
        SET_GRAPHICS_ENUM(TEXTURE_WRAP_CLAMP_TO_EDGE);
        SET_GRAPHICS_ENUM(TEXTURE_WRAP_MIRRORED_REPEAT);
        SET_GRAPHICS_ENUM(TEXTURE_WRAP_REPEAT);

        // Compression type
        SET_GRAPHICS_ENUM_NAMED(TextureImage::COMPRESSION_TYPE_DEFAULT,     COMPRESSION_TYPE_DEFAULT);
        SET_GRAPHICS_ENUM_NAMED(TextureImage::COMPRESSION_TYPE_WEBP,        COMPRESSION_TYPE_WEBP);
        SET_GRAPHICS_ENUM_NAMED(TextureImage::COMPRESSION_TYPE_WEBP_LOSSY,  COMPRESSION_TYPE_WEBP_LOSSY);
        SET_GRAPHICS_ENUM_NAMED(TextureImage::COMPRESSION_TYPE_BASIS_UASTC, COMPRESSION_TYPE_BASIS_UASTC);
        SET_GRAPHICS_ENUM_NAMED(TextureImage::COMPRESSION_TYPE_BASIS_ETC1S, COMPRESSION_TYPE_BASIS_ETC1S);

        // TextureFormat custom
        SET_GRAPHICS_ENUM(TEXTURE_FORMAT_DEPTH);
        SET_GRAPHICS_ENUM(TEXTURE_FORMAT_STENCIL);

        // TextureFormat
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_LUMINANCE);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_LUMINANCE_ALPHA);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_16BPP);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_16BPP);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_ETC1);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R_ETC2);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG_ETC2);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_ETC2);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_ASTC_4X4);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB_BC1);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_BC3);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R_BC4);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG_BC5);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_BC7);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB16F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGB32F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA16F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA32F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R16F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG16F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R32F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RG32F);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA32UI);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_BGRA8U);
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_R32UI);

    #undef SET_GRAPHICS_ENUM_NAMED
    #undef SET_GRAPHICS_ENUM
    #undef SET_TEXTUREFORMAT_IF_SUPPORTED

        lua_pop(L, 1);
    }

    #undef SCRIPT_LIB_NAME
}
