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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <string.h>

#include <testmain/testmain.h>

#include "render/render_frame.h"

using namespace dmRender;

struct ReleaseLog
{
    uint32_t m_Values[8];
    uint32_t m_Count;
};

static void ReleaseReference(void* context, void* resource)
{
    ReleaseLog* log = (ReleaseLog*)context;
    log->m_Values[log->m_Count++] = *(uint32_t*)resource;
}

// Verify empty frames require no payload/tables and still follow begin/seal/reset correctly.
TEST(RenderFrame, EmptyFrame)
{
    RenderFrameLimits limits = {};
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
    BeginRenderFrame(frame, 123);
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    ASSERT_NE((const RenderFrameView*)0, view);
    ASSERT_EQ((RenderFrameId)123, view->m_FrameId);
    ASSERT_EQ((const uint8_t*)0, view->m_Payload);
    ASSERT_EQ((const RenderFrameTable*)0, view->m_Tables);
    ASSERT_EQ(0u, view->m_Statistics.m_PayloadBytes);
    ASSERT_EQ(0u, view->m_Statistics.m_CopiedBytes);
    ASSERT_EQ(0u, view->m_Statistics.m_AllocationCount);
    ASSERT_EQ(0u, view->m_Statistics.m_TableCount);
    ASSERT_EQ(0u, view->m_Statistics.m_ReferenceCount);
    ResetRenderFrame(frame);
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
    BeginRenderFrame(frame, 124);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    ASSERT_EQ((RenderFrameId)124, GetRenderFrameView(frame)->m_FrameId);
    DeleteRenderFrame(frame);
}

// Verify copied input owns its bytes while direct capture avoids a copy and exposes typed table metadata.
TEST(RenderFrame, CopiedAndDirectTables)
{
    RenderFrameLimits limits = { 32, 2, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 17);
    uint32_t input[] = { 10, 20, 30 };
    uint32_t copied_index = UINT32_MAX;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 101, 3, sizeof(uint32_t), 4, input, &copied_index));
    ASSERT_EQ(0u, copied_index);
    uint32_t direct_index = UINT32_MAX;
    void* direct_data = 0;
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 102, 4, sizeof(uint32_t), 16, &direct_index, &direct_data));
    ASSERT_EQ(1u, direct_index);
    ASSERT_EQ((uintptr_t)0, (uintptr_t)direct_data % 16);
    uint32_t* values = (uint32_t*)direct_data;
    for (uint32_t i = 0; i < 4; ++i)
        values[i] = i + 40;
    memset(input, 0, sizeof(input));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    ASSERT_NE((const RenderFrameView*)0, view);
    const uint32_t* copy = (const uint32_t*)GetRenderFrameTableData(*view, copied_index);
    ASSERT_EQ(10u, copy[0]);
    ASSERT_EQ(20u, copy[1]);
    ASSERT_EQ(30u, copy[2]);
    const uint32_t* direct = (const uint32_t*)GetRenderFrameTableData(*view, direct_index);
    ASSERT_EQ((const void*)direct_data, (const void*)direct);
    for (uint32_t i = 0; i < 4; ++i)
        ASSERT_EQ(i + 40, direct[i]);
    ASSERT_EQ(101u, view->m_Tables[0].m_Type);
    ASSERT_EQ(0u, view->m_Tables[0].m_Offset);
    ASSERT_EQ(3u, view->m_Tables[0].m_Count);
    ASSERT_EQ((uint32_t)sizeof(uint32_t), view->m_Tables[0].m_Stride);
    ASSERT_EQ(102u, view->m_Tables[1].m_Type);
    ASSERT_EQ(16u, view->m_Tables[1].m_Offset);
    ASSERT_EQ(4u, view->m_Tables[1].m_Count);
    ASSERT_EQ(32u, view->m_Statistics.m_PayloadBytes);
    ASSERT_EQ(12u, view->m_Statistics.m_CopiedBytes);
    ASSERT_EQ(2u, view->m_Statistics.m_AllocationCount);
    ASSERT_EQ(2u, view->m_Statistics.m_TableCount);
    DeleteRenderFrame(frame);
}

// Verify each supported alignment and its padding are reflected in the fixed payload budget.
TEST(RenderFrame, Alignment)
{
    RenderFrameLimits limits = { 17, 5, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    const uint32_t expected_offsets[] = { 0, 2, 4, 8, 16 };
    for (uint32_t i = 0; i < 5; ++i)
    {
        uint32_t index;
        void* data;
        const uint32_t alignment = 1u << i;
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, i, 1, 1, alignment, &index, &data));
        ASSERT_EQ(i, index);
        ASSERT_EQ((uintptr_t)0, (uintptr_t)data % alignment);
        *(uint8_t*)data = (uint8_t)i;
    }
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    ASSERT_EQ(17u, view->m_Statistics.m_PayloadBytes);
    ASSERT_EQ(0u, view->m_Statistics.m_CopiedBytes);
    for (uint32_t i = 0; i < 5; ++i)
    {
        ASSERT_EQ(expected_offsets[i], view->m_Tables[i].m_Offset);
        ASSERT_EQ((uint8_t)i, *(const uint8_t*)GetRenderFrameTableData(*view, i));
    }
    DeleteRenderFrame(frame);
}

