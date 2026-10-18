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

#ifndef DM_GAMESYS_COMPONENT_RENDER_H
#define DM_GAMESYS_COMPONENT_RENDER_H

#include <gameobject/gameobject.h>
#include <render/render_component.h>

namespace dmGameSystem
{
    struct ComponentRenderContext;
    typedef ComponentRenderContext* HComponentRenderContext;

    // Immutable initialization values copied for each distinct component-world lifetime.
    // No collection handle, game world or game component context crosses to the renderer.
    struct ComponentRenderWorldInfo
    {
        uint64_t m_WorldId;
        uint32_t m_ComponentIndex;
        uint32_t m_MaxInstances;
        uint32_t m_MaxComponentInstances;
    };

    // Borrowed game-side inputs. Extraction copies values and retains dependencies into
    // m_Frame; renderer callbacks never receive this structure or any of its game pointers.
    struct ComponentRenderExtractParams
    {
        dmRender::HRenderFrame    m_Frame;
        void*                     m_World;
        void*                     m_Context;
        dmGameObject::HCollection m_Collection;
    };

    typedef dmRender::RenderFrameResult (*ComponentRenderExtract)(const ComponentRenderExtractParams& params);

    // Internal opt-in, not the SDK component interface. m_RenderContext must be explicitly
    // renderer-owned/read-safe and outlive deferred destruction; never reuse the game context.
    // Initialize receives a copied ComponentRenderWorldInfo as its init_data.
    struct ComponentRenderType
    {
        dmRender::RenderComponentCallbacks m_RenderCallbacks;
        ComponentRenderExtract             m_Extract;
        void*                              m_RenderContext;
    };

    // Frame buffers are allocated for the first successful type registration; failed
    // registration releases provisional storage again. Register before starting any pumps.
    // One context per register; register/execution outlive it. No production thread is created.
    HComponentRenderContext NewComponentRenderContext(dmGameObject::HRegister regist, dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits);
    dmGameObject::Result    RegisterComponentRenderType(HComponentRenderContext context, uint32_t component_index, const ComponentRenderType& type);

    // Existing render traversal plus capture/consume for opted-in components. Inline
    // only until the platform executor is integrated. With no registrations, calls legacy Render.
    // Retire after the surrounding render-script/draw work, before destructive PostUpdate.
    bool RenderComponentsInline(HComponentRenderContext context, dmGameObject::HCollection collection);
    // Game owner, explicit end-of-render cleanup. No queue means no work.
    void RetireComponentRenderFrames(HComponentRenderContext context);
    // Render owner, also on non-drawing frames and after PostUpdate: deferred world destruction.
    void ServiceComponentRenderContext(HComponentRenderContext context);

    // Internal executor/test access. Null queue means no types have opted in.
    dmRender::HRenderFrameQueue       GetComponentRenderQueue(HComponentRenderContext context);
    dmRender::HRenderComponentContext GetComponentRenderManager(HComponentRenderContext context);

    // Game owner, final teardown only, after all collections are destroyed. Inline drains
    // remaining cleanup itself. Threaded callers must consume/retire/service worlds to zero,
    // stop/drain/join the queue executor first. No blocking drain occurs inside DeleteWorld.
    // Must precede destruction of registered callback contexts, render context and resources.
    void DeleteComponentRenderContext(HComponentRenderContext context);
}

#endif // DM_GAMESYS_COMPONENT_RENDER_H
