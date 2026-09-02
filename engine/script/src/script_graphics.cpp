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

    /*# Blend factors
     * @enum
     * @name graphics.BLEND_FACTOR
     * @member graphics.BLEND_FACTOR_CONSTANT_ALPHA constant blend alpha for every component
     * @member graphics.BLEND_FACTOR_CONSTANT_COLOR constant blend color
     * @member graphics.BLEND_FACTOR_DST_ALPHA destination alpha for every component
     * @member graphics.BLEND_FACTOR_DST_COLOR destination color
     * @member graphics.BLEND_FACTOR_ONE one for every component
     * @member graphics.BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA one minus the constant blend alpha for every component
     * @member graphics.BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR one minus the constant blend color
     * @member graphics.BLEND_FACTOR_ONE_MINUS_DST_ALPHA one minus the destination alpha for every component
     * @member graphics.BLEND_FACTOR_ONE_MINUS_DST_COLOR one minus the destination color
     * @member graphics.BLEND_FACTOR_ONE_MINUS_SRC_ALPHA one minus the source alpha for every component
     * @member graphics.BLEND_FACTOR_ONE_MINUS_SRC_COLOR one minus the source color
     * @member graphics.BLEND_FACTOR_SRC_ALPHA source alpha for every component
     * @member graphics.BLEND_FACTOR_SRC_ALPHA_SATURATE minimum of source alpha and one minus destination alpha for color, and one for alpha
     * @member graphics.BLEND_FACTOR_SRC_COLOR source color
     * @member graphics.BLEND_FACTOR_ZERO zero for every component
     */

    /*# Blend equations
     * @enum
     * @name graphics.BLEND_EQUATION
     * @member graphics.BLEND_EQUATION_ADD
     * @member graphics.BLEND_EQUATION_MAX
     * @member graphics.BLEND_EQUATION_MIN
     * @member graphics.BLEND_EQUATION_REVERSE_SUBTRACT
     * @member graphics.BLEND_EQUATION_SUBTRACT
     */

    /*# Buffer types
     * @enum
     * @name graphics.BUFFER_TYPE
     * @member graphics.BUFFER_TYPE_COLOR0_BIT first color attachment
     * @member graphics.BUFFER_TYPE_COLOR1_BIT [type:graphics.BUFFER_TYPE|nil] second color attachment; may be nil if multiple render targets are unsupported
     * @member graphics.BUFFER_TYPE_COLOR2_BIT [type:graphics.BUFFER_TYPE|nil] third color attachment; may be nil if multiple render targets are unsupported
     * @member graphics.BUFFER_TYPE_COLOR3_BIT [type:graphics.BUFFER_TYPE|nil] fourth color attachment; may be nil if multiple render targets are unsupported
     * @member graphics.BUFFER_TYPE_DEPTH_BIT depth attachment
     * @member graphics.BUFFER_TYPE_STENCIL_BIT stencil attachment
     */

    /*# Vertex attribute data types
     * @enum
     * @name graphics.DATA_TYPE
     * @member graphics.DATA_TYPE_BYTE Signed 8-bit vertex attribute data.
     * @member graphics.DATA_TYPE_FLOAT 32-bit floating-point vertex attribute data.
     * @member graphics.DATA_TYPE_INT Signed 32-bit vertex attribute data.
     * @member graphics.DATA_TYPE_SHORT Signed 16-bit vertex attribute data.
     * @member graphics.DATA_TYPE_UNSIGNED_BYTE Unsigned 8-bit vertex attribute data.
     * @member graphics.DATA_TYPE_UNSIGNED_INT Unsigned 32-bit vertex attribute data.
     * @member graphics.DATA_TYPE_UNSIGNED_SHORT Unsigned 16-bit vertex attribute data.
     */

    /*# Comparison functions
     * @enum
     * @name graphics.COMPARE_FUNC
     * @member graphics.COMPARE_FUNC_ALWAYS always passes
     * @member graphics.COMPARE_FUNC_EQUAL passes when the values are equal
     * @member graphics.COMPARE_FUNC_GEQUAL passes when the incoming value is greater than or equal to the stored value
     * @member graphics.COMPARE_FUNC_GREATER passes when the incoming value is greater than the stored value
     * @member graphics.COMPARE_FUNC_LEQUAL passes when the incoming value is less than or equal to the stored value
     * @member graphics.COMPARE_FUNC_LESS passes when the incoming value is less than the stored value
     * @member graphics.COMPARE_FUNC_NEVER never passes
     * @member graphics.COMPARE_FUNC_NOTEQUAL passes when the values are not equal
     */

    /*# Vertex attribute coordinate spaces
     * @enum
     * @name graphics.COORDINATE_SPACE
     * @member graphics.COORDINATE_SPACE_DEFAULT Default vertex attribute coordinate space.
     * @member graphics.COORDINATE_SPACE_LOCAL Local vertex attribute coordinate space.
     * @member graphics.COORDINATE_SPACE_WORLD World vertex attribute coordinate space.
     */

    /*# Optional graphics-context features
     * @enum
     * @name graphics.CONTEXT_FEATURE
     * @member graphics.CONTEXT_FEATURE_3D_TEXTURES Context feature flag indicating support for 3D (volume) textures.
     * @member graphics.CONTEXT_FEATURE_ASTC_ARRAY_TEXTURES Context feature flag indicating support for ASTC compressed 2D array textures. Some WebGL/GLES drivers fail array texture ASTC uploads while 2D ASTC works.
     * @member graphics.CONTEXT_FEATURE_BLEND_EQUATION_MIN_MAX Context feature flag indicating support for min/max blend equations. Requires GLES3+ or EXT_blend_minmax.
     * @member graphics.CONTEXT_FEATURE_COMPUTE_SHADER Context feature flag indicating support for compute shaders.
     * @member graphics.CONTEXT_FEATURE_INSTANCING Context feature flag indicating support for hardware instancing.
     * @member graphics.CONTEXT_FEATURE_MULTI_TARGET_RENDERING Context feature flag indicating support for rendering to multiple color targets simultaneously.
     * @member graphics.CONTEXT_FEATURE_STORAGE_BUFFER Context feature flag indicating support for storage buffers.
     * @member graphics.CONTEXT_FEATURE_TEXTURE_ARRAY Context feature flag indicating support for texture arrays.
     * @member graphics.CONTEXT_FEATURE_VSYNC Context feature flag indicating support for vertical sync (vsync).
     */

    /*# Texture compression types
     * @enum
     * @name graphics.COMPRESSION_TYPE
     * @member graphics.COMPRESSION_TYPE_BASIS_ETC1S
     * @member graphics.COMPRESSION_TYPE_BASIS_UASTC
     * @member graphics.COMPRESSION_TYPE_DEFAULT
     * @member graphics.COMPRESSION_TYPE_WEBP
     * @member graphics.COMPRESSION_TYPE_WEBP_LOSSY
     */

    /*# Face types
     * @enum
     * @name graphics.FACE_TYPE
     * @member graphics.FACE_TYPE_BACK back-facing polygons
     * @member graphics.FACE_TYPE_FRONT front-facing polygons
     * @member graphics.FACE_TYPE_FRONT_AND_BACK both front- and back-facing polygons
     */

    /*# Graphics states
     * @enum
     * @name graphics.STATE
     * @member graphics.STATE_ALPHA_TEST
     * @member graphics.STATE_ALPHA_TEST_SUPPORTED
     * @member graphics.STATE_BLEND
     * @member graphics.STATE_CULL_FACE
     * @member graphics.STATE_DEPTH_TEST
     * @member graphics.STATE_POLYGON_OFFSET_FILL
     * @member graphics.STATE_SCISSOR_TEST
     * @member graphics.STATE_STENCIL_TEST
     */

    /*# Vertex attribute semantic types
     * @enum
     * @name graphics.SEMANTIC_TYPE
     * @member graphics.SEMANTIC_TYPE_BONE_INDICES Bone-index vertex attribute.
     * @member graphics.SEMANTIC_TYPE_BONE_WEIGHTS Bone-weight vertex attribute.
     * @member graphics.SEMANTIC_TYPE_COLOR Color vertex attribute.
     * @member graphics.SEMANTIC_TYPE_MORPH_TARGET_WEIGHTS Morph-target-weight vertex attribute.
     * @member graphics.SEMANTIC_TYPE_NONE Vertex attribute without a predefined semantic.
     * @member graphics.SEMANTIC_TYPE_NORMAL Normal vertex attribute.
     * @member graphics.SEMANTIC_TYPE_NORMAL_MATRIX Normal-matrix vertex attribute.
     * @member graphics.SEMANTIC_TYPE_PAGE_INDEX Texture page-index vertex attribute.
     * @member graphics.SEMANTIC_TYPE_POSITION Position vertex attribute.
     * @member graphics.SEMANTIC_TYPE_TANGENT Tangent vertex attribute.
     * @member graphics.SEMANTIC_TYPE_TEXCOORD Texture-coordinate vertex attribute.
     * @member graphics.SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D 2D texture-transform vertex attribute.
     * @member graphics.SEMANTIC_TYPE_WORLD_MATRIX World-matrix vertex attribute.
     */

    /*# Stencil operations
     * @enum
     * @name graphics.STENCIL_OP
     * @member graphics.STENCIL_OP_DECR decrement and clamp at zero
     * @member graphics.STENCIL_OP_DECR_WRAP decrement and wrap zero to the maximum unsigned value
     * @member graphics.STENCIL_OP_INCR increment and clamp at the maximum unsigned value
     * @member graphics.STENCIL_OP_INCR_WRAP increment and wrap the maximum unsigned value to zero
     * @member graphics.STENCIL_OP_INVERT bitwise invert
     * @member graphics.STENCIL_OP_KEEP keep the current value
     * @member graphics.STENCIL_OP_REPLACE replace with the reference value from [ref:render.set_stencil_func]
     * @member graphics.STENCIL_OP_ZERO set to zero
     */

    /*# Texture filters
     * @enum
     * @name graphics.TEXTURE_FILTER
     * @member graphics.TEXTURE_FILTER_DEFAULT
     * @member graphics.TEXTURE_FILTER_LINEAR
     * @member graphics.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR
     * @member graphics.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST
     * @member graphics.TEXTURE_FILTER_NEAREST
     * @member graphics.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR
     * @member graphics.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST
     */

    /*# Texture formats
     * @enum
     * @name graphics.TEXTURE_FORMAT
     * @member graphics.TEXTURE_FORMAT_BGRA8U [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_DEPTH
     * @member graphics.TEXTURE_FORMAT_LUMINANCE [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_LUMINANCE_ALPHA [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_R16F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_R32F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_R32UI [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RG16F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RG32F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB16F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB32F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA16F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA32F [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA32UI [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_16BPP [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_ASTC_4X4 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_BC3 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_BC7 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_ETC2 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB_16BPP [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB_BC1 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB_ETC1 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RG_BC5 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_RG_ETC2 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_R_BC4 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_R_ETC2 [type:graphics.TEXTURE_FORMAT|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_FORMAT_STENCIL
     */

    /*# Texture types
     * @enum
     * @name graphics.TEXTURE_TYPE
     * @member graphics.TEXTURE_TYPE_2D
     * @member graphics.TEXTURE_TYPE_2D_ARRAY
     * @member graphics.TEXTURE_TYPE_3D [type:graphics.TEXTURE_TYPE|nil] May be nil if the graphics driver doesn't support it
     * @member graphics.TEXTURE_TYPE_CUBE_MAP
     * @member graphics.TEXTURE_TYPE_IMAGE_2D
     * @member graphics.TEXTURE_TYPE_IMAGE_3D [type:graphics.TEXTURE_TYPE|nil] May be nil if the graphics driver doesn't support it
     */

    /*# Texture usage flags
     * @enum
     * @name graphics.TEXTURE_USAGE_FLAG
     * @member graphics.TEXTURE_USAGE_FLAG_COLOR
     * @member graphics.TEXTURE_USAGE_FLAG_INPUT
     * @member graphics.TEXTURE_USAGE_FLAG_MEMORYLESS
     * @member graphics.TEXTURE_USAGE_FLAG_SAMPLE
     * @member graphics.TEXTURE_USAGE_FLAG_STORAGE
     */

    /*# Texture wrapping modes
     * @enum
     * @name graphics.TEXTURE_WRAP
     * @member graphics.TEXTURE_WRAP_CLAMP_TO_BORDER
     * @member graphics.TEXTURE_WRAP_CLAMP_TO_EDGE
     * @member graphics.TEXTURE_WRAP_MIRRORED_REPEAT
     * @member graphics.TEXTURE_WRAP_REPEAT
     */

    /*# Graphics context limits
     * @struct
     * @name graphics.adapter_limits
     * @member max_texture_size_2d [type:integer] Maximum 2D texture dimension in texels.
     * @member max_texture_size_3d [type:integer] Maximum 3D texture dimension in texels.
     * @member max_texture_size_cube [type:integer] Maximum cube-map face dimension in texels.
     * @member max_texture_array_layers [type:integer] Maximum number of array texture layers.
     * @member max_framebuffer_width [type:integer] Maximum framebuffer width in pixels.
     * @member max_framebuffer_height [type:integer] Maximum framebuffer height in pixels.
     * @member max_color_attachments [type:integer] Maximum number of simultaneous color attachments.
     * @member max_samplers_per_stage [type:integer] Maximum number of texture samplers per shader stage.
     * @member max_textures_per_stage [type:integer] Maximum number of sampled textures per shader stage.
     * @member max_vertex_attributes [type:integer] Maximum number of vertex attributes.
     * @member max_vertex_buffers [type:integer] Maximum number of vertex-buffer bindings.
     * @member max_compute_workgroup_size_x [type:integer] Maximum compute workgroup size on the X axis.
     * @member max_compute_workgroup_size_y [type:integer] Maximum compute workgroup size on the Y axis.
     * @member max_compute_workgroup_size_z [type:integer] Maximum compute workgroup size on the Z axis.
     * @member max_compute_workgroup_invocations [type:integer] Maximum invocations per compute workgroup.
     * @member max_compute_shared_memory_size [type:integer] Maximum shared memory per compute workgroup in bytes.
     * @member max_uniform_buffer_range [type:integer] Maximum bindable uniform-buffer range in bytes.
     * @member max_storage_buffer_range [type:integer] Maximum bindable storage-buffer range in bytes.
     */

    /*# Graphics adapter information
     * @struct
     * @name graphics.adapter_info
     * @member family [type:string] Adapter family name.
     * @member version_major [type:integer] Adapter API major version.
     * @member version_minor [type:integer] Adapter API minor version.
     * @member limits [type:graphics.adapter_limits] Hardware and driver limits.
     * @member extensions [type:string[]] Driver-reported extension names.
     * @member features [type:graphics.CONTEXT_FEATURE[]] Supported optional context features.
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
     * @return adapters [type:string[]] array of adapter family name strings (e.g. "opengl", "vulkan", "webgpu")
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
     * @return info [type:graphics.adapter_info] information about the active graphics adapter and context
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
        SET_GRAPHICS_ENUM_NAMED(VertexAttribute::SEMANTIC_TYPE_MORPH_TARGET_WEIGHTS, SEMANTIC_TYPE_MORPH_TARGET_WEIGHTS);

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
