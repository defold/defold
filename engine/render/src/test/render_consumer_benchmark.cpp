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
#include <string.h>

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <platform/window.hpp>
#include <script/script.h>

#include "render/render_command.h"
#include "render/render_frame.h"
#include "render/render_frame_queue.h"
#include "render/render_private.h"
#include "render_consumer_benchmark.h"

using namespace dmRender;

namespace
{
    const uint32_t WARMUP_COUNT = 100;
    const uint32_t ITEM_TABLE_TYPE = 1;

    enum BenchmarkMode
    {
        DIRECT_LEGACY,
        SNAPSHOT_INLINE,
        SNAPSHOT_THREADED,
    };

    // Captured input is deliberately small. This is not a sprite memory estimate.
    struct Item
    {
        dmVMath::Point3 m_Position;
        uint32_t        m_Batch;
    };

    struct Workload
    {
        uint32_t m_Items;
        uint32_t m_Views;
        uint8_t  m_Moving : 1;
    };

    struct ResultData
    {
        uint64_t m_Wall;
        uint64_t m_Source;
        uint64_t m_Capture;
        uint64_t m_AdmissionRetire;
        uint64_t m_Submit;
        uint64_t m_SetupItems;
        uint64_t m_DrawPreparationDispatch;
        uint64_t m_Cleanup;
        uint64_t m_FinalRetire;
        uint64_t m_Checksum;
        uint64_t m_Visible;
        uint64_t m_Batches;
        uint64_t m_CopiedBytes;
        uint64_t m_Reservations;
        uint64_t m_DynamicAllocations;
        uint64_t m_FixedStorage;
        uint64_t m_ScratchBytes;
        uint64_t m_RetainedBytes;
        uint32_t m_CapacityGrowthFrames;
        uint32_t m_Consumed;
    };

    // Producer and consumer write different full-width fields. All shared setup is
    // published by the startup barrier; consumer results are read only after drain/join.
    struct Benchmark
    {
        ResultData                              m_Result;
        Workload                                m_Workload;
        HRenderContext                          m_Render;
        HRenderExecution                        m_Execution;
        HRenderFrameQueue                       m_Queue;
        HRenderItem*                            m_Items;
        const Item*                             m_Inputs;
        dmThread::Thread                        m_Thread;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        uint64_t                                m_FrameChecksum;
        uint64_t                                m_FrameVisible;
        uint64_t                                m_FrameBatches;
        HRenderItemDispatch                     m_Dispatch;
        BenchmarkMode                           m_Mode;
        uint32_t                                m_Iteration;
        uint32_t                                m_Iterations;
        // Separate bytes: startup flags and consumer flags have different owners.
        uint8_t                                 m_Started;
        uint8_t                                 m_Bound;
        uint8_t                                 m_InvalidOwner;
        uint8_t                                 m_Phases;
    };

    static uint64_t Clock(const Benchmark* bench)
    {
        return bench->m_Phases ? dmTime::GetMonotonicTime() : 0;
    }

    static void FillItems(Item* items, const Workload& workload, uint32_t iteration)
    {
        // Identical game work for all paths; moving inputs vary bounds/depth every
        // frame. Static inputs retain their values, exercising persistent caches.
        for (uint32_t i = 0; i < workload.m_Items; ++i)
        {
            uint32_t offset = workload.m_Moving ? iteration : 0;
            float x = (float)((i * 17 + offset) % 600) / 100.0f - 3.0f;
            float y = (float)((i * 7 + offset) % 150) / 100.0f - 0.75f;
            float z = (float)((i * 13 + offset) % 150) / 100.0f - 0.75f;
            items[i].m_Position = dmVMath::Point3(x, y, z);
            items[i].m_Batch = i % 16;
        }
    }

