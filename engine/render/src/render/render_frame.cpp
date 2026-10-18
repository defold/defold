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
#include <stdlib.h>
#include <string.h>

#include <dlib/memory.h>

#include "render_frame.h"

namespace dmRender
{
    // Storage-local lifecycle; the queue separately tracks ready/reading/retired ownership.
    enum RenderFrameStorageState
    {
        RENDER_FRAME_STORAGE_FREE,
        RENDER_FRAME_STORAGE_BUILDING,
        RENDER_FRAME_STORAGE_SEALED,
        RENDER_FRAME_STORAGE_RESETTING,
    };

    // One adopted reference. The producer is responsible for retaining before insertion.
    struct RenderFrameReference
    {
        RenderFrameRelease m_Release;
        void*              m_Resource;
        void*              m_Context;
    };

    // Fixed storage reused across captures without clearing payload bytes or allocating.
    // Only the producer mutates storage; publication makes the sealed view immutable
    // until the queue returns retirement to that producer.
    struct RenderFrameStorage
    {
        RenderFrameView         m_View;
        RenderFrameLimits       m_Limits;
        uint8_t*                m_Payload;
        RenderFrameTable*       m_Tables;
        RenderFrameReference*   m_References;
        RenderFrameStorageState m_State;
        RenderFrameResult       m_Error;
    };

    uint64_t GetRenderFrameStorageSize(const RenderFrameLimits& limits)
    {
        return sizeof(RenderFrameStorage) + (uint64_t)limits.m_PayloadBytes
            + (uint64_t)limits.m_MaxTables * sizeof(RenderFrameTable)
            + (uint64_t)limits.m_MaxReferences * sizeof(RenderFrameReference);
    }

    HRenderFrame NewRenderFrame(const RenderFrameLimits& limits)
    {
        const uint64_t table_bytes = (uint64_t)limits.m_MaxTables * sizeof(RenderFrameTable);
        const uint64_t reference_bytes = (uint64_t)limits.m_MaxReferences * sizeof(RenderFrameReference);
        // All individual allocations fit the unsigned size accepted by dmMemory,
        // including on 32-bit targets. Never truncate an oversized capacity.
        if (table_bytes > UINT32_MAX || reference_bytes > UINT32_MAX)
            return 0;

        RenderFrameStorage* frame = (RenderFrameStorage*)calloc(1, sizeof(RenderFrameStorage));
        if (!frame)
            return 0;

        frame->m_Limits = limits;
        frame->m_State = RENDER_FRAME_STORAGE_FREE;
        frame->m_Error = RENDER_FRAME_OK;

        if (limits.m_PayloadBytes && dmMemory::RESULT_OK != dmMemory::AlignedMalloc((void**)&frame->m_Payload, 16, limits.m_PayloadBytes))
        {
            DeleteRenderFrame(frame);
            return 0;
        }

        if (table_bytes)
        {
            frame->m_Tables = (RenderFrameTable*)malloc((size_t)table_bytes);
            if (!frame->m_Tables)
            {
                DeleteRenderFrame(frame);
                return 0;
            }
        }

        if (reference_bytes)
        {
            frame->m_References = (RenderFrameReference*)malloc((size_t)reference_bytes);
            if (!frame->m_References)
            {
                DeleteRenderFrame(frame);
                return 0;
            }
        }
        frame->m_View.m_Payload = frame->m_Payload;
        frame->m_View.m_Tables = frame->m_Tables;
        return frame;
    }

    void BeginRenderFrame(HRenderFrame frame, RenderFrameId frame_id)
    {
        assert(frame->m_State == RENDER_FRAME_STORAGE_FREE);
        assert(frame->m_Error == RENDER_FRAME_OK);
        frame->m_View.m_FrameId = frame_id;
        frame->m_State = RENDER_FRAME_STORAGE_BUILDING;
    }

    static RenderFrameResult FailCapture(HRenderFrame frame, RenderFrameResult error)
    {
        if (frame->m_Error == RENDER_FRAME_OK)
            frame->m_Error = error;
        return frame->m_Error;
    }

    static RenderFrameResult CheckCapture(HRenderFrame frame)
    {
        if (frame->m_Error != RENDER_FRAME_OK)
            return frame->m_Error;
        if (frame->m_State != RENDER_FRAME_STORAGE_BUILDING)
            return FailCapture(frame, RENDER_FRAME_INVALID_STATE);
        return RENDER_FRAME_OK;
    }

    RenderFrameResult FailRenderFrame(HRenderFrame frame, RenderFrameResult error)
    {
        assert(error != RENDER_FRAME_OK);
        RenderFrameResult result = CheckCapture(frame);
        return result != RENDER_FRAME_OK ? result : FailCapture(frame, error);
    }

