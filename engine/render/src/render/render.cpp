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
#include "font_renderer.h"

DM_PROPERTY_GROUP(rmtp_Render, "Renderer");

namespace dmRender
{
    using namespace dmVMath;

    const char* RENDER_SOCKET_NAME = "@render";

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
    , m_VertexShaderDesc(0x0)
    , m_FragmentShaderDesc(0x0)
    , m_MaxRenderTypes(0)
    , m_MaxInstances(0)
    , m_MaxRenderTargets(0)
    , m_VertexShaderDescSize(0)
    , m_FragmentShaderDescSize(0)
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

        context->m_SystemFontMap = params.m_SystemFontMap;

        context->m_Material = 0;

        context->m_View = Matrix4::identity();
        context->m_Projection = Matrix4::identity();
        context->m_ViewProj = context->m_Projection * context->m_View;

        context->m_ScriptContext = params.m_ScriptContext;
        InitializeRenderScriptContext(context->m_RenderScriptContext, graphics_context, params.m_ScriptContext, params.m_CommandBufferSize);
        context->m_ScriptWorld = dmScript::NewScriptWorld(context->m_ScriptContext);

        context->m_DebugRenderer.m_RenderContext = 0;
        if (params.m_VertexShaderDesc != 0 && params.m_VertexShaderDescSize != 0 &&
            params.m_FragmentShaderDesc != 0 && params.m_FragmentShaderDescSize != 0) {
            InitializeDebugRenderer(context, params.m_MaxDebugVertexCount, params.m_VertexShaderDesc, params.m_VertexShaderDescSize, params.m_FragmentShaderDesc, params.m_FragmentShaderDescSize);
        }

        memset(context->m_Textures, 0, sizeof(dmGraphics::HTexture) * RenderObject::MAX_TEXTURE_COUNT);

        InitializeTextContext(context, params.m_MaxCharacters);

        context->m_OutOfResources = 0;

        context->m_StencilBufferCleared = 0;

        context->m_RenderListDispatch.SetCapacity(255);

