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
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <platform/window.hpp>
#include "render/render_script.h"
#if !defined(RENDER_BENCHMARK_BASELINE)
#include "render/render_frame_queue.h"
#endif

using namespace dmRender;

namespace
{
    // Opt-in CPU benchmark: run the same Lua workload through the legacy or owned-frame
    // path with an empty render list. Batch timings exclude the optional phase clocks.
    // RENDER_BENCHMARK_BASELINE is used only when copying this harness to an older tree.
    const uint32_t WARMUP = 500;
    const uint32_t BATCH_FRAMES = 100;
    const uint32_t BATCHES = 30;

    struct Workload
    {
        const char* m_Name;
        uint32_t    m_Draws;
        uint32_t    m_Vectors;
        uint8_t     m_Camera : 1;
        uint8_t     m_Frustum : 1;
    };

    struct Benchmark
    {
        HRenderContext        m_Render;
        HRenderScriptInstance m_Instance;
#if !defined(RENDER_BENCHMARK_BASELINE)
        HRenderFrameQueue     m_Queue;
        RenderFrameStatistics m_Statistics;
#endif
        uint64_t m_RecordTime;
        uint64_t m_ConsumeTime;
        uint64_t m_RetireTime;
        uint32_t m_FrameCount;
        uint8_t  m_Phases : 1;
    };

#if !defined(RENDER_BENCHMARK_BASELINE)
    static void Consume(void* user, const RenderFrameView& frame)
    {
        Benchmark* bench = (Benchmark*)user;
        BeginRenderFrameConsumption(bench->m_Render, frame);
        RenderListEnd(bench->m_Render);
        ConsumeRenderCommands(bench->m_Render, frame);
    }
#endif

    static bool RunFrame(Benchmark* bench)
    {
        HRenderContext context = bench->m_Render;
#if !defined(RENDER_BENCHMARK_BASELINE)
        uint64_t start = bench->m_Phases ? dmTime::GetMonotonicTime() : 0;
        RenderFrameTicket ticket = {};
        if (AcquireRenderFrame(bench->m_Queue, &ticket) != RENDER_FRAME_OK)
        {
            return false;
        }
#endif
        BeginFrame(context, 1.0f, 1.0f / 60);
        RenderListBegin(context);
        dmGraphics::BeginFrame(GetGraphicsContext(context));
#if defined(RENDER_BENCHMARK_BASELINE)
        RenderListEnd(context);
        if (UpdateRenderScriptInstance(bench->m_Instance, 1.0f / 60) != RENDER_SCRIPT_RESULT_OK)
        {
            return false;
        }
#else
        if (RecordRenderFrameInfo(ticket.m_Frame, GetRenderFrameInfo(context)) != RENDER_FRAME_OK ||
            RecordRenderScriptInstance(bench->m_Instance, 1.0f / 60, ticket.m_Frame) != RENDER_FRAME_OK)
        {
            return false;
        }
        uint64_t recorded = bench->m_Phases ? dmTime::GetMonotonicTime() : 0;
        if (SubmitRenderFrame(bench->m_Queue, ticket) != RENDER_FRAME_OK)
        {
            return false;
        }
        uint64_t consumed = bench->m_Phases ? dmTime::GetMonotonicTime() : 0;
        bench->m_Statistics = GetRenderFrameStatistics(ticket.m_Frame);
        if (RetireRenderFrames(bench->m_Queue) != 1)
        {
            return false;
        }
        uint64_t retired = bench->m_Phases ? dmTime::GetMonotonicTime() : 0;
        bench->m_RecordTime += recorded - start;
        bench->m_ConsumeTime += consumed - recorded;
        bench->m_RetireTime += retired - consumed;
#endif
        dmGraphics::Flip(GetGraphicsContext(context));
        ++bench->m_FrameCount;
        return GetViewMatrix(context).getTranslation().getX() == (float)bench->m_FrameCount;
    }

    static int CompareDouble(const void* a, const void* b)
    {
        double x = *(const double*)a;
        double y = *(const double*)b;
        return x < y ? -1 : x > y ? 1 : 0;
    }

