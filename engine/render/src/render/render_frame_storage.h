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

#ifndef DM_RENDER_FRAME_STORAGE_H
#define DM_RENDER_FRAME_STORAGE_H

#include <stdint.h>

namespace dmRender
{
    struct RenderFrameStorage;

    typedef uint64_t            RenderFrameId;
    typedef RenderFrameStorage* HRenderFrameStorage;

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
     * Hard per-slot capacities for the fixed arena, tables and references, allocated once.
     * Dynamic tables use separately allocated, reusable blocks while sharing the table limit.
     * All payload alignment is at most 16 bytes.
     */
    struct RenderFrameLimits
    {
        uint32_t m_PayloadBytes;
        uint32_t m_MaxTables;
        uint32_t m_MaxReferences;
    };

    /** Typed, contiguous input range owned by fixed or dynamic frame storage. */
    struct RenderFrameTable
    {
        const void* m_Data;
        uint32_t    m_Type;
        uint32_t    m_Count;
        uint32_t    m_Stride;
    };

    /**
     * Capture counters without per-operation clocks. AllocationCount counts all table reservations.
     * PayloadBytes and DynamicPayloadBytes include alignment padding within their storage.
     * DynamicPayloadCapacity counts retained block payload bytes and survives reset;
     * DynamicAllocationCount counts new blocks allocated during this capture, not table reservations.
     */
    struct RenderFrameStatistics
    {
        uint64_t m_DynamicPayloadBytes;
        uint64_t m_DynamicPayloadCapacity;
        uint64_t m_CopiedBytes;
        uint32_t m_PayloadBytes;
        uint32_t m_AllocationCount;
        uint32_t m_TableCount;
        uint32_t m_ReferenceCount;
        uint32_t m_DynamicAllocationCount;
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

    /**
     * Allocate the fixed arena and metadata once. Dynamic blocks are allocated on demand
     * during capture and retained for reuse until deletion.
     * @param limits Hard payload, table and retained-reference capacities
     * @return Empty frame storage, or null for unsupported capacities/allocation failure
     */
    HRenderFrameStorage NewRenderFrame(const RenderFrameLimits& limits);

    /**
     * Release remaining references and delete storage on the game owner, after all CPU readers finish.
     * @param frame Frame storage, or null
     */
    void DeleteRenderFrame(HRenderFrameStorage frame);

    /**
     * Begin capture using empty, error-free storage. Call ResetRenderFrame before reuse.
     * @param frame Storage exclusively owned by the producer
     * @param frame_id Identifier assigned to this capture
     */
    void BeginRenderFrame(HRenderFrameStorage frame, RenderFrameId frame_id);

    /**
     * Seal a Building frame for immutable reading. Publication/synchronization belongs to the queue.
     * @param frame Building storage; no reader may access it before publication
     * @return RENDER_FRAME_OK on success, otherwise the first sticky capture error
     */
    RenderFrameResult SealRenderFrame(HRenderFrameStorage frame);

    /**
     * Cancel or retire capture on the game owner after all CPU readers finish. Releases references
     * and clears capture counters/errors, retaining all storage and its dynamic capacity.
     * Payload bytes are left uncleared.
     * @param frame Exclusively owned storage; must not be inside a release callback
     */
    void ResetRenderFrame(HRenderFrameStorage frame);

    /**
     * Record a higher-level extraction failure so a partial capture cannot be published.
     * @param frame Building storage
     * @param error Non-OK failure; never replaces an earlier capture error
     * @return The first sticky capture error
     */
    RenderFrameResult FailRenderFrame(HRenderFrameStorage frame, RenderFrameResult error);

    /**
     * Reserve an owned table in the fixed arena. Initialize every element before sealing; do not
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
    RenderFrameResult AllocateRenderFrameTable(HRenderFrameStorage frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data);

    /**
     * Reserve a contiguous table in reusable dynamic storage. Growth never moves existing
     * payloads and is allowed only while building; cancellation and retirement retain blocks.
     * Initialize every element before sealing. Errors are sticky until ResetRenderFrame.
     * @param frame Building storage
     * @param type Caller-defined table type
     * @param count Nonzero element count
     * @param stride Nonzero bytes per element; count * stride must fit uint32_t
     * @param alignment Power-of-two alignment, from 1 through 16 bytes
     * @param table_index Output index, assigned only on success
     * @param data Output payload pointer, assigned only on success
     * @return RENDER_FRAME_OK, or a sticky argument/state/capacity/allocation error
     */
    RenderFrameResult AllocateRenderFrameDynamicTable(HRenderFrameStorage frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, uint32_t* table_index, void** data);

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
     * @param copied_data Optional output pointer to the copied table while the frame is building
     * @return RENDER_FRAME_OK, or a sticky argument/state/capacity error
     */
    RenderFrameResult CopyRenderFrameTable(HRenderFrameStorage frame, uint32_t type, uint32_t count, uint32_t stride, uint32_t alignment, const void* data, uint32_t* table_index, void** copied_data = 0);

    /**
     * Copy into an already reserved table and include those bytes in capture statistics.
     * @param frame Building storage
     * @param destination Caller-owned range within an allocated frame table
     * @param source Input bytes; may be null when bytes is zero
     * @param bytes Number of bytes to copy, within the reserved destination range
     * @return RENDER_FRAME_OK, or a sticky state error
     */
    RenderFrameResult CopyRenderFrameData(HRenderFrameStorage frame, void* destination, const void* source, uint32_t bytes);

    /**
     * Publish only the initialized prefix of a reserved table, without moving payloads.
     * Does not reclaim reserved bytes or a table slot. Count may be zero.
     * @param frame Building storage
     * @param table_index Previously allocated table
     * @param count Element count no greater than the table's current count
     * @return RENDER_FRAME_OK, or a sticky argument/state error
     */
    RenderFrameResult TrimRenderFrameTable(HRenderFrameStorage frame, uint32_t table_index, uint32_t count);

    /**
     * Adopt an already-retained reference, released on game-owner cancellation/retirement.
     * Ownership transfers only on success; the caller releases rejected references.
     * @param frame Building storage
     * @param resource Nonnull retained dependency
     * @param release Nonnull release callback, valid until the reference is released
     * @param context Release callback context, valid until the reference is released
     * @return RENDER_FRAME_OK, or a sticky argument/state/capacity error
     */
    RenderFrameResult AddRenderFrameReference(HRenderFrameStorage frame, void* resource, FRenderFrameRelease release, void* context);

    /**
     * Access immutable published inputs. Readers must not retain the view or its pointers after
     * consumption; uploads lasting longer must copy/transfer bytes into independent storage.
     * @param frame Storage to inspect; protect its CPU lifetime before using a returned view
     * @return Borrowed view of sealed storage, or null if not sealed
     */
    const RenderFrameView* GetRenderFrameView(HRenderFrameStorage frame);

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
    RenderFrameStatistics GetRenderFrameStatistics(HRenderFrameStorage frame);

    /**
     * Calculate initial fixed storage using wide arithmetic, without allocating.
     * @param limits Requested per-slot capacities
     * @return Fixed payload, table/reference capacity and private metadata bytes, excluding
     *         allocator overhead and any dynamic blocks allocated during later captures
     */
    uint64_t GetRenderFrameStorageSize(const RenderFrameLimits& limits);
}

#endif // DM_RENDER_FRAME_STORAGE_H