    static void Dispatch(const RenderListDispatchParams& params)
    {
        if (params.m_Operation != RENDER_LIST_OPERATION_BATCH)
        {
            return;
        }
        Benchmark* bench = (Benchmark*)params.m_UserData;
        ++bench->m_FrameBatches;
        for (const uint32_t* index = params.m_Begin; index != params.m_End; ++index)
        {
            const RenderListEntry& entry = params.m_Buf[*index];
            uint32_t item = (uint32_t)entry.m_UserData;
            uint32_t position_bits;
            float x = bench->m_Inputs[item].m_Position.getX();
            memcpy(&position_bits, &x, sizeof(position_bits));
            bench->m_FrameChecksum = bench->m_FrameChecksum * 1099511628211ULL ^
                                     ((uint64_t)item << 32 | position_bits);
            ++bench->m_FrameVisible;
        }
        // No render objects are emitted: measure real culling/sorting/batching,
        // but not geometry generation, uploads, driver draw cost or GPU execution.
    }

    static void InitializeItems(Benchmark* bench)
    {
        bench->m_Dispatch = RegisterRenderListDispatch(bench->m_Render, Dispatch, 0, bench);
        for (uint32_t i = 0; i < bench->m_Workload.m_Items; ++i)
        {
            bench->m_Items[i] = NewRenderItem(bench->m_Render, bench->m_Dispatch, i);
            // Also registers the empty material-tag list used by material-free draws.
            SetMaterial(bench->m_Render, bench->m_Items[i], 0);
            Enable(bench->m_Render, bench->m_Items[i], true);
        }
    }

    static void DeleteItems(Benchmark* bench)
    {
        for (uint32_t i = 0; i < bench->m_Workload.m_Items; ++i)
        {
            DeleteRenderItem(bench->m_Render, bench->m_Items[i]);
        }
        UnregisterRenderListDispatch(bench->m_Render, bench->m_Dispatch);
    }

    static void BindItems(Benchmark* bench, const Item* inputs)
    {
        bench->m_Inputs = inputs;
        // A static scene does not resend unchanged setters: SetWorldTransform
        // invalidates visibility even when the supplied matrix is unchanged.
        if (bench->m_Workload.m_Moving || bench->m_Iteration == 0)
        {
            for (uint32_t i = 0; i < bench->m_Workload.m_Items; ++i)
            {
                SetWorldTransform(bench->m_Render, bench->m_Items[i],
                                  dmVMath::Matrix4::translation(dmVMath::Vector3(inputs[i].m_Position)));
                SetBoundingSphere(bench->m_Render, bench->m_Items[i], inputs[i].m_Position, 0.025f);
                SetBatchKey(bench->m_Render, bench->m_Items[i], inputs[i].m_Batch);
            }
        }
        SubmitRenderListDispatch(bench->m_Render, bench->m_Dispatch);
    }

    static FrustumOptions GetFrustum(uint32_t view)
    {
        FrustumOptions frustum;
        frustum.m_Matrix = dmVMath::Matrix4::translation(dmVMath::Vector3((float)view * 0.4f, 0, 0));
        frustum.m_NumPlanes = FRUSTUM_PLANES_ALL;
        return frustum;
    }

    static uint64_t ScratchBytes(HRenderContext context)
    {
#if defined(RENDER_CONSUMER_BENCHMARK_BASELINE)
        // Compile the identical direct workload against the branch base, where
        // draw scratch arrays were direct RenderContext members.
        return (uint64_t)context->m_RenderListSortValues.Capacity() * sizeof(RenderListSortValue) +
               (uint64_t)context->m_RenderListSortBuffer.Capacity() * sizeof(uint32_t) +
               (uint64_t)context->m_RenderListSortedRanges.Capacity() * sizeof(RenderListRange) +
               (uint64_t)context->m_RenderListRanges.Capacity() * sizeof(RenderListRange) +
               (uint64_t)context->m_RenderListSpans.Capacity() * sizeof(RenderListSpan);
#else
        const RenderListScratch& scratch = context->m_RenderListScratch;
        return (uint64_t)scratch.m_SortValues.Capacity() * sizeof(RenderListSortValue) +
               (uint64_t)scratch.m_SortBuffer.Capacity() * sizeof(uint32_t) +
               (uint64_t)scratch.m_SortedRanges.Capacity() * sizeof(RenderListRange) +
               (uint64_t)scratch.m_Ranges.Capacity() * sizeof(RenderListRange) +
               (uint64_t)scratch.m_Spans.Capacity() * sizeof(RenderListSpan);
#endif
    }

