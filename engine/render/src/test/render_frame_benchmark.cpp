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

#include <stdio.h>
#include <stdlib.h>

#include <dlib/time.h>

#include "render/render_frame_queue.h"
#include "render_frame_benchmark.h"

using namespace dmRender;

namespace
{
    const uint32_t ITEM_COUNT = 10000;
    const uint32_t WARMUP_COUNT = 500;
    const uint32_t ITERATION_COUNT = 5000;

    struct BenchmarkItem { uint32_t m_Values[8]; };
    enum BenchmarkMode { DIRECT_CONTROL, INLINE_ARENA, INLINE_COPY };

    struct BenchmarkConsumer
    {
        uint64_t m_Checksum;
        uint64_t m_Released;
        uint32_t m_DummyResource;
    };

    struct BenchmarkResult
    {
        uint64_t m_Capture;
        uint64_t m_Copy;
        uint64_t m_Consume;
        uint64_t m_Retire;
        uint64_t m_CopiedBytes;
        uint64_t m_Reservations;
        BenchmarkConsumer m_Consumer;
    };

    static void FillItems(BenchmarkItem* items, uint32_t iteration)
    {
        for (uint32_t i = 0; i < ITEM_COUNT; ++i)
            for (uint32_t j = 0; j < 8; ++j)
                items[i].m_Values[j] = iteration + i * 8 + j;
    }

    // The direct-input control and both queue cases call exactly this consumer.
    static void ConsumeItems(void* context, const RenderFrameView& frame)
    {
        BenchmarkConsumer* consumer = (BenchmarkConsumer*)context;
        const BenchmarkItem* items = (const BenchmarkItem*)GetRenderFrameTableData(frame, 0);
        uint64_t checksum = 0;
        for (uint32_t i = 0; i < frame.m_Tables[0].m_Count; ++i)
            for (uint32_t j = 0; j < 8; ++j)
                checksum += items[i].m_Values[j];
        consumer->m_Checksum += checksum;
    }

    static void ReleaseDummy(void* context, void* resource)
    {
        ((BenchmarkConsumer*)context)->m_Released += *(uint32_t*)resource;
    }

    static bool RunFrame(BenchmarkMode mode, HRenderFrameQueue queue, BenchmarkItem* source,
        uint32_t iteration, BenchmarkResult* result)
    {
        uint64_t begin = dmTime::GetMonotonicTime();
        RenderFrameTicket ticket = {};
        RenderFrameStatistics statistics = {};
        RenderFrameTable table = { 1, 0, ITEM_COUNT, sizeof(BenchmarkItem) };
        RenderFrameView direct_view = {};
        direct_view.m_Payload = (const uint8_t*)source;
        direct_view.m_Tables = &table;
        direct_view.m_Statistics.m_TableCount = 1;
        uint32_t index = 0;
        if (queue && BeginRenderFrameCapture(queue, &ticket) != RENDER_FRAME_OK)
            return false;
        if (mode == INLINE_ARENA)
        {
            void* data;
            if (AllocateRenderFrameTable(ticket.m_Frame, 1, ITEM_COUNT, sizeof(BenchmarkItem), 16, &index, &data) != RENDER_FRAME_OK)
                return false;
            FillItems((BenchmarkItem*)data, iteration);
        }
        else
            FillItems(source, iteration);
        if (queue && AddRenderFrameReference(ticket.m_Frame, &result->m_Consumer.m_DummyResource, ReleaseDummy, &result->m_Consumer) != RENDER_FRAME_OK)
            return false;
        uint64_t captured = dmTime::GetMonotonicTime();
        if (mode == INLINE_COPY && CopyRenderFrameTable(ticket.m_Frame, 1, ITEM_COUNT, sizeof(BenchmarkItem), 16, source, &index) != RENDER_FRAME_OK)
            return false;
        uint64_t copied = dmTime::GetMonotonicTime();
        if (queue)
        {
            if (PublishRenderFrame(queue, ticket) != RENDER_FRAME_OK)
                return false;
        }
        else
            ConsumeItems(&result->m_Consumer, direct_view);
        uint64_t consumed = dmTime::GetMonotonicTime();
        // Read counters before retirement resets them; this cost is included in retirement.
        if (queue)
        {
            statistics = GetRenderFrameStatistics(ticket.m_Frame);
            if (RetireRenderFrames(queue) != 1)
                return false;
        }
        else
            ReleaseDummy(&result->m_Consumer, &result->m_Consumer.m_DummyResource);
        uint64_t retired = dmTime::GetMonotonicTime();
        if (iteration >= WARMUP_COUNT)
        {
            result->m_Capture += captured - begin;
            result->m_Copy += copied - captured;
            result->m_Consume += consumed - copied;
            result->m_Retire += retired - consumed;
            result->m_CopiedBytes += statistics.m_CopiedBytes;
            result->m_Reservations += statistics.m_AllocationCount;
        }
        return true;
    }

