// Copyright 2020-2024 The Defold Foundation
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

#include <dlib/hash.h>
#include <script/script.h>
#include <script/lua_source_ddf.h>
#include <graphics/graphics.h>
#include "render/material_ddf.h"

namespace dmRender
{
    extern const char* RENDER_SOCKET_NAME;

    static const uint32_t MAX_MATERIAL_TAG_COUNT = 32; // Max tag count per material

    static const dmhash_t VERTEX_STREAM_POSITION   = dmHashString64("position");
    static const dmhash_t VERTEX_STREAM_NORMAL     = dmHashString64("normal");
    static const dmhash_t VERTEX_STREAM_TANGENT    = dmHashString64("tangent");
    static const dmhash_t VERTEX_STREAM_COLOR      = dmHashString64("color");
    static const dmhash_t VERTEX_STREAM_TEXCOORD0  = dmHashString64("texcoord0");
    static const dmhash_t VERTEX_STREAM_TEXCOORD1  = dmHashString64("texcoord1");
    static const dmhash_t VERTEX_STREAM_PAGE_INDEX = dmHashString64("page_index");

    typedef struct RenderTargetSetup*       HRenderTargetSetup;
    typedef uint64_t                        HRenderType;
    typedef uint64_t                        HSampler;
    typedef struct RenderScript*            HRenderScript;
    typedef struct RenderScriptInstance*    HRenderScriptInstance;
    typedef struct Predicate*               HPredicate;
    typedef struct ComputeProgram*          HComputeProgram;
    typedef uintptr_t                       HRenderBuffer;
    typedef struct BufferedRenderBuffer*    HBufferedRenderBuffer;
    typedef HOpaqueHandle                   HRenderCamera;

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
        uint16_t                                m_NumValues;