    static uint64_t RetainedBytes(const Benchmark* bench)
    {
        HRenderContext context = bench->m_Render;
        const RenderItemDispatch* dispatch = context->m_RenderItemDispatches[bench->m_Dispatch];
        return (uint64_t)context->m_PersistentRenderItems.Capacity() * sizeof(PersistentRenderItem) +
               (uint64_t)context->m_RenderList.Capacity() * sizeof(RenderListEntry) +
               (uint64_t)dispatch->m_EnabledItems.Capacity() * sizeof(uint32_t) +
               (uint64_t)dispatch->m_SortIndices.Capacity() * sizeof(uint32_t) +
               (uint64_t)dispatch->m_Ranges.Capacity() * sizeof(RenderItemRange);
    }

    static void FinishFrame(Benchmark* bench, uint64_t started, uint64_t prepared, uint64_t drawn,
                            uint64_t finished, const RenderFrameStatistics* statistics)
    {
        bool measured = bench->m_Iteration >= WARMUP_COUNT;
        ResultData* result = &bench->m_Result;
        if (measured)
        {
            result->m_Checksum = result->m_Checksum * 1099511628211ULL ^ bench->m_FrameChecksum;
            result->m_Visible += bench->m_FrameVisible;
            result->m_Batches += bench->m_FrameBatches;
            if (bench->m_Phases)
            {
                result->m_SetupItems += prepared - started;
                result->m_DrawPreparationDispatch += drawn - prepared;
                result->m_Cleanup += finished - drawn;
            }
            ++result->m_Consumed;
            if (statistics)
            {
                result->m_CopiedBytes += statistics->m_CopiedBytes;
                result->m_Reservations += statistics->m_AllocationCount;
                result->m_DynamicAllocations += statistics->m_DynamicAllocationCount;
            }
        }
        if (bench->m_Phases || bench->m_Iteration + 1 == WARMUP_COUNT + bench->m_Iterations)
        {
            uint64_t scratch = ScratchBytes(bench->m_Render);
            uint64_t retained = RetainedBytes(bench);
            if (bench->m_Phases && measured &&
                (scratch > result->m_ScratchBytes || retained > result->m_RetainedBytes))
            {
                ++result->m_CapacityGrowthFrames;
            }
            result->m_ScratchBytes = scratch;
            result->m_RetainedBytes = retained;
        }
        ++bench->m_Iteration;
    }

    static void Consume(void* user_data, const RenderFrameView& frame)
    {
#if defined(RENDER_CONSUMER_BENCHMARK_BASELINE)
        // Baseline builds run only DIRECT_LEGACY; its snapshot contract predates
        // consumer-owned setup/cleanup and is deliberately not benchmarked here.
        (void)user_data;
        (void)frame;
#else
        Benchmark* bench = (Benchmark*)user_data;
        bench->m_InvalidOwner |= !IsRenderExecutionOwner(bench->m_Execution, EXECUTION_OWNER_RENDER);
        bench->m_FrameChecksum = frame.m_FrameId;
        bench->m_FrameVisible = 0;
        bench->m_FrameBatches = 0;
        uint64_t started = Clock(bench);
        dmGraphics::BeginFrame(GetGraphicsContext(bench->m_Render));
        BeginRenderFrameConsumption(bench->m_Render, frame, RENDER_FRAME_PREPARATION_CONSUMER);
        BindItems(bench, (const Item*)GetRenderFrameTableData(frame, 0));
        RenderListEnd(bench->m_Render);
        uint64_t prepared = Clock(bench);
        ConsumeRenderCommands(bench->m_Render, frame);
        uint64_t drawn = Clock(bench);
        bench->m_Inputs = 0;
        EndRenderFrameConsumption(bench->m_Render, RENDER_FRAME_PREPARATION_CONSUMER);
        dmGraphics::Flip(GetGraphicsContext(bench->m_Render));
        uint64_t finished = Clock(bench);
        FinishFrame(bench, started, prepared, drawn, finished, &frame.m_Statistics);
#endif
    }