// Verify an exhausted arena cannot grow or publish a partial capture, and failed outputs stay unchanged.
TEST(RenderFrame, PayloadOverflowIsSticky)
{
    RenderFrameLimits limits = { 4, 3, 1 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    uint32_t value = 7;
    uint32_t index;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, &value, &index));
    index = 123;
    void* data = &value;
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &index, &data));
    ASSERT_EQ(123u, index);
    ASSERT_EQ((void*)&value, data);
    RenderFrameStatistics statistics = GetRenderFrameStatistics(frame);
    ASSERT_EQ(4u, statistics.m_PayloadBytes);
    ASSERT_EQ(4u, statistics.m_CopiedBytes);
    ASSERT_EQ(1u, statistics.m_AllocationCount);
    ASSERT_EQ(1u, statistics.m_TableCount);
    ReleaseLog releases = {};
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AddRenderFrameReference(frame, &value, ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, CopyRenderFrameTable(frame, 0, 1, 1, 1, 0, &index));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
    ResetRenderFrame(frame);
    ASSERT_EQ(0u, releases.m_Count);
    BeginRenderFrame(frame, 2);
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, &value, &index));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
}

// Verify alignment padding cannot take a capture beyond its declared payload capacity.
TEST(RenderFrame, PaddingOverflow)
{
    RenderFrameLimits limits = { 17, 2, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    uint32_t index;
    void* data;
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &index, &data));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameTable(frame, 0, 2, 1, 16, &index, &data));
    ASSERT_EQ(1u, GetRenderFrameStatistics(frame).m_PayloadBytes);
    ASSERT_EQ(1u, GetRenderFrameStatistics(frame).m_TableCount);
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
}

// Verify count/stride multiplication uses wide arithmetic instead of wrapping into a small allocation.
TEST(RenderFrame, CountStrideOverflow)
{
    RenderFrameLimits limits = { 32, 1, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    const uint32_t counts[] = { UINT32_MAX, UINT32_MAX, 0x80000001u };
    const uint32_t strides[] = { UINT32_MAX, 2, 2 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        BeginRenderFrame(frame, i);
        uint32_t index = 123;
        void* data = &index;
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameTable(frame, 0, counts[i], strides[i], 16, &index, &data));
        ASSERT_EQ(123u, index);
        ASSERT_EQ((void*)&index, data);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_PayloadBytes);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_AllocationCount);
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
        ResetRenderFrame(frame);
    }
    DeleteRenderFrame(frame);
}

// Verify invalid capture arguments poison only the current frame without modifying its outputs.
TEST(RenderFrame, InvalidTableArguments)
{
    struct InvalidArguments
    {
        uint32_t m_Count;
        uint32_t m_Stride;
        uint32_t m_Alignment;
        bool m_NullIndex;
        bool m_NullData;
    };
    const InvalidArguments invalid[] = {
        { 0, 1, 1, false, false },
        { 1, 0, 1, false, false },
        { 1, 1, 0, false, false },
        { 1, 1, 3, false, false },
        { 1, 1, 32, false, false },
        { 1, 1, UINT32_MAX, false, false },
        { 1, 1, 1, true, false },
        { 1, 1, 1, false, true },
    };
    RenderFrameLimits limits = { 32, 1, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    for (uint32_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
    {
        BeginRenderFrame(frame, i);
        uint32_t index = 123;
        void* data = &index;
        ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, AllocateRenderFrameTable(frame, 0,
            invalid[i].m_Count, invalid[i].m_Stride, invalid[i].m_Alignment,
            invalid[i].m_NullIndex ? 0 : &index, invalid[i].m_NullData ? 0 : &data));
        ASSERT_EQ(123u, index);
        ASSERT_EQ((void*)&index, data);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_AllocationCount);
        ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &index, &data));
        ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, SealRenderFrame(frame));
        ResetRenderFrame(frame);
    }
    BeginRenderFrame(frame, 10);
    uint32_t index = 123;
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, CopyRenderFrameTable(frame, 0, 1, 1, 1, 0, &index));
    ASSERT_EQ(123u, index);
    ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_CopiedBytes);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
}

