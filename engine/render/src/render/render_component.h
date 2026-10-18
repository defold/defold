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

    // Render-owner callbacks. Initialization is lazy on first consumption, with an owned
    // immutable copy of init_data. It must set state before returning if partial cleanup
    // is required: Destroy is called once even on failure when a nonnull state was returned.
    typedef RenderFrameResult (*RenderComponentInitialize)(void* renderer_context, const void* init_data, uint32_t init_size, void** state);
    // Table indices identify this extraction's contiguous table span in the whole frame.
    // Only renderer-owned state/context and immutable published inputs cross this API.
    typedef RenderFrameResult (*RenderComponentConsume)(void* renderer_context, void* state, const RenderFrameView& frame, uint32_t first_table, uint32_t table_count);
    typedef void (*RenderComponentDestroy)(void* renderer_context, void* state);

    // Copied on world creation; all callbacks are required. Callback code and renderer_context
    // must outlive every world, including queued frames and deferred destruction.
    struct RenderComponentCallbacks
    {
        RenderComponentInitialize m_Initialize;
        RenderComponentConsume m_Consume;
        RenderComponentDestroy m_Destroy;
    };

    // Game-owner-only extraction. Its transient producer_context is never stored in a frame.
    // Append owned tables/references, never publish/reset the frame or reenter this manager.
    // UINT32_MAX is reserved for component dispatch tables, not component payload tables.
    typedef RenderFrameResult (*RenderComponentCapture)(HRenderFrame frame, void* producer_context);

    HRenderComponentContext NewRenderComponentContext(HRenderExecution execution);
    // Optional service wake target. Set before creating worlds; queue outlives all worlds
    // and execution outlives the manager. Without a queue, explicitly service destruction.
    void SetRenderComponentQueue(HRenderComponentContext context, HRenderFrameQueue queue);
    // Game owner; all worlds must have completed renderer destruction and all queue pumps
    // must have stopped/joined before deletion (including pumps with no component frames).
    void DeleteRenderComponentContext(HRenderComponentContext context);

    // Game owner creates the lifetime record, not renderer state. init_data is copied once;
    // it may contain values and independently retained/read-safe references, not game pointers.
    // Copying bytes does not adopt/release embedded pointers. Their owner must keep them
    // valid until the record is reaped; a never-initialized world skips Initialize/Destroy.
    HRenderComponentWorld NewRenderComponentWorld(HRenderComponentContext context, const RenderComponentCallbacks& callbacks, void* renderer_context, const void* init_data, uint32_t init_size);
    // Game owner closes the handle; no new capture may use it. Existing frame references
    // survive game-world deletion. The handle becomes invalid after this call.
    void DeleteRenderComponentWorld(HRenderComponentWorld world);
    // Adds one dispatch table and one world reference in addition to producer payloads.
    // Any failure poisons the complete frame; caller must cancel it, never publish a subset.
    RenderFrameResult CaptureRenderComponent(HRenderComponentWorld world, HRenderFrame frame, RenderComponentCapture capture, void* producer_context);

    // Queue-compatible render-owner callbacks. Consume never receives the game world/context.
    // Service uses TryLock and invokes destructors outside the lock, including with no frame.
    void ConsumeRenderComponents(void* context, const RenderFrameView& frame);
    void ServiceRenderComponents(void* context);
    // Game-owner diagnostics: count includes closed worlds awaiting renderer destruction;
    // result preserves the first asynchronous init/consume/service-wake error until deletion.
    uint32_t GetRenderComponentWorldCount(HRenderComponentContext context);
    RenderFrameResult GetRenderComponentResult(HRenderComponentContext context);
    // Initialization failure cleans returned partial state, skips that world's payloads,
    // and records a fatal context error; it is never retried. Other accepted entries still
    // execute, including after a consume error. The game owner must observe/report errors.

    // Shutdown: close game worlds/capture admission, consume and game-retire accepted frames,
    // service destruction until world count is zero, THEN StopRenderFrameQueue/join. The queue
    // must remain open for wake requests generated by final reference releases. CPU world
    // destruction does not imply GPU completion; backend resource retirement remains separate.
}

#endif // DM_RENDER_COMPONENT_H
