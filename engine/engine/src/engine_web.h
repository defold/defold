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
