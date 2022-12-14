// Copyright 2020-2022 The Defold Foundation
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

    typedef struct RenderTargetSetup*       HRenderTargetSetup;
    typedef uint64_t                        HRenderType;
    typedef struct RenderScript*            HRenderScript;
    typedef struct RenderScriptInstance*    HRenderScriptInstance;
    typedef struct Predicate*               HPredicate;

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
        int32_t                                 m_Location;     // Vulkan encodes vs/fs location in the lower/upper bits
        uint16_t                                m_NumValues;

        Constant();
        Constant(dmhash_t name_hash, int32_t location);
    };

    struct MaterialConstant
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
        uint32_t                        m_CommandBufferSize;
        /// Max debug vertex count
        /// NOTE: This is per debug-type and not the total sum
        uint32_t                        m_MaxDebugVertexCount;
    };

    static const uint8_t RENDERLIST_INVALID_DISPATCH = 0xff;

    static const HRenderType INVALID_RENDER_TYPE_HANDLE = ~0ULL;

    HRenderContext NewRenderContext(dmGraphics::HContext graphics_context, const RenderContextParams& params);
    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context);

    dmScript::HContext GetScriptContext(HRenderContext render_context);

    void RenderListBegin(HRenderContext render_context);
    void RenderListEnd(HRenderContext render_context);

    void SetSystemFontMap(HRenderContext render_context, HFontMap font_map);

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context);

    const dmVMath::Matrix4& GetViewProjectionMatrix(HRenderContext render_context);
    void SetViewMatrix(HRenderContext render_context, const dmVMath::Matrix4& view);
    void SetProjectionMatrix(HRenderContext render_context, const dmVMath::Matrix4& projection);

    Result ClearRenderObjects(HRenderContext context);

    // Takes the contents of the render list, sorts by view and inserts all the objects in the
    // render list, unless they already are in place from a previous call.
    Result DrawRenderList(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer, const dmVMath::Matrix4* frustum_matrix);

    Result Draw(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer);
    Result DrawDebug3d(HRenderContext context, const dmVMath::Matrix4* frustum_matrix);
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
    void                    AddRenderScriptInstanceMaterial(HRenderScriptInstance render_script_instance, const char* material_name, dmRender::HMaterial material);
    void                    ClearRenderScriptInstanceMaterials(HRenderScriptInstance render_script_instance);
    RenderScriptResult      InitRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      DispatchRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      UpdateRenderScriptInstance(HRenderScriptInstance render_script_instance, float dt);
    void                    OnReloadRenderScriptInstance(HRenderScriptInstance render_script_instance);

    // Material
    HMaterial                       NewMaterial(dmRender::HRenderContext render_context, dmGraphics::HVertexProgram vertex_program, dmGraphics::HFragmentProgram fragment_program);
    void                            DeleteMaterial(dmRender::HRenderContext render_context, HMaterial material);
    void                            ApplyMaterialConstants(dmRender::HRenderContext render_context, HMaterial material, const RenderObject* ro);
    void                            ApplyMaterialSampler(dmRender::HRenderContext render_context, HMaterial material, uint32_t unit, dmGraphics::HTexture texture);

    dmGraphics::HProgram            GetMaterialProgram(HMaterial material);
    dmGraphics::HVertexProgram      GetMaterialVertexProgram(HMaterial material);
    dmGraphics::HFragmentProgram    GetMaterialFragmentProgram(HMaterial material);
    void                            SetMaterialProgramConstantType(HMaterial material, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool                            GetMaterialProgramConstant(HMaterial, dmhash_t name_hash, HConstant& out_value);

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
    int32_t                         GetMaterialConstantLocation(HMaterial material, dmhash_t name_hash);
    void                            SetMaterialSampler(HMaterial material, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy);
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

}

#endif /* DM_RENDER_H */
