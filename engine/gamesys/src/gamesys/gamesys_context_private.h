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

#ifndef DM_GAMESYS_CONTEXT_PRIVATE_H
#define DM_GAMESYS_CONTEXT_PRIVATE_H

#include "gamesys.h"
#include "component_render_state_private.h"

namespace dmGameSystem
{
    // Owns built-in contexts, their render bridge and the early-created graphics service.
    // Finalize closes component rendering; the service and type storage survive through
    // factory destruction. Isolated tests may instead borrow a renderer's service.
    struct Context
    {
        ContextParams              m_Params;
        SpriteContext              m_SpriteContext;
        ModelContext               m_ModelContext;
        FactoryContext             m_FactoryContext;
        CollectionFactoryContext   m_CollectionFactoryContext;
        LabelContext               m_LabelContext;
        CollectionProxyContext     m_CollectionProxyContext;
        HComponentRenderContext    m_ComponentRenderContext;
        dmRender::HGraphicsService m_GraphicsService;
        // Unlike nested synchronous reloads, an asynchronous mutation is exclusive
        // until its game-owner completion has published the replacement state.
        uint8_t                    m_AsyncResourceMutationPending : 1;
        uint8_t                    m_GraphicsFramePending : 1;
        uint8_t                    m_OwnsGraphicsService : 1;
        uint8_t                    m_RunWhileIconified : 1;
    };

    // Internal setup for snapshot consumers and isolated executors. The ordinary
    // engine keeps legacy frame preparation until its components have migrated.
    // Borrowed execution and render contexts must outlive Finalize.
    HContext NewContext(const ContextParams& params, dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits, dmRender::RenderFramePreparation preparation);

    // Queued graphics mutations always hold the barrier, including legacy inline
    // execution, until completion. They cannot run inside an unsubmitted graphics frame.
    dmResource::Result BeginRenderResourceMutation(HContext context);
    void EndRenderResourceMutation(HContext context);
}

#endif // DM_GAMESYS_CONTEXT_PRIVATE_H
