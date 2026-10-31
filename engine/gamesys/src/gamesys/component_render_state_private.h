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

#ifndef DM_GAMESYS_COMPONENT_RENDER_STATE_PRIVATE_H
#define DM_GAMESYS_COMPONENT_RENDER_STATE_PRIVATE_H

#include <gameobject/component.h>
#include <render/render_frame_queue.h>

#include "component_render.h"

namespace dmGameSystem
{
    // Reserved gamesys dispatch table type; component input tables must not use it.
    static const uint32_t COMPONENT_RENDER_DISPATCH_TABLE_TYPE = UINT32_MAX;

    struct ComponentRenderContext;
    struct ComponentRenderStateContext;
    struct ComponentRenderWorld;
    typedef ComponentRenderContext* HComponentRenderContext;
    typedef ComponentRenderStateContext* HComponentRenderStateContext;
    typedef ComponentRenderWorld* HComponentRenderWorld;

    /**
     * Create the internal bridge between collection lifecycle and component rendering.
     * Owned by gamesys, one per register, installed before collection creation. Owns
     * render descriptors, collection-to-lifetime associations and the active capture scope;
     * gameobject only reports world lifecycle and dispatches its ordered Render traversal.
     * Creates no thread or frame buffers.
     * Borrows execution, which must outlive the bridge; also usable by isolated tests.
     * @param regist Game-object register, which must outlive the bridge
     * @param execution Borrowed execution context
     * @param limits Per-slot storage limits used when the first component opts in
     * @return New bridge, or null on allocation failure or unavailable world-owner hooks
     */
    HComponentRenderContext NewComponentRenderContext(dmGameObject::HRegister regist, dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits);

    /**
     * Register a game type and optional render bridge in one operation on the game owner.
     * Failure leaves no new component type, binding or queue behind. Existing bindings
     * and their queue remain unchanged. Called by the owning gamesys context.
     * @param context Component rendering bridge
     * @param type Game-side component type
     * @param render_type Render callbacks, or null to register a legacy game type
     * @return RESULT_OK, or a component/bridge registration error
     */
    dmGameObject::Result RegisterComponentType(HComponentRenderContext context, const dmGameObject::ComponentType& type, const ComponentRenderType* render_type);

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
     * Acquire a frame before simulation. The renderer handles backpressure and any wait.
     * With no registered render types, does not allocate/reserve frame storage.
     * @param context Component rendering bridge
     * @return RENDER_FRAME_OK, or an admission/component error
     */
    dmRender::RenderFrameResult BeginComponentRenderFrame(HComponentRenderContext context);

    /**
     * Capture into caller-owned storage on the game owner without submitting.
     * Recursive gameobject Render calls inherit this gamesys scope; nested root captures
     * are rejected.
     * @param context Component rendering bridge
     * @param collection Root collection to render/capture
     * @param frame Non-null building frame, valid throughout this synchronous call
     * @return True on success; false on invalid frame, nested capture or traversal/capture failure
     */
    bool CaptureComponentRenderData(HComponentRenderContext context, dmGameObject::HCollection collection, dmRender::HRenderFrame frame);

    /**
     * Capture into the admitted frame, then submit the same inputs despite backpressure.
     * Inline consumes now; threaded execution consumes on its render owner.
     * With no registered render types, calls the existing game-object Render path.
     * @param context Component rendering bridge with an admitted frame
     * @param collection Root collection to render/capture
     * @return True on success; false on traversal, capture or renderer failure
     */
    bool RenderComponents(HComponentRenderContext context, dmGameObject::HCollection collection);

    /**
     * Retire completed frame references on the game owner. Call after surrounding
     * render-script/draw work, before destructive PostUpdate. Cancels unused capture on
     * non-drawing updates/early exits. No queue means no work.
     * @param context Component rendering bridge
     */
    void EndComponentRenderFrame(HComponentRenderContext context);

    /**
     * Service deferred world destruction from the game owner without drawing a frame.
     * Inline pumps now; threaded execution wakes the render owner. Call after PostUpdate,
     * including non-drawing updates.
     * @param context Component rendering bridge
     * @return First component renderer or service scheduling error
     */
    dmRender::RenderFrameResult ServiceComponentRenderContext(HComponentRenderContext context);

