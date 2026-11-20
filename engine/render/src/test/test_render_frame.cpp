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
#include <stdlib.h>

#include <dlib/memory.h>

#include <testmain/testmain.h>

#include "render/render_frame_storage.h"

using namespace dmRender;

enum FrameAllocationFailure
{
    FRAME_ALLOCATION_FAILURE_NONE,
    FRAME_ALLOCATION_FAILURE_DESCRIPTOR,
    FRAME_ALLOCATION_FAILURE_PAYLOAD,
};

static FrameAllocationFailure g_FrameAllocationFailure = FRAME_ALLOCATION_FAILURE_NONE;
static uint32_t g_LastPayloadAllocationSize = 0;

// Only the test's separately compiled storage implementation uses these wrappers.
extern "C" void* RenderFrameTestMalloc(size_t size)
{
    if (g_FrameAllocationFailure == FRAME_ALLOCATION_FAILURE_DESCRIPTOR)
    {
        g_FrameAllocationFailure = FRAME_ALLOCATION_FAILURE_NONE;
        return 0;
    }
    return malloc(size);
}

namespace dmMemory
{
    Result RenderFrameTestAlignedMalloc(void** memory, unsigned int alignment, unsigned int size)
    {
        g_LastPayloadAllocationSize = size;
        if (g_FrameAllocationFailure == FRAME_ALLOCATION_FAILURE_PAYLOAD)
        {
            g_FrameAllocationFailure = FRAME_ALLOCATION_FAILURE_NONE;
            return RESULT_NOMEM;
        }
        return AlignedMalloc(memory, alignment, size);
    }
}

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

static void AssertStatistics(const RenderFrameStatistics& expected, const RenderFrameStatistics& actual)
{
    ASSERT_EQ(expected.m_DynamicPayloadBytes, actual.m_DynamicPayloadBytes);
    ASSERT_EQ(expected.m_DynamicPayloadCapacity, actual.m_DynamicPayloadCapacity);
    ASSERT_EQ(expected.m_CopiedBytes, actual.m_CopiedBytes);
    ASSERT_EQ(expected.m_PayloadBytes, actual.m_PayloadBytes);
    ASSERT_EQ(expected.m_AllocationCount, actual.m_AllocationCount);
    ASSERT_EQ(expected.m_TableCount, actual.m_TableCount);
    ASSERT_EQ(expected.m_ReferenceCount, actual.m_ReferenceCount);
    ASSERT_EQ(expected.m_DynamicAllocationCount, actual.m_DynamicAllocationCount);
}

// Verify empty frames contain no payload/tables and still follow begin/seal/reset correctly.
// Check the table count, not its pointer: malloc(0) may return a non-null pointer.
TEST(RenderFrame, EmptyFrame)
{
    RenderFrameLimits limits = {};
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
    BeginRenderFrame(frame, 123);
    ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    ASSERT_NE((const RenderFrameView*)0, view);
    ASSERT_EQ((RenderFrameId)123, view->m_FrameId);
    ASSERT_EQ((const uint8_t*)0, view->m_Payload);
    ASSERT_EQ(0u, view->m_Statistics.m_PayloadBytes);
    ASSERT_EQ(0u, view->m_Statistics.m_CopiedBytes);
    ASSERT_EQ(0u, view->m_Statistics.m_AllocationCount);
    ASSERT_EQ(0u, view->m_Statistics.m_TableCount);
    ASSERT_EQ(0u, view->m_Statistics.m_ReferenceCount);
    ASSERT_EQ((uint64_t)0, view->m_Statistics.m_DynamicPayloadBytes);
    ASSERT_EQ((uint64_t)0, view->m_Statistics.m_DynamicPayloadCapacity);
    ASSERT_EQ(0u, view->m_Statistics.m_DynamicAllocationCount);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    {
        ASSERT_EQ(i + 40, direct[i]);
    }
    ASSERT_EQ(101u, view->m_Tables[0].m_Type);
    ASSERT_EQ(0u, (uint32_t)((const uint8_t*)view->m_Tables[0].m_Data - view->m_Payload));
    ASSERT_EQ(3u, view->m_Tables[0].m_Count);
    ASSERT_EQ((uint32_t)sizeof(uint32_t), view->m_Tables[0].m_Stride);
    ASSERT_EQ(102u, view->m_Tables[1].m_Type);
    ASSERT_EQ(16u, (uint32_t)((const uint8_t*)view->m_Tables[1].m_Data - view->m_Payload));
    ASSERT_EQ(4u, view->m_Tables[1].m_Count);
    ASSERT_EQ(32u, view->m_Statistics.m_PayloadBytes);
    ASSERT_EQ(12u, view->m_Statistics.m_CopiedBytes);
    ASSERT_EQ(2u, view->m_Statistics.m_AllocationCount);
    ASSERT_EQ(2u, view->m_Statistics.m_TableCount);
    DeleteRenderFrame(frame);
}

