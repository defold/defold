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

#ifndef DM_COMPONENT_RENDER_H
#define DM_COMPONENT_RENDER_H

#include "gameobject.h"

namespace dmGameObject
{
    // Game-owner capture inputs, borrowed only for the duration of Extract. The bridge
    // copies immutable render inputs into m_Frame; none of these game pointers may be
    // retained by render-owner callbacks. This header is internal, not an extension SDK.
    struct ComponentRenderExtractParams
    {
        void* m_World;
        void* m_Context;
        void* m_Frame;
        HCollection m_Collection;
    };

    // Game-owner bridge callbacks, separate from the component's existing world hooks.
    // CreateWorld returns an opaque lifetime handle, not renderer-owned mutable state.
    // On failure it cleans up its partial work and leaves *render_world null. DeleteWorld
    // drops the game's ownership; the bridge keeps accepted frames and render state alive
    // until their own completion. The callback context outlives all worlds using it.
    struct ComponentRenderCallbacks
    {
        void* m_Context;
        bool (*m_CreateWorld)(void* context, uint32_t component_index, uint32_t max_instances,
            uint32_t max_components, void** render_world);
        void (*m_DeleteWorld)(void* context, void* render_world);
        bool (*m_Extract)(void* context, void* render_world, const ComponentRenderExtractParams& params);
    };

    // Register against the actual registration-array index, before any collection starts
    // creating worlds. All callbacks are required; duplicate and late registration fail.
    // No ComponentType layout or public extension registration API is changed.
    Result RegisterComponentRenderCallbacks(HRegister regist, uint32_t component_index,
        const ComponentRenderCallbacks& callbacks);

    // Game owner, at the normal render boundary after final transforms and before
    // destructive PostUpdate. Uses Render's existing ordering and enabled proxy traversal.
    // frame must be non-null and remain valid throughout this synchronous capture; recursive
    // Render calls inherit it. Nested ExtractRenderData calls are rejected.
    bool ExtractRenderData(HCollection collection, void* frame);
}

#endif // DM_COMPONENT_RENDER_H
