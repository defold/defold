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

#ifndef DM_ENGINE_WEB_H
#define DM_ENGINE_WEB_H
#include <stdint.h>

namespace dmEngine
{
    // Diagnostic record for one admitted update. Main publishes the prefix
    // before waking the worker; worker completes it before publishing WAITING.
    struct WebUpdateDiagnostic
    {
        uint32_t m_Id, m_BrowserTick, m_Source, m_GraphicsCalls;
        double m_DispatchBegin, m_DispatchEnd, m_Wake, m_EventsEnd, m_UpdateEnd;
        double m_OwnerQueue, m_OwnerExecute, m_OwnerReturn, m_MaxOwnerQueue;
        void RecordGraphicsCall(double begin, double owner_begin, double owner_end, double end)
        {
            ++m_GraphicsCalls;
            m_OwnerQueue += owner_begin - begin;
            m_OwnerExecute += owner_end - owner_begin;
            m_OwnerReturn += end - owner_end;
            if (owner_begin - begin > m_MaxOwnerQueue) m_MaxOwnerQueue = owner_begin - begin;
        }
    };

    // Main-thread admission credit: completion can fill a missed browser tick,
    // but fast workers cannot run ahead or accumulate hidden-tab catch-up ticks.
    struct WebFrameAdmission
    {
        bool m_Paced, m_Credit;
        WebFrameAdmission() : m_Paced(false), m_Credit(false) {}
        void BrowserTick(bool visible) { m_Credit = visible; }
        bool Admit(bool ready, bool visible, bool stopping)
        {
            if (!ready || (!visible && !stopping)) return false;
            if (m_Paced && !stopping && !m_Credit) return false;
            m_Credit = false;
            return true;
        }
    };

    // One successful consumption per visible browser tick. A readiness callback
    // may fill an empty tick, but cannot create an unbounded presentation loop.
    struct WebRenderAdmission
    {
        bool m_Credit;
        WebRenderAdmission() : m_Credit(false) {}
        void BrowserTick(bool visible) { m_Credit = visible; }
        bool CanConsume(bool visible, bool stopping) const { return stopping || (visible && m_Credit); }
        void Consumed() { m_Credit = false; }
    };
}

#if defined(__EMSCRIPTEN_PTHREADS__)
namespace dmEngine
{
    struct Engine;
    struct RunLoopParams;
    bool StartWebComponentLoop(Engine* engine, const RunLoopParams* params);
    bool WebComponentActive();
    bool QueueWebWindowEvent(uint32_t kind, uint32_t a, uint32_t b);
    void ApplyWebWindowEvent(Engine* engine, uint32_t kind, uint32_t a, uint32_t b);
    void PrepareWebInput(Engine* engine, float dt);
    bool PumpWebComponentFrame(Engine* engine);
    void DrainWebComponentFrame(Engine* engine);
    void AttachWebComponentProducer(Engine* engine);
    bool WebCanRender();
    bool WebHasContinuousFrames(Engine* engine);
    void WebDispatchGraphics(void (*execute)(void*), void* data);
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

void dmEngineSetUpdateEnabled(int enabled);
void dmEngineSetRenderEnabled(int enabled);

#ifdef __cplusplus
}
#endif

#endif // DM_ENGINE_WEB_H