// Verify compound tables can account for copied subranges without adding more table entries.
TEST(RenderFrame, CompoundTableCopies)
{
    RenderFrameLimits limits = { 16, 1, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    BeginRenderFrame(frame, 1);
    uint32_t input[] = { 10, 20 };
    uint32_t table_index;
    void* destination;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 101, 2, sizeof(uint32_t), 4, input, &table_index, &destination));
    uint32_t replacement = 30;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameData(frame, destination, &replacement, sizeof(replacement)));
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameData(frame, destination, 0, 0));
    ASSERT_EQ(12u, GetRenderFrameStatistics(frame).m_CopiedBytes);
    ASSERT_EQ(1u, GetRenderFrameStatistics(frame).m_TableCount);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));

    const uint32_t* values = (const uint32_t*)GetRenderFrameTableData(*GetRenderFrameView(frame), table_index);
    ASSERT_EQ(30u, values[0]);
    ASSERT_EQ(20u, values[1]);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CopyRenderFrameData(frame, destination, input, sizeof(input)));
    ASSERT_EQ(30u, values[0]);
    DeleteRenderFrame(frame);
}

// Verify each supported alignment and its padding are reflected in the fixed payload budget.
TEST(RenderFrame, Alignment)
{
    RenderFrameLimits limits = { 17, 5, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
        ASSERT_EQ(expected_offsets[i], (uint32_t)((const uint8_t*)view->m_Tables[i].m_Data - view->m_Payload));
        ASSERT_EQ((uint8_t)i, *(const uint8_t*)GetRenderFrameTableData(*view, i));
    }
    DeleteRenderFrame(frame);
}

