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
#include <render/render_frame_queue.h>

namespace dmGameSystem
{
    struct ComponentRenderContext;
    typedef ComponentRenderContext* HComponentRenderContext;

    /**
     * Initialize component renderer state on first consumption, on the render owner.
     * Return partial state through state if failure requires cleanup: Destroy is called
     * once for non-null state on failure. Initialization is never retried.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param init_data Owned immutable copy of world initialization data
     * @param init_size Size of init_data in bytes
     * @param state Output renderer state; null is allowed on success
     * @return RENDER_FRAME_OK on success, or a fatal initialization error
     */
    typedef dmRender::RenderFrameResult (*ComponentRenderInitialize)(void* renderer_context, const void* init_data, uint32_t init_size, void** state);

    /**
     * Consume published component inputs on the render owner, without accessing mutable game data.
     * Finish all CPU readers before returning; do not retain borrowed frame pointers.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param state State returned by initialization
     * @param frame Immutable view of the complete frame
     * @param first_table First table belonging to this capture
     * @param table_count Number of contiguous input tables for this capture
     * @return RENDER_FRAME_OK on success, or a fatal consumption error
     */
    typedef dmRender::RenderFrameResult (*ComponentRenderConsume)(void* renderer_context, void* state, const dmRender::RenderFrameView& frame, uint32_t first_table, uint32_t table_count);

    /**
     * Destroy component renderer state on the render owner after its final frame reference is released.
     * Also used to clean up partial initialization. CPU retirement does not imply GPU completion.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param state State returned by initialization
     */
    typedef void (*ComponentRenderDestroy)(void* renderer_context, void* state);

    /**
     * Component renderer callbacks copied on world creation; all are required. Callback code
     * and renderer_context must outlive queued frames and deferred world destruction.
     * These gamesys functions execute on the render owner, not necessarily the game thread.
     */
    struct ComponentRenderCallbacks
    {
        ComponentRenderInitialize m_Initialize;
        ComponentRenderConsume    m_Consume;
        ComponentRenderDestroy    m_Destroy;
    };

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
     * Borrowed game-side capture inputs. Copy values and retain dependencies into
     * m_Frame; renderer callbacks never receive this structure or its game pointers.
     */
    struct ComponentRenderCaptureParams
    {
        dmRender::HRenderFrame    m_Frame;
        void*                    m_World;
        void*                    m_Context;
        dmGameObject::HCollection m_Collection;
    };

    /**
     * Capture a snapshot of a component world's render inputs on the game owner.
     * Copy values and retain dependencies into the frame for later consumption by the
     * render owner. UINT32_MAX is reserved by gamesys for dispatch tables; use other
     * values for component input table types.
     * @param params Borrowed game state and building frame
     * @return RENDER_FRAME_OK on success; any error rejects the complete frame capture
     */
    typedef dmRender::RenderFrameResult (*ComponentRenderCapture)(const ComponentRenderCaptureParams& params);

    /**
     * Internal component opt-in, not an SDK interface. m_RenderContext must be
     * renderer-owned/read-safe and outlive deferred destruction; never reuse the mutable
     * game context. Initialize receives a copied ComponentRenderWorldInfo as init_data.
     */
    struct ComponentRenderType
    {
        ComponentRenderCallbacks m_RenderCallbacks;
        ComponentRenderCapture   m_Capture;
        void*                    m_RenderContext;
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
     * @param type Render-input capture callback, renderer callbacks and renderer context
     * @return RESULT_OK, or a game-object registration/allocation error
     */
    dmGameObject::Result RegisterComponentRenderType(HComponentRenderContext context, uint32_t component_index, const ComponentRenderType& type);

    /**
     * Run existing render traversal plus capture/consume for opted-in components, inline only.
     * With no registered render types, calls the existing game-object Render path.
     * @param context Component rendering bridge with inline execution
     * @param collection Root collection to render/capture
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
    dmRender::HRenderFrameQueue GetComponentRenderQueue(HComponentRenderContext context);

    /**
     * Read the first component renderer initialization, consumption or service-wake error
     * on the game owner. The error remains set until context deletion.
     * @param context Component rendering bridge
     * @return RENDER_FRAME_OK if no error has occurred, otherwise the first error
     */
    dmRender::RenderFrameResult GetComponentRenderResult(HComponentRenderContext context);

    /**
     * Read the number of component renderer-world records on the game owner.
     * @param context Component rendering bridge
     * @return Count including closed worlds awaiting render-owner destruction
     */
    uint32_t GetComponentRenderWorldCount(HComponentRenderContext context);

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
