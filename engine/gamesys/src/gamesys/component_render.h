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
#include <render/render_frame.h>

namespace dmGameSystem
{
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
    typedef dmRender::RenderFrameResult (*FComponentRenderInitialize)(void* renderer_context, const void* init_data, uint32_t init_size, void** state);

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
    typedef dmRender::RenderFrameResult (*FComponentRenderConsume)(void* renderer_context, void* state, const dmRender::RenderFrameView& frame, uint32_t first_table, uint32_t table_count);

    /**
     * Destroy component renderer state on the render owner after its final frame reference is released.
     * Also used to clean up partial initialization. CPU retirement does not imply GPU completion.
     * @param renderer_context Registered renderer-owned/read-safe context
     * @param state State returned by initialization
     */
    typedef void (*FComponentRenderDestroy)(void* renderer_context, void* state);

    /**
     * Component renderer callbacks copied on world creation; all are required. Callback code
     * and renderer_context must outlive queued frames and deferred world destruction.
     * These gamesys functions execute on the render owner, not necessarily the game thread.
     */
    struct ComponentRenderCallbacks
    {
        FComponentRenderInitialize m_Initialize;
        FComponentRenderConsume    m_Consume;
        FComponentRenderDestroy    m_Destroy;
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
    typedef dmRender::RenderFrameResult (*FComponentRenderCapture)(const ComponentRenderCaptureParams& params);

    /**
     * Internal component opt-in, not an SDK interface. m_RenderContext must be
     * renderer-owned/read-safe and outlive deferred destruction; never reuse the mutable
     * game context. Initialize receives a copied ComponentRenderWorldInfo as init_data.
     */
    struct ComponentRenderType
    {
        ComponentRenderCallbacks m_RenderCallbacks;
        FComponentRenderCapture  m_Capture;
        void*                    m_RenderContext;
    };
}

#endif // DM_GAMESYS_COMPONENT_RENDER_H
