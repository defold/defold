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

#ifndef DM_RENDER_H
#define DM_RENDER_H

#include <string.h>
#include <stdint.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/render/render.h>
#include <dmsdk/graphics/graphics.h>

#include <dlib/hash.h>
#include <script/script.h>
#include <script/lua_source_ddf.h>
#include <graphics/graphics.h>
#include "render/material_ddf.h"

namespace dmRender
{
    extern const char* RENDER_SOCKET_NAME;

    static const uint32_t MAX_MATERIAL_TAG_COUNT = 32; // Max tag count per material

    // Keep only declarations to deduplicate symbols in binary
    // Definitions are in render.cpp
    extern const dmhash_t VERTEX_STREAM_POSITION;
    extern const dmhash_t VERTEX_STREAM_NORMAL;
    extern const dmhash_t VERTEX_STREAM_TANGENT;
    extern const dmhash_t VERTEX_STREAM_COLOR;
    extern const dmhash_t VERTEX_STREAM_TEXCOORD0;
    extern const dmhash_t VERTEX_STREAM_TEXCOORD1;
    extern const dmhash_t VERTEX_STREAM_PAGE_INDEX;
    extern const dmhash_t VERTEX_STREAM_WORLD_MATRIX;
    extern const dmhash_t VERTEX_STREAM_NORMAL_MATRIX;
    extern const dmhash_t VERTEX_STREAM_BONE_WEIGHTS;
    extern const dmhash_t VERTEX_STREAM_BONE_INDICES;
    extern const dmhash_t VERTEX_STREAM_ANIMATION_DATA;
    extern const dmhash_t SAMPLER_POSE_MATRIX_CACHE;

    typedef struct RenderTargetSetup*       HRenderTargetSetup;
    typedef uint64_t                        HRenderType;
    typedef struct RenderScript*            HRenderScript;
    typedef struct RenderScriptInstance*    HRenderScriptInstance;
    typedef struct Predicate*               HPredicate;
    typedef struct ComputeProgram*          HComputeProgram;
    typedef uintptr_t                       HRenderBuffer;
    typedef struct BufferedRenderBuffer*    HBufferedRenderBuffer;
    typedef HOpaqueHandle                   HRenderCamera;

    static const uint8_t RENDERLIST_INVALID_DISPATCH       = 0xff;
    static const HRenderType INVALID_RENDER_TYPE_HANDLE    = ~0ULL;
    static const uint32_t INVALID_SAMPLER_UNIT             = 0xffffffff;
    static const uint8_t  INVALID_MATERIAL_ATTRIBUTE_INDEX = 0xff;

    /**
     * Display profiles handle
     */
    typedef struct DisplayProfiles* HDisplayProfiles;

    enum RenderScriptResult
    {
        RENDER_SCRIPT_RESULT_FAILED = -1,
        RENDER_SCRIPT_RESULT_NO_FUNCTION = 0,
        RENDER_SCRIPT_RESULT_OK = 1
    };

    enum RenderResourceType
    {
        RENDER_RESOURCE_TYPE_INVALID       = 0,
        RENDER_RESOURCE_TYPE_MATERIAL      = 1,
        RENDER_RESOURCE_TYPE_RENDER_TARGET = 2,
        RENDER_RESOURCE_TYPE_COMPUTE       = 3,
    };

    enum RenderBufferType
    {
        RENDER_BUFFER_TYPE_VERTEX_BUFFER = 0,
        RENDER_BUFFER_TYPE_INDEX_BUFFER  = 1,
    };

    enum TextAlign
    {
        TEXT_ALIGN_LEFT = 0,
        TEXT_ALIGN_CENTER = 1,
        TEXT_ALIGN_RIGHT = 2
    };

    enum TextVAlign
    {
        TEXT_VALIGN_TOP = 0,
        TEXT_VALIGN_MIDDLE = 1,
        TEXT_VALIGN_BOTTOM = 2
    };

    enum RenderContextEvent
    {
        CONTEXT_LOST = 0,
        CONTEXT_RESTORED = 1
    };

    enum SortOrder
    {
        SORT_UNSPECIFIED   = 0,
        SORT_BACK_TO_FRONT = 1,
        SORT_FRONT_TO_BACK = 2,
        SORT_NONE          = 3
    };

    struct Predicate
    {
        static const uint32_t MAX_TAG_COUNT = 32;
        dmhash_t m_Tags[MAX_TAG_COUNT];
        uint32_t m_TagCount;
    };