    static void DirectFrame(Benchmark* bench, const Item* inputs, uint32_t iteration)
    {
        bench->m_InvalidOwner |= !IsRenderExecutionOwner(bench->m_Execution, EXECUTION_OWNER_RENDER);
        bench->m_FrameChecksum = iteration + 1;
        bench->m_FrameVisible = 0;
        bench->m_FrameBatches = 0;
        uint64_t started = Clock(bench);
        dmGraphics::BeginFrame(GetGraphicsContext(bench->m_Render));
        BeginFrame(bench->m_Render, (float)iteration / 60.0f, 1.0f / 60.0f);
        RenderListBegin(bench->m_Render);
        BindItems(bench, inputs);
        RenderListEnd(bench->m_Render);
        uint64_t prepared = Clock(bench);
        for (uint32_t view = 0; view < bench->m_Workload.m_Views; ++view)
        {
            FrustumOptions frustum = GetFrustum(view);
            DrawRenderList(bench->m_Render, 0, 0, &frustum, SORT_BACK_TO_FRONT);
        }
        uint64_t drawn = Clock(bench);
        bench->m_Inputs = 0;
        ClearRenderObjects(bench->m_Render);
        dmGraphics::Flip(GetGraphicsContext(bench->m_Render));
        uint64_t finished = Clock(bench);
        FinishFrame(bench, started, prepared, drawn, finished, 0);
    }

    static bool Record(Benchmark* bench, HRenderFrameStorage frame, const Item* inputs, uint32_t iteration)
    {
        uint32_t table;
        RenderFrameInfo info = { 0, 20, 10, (float)iteration / 60.0f, 1.0f / 60.0f };
        BeginRenderFrameRecording(bench->m_Render, info.m_Time, info.m_Dt);
        if (CopyRenderFrameTable(frame, ITEM_TABLE_TYPE, bench->m_Workload.m_Items, sizeof(Item),
                                 alignof(Item), inputs, &table, 0) != RENDER_FRAME_OK ||
            RecordRenderFrameInfo(frame, info) != RENDER_FRAME_OK)
        {
            return false;
        }
        Command commands[] = { Command(COMMAND_TYPE_DRAW), Command(COMMAND_TYPE_DRAW),
                               Command(COMMAND_TYPE_DRAW), Command(COMMAND_TYPE_DRAW) };
        for (uint32_t view = 0; view < bench->m_Workload.m_Views; ++view)
        {
            FrustumOptions frustum = GetFrustum(view);
            Command source(COMMAND_TYPE_DRAW, 0, 0, (uint64_t)&frustum, SORT_BACK_TO_FRONT);
            if (RecordRenderCommand(bench->m_Render, frame, source, &commands[view], 0, false, false) != RENDER_FRAME_OK)
            {
                return false;
            }
        }
        return AppendRenderCommands(frame, commands, bench->m_Workload.m_Views) == RENDER_FRAME_OK;
    }

    static void RunExecutor(void* user_data)
    {
        Benchmark* bench = (Benchmark*)user_data;
        bench->m_Bound = BindRenderExecutionOwner(bench->m_Execution);
        InitializeItems(bench);
        dmMutex::Lock(bench->m_Mutex);
        bench->m_Started = true;
        dmConditionVariable::Signal(bench->m_Changed);
        dmMutex::Unlock(bench->m_Mutex);
        for (;;)
        {
            RenderFramePumpResult result = PumpRenderFrameQueue(bench->m_Queue, true);
            if (result == RENDER_FRAME_PUMP_DRAINED)
            {
                break;
            }
            if (result == RENDER_FRAME_PUMP_IDLE || result == RENDER_FRAME_PUMP_BUSY)
            {
                WaitForRenderFrameWork(bench->m_Queue);
            }
        }
        DeleteItems(bench);
        ReleaseRenderExecutionOwner(bench->m_Execution);
    }