        Constant();
        Constant(dmhash_t name_hash, dmGraphics::HUniformLocation location);
    };

    struct RenderConstant
    {
        HConstant           m_Constant;
        dmhash_t            m_ElementIds[4];
    };

    struct RenderContextParams
    {
        RenderContextParams();

        dmScript::HContext              m_ScriptContext;
        HFontMap                        m_SystemFontMap;
        void*                           m_VertexShaderDesc;
        void*                           m_FragmentShaderDesc;
        uint32_t                        m_MaxRenderTypes;
        uint32_t                        m_MaxInstances;
        uint32_t                        m_MaxRenderTargets;
        uint32_t                        m_VertexShaderDescSize;
        uint32_t                        m_FragmentShaderDescSize;
        uint32_t                        m_MaxCharacters;
        uint32_t                        m_MaxBatches;
        uint32_t                        m_CommandBufferSize;
        /// Max debug vertex count
        /// NOTE: This is per debug-type and not the total sum
        uint32_t                        m_MaxDebugVertexCount;
    };

    static const uint8_t RENDERLIST_INVALID_DISPATCH    = 0xff;
    static const uint16_t INVALID_RENDER_CAMERA         = 0xffff;
    static const HRenderType INVALID_RENDER_TYPE_HANDLE = ~0ULL;

    HRenderContext NewRenderContext(dmGraphics::HContext graphics_context, const RenderContextParams& params);
    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context);

    dmScript::HContext GetScriptContext(HRenderContext render_context);

    void RenderListBegin(HRenderContext render_context);
    void RenderListEnd(HRenderContext render_context);

    void SetSystemFontMap(HRenderContext render_context, HFontMap font_map);

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context);

    const dmVMath::Matrix4& GetViewProjectionMatrix(HRenderContext render_context);
    const dmVMath::Matrix4& GetViewMatrix(HRenderContext render_context);
    void SetViewMatrix(HRenderContext render_context, const dmVMath::Matrix4& view);
    void SetProjectionMatrix(HRenderContext render_context, const dmVMath::Matrix4& projection);

    Result ClearRenderObjects(HRenderContext context);

    // Takes the contents of the render list, sorts by view and inserts all the objects in the
    // render list, unless they already are in place from a previous call.
    Result DrawRenderList(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer, const FrustumOptions* frustum_options);

    Result Draw(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer);
    Result DrawDebug3d(HRenderContext context, const FrustumOptions* frustum_options);
    Result DrawDebug2d(HRenderContext context);

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
    HMaterial                       NewMaterial(dmRender::HRenderContext render_context, dmGraphics::HVertexProgram vertex_program, dmGraphics::HFragmentProgram fragment_program);
    void                            DeleteMaterial(dmRender::HRenderContext render_context, HMaterial material);
    HSampler                        GetMaterialSampler(HMaterial material, uint32_t unit);
    dmhash_t                        GetMaterialSamplerNameHash(HMaterial material, uint32_t unit);
    uint32_t                        GetMaterialSamplerUnit(HMaterial material, dmhash_t name_hash);
    void                            ApplyMaterialConstants(dmRender::HRenderContext render_context, HMaterial material, const RenderObject* ro);
    void                            ApplyMaterialSampler(dmRender::HRenderContext render_context, HMaterial material, HSampler sampler, uint8_t value_index, dmGraphics::HTexture texture);

    dmGraphics::HProgram            GetMaterialProgram(HMaterial material);
    dmGraphics::HVertexProgram      GetMaterialVertexProgram(HMaterial material);
    dmGraphics::HFragmentProgram    GetMaterialFragmentProgram(HMaterial material);
    void                            SetMaterialProgramConstantType(HMaterial material, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool                            GetMaterialProgramConstant(HMaterial, dmhash_t name_hash, HConstant& out_value);

    struct MaterialProgramAttributeInfo
    {
        dmhash_t                           m_AttributeNameHash;
        const dmGraphics::VertexAttribute* m_Attribute;
        const uint8_t*                     m_ValuePtr;
        dmhash_t                           m_ElementIds[4];
        uint32_t                           m_ElementIndex;
    };

    dmGraphics::HVertexDeclaration  GetVertexDeclaration(HMaterial material);
    bool                            GetMaterialProgramAttributeInfo(HMaterial material, dmhash_t name_hash, MaterialProgramAttributeInfo& info);
    void                            GetMaterialProgramAttributes(HMaterial material, const dmGraphics::VertexAttribute** attributes, uint32_t* attribute_count);
    void                            GetMaterialProgramAttributeValues(HMaterial material, uint32_t index, const uint8_t** value_ptr, uint32_t* num_values);
    void                            SetMaterialProgramAttributes(HMaterial material, const dmGraphics::VertexAttribute* attributes, uint32_t attributes_count);

    // Compute
    HComputeProgram                 NewComputeProgram(HRenderContext render_context, dmGraphics::HComputeProgram shader);
    void                            DeleteComputeProgram(dmRender::HRenderContext render_context, HComputeProgram program);
    HRenderContext                  GetProgramRenderContext(HComputeProgram program);
    dmGraphics::HComputeProgram     GetComputeProgramShader(HComputeProgram program);
    dmGraphics::HProgram            GetComputeProgram(HComputeProgram program);
    uint64_t                        GetProgramUserData(HComputeProgram program);
    void                            SetProgramUserData(HComputeProgram program, uint64_t user_data);

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
    bool                            SetMaterialSampler(HMaterial material, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy);
    HRenderContext                  GetMaterialRenderContext(HMaterial material);
    void                            SetMaterialVertexSpace(HMaterial material, dmRenderDDF::MaterialDesc::VertexSpace vertex_space);

    uint64_t                        GetMaterialUserData1(HMaterial material);
    void                            SetMaterialUserData1(HMaterial material, uint64_t user_data);
    uint64_t                        GetMaterialUserData2(HMaterial material);
    void                            SetMaterialUserData2(HMaterial material, uint64_t user_data);

    void                            ApplyNamedConstantBuffer(dmRender::HRenderContext render_context, HMaterial material, HNamedConstantBuffer buffer);

    void                            ClearMaterialTags(HMaterial material);
    void                            SetMaterialTags(HMaterial material, uint32_t tag_count, const dmhash_t* tags);

    HPredicate                      NewPredicate();
    void                            DeletePredicate(HPredicate predicate);
    Result                          AddPredicateTag(HPredicate predicate, dmhash_t tag);

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
    void                            TrimBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);
    void                            RewindBuffer(HRenderContext render_context, HBufferedRenderBuffer buffer);

    struct RenderCameraData
    {
        dmVMath::Vector4 m_Viewport;
        float            m_AspectRatio;
        float            m_Fov;
        float            m_NearZ;
        float            m_FarZ;
        float            m_OrthographicZoom;
        uint8_t          m_AutoAspectRatio        : 1;
        uint8_t          m_OrthographicProjection : 1;
    };

    /** Render cameras
     * TODO: Description
     */
    HRenderCamera                   NewRenderCamera(HRenderContext context);
    void                            DeleteRenderCamera(HRenderContext context, HRenderCamera camera);
    void                            SetRenderCameraURL(HRenderContext render_context, HRenderCamera camera, const dmMessage::URL& camera_url);
    void                            SetRenderCameraData(HRenderContext render_context, HRenderCamera camera, RenderCameraData data);
    void                            SetRenderCameraMatrices(HRenderContext render_context, HRenderCamera camera, const dmVMath::Matrix4 view, const dmVMath::Matrix4 projection);
    RenderCameraData                GetRenderCameraData(HRenderContext render_context, HRenderCamera camera);
    void                            SetRenderCameraMainCamera(HRenderContext render_context, HRenderCamera camera);
}

#endif /* DM_RENDER_H */