// Verify an exhausted arena cannot grow or publish a partial capture, and failed outputs stay unchanged.
TEST(RenderFrame, PayloadOverflowIsSticky)
{
    RenderFrameLimits limits = { 4, 3, 1 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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

// Verify dynamic growth preserves aligned table pointers and contents, then reuses all blocks after reset.
TEST(RenderFrame, DynamicGrowthPreservesPointersAndReusesStorage)
{
    RenderFrameLimits limits = { 0, 3, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
    void* previous[3] = {};
    const uint32_t sizes[] = { 1, 16, 128 * 1024 };
    const uint32_t alignments[] = { 1, 16, 16 };
    uint64_t retained_capacity = 0;
    for (uint32_t capture = 0; capture < 2; ++capture)
    {
        BeginRenderFrame(frame, capture);
        void* pointers[3];
        for (uint32_t i = 0; i < 3; ++i)
        {
            uint32_t index;
            ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, i, 1, sizes[i], alignments[i], &index, &pointers[i]));
            ASSERT_EQ(i, index);
            ASSERT_EQ((uintptr_t)0, (uintptr_t)pointers[i] % alignments[i]);
            memset(pointers[i], i + 1, sizes[i]);
            if (capture)
            {
                ASSERT_EQ(previous[i], pointers[i]);
            }
            previous[i] = pointers[i];
        }
        ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
        const RenderFrameView* view = GetRenderFrameView(frame);
        ASSERT_EQ((const uint8_t*)0, view->m_Payload);
        ASSERT_EQ(0u, view->m_Statistics.m_PayloadBytes);
        ASSERT_EQ((uint64_t)(32 + sizes[2]), view->m_Statistics.m_DynamicPayloadBytes);
        ASSERT_EQ(3u, view->m_Statistics.m_AllocationCount);
        if (!capture)
        {
            ASSERT_EQ(2u, view->m_Statistics.m_DynamicAllocationCount);
            retained_capacity = view->m_Statistics.m_DynamicPayloadCapacity;
        }
        else
        {
            ASSERT_EQ(0u, view->m_Statistics.m_DynamicAllocationCount);
            ASSERT_EQ(retained_capacity, view->m_Statistics.m_DynamicPayloadCapacity);
        }
        for (uint32_t i = 0; i < 3; ++i)
        {
            ASSERT_EQ((const void*)pointers[i], GetRenderFrameTableData(*view, i));
            const uint8_t* bytes = (const uint8_t*)pointers[i];
            for (uint32_t j = 0; j < sizes[i]; ++j)
            {
                ASSERT_EQ((uint8_t)(i + 1), bytes[j]);
            }
        }
        ResetRenderFrame(frame);
        ASSERT_EQ((uint64_t)0, GetRenderFrameStatistics(frame).m_DynamicPayloadBytes);
        ASSERT_EQ(retained_capacity, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_DynamicAllocationCount);
    }
    DeleteRenderFrame(frame);
}

// Verify small snapshot growth between captures reuses one larger size class instead of retaining a new block each time.
TEST(RenderFrame, DynamicSizeClassesReuseSmallGrowth)
{
    RenderFrameLimits limits = { 0, 1, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
    void* previous = 0;
    uint64_t retained_capacity = 0;
    for (uint32_t capture = 0; capture < 2; ++capture)
    {
        BeginRenderFrame(frame, capture);
        uint32_t index;
        void* data;
        const uint32_t bytes = 64 * 1024 + (capture + 1) * 16;
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, bytes, 16, &index, &data));
        memset(data, 42, bytes);
        if (!capture)
        {
            retained_capacity = GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity;
            ASSERT_GT(retained_capacity, (uint64_t)bytes);
            ASSERT_EQ(1u, GetRenderFrameStatistics(frame).m_DynamicAllocationCount);
            previous = data;
        }
        else
        {
            ASSERT_EQ(previous, data);
            ASSERT_EQ(retained_capacity, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
            ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_DynamicAllocationCount);
        }
        ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
        ResetRenderFrame(frame);
    }
    DeleteRenderFrame(frame);
}

// Verify size-class rounding saturates above 2 GiB without wrapping to a smaller allocation.
TEST(RenderFrame, DynamicSizeClassBoundaries)
{
    const uint32_t requested[] = { 1, 65536, 65537, 0x7fffffffu, 0x80000000u, 0x80000001u, UINT32_MAX };
    const uint32_t expected[] = { 65536, 65536, 131072, 0x80000000u, 0x80000000u, UINT32_MAX, UINT32_MAX };
    const RenderFrameLimits limits = { 0, 1, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    for (uint32_t i = 0; i < sizeof(requested) / sizeof(requested[0]); ++i)
    {
        BeginRenderFrame(frame, i);
        uint32_t index = 42;
        void* data = &index;
        // Inspect the requested size without making multi-gigabyte allocations.
        g_FrameAllocationFailure = FRAME_ALLOCATION_FAILURE_PAYLOAD;
        ASSERT_EQ(RENDER_FRAME_OUT_OF_MEMORY, AllocateRenderFrameDynamicTable(frame, 0, 1, requested[i], 16, &index, &data));
        ASSERT_EQ(expected[i], g_LastPayloadAllocationSize);
        ASSERT_EQ(42u, index);
        ASSERT_EQ((void*)&index, data);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_TableCount);
        ResetRenderFrame(frame);
    }
    DeleteRenderFrame(frame);
}

