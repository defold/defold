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

#include <dlib/log.h>
#include <dlib/memory.h>

#include "render_frame_storage.h"

namespace dmRender
{
    // Storage validity, separate from the queue's scheduling/ownership states. Normal
    // capture goes Free -> Building -> Sealed -> Resetting -> Free. Cancellation or
    // failed capture goes Building -> Resetting -> Free without exposing a reader view.
    enum RenderFrameStorageState
    {
        RENDER_FRAME_STORAGE_FREE,      // Empty reusable storage, before BeginRenderFrame.
        RENDER_FRAME_STORAGE_BUILDING,  // Game-owner capture; errors stay sticky until reset.
        RENDER_FRAME_STORAGE_SEALED,    // Immutable reader view; reset only after CPU readers finish.
        RENDER_FRAME_STORAGE_RESETTING, // Releasing adopted references; prevents callback reentry.
    };

    // One adopted resource reference. The producer is responsible for retaining before insertion.
    struct RenderFrameReference
    {
        FRenderFrameRelease m_Release;
        void*               m_Resource;
        void*               m_Context;
    };

    // Append-only backing storage preserves existing table pointers when a later table needs growth.
    struct RenderFrameDynamicBlock
    {
        uint8_t*                 m_Payload;
        RenderFrameDynamicBlock* m_Next;
        uint32_t                 m_Capacity;
        uint32_t                 m_Used;
    };

    // Fixed storage and retained dynamic blocks are reused without clearing payload bytes.
    // Only the producer mutates storage; publication makes the sealed view immutable
    // until the queue returns retirement to that producer.
    struct RenderFrameStorage
    {
        RenderFrameView          m_View;
        RenderFrameLimits        m_Limits;
        uint8_t*                 m_Payload;
        RenderFrameTable*        m_Tables;
        RenderFrameReference*    m_References;
        RenderFrameDynamicBlock* m_DynamicBlocks;
        RenderFrameDynamicBlock* m_CurrentDynamicBlock;
        RenderFrameStorageState  m_State;
        RenderFrameResult        m_Error;
    };

    uint64_t GetRenderFrameStorageSize(const RenderFrameLimits& limits)
    {
        return sizeof(RenderFrameStorage) + (uint64_t)limits.m_PayloadBytes
            + (uint64_t)limits.m_MaxTables * sizeof(RenderFrameTable)
            + (uint64_t)limits.m_MaxReferences * sizeof(RenderFrameReference);
    }

    HRenderFrameStorage NewRenderFrame(const RenderFrameLimits& limits)
    {
        const uint64_t table_bytes = (uint64_t)limits.m_MaxTables * sizeof(RenderFrameTable);
        const uint64_t reference_bytes = (uint64_t)limits.m_MaxReferences * sizeof(RenderFrameReference);
        // All individual allocations fit the unsigned size accepted by dmMemory,
        // including on 32-bit targets. Never truncate an oversized capacity.
        if (table_bytes > UINT32_MAX || reference_bytes > UINT32_MAX)
            return 0;

        RenderFrameStorage* frame = (RenderFrameStorage*)calloc(1, sizeof(RenderFrameStorage));
        frame->m_Limits = limits;
        frame->m_State = RENDER_FRAME_STORAGE_FREE;
        frame->m_Error = RENDER_FRAME_OK;

        if (limits.m_PayloadBytes)
        {
            dmMemory::Result allocation_result = dmMemory::AlignedMalloc((void**)&frame->m_Payload, 16, limits.m_PayloadBytes);
            if (allocation_result != dmMemory::RESULT_OK)
            {
                DeleteRenderFrame(frame);
                return 0;
            }
        }

        frame->m_Tables = (RenderFrameTable*) malloc((size_t) table_bytes);
        frame->m_References = (RenderFrameReference*) malloc((size_t) reference_bytes);
        frame->m_View.m_Payload = frame->m_Payload;
        frame->m_View.m_Tables = frame->m_Tables;
        return frame;
    }

