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

#include "engine_private.h"
#include "engine_web.h"
#include <graphics/graphics_packet.h>

extern "C"
{
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
    struct WebComponentLoop
    {
        Engine* m_Engine;
        RunLoopParams m_Params;
        pthread_t m_Worker;
        uint32_t m_Phase, m_Stop, m_Lost, m_EventRead, m_EventWrite;
        WebEvent m_Events[64];
        double m_PreviousTick;
        uint64_t m_Ticks, m_Retired, m_NoFrame, m_InputPublished;
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

    void WebDispatchGraphics(void (*execute)(void*), void* data)
    {
        // One synchronous caller: its operands remain alive until acknowledgment.
        // Only the worker waits. Browser main never joins or drains a producer.
        BrowserCall call = {execute, data};
        emscripten_sync_run_in_main_runtime_thread(EM_FUNC_SIG_VI, ExecuteBrowserCall, &call);
    }

    static void* WebSimulation(void* data)
    {
        WebComponentLoop* loop = (WebComponentLoop*)data;
        assert(!emscripten_is_main_browser_thread());
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
            UpdateResult result = loop->m_Params.m_EngineUpdate(loop->m_Engine);
            if (result != RESULT_OK) break;
            __atomic_store_n(&loop->m_Phase, WEB_WAITING, __ATOMIC_RELEASE);
        }
        DrainWebComponentFrame(loop->m_Engine);
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
            dmGraphics::DetachExternalGraphicsProducer();
            emscripten_set_webglcontextlost_callback("#canvas", 0, true, 0);
            // Disable event forwarding before freeing engine/Lua state.
            g_Web = 0;
            loop->m_Params.m_EngineDestroy(loop->m_Engine);
            if (loop->m_Params.m_AppDestroy) loop->m_Params.m_AppDestroy(loop->m_Params.m_AppCtx);
            fprintf(stderr, "WEB_POC_DONE code=%d ticks=%llu retired=%llu no_frame=%llu input_snapshots=%llu\n",
                code, (unsigned long long)loop->m_Ticks, (unsigned long long)loop->m_Retired,
                (unsigned long long)loop->m_NoFrame, (unsigned long long)loop->m_InputPublished);
            EM_ASM({ Module['webPocResult'] = $0; }, code);
            delete loop; // Pending callbacks read only g_Web, now null.
            emscripten_cancel_main_loop();
            return;
        }
        ++loop->m_Ticks;
        bool visible = WebCanRender();
        bool stopping = __atomic_load_n(&loop->m_Stop, __ATOMIC_ACQUIRE) != 0;
        if (phase == WEB_WAITING && (visible || stopping))
        {
            double now = emscripten_get_now();
            float dt = (float)((now - loop->m_PreviousTick) * 0.001);
            loop->m_PreviousTick = now;
            if (!stopping)
            {
                dmGraphics::UpdateExternalGraphicsWindow(loop->m_Engine->m_GraphicsContext);
                PrepareWebInput(loop->m_Engine, dt);
                ++loop->m_InputPublished;
            }
            __atomic_store_n(&loop->m_Phase, WEB_TICK, __ATOMIC_RELEASE);
            emscripten_futex_wake(&loop->m_Phase, 1);
        }
        if (PumpWebComponentFrame(loop->m_Engine)) ++loop->m_Retired;
        else ++loop->m_NoFrame;
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
        g_Web = loop;
        if (!dmGraphics::StartExternalGraphicsOwner(engine->m_GraphicsContext, WebDispatchGraphics)) abort();
        dmGraphics::UpdateExternalGraphicsWindow(engine->m_GraphicsContext);
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 5 * 1024 * 1024);
        int result = pthread_create(&loop->m_Worker, &attr, WebSimulation, loop);
        pthread_attr_destroy(&attr);
        if (result) { dmLogFatal("Web simulation pthread creation failed: %d", result); abort(); }
        emscripten_set_webglcontextlost_callback("#canvas", 0, true, ContextLost);
        fprintf(stderr, "WEB_POC_ACTIVE component-web-threaded: simulation=pthread graphics=browser-main slots=2\n");
        emscripten_set_timeout_loop(HiddenService, 100, 0);
        emscripten_set_main_loop_arg(WebBrowserTick, 0, 0, 1);
        return true;
    }
}
#endif