    struct Constant
    {
        dmVMath::Vector4*                       m_Values;
        dmhash_t                                m_NameHash;
        dmRenderDDF::MaterialDesc::ConstantType m_Type;         // TODO: Make this a uint16_t as well
        dmGraphics::HUniformLocation            m_Location;     // Vulkan encodes vs/fs location in the lower/upper bits
        uint16_t                                m_NumValues:15;
        uint16_t                                m_AllocatedValues:1; // If set, this constant owns the actual values

        Constant();
        Constant(dmhash_t name_hash, dmGraphics::HUniformLocation location);
    };

    struct RenderConstant
    {
        HConstant  m_Constant;
        dmhash_t   m_ElementIdsName[4];
    };

    struct RenderContextParams
    {
        RenderContextParams();

        dmScript::HContext              m_ScriptContext;
        HFontMap                        m_SystemFontMap;
        void*                           m_ShaderProgramDesc;
        uint32_t                        m_MaxRenderTypes;
        uint32_t                        m_MaxInstances;
        uint32_t                        m_MaxRenderTargets;
        uint32_t                        m_ShaderProgramDescSize;
        uint32_t                        m_MaxCharacters;
        uint32_t                        m_MaxBatches;
        uint32_t                        m_CommandBufferSize;
        /// Max debug vertex count
        /// NOTE: This is per debug-type and not the total sum
        uint32_t                        m_MaxDebugVertexCount;
    };

    // NOTE: These enum values are duplicated in gamesys camera DDF (camera_ddf.proto)
    // Don't forget to change dmGamesysDDF::OrthoZoomMode if you change here
    enum OrthoZoomMode
    {
        ORTHO_MODE_FIXED        = 0,
        ORTHO_MODE_AUTO_FIT     = 1,
        ORTHO_MODE_AUTO_COVER   = 2,
    };

    struct RenderCameraData
    {
        dmVMath::Vector4 m_Viewport;
        float            m_AspectRatio;
        float            m_Fov;
        float            m_NearZ;
        float            m_FarZ;
        float            m_OrthographicZoom;
        // These bitfields are packed into a single byte
        uint8_t          m_AutoAspectRatio        : 1;
        uint8_t          m_OrthographicProjection : 1;
        uint8_t          m_OrthographicMode   : 2; // dmRender::OrthoZoomMode
    };

    struct MaterialProgramAttributeInfo
    {
        dmhash_t                           m_AttributeNameHash;
        const dmGraphics::VertexAttribute* m_Attribute;
        const uint8_t*                     m_ValuePtr;
        dmhash_t                           m_ElementIds[4];
        uint32_t                           m_ElementIndex;
    };