// Verify table capacity is independent of free payload space and cannot silently reallocate metadata.
TEST(RenderFrame, TableOverflow)
{
    RenderFrameLimits limits = { 32, 1, 0 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    uint32_t index;
    void* data;
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &index, &data));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &index, &data));
    ASSERT_EQ(1u, GetRenderFrameStatistics(frame).m_PayloadBytes);
    ASSERT_EQ(1u, GetRenderFrameStatistics(frame).m_TableCount);
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
}

// Verify a full reference table adopts only successful insertions and rolls back in reverse order exactly once.
TEST(RenderFrame, ReferenceOverflowAndCleanup)
{
    RenderFrameLimits limits = { 0, 0, 2 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    uint32_t resources[] = { 10, 20, 30 };
    ReleaseLog releases = {};
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resources[0], ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resources[1], ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AddRenderFrameReference(frame, &resources[2], ReleaseReference, &releases));
    ASSERT_EQ(2u, GetRenderFrameStatistics(frame).m_ReferenceCount);
    ASSERT_EQ(0u, releases.m_Count);
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
    ResetRenderFrame(frame);
    ASSERT_EQ(2u, releases.m_Count);
    ASSERT_EQ(20u, releases.m_Values[0]);
    ASSERT_EQ(10u, releases.m_Values[1]);
    ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_ReferenceCount);
    ResetRenderFrame(frame);
    DeleteRenderFrame(frame);
    ASSERT_EQ(2u, releases.m_Count);
    // Ownership of the rejected insertion never left the caller.
    ReleaseReference(&releases, &resources[2]);
    ASSERT_EQ(3u, releases.m_Count);
    ASSERT_EQ(30u, releases.m_Values[2]);
}

// Verify sealing does not release dependencies: they remain retained until consumer retirement resets storage.
TEST(RenderFrame, SealedReferenceLifetime)
{
    RenderFrameLimits limits = { 0, 0, 1 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    uint32_t resource = 42;
    ReleaseLog releases = {};
    BeginRenderFrame(frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resource, ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    ASSERT_EQ(1u, GetRenderFrameView(frame)->m_Statistics.m_ReferenceCount);
    ASSERT_EQ(0u, releases.m_Count);
    ResetRenderFrame(frame);
    ASSERT_EQ(1u, releases.m_Count);
    ASSERT_EQ(42u, releases.m_Values[0]);
    BeginRenderFrame(frame, 2);
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resource, ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
    ASSERT_EQ(2u, releases.m_Count);
}

// Verify deletion of a failed partial capture releases adopted dependencies even if no view was published.
TEST(RenderFrame, DeletePartialCapture)
{
    RenderFrameLimits limits = { 1, 1, 2 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    uint32_t resources[] = { 10, 20 };
    ReleaseLog releases = {};
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resources[0], ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resources[1], ReleaseReference, &releases));
    uint32_t index;
    ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, CopyRenderFrameTable(frame, 0, 1, sizeof(uint32_t), 4, resources, &index));
    DeleteRenderFrame(frame);
    ASSERT_EQ(2u, releases.m_Count);
    ASSERT_EQ(20u, releases.m_Values[0]);
    ASSERT_EQ(10u, releases.m_Values[1]);
    DeleteRenderFrame(0);
}

// Verify invalid reference insertions do not adopt resources or permit a partial frame to be sealed.
TEST(RenderFrame, InvalidReferenceArguments)
{
    RenderFrameLimits limits = { 0, 0, 1 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    uint32_t resource = 42;
    ReleaseLog releases = {};
    BeginRenderFrame(frame, 1);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, AddRenderFrameReference(frame, &resource, 0, &releases));
    ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_ReferenceCount);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, SealRenderFrame(frame));
    ResetRenderFrame(frame);
    BeginRenderFrame(frame, 2);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, AddRenderFrameReference(frame, 0, ReleaseReference, &releases));
    ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_ReferenceCount);
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
    ASSERT_EQ(0u, releases.m_Count);
}

// Verify capture is building-only and rejected post-seal writes do not alter the immutable consumer view.
TEST(RenderFrame, CaptureState)
{
    RenderFrameLimits limits = { 4, 1, 1 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    uint32_t index = 123;
    uint32_t value = 42;
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, &value, &index));
    ASSERT_EQ(123u, index);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, SealRenderFrame(frame));
    ResetRenderFrame(frame);
    BeginRenderFrame(frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, &value, &index));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    RenderFrameStatistics before = view->m_Statistics;
    value = 99;
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, &value, &index));
    ReleaseLog releases = {};
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, AddRenderFrameReference(frame, &value, ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, SealRenderFrame(frame));
    ASSERT_EQ(view, GetRenderFrameView(frame));
    ASSERT_EQ(0, memcmp(&before, &view->m_Statistics, sizeof(before)));
    ASSERT_EQ(42u, *(const uint32_t*)GetRenderFrameTableData(*view, 0));
    DeleteRenderFrame(frame);
    ASSERT_EQ(0u, releases.m_Count);
}