    static void WaitForFrames(Benchmark* bench, uint32_t count)
    {
        for (;;)
        {
            RenderFrameQueueStatus status = GetRenderFrameQueueStatus(bench->m_Queue);
            if (status.m_Consumed == count)
            {
                break;
            }
            WaitForRenderFrameProgress(bench->m_Queue, status.m_Revision);
        }
    }

    static bool RunCase(const Workload& workload, BenchmarkMode mode, uint32_t iterations, bool phases, ResultData* result)
    {
        dmGraphics::InstallAdapter(dmGraphics::ADAPTER_FAMILY_NONE);
        WindowCreateParams window_params;
        WindowCreateParamsInitialize(&window_params);
        window_params.m_Width = 20;
        window_params.m_Height = 10;
        HWindow window = dmPlatform::NewWindow();
        dmPlatform::OpenWindow(window, window_params);
        dmGraphics::ContextParams graphics_params;
        graphics_params.m_Window = window;
        graphics_params.m_Width = 20;
        graphics_params.m_Height = 10;
        dmGraphics::HContext graphics = dmGraphics::NewContext(graphics_params);
        dmScript::ContextParams script_params = {};
        script_params.m_GraphicsContext = graphics;
        dmScript::HContext script = dmScript::NewContext(script_params);
        dmScript::Initialize(script);
        RenderContextParams render_params;
        render_params.m_ScriptContext = script;
        render_params.m_UseThread = false;
        render_params.m_CommandBufferSize = 4;
        render_params.m_MaxInstances = 1;
        render_params.m_MaxBatches = 1;
        render_params.m_MaxCharacters = 8;
        Benchmark bench = {};
        bench.m_Render = NewRenderContext(graphics, render_params);
        bench.m_Workload = workload;
        bench.m_Mode = mode;
        bench.m_Phases = phases;
        bench.m_Iterations = iterations;
        bench.m_Items = new HRenderItem[workload.m_Items];
        Item* inputs = new Item[workload.m_Items];
        ExecutionSelection selection;
        bench.m_Execution = NewRenderExecution(mode == SNAPSHOT_THREADED ? EXECUTION_MODE_THREADED : EXECUTION_MODE_INLINE,
                                               true, &selection);
        bool success = bench.m_Render && bench.m_Execution;
        if (success && mode != DIRECT_LEGACY)
        {
            RenderFrameQueueParams params = {};
            params.m_Execution = bench.m_Execution;
            params.m_Consume = Consume;
            params.m_ConsumeContext = &bench;
            RenderFrameLimits components = { workload.m_Items * (uint32_t)sizeof(Item), 1, 0 };
            success = CalculateRenderFrameLimits(bench.m_Render, components, &params.m_Limits) == RENDER_FRAME_OK;
            bench.m_Queue = success ? NewRenderFrameQueue(params) : 0;
            success = bench.m_Queue != 0;
            bench.m_Result.m_FixedStorage = GetRenderFrameQueueStorageSize(params);
        }
        if (!success)
        {
            fprintf(stderr, "Unable to initialize render consumer benchmark.\n");
            return false;
        }
        SetViewMatrix(bench.m_Render, dmVMath::Matrix4::identity());
        SetProjectionMatrix(bench.m_Render, dmVMath::Matrix4::identity());
        if (mode == SNAPSHOT_THREADED)
        {
            bench.m_Mutex = dmMutex::New();
            bench.m_Changed = dmConditionVariable::New();
            bench.m_Thread = dmThread::New(RunExecutor, 0x80000, &bench, "render-benchmark");
            dmMutex::Lock(bench.m_Mutex);
            while (!bench.m_Started)
            {
                dmConditionVariable::Wait(bench.m_Changed, bench.m_Mutex);
            }
            dmMutex::Unlock(bench.m_Mutex);
            success = bench.m_Bound;
        }
        else
        {
            InitializeItems(&bench);
        }

        uint64_t wall_started = 0;
        for (uint32_t iteration = 0; success && iteration < WARMUP_COUNT + iterations; ++iteration)
        {
            if (iteration == WARMUP_COUNT)
            {
                if (bench.m_Queue)
                {
                    WaitForFrames(&bench, WARMUP_COUNT);
                    RetireRenderFrames(bench.m_Queue);
                }
                wall_started = dmTime::GetMonotonicTime();
            }
            uint64_t begin = Clock(&bench);
            RenderFrameTicket ticket = {};
            if (bench.m_Queue && AcquireRenderFrame(bench.m_Queue, &ticket) != RENDER_FRAME_OK)
            {
                success = false;
                break;
            }
            uint64_t admitted = Clock(&bench);
            FillItems(inputs, workload, iteration);
            uint64_t updated = Clock(&bench);
            if (bench.m_Queue)
            {
                success = Record(&bench, ticket.m_Frame, inputs, iteration);
            }
            uint64_t recorded = Clock(&bench);
            if (success)
            {
                if (bench.m_Queue)
                {
                    success = SubmitRenderFrame(bench.m_Queue, ticket) == RENDER_FRAME_OK;
                }
                else
                {
                    DirectFrame(&bench, inputs, iteration);
                }
            }
            uint64_t submitted = Clock(&bench);
            if (phases && iteration >= WARMUP_COUNT)
            {
                bench.m_Result.m_AdmissionRetire += admitted - begin;
                bench.m_Result.m_Source += updated - admitted;
                bench.m_Result.m_Capture += recorded - updated;
                bench.m_Result.m_Submit += submitted - recorded;
            }
        }
        if (success && bench.m_Queue)
        {
            WaitForFrames(&bench, WARMUP_COUNT + iterations);
            uint64_t begin = Clock(&bench);
            RetireRenderFrames(bench.m_Queue);
            bench.m_Result.m_FinalRetire += Clock(&bench) - begin;
        }
        bench.m_Result.m_Wall = dmTime::GetMonotonicTime() - wall_started;
        if (bench.m_Queue)
        {
            StopRenderFrameQueue(bench.m_Queue);
        }
        if (mode == SNAPSHOT_THREADED)
        {
            dmThread::Join(bench.m_Thread);
            dmConditionVariable::Delete(bench.m_Changed);
            dmMutex::Delete(bench.m_Mutex);
        }
        else
        {
            DeleteItems(&bench);
        }
        if (bench.m_Queue)
        {
            RetireRenderFrames(bench.m_Queue);
            DeleteRenderFrameQueue(bench.m_Queue);
        }
        success = success && !bench.m_InvalidOwner && bench.m_Result.m_Consumed == iterations && bench.m_Result.m_Visible > 0;
        *result = bench.m_Result;
        DeleteRenderExecution(bench.m_Execution);
        DeleteRenderContext(bench.m_Render, script);
        dmScript::Finalize(script);
        dmScript::DeleteContext(script);
        dmGraphics::DeleteContext(graphics);
        dmPlatform::CloseWindow(window);
        dmPlatform::DeleteWindow(window);
        delete[] inputs;
        delete[] bench.m_Items;
        return success;
    }