    void BeginRenderFrame(HRenderFrameStorage frame, RenderFrameId frame_id)
    {
        assert(frame->m_State == RENDER_FRAME_STORAGE_FREE);
        assert(frame->m_Error == RENDER_FRAME_OK);
        frame->m_View.m_FrameId = frame_id;
        frame->m_State = RENDER_FRAME_STORAGE_BUILDING;
    }

    static inline RenderFrameResult FailCapture(HRenderFrameStorage frame, RenderFrameResult error)
    {
        frame->m_Error = error;
        return error;
    }

    static inline RenderFrameResult FailCapacity(HRenderFrameStorage frame, const char* budget, uint64_t required, uint32_t limit)
    {
        dmLogError("Render frame %s budget exceeded (required %llu, limit %u).", budget, (unsigned long long)required, limit);
        return FailCapture(frame, RENDER_FRAME_CAPACITY_EXCEEDED);
    }

    static RenderFrameResult CheckCapture(HRenderFrameStorage frame)
    {
        if (frame->m_Error != RENDER_FRAME_OK)
            return frame->m_Error;
        else if (frame->m_State != RENDER_FRAME_STORAGE_BUILDING)
            return FailCapture(frame, RENDER_FRAME_INVALID_STATE);
        return RENDER_FRAME_OK;
    }

    RenderFrameResult FailRenderFrame(HRenderFrameStorage frame, RenderFrameResult error)
    {
        assert(error != RENDER_FRAME_OK);
        RenderFrameResult result = CheckCapture(frame);
        return result != RENDER_FRAME_OK ? result : FailCapture(frame, error);
    }

