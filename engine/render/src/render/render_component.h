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

#ifndef DM_RENDER_COMPONENT_H
#define DM_RENDER_COMPONENT_H

#include "render_frame_queue.h"

namespace dmRender
{
    struct RenderComponentContext;
    struct RenderComponentWorld;
    typedef RenderComponentContext* HRenderComponentContext;
    typedef RenderComponentWorld* HRenderComponentWorld;

    /**
     * Initialize renderer-owned state on first consumption, on the render owner.
     * Return partial state through state if failure requires cleanup: Destroy is called
     * once for non-null state on failure. Initialization is never retried.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param init_data Owned immutable copy of world initialization data
     * @param init_size Size of init_data in bytes
     * @param state Output renderer state; null is allowed on success
     * @return RENDER_FRAME_OK on success, or a fatal initialization error
     */
    typedef RenderFrameResult (*RenderComponentInitialize)(void* renderer_context, const void* init_data, uint32_t init_size, void** state);

    /**
     * Consume published inputs on the render owner, without accessing mutable game data.
     * Finish all CPU readers before returning; do not retain borrowed frame pointers.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param state State returned by initialization
     * @param frame Immutable view of the complete frame
     * @param first_table First table belonging to this extraction
     * @param table_count Number of contiguous input tables for this extraction
     * @return RENDER_FRAME_OK on success, or a fatal consumption error
     */
    typedef RenderFrameResult (*RenderComponentConsume)(void* renderer_context, void* state, const RenderFrameView& frame, uint32_t first_table, uint32_t table_count);

    /**
     * Destroy renderer state on the render owner after its final frame reference is released.
     * Also used to clean up partial initialization. CPU retirement does not imply GPU completion.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param state State returned by initialization
     */
    typedef void (*RenderComponentDestroy)(void* renderer_context, void* state);

    /**
     * Renderer callbacks copied on world creation; all are required. Callback code and
     * renderer_context must outlive queued frames and deferred world destruction.
     */
    struct RenderComponentCallbacks
    {
        RenderComponentInitialize m_Initialize;
        RenderComponentConsume    m_Consume;
        RenderComponentDestroy    m_Destroy;
    };

    /**
     * Extract owned tables/references on the game owner. Do not publish/reset the frame
     * or reenter the manager. UINT32_MAX is reserved for internal dispatch table types.
     * @param frame Building frame to append inputs to
     * @param producer_context Borrowed producer data, never stored in the frame
     * @return RENDER_FRAME_OK on success; any error rejects the complete capture
     */
    typedef RenderFrameResult (*RenderComponentCapture)(HRenderFrame frame, void* producer_context);

    /**
     * Create a component lifetime manager on the game owner. Creates no thread.
     * @param execution Execution context, which must outlive the manager
     * @return New manager, or null on allocation failure
     */
    HRenderComponentContext NewRenderComponentContext(HRenderExecution execution);

    /**
     * Set the optional service wake target on the game owner, before creating any worlds.
     * Without a queue, the caller must explicitly service deferred destruction.
     * @param context Component lifetime manager
     * @param queue Service queue, or null; a configured queue must outlive all worlds
     */
    void SetRenderComponentQueue(HRenderComponentContext context, HRenderFrameQueue queue);

    /**
     * Delete the manager on the game owner after all worlds are destroyed and pumps stopped/joined.
     * Close game worlds and capture admission, consume/game-retire accepted frames, then
     * service destruction until the world count is zero BEFORE stopping the queue. The
     * queue must remain open for wake requests from final reference releases.
     * @param context Manager with no remaining worlds or active callbacks
     */
    void DeleteRenderComponentContext(HRenderComponentContext context);

    /**
     * Create a lifetime record on the game owner; renderer state is initialized lazily.
     * Copies initialization bytes, but does not adopt/release embedded pointers. Use values
     * or independently retained/read-safe references, not mutable game pointers. Keep any
     * referenced data valid until destruction. Never-consumed worlds skip Initialize/Destroy.
     * @param context Component lifetime manager
     * @param callbacks Required renderer callbacks, copied into the record
     * @param renderer_context Renderer-owned/read-safe context, valid until deferred destruction
     * @param init_data Initialization data, or null when init_size is zero
     * @param init_size Initialization data size in bytes
     * @return Lifetime handle, or null on invalid arguments, capacity or allocation failure
     */
    HRenderComponentWorld NewRenderComponentWorld(HRenderComponentContext context, const RenderComponentCallbacks& callbacks, void* renderer_context, const void* init_data, uint32_t init_size);

    /**
     * Close a world on the game owner. Accepted frames keep it alive independently of game data.
     * @param world Lifetime handle; invalid for further calls after this operation
     */
    void DeleteRenderComponentWorld(HRenderComponentWorld world);

    /**
     * Capture one component world on the game owner. Adds one internal dispatch table and
     * one retained world reference in addition to the producer's payloads.
     * @param world Open lifetime handle
     * @param frame Building frame
     * @param capture Extraction callback
     * @param producer_context Borrowed extraction context, used only during this call
     * @return RENDER_FRAME_OK on success; any error poisons the frame, which must be cancelled
     */
    RenderFrameResult CaptureRenderComponent(HRenderComponentWorld world, HRenderFrame frame, RenderComponentCapture capture, void* producer_context);

    /**
     * Queue-compatible render-owner callback dispatching immutable component inputs.
     * Initialization failure cleans partial state and skips that world's payloads. The
     * first error is retained; other accepted entries still execute after an init/consume error.
     * @param context Component lifetime manager
     * @param frame Published frame view; no game world/context is passed to consumers
     */
    void ConsumeRenderComponents(void* context, const RenderFrameView& frame);

    /**
     * Service deferred world destruction on the render owner, including without drawing.
     * Uses TryLock to detach pending records, then calls destructors outside the lock.
     * @param context Component lifetime manager
     */
    void ServiceRenderComponents(void* context);

    /**
     * Read the number of live renderer-world records on the game owner.
     * @param context Component lifetime manager
     * @return Count including closed worlds awaiting renderer destruction
     */
    uint32_t GetRenderComponentWorldCount(HRenderComponentContext context);

    /**
     * Read the first asynchronous init/consume/service-wake error on the game owner.
     * The caller must observe/report failures; errors remain set until manager deletion.
     * @param context Component lifetime manager
     * @return RENDER_FRAME_OK if no error has occurred, otherwise the first error
     */
    RenderFrameResult GetRenderComponentResult(HRenderComponentContext context);
}

#endif // DM_RENDER_COMPONENT_H