// Reserved command capacity must expose only the initialized prefix, without moving later operands.
TEST(RenderFrame, TrimReservedTable)
{
    const RenderFrameLimits limits = { 64, 2, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    BeginRenderFrame(frame, 1);
    uint32_t table, operand;
    void* commands;
    void* data;
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 1, 8, sizeof(uint32_t), 4, &table, &commands));
    ((uint32_t*)commands)[0] = 7;
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 2, 1, sizeof(uint32_t), 4, &operand, &data));
    *(uint32_t*)data = 9;
    ASSERT_EQ(RENDER_FRAME_OK, TrimRenderFrameTable(frame, table, 1));
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView& view = *GetRenderFrameView(frame);
    ASSERT_EQ(1u, view.m_Tables[table].m_Count);
    ASSERT_EQ(commands, view.m_Tables[table].m_Data);
    ASSERT_EQ(data, view.m_Tables[operand].m_Data);
    ASSERT_EQ(9u, *(const uint32_t*)view.m_Tables[operand].m_Data);
    ASSERT_EQ(36u, view.m_Statistics.m_PayloadBytes);
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, TrimRenderFrameTable(frame, table, 0));
    ResetRenderFrame(frame);
    BeginRenderFrame(frame, 2);
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 1, 8, sizeof(uint32_t), 4, &table, &commands));
    ASSERT_EQ(RENDER_FRAME_OK, TrimRenderFrameTable(frame, table, 0));
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, TrimRenderFrameTable(frame, table, 1));
    ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, SealRenderFrame(frame));
    DeleteRenderFrame(frame);
}

// Verify dynamic tables do not consume fixed payload and can grow after the fixed arena is full.
TEST(RenderFrame, FixedAndDynamicPayloadAreIndependent)
{
    RenderFrameLimits limits = { 4, 3, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
    BeginRenderFrame(frame, 1);
    uint32_t index;
    void* data;
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 16, 16, &index, &data));
    memset(data, 7, 16);
    uint32_t fixed_value = 42;
    ASSERT_EQ(RENDER_FRAME_OK, CopyRenderFrameTable(frame, 1, 1, sizeof(fixed_value), 4, &fixed_value, &index));
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 2, 1, 128 * 1024, 16, &index, &data));
    memset(data, 9, 128 * 1024);
    ASSERT_EQ(4u, GetRenderFrameStatistics(frame).m_PayloadBytes);
    ASSERT_EQ((uint64_t)(16 + 128 * 1024), GetRenderFrameStatistics(frame).m_DynamicPayloadBytes);
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    ASSERT_EQ((uint8_t)7, *(const uint8_t*)GetRenderFrameTableData(*view, 0));
    ASSERT_EQ(42u, *(const uint32_t*)GetRenderFrameTableData(*view, 1));
    ASSERT_EQ((uint8_t)9, *(const uint8_t*)GetRenderFrameTableData(*view, 2));
    DeleteRenderFrame(frame);
}

// Verify fixed and dynamic tables share the metadata limit and rejected growth is atomic and sticky.
TEST(RenderFrame, DynamicTableLimitIsAtomic)
{
    for (uint32_t dynamic_first = 0; dynamic_first < 2; ++dynamic_first)
    {
        RenderFrameLimits limits = { 1, 1, 0 };
        HRenderFrameStorage frame = NewRenderFrame(limits);
        ASSERT_NE((HRenderFrameStorage)0, frame);
        BeginRenderFrame(frame, 1);
        uint32_t first_index;
        void* first_data;
        if (dynamic_first)
        {
            ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 1, 1, &first_index, &first_data));
        }
        else
        {
            ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &first_index, &first_data));
        }
        uint32_t index = 123;
        void* data = &index;
        RenderFrameStatistics before = GetRenderFrameStatistics(frame);
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameDynamicTable(frame, 0, 1, 128 * 1024, 16, &index, &data));
        ASSERT_EQ(123u, index);
        ASSERT_EQ((void*)&index, data);
        AssertStatistics(before, GetRenderFrameStatistics(frame));
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameTable(frame, 0, 1, 1, 1, &index, &data));
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, SealRenderFrame(frame));
        ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
        ResetRenderFrame(frame);
        ASSERT_EQ(before.m_DynamicPayloadCapacity, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
        BeginRenderFrame(frame, 2);
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 1, 1, &index, &data));
        if (dynamic_first)
        {
            ASSERT_EQ(first_data, data);
            ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_DynamicAllocationCount);
        }
        *(uint8_t*)data = 42;
        ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
        DeleteRenderFrame(frame);
    }
}