    RenderFrameResult AllocateRenderFrameTable(HRenderFrame frame, uint32_t type, uint32_t count,
        uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
            return result;
        if (!count || !stride || !alignment || alignment > 16 || (alignment & (alignment - 1)) || !table_index || !data)
            return FailCapture(frame, RENDER_FRAME_INVALID_ARGUMENT);

        RenderFrameStatistics& statistics = frame->m_View.m_Statistics;
        const uint64_t bytes = (uint64_t)count * stride;
        const uint64_t offset = ((uint64_t)statistics.m_PayloadBytes + alignment - 1) & ~((uint64_t)alignment - 1);

        if (statistics.m_TableCount == frame->m_Limits.m_MaxTables || offset + bytes > frame->m_Limits.m_PayloadBytes)
            return FailCapture(frame, RENDER_FRAME_CAPACITY_EXCEEDED);

        RenderFrameTable& table = frame->m_Tables[statistics.m_TableCount];
        table.m_Type = type;
        table.m_Offset = (uint32_t)offset;
        table.m_Count = count;
        table.m_Stride = stride;
        *table_index = statistics.m_TableCount++;
        *data = frame->m_Payload + offset;
        statistics.m_PayloadBytes = (uint32_t)(offset + bytes);
        ++statistics.m_AllocationCount;

        return RENDER_FRAME_OK;
    }

    RenderFrameResult CopyRenderFrameTable(HRenderFrame frame, uint32_t type, uint32_t count,
        uint32_t stride, uint32_t alignment, const void* data, uint32_t* table_index)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
            return result;
        if (!data)
            return FailCapture(frame, RENDER_FRAME_INVALID_ARGUMENT);

        void* destination;
        result = AllocateRenderFrameTable(frame, type, count, stride, alignment, table_index, &destination);
        if (result != RENDER_FRAME_OK)
            return result;

        // Allocation validated the product against the uint32_t payload capacity.
        const uint32_t bytes = count * stride;
        memcpy(destination, data, bytes);
        frame->m_View.m_Statistics.m_CopiedBytes += bytes;
        return RENDER_FRAME_OK;
    }

    RenderFrameResult AddRenderFrameReference(HRenderFrame frame, void* resource,
        RenderFrameRelease release, void* context)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
            return result;
        if (!resource || !release)
            return FailCapture(frame, RENDER_FRAME_INVALID_ARGUMENT);

        RenderFrameStatistics& statistics = frame->m_View.m_Statistics;
        if (statistics.m_ReferenceCount == frame->m_Limits.m_MaxReferences)
        {
            return FailCapture(frame, RENDER_FRAME_CAPACITY_EXCEEDED);
        }

        RenderFrameReference& reference = frame->m_References[statistics.m_ReferenceCount++];
        reference.m_Resource = resource;
        reference.m_Release = release;
        reference.m_Context = context;
        return RENDER_FRAME_OK;
    }

    RenderFrameResult SealRenderFrame(HRenderFrame frame)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
            return result;
        frame->m_State = RENDER_FRAME_STORAGE_SEALED;
        return RENDER_FRAME_OK;
    }

    const RenderFrameView* GetRenderFrameView(HRenderFrame frame)
    {
        return frame->m_State == RENDER_FRAME_STORAGE_SEALED ? &frame->m_View : 0;
    }

    const void* GetRenderFrameTableData(const RenderFrameView& frame, uint32_t table_index)
    {
        assert(table_index < frame.m_Statistics.m_TableCount);
        return frame.m_Payload + frame.m_Tables[table_index].m_Offset;
    }

    RenderFrameStatistics GetRenderFrameStatistics(HRenderFrame frame)
    {
        return frame->m_View.m_Statistics;
    }

    void ResetRenderFrame(HRenderFrame frame)
    {
        assert(frame->m_State != RENDER_FRAME_STORAGE_RESETTING);
        frame->m_State = RENDER_FRAME_STORAGE_RESETTING;
        while (frame->m_View.m_Statistics.m_ReferenceCount)
        {
            const RenderFrameReference& reference = frame->m_References[--frame->m_View.m_Statistics.m_ReferenceCount];
            reference.m_Release(reference.m_Context, reference.m_Resource);
        }
        memset(&frame->m_View.m_Statistics, 0, sizeof(frame->m_View.m_Statistics));
        frame->m_View.m_FrameId = 0;
        frame->m_Error = RENDER_FRAME_OK;
        frame->m_State = RENDER_FRAME_STORAGE_FREE;
    }

    void DeleteRenderFrame(HRenderFrame frame)
    {
        if (!frame)
            return;
        ResetRenderFrame(frame);
        dmMemory::AlignedFree(frame->m_Payload);
        free(frame->m_Tables);
        free(frame->m_References);
        free(frame);
    }
}
