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

    struct GraphicsModule
    {
        dmGraphics::HContext m_GraphicsContext;
    } g_GraphicsModule;

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
     * @name graphics.BLEND_EQUATION_ADD
     * @constant
     */

    /*#
     * @name graphics.BLEND_EQUATION_SUBTRACT
     * @constant
     */

    /*#
     * @name graphics.BLEND_EQUATION_REVERSE_SUBTRACT
     * @constant
     */

    /*#
     * @name graphics.BLEND_EQUATION_MIN
     * @constant
     */

    /*#
     * @name graphics.BLEND_EQUATION_MAX
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

    /*#
     * Context feature flag indicating support for rendering to multiple color targets simultaneously.
     * @name graphics.CONTEXT_FEATURE_MULTI_TARGET_RENDERING
     * @constant
     */

    /*#
     * Context feature flag indicating support for texture arrays.
     * @name graphics.CONTEXT_FEATURE_TEXTURE_ARRAY
     * @constant
     */

    /*#
     * Context feature flag indicating support for compute shaders.
     * @name graphics.CONTEXT_FEATURE_COMPUTE_SHADER
     * @constant
     */

    /*#
     * Context feature flag indicating support for storage buffers.
     * @name graphics.CONTEXT_FEATURE_STORAGE_BUFFER
     * @constant
     */

    /*#
     * Context feature flag indicating support for vertical sync (vsync).
     * @name graphics.CONTEXT_FEATURE_VSYNC
     * @constant
     */

    /*#
     * Context feature flag indicating support for hardware instancing.
     * @name graphics.CONTEXT_FEATURE_INSTANCING
     * @constant
     */

    /*#
     * Context feature flag indicating support for 3D (volume) textures.
     * @name graphics.CONTEXT_FEATURE_3D_TEXTURES
     * @constant
     */

    /*#
     * Context feature flag indicating support for ASTC compressed 2D array textures.
     * Some WebGL/GLES drivers fail array texture ASTC uploads while 2D ASTC works.
     * @name graphics.CONTEXT_FEATURE_ASTC_ARRAY_TEXTURES
     * @constant
     */

    /*#
     * Context feature flag indicating support for min/max blend equations.
     * Requires GLES3+ or EXT_blend_minmax.
     * @name graphics.CONTEXT_FEATURE_BLEND_EQUATION_MIN_MAX
     * @constant
     */

    // Returns the short, lowercase Lua-facing name for an adapter family
    // ("opengl", "vulkan", ...). Returns "<unsupported>" for values not
    // covered below — never returns null.
    static const char* AdapterFamilyToName(dmGraphics::AdapterFamily family)
    {
    #define ADAPTER_FAMILY_CASE(c, s) case dmGraphics::c: return s;
        switch(family)
        {
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_NONE,     "none");
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_NULL,     "null");
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_OPENGL,   "opengl");
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_OPENGLES, "opengles");
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_VULKAN,   "vulkan");
        // TODO: For vendor platforms, we should probably return the actual platform literal.
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_VENDOR,   "vendor");
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_WEBGPU,   "webgpu");
        ADAPTER_FAMILY_CASE(ADAPTER_FAMILY_DIRECTX,  "dx12");
        default: break;
        }
    #undef ADAPTER_FAMILY_CASE
        return "<unsupported>";
    }

    /*# get the list of graphics adapters that have been registered with the engine
     *
     * @name graphics.get_engine_adapters
     * @return adapters [type:table] array of adapter family name strings (e.g. "opengl", "vulkan", "webgpu")
     */
    static int Graphics_GetEngineAdapters(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        uint32_t num_adapters = dmGraphics::GetRegisteredAdaptersCount();
        lua_createtable(L, (int) num_adapters, 0);

        for (uint32_t i = 0; i < num_adapters; ++i)
        {
            dmGraphics::HGraphicsAdapter adapter = dmGraphics::GetRegisteredAdapter(i);
            dmGraphics::AdapterFamily adapter_family = dmGraphics::GetAdapterFamily(adapter);

            lua_pushstring(L, AdapterFamilyToName(adapter_family));
            lua_rawseti(L, -2, (int) i + 1);
        }

        return 1;
    }

    /*# get information about the currently installed graphics adapter
     *
     * Returns a table describing the active graphics context: the adapter family,
     * its hardware limits, the list of driver-reported extensions, and the set of
     * optional context features supported by the backend.
     *
     * @name graphics.get_adapter_info
     * @return info [type:table] table with the following fields:
     *
     *   `family`         [type:string]   adapter family name (e.g. "opengl", "vulkan")
     *   `version_major`  [type:number]   adapter API major version (e.g. 1 for Vulkan 1.4)
     *   `version_minor`  [type:number]   adapter API minor version (e.g. 4 for Vulkan 1.4)
     *
     *   `limits`         [type:table]    hardware/driver limits:
     *
     *     `max_texture_size_2d`              [type:number]  max 2D texture dimension in texels
     *     `max_texture_size_3d`              [type:number]  max 3D (volume) texture dimension in texels
     *     `max_texture_size_cube`            [type:number]  max cube map face dimension in texels
     *     `max_texture_array_layers`         [type:number]  max layers in an array texture
     *     `max_framebuffer_width`            [type:number]  max framebuffer width in pixels
     *     `max_framebuffer_height`           [type:number]  max framebuffer height in pixels
     *     `max_color_attachments`            [type:number]  max simultaneous color attachments
     *     `max_samplers_per_stage`           [type:number]  max texture samplers per shader stage
     *     `max_textures_per_stage`           [type:number]  max sampled textures per shader stage
     *     `max_vertex_attributes`            [type:number]  max vertex attributes
     *     `max_vertex_buffers`               [type:number]  max vertex buffer bindings
     *     `max_compute_workgroup_size_x`     [type:number]  max compute workgroup size (X)
     *     `max_compute_workgroup_size_y`     [type:number]  max compute workgroup size (Y)
     *     `max_compute_workgroup_size_z`     [type:number]  max compute workgroup size (Z)
     *     `max_compute_workgroup_invocations` [type:number] max invocations per compute workgroup
     *     `max_compute_shared_memory_size`   [type:number]  max shared memory per compute workgroup (bytes)
     *     `max_uniform_buffer_range`         [type:number]  max bindable uniform buffer range (bytes)
     *     `max_storage_buffer_range`         [type:number]  max bindable storage buffer range (bytes)
     *
     *   `extensions`     [type:table]    array of driver-reported extension name strings
     *
     *   `features`       [type:table]    array of supported context feature ids:
     *
     *     `graphics.CONTEXT_FEATURE_MULTI_TARGET_RENDERING`  multi-target rendering
     *     `graphics.CONTEXT_FEATURE_TEXTURE_ARRAY`           texture arrays
     *     `graphics.CONTEXT_FEATURE_COMPUTE_SHADER`          compute shaders
     *     `graphics.CONTEXT_FEATURE_STORAGE_BUFFER`          storage buffers
     *     `graphics.CONTEXT_FEATURE_VSYNC`                   vertical sync
     *     `graphics.CONTEXT_FEATURE_INSTANCING`              hardware instancing
     *     `graphics.CONTEXT_FEATURE_3D_TEXTURES`             3D (volume) textures
     *     `graphics.CONTEXT_FEATURE_ASTC_ARRAY_TEXTURES`     ASTC compressed 2D array textures
     *     `graphics.CONTEXT_FEATURE_BLEND_EQUATION_MIN_MAX`  min/max blend equations
     */
    static int Graphics_GetAdapterInfo(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmGraphics::HContext context = g_GraphicsModule.m_GraphicsContext;

        lua_newtable(L);

        // adapter family literal
        {
            const char* family_name = "none";
            if (context)
            {
                family_name = AdapterFamilyToName(dmGraphics::GetInstalledAdapterFamily());
            }
            lua_pushstring(L, family_name);
            lua_setfield(L, -2, "family");
        }

        // adapter API version
        {
            uint16_t major = 0, minor = 0;
            if (context)
            {
                dmGraphics::GetAdapterVersion(context, major, minor);
            }
            lua_pushinteger(L, (lua_Integer) major);
            lua_setfield(L, -2, "version_major");
            lua_pushinteger(L, (lua_Integer) minor);
            lua_setfield(L, -2, "version_minor");
        }

        // limits sub-table
        {
            lua_newtable(L);

            dmGraphics::GraphicsContextLimits limits = {};
            if (context)
            {
                dmGraphics::GetGraphicsContextLimits(context, limits);
            }

            // Lua numbers are doubles, so uint64_t fields can lose precision above
            // 2^53. The buffer ranges we report fit comfortably below that today,
            // but cast through lua_Number deliberately so a future change shows
            // up in code review.
        #define PUSH_LIMIT(field, key) \
            lua_pushnumber(L, (lua_Number) limits.field); \
            lua_setfield(L, -2, key);

            // Texture limits
            PUSH_LIMIT(m_MaxTextureSize2D,                "max_texture_size_2d");
            PUSH_LIMIT(m_MaxTextureSize3D,                "max_texture_size_3d");
            PUSH_LIMIT(m_MaxTextureSizeCube,              "max_texture_size_cube");
            PUSH_LIMIT(m_MaxTextureArrayLayers,           "max_texture_array_layers");

            // Framebuffer limits
            PUSH_LIMIT(m_MaxFramebufferWidth,             "max_framebuffer_width");
            PUSH_LIMIT(m_MaxFramebufferHeight,            "max_framebuffer_height");
            PUSH_LIMIT(m_MaxColorAttachments,             "max_color_attachments");

            // Per-stage binding limits
            PUSH_LIMIT(m_MaxSamplersPerStage,             "max_samplers_per_stage");
            PUSH_LIMIT(m_MaxTexturesPerStage,             "max_textures_per_stage");
            PUSH_LIMIT(m_MaxVertexAttributes,             "max_vertex_attributes");
            PUSH_LIMIT(m_MaxVertexBuffers,                "max_vertex_buffers");

            // Compute limits
            PUSH_LIMIT(m_MaxComputeWorkgroupSizeX,        "max_compute_workgroup_size_x");
            PUSH_LIMIT(m_MaxComputeWorkgroupSizeY,        "max_compute_workgroup_size_y");
            PUSH_LIMIT(m_MaxComputeWorkgroupSizeZ,        "max_compute_workgroup_size_z");
            PUSH_LIMIT(m_MaxComputeWorkgroupInvocations,  "max_compute_workgroup_invocations");
            PUSH_LIMIT(m_MaxComputeSharedMemorySize,      "max_compute_shared_memory_size");

            // Buffer limits
            PUSH_LIMIT(m_MaxUniformBufferRange,           "max_uniform_buffer_range");
            PUSH_LIMIT(m_MaxStorageBufferRange,           "max_storage_buffer_range");

        #undef PUSH_LIMIT

            lua_setfield(L, -2, "limits");
        }

        // extensions array
        {
            uint32_t num_extensions = context ? dmGraphics::GetNumSupportedExtensions(context) : 0;
            lua_createtable(L, (int) num_extensions, 0);

            for (uint32_t i = 0; i < num_extensions; ++i)
            {
                const char* ext = dmGraphics::GetSupportedExtension(context, i);
                lua_pushstring(L, ext ? ext : "");
                lua_rawseti(L, -2, (int) i + 1);
            }

            lua_setfield(L, -2, "extensions");
        }

        // features array (list of supported CONTEXT_FEATURE_* ids)
        {
            lua_createtable(L, (int) dmGraphics::MAX_CONTEXT_FEATURE_COUNT, 0);

            int supported_features_count = 0;
            for (int i = 0; i < (int) dmGraphics::MAX_CONTEXT_FEATURE_COUNT; ++i)
            {
                if (context && dmGraphics::IsContextFeatureSupported(context, (dmGraphics::ContextFeature) i))
                {
                    lua_pushnumber(L, (lua_Number) i);
                    lua_rawseti(L, -2, ++supported_features_count);
                }
            }

            lua_setfield(L, -2, "features");
        }

        return 1;
    }

    static const luaL_reg ScriptGraphics_methods[] =
    {
        {"get_engine_adapters", Graphics_GetEngineAdapters},
        {"get_adapter_info", Graphics_GetAdapterInfo},
        {0, 0}
    };

    void InitializeGraphics(lua_State* L, dmGraphics::HContext graphics_context)
    {
        g_GraphicsModule.m_GraphicsContext = graphics_context;

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

        // BlendEquation
        SET_GRAPHICS_ENUM(BLEND_EQUATION_ADD);
        SET_GRAPHICS_ENUM(BLEND_EQUATION_SUBTRACT);
        SET_GRAPHICS_ENUM(BLEND_EQUATION_REVERSE_SUBTRACT);
        SET_GRAPHICS_ENUM(BLEND_EQUATION_MIN);
        SET_GRAPHICS_ENUM(BLEND_EQUATION_MAX);

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

        // VertexAttribute::DataType
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_BYTE,           DATA_TYPE_BYTE);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_UNSIGNED_BYTE,  DATA_TYPE_UNSIGNED_BYTE);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_SHORT,          DATA_TYPE_SHORT);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_UNSIGNED_SHORT, DATA_TYPE_UNSIGNED_SHORT);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_INT,            DATA_TYPE_INT);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_UNSIGNED_INT,   DATA_TYPE_UNSIGNED_INT);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::TYPE_FLOAT,          DATA_TYPE_FLOAT);

        // VertexAttribute::SemanticType
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_NONE,                 SEMANTIC_TYPE_NONE);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_POSITION,             SEMANTIC_TYPE_POSITION);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_TEXCOORD,             SEMANTIC_TYPE_TEXCOORD);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX,           SEMANTIC_TYPE_PAGE_INDEX);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_COLOR,                SEMANTIC_TYPE_COLOR);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_NORMAL,               SEMANTIC_TYPE_NORMAL);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_TANGENT,              SEMANTIC_TYPE_TANGENT);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX,         SEMANTIC_TYPE_WORLD_MATRIX);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX,        SEMANTIC_TYPE_NORMAL_MATRIX);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_BONE_WEIGHTS,         SEMANTIC_TYPE_BONE_WEIGHTS);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_BONE_INDICES,         SEMANTIC_TYPE_BONE_INDICES);
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D, SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D);

        // CoordinateSpace
        SET_GRAPHICS_ENUM(COORDINATE_SPACE_DEFAULT);
        SET_GRAPHICS_ENUM(COORDINATE_SPACE_WORLD);
        SET_GRAPHICS_ENUM(COORDINATE_SPACE_LOCAL);

        // Context features
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_MULTI_TARGET_RENDERING);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_TEXTURE_ARRAY);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_COMPUTE_SHADER);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_STORAGE_BUFFER);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_VSYNC);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_INSTANCING);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_3D_TEXTURES);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_ASTC_ARRAY_TEXTURES);
        SET_GRAPHICS_ENUM(CONTEXT_FEATURE_BLEND_EQUATION_MIN_MAX);

    #undef SET_GRAPHICS_ENUM_NAMED
    #undef SET_GRAPHICS_ENUM
    #undef SET_TEXTUREFORMAT_IF_SUPPORTED

        lua_pop(L, 1);
    }

    #undef SCRIPT_LIB_NAME
}