    static RenderFrameResult CheckTableAllocation(HRenderFrameStorage frame, uint32_t count, uint32_t stride,
                                                  uint32_t alignment, uint32_t* table_index, void** data)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
        {
            return result;
        }
        if (!count || !stride || !alignment || alignment > 16 || (alignment & (alignment - 1)) || !table_index || !data)
        {
            return FailCapture(frame, RENDER_FRAME_INVALID_ARGUMENT);
        }
        if (frame->m_View.m_Statistics.m_TableCount == frame->m_Limits.m_MaxTables)
        {
            return FailCapacity(frame, "table count", (uint64_t)frame->m_View.m_Statistics.m_TableCount + 1, frame->m_Limits.m_MaxTables);
        }
        return RENDER_FRAME_OK;
    }

    static void AppendTable(HRenderFrameStorage frame, uint32_t type, uint32_t count, uint32_t stride,
                            void* payload, uint32_t* table_index, void** data)
    {
        RenderFrameStatistics& statistics = frame->m_View.m_Statistics;
        RenderFrameTable& table = frame->m_Tables[statistics.m_TableCount];
        table.m_Data = payload;
        table.m_Type = type;
        table.m_Count = count;
        table.m_Stride = stride;
        *table_index = statistics.m_TableCount++;
        *data = payload;
        ++statistics.m_AllocationCount;
    }

    RenderFrameResult AllocateRenderFrameTable(HRenderFrameStorage frame, uint32_t type, uint32_t count,
                                               uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data)
    {
        RenderFrameResult result = CheckTableAllocation(frame, count, stride, alignment, table_index, data);
        if (result != RENDER_FRAME_OK)
        {
            return result;
        }

        RenderFrameStatistics& statistics = frame->m_View.m_Statistics;
        const uint64_t bytes = (uint64_t)count * stride;
        const uint64_t offset = ((uint64_t)statistics.m_PayloadBytes + alignment - 1) & ~((uint64_t)alignment - 1);
        if (offset + bytes > frame->m_Limits.m_PayloadBytes)
        {
            return FailCapacity(frame, "fixed payload bytes", offset + bytes, frame->m_Limits.m_PayloadBytes);
        }

        AppendTable(frame, type, count, stride, frame->m_Payload + offset, table_index, data);
        statistics.m_PayloadBytes = (uint32_t)(offset + bytes);
        return RENDER_FRAME_OK;
    }

    RenderFrameResult AllocateRenderFrameDynamicTable(HRenderFrameStorage frame, uint32_t type, uint32_t count,
                                                      uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data)
    {
        RenderFrameResult result = CheckTableAllocation(frame, count, stride, alignment, table_index, data);
        if (result != RENDER_FRAME_OK)
        {
            return result;
        }

        const uint64_t bytes = (uint64_t)count * stride;
        if (bytes > UINT32_MAX)
        {
            return FailCapacity(frame, "dynamic table bytes", bytes, UINT32_MAX);
        }

        RenderFrameDynamicBlock* block = frame->m_CurrentDynamicBlock;
        RenderFrameDynamicBlock* previous = 0;
        uint64_t offset = 0;
        while (block)
        {
            // Keep this 64-bit: DM_ALIGN uses uintptr_t and can wrap on 32-bit targets.
            offset = ((uint64_t)block->m_Used + alignment - 1) & ~((uint64_t)alignment - 1);
            if (offset + bytes <= block->m_Capacity)
            {
                break;
            }
            previous = block;
            block = block->m_Next;
        }

        RenderFrameStatistics& statistics = frame->m_View.m_Statistics;
        if (!block)
        {
            // Size classes avoid retaining a new near-identical block whenever a
            // snapshot grows slightly between frames. Old table pointers stay valid.
            uint32_t capacity = 64 * 1024;
            while (capacity < bytes)
            {
                capacity = capacity > UINT32_MAX / 2 ? UINT32_MAX : capacity * 2;
            }
            if (statistics.m_DynamicPayloadCapacity > UINT64_MAX - capacity)
            {
                dmLogError("Render frame dynamic payload capacity overflow (retained %llu bytes, requested %u bytes).",
                           (unsigned long long)statistics.m_DynamicPayloadCapacity, capacity);
                return FailCapture(frame, RENDER_FRAME_CAPACITY_EXCEEDED);
            }

            block = (RenderFrameDynamicBlock*)malloc(sizeof(RenderFrameDynamicBlock));
            if (!block)
            {
                dmLogError("Unable to allocate render frame dynamic block descriptor.");
                return FailCapture(frame, RENDER_FRAME_OUT_OF_MEMORY);
            }
            block->m_Payload = 0;

            dmMemory::Result allocation_result = dmMemory::AlignedMalloc((void**)&block->m_Payload, 16, capacity);
            if (allocation_result != dmMemory::RESULT_OK || !block->m_Payload)
            {
                dmMemory::AlignedFree(block->m_Payload);
                free(block);
                dmLogError("Unable to allocate render frame dynamic payload block (%u bytes).", capacity);
                return FailCapture(frame, RENDER_FRAME_OUT_OF_MEMORY);
            }

            block->m_Next = 0;
            block->m_Capacity = capacity;
            block->m_Used = 0;

            if (previous)
            {
                previous->m_Next = block;
            }
            else
            {
                frame->m_DynamicBlocks = block;
            }
            statistics.m_DynamicPayloadCapacity += capacity;
            ++statistics.m_DynamicAllocationCount;
            offset = 0;
        }

        // Advance through existing blocks monotonically; reset restores every cursor.
        // Identical captures reuse the same allocations and addresses without growth.
        frame->m_CurrentDynamicBlock = block;
        const uint32_t used = (uint32_t)(offset + bytes);
        statistics.m_DynamicPayloadBytes += used - block->m_Used;
        block->m_Used = used;
        AppendTable(frame, type, count, stride, block->m_Payload + offset, table_index, data);
        return RENDER_FRAME_OK;
    }

    RenderFrameResult CopyRenderFrameTable(HRenderFrameStorage frame, uint32_t type, uint32_t count,
                                           uint32_t stride, uint32_t alignment, const void* data, uint32_t* table_index, void** copied_data)
    {
        if (!data)
        {
            return FailRenderFrame(frame, RENDER_FRAME_INVALID_ARGUMENT);
        }

        void* destination;
        RenderFrameResult result = AllocateRenderFrameTable(frame, type, count, stride, alignment, table_index, &destination);
        if (result != RENDER_FRAME_OK)
        {
            return result;
        }

        // Allocation validated the product against the uint32_t payload capacity.
        const uint32_t bytes = count * stride;
        CopyRenderFrameData(frame, destination, data, bytes);
        if (copied_data)
        {
            *copied_data = destination;
        }
        return RENDER_FRAME_OK;
    }

    RenderFrameResult CopyRenderFrameData(HRenderFrameStorage frame, void* destination, const void* source, uint32_t bytes)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
        {
            return result;
        }
        if (bytes)
        {
            memcpy(destination, source, bytes);
        }
        frame->m_View.m_Statistics.m_CopiedBytes += bytes;
        return RENDER_FRAME_OK;
    }

    RenderFrameResult AddRenderFrameReference(HRenderFrameStorage frame, void* resource, FRenderFrameRelease release, void* context)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
            return result;
        else if (!resource || !release)
            return FailCapture(frame, RENDER_FRAME_INVALID_ARGUMENT);

        RenderFrameStatistics& statistics = frame->m_View.m_Statistics;
        if (statistics.m_ReferenceCount == frame->m_Limits.m_MaxReferences)
        {
            return FailCapacity(frame, "reference count", (uint64_t)statistics.m_ReferenceCount + 1, frame->m_Limits.m_MaxReferences);
        }

        RenderFrameReference& reference = frame->m_References[statistics.m_ReferenceCount++];
        reference.m_Resource = resource;
        reference.m_Release = release;
        reference.m_Context = context;
        return RENDER_FRAME_OK;
    }

    RenderFrameResult SealRenderFrame(HRenderFrameStorage frame)
    {
        RenderFrameResult result = CheckCapture(frame);
        if (result != RENDER_FRAME_OK)
            return result;
        frame->m_State = RENDER_FRAME_STORAGE_SEALED;
        return RENDER_FRAME_OK;
    }

    const RenderFrameView* GetRenderFrameView(HRenderFrameStorage frame)
    {
        return frame->m_State == RENDER_FRAME_STORAGE_SEALED ? &frame->m_View : 0;
    }

    const void* GetRenderFrameTableData(const RenderFrameView& frame, uint32_t table_index)
    {
        assert(table_index < frame.m_Statistics.m_TableCount);
        return frame.m_Tables[table_index].m_Data;
    }

    RenderFrameStatistics GetRenderFrameStatistics(HRenderFrameStorage frame)
    {
        return frame->m_View.m_Statistics;
    }

    void ResetRenderFrame(HRenderFrameStorage frame)
    {
        assert(frame->m_State != RENDER_FRAME_STORAGE_RESETTING);
        frame->m_State = RENDER_FRAME_STORAGE_RESETTING;
        while (frame->m_View.m_Statistics.m_ReferenceCount)
        {
            const RenderFrameReference& reference = frame->m_References[--frame->m_View.m_Statistics.m_ReferenceCount];
            reference.m_Release(reference.m_Context, reference.m_Resource);
        }
        for (RenderFrameDynamicBlock* block = frame->m_DynamicBlocks; block; block = block->m_Next)
        {
            block->m_Used = 0;
        }
        frame->m_CurrentDynamicBlock = frame->m_DynamicBlocks;
        const uint64_t dynamic_capacity = frame->m_View.m_Statistics.m_DynamicPayloadCapacity;
        memset(&frame->m_View.m_Statistics, 0, sizeof(frame->m_View.m_Statistics));
        frame->m_View.m_Statistics.m_DynamicPayloadCapacity = dynamic_capacity;
        frame->m_View.m_FrameId = 0;
        frame->m_Error = RENDER_FRAME_OK;
        frame->m_State = RENDER_FRAME_STORAGE_FREE;
    }

    void DeleteRenderFrame(HRenderFrameStorage frame)
    {
        if (!frame)
            return;
        ResetRenderFrame(frame);
        while (frame->m_DynamicBlocks)
        {
            RenderFrameDynamicBlock* block = frame->m_DynamicBlocks;
            frame->m_DynamicBlocks = block->m_Next;
            dmMemory::AlignedFree(block->m_Payload);
            free(block);
        }
        dmMemory::AlignedFree(frame->m_Payload);
        free(frame->m_Tables);
        free(frame->m_References);
        free(frame);
    }
}