    HRenderContext NewRenderContext(dmGraphics::HContext graphics_context, const RenderContextParams& params);
    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context);

    dmScript::HContext GetScriptContext(HRenderContext render_context);

    void RenderListBegin(HRenderContext render_context);
    void RenderListEnd(HRenderContext render_context);

    void SetSystemFontMap(HRenderContext render_context, HFontMap font_map);

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context);

    const dmVMath::Matrix4& GetViewProjectionMatrix(HRenderContext render_context);
    dmVMath::Matrix4 GetNormalMatrix(HRenderContext render_context, const dmVMath::Matrix4& world_matrix);

    void SetViewMatrix(HRenderContext render_context, const dmVMath::Matrix4& view);
    void SetProjectionMatrix(HRenderContext render_context, const dmVMath::Matrix4& projection);

    HMaterial GetContextMaterial(HRenderContext render_context);

    Result ClearRenderObjects(HRenderContext context);

    // Takes the contents of the render list, sorts by view and inserts all the objects in the
    // render list, unless they already are in place from a previous call.
    Result DrawRenderList(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer, const FrustumOptions* frustum_options, SortOrder sort_order);

    Result Draw(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer);
    Result DrawDebug3d(HRenderContext context, const FrustumOptions* frustum_options);
    Result DrawDebug2d(HRenderContext context);

    void SetRenderPause(HRenderContext context, uint8_t is_paused);
    bool IsRenderPaused(HRenderContext context);

    /**
     * Render debug square. The upper left corner of the screen is (-1,-1) and the bottom right is (1,1).
     * @param context Render context handle
     * @param x0 x coordinate of the left edge of the square
     * @param y0 y coordinate of the upper edge of the square
     * @param x1 x coordinate of the right edge of the square
     * @param y1 y coordinate of the bottom edge of the square
     * @param color Color
     */
    void Square2d(HRenderContext context, float x0, float y0, float x1, float y1, dmVMath::Vector4 color);

    /**
     * Maps screen coordinates to a world-space point along the pixel ray.
     * The mapping is viewport-aware and woks for both perspective and orthographic cameras.
     *
     * z is interpreted as view depth in world units measured from the camera plane,
     * along the camera forward axis. The returned point lies on the ray passing through
     * (screen_x, screen_y) at the specified view depth. The point is guaranteed to be
     * inside the camera frustum only if z is between the camera near and far planes.
     *
     * @param render_context Render context handle
     * @param camera         Camera handle
     * @param screen_x       X coordinate in window pixels
     * @param screen_y       Y coordinate in window pixels
     * @param z              View depth in world units from the camera plane
     * @param out_world      Output world-space position
     * @return Result        RESULT_OK on success, error otherwise
     */
    Result CameraScreenToWorld(HRenderContext render_context, HRenderCamera camera, float screen_x, float screen_y, float z, dmVMath::Vector3* out_world);

    /**
     * Maps a world-space position to screen coordinates.
     * The mapping is viewport-aware and works for both perspective and orthographic cameras.
     *
     * Returns screen-space X and Y in window pixels and Z as the view depth in world units
     * measured from the camera plane along the camera forward axis. The value of Z can be
     * used with CameraScreenToWorld to reconstruct the world position on the same pixel ray.
     *
     * @param render_context Render context handle
     * @param camera         Camera handle
     * @param world          World-space position
     * @param out_screen     Output screen-space position (x,y in pixels, z is view depth)
     * @return Result        RESULT_OK on success, error otherwise
     */
    Result CameraWorldToScreen(HRenderContext render_context, HRenderCamera camera, const dmVMath::Vector3& world, dmVMath::Vector3* out_screen);

    /**
     * Render debug triangle in world space.
     * @param context Render context handle
     * @param vertices Vertices of the triangle, CW winding
     * @param color Color
     */
    void Triangle3d(HRenderContext context, dmVMath::Point3 vertices[3], dmVMath::Vector4 color);

    /**
     * Render debug line. The upper left corner of the screen is (-1,-1) and the bottom right is (1,1).
     * @param context Render context handle
     * @param x0 x coordinate of the start of the line
     * @param y0 y coordinate of the start of the line
     * @param x1 x coordinate of the end of the line
     * @param y1 y coordinate of the end of the line
     * @param color0 Color of the start of the line
     * @param color1 Color of the end of the line
     */
    void Line2D(HRenderContext context, float x0, float y0, float x1, float y1, dmVMath::Vector4 color0, dmVMath::Vector4 color1);

    /**
     * Line3D Render debug line
     * @param context Render context handle
     * @param start Start point
     * @param end End point
     * @param color Color
     */
    void Line3D(HRenderContext context, dmVMath::Point3 start, dmVMath::Point3 end, dmVMath::Vector4 start_color, dmVMath::Vector4 end_color);

    HRenderScript   NewRenderScript(HRenderContext render_context, dmLuaDDF::LuaSource *source);

    bool            ReloadRenderScript(HRenderContext render_context, HRenderScript render_script, dmLuaDDF::LuaSource *source);

    void            DeleteRenderScript(HRenderContext render_context, HRenderScript render_script);

    HRenderScriptInstance   NewRenderScriptInstance(HRenderContext render_context, HRenderScript render_script);
    void                    DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance);
    void                    SetRenderScriptInstanceRenderScript(HRenderScriptInstance render_script_instance, HRenderScript render_script);

    void                    AddRenderScriptInstanceRenderResource(HRenderScriptInstance render_script_instance, const char* name, uint64_t resource, RenderResourceType type);
    void                    ClearRenderScriptInstanceRenderResources(HRenderScriptInstance render_script_instance);

    RenderScriptResult      InitRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      DispatchRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      UpdateRenderScriptInstance(HRenderScriptInstance render_script_instance, float dt);
    void                    OnReloadRenderScriptInstance(HRenderScriptInstance render_script_instance);

    // Material
    dmGraphics::HProgram            GetMaterialProgram(HMaterial material);
    void                            SetMaterialProgramConstantType(HMaterial material, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool                            GetMaterialProgramConstant(HMaterial, dmhash_t name_hash, HConstant& out_value);

    Result                          SetConstantValuesRef(HConstant constant, dmVMath::Vector4* values, uint32_t num_values);

    dmGraphics::HVertexDeclaration  GetVertexDeclaration(HMaterial material);
    dmGraphics::HVertexDeclaration  GetVertexDeclaration(HMaterial material, dmGraphics::VertexStepFunction step_function);
    bool                            GetMaterialProgramAttributeInfo(HMaterial material, dmhash_t name_hash, MaterialProgramAttributeInfo& info);
    void                            GetMaterialProgramAttributes(HMaterial material, const dmGraphics::VertexAttribute** attributes, uint32_t* attribute_count);
    void                            GetMaterialProgramAttributeValues(HMaterial material, uint32_t index, const uint8_t** value_ptr, uint32_t* num_values);
    void                            SetMaterialProgramAttributes(HMaterial material, const dmGraphics::VertexAttribute* attributes, uint32_t attributes_count);
    void                            GetMaterialProgramAttributeMetadata(HMaterial material, dmGraphics::VertexAttributeInfoMetadata* metadata);
    uint8_t                         GetMaterialAttributeIndex(HMaterial material, dmhash_t name_hash);
    bool                            GetMaterialHasSkinnedAttributes(HMaterial material);
    bool                            GetMaterialHasSkinnedMatrixCache(HMaterial material);

    // Compute
    HComputeProgram                 NewComputeProgram(HRenderContext render_context, dmGraphics::HProgram program);
    void                            DeleteComputeProgram(dmRender::HRenderContext render_context, HComputeProgram program);
    HSampler                        GetComputeProgramSampler(HComputeProgram program, uint32_t unit);
    HRenderContext                  GetProgramRenderContext(HComputeProgram program);
    dmGraphics::HProgram            GetComputeProgram(HComputeProgram program);
    void                            SetComputeProgramConstant(HComputeProgram compute_program, dmhash_t name_hash, dmVMath::Vector4* values, uint32_t count);
    void                            SetComputeProgramConstantType(HComputeProgram compute_program, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool                            SetComputeProgramSampler(HComputeProgram compute_program, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy);
    uint32_t                        GetComputeProgramSamplerUnit(HComputeProgram compute_program, dmhash_t name_hash);
    bool                            GetComputeProgramConstant(HComputeProgram compute_program, dmhash_t name_hash, HConstant& out_value);

    /** Retrieve info about a hash related to a program constant
     * The function checks if the hash matches a constant or any element of it.
     * In the former case, the available element ids are returned.
     * In the latter, the index of the element is returned (~0u otherwise).
     * @param material Material to search for constants
     * @param name_hash Hash of the constant name or element id
     * @param out_constant_id Hash of the constant name
     * @param out_element_ids List of 4 element ids, only set if the hash matched a constant
     * @param out_element_index Set if the hash matched an element
     * @return True if a constant or element was found
     */
    bool                            GetMaterialProgramConstantInfo(HMaterial material, dmhash_t name_hash, dmhash_t* out_constant_id, dmhash_t* out_element_ids[4], uint32_t* out_element_index, uint16_t* out_num_components);

    void                            SetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, dmVMath::Vector4* constant, uint32_t count);
    dmGraphics::HUniformLocation    GetMaterialConstantLocation(HMaterial material, dmhash_t name_hash);

    HRenderContext                  GetMaterialRenderContext(HMaterial material);
    void                            SetMaterialVertexSpace(HMaterial material, dmRenderDDF::MaterialDesc::VertexSpace vertex_space);

    void                            ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBuffer buffer);
    void                            ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HComputeProgram program, HNamedConstantBuffer buffer);

    HPredicate                      NewPredicate();
    void                            DeletePredicate(HPredicate predicate);
    Result                          AddPredicateTag(HPredicate predicate, dmhash_t tag);

    HConstant                       NewConstant(dmhash_t name_hash);

    /** PBR properties
     *
     */
    void SetMaterialPBRParameters(HMaterial material, const dmRenderDDF::MaterialDesc::PbrParameters* parameters);
    void GetMaterialPBRParameters(HMaterial material, dmRenderDDF::MaterialDesc::PbrParameters* parameters);

    /** Buffered render buffers
     * A render buffer is a thin wrapper around vertex and index buffers that, depending on graphics context,
     * can allocate more backing storage if needed. E.g for Vulkan and vendor adapters, we cannot reuse the same
     * vertex buffer during one scene since we will rewrite the data from the previous draw call during the frame.
     *
     * A typical usage scenario will look like this:
     * // During a frame
     * HRenderBuffer draw_call_1 = AddRenderBuffer(ctx, buffer);
     * HRenderBuffer draw_call_2 = AddRenderBuffer(ctx, buffer);
     * ...
     *
     * // After all draw calls have been submitted
     * TrimBuffer(ctx, buffer);
     * RewindBuffer(ctx, buffer);
     *
     * Note on trimming and rewind:
     * Trimming the buffer means that we will resize the buffer count to
     * however many internal render buffers were used since last Rewind call.
     * This can be used to reduce the amount of unused buffers between consecutive calls,
     * as well as to avoid excessive buffer allocations between frames.
     * Rewinding the buffer means that we set the buffer index to the head of the buffer list,
     * to prepare for setting data to the beginning of the buffer list again.
     */
    HBufferedRenderBuffer           NewBufferedRenderBuffer(HRenderContext render_context, RenderBufferType type);
    void                            DeleteBufferedRenderBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);
    HRenderBuffer                   AddRenderBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);
    HRenderBuffer                   GetBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);
    int32_t                         GetBufferIndex(HRenderContext render_context, HBufferedRenderBuffer buffer);
    void                            SetBufferData(HRenderContext render_context, HBufferedRenderBuffer buffer, uint32_t size, void* data, dmGraphics::BufferUsage buffer_usage);
    void                            SetBufferSubData(HRenderContext render_context, HBufferedRenderBuffer buffer, uint32_t offset, uint32_t size, void* data);
    void                            TrimBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);
    void                            RewindBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);

    /** Render cameras
     * A render camera is a wrapper around common camera properties such as fov, near and far planes, viewport and aspect ratio.
     * Within the engine, the render cameras are "owned" by the renderer, but they can be manipulated elsewhere.
     * The render script can set current camera used for rendering by using render.set_camera(...), which will automatically
     * take precedence over the view and projection matrices set by render.set_view() and render.set_projection().
     */
    HRenderCamera                   NewRenderCamera(HRenderContext context);
    void                            DeleteRenderCamera(HRenderContext context, HRenderCamera camera);
    void                            SetRenderCameraURL(HRenderContext render_context, HRenderCamera camera, const dmMessage::URL* camera_url);
    void                            GetRenderCameraView(HRenderContext render_context, HRenderCamera camera, dmVMath::Matrix4* mtx);
    void                            GetRenderCameraProjection(HRenderContext render_context, HRenderCamera camera, dmVMath::Matrix4* mtx);
    void                            SetRenderCameraData(HRenderContext render_context, HRenderCamera camera, const RenderCameraData* data);
    void                            GetRenderCameraData(HRenderContext render_context, HRenderCamera camera, RenderCameraData* data);
    void                            SetRenderCameraEnabled(HRenderContext render_context, HRenderCamera camera, bool value);
    void                            UpdateRenderCamera(HRenderContext render_context, HRenderCamera camera, const dmVMath::Point3* position, const dmVMath::Quat* rotation);
    float                           GetRenderCameraEffectiveAspectRatio(HRenderContext render_context, HRenderCamera camera);

    static inline dmGraphics::TextureWrap WrapFromDDF(dmRenderDDF::MaterialDesc::WrapMode wrap_mode)
    {
        switch(wrap_mode)
        {
            case dmRenderDDF::MaterialDesc::WRAP_MODE_REPEAT:          return dmGraphics::TEXTURE_WRAP_REPEAT;
            case dmRenderDDF::MaterialDesc::WRAP_MODE_MIRRORED_REPEAT: return dmGraphics::TEXTURE_WRAP_MIRRORED_REPEAT;
            case dmRenderDDF::MaterialDesc::WRAP_MODE_CLAMP_TO_EDGE:   return dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE;
            default:break;
        }
        return dmGraphics::TEXTURE_WRAP_REPEAT;
    }

    static inline dmGraphics::TextureFilter FilterMinFromDDF(dmRenderDDF::MaterialDesc::FilterModeMin min_filter)
    {
        switch(min_filter)
        {
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_NEAREST:                return dmGraphics::TEXTURE_FILTER_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_LINEAR:                 return dmGraphics::TEXTURE_FILTER_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_NEAREST_MIPMAP_NEAREST: return dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_NEAREST_MIPMAP_LINEAR:  return dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_LINEAR_MIPMAP_NEAREST:  return dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR:   return dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MIN_DEFAULT:                return dmGraphics::TEXTURE_FILTER_DEFAULT;
            default:break;
        }
        return dmGraphics::TEXTURE_FILTER_DEFAULT;
    }

    static inline dmGraphics::TextureFilter FilterMagFromDDF(dmRenderDDF::MaterialDesc::FilterModeMag mag_filter)
    {
        switch(mag_filter)
        {
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MAG_NEAREST: return dmGraphics::TEXTURE_FILTER_NEAREST;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MAG_LINEAR:  return dmGraphics::TEXTURE_FILTER_LINEAR;
            case dmRenderDDF::MaterialDesc::FILTER_MODE_MAG_DEFAULT: return dmGraphics::TEXTURE_FILTER_DEFAULT;
            default:break;
        }
        return dmGraphics::TEXTURE_FILTER_DEFAULT;
    }
}

#endif /* DM_RENDER_H */