    static void PrintResult(const Workload& workload, BenchmarkMode mode, uint32_t iterations, bool phases, const ResultData& result)
    {
        const char* names[] = { "direct_legacy", "snapshot_inline", "snapshot_threaded" };
        const double frames = iterations;
        printf("items=%u views=%u motion=%s mode=%s wall_us/frame=%.3f checksum=%llu visible/frame=%.1f batches/frame=%.1f\n",
               workload.m_Items, workload.m_Views, workload.m_Moving ? "moving" : "static", names[mode],
               result.m_Wall / frames, (unsigned long long)result.m_Checksum, result.m_Visible / frames, result.m_Batches / frames);
        if (phases)
        {
            printf("  producer_us/frame: source=%.3f capture_copy=%.3f admission_retire_wait=%.3f submit_wait_or_inline_consume=%.3f final_retire=%.3f\n",
                   result.m_Source / frames, result.m_Capture / frames, result.m_AdmissionRetire / frames,
                   result.m_Submit / frames, result.m_FinalRetire / frames);
            printf("  consumer_us/frame: setup_items=%.3f draw_preparation_dispatch=%.3f cleanup=%.3f\n",
                   result.m_SetupItems / frames, result.m_DrawPreparationDispatch / frames, result.m_Cleanup / frames);
        }
        printf("  memory: fixed_queue=%llu B scratch_capacity=%llu B retained_capacity=%llu B source=%u B handles=%u B\n",
               (unsigned long long)result.m_FixedStorage, (unsigned long long)result.m_ScratchBytes,
               (unsigned long long)result.m_RetainedBytes, workload.m_Items * (uint32_t)sizeof(Item),
               workload.m_Items * (uint32_t)sizeof(HRenderItem));
        printf("  frame_storage: copied_bytes/frame=%.0f reservations/frame=%.1f new_dynamic_blocks=%llu",
               result.m_CopiedBytes / frames, result.m_Reservations / frames, (unsigned long long)result.m_DynamicAllocations);
        if (phases)
        {
            printf(" capacity_growth_frames=%u", result.m_CapacityGrowthFrames);
        }
        printf("\n");
    }
}