// Verify fixed allocation budgets use wide arithmetic and reject capacities that cannot fit allocator sizes.
TEST(RenderFrame, StorageBudget)
{
    RenderFrameLimits empty = {};
    RenderFrameLimits payload = { 100, 0, 0 };
    RenderFrameLimits tables = { 0, 3, 0 };
    RenderFrameLimits reference = { 0, 0, 1 };
    RenderFrameLimits combined = { 100, 3, 4 };
    const uint64_t overhead = GetRenderFrameStorageSize(empty);
    const uint64_t reference_size = GetRenderFrameStorageSize(reference) - overhead;
    ASSERT_GT(overhead, (uint64_t)0);
    ASSERT_EQ(overhead + 100, GetRenderFrameStorageSize(payload));
    ASSERT_EQ(overhead + 3 * sizeof(RenderFrameTable), GetRenderFrameStorageSize(tables));
    ASSERT_GE(reference_size, (uint64_t)(2 * sizeof(void*) + sizeof(RenderFrameRelease)));
    ASSERT_EQ(overhead + 100 + 3 * sizeof(RenderFrameTable) + 4 * reference_size, GetRenderFrameStorageSize(combined));
    RenderFrameLimits oversized_tables = { 0, UINT32_MAX, 0 };
    RenderFrameLimits oversized_references = { 0, 0, UINT32_MAX };
    ASSERT_GT(GetRenderFrameStorageSize(oversized_tables), (uint64_t)UINT32_MAX);
    ASSERT_GT(GetRenderFrameStorageSize(oversized_references), (uint64_t)UINT32_MAX);
    ASSERT_EQ((HRenderFrame)0, NewRenderFrame(oversized_tables));
    ASSERT_EQ((HRenderFrame)0, NewRenderFrame(oversized_references));
}

// Verify reset reuses the original payload/table allocations and clears per-capture counters rather than growing storage.
TEST(RenderFrame, RepeatedReuse)
{
    RenderFrameLimits limits = { 16, 1, 1 };
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    const uint8_t* payload = 0;
    const RenderFrameTable* tables = 0;
    const uint64_t budget = GetRenderFrameStorageSize(limits);
    for (uint32_t i = 0; i < 64; ++i)
    {
        RenderFrameStatistics statistics = GetRenderFrameStatistics(frame);
        ASSERT_EQ(0u, statistics.m_PayloadBytes);
        ASSERT_EQ(0u, statistics.m_CopiedBytes);
        ASSERT_EQ(0u, statistics.m_AllocationCount);
        ASSERT_EQ(0u, statistics.m_TableCount);
        ASSERT_EQ(0u, statistics.m_ReferenceCount);
        BeginRenderFrame(frame, i + 1);
        uint32_t index;
        uint32_t input[] = { i, i + 1, i + 2, i + 3 };
        ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 0, 4, sizeof(uint32_t), 16, input, &index));
        ReleaseLog releases = {};
        ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &input[0], ReleaseReference, &releases));
        ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
        const RenderFrameView* view = GetRenderFrameView(frame);
        if (!i)
        {
            payload = view->m_Payload;
            tables = view->m_Tables;
        }
        ASSERT_EQ(payload, view->m_Payload);
        ASSERT_EQ(tables, view->m_Tables);
        ASSERT_EQ(budget, GetRenderFrameStorageSize(limits));
        ASSERT_EQ((RenderFrameId)i + 1, view->m_FrameId);
        ASSERT_EQ(i, *(const uint32_t*)GetRenderFrameTableData(*view, 0));
        ASSERT_EQ(1u, view->m_Statistics.m_AllocationCount);
        ASSERT_EQ(16u, view->m_Statistics.m_CopiedBytes);
        ResetRenderFrame(frame);
        ASSERT_EQ(1u, releases.m_Count);
        ASSERT_EQ(i, releases.m_Values[0]);
    }
    DeleteRenderFrame(frame);
}

#if !defined(JC_TEST_NO_DEATH_TEST) && !defined(NDEBUG)
// Verify begin cannot overwrite a building or sealed frame before ownership has returned through reset.
TEST(RenderFrame, BeginRequiresReset)
{
    RenderFrameLimits limits = {};
    HRenderFrame frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrame)0, frame);
    BeginRenderFrame(frame, 1);
    ASSERT_DEATH(BeginRenderFrame(frame, 2), "");
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    ASSERT_DEATH(BeginRenderFrame(frame, 2), "");
    ResetRenderFrame(frame);
    BeginRenderFrame(frame, 2);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
}
#endif

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
