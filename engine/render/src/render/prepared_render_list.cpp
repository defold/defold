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

#include "prepared_render_list.h"
#include "render_private.h"
#include "font/font_renderer.h"
#include <dlib/time.h>
#include <dlib/log.h>
#include <dmsdk/dlib/intersection.h>

namespace dmRender
{
    PreparedRenderList::PreparedRenderList() : m_PrepareTime(0), m_PrepareCount(0) {}

    PreparedRenderList::~PreparedRenderList()
    {
        for (uint32_t i = 0; i < m_Constants.Size(); ++i)
            DeleteNamedConstantBuffer(m_Constants[i]);
    }

    uint64_t GetPreparedRenderListCapacity(const PreparedRenderList* frame)
    {
        uint64_t bytes = sizeof(*frame) + frame->m_Entries.Capacity() * sizeof(PreparedRenderEntry) +
                         frame->m_Constants.Capacity() * sizeof(HNamedConstantBuffer);
        for (uint32_t i = 0; i < frame->m_Constants.Size(); ++i)
            bytes += GetNamedConstantBufferCapacity(frame->m_Constants[i]);
        return bytes;
    }

    void SetRenderListSnapshotBounds(HRenderContext context, HRenderListDispatch dispatch, RenderListBoundsFn fn)
    {
        if (dispatch != RENDERLIST_INVALID_DISPATCH)
            context->m_RenderListDispatch[dispatch].m_SnapshotBoundsFn = fn;
    }

    static void PreparedDispatch(const RenderListDispatchParams& params);
    static void PreparedVisibility(const RenderListVisibilityParams& params);

    static bool CanPrepare(const RenderListDispatch& dispatch)
    {
        return !dispatch.m_VisibilityFn || dispatch.m_SnapshotBoundsFn;
    }

    bool PrepareRenderList(HRenderContext context, PreparedRenderList* output, bool keep_other_dispatches)
    {
        const uint64_t start = dmTime::GetMonotonicTime();
        output->m_Entries.SetSize(0);
        uint32_t selected_count = 0;
        bool ok = context->m_TextContext.m_TextEntries.Size() <= context->m_TextContext.m_RenderObjects.Size();
        for (uint32_t i = 0; ok && i < context->m_RenderList.Size(); ++i)
        {
            const RenderListEntry& entry = context->m_RenderList[i];
            if (entry.m_Dispatch >= context->m_RenderListDispatch.Size())
                ok = false;
            else if (CanPrepare(context->m_RenderListDispatch[entry.m_Dispatch]))
                ++selected_count;
            else if (!keep_other_dispatches)
                ok = false;
        }
        // Reject unsupported dispatches even when they submitted no entries.
        for (uint32_t i = 0; ok && !keep_other_dispatches && i < context->m_RenderListDispatch.Size(); ++i)
            ok = CanPrepare(context->m_RenderListDispatch[i]);
        ok = ok && selected_count <= PreparedRenderList::MAX_ENTRIES;
        if (!ok)
        {
            dmLogError("Mixed preparation exceeds entry limit or has an unsupported visibility callback");
            ClearTextEntries(context);
            return false;
        }

        RenderListDispatchParams params = {};
        params.m_Context = context;
        params.m_Operation = RENDER_LIST_OPERATION_BEGIN;
        for (uint32_t i = 0; i < context->m_RenderListDispatch.Size(); ++i)
        {
            const RenderListDispatch& d = context->m_RenderListDispatch[i];
            if (keep_other_dispatches && !CanPrepare(d)) continue;
            params.m_UserData = d.m_UserData;
            d.m_DispatchFn(params);
        }

        uint64_t constant_bytes = 0;
        for (uint32_t i = 0; i < output->m_Constants.Size(); ++i)
            constant_bytes += GetNamedConstantBufferCapacity(output->m_Constants[i]);
        for (uint32_t i = 0; ok && i < context->m_RenderList.Size(); ++i)
        {
            const RenderListEntry entry = context->m_RenderList[i];
            if (entry.m_Dispatch == RENDERLIST_INVALID_DISPATCH)
            {
                ok = false;
                break;
            }
            const RenderListDispatch& d = context->m_RenderListDispatch[entry.m_Dispatch];
            if (keep_other_dispatches && !CanPrepare(d)) continue;
            dmVMath::Vector4 sphere(0, 0, 0, -1);
            if (d.m_SnapshotBoundsFn)
                d.m_SnapshotBoundsFn(entry, &sphere);
            context->m_RenderObjects.SetSize(0);
            params.m_Operation = RENDER_LIST_OPERATION_BATCH;
            params.m_UserData = d.m_UserData;
            params.m_Buf = context->m_RenderList.Begin();
            params.m_Begin = &i;
            params.m_End = &i + 1;
            d.m_DispatchFn(params);
            if (keep_other_dispatches && context->m_RenderObjects.Size() != 1)
            {
                ok = false;
                break;
            }
            for (uint32_t j = 0; j < context->m_RenderObjects.Size(); ++j)
            {
                RenderObject* ro = context->m_RenderObjects[j];
                const uint32_t n = output->m_Entries.Size();
                // Conservatively admit old storage plus a complete replacement.
                const uint64_t incoming = ro->m_ConstantBuffer ? GetNamedConstantBufferCapacity(ro->m_ConstantBuffer) : 0;
                if (n == PreparedRenderList::MAX_ENTRIES || constant_bytes + incoming + GetNamedConstantBufferCapacity(0) > PreparedRenderList::MAX_CONSTANT_BYTES)
                {
                    ok = false;
                    break;
                }
                if (output->m_Entries.Full())
                    output->m_Entries.SetCapacity(dmMath::Min<uint32_t>(PreparedRenderList::MAX_ENTRIES, dmMath::Max<uint32_t>(16, n * 2)));
                if (n == output->m_Constants.Size())
                {
                    if (output->m_Constants.Full())
                        output->m_Constants.SetCapacity(output->m_Entries.Capacity());
                    output->m_Constants.Push(NewNamedConstantBuffer());
                    constant_bytes += GetNamedConstantBufferCapacity(output->m_Constants[n]);
                }
                PreparedRenderEntry prepared;
                prepared.m_Entry = entry;
                // Store an index, never a component pointer, even in metadata.
                prepared.m_Entry.m_UserData = i;
                prepared.m_Object = *ro;
                prepared.m_Sphere = sphere;
                ClearNamedConstantBuffer(output->m_Constants[n]);
                if (ro->m_ConstantBuffer)
                {
                    constant_bytes -= GetNamedConstantBufferCapacity(output->m_Constants[n]);
                    CopyNamedConstantBuffer(output->m_Constants[n], ro->m_ConstantBuffer);
                    constant_bytes += GetNamedConstantBufferCapacity(output->m_Constants[n]);
                }
                prepared.m_Object.m_ConstantBuffer = ro->m_ConstantBuffer ? output->m_Constants[n] : 0;
                output->m_Entries.Push(prepared);
            }
        }
        params.m_Operation = RENDER_LIST_OPERATION_END;
        params.m_Buf = 0;
        params.m_Begin = params.m_End = 0;
        for (uint32_t i = 0; i < context->m_RenderListDispatch.Size(); ++i)
        {
            const RenderListDispatch& d = context->m_RenderListDispatch[i];
            if (keep_other_dispatches && !CanPrepare(d)) continue;
            params.m_UserData = d.m_UserData;
            d.m_DispatchFn(params);
        }
        // TextLayout's non-atomic references never cross the producer boundary.
        ClearTextEntries(context);
        context->m_RenderObjects.SetSize(0);
        output->m_PrepareTime += dmTime::GetMonotonicTime() - start;
        ++output->m_PrepareCount;
        if (!ok)
        {
            output->m_Entries.SetSize(0);
            dmLogError("Mixed preparation exceeded its packet or constant budget");
        }
        if (ok && keep_other_dispatches)
        {
            for (uint32_t i = 0; i < output->m_Entries.Size(); ++i)
                context->m_RenderList[output->m_Entries[i].m_Entry.m_UserData].m_UserData = i;
            for (uint32_t i = 0; i < context->m_RenderListDispatch.Size(); ++i)
            {
                RenderListDispatch& d = context->m_RenderListDispatch[i];
                if (!CanPrepare(d)) continue;
                d.m_DispatchFn = PreparedDispatch;
                d.m_VisibilityFn = PreparedVisibility;
                d.m_UserData = output;
                d.m_SnapshotBoundsFn = 0;
            }
        }
        return ok;
    }