    /**
     * Drain accepted frames, retire references and finish service, then shut down the
     * registered executor before deleting callback state. All collections must be deleted.
     * Cancels only unpublished work. The renderer owns the executor shutdown contract.
     * @param context Component rendering bridge on the game owner
     * @return RENDER_FRAME_OK when safe to delete, otherwise a queue shutdown error
     */
    dmRender::RenderFrameResult ShutdownComponentRenderContext(HComponentRenderContext context);

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
     * Delete the bridge on the game owner after ShutdownComponentRenderContext.
     * Individual DeleteWorld calls never block to drain. Callback contexts,
     * rendering and resources must remain alive through draining.
     * @param context Component rendering bridge ready for final teardown
     */
    void DeleteComponentRenderContext(HComponentRenderContext context);

    /**
     * Write owned input tables/references on the game owner. Do not publish/reset the frame
     * or reenter the manager. UINT32_MAX is reserved for internal dispatch table types.
     * @param frame Building frame to append inputs to
     * @param producer_context Borrowed producer data, never stored in the frame
     * @return dmRender::RENDER_FRAME_OK on success; any error rejects the complete capture
     */
    typedef dmRender::RenderFrameResult (*FComponentRenderWriteInputs)(dmRender::HRenderFrame frame, void* producer_context);

    /**
     * Create a component lifetime manager on the game owner. Creates no thread.
     * @param execution Execution context, which must outlive the manager
     * @return New manager, or null on allocation failure
     */
    HComponentRenderStateContext NewComponentRenderStateContext(dmRender::HRenderExecution execution);

    /**
     * Set the optional service wake target on the game owner, before creating any worlds.
     * Without a queue, the caller must explicitly service deferred destruction.
     * @param context Component lifetime manager
     * @param queue Service queue, or null; a configured queue must outlive all worlds
     */
    void SetComponentRenderStateQueue(HComponentRenderStateContext context, dmRender::HRenderFrameQueue queue);

    /**
     * Delete the manager on the game owner after all worlds are destroyed and pumps stopped/joined.
     * Close game worlds and capture admission, consume/game-retire accepted frames, then
     * service destruction until the world count is zero BEFORE stopping the queue. The
     * queue must remain open for wake requests from final reference releases.
     * @param context Manager with no remaining worlds or active callbacks
     */
    void DeleteComponentRenderStateContext(HComponentRenderStateContext context);

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
    HComponentRenderWorld NewComponentRenderWorld(HComponentRenderStateContext context, const ComponentRenderCallbacks& callbacks, void* renderer_context, const void* init_data, uint32_t init_size);

    /**
     * Close a world on the game owner. Accepted frames keep it alive independently of game data.
     * @param world Lifetime handle; invalid for further calls after this operation
     */
    void DeleteComponentRenderWorld(HComponentRenderWorld world);

    /**
     * Capture one component world on the game owner. Adds one internal dispatch table and
     * one retained world reference in addition to the producer's payloads.
     * @param world Open lifetime handle
     * @param frame Building frame
     * @param write_inputs Callback writing the component's input tables and references
     * @param producer_context Borrowed capture context, used only during this call
     * @return dmRender::RENDER_FRAME_OK on success; any error poisons the frame, which must be cancelled
     */
    dmRender::RenderFrameResult CaptureComponentRenderWorld(HComponentRenderWorld world, dmRender::HRenderFrame frame, FComponentRenderWriteInputs write_inputs, void* producer_context);

    /**
     * Queue-compatible render-owner callback dispatching immutable component inputs.
     * Initialization failure cleans partial state and skips that world's payloads. The
     * first error is retained; other accepted entries still execute after an init/consume error.
     * @param context Component lifetime manager
     * @param frame Published frame view; no game world/context is passed to consumers
     */
    void ConsumeComponentRenderFrame(void* context, const dmRender::RenderFrameView& frame);

    /**
     * Service deferred world destruction on the render owner, including without drawing.
     * Uses TryLock to detach pending records, then calls destructors outside the lock.
     * @param context Component lifetime manager
     */
    void ServiceComponentRenderState(void* context);

    /**
     * Read the number of live renderer-world records on the game owner.
     * @param context Component lifetime manager
     * @return Count including closed worlds awaiting renderer destruction
     */
    uint32_t GetComponentRenderStateWorldCount(HComponentRenderStateContext context);

    /**
     * Read the first asynchronous init/consume/service-wake error on the game owner.
     * The caller must observe/report failures; errors remain set until manager deletion.
     * @param context Component lifetime manager
     * @return dmRender::RENDER_FRAME_OK if no error has occurred, otherwise the first error
     */
    dmRender::RenderFrameResult GetComponentRenderStateResult(HComponentRenderStateContext context);
}

#endif // DM_GAMESYS_COMPONENT_RENDER_STATE_PRIVATE_H