// Verify dynamic payload cannot allocate before capture or mutate a published table after sealing.
TEST(RenderFrame, DynamicCaptureState)
{
    RenderFrameLimits limits = { 0, 2, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
    uint32_t index = 123;
    void* data = &index;
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, AllocateRenderFrameDynamicTable(frame, 0, 1, 1, 1, &index, &data));
    ASSERT_EQ(123u, index);
    ASSERT_EQ((void*)&index, data);
    ASSERT_EQ((uint64_t)0, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
    ResetRenderFrame(frame);
    BeginRenderFrame(frame, 1);
    ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 1, 1, &index, &data));
    *(uint8_t*)data = 42;
    ASSERT_EQ(RENDER_FRAME_OK, SealRenderFrame(frame));
    const RenderFrameView* view = GetRenderFrameView(frame);
    RenderFrameStatistics before = GetRenderFrameStatistics(frame);
    index = 123;
    data = &index;
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, AllocateRenderFrameDynamicTable(frame, 0, 1, 128 * 1024, 16, &index, &data));
    ASSERT_EQ(123u, index);
    ASSERT_EQ((void*)&index, data);
    AssertStatistics(before, GetRenderFrameStatistics(frame));
    ASSERT_EQ(view, GetRenderFrameView(frame));
    ASSERT_EQ((uint8_t)42, *(const uint8_t*)GetRenderFrameTableData(*view, 0));
    DeleteRenderFrame(frame);
}

// Verify descriptor and payload allocation failures reject the frame and preserve existing blocks for reuse.
TEST(RenderFrame, DynamicFailureCleanup)
{
    const FrameAllocationFailure failures[] = { FRAME_ALLOCATION_FAILURE_DESCRIPTOR, FRAME_ALLOCATION_FAILURE_PAYLOAD };
    for (uint32_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i)
    {
        RenderFrameLimits limits = { 0, 3, 1 };
        HRenderFrameStorage frame = NewRenderFrame(limits);
        ASSERT_NE((HRenderFrameStorage)0, frame);
        ReleaseLog releases = {};
        uint32_t resource = 42;
        BeginRenderFrame(frame, 1);
        uint32_t index;
        void* first;
        void* second;
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 1, 1, &index, &first));
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 128 * 1024, 16, &index, &second));
        ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resource, ReleaseReference, &releases));
        const uint64_t retained_capacity = GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity;
        // Force a new block, then fail one of its actual allocations.
        g_FrameAllocationFailure = failures[i];
        void* data = &index;
        ASSERT_EQ(RENDER_FRAME_OUT_OF_MEMORY, AllocateRenderFrameDynamicTable(frame, 0, 1, 256 * 1024, 16, &index, &data));
        ASSERT_EQ(FRAME_ALLOCATION_FAILURE_NONE, g_FrameAllocationFailure);
        ASSERT_EQ((void*)&index, data);
        ASSERT_EQ(2u, GetRenderFrameStatistics(frame).m_TableCount);
        ASSERT_EQ(retained_capacity, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
        ASSERT_EQ(RENDER_FRAME_OUT_OF_MEMORY, SealRenderFrame(frame));
        ASSERT_EQ((const RenderFrameView*)0, GetRenderFrameView(frame));
        ResetRenderFrame(frame);
        ASSERT_EQ(1u, releases.m_Count);
        ASSERT_EQ(retained_capacity, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
        BeginRenderFrame(frame, 2);
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 1, 1, &index, &data));
        ASSERT_EQ(first, data);
        ASSERT_EQ(RENDER_FRAME_OK, AllocateRenderFrameDynamicTable(frame, 0, 1, 128 * 1024, 16, &index, &data));
        ASSERT_EQ(second, data);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_DynamicAllocationCount);
        ASSERT_EQ(RENDER_FRAME_OK, AddRenderFrameReference(frame, &resource, ReleaseReference, &releases));
        DeleteRenderFrame(frame);
        ASSERT_EQ(2u, releases.m_Count);
    }
}