    static void PreparedDispatch(const RenderListDispatchParams& params)
    {
        if (params.m_Operation != RENDER_LIST_OPERATION_BATCH)
            return;
        PreparedRenderList* frame = (PreparedRenderList*)params.m_UserData;
        for (uint32_t* i = params.m_Begin; i != params.m_End; ++i)
            AddToRender(params.m_Context, &frame->m_Entries[params.m_Buf[*i].m_UserData].m_Object);
    }

    static void PreparedVisibility(const RenderListVisibilityParams& params)
    {
        PreparedRenderList* frame = (PreparedRenderList*)params.m_UserData;
        for (uint32_t i = 0; i < params.m_NumEntries; ++i)
        {
            RenderListEntry& entry = params.m_Entries[i];
            const dmVMath::Vector4& sphere = frame->m_Entries[entry.m_UserData].m_Sphere;
            const bool visible = sphere.getW() < 0 || dmIntersection::TestFrustumSphereSq(*params.m_Frustum, dmVMath::Point3(sphere.getXYZ()), sphere.getW());
            entry.m_Visibility = visible ? VISIBILITY_FULL : VISIBILITY_NONE;
        }
    }

    void SubmitPreparedRenderList(HRenderContext context, PreparedRenderList* frame)
    {
        if (frame->m_Entries.Empty())
            return;
        HRenderListDispatch dispatch = RenderListMakeDispatch(context, PreparedDispatch, PreparedVisibility, frame);
        RenderListEntry* entries = RenderListAlloc(context, frame->m_Entries.Size());
        for (uint32_t i = 0; i < frame->m_Entries.Size(); ++i)
        {
            entries[i] = frame->m_Entries[i].m_Entry;
            entries[i].m_Dispatch = dispatch;
            entries[i].m_UserData = i;
        }
        RenderListSubmit(context, entries, entries + frame->m_Entries.Size());
    }
}
