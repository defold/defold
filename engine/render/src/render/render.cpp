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

#include <assert.h>
#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/profile.h>
#include <dlib/math.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/intersection.h>

#include <ddf/ddf.h>

#include "render_private.h"
#include "render_script.h"
#include "debug_renderer.h"
#include "font/font_renderer.h"

DM_PROPERTY_GROUP(rmtp_Render, "Renderer", 0);

namespace dmRender
{
    using namespace dmVMath;

    static void UpdateRenderContextMatrices(HRenderContext render_context, const Matrix4& view, const Matrix4& projection)
    {
        static const Matrix4 adjusted_ndc_matrix = []()
        {
            Matrix4 ndc_matrix = Matrix4::identity();
            ndc_matrix.setElem(2, 2, 0.5f);
            ndc_matrix.setElem(3, 2, 0.5f);
            return ndc_matrix;
        }();

        render_context->m_View = view;
        render_context->m_Projection = projection;
        render_context->m_ViewProj = projection * view;
        render_context->m_ProjectionAdjustedNDC = adjusted_ndc_matrix * projection;
        render_context->m_ViewProjAdjustedNDC = adjusted_ndc_matrix * render_context->m_ViewProj;
    }

    const char* RENDER_SOCKET_NAME = "@render";

    // Declararions are in render.h
    const dmhash_t VERTEX_STREAM_POSITION             = dmHashString64("position");
    const dmhash_t VERTEX_STREAM_NORMAL               = dmHashString64("normal");
    const dmhash_t VERTEX_STREAM_TANGENT              = dmHashString64("tangent");
    const dmhash_t VERTEX_STREAM_COLOR                = dmHashString64("color");
    const dmhash_t VERTEX_STREAM_TEXCOORD0            = dmHashString64("texcoord0");
    const dmhash_t VERTEX_STREAM_TEXCOORD1            = dmHashString64("texcoord1");
    const dmhash_t VERTEX_STREAM_PAGE_INDEX           = dmHashString64("page_index");
    const dmhash_t VERTEX_STREAM_WORLD_MATRIX         = dmHashString64("mtx_world");
    const dmhash_t VERTEX_STREAM_NORMAL_MATRIX        = dmHashString64("mtx_normal");
    const dmhash_t VERTEX_STREAM_BONE_WEIGHTS         = dmHashString64("bone_weights");
    const dmhash_t VERTEX_STREAM_BONE_INDICES         = dmHashString64("bone_indices");
    const dmhash_t VERTEX_STREAM_ANIMATION_DATA       = dmHashString64("animation_data");
    const dmhash_t VERTEX_STREAM_TEXTURE_TRANSFORM_2D = dmHashString64("texture_transform_2d");
    const dmhash_t VERTEX_STREAM_MORPH_TARGET_WEIGHTS = dmHashString64("morph_targets_weights");
    const dmhash_t SAMPLER_POSE_MATRIX_CACHE          = dmHashString64("pose_matrix_cache");
    const dmhash_t SAMPLER_MORPH_TARGETS              = dmHashString64("morph_targets");
    const dmhash_t CONSTANT_MORPH_TARGETS_WEIGHTS     = dmHashString64("morph_targets_weights");

    StencilTestParams::StencilTestParams() {
        Init();
    }

    void StencilTestParams::Init() {
        m_Front.m_Func = dmGraphics::COMPARE_FUNC_ALWAYS;
        m_Front.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
        m_Front.m_OpDPFail = dmGraphics::STENCIL_OP_KEEP;
        m_Front.m_OpDPPass = dmGraphics::STENCIL_OP_KEEP;
        m_Back.m_Func = dmGraphics::COMPARE_FUNC_ALWAYS;
        m_Back.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
        m_Back.m_OpDPFail = dmGraphics::STENCIL_OP_KEEP;
        m_Back.m_OpDPPass = dmGraphics::STENCIL_OP_KEEP;
        m_Ref = 0;
        m_RefMask = 0xff;
        m_BufferMask = 0xff;
        m_ColorBufferMask = 0xf;
        m_ClearBuffer = 0;
        m_SeparateFaceStates = 0;
    }

    RenderObject::RenderObject()
    {
        Init();
    }

    void RenderObject::Init()
    {
        // See case 2264 why this method was added
        memset(this, 0, sizeof(RenderObject));
        m_WorldTransform = Matrix4::identity();
        m_TextureTransform = Matrix4::identity();
    }

    RenderContextParams::RenderContextParams()
    : m_ScriptContext(0x0)
    , m_SystemFontMap(0)
    , m_ShaderProgramDesc(0x0)
    , m_MaxRenderTypes(0)
    , m_MaxInstances(0)
    , m_MaxRenderTargets(0)
    , m_ShaderProgramDescSize(0)
    , m_MaxCharacters(0)
    , m_CommandBufferSize(1024)
    , m_MaxDebugVertexCount(0)
    {

    }

    RenderScriptContext::RenderScriptContext()
    : m_LuaState(0)
    , m_CommandBufferSize(0)
    {

    }