        dmMessage::Result r = dmMessage::NewSocket(RENDER_SOCKET_NAME, &context->m_Socket);
        assert(r == dmMessage::RESULT_OK);
        return context;
    }

    Result DeleteRenderContext(HRenderContext render_context, dmScript::HContext script_context)
    {
        if (render_context == 0x0) return RESULT_INVALID_CONTEXT;

        FinalizeRenderScriptContext(render_context->m_RenderScriptContext, script_context);
        dmScript::DeleteScriptWorld(render_context->m_ScriptWorld);
        FinalizeDebugRenderer(render_context);
        FinalizeTextContext(render_context);
        dmMessage::DeleteSocket(render_context->m_Socket);
        delete render_context;

        return RESULT_OK;
    }

    dmScript::HContext GetScriptContext(HRenderContext render_context) {
        return render_context->m_ScriptContext;
    }

    void RenderListBegin(HRenderContext render_context)
    {
        render_context->m_RenderList.SetSize(0);
        render_context->m_RenderListSortIndices.SetSize(0);
        render_context->m_RenderListDispatch.SetSize(0);
        render_context->m_RenderListRanges.SetSize(0);
        render_context->m_FrustumHash = 0xFFFFFFFF; // trigger a first recalculation each frame
    }

    HRenderListDispatch RenderListMakeDispatch(HRenderContext render_context, RenderListDispatchFn dispatch_fn, RenderListVisibilityFn visibility_fn, void* user_data)
    {
        if (render_context->m_RenderListDispatch.Size() == render_context->m_RenderListDispatch.Capacity())
        {
            dmLogError("Exhausted number of render dispatches. Too many collections?");
            return RENDERLIST_INVALID_DISPATCH;
        }

        // store & return index
        RenderListDispatch d;
        d.m_DispatchFn = dispatch_fn;
        d.m_VisibilityFn = visibility_fn;
        d.m_UserData = user_data;
        render_context->m_RenderListDispatch.Push(d);

        return render_context->m_RenderListDispatch.Size() - 1;
    }

    HRenderListDispatch RenderListMakeDispatch(HRenderContext render_context, RenderListDispatchFn dispatch_fn, void* user_data)
    {
        return RenderListMakeDispatch(render_context, dispatch_fn, 0, user_data);
    }

    // Allocate a buffer (from the array) with room for 'entries' entries.
    //
    // NOTE: Pointer might go invalid after a consecutive call to RenderListAlloc if reallocation
    //       of backing buffer happens.
    RenderListEntry* RenderListAlloc(HRenderContext render_context, uint32_t entries)
    {
        dmArray<RenderListEntry> & render_list = render_context->m_RenderList;

        if (render_list.Remaining() < entries)
        {
            const uint32_t needed = entries - render_list.Remaining();
            render_list.OffsetCapacity(dmMath::Max<uint32_t>(256, needed));
            render_context->m_RenderListSortIndices.SetCapacity(render_list.Capacity());
        }

        uint32_t size = render_list.Size();
        render_list.SetSize(size + entries);

        // If we push new items after the last frustum culling, we need to reevaluate it
        render_context->m_FrustumHash = 0xFFFFFFFF;

        return (render_list.Begin() + size);
    }

    // Submit a range of entries (pointers must be from a range allocated by RenderListAlloc, and not between two alloc calls).
    void RenderListSubmit(HRenderContext render_context, RenderListEntry *begin, RenderListEntry *end)
    {
        // Insert the used up indices into the sort buffer.
        assert(end - begin <= (intptr_t)render_context->m_RenderListSortIndices.Remaining());
        assert(end <= render_context->m_RenderList.End());

        // If we didn't use all entries, let's put them back into the list
        if (end < render_context->m_RenderList.End())
        {
            uint32_t list_size = end - render_context->m_RenderList.Begin();
            render_context->m_RenderList.SetSize(list_size);
        }

        if (end == begin) {
            return;
        }

        // Transform pointers back to indices.
        RenderListEntry *base = render_context->m_RenderList.Begin();
        uint32_t *insert = render_context->m_RenderListSortIndices.End();

        for (RenderListEntry* i=begin;i!=end;i++)
            *insert++ = i - base;

        render_context->m_RenderListSortIndices.SetSize(render_context->m_RenderListSortIndices.Size() + (end - begin));

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
        FlushTexts(render_context, RENDER_ORDER_AFTER_WORLD, 0xffffff, true);

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

    void SetViewMatrix(HRenderContext render_context, const Matrix4& view)
    {
        render_context->m_View = view;
        render_context->m_ViewProj = render_context->m_Projection * view;
    }

    void SetProjectionMatrix(HRenderContext render_context, const Matrix4& projection)
    {
        render_context->m_Projection = projection;
        render_context->m_ViewProj = projection * render_context->m_View;
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
        context->m_TextContext.m_TextEntries.SetSize(0);

        return RESULT_OK;
    }

    // This function will compare the values in ps_orig and ps_now and reset the render state that is different between them
    // It is expected that the first parameter is the "default" state, i.e the values from that pipeline will be used
    static void ResetRenderStateIfChanged(dmGraphics::HContext graphics_context, dmGraphics::PipelineState ps_orig, dmGraphics::PipelineState ps_now)
    {
        #define HAS_CHANGED(name) (ps_now.name != ps_orig.name)

        if (HAS_CHANGED(m_BlendSrcFactor) || HAS_CHANGED(m_BlendDstFactor))
        {
            dmGraphics::SetBlendFunc(graphics_context, (dmGraphics::BlendFactor) ps_orig.m_BlendSrcFactor, (dmGraphics::BlendFactor) ps_orig.m_BlendDstFactor);
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
            ps_now.m_BlendSrcFactor = ro->m_SourceBlendFactor;
            ps_now.m_BlendDstFactor = ro->m_DestinationBlendFactor;
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
    static void MakeSortBuffer(HRenderContext context, uint32_t tag_count, dmhash_t* tags)
    {
        DM_PROFILE("MakeSortBuffer");

        const uint32_t required_capacity = context->m_RenderListSortIndices.Capacity();
        // SetCapacity does early out if they are the same, so just call anyway.
        context->m_RenderListSortBuffer.SetCapacity(required_capacity);
        context->m_RenderListSortBuffer.SetSize(0);
        context->m_RenderListSortValues.SetCapacity(required_capacity);
        context->m_RenderListSortValues.SetSize(context->m_RenderListSortIndices.Size());

        RenderListSortValue* sort_values = context->m_RenderListSortValues.Begin();
        RenderListEntry* entries = context->m_RenderList.Begin();

        const Matrix4& transform = context->m_ViewProj;

        float minZW = FLT_MAX;
        float maxZW = -FLT_MAX;

        RenderListRange* ranges = context->m_RenderListRanges.Begin();
        uint32_t num_ranges = context->m_RenderListRanges.Size();
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
            for (uint32_t i = range.m_Start; i < range.m_Start+range.m_Count; ++i)
            {
                uint32_t idx = context->m_RenderListSortIndices[i];
                RenderListEntry* entry = &entries[idx];
                if (entry->m_Visibility == dmRender::VISIBILITY_NONE)
                {
                    num_visibility_skipped++;
                    continue;
                }

                if (entry->m_MajorOrder != RENDER_ORDER_WORLD)
                {
                    continue; // Could perhaps break here, if we also sorted on the major order (cost more when I tested it /MAWE)
                }

                const Vector4 res = transform * entry->m_WorldPosition;
                const float zw = res.getZ() / res.getW();
                sort_values[idx].m_ZW = zw;
                if (zw < minZW) minZW = zw;
                if (zw > maxZW) maxZW = zw;
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

        for( uint32_t i = 0; i < num_ranges; ++i)
        {
            const RenderListRange& range = ranges[i];
            if (range.m_Skip)
                continue;

            for (uint32_t i = range.m_Start; i < range.m_Start+range.m_Count; ++i)
            {
                uint32_t idx = context->m_RenderListSortIndices[i];
                RenderListEntry* entry = &entries[idx];

                if (entry->m_Visibility == dmRender::VISIBILITY_NONE)
                {
                    continue;
                }

                sort_values[idx].m_MajorOrder = entry->m_MajorOrder;
                if (entry->m_MajorOrder == RENDER_ORDER_WORLD)
                {
                    const float z = sort_values[idx].m_ZW;
                    sort_values[idx].m_Order = (uint32_t) (0xfffff8 - 0xfffff0 * rc * (z - minZW));
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

    static void CollectRenderEntryRange(void* _ctx, uint32_t tag_list_key, size_t start, size_t count)
    {
        HRenderContext context = (HRenderContext)_ctx;
        if (context->m_RenderListRanges.Full())
        {
            context->m_RenderListRanges.SetCapacity(context->m_RenderListRanges.Capacity() + 16);
        }
        RenderListRange range;
        range.m_TagListKey = tag_list_key;
        range.m_Start = start;
        range.m_Count = count;
        context->m_RenderListRanges.Push(range);
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

    static void SortRenderList(HRenderContext context)
    {
        DM_PROFILE("SortRenderList");

        if (context->m_RenderList.Empty())
            return;

        // First sort on the tag masks
        {
            RenderListEntrySorter sort;
            sort.m_Base = context->m_RenderList.Begin();
            std::stable_sort(context->m_RenderListSortIndices.Begin(), context->m_RenderListSortIndices.End(), sort);
        }
        // Now find the ranges of tag masks
        {
            RenderListEntry* entries = context->m_RenderList.Begin();
            FindRangeComparator comp;
            comp.m_Entries = entries;
            FindRenderListRanges(context->m_RenderListSortIndices.Begin(), 0, context->m_RenderListSortIndices.Size(), entries, comp, context, CollectRenderEntryRange);
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    static bool RenderListEntryEqFn(RenderListEntry* a, RenderListEntry* b)
    {
        return a->m_Dispatch == b->m_Dispatch;
    }

    static void SetVisibility(uint32_t count, RenderListEntry* entries, Visibility visibility)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            entries[i].m_Visibility = visibility;
        }
    }

    static void FrustumCulling(HRenderContext context, const dmIntersection::Frustum& frustum)
    {
        DM_PROFILE("FrustumCulling");

        uint32_t num_entries = context->m_RenderList.Size();
        if (num_entries == 0)
            return;

        BatchIterator<RenderListEntry*> iter(num_entries, context->m_RenderList.Begin(), RenderListEntryEqFn);
        while(iter.Next())
        {
            RenderListEntry* batch_start = iter.Begin();

            const RenderListDispatch* d = &context->m_RenderListDispatch[batch_start->m_Dispatch];
            if (!d->m_VisibilityFn)
            {
                SetVisibility(iter.Length(), iter.Begin(), dmRender::VISIBILITY_FULL);
            }
            else {
                RenderListVisibilityParams params;
                params.m_Frustum = &frustum;
                params.m_UserData = d->m_UserData;
                params.m_Entries = batch_start;
                params.m_NumEntries = iter.Length();
                d->m_VisibilityFn(params);
            }
        }
    }

    Result DrawRenderList(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constant_buffer, const dmVMath::Matrix4* frustum_matrix)
    {
        DM_PROFILE("DrawRenderList");

        // This will add new entries for the most recent debug draw render objects.
        // The internal dispatch functions knows to only actually use the latest ones.
        // The sort order is also one below the Texts flush which is only also debug stuff.
        FlushDebug(context, 0xfffffe);

        // Cleared once per frame
        if (context->m_RenderListRanges.Empty())
        {
            SortRenderList(context);
        }

        dmhash_t frustum_hash = frustum_matrix ? dmHashBuffer64((const void*)frustum_matrix, 16*sizeof(float)) : 0;
        if (context->m_FrustumHash != frustum_hash)
        {
            // We use this to avoid calling the culling functions more than once in a row
            context->m_FrustumHash = frustum_hash;

            if (frustum_matrix)
            {
                dmIntersection::Frustum frustum;
                dmIntersection::CreateFrustumFromMatrix(*frustum_matrix, true, frustum);
                FrustumCulling(context, frustum);
            }
            else
            {
                // Reset the visibility
                SetVisibility(context->m_RenderList.Size(), context->m_RenderList.Begin(), dmRender::VISIBILITY_FULL);
            }
        }

        MakeSortBuffer(context, predicate?predicate->m_TagCount:0, predicate?predicate->m_Tags:0);

        if (context->m_RenderListSortBuffer.Empty())
            return RESULT_OK;

        {
            DM_PROFILE("DrawRenderList_SORT");
            RenderListSorter sort;
            sort.values = context->m_RenderListSortValues.Begin();
            std::stable_sort(context->m_RenderListSortBuffer.Begin(), context->m_RenderListSortBuffer.End(), sort);
        }

        // Construct render objects
        context->m_RenderObjects.SetSize(0);

        RenderListDispatchParams params;
        memset(&params, 0x00, sizeof(params));
        params.m_Operation = RENDER_LIST_OPERATION_BEGIN;
        params.m_Context = context;

        // All get begin operation first
        for (uint32_t i=0;i!=context->m_RenderListDispatch.Size();i++)
        {
            const RenderListDispatch& d = context->m_RenderListDispatch[i];
            params.m_UserData = d.m_UserData;
            d.m_DispatchFn(params);
        }

        params.m_Operation = RENDER_LIST_OPERATION_BATCH;
        params.m_Buf = context->m_RenderList.Begin();

        // Make batches for matching dispatch, batch key & minor order
        RenderListEntry *base = context->m_RenderList.Begin();
        uint32_t *last = context->m_RenderListSortBuffer.Begin();
        uint32_t count = context->m_RenderListSortBuffer.Size();

        for (uint32_t i=1;i<=count;i++)
        {
            uint32_t *idx = context->m_RenderListSortBuffer.Begin() + i;
            const RenderListEntry *last_entry = &base[*last];
            const RenderListEntry *current_entry = &base[*idx];

            // continue batch on match, or dispatch
            if (i < count && (last_entry->m_Dispatch == current_entry->m_Dispatch && last_entry->m_BatchKey == current_entry->m_BatchKey && last_entry->m_MinorOrder == current_entry->m_MinorOrder))
                continue;

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

        params.m_Operation = RENDER_LIST_OPERATION_END;
        params.m_Begin = 0;
        params.m_End = 0;
        params.m_Buf = 0;

        for (uint32_t i=0;i!=context->m_RenderListDispatch.Size();i++)
        {
            const RenderListDispatch& d = context->m_RenderListDispatch[i];
            params.m_UserData = d.m_UserData;
            d.m_DispatchFn(params);
        }

        return Draw(context, predicate, constant_buffer);
    }

    // NOTE: Currently only used externally in 1 test (fontview.cpp)
    // TODO: Replace that occurrance with DrawRenderList
    Result Draw(HRenderContext render_context, HPredicate predicate, HNamedConstantBuffer constant_buffer)
    {
        if (render_context == 0x0)
            return RESULT_INVALID_CONTEXT;


        dmGraphics::HContext context = dmRender::GetGraphicsContext(render_context);

        HMaterial material = render_context->m_Material;
        HMaterial context_material = render_context->m_Material;
        if(context_material)
        {
            dmGraphics::EnableProgram(context, GetMaterialProgram(context_material));
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
                }
            }

            ApplyMaterialConstants(render_context, material, ro);

            if (ro->m_ConstantBuffer) // from components/scripts
                ApplyNamedConstantBuffer(render_context, material, ro->m_ConstantBuffer);

            if (constant_buffer) // from render script
                ApplyNamedConstantBuffer(render_context, material, constant_buffer);

            ApplyRenderState(render_context, render_context->m_GraphicsContext, ps_orig, ro);

            for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
            {
                dmGraphics::HTexture texture = ro->m_Textures[i];
                if (render_context->m_Textures[i])
                    texture = render_context->m_Textures[i];
                if (texture)
                {
                    dmGraphics::EnableTexture(context, i, texture);
                    ApplyMaterialSampler(render_context, material, i, texture);
                }
            }

            dmGraphics::EnableVertexDeclaration(context, ro->m_VertexDeclaration, ro->m_VertexBuffer, GetMaterialProgram(material));

            if (ro->m_IndexBuffer)
                dmGraphics::DrawElements(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount, ro->m_IndexType, ro->m_IndexBuffer);
            else
                dmGraphics::Draw(context, ro->m_PrimitiveType, ro->m_VertexStart, ro->m_VertexCount);

            dmGraphics::DisableVertexDeclaration(context, ro->m_VertexDeclaration);

            for (uint32_t i = 0; i < RenderObject::MAX_TEXTURE_COUNT; ++i)
            {
                dmGraphics::HTexture texture = ro->m_Textures[i];
                if (render_context->m_Textures[i])
                    texture = render_context->m_Textures[i];
                if (texture)
                    dmGraphics::DisableTexture(context, i, texture);
            }
        }

        ResetRenderStateIfChanged(context, ps_orig, dmGraphics::GetPipelineState(context));

        return RESULT_OK;
    }

    Result DrawDebug3d(HRenderContext context, const dmVMath::Matrix4* frustum_matrix)
    {
        if (!context->m_DebugRenderer.m_RenderContext) {
            return RESULT_INVALID_CONTEXT;
        }
        return DrawRenderList(context, &context->m_DebugRenderer.m_3dPredicate, 0, frustum_matrix);
    }

    Result DrawDebug2d(HRenderContext context) // Deprecated
    {
        if (!context->m_DebugRenderer.m_RenderContext) {
            return RESULT_INVALID_CONTEXT;
        }
        return DrawRenderList(context, &context->m_DebugRenderer.m_2dPredicate, 0, 0);
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
}
