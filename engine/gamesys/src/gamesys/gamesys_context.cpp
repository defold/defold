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

#include "gamesys_context_private.h"

namespace dmGameSystem
{
    HContext NewContext(const ContextParams& params)
    {
        // Foundation budget, allocated only when a component opts into capture.
        // Component migration will size these limits for real captured workloads.
        const dmRender::RenderFrameLimits limits = { 256 * 1024, 1024, 1024 };
        return NewContext(params, dmRender::GetRenderExecution(params.m_RenderContext), limits);
    }

    HContext NewContext(const ContextParams& params, dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits)
    {
        Context* context = new Context;
        context->m_Params = params;
        context->m_ComponentRenderContext = NewComponentRenderContext(params.m_Register, execution, limits);
        if (!context->m_ComponentRenderContext)
        {
            delete context;
            return 0;
        }
        return context;
    }

    dmGameObject::Result RegisterComponentType(HContext context, const dmGameObject::ComponentType& type, const ComponentRenderType* render_type)
    {
        return RegisterComponentType(context->m_ComponentRenderContext, type, render_type);
    }

    bool RenderComponentsInline(HContext context, dmGameObject::HCollection collection)
    {
        return RenderComponentsInline(context->m_ComponentRenderContext, collection);
    }

    void RetireComponentRenderFrames(HContext context)
    {
        RetireComponentRenderFrames(context->m_ComponentRenderContext);
    }

    dmRender::RenderFrameResult ServiceComponentRendering(HContext context)
    {
        ServiceComponentRenderContext(context->m_ComponentRenderContext);
        return GetComponentRenderResult(context->m_ComponentRenderContext);
    }

    void Finalize(HContext context)
    {
        DeleteComponentRenderContext(context->m_ComponentRenderContext);
        context->m_ComponentRenderContext = 0;
    }

    void DeleteContext(HContext context)
    {
        // Resource types (notably modelc) retain pointers into this allocation.
        // It outlives the renderer bridge and is freed only after the factory.
        delete context;
    }
}