    static bool RunCase(BenchmarkMode mode, const char* name, BenchmarkResult* result)
    {
        uint64_t started = dmTime::GetMonotonicTime();
        BenchmarkItem* source = mode == INLINE_ARENA ? 0 : (BenchmarkItem*)malloc(ITEM_COUNT * sizeof(BenchmarkItem));
        HRenderExecution execution = 0;
        HRenderFrameQueue queue = 0;
        RenderFrameQueueParams params = {};
        params.m_Limits.m_PayloadBytes = ITEM_COUNT * sizeof(BenchmarkItem);
        params.m_Limits.m_MaxTables = 1;
        params.m_Limits.m_MaxReferences = 1;
        params.m_Consume = ConsumeItems;
        params.m_ConsumeContext = &result->m_Consumer;
        result->m_Consumer.m_DummyResource = 1;
        bool success = mode == INLINE_ARENA || source != 0;
        if (success && mode != DIRECT_CONTROL)
        {
            ExecutionSelection selection;
            execution = NewRenderExecution(EXECUTION_MODE_INLINE, false, &selection);
            params.m_Execution = execution;
            if (execution)
                queue = NewRenderFrameQueue(params);
            success = queue != 0;
        }
        uint64_t allocated = dmTime::GetMonotonicTime();
        for (uint32_t i = 0; success && i < WARMUP_COUNT + ITERATION_COUNT; ++i)
        {
            if (i == WARMUP_COUNT)
            {
                result->m_Consumer.m_Checksum = 0;
                result->m_Consumer.m_Released = 0;
            }
            success = RunFrame(mode, queue, source, i, result);
        }
        if (queue)
        {
            StopRenderFrameQueue(queue);
            PumpRenderFrameQueue(queue, true);
            RetireRenderFrames(queue);
            DeleteRenderFrameQueue(queue);
        }
        if (execution)
            DeleteRenderExecution(execution);
        free(source);
        if (!success || result->m_Consumer.m_Released != ITERATION_COUNT)
        {
            fprintf(stderr, "render-frame benchmark failed: %s (capture/allocation/retirement)\n", name);
            return false;
        }
        const double iterations = ITERATION_COUNT;
        printf("%s: startup=%llu us, fixed_frame_storage=%llu B, source=%u B\n", name,
            (unsigned long long)(allocated - started),
            (unsigned long long)(mode == DIRECT_CONTROL ? 0 : GetRenderFrameQueueStorageSize(params.m_Limits)),
            mode == INLINE_ARENA ? 0u : (uint32_t)(ITEM_COUNT * sizeof(BenchmarkItem)));
        printf("  mean us/frame: capture=%.3f copy=%.3f publish_consume=%.3f retire=%.3f total=%.3f\n",
            result->m_Capture / iterations, result->m_Copy / iterations, result->m_Consume / iterations,
            result->m_Retire / iterations, (result->m_Capture + result->m_Copy + result->m_Consume + result->m_Retire) / iterations);
        printf("  mean/frame: copied_bytes=%.0f arena_reservations=%.1f; checksum=%llu releases=%llu\n",
            result->m_CopiedBytes / iterations, result->m_Reservations / iterations,
            (unsigned long long)result->m_Consumer.m_Checksum, (unsigned long long)result->m_Consumer.m_Released);
        return true;
    }
}

int RunRenderFrameBenchmark()
{
    printf("Render-frame microbenchmark: %u items x %u B, %u warmup + %u measured frames/case.\n",
        ITEM_COUNT, (uint32_t)sizeof(BenchmarkItem), WARMUP_COUNT, ITERATION_COUNT);
    printf("Microsecond phase clocks include timer overhead; small phases need aggregate interpretation.\n"
        "Copy phase includes CopyRenderFrameTable reservation/validation; copy mode also fills the source during capture.\n"
        "Startup includes execution context creation; storage bytes exclude that context and allocator overhead.\n"
        "Sequential synthetic runs are diagnostic only, not engine inline-regression qualification.\n");
    BenchmarkResult results[3] = {};
    const char* names[] = { "direct_input_control", "inline_direct_arena", "inline_copied_capture" };
    for (uint32_t i = 0; i < 3; ++i)
        if (!RunCase((BenchmarkMode)i, names[i], &results[i]))
            return 1;
    const uint64_t values = ITEM_COUNT * 8;
    const uint64_t iteration_sum = (uint64_t)ITERATION_COUNT * (2 * WARMUP_COUNT + ITERATION_COUNT - 1) / 2;
    const uint64_t expected = values * iteration_sum + ITERATION_COUNT * values * (values - 1) / 2;
    if (results[0].m_Consumer.m_Checksum != expected
        || results[0].m_Consumer.m_Checksum != results[1].m_Consumer.m_Checksum
        || results[0].m_Consumer.m_Checksum != results[2].m_Consumer.m_Checksum)
    {
        fprintf(stderr, "render-frame benchmark failed: consumer checksums differ\n");
        return 1;
    }
    return 0;
}