    static bool RunCase(const Workload& workload, bool phases, bool growth)
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
        dmScript::HContext script_context = dmScript::NewContext(script_params);
        dmScript::Initialize(script_context);
        RenderContextParams render_params;
        render_params.m_ScriptContext = script_context;
        render_params.m_CommandBufferSize = 1024;
        render_params.m_MaxInstances = 8;
        render_params.m_MaxBatches = 1;
        render_params.m_MaxCharacters = 8;
        HRenderContext render = NewRenderContext(graphics, render_params);

        HRenderCamera camera = NewRenderCamera(render);
        RenderCameraData camera_data = {};
        camera_data.m_AspectRatio = 2;
        camera_data.m_Fov = 1;
        camera_data.m_NearZ = 0.1f;
        camera_data.m_FarZ = 100;
        SetRenderCameraData(render, camera, &camera_data);
        dmVMath::Point3 position(0, 0, 0);
        dmVMath::Quat rotation = dmVMath::Quat::identity();
        UpdateRenderCamera(render, camera, &position, &rotation);

        char code[4096];
        dmSnPrintf(code, sizeof(code),
                   "function init(self)\n"
                   " self.frame=0; self.pred=render.predicate({'bench'}); self.matrix=vmath.matrix4()\n"
                   " self.options={}; self.small=render.constant_buffer(); self.large=render.constant_buffer()\n"
                   " self.small.tint={}; self.large.tint={}\n"
                   " for i=1,%u do self.small.tint[i]=vmath.vector4(i,0,0,1) end\n"
                   " for i=1,%u do self.large.tint[i]=vmath.vector4(i,0,0,1) end\n"
                   " self.clear={[graphics.BUFFER_TYPE_COLOR0_BIT]=vmath.vector4(0,0,0,1)}\n"
                   "end\n"
                   "function update(self,dt)\n"
                   " self.frame=self.frame+1\n"
                   " %s\n"
                   " %s\n"
                   " %s\n"
                   " render.clear(self.clear)\n"
                   " for i=1,%u do render.draw(self.pred,self.options) end\n"
                   " render.set_camera()\n"
                   " self.matrix.m03=self.frame; render.set_view(self.matrix)\n"
                   "end\n",
                   workload.m_Vectors, growth ? 256 : 0,
                   workload.m_Camera ? "render.set_camera(camera.get_cameras()[1],{use_frustum=true})" : "",
                   workload.m_Frustum ? "self.options.frustum=vmath.matrix4()" : "",
                   growth ? "self.options.constants=self.frame==2 and self.large or self.small" :
                   workload.m_Vectors ? "self.options.constants=self.small" : "",
                   workload.m_Draws);
        dmLuaDDF::LuaSource source = {};
        source.m_Script.m_Data = (uint8_t*)code;
        source.m_Script.m_Count = strlen(code);
        source.m_Bytecode.m_Data = (uint8_t*)code;
        source.m_Bytecode.m_Count = strlen(code);
        source.m_Bytecode64.m_Data = (uint8_t*)code;
        source.m_Bytecode64.m_Count = strlen(code);
        source.m_Filename = "render-script-benchmark";
        HRenderScript script = NewRenderScript(render, &source);
        Benchmark bench = {};
        bench.m_Render = render;
        bench.m_Instance = NewRenderScriptInstance(render, script);
        bench.m_Phases = phases;
        bool success = InitRenderScriptInstance(bench.m_Instance) == RENDER_SCRIPT_RESULT_OK;
#if !defined(RENDER_BENCHMARK_BASELINE)
        RenderFrameQueueParams params = {};
        const RenderFrameLimits component_reserve = { 256 * 1024, 1024, 1024 };
        success = success && CalculateRenderFrameLimits(render, component_reserve, &params.m_Limits) == RENDER_FRAME_OK;
        params.m_Execution = GetRenderExecution(render);
        params.m_Consume = Consume;
        params.m_ConsumeContext = &bench;
        bench.m_Queue = NewRenderFrameQueue(params);
        success = success && bench.m_Queue;
#endif
        const uint32_t warmup = growth ? 4 : WARMUP;
        for (uint32_t i = 0; success && i < warmup; ++i)
        {
#if !defined(RENDER_BENCHMARK_BASELINE)
            uint64_t begin = dmTime::GetMonotonicTime();
#endif
            success = RunFrame(&bench);
#if !defined(RENDER_BENCHMARK_BASELINE)
            if (i == 0 || growth)
            {
                printf("MEMORY,%s,%u,%llu,%llu,%llu,%u,%llu\n", workload.m_Name, i + 1,
                       (unsigned long long)GetRenderFrameQueueStorageSize(params.m_Limits),
                       (unsigned long long)bench.m_Statistics.m_DynamicPayloadBytes,
                       (unsigned long long)bench.m_Statistics.m_DynamicPayloadCapacity,
                       bench.m_Statistics.m_DynamicAllocationCount,
                       (unsigned long long)(dmTime::GetMonotonicTime() - begin));
            }
#endif
        }
        bench.m_RecordTime = bench.m_ConsumeTime = bench.m_RetireTime = 0;
        double samples[BATCHES] = {};
        if (!growth)
        {
            for (uint32_t batch = 0; success && batch < BATCHES; ++batch)
            {
                uint64_t begin = dmTime::GetMonotonicTime();
                for (uint32_t i = 0; success && i < BATCH_FRAMES; ++i)
                {
                    success = RunFrame(&bench);
                }
                samples[batch] = (double)(dmTime::GetMonotonicTime() - begin) / BATCH_FRAMES;
                printf("SAMPLE,%s,%u,%.4f\n", workload.m_Name, batch, samples[batch]);
            }
            qsort(samples, BATCHES, sizeof(double), CompareDouble);
            printf("RESULT,%s,%u,%u,%.4f,%.4f,%u\n", workload.m_Name, workload.m_Draws, workload.m_Vectors,
                   samples[BATCHES / 2], samples[BATCHES * 95 / 100], success ? bench.m_FrameCount : 0);
#if !defined(RENDER_BENCHMARK_BASELINE)
            printf("STATS,%s,%llu,%llu,%u,%u\n", workload.m_Name,
                   (unsigned long long)bench.m_Statistics.m_CopiedBytes,
                   (unsigned long long)bench.m_Statistics.m_DynamicPayloadCapacity,
                   bench.m_Statistics.m_AllocationCount, bench.m_Statistics.m_DynamicAllocationCount);
            if (phases)
            {
                printf("PHASES,%s,%.4f,%.4f,%.4f\n", workload.m_Name,
                       (double)bench.m_RecordTime / (BATCHES * BATCH_FRAMES),
                       (double)bench.m_ConsumeTime / (BATCHES * BATCH_FRAMES),
                       (double)bench.m_RetireTime / (BATCHES * BATCH_FRAMES));
            }
#endif
        }
#if !defined(RENDER_BENCHMARK_BASELINE)
        ShutdownRenderFrameQueue(bench.m_Queue);
        DeleteRenderFrameQueue(bench.m_Queue);
#endif
        DeleteRenderScriptInstance(bench.m_Instance);
        DeleteRenderScript(render, script);
        DeleteRenderCamera(render, camera);
        DeleteRenderContext(render, script_context);
        dmScript::Finalize(script_context);
        dmScript::DeleteContext(script_context);
        dmGraphics::DeleteContext(graphics);
        dmPlatform::CloseWindow(window);
        dmPlatform::DeleteWindow(window);
        return success;
    }
}

