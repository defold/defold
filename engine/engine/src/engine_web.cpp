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

#if defined(__EMSCRIPTEN__)

#include <emscripten/emscripten.h>
#include <malloc.h>

#include "engine_private.h"
#include "engine_web.h"
#include <graphics/graphics_packet.h>

extern "C"
{
    // Sampled on demand, not every frame. dlmalloc's mallinfo holds its allocator
    // lock on pthread builds. These figures exclude static data and GPU storage.
    void EMSCRIPTEN_KEEPALIVE dmEngineSampleWebMemory()
    {
        struct mallinfo info = mallinfo();
        EM_ASM({ Module['webAllocator'] = ({allocated:$0, free:$1, arena:$2}); },
            info.uordblks, info.fordblks, info.arena);
    }

    void EMSCRIPTEN_KEEPALIVE dmEngineSetUpdateEnabled(int enabled)
    {
#if defined(__EMSCRIPTEN_PTHREADS__)
        if (dmEngine::QueueWebWindowEvent(5, enabled != 0, 0)) return;
#endif
        dmEngine::SetUpdateEnabled(enabled != 0);
    }

    void EMSCRIPTEN_KEEPALIVE dmEngineSetRenderEnabled(int enabled)
    {
#if defined(__EMSCRIPTEN_PTHREADS__)
        if (dmEngine::QueueWebWindowEvent(6, enabled != 0, 0)) return;
#endif
        dmEngine::SetRenderEnabled(enabled != 0);
    }
}

#endif // __EMSCRIPTEN__

#if defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/threading.h>
#include <emscripten/html5.h>
#include <emscripten/stack.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <render/render_frame.h>

namespace dmEngine
{
    enum WebPhase { WEB_WAITING, WEB_TICK, WEB_RUNNING, WEB_DONE };
    struct WebEvent { uint32_t m_Kind, m_A, m_B; };
    // Separate owner-written buffers; exported only after the worker has joined.
    struct WebScheduleSample { double m_Begin, m_End; uint32_t m_Kind, m_Phase; };
    enum { WEB_SCHEDULE_CAPACITY = 32768 };
    struct WebComponentLoop
    {
        Engine* m_Engine;
        RunLoopParams m_Params;
        pthread_t m_Worker;
        uint32_t m_Phase, m_Stop, m_Lost, m_EventRead, m_EventWrite;
        WebEvent m_Events[64];
        double m_PreviousTick;
        uint64_t m_Ticks, m_Retired, m_NoFrame, m_InputPublished;
        bool m_CompletionDispatch;
        bool m_RetryDispatch, m_CacheWindowOpened;
        uint32_t m_RetryDispatches;
        WebScheduleSample* m_MainSamples;
        WebScheduleSample* m_WorkerSamples;
        uint32_t m_MainCount, m_WorkerCount, m_MainDropped, m_WorkerDropped;
        uint32_t m_StackBytes, m_StackTouchedBytes;
        bool m_MeasureStack;
    };
    static WebComponentLoop* g_Web = 0;

    bool WebComponentActive() { return g_Web != 0; }

    bool WebCanRender()
    {
        return !__atomic_load_n(&g_Web->m_Lost, __ATOMIC_ACQUIRE) &&
            !EM_ASM_INT({ return document.hidden ? 1 : 0; });
    }

    bool QueueWebWindowEvent(uint32_t kind, uint32_t a, uint32_t b)
    {
        if (!g_Web || !emscripten_is_main_browser_thread()) return false;
        // One bounded SPSC event stream. Overflow is an explicit failed run.
        uint32_t write = __atomic_load_n(&g_Web->m_EventWrite, __ATOMIC_RELAXED);
        uint32_t read = __atomic_load_n(&g_Web->m_EventRead, __ATOMIC_ACQUIRE);
        if (write - read == 64)
        {
            dmLogError("Web component event queue overflow");
            __atomic_store_n(&g_Web->m_Stop, 1, __ATOMIC_RELEASE);
            return true;
        }
        WebEvent& event = g_Web->m_Events[write % 64];
        event.m_Kind = kind; event.m_A = a; event.m_B = b;
        __atomic_store_n(&g_Web->m_EventWrite, write + 1, __ATOMIC_RELEASE);
        return true;
    }

