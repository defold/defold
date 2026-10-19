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

    /**
     * Immutable initialization values copied for each component-world lifetime.
     * Contains no collection handle, game world or mutable game component context.
     */
    struct ComponentRenderWorldInfo
    {
        uint64_t m_WorldId;
        uint32_t m_ComponentIndex;
        uint32_t m_MaxInstances;
        uint32_t m_MaxComponentInstances;
    };

    /**
     * Borrowed game-side extraction inputs. Copy values and retain dependencies into
     * m_Frame; renderer callbacks never receive this structure or its game pointers.
     */
    struct ComponentRenderExtractParams
    {
        dmRender::HRenderFrame    m_Frame;
        void*                     m_World;
        void*                     m_Context;
        dmGameObject::HCollection m_Collection;
    };

    /**
     * Extract a component world's inputs on the game owner.
     * @param params Borrowed game state and building frame
     * @return RENDER_FRAME_OK on success; any error rejects the complete frame capture
     */
    typedef dmRender::RenderFrameResult (*ComponentRenderExtract)(const ComponentRenderExtractParams& params);

    /**
     * Internal component opt-in, not an SDK interface. m_RenderContext must be
     * renderer-owned/read-safe and outlive deferred destruction; never reuse the mutable
     * game context. Initialize receives a copied ComponentRenderWorldInfo as init_data.
     */
    struct ComponentRenderType
    {
        dmRender::RenderComponentCallbacks m_RenderCallbacks;
        ComponentRenderExtract             m_Extract;
        void*                              m_RenderContext;
    };

    /**
     * Create the game-owner bridge between collection lifecycle and component rendering.
     * One context per register. Creates no production thread or frame buffers.
     * @param regist Game-object register, which must outlive this context
     * @param execution Execution context, which must outlive this context
     * @param limits Per-slot storage limits used when the first component opts in
     * @return New context, or null on allocation failure
     */
    HComponentRenderContext NewComponentRenderContext(dmGameObject::HRegister regist, dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits);

    /**
     * Register component render callbacks on the game owner, before world creation or pumping.
     * The first successful registration allocates frame storage; failure releases provisional storage.
     * @param context Component rendering bridge
     * @param component_index Actual component registration index
     * @param type Extraction callback, renderer callbacks and renderer context
     * @return RESULT_OK, or a game-object registration/allocation error
     */
    dmGameObject::Result    RegisterComponentRenderType(HComponentRenderContext context, uint32_t component_index, const ComponentRenderType& type);

    /**
     * Run existing render traversal plus capture/consume for opted-in components, inline only.
     * With no registered render types, calls the existing game-object Render path.
     * @param context Component rendering bridge with inline execution
     * @param collection Root collection to render/extract
     * @return True on success; false on traversal, capture or renderer failure
     */
    bool RenderComponentsInline(HComponentRenderContext context, dmGameObject::HCollection collection);

    /**
     * Retire completed frame references on the game owner. Call after surrounding
     * render-script/draw work, before destructive PostUpdate. No queue means no work.
     * @param context Component rendering bridge
     */
    void RetireComponentRenderFrames(HComponentRenderContext context);

    /**
     * Service deferred world destruction on the render owner without drawing a frame.
     * Call on non-drawing frames too, including after PostUpdate in inline execution.
     * @param context Component rendering bridge
     */
    void ServiceComponentRenderContext(HComponentRenderContext context);

    /**
     * Get the shared frame queue for internal executor/test integration.
     * @param context Component rendering bridge
     * @return Frame queue, or null when no component has opted in
     */
    dmRender::HRenderFrameQueue       GetComponentRenderQueue(HComponentRenderContext context);

    /**
     * Get the manager responsible for independent renderer-world lifetimes.
     * @param context Component rendering bridge
     * @return Component lifetime manager
     */
    dmRender::HRenderComponentContext GetComponentRenderManager(HComponentRenderContext context);

    /**
     * Delete the bridge on the game owner after all collections are destroyed.
     * Inline drains remaining cleanup. Threaded callers must first consume/retire frames,
     * service worlds to zero, then stop/drain/join the executor. Individual DeleteWorld
     * calls never block to drain. Callback contexts, rendering and resources must remain alive.
     * @param context Component rendering bridge ready for final teardown
     */
    void DeleteComponentRenderContext(HComponentRenderContext context);
}

#endif // DM_GAMESYS_COMPONENT_RENDER_H