int RunRenderScriptBenchmark(int argc, char** argv)
{
    bool phases = false;
    for (int i = 1; i < argc; ++i)
    {
        phases = phases || strcmp(argv[i], "--benchmark-phases") == 0;
    }
    const Workload workloads[] = {
        { "clear", 0, 0, 0, 0 },
        { "draw16", 16, 0, 0, 0 },
        { "draw128", 128, 0, 0, 0 },
        { "draw1000", 1000, 0, 0, 0 },
        { "camera128", 128, 0, 1, 1 },
        { "camera1000", 1000, 0, 1, 1 },
        { "constants128x4", 128, 4, 1, 1 },
        { "constants128x256", 128, 256, 1, 1 },
        { "constants1000x32", 1000, 32, 1, 1 },
    };
    printf("BENCHMARK,empty-render-list,null-backend,warmup=%u,batches=%u,frames_per_batch=%u,phases=%u\n",
           WARMUP, BATCHES, BATCH_FRAMES, phases ? 1 : 0);
    for (uint32_t i = 0; i < sizeof(workloads) / sizeof(workloads[0]); ++i)
    {
        if (!RunCase(workloads[i], phases, false))
        {
            return 1;
        }
    }
    const Workload growth = { "small_large_small", 128, 4, 1, 1 };
    return RunCase(growth, false, true) ? 0 : 1;
}
