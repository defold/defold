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

#include "component_render_resource.h"
#include "gamesys_context_private.h"

namespace dmGameSystem
{
    static void ReleaseComponentRenderResource(void* context, void* resource)
    {
        dmResource::Release((dmResource::HFactory)context, resource);
    }

    dmRender::RenderFrameResult RetainComponentRenderResource(dmResource::HFactory factory, dmRender::HRenderFrameStorage frame, void* resource)
    {
        dmResource::IncRef(factory, resource);
        dmRender::RenderFrameResult result = dmRender::AddRenderFrameReference(frame, resource, ReleaseComponentRenderResource, factory);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            dmResource::Release(factory, resource);
        }
        return result;
    }

    // The following payload stays 16-byte aligned after this header. Only completion
    // uses gamesys/game context; execution sees the copied operation bytes alone.
    struct DM_ALIGNED(16) ResourceMutationTask
    {
        dmRender::FGraphicsTaskExecute  m_Execute;
        dmRender::FGraphicsTaskComplete m_Complete;
        void*                           m_CompleteContext;
        HContext                        m_GameSystem;
        uint32_t                        m_PayloadSize;
    };

    static int32_t ExecuteResourceMutation(dmGraphics::HContext graphics, void* payload, uint32_t)
    {
        ResourceMutationTask* request = (ResourceMutationTask*)payload;
        return request->m_Execute(graphics, request->m_PayloadSize ? request + 1 : 0, request->m_PayloadSize);
    }

    static void CompleteResourceMutation(void*, int32_t result, const void* payload, uint32_t)
    {
        const ResourceMutationTask* request = (const ResourceMutationTask*)payload;
        if (request->m_Complete)
        {
            request->m_Complete(request->m_CompleteContext, result, request->m_PayloadSize ? request + 1 : 0, request->m_PayloadSize);
        }
        // Publish the callback's new handle/state before admitting subsequent captures.
        request->m_GameSystem->m_AsyncResourceMutationPending = false;
        EndResourceMutation(request->m_GameSystem);
    }

    dmRender::GraphicsServiceResult EnqueueRenderResourceMutation(HContext context, const dmRender::GraphicsTask& task)
    {
        if (!context->m_ComponentRenderContext || !task.m_Execute || (task.m_PayloadSize && !task.m_Payload))
        {
            return dmRender::GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        dmRender::HGraphicsService service = dmRender::GetGraphicsService(context->m_Params.m_RenderContext);
        if (!service || BeginResourceMutation(context) != dmResource::RESULT_OK)
        {
            return dmRender::GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }

        ResourceMutationTask request;
        request.m_Execute = task.m_Execute;
        request.m_Complete = task.m_Complete;
        request.m_CompleteContext = task.m_CompleteContext;
        request.m_GameSystem = context;
        request.m_PayloadSize = task.m_PayloadSize;
        dmRender::GraphicsTask owned = {};
        owned.m_Execute = ExecuteResourceMutation;
        owned.m_Complete = CompleteResourceMutation;
        owned.m_Payload = &request;
        owned.m_PayloadSize = sizeof(request);
        context->m_AsyncResourceMutationPending = true;
        dmRender::GraphicsServiceResult result = dmRender::EnqueueGraphicsTask(service, owned, task.m_Payload, task.m_PayloadSize);
        if (result != dmRender::GRAPHICS_SERVICE_OK)
        {
            context->m_AsyncResourceMutationPending = false;
            EndResourceMutation(context);
        }
        return result;
    }
}