    struct BrowserCall { void (*m_Execute)(void*); void* m_Data; };
    static void ExecuteBrowserCall(void* data)
    {
        BrowserCall* call = (BrowserCall*)data;
        assert(emscripten_is_main_browser_thread());
        call->m_Execute(call->m_Data);
    }

    static void AddScheduleSample(WebComponentLoop* loop, bool worker, uint32_t kind, uint32_t phase, double begin, double end);

    void WebDispatchGraphics(void (*execute)(void*), void* data)
    {
        // One synchronous caller: its operands remain alive until acknowledgment.
        // Only the worker waits. Browser main never joins or drains a producer.
        BrowserCall call = {execute, data};
        double begin = g_Web->m_WorkerSamples ? emscripten_get_now() : 0;
        emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VI, ExecuteBrowserCall, &call);
        AddScheduleSample(g_Web, true, 3, 0, begin, g_Web->m_WorkerSamples ? emscripten_get_now() : 0);
    }

    static void AddScheduleSample(WebComponentLoop* loop, bool worker, uint32_t kind, uint32_t phase, double begin, double end)
    {
        WebScheduleSample* samples = worker ? loop->m_WorkerSamples : loop->m_MainSamples;
        if (!samples) return;
        uint32_t& count = worker ? loop->m_WorkerCount : loop->m_MainCount;
        if (count == WEB_SCHEDULE_CAPACITY)
        {
            ++(worker ? loop->m_WorkerDropped : loop->m_MainDropped);
            return;
        }
        WebScheduleSample& sample = samples[count++];
        sample.m_Kind = kind; sample.m_Phase = phase;
        sample.m_Begin = begin; sample.m_End = end;
    }

    static bool DispatchWebUpdate();
    static void WebWorkerReady()
    {
        // Queued notifications can outlive shutdown. Never retain the loop pointer.
        if (g_Web) DispatchWebUpdate();
    }

    static void* WebSimulation(void* data)
    {
        WebComponentLoop* loop = (WebComponentLoop*)data;
        assert(!emscripten_is_main_browser_thread());
        // Opt-in watermark of unused WASM stack, leaving the runtime's bottom
        // cookies and the current frame untouched. Scan only at worker shutdown.
        // This measures deepest writes, not unwritten reservations or JS stack.
        uintptr_t stack_end = emscripten_stack_get_end() + 64;
        uintptr_t paint_end = emscripten_stack_get_current() - 256;
        if (loop->m_MeasureStack)
            for (uintptr_t p = stack_end; p < paint_end; p += sizeof(uint32_t))
                *(volatile uint32_t*)p = 0xa5c39e71;
        AttachWebComponentProducer(loop->m_Engine);
        for (;;)
        {
            while (__atomic_load_n(&loop->m_Phase, __ATOMIC_ACQUIRE) == WEB_WAITING)
                emscripten_futex_wait(&loop->m_Phase, WEB_WAITING, 1000);
            __atomic_store_n(&loop->m_Phase, WEB_RUNNING, __ATOMIC_RELEASE);
            if (__atomic_load_n(&loop->m_Stop, __ATOMIC_ACQUIRE)) break;
            uint32_t read = __atomic_load_n(&loop->m_EventRead, __ATOMIC_RELAXED);
            uint32_t write = __atomic_load_n(&loop->m_EventWrite, __ATOMIC_ACQUIRE);
            while (read != write)
            {
                WebEvent event = loop->m_Events[read++ % 64];
                if (event.m_Kind == 2) __atomic_store_n(&loop->m_Stop, 1, __ATOMIC_RELEASE);
                else ApplyWebWindowEvent(loop->m_Engine, event.m_Kind, event.m_A, event.m_B);
            }
            __atomic_store_n(&loop->m_EventRead, read, __ATOMIC_RELEASE);
            if (__atomic_load_n(&loop->m_Stop, __ATOMIC_ACQUIRE)) break;
            double begin = loop->m_WorkerSamples ? emscripten_get_now() : 0;
            UpdateResult result = loop->m_Params.m_EngineUpdate(loop->m_Engine);
            AddScheduleSample(loop, true, 2, 0, begin, loop->m_WorkerSamples ? emscripten_get_now() : 0);
            if (result != RESULT_OK) break;
            __atomic_store_n(&loop->m_Phase, WEB_WAITING, __ATOMIC_RELEASE);
            if (loop->m_CompletionDispatch && WebHasContinuousFrames(loop->m_Engine))
                emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_V, WebWorkerReady);
        }
        DrainWebComponentFrame(loop->m_Engine);
        if (loop->m_MeasureStack)
        {
            uintptr_t p = stack_end;
            while (p < paint_end && *(volatile uint32_t*)p == 0xa5c39e71) p += sizeof(uint32_t);
            loop->m_StackTouchedBytes = emscripten_stack_get_base() - p;
        }
        __atomic_store_n(&loop->m_Phase, WEB_DONE, __ATOMIC_RELEASE);
        return 0;
    }

    static EM_BOOL ContextLost(int, const void*, void*)
    {
        dmLogError("Web component PoC: WebGL context lost; stopping this run");
        __atomic_store_n(&g_Web->m_Lost, 1, __ATOMIC_RELEASE);
        __atomic_store_n(&g_Web->m_Stop, 1, __ATOMIC_RELEASE);
        return EM_TRUE;
    }

    static bool DispatchWebUpdate()
    {
        WebComponentLoop* loop = g_Web;
        uint32_t phase = __atomic_load_n(&loop->m_Phase, __ATOMIC_ACQUIRE);
        bool visible = WebCanRender();
        bool stopping = __atomic_load_n(&loop->m_Stop, __ATOMIC_ACQUIRE) != 0;
        if (phase == WEB_WAITING && (visible || stopping))
        {
            double now = emscripten_get_now();
            float dt = (float)((now - loop->m_PreviousTick) * 0.001);
            loop->m_PreviousTick = now;
            if (!stopping)
            {
                dmGraphics::UpdateExternalGraphicsWindow(loop->m_Engine->m_GraphicsContext, loop->m_CacheWindowOpened);
                PrepareWebInput(loop->m_Engine, dt);
                ++loop->m_InputPublished;
            }
            AddScheduleSample(loop, false, 1, phase, now, emscripten_get_now());
            __atomic_store_n(&loop->m_Phase, WEB_TICK, __ATOMIC_RELEASE);
            emscripten_futex_wake(&loop->m_Phase, 1);
            return true;
        }
        return false;
    }

    static void ExportSchedule(WebComponentLoop* loop)
    {
        if (!loop->m_MainSamples) return;
        EM_ASM({ Module['webSchedule'] = ({main:[], worker:[], dropped:$0+$1, retries:$2}); }, loop->m_MainDropped, loop->m_WorkerDropped, loop->m_RetryDispatches);
        for (uint32_t owner = 0; owner < 2; ++owner)
        {
            WebScheduleSample* samples = owner ? loop->m_WorkerSamples : loop->m_MainSamples;
            uint32_t count = owner ? loop->m_WorkerCount : loop->m_MainCount;
            for (uint32_t i = 0; i < count; ++i)
            {
                WebScheduleSample& sample = samples[i];
                EM_ASM({ Module['webSchedule'][$0 ? 'worker' : 'main'].push([$1,$2,$3,$4]); },
                    owner, sample.m_Kind, sample.m_Phase, sample.m_Begin, sample.m_End);
            }
        }
        delete[] loop->m_MainSamples;
        delete[] loop->m_WorkerSamples;
    }

    static void WebBrowserTick(void*)
    {
        assert(emscripten_is_main_browser_thread());
        WebComponentLoop* loop = g_Web;
        if (!loop) return;
        uint32_t phase = __atomic_load_n(&loop->m_Phase, __ATOMIC_ACQUIRE);
        if (phase == WEB_DONE)
        {
            // The publication above precedes pthread runtime teardown. Never join
            // on browser main until the nonblocking probe confirms completion.
            int joined = pthread_tryjoin_np(loop->m_Worker, 0);
            if (joined == EBUSY) return;
            assert(joined == 0);
            int action = 0, code = 0;
            loop->m_Params.m_EngineGetResult(loop->m_Engine, &action, &code, 0, 0);
            if (action == RESULT_REBOOT) { dmLogError("Web component PoC does not support reboot"); code = 1; }
            if (__atomic_load_n(&loop->m_Stop, __ATOMIC_ACQUIRE)) code = 1;
            ExportSchedule(loop);
            EM_ASM({ Module['webStack'] = ({reserved:$0, touched:$1, measured:!!$2}); },
                loop->m_StackBytes, loop->m_StackTouchedBytes, loop->m_MeasureStack);
            dmGraphics::DetachExternalGraphicsProducer();
            emscripten_set_webglcontextlost_callback("#canvas", 0, true, 0);
            // Disable event forwarding before freeing engine/Lua state.
            bool broad_components = loop->m_Engine->m_PocWebComponents;
            bool overlapping = loop->m_Engine->m_PocWebOverlap;
            uint64_t updates = loop->m_Engine->m_Stats.m_FrameCount;
            g_Web = 0;
            loop->m_Params.m_EngineDestroy(loop->m_Engine);
            if (loop->m_Params.m_AppDestroy) loop->m_Params.m_AppDestroy(loop->m_Params.m_AppCtx);
            if (broad_components)
                fprintf(stderr, "WEB_POC_DONE code=%d mode=%s updates=%llu input_snapshots=%llu\n",
                    code, overlapping ? "component-web-snapshots" : "component-web-serialized", (unsigned long long)updates, (unsigned long long)loop->m_InputPublished);
            else
                fprintf(stderr, "WEB_POC_DONE code=%d ticks=%llu retired=%llu no_frame=%llu input_snapshots=%llu\n",
                    code, (unsigned long long)loop->m_Ticks, (unsigned long long)loop->m_Retired,
                    (unsigned long long)loop->m_NoFrame, (unsigned long long)loop->m_InputPublished);
            EM_ASM({ Module['webPocResult'] = $0; }, code);
            delete loop; // Pending callbacks read only g_Web, now null.
            emscripten_cancel_main_loop();
            return;
        }
        ++loop->m_Ticks;
        double begin = loop->m_MainSamples ? emscripten_get_now() : 0;
        bool dispatched = DispatchWebUpdate();
        if (PumpWebComponentFrame(loop->m_Engine)) ++loop->m_Retired;
        else ++loop->m_NoFrame;
        // A worker may finish while main consumes its previous snapshot. Retry
        // only if this callback has not already admitted an update: no run-ahead.
        if (loop->m_RetryDispatch && !dispatched && DispatchWebUpdate())
            ++loop->m_RetryDispatches;
        AddScheduleSample(loop, false, 0, phase, begin, loop->m_MainSamples ? emscripten_get_now() : 0);
    }

    static EM_BOOL HiddenService(double, void*)
    {
        if (!g_Web) return EM_FALSE;
        // rAF may stop in a hidden tab. Retire accepted frames and service stop;
        // no new simulation ticks or GL draws are issued while hidden.
        if (!WebCanRender() || __atomic_load_n(&g_Web->m_Stop, __ATOMIC_ACQUIRE)) WebBrowserTick(0);
        return g_Web ? EM_TRUE : EM_FALSE;
    }

    bool StartWebComponentLoop(Engine* engine, const RunLoopParams* params)
    {
        if (engine->m_PocPipeline != dmRender::POC_COMPONENT || !engine->m_PocThreaded) return false;
        assert(emscripten_is_main_browser_thread() && !g_Web);
        WebComponentLoop* loop = new WebComponentLoop;
        memset(loop, 0, sizeof(*loop));
        loop->m_Engine = engine;
        loop->m_Params = *params;
        loop->m_PreviousTick = emscripten_get_now();
        loop->m_CompletionDispatch = dmConfigFile::GetInt(engine->m_Config, "render.poc_web_schedule", 0) == 1;
        loop->m_RetryDispatch = dmConfigFile::GetInt(engine->m_Config, "render.poc_web_schedule", 0) == 2;
        loop->m_CacheWindowOpened = engine->m_PocWebOverlap || dmConfigFile::GetInt(engine->m_Config, "render.poc_web_cache_window", 0) != 0;
        int stack_kb = dmConfigFile::GetInt(engine->m_Config, "render.poc_web_stack_kb", 5120);
        if (stack_kb < 256 || stack_kb > 16384)
        { dmLogFatal("render.poc_web_stack_kb must be between 256 and 16384"); abort(); }
        loop->m_StackBytes = stack_kb * 1024;
        loop->m_MeasureStack = dmConfigFile::GetInt(engine->m_Config, "render.poc_web_stack_measure", 0) != 0;
        if ((loop->m_CompletionDispatch || loop->m_RetryDispatch) && engine->m_PocWebComponents && !engine->m_PocWebOverlap)
        { dmLogFatal("Scheduling experiments require the overlapping sprite path"); abort(); }
        if (dmConfigFile::GetInt(engine->m_Config, "render.poc_web_metrics", 0))
        {
            loop->m_MainSamples = new WebScheduleSample[WEB_SCHEDULE_CAPACITY];
            loop->m_WorkerSamples = new WebScheduleSample[WEB_SCHEDULE_CAPACITY];
        }
        g_Web = loop;
        if (!dmGraphics::StartExternalGraphicsOwner(engine->m_GraphicsContext, WebDispatchGraphics)) abort();
        dmGraphics::UpdateExternalGraphicsWindow(engine->m_GraphicsContext, loop->m_CacheWindowOpened);
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        int result = pthread_attr_setstacksize(&attr, loop->m_StackBytes);
        if (!result) result = pthread_create(&loop->m_Worker, &attr, WebSimulation, loop);
        pthread_attr_destroy(&attr);
        if (result) { dmLogFatal("Web simulation pthread creation failed: %d", result); abort(); }
        emscripten_set_webglcontextlost_callback("#canvas", 0, true, ContextLost);
        fprintf(stderr, "WEB_POC_ACTIVE %s: simulation=pthread graphics=browser-main %s\n",
            engine->m_PocWebOverlap ? "component-web-snapshots" : engine->m_PocWebComponents ? "component-web-serialized" : "component-web-threaded",
            engine->m_PocWebComponents && !engine->m_PocWebOverlap ? "exclusive-handoff" : "slots=2");
        fprintf(stderr, "WEB_POC_SCHEDULE completion_dispatch=%d metrics=%d\n", loop->m_CompletionDispatch, loop->m_MainSamples != 0);
        fprintf(stderr, "WEB_POC_OPTIONS schedule=%d cache_window=%d\n", loop->m_CompletionDispatch ? 1 : loop->m_RetryDispatch ? 2 : 0, loop->m_CacheWindowOpened);
        emscripten_set_timeout_loop(HiddenService, 100, 0);
        emscripten_set_main_loop_arg(WebBrowserTick, 0, 0, 1);
        return true;
    }
}
#endif