// Verify alignment padding cannot take a capture beyond its declared payload capacity.
TEST(RenderFrame, PaddingOverflow)
{
    RenderFrameLimits limits = { 17, 2, 0 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
        BeginRenderFrame(frame, i);
        ASSERT_EQ(RENDER_FRAME_CAPACITY_EXCEEDED, AllocateRenderFrameDynamicTable(frame, 0, counts[i], strides[i], 16, &index, &data));
        ASSERT_EQ(123u, index);
        ASSERT_EQ((void*)&index, data);
        ASSERT_EQ((uint64_t)0, GetRenderFrameStatistics(frame).m_DynamicPayloadBytes);
        ASSERT_EQ((uint64_t)0, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
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
        uint8_t m_NullIndex : 1;
        uint8_t m_NullData : 1;
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
        BeginRenderFrame(frame, i);
        ASSERT_EQ(RENDER_FRAME_INVALID_ARGUMENT, AllocateRenderFrameDynamicTable(frame, 0,
                                                                                 invalid[i].m_Count, invalid[i].m_Stride, invalid[i].m_Alignment,
                                                                                 invalid[i].m_NullIndex ? 0 : &index, invalid[i].m_NullData ? 0 : &data));
        ASSERT_EQ(123u, index);
        ASSERT_EQ((void*)&index, data);
        ASSERT_EQ((uint64_t)0, GetRenderFrameStatistics(frame).m_DynamicPayloadCapacity);
        ASSERT_EQ(0u, GetRenderFrameStatistics(frame).m_AllocationCount);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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

// Verify capture is building-only, state errors precede null-source errors, and post-seal writes preserve the view.
TEST(RenderFrame, CaptureState)
{
    RenderFrameLimits limits = { 4, 1, 1 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, 0, &index));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, CopyRenderFrameTable(frame, 0, 1, sizeof(value), 4, &value, &index));
    ReleaseLog releases = {};
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, AddRenderFrameReference(frame, &value, ReleaseReference, &releases));
    ASSERT_EQ(RENDER_FRAME_INVALID_STATE, SealRenderFrame(frame));
    ASSERT_EQ(view, GetRenderFrameView(frame));
    AssertStatistics(before, view->m_Statistics);
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
    ASSERT_GE(reference_size, (uint64_t)(2 * sizeof(void*) + sizeof(FRenderFrameRelease)));
    ASSERT_EQ(overhead + 100 + 3 * sizeof(RenderFrameTable) + 4 * reference_size, GetRenderFrameStorageSize(combined));
    RenderFrameLimits oversized_tables = { 0, UINT32_MAX, 0 };
    RenderFrameLimits oversized_references = { 0, 0, UINT32_MAX };
    ASSERT_GT(GetRenderFrameStorageSize(oversized_tables), (uint64_t)UINT32_MAX);
    ASSERT_GT(GetRenderFrameStorageSize(oversized_references), (uint64_t)UINT32_MAX);
    ASSERT_EQ((HRenderFrameStorage)0, NewRenderFrame(oversized_tables));
    ASSERT_EQ((HRenderFrameStorage)0, NewRenderFrame(oversized_references));
}

// Verify reset reuses the original payload/table allocations and clears per-capture counters rather than growing storage.
TEST(RenderFrame, RepeatedReuse)
{
    RenderFrameLimits limits = { 16, 1, 1 };
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
    HRenderFrameStorage frame = NewRenderFrame(limits);
    ASSERT_NE((HRenderFrameStorage)0, frame);
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
