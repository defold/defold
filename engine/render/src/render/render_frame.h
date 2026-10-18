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

#ifndef DM_RENDER_FRAME_H
#define DM_RENDER_FRAME_H

#include <stdint.h>

namespace dmRender
{
    struct RenderFrameStorage;

    typedef uint64_t            RenderFrameId;
    typedef RenderFrameStorage* HRenderFrame;

    // Explicit backpressure and capture errors; failed capture cannot be published.
    enum RenderFrameResult
    {
        RENDER_FRAME_OK,
        RENDER_FRAME_WOULD_BLOCK,
        RENDER_FRAME_STOPPED,
        RENDER_FRAME_INVALID_ARGUMENT,
        RENDER_FRAME_INVALID_STATE,
        RENDER_FRAME_OUT_OF_MEMORY,
        RENDER_FRAME_CAPACITY_EXCEEDED,
    };

    // Hard per-slot capacities, allocated once. No arena growth or per-frame heap allocation.
    // Payload alignment is at most 16 bytes; metadata/reference storage is budgeted separately.
    struct RenderFrameLimits
    {
        uint32_t m_PayloadBytes;
        uint32_t m_MaxTables;
        uint32_t m_MaxReferences;
    };

    // Typed, contiguous input range addressed by offset rather than a borrowed game pointer.
    struct RenderFrameTable
    {
        uint32_t m_Type;
        uint32_t m_Offset;
        uint32_t m_Count;
        uint32_t m_Stride;
    };

    // Capture counters without per-operation clocks. AllocationCount counts arena reservations,
    // not heap allocations; storage is preallocated. PayloadBytes includes alignment padding.
    struct RenderFrameStatistics
    {
        uint32_t m_PayloadBytes;
        uint32_t m_CopiedBytes;
        uint32_t m_AllocationCount;
        uint32_t m_TableCount;
        uint32_t m_ReferenceCount;
    };

    // Immutable for the duration of consumption. Consumers must not retain these pointers.
    // Uploads outliving consumption must copy/transfer into independently owned storage.
    struct RenderFrameView
    {
        RenderFrameId           m_FrameId;
        const uint8_t*          m_Payload;
        const RenderFrameTable* m_Tables;
        RenderFrameStatistics   m_Statistics;
    };

    // Releases one already-retained dependency on the game owner, in reverse insertion order.
    typedef void (*RenderFrameRelease)(void* context, void* resource);

    // Storage lifecycle used by the frame queue (or isolated storage tests), never by readers.
    HRenderFrame      NewRenderFrame(const RenderFrameLimits& limits);
    void              DeleteRenderFrame(HRenderFrame frame);
    void              BeginRenderFrame(HRenderFrame frame, RenderFrameId frame_id);
    RenderFrameResult SealRenderFrame(HRenderFrame frame);
    void              ResetRenderFrame(HRenderFrame frame);

    // Building-only capture. Every error is sticky until Reset; output is assigned only on
    // success. Direct allocation avoids a copy; callers initialize every declared element.
    // Count and stride must be nonzero; an empty frame simply contains no tables.
    RenderFrameResult AllocateRenderFrameTable(HRenderFrame frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data);
    RenderFrameResult CopyRenderFrameTable(HRenderFrame frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, const void* data, uint32_t* table_index);
    // Resource and release callback must be nonnull. Ownership transfers ONLY on success;
    // on failure the caller still releases its reference.
    RenderFrameResult AddRenderFrameReference(HRenderFrame frame, void* resource, RenderFrameRelease release, void* context);

    // Returns null until sealed. Reader access is valid only until the queue acknowledges it.
    const RenderFrameView* GetRenderFrameView(HRenderFrame frame);
    const void*            GetRenderFrameTableData(const RenderFrameView& frame, uint32_t table_index);
    RenderFrameStatistics  GetRenderFrameStatistics(HRenderFrame frame);
    // Fixed allocation budget (payload + table/reference capacities + private storage).
    uint64_t GetRenderFrameStorageSize(const RenderFrameLimits& limits);
}

#endif // DM_RENDER_FRAME_H
