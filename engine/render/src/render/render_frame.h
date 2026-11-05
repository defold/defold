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

    /** Explicit backpressure and capture errors; failed capture cannot be published. */
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

    /**
     * Hard per-slot capacities, allocated once. No arena growth or per-frame heap allocation.
     * Payload alignment is at most 16 bytes; metadata/reference storage is budgeted separately.
     */
    struct RenderFrameLimits
    {
        uint32_t m_PayloadBytes;
        uint32_t m_MaxTables;
        uint32_t m_MaxReferences;
    };

    /** Typed, contiguous input range addressed by offset rather than a borrowed game pointer. */
    struct RenderFrameTable
    {
        uint32_t m_Type;
        uint32_t m_Offset;
        uint32_t m_Count;
        uint32_t m_Stride;
    };

    /**
     * Capture counters without per-operation clocks. AllocationCount counts arena reservations,
     * not heap allocations; storage is preallocated. PayloadBytes includes alignment padding.
     */
    struct RenderFrameStatistics
    {
        uint32_t m_PayloadBytes;
        uint32_t m_CopiedBytes;
        uint32_t m_AllocationCount;
        uint32_t m_TableCount;
        uint32_t m_ReferenceCount;
    };

    /**
     * Immutable for the duration of consumption. Consumers must not retain these pointers.
     * Uploads outliving consumption must copy/transfer into independently owned storage.
     */
    struct RenderFrameView
    {
        RenderFrameStatistics   m_Statistics;
        RenderFrameId           m_FrameId;
        const uint8_t*          m_Payload;
        const RenderFrameTable* m_Tables;
    };

    /**
     * Release an adopted dependency on the game owner during cancellation or retirement.
     * References are released in reverse insertion order. Do not reenter frame lifecycle calls.
     * @param context Caller context supplied when the reference was added
     * @param resource Already-retained dependency being released
     */
    typedef void (*FRenderFrameRelease)(void* context, void* resource);

    // Storage lifecycle used by the frame queue (or isolated storage tests), never by readers.
    /**
     * Allocate fixed-capacity storage once; capture and reset never grow it.
     * @param limits Hard payload, table and retained-reference capacities
     * @return Empty frame storage, or null for unsupported capacities/allocation failure
     */
    HRenderFrame NewRenderFrame(const RenderFrameLimits& limits);

    /**
     * Release remaining references and delete storage on the game owner, after all CPU readers finish.
     * @param frame Frame storage, or null
     */
    void DeleteRenderFrame(HRenderFrame frame);

    /**
     * Begin capture using empty, error-free storage. Call ResetRenderFrame before reuse.
     * @param frame Storage exclusively owned by the producer
     * @param frame_id Identifier assigned to this capture
     */
    void BeginRenderFrame(HRenderFrame frame, RenderFrameId frame_id);

    /**
     * Seal a Building frame for immutable reading. Publication/synchronization belongs to the queue.
     * @param frame Building storage; no reader may access it before publication
     * @return RENDER_FRAME_OK on success, otherwise the first sticky capture error
     */
    RenderFrameResult SealRenderFrame(HRenderFrame frame);

    /**
     * Cancel or retire capture on the game owner after all CPU readers finish. Releases references
     * and clears counters/errors, keeping the allocation and leaving payload bytes uncleared.
     * @param frame Exclusively owned storage; must not be inside a release callback
     */
    void ResetRenderFrame(HRenderFrame frame);

    /**
     * Record a higher-level extraction failure so a partial capture cannot be published.
     * @param frame Building storage
     * @param error Non-OK failure; never replaces an earlier capture error
     * @return The first sticky capture error
     */
    RenderFrameResult FailRenderFrame(HRenderFrame frame, RenderFrameResult error);

    /**
     * Reserve an owned table without copying. Initialize every element before sealing; do not
     * retain mutable access after publication. Errors are sticky until ResetRenderFrame.
     * @param frame Building storage
     * @param type Caller-defined table type
     * @param count Nonzero element count; an empty frame contains no tables
     * @param stride Nonzero bytes per element
     * @param alignment Power-of-two alignment, from 1 through 16 bytes
     * @param table_index Output index, assigned only on success
     * @param data Output payload pointer, assigned only on success
     * @return RENDER_FRAME_OK, or a sticky argument/state/capacity error
     */
    RenderFrameResult AllocateRenderFrameTable(HRenderFrame frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data);

    /**
     * Reserve a table and copy its input bytes. The source remains caller-owned and may be
     * released after return. Errors are sticky until ResetRenderFrame.
     * @param frame Building storage
     * @param type Caller-defined table type
     * @param count Nonzero element count
     * @param stride Nonzero bytes per element
     * @param alignment Power-of-two alignment, from 1 through 16 bytes
     * @param data Nonnull source containing count * stride bytes
     * @param table_index Output index, assigned only on success
     * @return RENDER_FRAME_OK, or a sticky argument/state/capacity error
     */
    RenderFrameResult CopyRenderFrameTable(HRenderFrame frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, const void* data, uint32_t* table_index);

    /**
     * Adopt an already-retained reference, released on game-owner cancellation/retirement.
     * Ownership transfers only on success; the caller releases rejected references.
     * @param frame Building storage
     * @param resource Nonnull retained dependency
     * @param release Nonnull release callback, valid until the reference is released
     * @param context Release callback context, valid until the reference is released
     * @return RENDER_FRAME_OK, or a sticky argument/state/capacity error
     */
    RenderFrameResult AddRenderFrameReference(HRenderFrame frame, void* resource, FRenderFrameRelease release, void* context);

    /**
     * Access immutable published inputs. Readers must not retain the view or its pointers after
     * consumption; uploads lasting longer must copy/transfer bytes into independent storage.
     * @param frame Storage to inspect; protect its CPU lifetime before using a returned view
     * @return Borrowed view of sealed storage, or null if not sealed
     */
    const RenderFrameView* GetRenderFrameView(HRenderFrame frame);

    /**
     * Look up table payload in an immutable view.
     * @param frame Published view with a protected CPU lifetime
     * @param table_index Valid index below frame.m_Statistics.m_TableCount
     * @return Borrowed data, valid for the same lifetime as the view
     */
    const void* GetRenderFrameTableData(const RenderFrameView& frame, uint32_t table_index);

    /**
     * Read capture counters without changing storage.
     * @param frame Storage owned by the caller, or whose capture/retirement cannot run concurrently
     * @return Current counters; allocation count measures arena reservations, not heap allocations
     */
    RenderFrameStatistics GetRenderFrameStatistics(HRenderFrame frame);

    /**
     * Calculate the fixed storage budget using wide arithmetic, without allocating.
     * @param limits Requested per-slot capacities
     * @return Payload, table/reference capacity and private metadata bytes, excluding allocator overhead
     */
    uint64_t GetRenderFrameStorageSize(const RenderFrameLimits& limits);
}

#endif // DM_RENDER_FRAME_H
