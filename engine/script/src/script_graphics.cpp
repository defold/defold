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
     * @variable
     */

    /*#
     * @name graphics.BUFFER_TYPE_DEPTH_BIT
     * @variable
     */

    /*#
     * @name graphics.BUFFER_TYPE_STENCIL_BIT
     * @variable
     */

    /*#
     * May be nil if multitarget rendering isn't supported
     * @name graphics.BUFFER_TYPE_COLOR1_BIT
     * @variable
     */

    /*#
     * May be nil if multitarget rendering isn't supported
     * @name graphics.BUFFER_TYPE_COLOR2_BIT
     * @variable
     */

    /*#
     * May be nil if multitarget rendering isn't supported
     * @name graphics.BUFFER_TYPE_COLOR3_BIT
     * @variable
     */

    /*#
     * @name graphics.STATE_DEPTH_TEST
     * @variable
     */

    /*#
     * @name graphics.STATE_SCISSOR_TEST
     * @variable
     */

    /*#
     * @name graphics.STATE_STENCIL_TEST
     * @variable
     */

    /*#
     * @name graphics.STATE_ALPHA_TEST
     * @variable
     */

    /*#
     * @name graphics.STATE_BLEND
     * @variable
     */

    /*#
     * @name graphics.STATE_CULL_FACE
     * @variable
     */

    /*#
     * @name graphics.STATE_POLYGON_OFFSET_FILL
     * @variable
     */

    /*#
     * @name graphics.STATE_ALPHA_TEST_SUPPORTED
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ZERO
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_SRC_COLOR
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_SRC_COLOR
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_DST_COLOR
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_DST_COLOR
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_SRC_ALPHA
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_DST_ALPHA
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_DST_ALPHA
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_SRC_ALPHA_SATURATE
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_CONSTANT_COLOR
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_CONSTANT_ALPHA
     * @variable
     */

    /*#
     * @name graphics.BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_NEVER
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_LESS
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_LEQUAL
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_GREATER
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_GEQUAL
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_EQUAL
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_NOTEQUAL
     * @variable
     */

    /*#
     * @name graphics.COMPARE_FUNC_ALWAYS
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_KEEP
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_ZERO
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_REPLACE
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_INCR
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_INCR_WRAP
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_DECR
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_DECR_WRAP
     * @variable
     */

    /*#
     * @name graphics.STENCIL_OP_INVERT
     * @variable
     */

    /*#
     * @name graphics.FACE_TYPE_FRONT
     * @variable
     */

    /*#
     * @name graphics.FACE_TYPE_BACK
     * @variable
     */

    /*#
     * @name graphics.FACE_TYPE_FRONT_AND_BACK
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_TYPE_2D
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_TYPE_2D_ARRAY
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_TYPE_CUBE_MAP
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_TYPE_IMAGE_2D
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_TYPE_3D
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_TYPE_IMAGE_3D
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_DEFAULT
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_NEAREST
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_LINEAR
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_SAMPLE
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_MEMORYLESS
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_STORAGE
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_INPUT
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_USAGE_FLAG_COLOR
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_WRAP_CLAMP_TO_BORDER
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_WRAP_CLAMP_TO_EDGE
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_WRAP_MIRRORED_REPEAT
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_WRAP_REPEAT
     * @variable
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_DEFAULT
     * @variable
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_WEBP
     * @variable
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_WEBP_LOSSY
     * @variable
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_BASIS_UASTC
     * @variable
     */

    /*#
     * @name graphics.COMPRESSION_TYPE_BASIS_ETC1S
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FORMAT_DEPTH
     * @variable
     */

    /*#
     * @name graphics.TEXTURE_FORMAT_STENCIL
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_LUMINANCE
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_LUMINANCE_ALPHA
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_16BPP
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_16BPP
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_ETC1
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R_ETC2
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG_ETC2
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_ETC2
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_ASTC_4x4
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB_BC1
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_BC3
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R_BC4
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG_BC5
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA_BC7
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB16F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGB32F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA16F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA32F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R16F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG16F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R32F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RG32F
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_RGBA32UI
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_BGRA8U
     * @variable
     */

    /*#
     * May be nil if the graphics driver doesn't support it
     * @name graphics.TEXTURE_FORMAT_R32UI
     * @variable
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
        SET_TEXTUREFORMAT_IF_SUPPORTED(TEXTURE_FORMAT_RGBA_ASTC_4x4);
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