    HRenderContext NewRenderContext(dmGraphics::HContext graphics_context, const RenderContextParams& params)
    {
        RenderContext* context = new RenderContext;

        context->m_RenderObjects.SetCapacity(params.m_MaxInstances);
        context->m_RenderObjects.SetSize(0);

        context->m_GraphicsContext = graphics_context;
        context->m_ConstantBufferCloneCursor = 0;

        context->m_SystemFontMap = params.m_SystemFontMap;

        context->m_Material = 0;
        context->m_CurrentRenderCamera = 0;

        UpdateRenderContextMatrices(context, Matrix4::identity(), Matrix4::identity());
        context->m_Time = 0.0f;
        context->m_Dt = 0.0f;

        context->m_ScriptContext = params.m_ScriptContext;
        InitializeRenderScriptContext(context->m_RenderScriptContext, graphics_context, params.m_ScriptContext, params.m_CommandBufferSize);
        InitializeRenderScriptCameraContext(context, params.m_ScriptContext);
        context->m_ScriptWorld = dmScript::NewScriptWorld(context->m_ScriptContext);
        context->m_CallbackInfo = 0x0;

        context->m_DebugRenderer.m_RenderContext = 0;
        if (params.m_ShaderProgramDesc != 0 && params.m_ShaderProgramDescSize != 0) {
            InitializeDebugRenderer(context, params.m_MaxDebugVertexCount, params.m_ShaderProgramDesc, params.m_ShaderProgramDescSize);
        }

        InitializeTextContext(context, params.m_MaxCharacters, params.m_MaxBatches);

        context->m_OutOfResources = 0;

        context->m_StencilBufferCleared = 0;

        context->m_MultiBufferingRequired = 0;

        context->m_IsRenderPaused = 0;

        // TODO: This should be a "context property" or something similar.
        dmGraphics::AdapterFamily installed_adapter_family = dmGraphics::GetInstalledAdapterFamily();
        if (installed_adapter_family == dmGraphics::ADAPTER_FAMILY_VULKAN ||
            installed_adapter_family == dmGraphics::ADAPTER_FAMILY_WEBGPU ||
            installed_adapter_family == dmGraphics::ADAPTER_FAMILY_DIRECTX ||
            installed_adapter_family == dmGraphics::ADAPTER_FAMILY_METAL ||
            installed_adapter_family == dmGraphics::ADAPTER_FAMILY_VENDOR)
        {
            context->m_MultiBufferingRequired = 1;
        }

        context->m_UseAdjustedNDC = installed_adapter_family == dmGraphics::ADAPTER_FAMILY_VULKAN ||
                                    installed_adapter_family == dmGraphics::ADAPTER_FAMILY_WEBGPU ||
                                    installed_adapter_family == dmGraphics::ADAPTER_FAMILY_DIRECTX ||
                                    installed_adapter_family == dmGraphics::ADAPTER_FAMILY_METAL ||
                                    installed_adapter_family == dmGraphics::ADAPTER_FAMILY_VENDOR;

        context->m_RenderListDispatch.SetCapacity(255);
        context->m_RenderItemDispatches.SetCapacity(8);
        context->m_RenderItemDispatches.Push(0); // Reserve the invalid handle.
        context->m_SubmittedRenderItemDispatches.SetCapacity(255);
        context->m_FirstFreeRenderItem = ~0u;
        context->m_TransientRenderListDirty = false;

        SetupContextEventCallback(context, &OnContextEvent);

        context->m_LightUniformBuffer = 0;
        SetLightBufferCount(context, 0);

        dmMessage::Result r = dmMessage::NewSocket(RENDER_SOCKET_NAME, &context->m_Socket);
        assert(r == dmMessage::RESULT_OK);
        return context;
    }

    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context)
    {
        if (render_context == 0x0) return RESULT_INVALID_CONTEXT;

        if (render_context->m_CallbackInfo != 0x0)
        {
            dmScript::DestroyCallback(render_context->m_CallbackInfo);
            render_context->m_CallbackInfo = 0x0;
        }
        FinalizeRenderScriptContext(render_context->m_RenderScriptContext, script_context);
        FinalizeRenderScriptCameraContext(render_context);
        dmScript::DeleteScriptWorld(render_context->m_ScriptWorld);
        FinalizeDebugRenderer(render_context);
        FinalizeTextContext(render_context);
        FinalizeLightData(render_context);
        for (uint32_t i = 0; i < render_context->m_ConstantBufferClones.Size(); ++i)
            DeleteNamedConstantBuffer(render_context->m_ConstantBufferClones[i]);
        for (uint32_t i = 0; i < render_context->m_RenderItemDispatches.Size(); ++i)
            delete render_context->m_RenderItemDispatches[i];
        dmMessage::DeleteSocket(render_context->m_Socket);
        delete render_context;

        return RESULT_OK;
    }

    dmScript::HContext GetScriptContext(HRenderContext render_context)
    {
        return render_context->m_ScriptContext;
    }

    void RenderListBegin(HRenderContext render_context)
    {
        render_context->m_ConstantBufferCloneCursor = 0;
        render_context->m_RenderList.SetSize(render_context->m_PersistentRenderItems.Size());
        render_context->m_TransientRenderList.SetSize(0);
        render_context->m_TransientRenderListIndices.SetSize(0);
        render_context->m_TransientRenderListDirty = false;
        for (uint32_t i = 0; i < render_context->m_SubmittedRenderItemDispatches.Size(); ++i)
        {
            RenderItemDispatch* dispatch = render_context->m_RenderItemDispatches[render_context->m_SubmittedRenderItemDispatches[i]];
            if (dispatch)
                dispatch->m_FrameDispatch = RENDERLIST_INVALID_DISPATCH;
        }
        render_context->m_SubmittedRenderItemDispatches.SetSize(0);
        render_context->m_RenderListDispatch.SetSize(0);
        render_context->m_RenderListRanges.SetSize(0);
    }

    HNamedConstantBuffer PushRenderConstants(HRenderContext render_context, HNamedConstantBuffer constant_buffer)
    {
        if (!constant_buffer)
            return 0;

        uint32_t clone_index = render_context->m_ConstantBufferCloneCursor++;
        if (clone_index == render_context->m_ConstantBufferClones.Size())
        {
            if (render_context->m_ConstantBufferClones.Full())
                render_context->m_ConstantBufferClones.OffsetCapacity(16);
            render_context->m_ConstantBufferClones.Push(NewNamedConstantBuffer());
        }

        HNamedConstantBuffer clone = render_context->m_ConstantBufferClones[clone_index];
        CopyNamedConstantBuffer(clone, constant_buffer);
        return clone;
    }

    // Allocate a buffer (from the array) with room for 'entries' entries.
    //
    // NOTE: Pointer might go invalid after a consecutive call to RenderListAlloc if reallocation
    //       of backing buffer happens.
    RenderListEntry* RenderListAlloc(HRenderContext render_context, uint32_t entries)
    {
        dmLogOnceWarning("RenderListAlloc() is deprecated. Use NewRenderItem() and the render item setters.");
        dmArray<RenderListEntry>& render_list = render_context->m_TransientRenderList;

        if (render_list.Remaining() < entries)
        {
            const uint32_t needed = entries - render_list.Remaining();
            render_list.OffsetCapacity(dmMath::Max<uint32_t>(256, needed));
        }

        uint32_t size = render_list.Size();
        render_list.SetSize(size + entries);

        // If we push new items after the last frustum culling, we need to reevaluate them.
        RenderListEntry* start = render_list.Begin() + size;
        if (entries)
            memset(start, 0, entries * sizeof(RenderListEntry));

        return start;
    }

    // Submit a range of entries (pointers must be from a range allocated by RenderListAlloc, and not between two alloc calls).
    void RenderListSubmit(HRenderContext render_context, RenderListEntry *begin, RenderListEntry *end)
    {
        dmLogOnceWarning("RenderListSubmit() is deprecated. Use Enable() and SubmitRenderListDispatch().");
        // Insert the used up indices into the sort buffer.
        dmArray<RenderListEntry>& render_list = render_context->m_TransientRenderList;
        assert(end <= render_list.End());

        // If we didn't use all entries, let's put them back into the list
        if (end < render_list.End())
        {
            uint32_t list_size = end - render_list.Begin();
            render_list.SetSize(list_size);
        }

        if (end == begin) {
            return;
        }

        // Transform pointers back to indices.
        RenderListEntry*   base = render_list.Begin();
        dmArray<uint32_t>& transient_indices = render_context->m_TransientRenderListIndices;
        if (transient_indices.Remaining() < (uint32_t)(end - begin))
            transient_indices.SetCapacity(render_list.Capacity());
        uint32_t* insert = transient_indices.End();

        for (RenderListEntry* i=begin;i!=end;i++)
            *insert++ = i - base;

        transient_indices.SetSize(transient_indices.Size() + (end - begin));
        render_context->m_TransientRenderListDirty = true;

        // invalidate the ranges if this is a call to the debug rendering (happening in the middle of the frame)
        render_context->m_RenderListRanges.SetSize(0);
    }

    struct RenderListSorter
    {
        bool operator()(uint32_t a, uint32_t b) const
        {
            const RenderListSortValue& u = values[a];
            const RenderListSortValue& v = values[b];
            return u.m_SortKey < v.m_SortKey;
        }
        RenderListSortValue* values;
    };

    void RenderListEnd(HRenderContext render_context)
    {
        // Unflushed leftovers are assumed to be the debug rendering
        // and we give them render orders statically here
        FlushTexts(render_context, RENDER_ORDER_AFTER_WORLD, true);
    }

    void SetSystemFontMap(HRenderContext render_context, HFontMap font_map)
    {
        render_context->m_SystemFontMap = font_map;
    }

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context)
    {
        return render_context->m_GraphicsContext;
    }

    const Matrix4& GetViewProjectionMatrix(HRenderContext render_context)
    {
        return render_context->m_ViewProj;
    }

    const Matrix4& GetViewMatrix(HRenderContext render_context)
    {
        return render_context->m_View;
    }

    HMaterial GetContextMaterial(HRenderContext render_context)
    {
        return render_context->m_Material;
    }

    dmVMath::Matrix4 GetNormalMatrix(HRenderContext render_context, const dmVMath::Matrix4& world_matrix)
    {
        // normalT = transp(inv(view * world))
        Matrix4 normalT = render_context->m_View * world_matrix;
        // The world transform might include non-uniform scaling, which breaks the orthogonality of the combined model-view transform
        // It is always affine however
        normalT = affineInverse(normalT);
        normalT = transpose(normalT);
        return normalT;
    }

    void SetViewMatrix(HRenderContext render_context, const Matrix4& view)
    {
        UpdateRenderContextMatrices(render_context, view, render_context->m_Projection);
    }

    void SetProjectionMatrix(HRenderContext render_context, const Matrix4& projection)
    {
        UpdateRenderContextMatrices(render_context, render_context->m_View, projection);
    }

    void BeginFrame(HRenderContext render_context, float time, float dt)
    {
        render_context->m_Time = time;
        render_context->m_Dt = dt;

        // A frame may contain several render lists, so reset light submissions at
        // the frame boundary and preserve them across all lists in the frame.
        if (!render_context->m_LightBufferSubmitted.Empty())
        {
            memset(render_context->m_LightBufferSubmitted.Begin(), 0, render_context->m_LightBufferSubmitted.Size());
        }
        InvalidateLightBuffer(render_context);
    }

    Result AddToRender(HRenderContext context, RenderObject* ro)
    {
        if (context == 0x0) return RESULT_INVALID_CONTEXT;
        if (context->m_RenderObjects.Full())
        {
            if (!context->m_OutOfResources)
            {
                dmLogWarning("Max number of draw calls reached (%u), some objects will not be rendered. Increase the capacity with graphics.max_draw_calls", context->m_RenderObjects.Capacity());
                context->m_OutOfResources = 1;
            }
            return RESULT_OUT_OF_RESOURCES;
        }
        context->m_RenderObjects.Push(ro);

        return RESULT_OK;
    }

    Result ClearRenderObjects(HRenderContext context)
    {
        context->m_RenderObjects.SetSize(0);
        ClearDebugRenderObjects(context);

        // Should probably be moved and/or refactored, see case 2261
        // (Cannot reset the text buffer until all render objects are dispatched)
        // Also see FontRenderListDispatch in font_renderer.cpp
        context->m_TextContext.m_Frame += 1;
        context->m_TextContext.m_TextBuffer.SetSize(0);
        ClearTextEntries(context);

        return RESULT_OK;
    }

    // This function will compare the values in ps_orig and ps_now and reset the render state that is different between them
    // It is expected that the first parameter is the "default" state, i.e the values from that pipeline will be used
    static void ResetRenderStateIfChanged(dmGraphics::HContext graphics_context, dmGraphics::PipelineState ps_orig, dmGraphics::PipelineState ps_now)
    {
        #define HAS_CHANGED(name) (ps_now.name != ps_orig.name)

        if (HAS_CHANGED(m_BlendSrcFactor) || HAS_CHANGED(m_BlendDstFactor) ||
            HAS_CHANGED(m_BlendSrcFactorAlpha) || HAS_CHANGED(m_BlendDstFactorAlpha))
        {
            dmGraphics::SetBlendFuncSeparate(graphics_context,
                (dmGraphics::BlendFactor) ps_orig.m_BlendSrcFactor,
                (dmGraphics::BlendFactor) ps_orig.m_BlendDstFactor,
                (dmGraphics::BlendFactor) ps_orig.m_BlendSrcFactorAlpha,
                (dmGraphics::BlendFactor) ps_orig.m_BlendDstFactorAlpha);
        }

        if (HAS_CHANGED(m_BlendEquationColor) || HAS_CHANGED(m_BlendEquationAlpha))
        {
            dmGraphics::SetBlendEquationSeparate(graphics_context,
                (dmGraphics::BlendEquation) ps_orig.m_BlendEquationColor,
                (dmGraphics::BlendEquation) ps_orig.m_BlendEquationAlpha);
        }

        if (HAS_CHANGED(m_FaceWinding))
        {
            dmGraphics::SetFaceWinding(graphics_context, (dmGraphics::FaceWinding) ps_orig.m_FaceWinding);
        }

        if (HAS_CHANGED(m_StencilWriteMask))
        {
            dmGraphics::SetStencilMask(graphics_context, ps_orig.m_StencilWriteMask);
        }

        if (HAS_CHANGED(m_WriteColorMask))
        {
            dmGraphics::SetColorMask(graphics_context,
                ps_orig.m_WriteColorMask & (1<<3),
                ps_orig.m_WriteColorMask & (1<<2),
                ps_orig.m_WriteColorMask & (1<<1),
                ps_orig.m_WriteColorMask & (1<<0));
        }

        if (HAS_CHANGED(m_StencilFrontTestFunc) || HAS_CHANGED(m_StencilReference) || HAS_CHANGED(m_StencilCompareMask))
        {
            dmGraphics::SetStencilFuncSeparate(graphics_context, dmGraphics::FACE_TYPE_FRONT,
                (dmGraphics::CompareFunc) ps_orig.m_StencilFrontTestFunc, ps_orig.m_StencilReference, ps_orig.m_StencilCompareMask);
        }

        if (HAS_CHANGED(m_StencilBackTestFunc) || HAS_CHANGED(m_StencilReference) || HAS_CHANGED(m_StencilCompareMask))
        {
            dmGraphics::SetStencilFuncSeparate(graphics_context, dmGraphics::FACE_TYPE_BACK,
                (dmGraphics::CompareFunc) ps_orig.m_StencilBackTestFunc, ps_orig.m_StencilReference, ps_orig.m_StencilCompareMask);
        }

        if (HAS_CHANGED(m_StencilFrontOpFail) || HAS_CHANGED(m_StencilFrontOpDepthFail) || HAS_CHANGED(m_StencilFrontOpPass))
        {
            dmGraphics::SetStencilOpSeparate(graphics_context, dmGraphics::FACE_TYPE_FRONT,
                (dmGraphics::StencilOp) ps_orig.m_StencilFrontOpFail,
                (dmGraphics::StencilOp) ps_orig.m_StencilFrontOpDepthFail,
                (dmGraphics::StencilOp) ps_orig.m_StencilFrontOpPass);
        }

        if (HAS_CHANGED(m_StencilBackOpFail) || HAS_CHANGED(m_StencilBackOpDepthFail) || HAS_CHANGED(m_StencilBackOpPass))
        {
            dmGraphics::SetStencilOpSeparate(graphics_context, dmGraphics::FACE_TYPE_BACK,
                (dmGraphics::StencilOp) ps_orig.m_StencilBackOpFail,
                (dmGraphics::StencilOp) ps_orig.m_StencilBackOpDepthFail,
                (dmGraphics::StencilOp) ps_orig.m_StencilBackOpPass);
        }

        #undef HAS_CHANGED
    }

    static void ApplyRenderState(HRenderContext render_context, dmGraphics::HContext graphics_context, dmGraphics::PipelineState ps_default, const RenderObject* ro)
    {
        dmGraphics::PipelineState ps_now = ps_default;

        if (ro->m_SetBlendFactors)
        {
            ps_now.m_BlendSrcFactor      = ro->m_SourceBlendFactor;
            ps_now.m_BlendDstFactor      = ro->m_DestinationBlendFactor;
            ps_now.m_BlendSrcFactorAlpha = ro->m_SourceBlendFactor;
            ps_now.m_BlendDstFactorAlpha = ro->m_DestinationBlendFactor;
        }

        if (ro->m_SetFaceWinding)
        {
            ps_now.m_FaceWinding = ro->m_FaceWinding;
        }

        if (ro->m_SetStencilTest)
        {
            const StencilTestParams& stp = ro->m_StencilTestParams;
            if (stp.m_ClearBuffer)
            {
                // Note: We don't need to save any of these values in the pipeline
                if (render_context->m_StencilBufferCleared)
                {
                    // render.clear command will set context m_StencilBufferCleared to 1 if stencil clear flag is set.
                    // We skip clear and reset context m_StencilBufferCleared to 0, indicating that the stencil is no longer cleared.
                    // Concecutive calls with m_ClearBuffer option will result in a clear until render.clear is called with stencil clear flag set.
                    render_context->m_StencilBufferCleared = 0;
                }
                else
                {
                    dmGraphics::SetStencilMask(graphics_context, 0xff);
                    dmGraphics::Clear(graphics_context, dmGraphics::BUFFER_TYPE_STENCIL_BIT, 0, 0, 0, 0, 1.0f, 0);
                }
            }

            ps_now.m_WriteColorMask          = stp.m_ColorBufferMask;
            ps_now.m_StencilWriteMask        = stp.m_BufferMask;
            ps_now.m_StencilReference        = stp.m_Ref;
            ps_now.m_StencilCompareMask      = stp.m_RefMask;
            ps_now.m_StencilFrontTestFunc    = stp.m_Front.m_Func;
            ps_now.m_StencilFrontOpFail      = stp.m_Front.m_OpSFail;
            ps_now.m_StencilFrontOpDepthFail = stp.m_Front.m_OpDPFail;
            ps_now.m_StencilFrontOpPass      = stp.m_Front.m_OpDPPass;

            if (stp.m_SeparateFaceStates)
            {
                ps_now.m_StencilBackTestFunc    = stp.m_Back.m_Func;
                ps_now.m_StencilBackOpFail      = stp.m_Back.m_OpSFail;
                ps_now.m_StencilBackOpDepthFail = stp.m_Back.m_OpDPFail;
                ps_now.m_StencilBackOpPass      = stp.m_Back.m_OpDPPass;
            }
            else
            {
                ps_now.m_StencilBackTestFunc    = stp.m_Front.m_Func;
                ps_now.m_StencilBackOpFail      = stp.m_Front.m_OpSFail;
                ps_now.m_StencilBackOpDepthFail = stp.m_Front.m_OpDPFail;
                ps_now.m_StencilBackOpPass      = stp.m_Front.m_OpDPPass;
            }
        }

        ResetRenderStateIfChanged(graphics_context, ps_now, ps_default);
    }

    // For unit testing only
    bool FindTagListRange(RenderListRange* ranges, uint32_t num_ranges, uint32_t tag_list_key, RenderListRange& range)
    {
        for( uint32_t i = 0; i < num_ranges; ++i)
        {
            if(ranges[i].m_TagListKey == tag_list_key)
            {
                range = ranges[i];
                return true;
            }
        }
        return false;
    }

    // Compute new sort values for everything that matches tag_mask
    static void MakeSortBuffer(HRenderContext context, uint32_t tag_count, dmhash_t* tags, SortOrder sort_order)
    {
        DM_PROFILE("MakeSortBuffer");

        const uint32_t required_capacity = context->m_RenderList.Capacity();
        // SetCapacity does early out if they are the same, so just call anyway.
        context->m_RenderListSortBuffer.SetCapacity(required_capacity);
        context->m_RenderListSortBuffer.SetSize(0);
        context->m_RenderListSortValues.SetCapacity(required_capacity);
        context->m_RenderListSortValues.SetSize(context->m_RenderList.Size());

        RenderListSortValue* sort_values = context->m_RenderListSortValues.Begin();
        RenderListEntry* entries = context->m_RenderList.Begin();

        const Matrix4& transform = context->m_ViewProj;

        float minZW = FLT_MAX;
        float maxZW = -FLT_MAX;

        RenderListRange* ranges = context->m_RenderListRanges.Begin();
        uint32_t num_ranges = context->m_RenderListRanges.Size();
        uint32_t num_persistent = context->m_PersistentRenderItems.Size();
        for( uint32_t r = 0; r < num_ranges; ++r)
        {
            RenderListRange& range = ranges[r];

            MaterialTagList taglist;
            dmRender::GetMaterialTagList(context, range.m_TagListKey, &taglist);

            range.m_Skip = 0;
            if (tag_count > 0 && !dmRender::MatchMaterialTags(taglist.m_Count, taglist.m_Tags, tag_count, tags))
            {
                range.m_Skip = 1;
                continue;
            }

            // Write z values...
            int num_visibility_skipped = 0;
            for (uint32_t s = range.m_SpanStart; s < range.m_SpanStart + range.m_SpanCount; ++s)
            {
                const RenderListSpan&     span = context->m_RenderListSpans[s];
                const RenderItemDispatch* registration = span.m_Dispatch != 0 ? context->m_RenderItemDispatches[span.m_Dispatch] : 0;
                uint32_t                  index_offset = registration ? 0 : num_persistent;
                for (uint32_t i = 0; i < span.m_Count; ++i)
                {
                    uint32_t         idx = span.m_Indices[i] + index_offset;
                    RenderListEntry* entry = &entries[idx];
                    if (registration)
                        entry->m_Dispatch = registration->m_FrameDispatch;
                    if (entry->m_Visibility == dmRender::VISIBILITY_NONE ||
                        entry->m_Dispatch == RENDERLIST_INVALID_DISPATCH)
                    {
                        num_visibility_skipped++;
                        continue;
                    }

                    RenderOrder major_order = (RenderOrder)entry->m_MajorOrder;
                    sort_values[idx].m_MajorOrder = major_order;

                    if (major_order != RENDER_ORDER_WORLD)
                    {
                        continue; // Could perhaps break here, if we also sorted on the major order (cost more when I tested it /MAWE)
                    }

                    const Vector4 res = transform * entry->m_WorldPosition;
                    const float zw = res.getZ() / res.getW();
                    sort_values[idx].m_ZW = zw;
                    if (zw < minZW)
                        minZW = zw;
                    if (zw > maxZW)
                        maxZW = zw;
                }
            }

            if (num_visibility_skipped == range.m_Count)
            {
                range.m_Skip = 1;
            }
        }

        // ... and compute range
        float rc = 0;
        if (maxZW > minZW)
            rc = 1.0f / (maxZW - minZW);

        const uint32_t ORDER_SCALE_MASK = 0xfffff0;
        const uint32_t ORDER_BASE_FRONT = 0x000008;
        const uint32_t ftb  = (sort_order == SORT_FRONT_TO_BACK);
        const float    base = (float)(ORDER_BASE_FRONT + ((uint32_t)!ftb) * ORDER_SCALE_MASK);
        const float    sign = ftb * 2.0f - 1.0f;
        const float    order_multiplier = sign * ORDER_SCALE_MASK * rc;

        for( uint32_t i = 0; i < num_ranges; ++i)
        {
            const RenderListRange& range = ranges[i];
            if (range.m_Skip)
                continue;

            for (uint32_t s = range.m_SpanStart; s < range.m_SpanStart + range.m_SpanCount; ++s)
            {
                const RenderListSpan& span = context->m_RenderListSpans[s];
                uint32_t              index_offset = span.m_Dispatch != 0 ? 0 : num_persistent;
                for (uint32_t i = 0; i < span.m_Count; ++i)
                {
                    uint32_t         idx = span.m_Indices[i] + index_offset;
                    RenderListEntry* entry = &entries[idx];

                    if (entry->m_Visibility == dmRender::VISIBILITY_NONE ||
                        entry->m_Dispatch == RENDERLIST_INVALID_DISPATCH)
                    {
                        continue;
                    }

                    if (sort_values[idx].m_MajorOrder == RENDER_ORDER_WORLD)
                    {
                        const float z = sort_values[idx].m_ZW;
                        sort_values[idx].m_Order = (uint32_t)(base + order_multiplier * (z - minZW));
                    }
                    else
                    {
                        // use the integer value provided.
                        sort_values[idx].m_Order = entry->m_Order;
                    }
                    sort_values[idx].m_MinorOrder = entry->m_MinorOrder;
                    sort_values[idx].m_BatchKey = entry->m_BatchKey & 0x00ffffff;
                    sort_values[idx].m_Dispatch = entry->m_Dispatch;
                    context->m_RenderListSortBuffer.Push(idx);
                }
            }
        }
    }

    static void PushRenderListRange(dmArray<RenderListRange>& ranges, const RenderListRange& range)
    {
        if (ranges.Full())
            ranges.OffsetCapacity(16);
        ranges.Push(range);
    }

    void FindRenderListRanges(uint32_t* first, size_t offset, size_t size, RenderListEntry* entries, FindRangeComparator& comp, void* ctx, RangeCallback callback )
    {
        if (size == 0)
            return;

        size_t half = size >> 1;
        uint32_t* low = first + offset;
        uint32_t* high = low + size;
        uint32_t* middle = low + half;
        uint32_t val = entries[*middle].m_TagListKey;

        low = std::lower_bound(low, middle, *middle, comp);
        high = std::upper_bound(middle, high, *middle, comp);

        callback(ctx, val, low - first, high - low);

        uint32_t* rangefirst = first + offset;
        FindRenderListRanges(first, offset, low - rangefirst, entries, comp, ctx, callback);
        FindRenderListRanges(first, high - first, size - (high - rangefirst), entries, comp, ctx, callback);
    }

    template <typename T, typename Compare>
    static void StableSortRenderListIndices(T* indices, uint32_t count, const Compare& compare)
    {
        // Already sorted lists need neither stable_sort's scratch allocation nor data movement.
        for (uint32_t i = 1; i < count; ++i)
        {
            if (compare(indices[i], indices[i - 1]))
            {
                std::stable_sort(indices, indices + count, compare);
                return;
            }
        }
    }

    struct RenderListUserDataSorter
    {
        bool operator()(uint32_t a, uint32_t b) const
        {
            return m_Entries[a].m_UserData < m_Entries[b].m_UserData;
        }
        const RenderListEntry* m_Entries;
    };

    void SortRenderListBatchByUserData(const RenderListDispatchParams& params)
    {
        RenderListUserDataSorter sort;
        sort.m_Entries = params.m_Buf;
        const RenderListSortValue* values = params.m_Context->m_RenderListSortValues.Begin();
        uint32_t* begin = params.m_Begin;
        while (begin != params.m_End)
        {
            uint32_t* end = begin + 1;
            while (end != params.m_End && values[*begin].m_SortKey == values[*end].m_SortKey)
                ++end;
            StableSortRenderListIndices(begin, end - begin, sort);
            begin = end;
        }
    }

    static void CollectMergedRenderListRanges(const RenderListRange* ranges, uint32_t offset, uint32_t count, dmArray<RenderListRange>& result)
    {
        if (count == 0)
            return;

        // Preserve FindRenderListRanges' midpoint-first traversal, including
        // stable draw order when different tag lists produce equal sort keys.
        const RenderListRange& last = ranges[offset + count - 1];
        uint32_t               midpoint = ranges[offset].m_Start + (last.m_Start + last.m_Count - ranges[offset].m_Start) / 2;
        uint32_t               low = offset;
        uint32_t               high = offset + count;
        while (low < high)
        {
            uint32_t middle = low + (high - low) / 2;
            if (ranges[middle].m_Start + ranges[middle].m_Count <= midpoint)
                low = middle + 1;
            else
                high = middle;
        }
        PushRenderListRange(result, ranges[low]);
        CollectMergedRenderListRanges(ranges, offset, low - offset, result);
        CollectMergedRenderListRanges(ranges, low + 1, offset + count - low - 1, result);
    }

    struct RenderItemEntrySorter
    {
        bool operator()(uint32_t a, uint32_t b) const
        {
            uint32_t ka = m_Entries[a].m_TagListKey;
            uint32_t kb = m_Entries[b].m_TagListKey;
            return ka != kb ? ka < kb : a < b;
        }
        RenderListEntry* m_Entries;
    };

    struct RenderListSpanSorter
    {
        bool operator()(const RenderListSpan& a, const RenderListSpan& b) const
        {
            return a.m_TagListKey < b.m_TagListKey;
        }
    };

    static uint32_t LowerBoundRenderItemRange(const dmArray<RenderItemRange>& ranges, uint32_t key)
    {
        uint32_t low = 0;
        uint32_t high = ranges.Size();
        while (low < high)
        {
            uint32_t middle = low + (high - low) / 2;
            if (ranges[middle].m_TagListKey < key)
                low = middle + 1;
            else
                high = middle;
        }
        return low;
    }

    static void ApplyRenderItemCacheChanges(HRenderContext context, HRenderItemDispatch handle, RenderItemDispatch& dispatch)
    {
        dmArray<uint32_t>&        indices = dispatch.m_SortIndices;
        dmArray<RenderItemRange>& ranges = dispatch.m_Ranges;
        bool                      insert[MAX_RENDER_ITEM_CACHE_CHANGES];
        // Search by the cached range key and numeric slot: current entry tags
        // may already differ from the keys represented by this cache.
        for (uint32_t i = 0; i < dispatch.m_NumChanges; ++i)
        {
            const RenderItemCacheChange& change = dispatch.m_Changes[i];
            const PersistentRenderItem&  item = context->m_PersistentRenderItems[change.m_Index];
            bool                         enabled = item.m_Alive && item.m_Enabled && item.m_Dispatch == handle;
            insert[i] = enabled;
            uint32_t r = LowerBoundRenderItemRange(ranges, change.m_TagListKey);
            if (r == ranges.Size() || ranges[r].m_TagListKey != change.m_TagListKey)
                continue;
            RenderItemRange& range = ranges[r];
            uint32_t         end = range.m_Start + range.m_Count;
            uint32_t         position = std::lower_bound(indices.Begin() + range.m_Start, indices.Begin() + end, change.m_Index) - indices.Begin();
            if (position == end || indices[position] != change.m_Index)
                continue;
            if (enabled && context->m_RenderList[change.m_Index].m_TagListKey == change.m_TagListKey)
            {
                insert[i] = false; // Repeated edits left the cached membership unchanged.
                continue;
            }
            memmove(indices.Begin() + position, indices.Begin() + position + 1, (indices.Size() - position - 1) * sizeof(uint32_t));
            indices.SetSize(indices.Size() - 1);
            --range.m_Count;
            for (uint32_t next = r + 1; next < ranges.Size(); ++next)
                --ranges[next].m_Start;
            if (range.m_Count == 0)
            {
                memmove(ranges.Begin() + r, ranges.Begin() + r + 1, (ranges.Size() - r - 1) * sizeof(RenderItemRange));
                ranges.SetSize(ranges.Size() - 1);
            }
        }

        // Remove all old memberships first so capacity is bounded by the final
        // enabled count, including when slots moved between worlds.
        if (indices.Capacity() < dispatch.m_EnabledItems.Size())
            indices.SetCapacity(dispatch.m_EnabledItems.Capacity());
        for (uint32_t i = 0; i < dispatch.m_NumChanges; ++i)
        {
            if (!insert[i])
                continue;
            uint32_t index = dispatch.m_Changes[i].m_Index;
            uint32_t key = context->m_RenderList[index].m_TagListKey;
            uint32_t r = LowerBoundRenderItemRange(ranges, key);
            if (r == ranges.Size() || ranges[r].m_TagListKey != key)
            {
                uint32_t start = r == ranges.Size() ? indices.Size() : ranges[r].m_Start;
                if (ranges.Full())
                    ranges.OffsetCapacity(8);
                ranges.SetSize(ranges.Size() + 1);
                memmove(ranges.Begin() + r + 1, ranges.Begin() + r, (ranges.Size() - r - 1) * sizeof(RenderItemRange));
                RenderItemRange range = { key, start, 0 };
                ranges[r] = range;
            }
            RenderItemRange& range = ranges[r];
            uint32_t         position = range.m_Start;
            if (range.m_Count)
                position = std::lower_bound(indices.Begin() + range.m_Start, indices.Begin() + range.m_Start + range.m_Count, index) - indices.Begin();
            indices.SetSize(indices.Size() + 1);
            memmove(indices.Begin() + position + 1, indices.Begin() + position, (indices.Size() - position - 1) * sizeof(uint32_t));
            indices[position] = index;
            ++range.m_Count;
            for (uint32_t next = r + 1; next < ranges.Size(); ++next)
                ++ranges[next].m_Start;
        }
        dispatch.m_NumChanges = 0;
    }

    static void SortRenderList(HRenderContext context)
    {
        DM_PROFILE("SortRenderList");

        dmArray<RenderListSpan>& spans = context->m_RenderListSpans;
        spans.SetSize(0);
        RenderItemEntrySorter retained_sort;
        retained_sort.m_Entries = context->m_RenderList.Begin();
        for (uint32_t d = 0; d < context->m_SubmittedRenderItemDispatches.Size(); ++d)
        {
            HRenderItemDispatch handle = context->m_SubmittedRenderItemDispatches[d];
            RenderItemDispatch& dispatch = *context->m_RenderItemDispatches[handle];
            dmArray<uint32_t>&  indices = dispatch.m_SortIndices;
            if (dispatch.m_RebuildCache)
            {
                indices.SetSize(0);
                if (indices.Capacity() < dispatch.m_EnabledItems.Size())
                    indices.SetCapacity(dispatch.m_EnabledItems.Capacity());
                indices.PushArray(dispatch.m_EnabledItems.Begin(), dispatch.m_EnabledItems.Size());
                // The dense list uses erase-swap. Restore slot order for equal
                // tags so disable/re-enable does not change stable draw order.
                StableSortRenderListIndices(indices.Begin(), indices.Size(), retained_sort);
                dispatch.m_Ranges.SetSize(0);
                for (uint32_t start = 0; start < indices.Size();)
                {
                    uint32_t end = start + 1;
                    uint32_t key = context->m_RenderList[indices[start]].m_TagListKey;
                    while (end < indices.Size() && context->m_RenderList[indices[end]].m_TagListKey == key)
                        ++end;
                    RenderItemRange range = { key, start, end - start };
                    if (dispatch.m_Ranges.Full())
                        dispatch.m_Ranges.OffsetCapacity(8);
                    dispatch.m_Ranges.Push(range);
                    start = end;
                }
                dispatch.m_RebuildCache = false;
            }
            else if (dispatch.m_NumChanges)
                ApplyRenderItemCacheChanges(context, handle, dispatch);
            for (uint32_t r = 0; r < dispatch.m_Ranges.Size(); ++r)
            {
                const RenderItemRange& range = dispatch.m_Ranges[r];
                RenderListSpan         span = { indices.Begin() + range.m_Start, range.m_Count, range.m_TagListKey, handle };
                if (spans.Full())
                    spans.OffsetCapacity(16);
                spans.Push(span);
            }
        }

        // Only span metadata is merged each frame. Idle worlds are not visited,
        // and retained index arrays stay in their owning world's cache.
        dmArray<uint32_t>&    transient = context->m_TransientRenderListIndices;
        RenderListEntrySorter sort;
        sort.m_Base = context->m_TransientRenderList.Begin();
        StableSortRenderListIndices(transient.Begin(), transient.Size(), sort);
        for (uint32_t start = 0; start < transient.Size();)
        {
            uint32_t end = start + 1;
            uint32_t key = context->m_TransientRenderList[transient[start]].m_TagListKey;
            while (end < transient.Size() && context->m_TransientRenderList[transient[end]].m_TagListKey == key)
                ++end;
            RenderListSpan span = { transient.Begin() + start, end - start, key, 0 };
            if (spans.Full())
                spans.OffsetCapacity(16);
            spans.Push(span);
            start = end;
        }
        StableSortRenderListIndices(spans.Begin(), spans.Size(), RenderListSpanSorter());

        dmArray<RenderListRange>& ranges = context->m_RenderListSortedRanges;
        ranges.SetSize(0);
        uint32_t offset = 0;
        for (uint32_t start = 0; start < spans.Size();)
        {
            uint32_t        end = start;
            RenderListRange range = {};
            range.m_TagListKey = spans[start].m_TagListKey;
            range.m_Start = offset;
            range.m_SpanStart = start;
            while (end < spans.Size() && spans[end].m_TagListKey == range.m_TagListKey)
                range.m_Count += spans[end++].m_Count;
            range.m_SpanCount = end - start;
            offset += range.m_Count;
            PushRenderListRange(ranges, range);
            start = end;
        }
        CollectMergedRenderListRanges(ranges.Begin(), 0, ranges.Size(), context->m_RenderListRanges);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    static bool SetVisibility(uint32_t count, RenderListEntry* entries, Visibility visibility, uint32_t frustum_hash)
    {
        bool changed = false;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (entries[i].m_Visibility != visibility || entries[i].m_FrustumHash != frustum_hash)
            {
                entries[i].m_Visibility = visibility;
                entries[i].m_FrustumHash = frustum_hash;
                changed = true;
            }
        }
        return changed;
    }

    static bool TestBoundingVolume(const dmIntersection::Frustum& frustum, const PersistentRenderItem& item)
    {
        const float* bounds = item.m_Bounds;
        if (item.m_BoundingVolumeType == BOUNDING_VOLUME_SPHERE)
            return dmIntersection::TestFrustumSphereSq(frustum, Point3(bounds[0], bounds[1], bounds[2]), bounds[3]);

        Vector3 min(bounds[0], bounds[1], bounds[2]);
        Vector3 max(bounds[3], bounds[4], bounds[5]);
        if (item.m_BoundingVolumeType == BOUNDING_VOLUME_AABB)
            return dmIntersection::TestFrustumOBB(frustum, Matrix4::identity(), min, max);
        return dmIntersection::TestFrustumOBB(frustum, item.m_WorldTransform, min, max);
    }

    static bool CullRenderListEntries(HRenderContext context, RenderListEntry* entries, uint32_t count, const dmIntersection::Frustum& frustum, uint32_t frustum_hash)
    {
        if (entries->m_Dispatch == RENDERLIST_INVALID_DISPATCH)
            return false;
        const RenderListDispatch& dispatch = context->m_RenderListDispatch[entries->m_Dispatch];

        if (!dispatch.m_VisibilityFn)
            return SetVisibility(count, entries, VISIBILITY_FULL, FRUSTUM_HASH_UNINITIALIZED);

        RenderListVisibilityParams params;
        params.m_Frustum = &frustum;
        params.m_UserData = dispatch.m_UserData;
        bool     changed = false;
        uint32_t i = 0;
        while (i < count)
        {
            // Skip entries whose visibility is already cached for this frustum.
            while (i < count && entries[i].m_FrustumHash == frustum_hash)
                ++i;
            uint32_t start = i;
            while (i < count && entries[i].m_FrustumHash != frustum_hash)
            {
                entries[i].m_FrustumHash = frustum_hash;
                ++i;
            }
            params.m_Entries = entries + start;
            params.m_NumEntries = i - start;
            if (params.m_NumEntries)
            {
                dispatch.m_VisibilityFn(params);
                changed = true;
            }
        }
        return changed;
    }

    static void FrustumCulling(HRenderContext context, const dmIntersection::Frustum& frustum, uint32_t frustum_hash)
    {
        DM_PROFILE("FrustumCulling");
        RenderListEntry* entries = context->m_RenderList.Begin();
        for (uint32_t d = 0; d < context->m_SubmittedRenderItemDispatches.Size(); ++d)
        {
            const RenderItemDispatch& dispatch = *context->m_RenderItemDispatches[context->m_SubmittedRenderItemDispatches[d]];
            const dmArray<uint32_t>&  persistent = dispatch.m_SortIndices;
            for (uint32_t i = 0; i < persistent.Size(); ++i)
            {
                uint32_t         index = persistent[i];
                RenderListEntry& entry = entries[index];
                entry.m_Dispatch = dispatch.m_FrameDispatch;
                if (entry.m_FrustumHash == frustum_hash)
                    continue;
                const PersistentRenderItem& item = context->m_PersistentRenderItems[index];
                if (item.m_BoundingVolumeType != BOUNDING_VOLUME_NONE)
                {
                    entry.m_Visibility = TestBoundingVolume(frustum, item) ? VISIBILITY_FULL : VISIBILITY_NONE;
                    entry.m_FrustumHash = frustum_hash;
                }
                else
                {
                    uint32_t count = 1;
                    while (i + 1 < persistent.Size() && persistent[i + 1] == index + count &&
                           context->m_PersistentRenderItems[index + count].m_BoundingVolumeType == BOUNDING_VOLUME_NONE)
                    {
                        entries[index + count].m_Dispatch = dispatch.m_FrameDispatch;
                        ++i;
                        ++count;
                    }
                    CullRenderListEntries(context, &entry, count, frustum, frustum_hash);
                }
            }
        }

        RenderListEntry* transient = context->m_TransientRenderList.Begin();
        uint32_t         num_transient = context->m_TransientRenderList.Size();
        uint32_t         i = 0;
        while (i < num_transient)
        {
            uint32_t start = i++;
            while (i < num_transient && transient[i].m_Dispatch == transient[start].m_Dispatch)
                ++i;
            context->m_TransientRenderListDirty |= CullRenderListEntries(context, transient + start, i - start, frustum, frustum_hash);
        }
    }

    static void ResetRenderListVisibility(HRenderContext context)
    {
        for (uint32_t d = 0; d < context->m_SubmittedRenderItemDispatches.Size(); ++d)
        {
            const RenderItemDispatch& dispatch = *context->m_RenderItemDispatches[context->m_SubmittedRenderItemDispatches[d]];
            for (uint32_t i = 0; i < dispatch.m_SortIndices.Size(); ++i)
            {
                RenderListEntry& entry = context->m_RenderList[dispatch.m_SortIndices[i]];
                entry.m_Dispatch = dispatch.m_FrameDispatch;
                SetVisibility(1, &entry, VISIBILITY_FULL, FRUSTUM_HASH_UNINITIALIZED);
            }
        }
        context->m_TransientRenderListDirty |= SetVisibility(context->m_TransientRenderList.Size(), context->m_TransientRenderList.Begin(), VISIBILITY_FULL, FRUSTUM_HASH_UNINITIALIZED);
    }

    static void PrepareRenderList(HRenderContext context)
    {
        if (!context->m_TransientRenderListDirty)
            return;

        uint32_t num_persistent = context->m_PersistentRenderItems.Size();
        uint32_t num_transient = context->m_TransientRenderList.Size();
        uint32_t size = num_persistent + num_transient;
        if (context->m_RenderList.Capacity() < size)
            context->m_RenderList.OffsetCapacity(dmMath::Max(256u, size - context->m_RenderList.Capacity()));
        context->m_RenderList.SetSize(size);
        if (num_transient)
            memcpy(context->m_RenderList.Begin() + num_persistent, context->m_TransientRenderList.Begin(), num_transient * sizeof(RenderListEntry));
        context->m_TransientRenderListDirty = false;
    }

    void SetTextureBindingByHash(dmRender::HRenderContext render_context, dmhash_t sampler_hash, dmGraphics::HTexture texture)
    {
        uint32_t num_bindings = render_context->m_TextureBindTable.Size();
        int32_t first_free_index = -1;

        // First pass
        // Check if the the sampler is already bound to this texture, if so we reuse or unbind the current binding
        for (int i = 0; i < num_bindings; ++i)
        {
            if (render_context->m_TextureBindTable[i].m_Samplerhash == sampler_hash)
            {
                if (texture == 0)
                {
                    render_context->m_TextureBindTable[i].m_Samplerhash = 0;
                }
                render_context->m_TextureBindTable[i].m_Texture = texture;
                return;
            }
            // Store the free index for later
            else if (render_context->m_TextureBindTable[i].m_Texture == 0 && first_free_index == -1)
            {
                first_free_index = i;
            }
        }

        // If we are unassigning the sampler, but it wasn't found we can exit here.
        if (texture == 0)
        {
            return;
        }

        // Take the first free index we found
        if (first_free_index != -1)
        {
            render_context->m_TextureBindTable[first_free_index].m_Texture     = texture;
            render_context->m_TextureBindTable[first_free_index].m_Samplerhash = sampler_hash;
            return;
        }

        // Otherwise, we add a new binding to the end of the list
        if (render_context->m_TextureBindTable.Full())
        {
            render_context->m_TextureBindTable.OffsetCapacity(4);
        }

        TextureBinding new_binding;
        new_binding.m_Samplerhash = sampler_hash;
        new_binding.m_Texture     = texture;
        render_context->m_TextureBindTable.Push(new_binding);
    }

    void SetTextureBindingByUnit(HRenderContext render_context, uint32_t unit, dmGraphics::HTexture texture)
    {
        if (unit >= render_context->m_TextureBindTable.Size())
        {
            render_context->m_TextureBindTable.SetCapacity(unit + 1);

            // Make sure new data area is zeroed out
            uint32_t fill_index_start = render_context->m_TextureBindTable.Size();
            uint32_t fill_size = render_context->m_TextureBindTable.Remaining() * sizeof(TextureBinding);

            render_context->m_TextureBindTable.SetSize(render_context->m_TextureBindTable.Capacity());
            memset(&render_context->m_TextureBindTable[fill_index_start], 0, fill_size);
        }

        render_context->m_TextureBindTable[unit].m_Texture     = texture;
        render_context->m_TextureBindTable[unit].m_Samplerhash = 0;
    }

    static void TrimTextureBindingTable(HRenderContext render_context)
    {
        uint32_t num_bindings    = render_context->m_TextureBindTable.Size();
        uint32_t last_zero_index = -1;

        for (uint32_t i = 0; i < num_bindings; ++i)
        {
            if (render_context->m_TextureBindTable[i].m_Texture == 0)
            {
                if (last_zero_index == -1)
                {
                    last_zero_index = i;
                }
            }
            else
            {
                last_zero_index = -1;
            }
        }

        // Trim the iteration space
        if (last_zero_index != -1)
        {
            render_context->m_TextureBindTable.SetSize(last_zero_index);
        }
    }

    static void GetRenderContextTextures(HRenderContext render_context, const dmArray<Sampler>& samplers, dmGraphics::HTexture* textures)
    {
        uint32_t num_bindings = render_context->m_TextureBindTable.Size();
        for (uint32_t i = 0; i < num_bindings; ++i)
        {
            uint32_t sampler_index       = i;
            dmGraphics::HTexture texture = textures[i];

            // If a texture has been bound by a sampler hash, the material must have a valid sampler for it
            if (render_context->m_TextureBindTable[i].m_Samplerhash)
            {
                int32_t hash_sampler_index = GetProgramSamplerIndex(samplers, render_context->m_TextureBindTable[i].m_Samplerhash);
                if (hash_sampler_index >= 0)
                {
                    sampler_index = hash_sampler_index;
                    texture       = render_context->m_TextureBindTable[i].m_Texture;
                }
                // The sampler doesn't exist, so we ignore it.
                else continue;
            }
            else if (texture == 0)
            {
                texture = render_context->m_TextureBindTable[i].m_Texture;
            }

            if (sampler_index >= 0 && sampler_index < RenderObject::MAX_TEXTURE_COUNT)
            {
                textures[sampler_index] = texture;
            }
            else
            {
                dmLogOnceWarning("Unable to bind texture to unit %d, max %d texture units are supported.", i, RenderObject::MAX_TEXTURE_COUNT);
            }
        }
    }

    Result DrawRenderList(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer, const FrustumOptions* frustum_options, SortOrder sort_order)
    {
        DM_PROFILE("DrawRenderList");

        // This will add new entries for the most recent debug draw render objects.
        // The internal dispatch functions knows to only actually use the latest ones.
        // The sort order is also one below the Texts flush which is only also debug stuff.
        FlushDebug(context, 0xfffffe);

        FrustumPlanes frustum_num_planes       = dmRender::FRUSTUM_PLANES_SIDES;
        const dmVMath::Matrix4* frustum_matrix = 0;

        if (frustum_options)
        {
            frustum_num_planes = frustum_options->m_NumPlanes;
            frustum_matrix     = &frustum_options->m_Matrix;
        }

        if (context->m_CurrentRenderCamera != 0)
        {
            RenderCamera* camera = context->m_RenderCameras.Get(context->m_CurrentRenderCamera);
            if (camera)
            {
                // In case the camera has changed since last update
                if (camera->m_Dirty)
                {
                    UpdateRenderCamera(context, context->m_CurrentRenderCamera, &camera->m_LastPosition, &camera->m_LastRotation);
                }

                UpdateRenderContextMatrices(context, camera->m_View, camera->m_Projection);

                if (context->m_CurrentRenderCameraUseFrustum)
                {
                    frustum_matrix = &camera->m_ViewProjection;
                }
            }
        }

        // Cleared once per frame
        if (context->m_RenderListRanges.Empty())
        {
            SortRenderList(context);
        }

        uint32_t frustum_hash = 0;

        if (frustum_matrix)
        {
            uint8_t frustum_key_buffer[sizeof(Matrix4) + sizeof(FrustumPlanes)];
            memcpy(frustum_key_buffer, frustum_matrix, sizeof(Matrix4));
            memcpy(frustum_key_buffer + sizeof(Matrix4), &frustum_num_planes, sizeof(frustum_num_planes));
            frustum_hash = dmHashBuffer32(frustum_key_buffer, (uint32_t)sizeof(frustum_key_buffer));

            dmIntersection::Frustum frustum;
            dmIntersection::CreateFrustumFromMatrix(*frustum_matrix, true, (int) frustum_num_planes, frustum);
            FrustumCulling(context, frustum, frustum_hash);
        }
        else
        {
            // Reset the visibility
            ResetRenderListVisibility(context);
        }

        PrepareRenderList(context);
        SortOrder effective_sort_order = (sort_order == SORT_UNSPECIFIED) ? SORT_BACK_TO_FRONT : sort_order;

        MakeSortBuffer(context, predicate?predicate->m_TagCount:0, predicate?predicate->m_Tags:0, effective_sort_order);

        if (context->m_RenderListSortBuffer.Empty())
            return RESULT_OK;

        {
            if (effective_sort_order != SORT_NONE)
            {
                DM_PROFILE("DrawRenderList_SORT");
                RenderListSorter sort;
                sort.values = context->m_RenderListSortValues.Begin();
                StableSortRenderListIndices(context->m_RenderListSortBuffer.Begin(), context->m_RenderListSortBuffer.Size(), sort);
            }
        }

        // Construct render objects
        context->m_RenderObjects.SetSize(0);

        RenderListDispatchParams params;
        memset(&params, 0x00, sizeof(params));
        params.m_Operation = RENDER_LIST_OPERATION_BEGIN;
        params.m_Context = context;

        {
            DM_PROFILE("Dispatch_Begin");

            // All get begin operation first
            for (uint32_t i=0;i!=context->m_RenderListDispatch.Size();i++)
            {
                const RenderListDispatch& d = context->m_RenderListDispatch[i];
                params.m_UserData = d.m_UserData;
                d.m_DispatchFn(params);
            }

        }

        params.m_Operation = RENDER_LIST_OPERATION_BATCH;
        params.m_Buf = context->m_RenderList.Begin();

        // Make batches for matching dispatch, batch key & minor order
        RenderListEntry *base = context->m_RenderList.Begin();
        uint32_t *last = context->m_RenderListSortBuffer.Begin();
        uint32_t count = context->m_RenderListSortBuffer.Size();

        {
            DM_PROFILE("Dispatch_Batch");

            for (uint32_t i=1;i<=count;i++)
            {
                uint32_t *idx = context->m_RenderListSortBuffer.Begin() + i;
                const RenderListEntry *last_entry = &base[*last];
                // continue batch on match, or dispatch
                if (i < count)
                {
                    const RenderListEntry *current_entry = &base[*idx];
                    if (last_entry->m_Dispatch == current_entry->m_Dispatch && last_entry->m_BatchKey == current_entry->m_BatchKey && last_entry->m_MinorOrder == current_entry->m_MinorOrder)
                        continue;
                }
                if (last_entry->m_Dispatch != RENDERLIST_INVALID_DISPATCH)
                {
                    assert(last_entry->m_Dispatch < context->m_RenderListDispatch.Size());
                    const RenderListDispatch* d = &context->m_RenderListDispatch[last_entry->m_Dispatch];
                    params.m_UserData = d->m_UserData;
                    params.m_Begin = last;
                    params.m_End = idx;
                    d->m_DispatchFn(params);
                }

                last = idx;
            }

        }

        params.m_Operation = RENDER_LIST_OPERATION_END;
        params.m_Begin = 0;
        params.m_End = 0;
        params.m_Buf = 0;

        {
            DM_PROFILE("Dispatch_End");

            for (uint32_t i=0;i!=context->m_RenderListDispatch.Size();i++)
            {
                const RenderListDispatch& d = context->m_RenderListDispatch[i];
                params.m_UserData = d.m_UserData;
                d.m_DispatchFn(params);
            }
        }

        return Draw(context, predicate, constant_buffer);
    }

    void DispatchCompute(HRenderContext render_context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z, HNamedConstantBuffer constant_buffer)
    {
        HComputeProgram compute_program = render_context->m_ComputeProgram;

        if (compute_program == 0)
        {
            return;
        }

        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::HTexture render_context_textures[RenderObject::MAX_TEXTURE_COUNT] = {};

        dmGraphics::EnableProgram(context, compute_program->m_Program);

        ApplyComputeProgramConstants(render_context, compute_program);

        if (constant_buffer)
        {
            ApplyNamedConstantBuffer(render_context, compute_program, constant_buffer);
        }

        GetRenderContextTextures(render_context, compute_program->m_Samplers, render_context_textures);

        uint8_t next_texture_unit = 0;
        for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (render_context_textures[i])
            {
                dmGraphics::HTexture texture = render_context_textures[i];

                uint32_t num_texture_handles = dmGraphics::GetNumTextureHandles(context, texture);
                for (int sub_handle = 0; sub_handle < num_texture_handles; ++sub_handle)
                {
                    dmGraphics::EnableTexture(context, next_texture_unit, sub_handle, texture);

                    HSampler sampler = GetProgramSampler(compute_program->m_Samplers, next_texture_unit);
                    ApplyProgramSampler(render_context, sampler, next_texture_unit, texture);

                    next_texture_unit++;
                }
            }
        }

        ApplyComputeProgramLightBuffers(render_context, compute_program);

        dmGraphics::DispatchCompute(context, group_count_x, group_count_y, group_count_z);

        next_texture_unit = 0;
        for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (render_context_textures[i])
            {
                dmGraphics::HTexture texture = render_context_textures[i];
                uint32_t num_texture_handles = dmGraphics::GetNumTextureHandles(context, texture);
                for (int sub_handle = 0; sub_handle < num_texture_handles; ++sub_handle)
                {
                    dmGraphics::DisableTexture(context, next_texture_unit, texture);
                    next_texture_unit++;
                }
            }
        }

        dmGraphics::DisableProgram(context);
        TrimTextureBindingTable(render_context);
    }

    // NOTE: Currently only used externally in 1 test (fontview.cpp)
    // TODO: Replace that occurrance with DrawRenderList
    Result Draw(HRenderContext render_context, HPredicate predicate, HNamedConstantBuffer constant_buffer)
    {
        if (render_context == 0x0)
        {
            return RESULT_INVALID_CONTEXT;
        }

        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::HTexture render_context_textures[RenderObject::MAX_TEXTURE_COUNT] = {};

        HMaterial material         = render_context->m_Material;
        HMaterial context_material = render_context->m_Material;

        if(context_material)
        {
            dmGraphics::EnableProgram(context, GetMaterialProgram(context_material));
            GetRenderContextTextures(render_context, context_material->m_Samplers, render_context_textures);
        }

        dmGraphics::PipelineState ps_orig = dmGraphics::GetPipelineState(context);

        for (uint32_t i = 0; i < render_context->m_RenderObjects.Size(); ++i)
        {
            RenderObject* ro = render_context->m_RenderObjects[i];
            if (ro->m_VertexCount == 0)
                continue;

            MaterialTagList taglist;
            uint32_t taglistkey = dmRender::GetMaterialTagListKey(ro->m_Material);
            dmRender::GetMaterialTagList(render_context, taglistkey, &taglist);

            if (predicate && !dmRender::MatchMaterialTags(taglist.m_Count, taglist.m_Tags, predicate->m_TagCount, predicate->m_Tags))
            {
                continue;
            }

            if (!context_material)
            {
                if(material != ro->m_Material)
                {
                    material = ro->m_Material;
                    dmGraphics::EnableProgram(context, GetMaterialProgram(material));

                    // Reset the override texture binding array. The new material may have a different
                    // resource layout than the current material.
                    memset(render_context_textures, 0, sizeof(render_context_textures));
                    GetRenderContextTextures(render_context, material->m_Samplers, render_context_textures);
                }
            }

            ApplyMaterialConstants(render_context, material, ro);

            if (ro->m_ConstantBuffer) // from components/scripts
                ApplyNamedConstantBuffer(render_context, material, ro->m_ConstantBuffer);

            if (constant_buffer) // from render script
                ApplyNamedConstantBuffer(render_context, material, constant_buffer);

            ApplyRenderState(render_context, render_context->m_GraphicsContext, dmGraphics::GetPipelineState(context), ro);

            uint8_t next_texture_unit = 0;
            for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
            {
                dmGraphics::HTexture texture = ro->m_Textures[i];
                if (render_context_textures[i])
                {
                    texture = render_context_textures[i];
                }

                if (texture)
                {
                    uint32_t num_texture_handles = dmGraphics::GetNumTextureHandles(context, texture);
                    for (int sub_handle = 0; sub_handle < num_texture_handles; ++sub_handle)
                    {
                        HSampler sampler = GetProgramSampler(material->m_Samplers, next_texture_unit);
                        dmGraphics::EnableTexture(context, next_texture_unit, sub_handle, texture);
                        ApplyProgramSampler(render_context, sampler, next_texture_unit, texture);

                        next_texture_unit++;
                    }
                }
            }

            ApplyMaterialProgramLightBuffers(render_context, material);

            dmGraphics::HProgram material_program = GetMaterialProgram(material);

            for (int i = 0; i < RenderObject::MAX_VERTEX_BUFFER_COUNT; ++i)
            {
                if (ro->m_VertexBuffers[i])
                {
                    dmGraphics::EnableVertexBuffer(context, ro->m_VertexBuffers[i], i);
                }
                if (ro->m_VertexDeclarations[i])
                {
                    dmGraphics::EnableVertexDeclaration(context, ro->m_VertexDeclarations[i], i, ro->m_VertexBufferOffsets[i], material_program);
                }
            }

            if (ro->m_IndexBuffer)
                dmGraphics::DrawElements(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount, ro->m_IndexType, ro->m_IndexBuffer, ro->m_InstanceCount);
            else
                dmGraphics::Draw(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount, ro->m_InstanceCount);

            for (int i = 0; i < RenderObject::MAX_VERTEX_BUFFER_COUNT; ++i)
            {
                if (ro->m_VertexBuffers[i])
                {
                    dmGraphics::DisableVertexBuffer(context, ro->m_VertexBuffers[i]);
                }

                if (ro->m_VertexDeclarations[i])
                {
                    dmGraphics::DisableVertexDeclaration(context, ro->m_VertexDeclarations[i]);
                }
            }

            next_texture_unit = 0;
            for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
            {
                dmGraphics::HTexture texture = ro->m_Textures[i];
                if (render_context_textures[i])
                    texture = render_context_textures[i];
                if (texture)
                {
                    for (int sub_handle = 0; sub_handle < dmGraphics::GetNumTextureHandles(context, texture); ++sub_handle)
                    {
                        dmGraphics::DisableTexture(context, next_texture_unit, texture);
                        next_texture_unit++;
                    }
                }
            }
        }

        ResetRenderStateIfChanged(context, ps_orig, dmGraphics::GetPipelineState(context));

        TrimTextureBindingTable(render_context);

        return RESULT_OK;
    }

    Result DrawDebug3d(HRenderContext context, const FrustumOptions* frustum_options)
    {
        if (!context->m_DebugRenderer.m_RenderContext) {
            return RESULT_INVALID_CONTEXT;
        }
        return DrawRenderList(context, &context->m_DebugRenderer.m_3dPredicate, 0, frustum_options, SORT_BACK_TO_FRONT);
    }

    HPredicate NewPredicate()
    {
        HPredicate predicate = new Predicate();
        return predicate;
    }

    void DeletePredicate(HPredicate predicate)
    {
        delete predicate;
    }

    Result AddPredicateTag(HPredicate predicate, dmhash_t tag)
    {
        if (predicate->m_TagCount == dmRender::Predicate::MAX_TAG_COUNT)
        {
            return RESULT_OUT_OF_RESOURCES;
        }
        predicate->m_Tags[predicate->m_TagCount++] = tag;
        std::sort(predicate->m_Tags, predicate->m_Tags+predicate->m_TagCount);
        return RESULT_OK;
    }

    void SetupContextEventCallback(void* context, ContextEventCallback callback)
    {
        PlatformSetupContextEventCallback(context, callback);
    }

    void OnContextEvent(void* context, RenderContextEvent event_type)
    {
        RenderContext* render_context = (RenderContext*)context;
        if (event_type == dmRender::CONTEXT_LOST)
        {
            SetRenderPause(render_context, 1u);
            dmGraphics::InvalidateGraphicsHandles(render_context->m_GraphicsContext);
        }
        if (render_context->m_CallbackInfo != 0x0)
        {
            dmScript::LuaCallbackInfo* cbk = render_context->m_CallbackInfo;
            if (!dmScript::IsCallbackValid(cbk))
            {
                return;
            }
            lua_State* L = dmScript::GetCallbackLuaContext(cbk);
            DM_LUA_STACK_CHECK(L, 0);

            if (!dmScript::SetupCallback(cbk))
            {
                return;
            }
            lua_pushinteger(L, event_type);
            int ret = dmScript::PCall(L, 2, 0);
            (void)ret;
            dmScript::TeardownCallback(cbk);
        }
    }

    void SetRenderPause(HRenderContext context, uint8_t is_paused)
    {
        context->m_IsRenderPaused = is_paused;
    }

    bool IsRenderPaused(HRenderContext context)
    {
        return context->m_IsRenderPaused;
    }
}