int RunRenderConsumerBenchmark(int argc, char** argv)
{
    bool phases = false;
    uint32_t iterations = 1000;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--benchmark-phases") == 0)
        {
            phases = true;
        }
        else if (strncmp(argv[i], "--benchmark-iterations=", 23) == 0)
        {
            char* end;
            unsigned long count = strtoul(argv[i] + 23, &end, 10);
            if (*end || count == 0 || count > 1000000)
            {
                fprintf(stderr, "--benchmark-iterations must be in [1, 1000000].\n");
                return 1;
            }
            iterations = (uint32_t)count;
        }
    }
#if defined(RENDER_CONSUMER_BENCHMARK_BASELINE)
    uint32_t modes = 1;
    printf("Branch-base compatibility build: direct retained-item control only.\n");
#else
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    uint32_t modes = capabilities.m_BuildEnabled && capabilities.m_ThreadsSupported ? 3 : 2;
#endif
    printf("Retained-item CPU benchmark: %u warmup + %u measured frames/case; phase clocks %s.\n",
           WARMUP_COUNT, iterations, phases ? "enabled" : "disabled");
    printf("Same game-input generation, item setters, typed sphere culling, depth sorting and ordered batch checksum in every mode.\n"
           "Draw preparation includes culling, sorting, batch dispatch/checksum and empty null-backend drawing; no geometry or GPU work.\n"
           "Threaded uses a test executor and two CPU slots. Wall time includes final reader completion; phases overlap and must not be added.\n"
           "Memory reports selected array capacities and fixed frame storage, NOT total allocations or GPU memory.\n"
           "Run throughput and --benchmark-phases separately; repeat throughput runs before comparing results.\n");
    if (modes == 2)
    {
        printf("Native threaded case skipped: requires --with-threaded-rendering and native thread support.\n");
    }
    for (uint32_t size = 0; size < 2; ++size)
    {
        for (uint32_t moving = 0; moving < 2; ++moving)
        {
            for (uint32_t views = 0; views < 2; ++views)
            {
                Workload workload = { size ? 10000u : 1000u, views ? 4u : 1u, (uint8_t)moving };
                ResultData control = {};
                for (uint32_t mode = 0; mode < modes; ++mode)
                {
                    ResultData result = {};
                    if (!RunCase(workload, (BenchmarkMode)mode, iterations, phases, &result))
                    {
                        fprintf(stderr, "Render consumer benchmark failed: execution or frame recording error.\n");
                        return 1;
                    }
                    PrintResult(workload, (BenchmarkMode)mode, iterations, phases, result);
                    if (mode == 0)
                    {
                        control = result;
                    }
                    else if (result.m_Checksum != control.m_Checksum || result.m_Visible != control.m_Visible ||
                             result.m_Batches != control.m_Batches)
                    {
                        fprintf(stderr, "Render consumer benchmark output differs from direct retained-item control.\n");
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}
