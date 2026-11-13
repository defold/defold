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

#ifndef DM_GAMESYS_COMPONENT_RENDER_RESOURCE_H
#define DM_GAMESYS_COMPONENT_RENDER_RESOURCE_H

#include <render/render_frame_storage.h>
#include <render/render_graphics_service.h>
#include <resource/resource.h>

namespace dmGameSystem
{
    struct Context;
    typedef Context* HContext;

    /**
     * Keep a loaded resource alive for captured component inputs. Call on the game owner;
     * cancellation or retirement releases the added reference on that same owner.
     * The factory must outlive the frame, and its mutation guard must protect captured
     * readers from reload/replacement. A reference alone does not freeze resource contents.
     * Retain the resource owning any borrowed arrays or nested dependencies, not an interior
     * pointer. Each call uses one frame-reference slot, including repeated resources.
     * @param factory Factory owning the currently loaded resource
     * @param frame Building frame receiving the retained reference
     * @param resource Loaded resource already held by the caller
     * @return Frame result; failure leaves the caller's reference count unchanged
     */
    dmRender::RenderFrameResult RetainComponentRenderResource(dmResource::HFactory factory, dmRender::HRenderFrameStorage frame, void* resource);

    /**
     * Drain captured readers, then enqueue an owned graphics mutation. Frame admission
     * stays closed until its game-owner completion returns, including failure/cancellation.
     * Only one asynchronous mutation may be pending; synchronous reloads are rejected too.
     * A rejected request reopens admission and has no callback. This is for already owned
     * handles: the caller keeps them and the completion context alive through delivery.
     * Requests after gamesys finalization are rejected before accessing the renderer.
     * The task must not synchronously wait for game work. Finalize drains an owned service;
     * callers must drain borrowed services themselves. Call on the game owner, never browser main in
     * threaded mode; native callers may wait for previously accepted frames to submit.
     * @param context Gamesys context with a graphics service
     * @param task Execute/completion callbacks and copied payload, as for EnqueueGraphicsTask
     * @return Admission result; INVALID_ARGUMENT includes a conflict with current capture
     */
    dmRender::GraphicsServiceResult EnqueueRenderResourceMutation(HContext context, const dmRender::GraphicsTask& task);
}

#endif // DM_GAMESYS_COMPONENT_RENDER_RESOURCE_H
