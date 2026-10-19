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

#include <assert.h>
#include <stdlib.h>

#include <gameobject/component.h>
#include <gameobject/component_render.h>

#include "component_render.h"

namespace dmGameSystem
{
    struct ComponentRenderTypeBinding;

    // Game-owned adapter between collection lifecycle and the independent render manager.
    // Bindings remain stable until all worlds/frames and renderer callbacks have drained.
    struct ComponentRenderContext
    {
        dmRender::RenderFrameLimits       m_Limits;
        uint64_t                          m_NextWorldId;
        dmGameObject::HRegister           m_Register;
        dmRender::HRenderExecution        m_Execution;
        dmRender::HRenderComponentContext m_Manager;
        dmRender::HRenderFrameQueue       m_Queue;
        ComponentRenderTypeBinding*       m_Types;
    };

    struct ComponentRenderTypeBinding
    {
        ComponentRenderType         m_Type;
        ComponentRenderContext*     m_Context;
        ComponentRenderTypeBinding* m_Next;
    };

    // Stack-only trampoline data, never retained by the renderer or by the frame.
    struct ComponentRenderCaptureContext
    {
        ComponentRenderExtractParams m_Params;
        ComponentRenderExtract       m_Extract;
    };

    static bool CreateWorld(void* context, uint32_t component_index, uint32_t max_instances, uint32_t max_components, void** render_world)
    {
        ComponentRenderTypeBinding* binding = (ComponentRenderTypeBinding*)context;
        ComponentRenderContext* owner = binding->m_Context;

        if (!owner->m_NextWorldId)
        {
            return false;
        }

        ComponentRenderWorldInfo info = {};
        info.m_WorldId                = owner->m_NextWorldId++;
        info.m_ComponentIndex         = component_index;
        info.m_MaxInstances           = max_instances;
        info.m_MaxComponentInstances  = max_components;
        *render_world                 = dmRender::NewRenderComponentWorld(owner->m_Manager, binding->m_Type.m_RenderCallbacks, binding->m_Type.m_RenderContext, &info, sizeof(info));
        return *render_world != 0;
    }

    static void DeleteWorld(void* context, void* render_world)
    {
        (void)context;
        dmRender::DeleteRenderComponentWorld((dmRender::HRenderComponentWorld)render_world);
    }

    static dmRender::RenderFrameResult Capture(dmRender::HRenderFrame frame, void* context)
    {
        ComponentRenderCaptureContext* capture = (ComponentRenderCaptureContext*)context;
        capture->m_Params.m_Frame = frame;
        return capture->m_Extract(capture->m_Params);
    }

    static bool Extract(void* context, void* render_world, const dmGameObject::ComponentRenderExtractParams& params)
    {
        ComponentRenderTypeBinding* binding = (ComponentRenderTypeBinding*)context;
        ComponentRenderCaptureContext capture;
        capture.m_Extract = binding->m_Type.m_Extract;
        capture.m_Params.m_Collection = params.m_Collection;
        capture.m_Params.m_World = params.m_World;
        capture.m_Params.m_Context = params.m_Context;
        return dmRender::CaptureRenderComponent((dmRender::HRenderComponentWorld)render_world, (dmRender::HRenderFrame)params.m_Frame, Capture, &capture) == dmRender::RENDER_FRAME_OK;
    }

    HComponentRenderContext NewComponentRenderContext(dmGameObject::HRegister regist,
        dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits)
    {
        assert(dmRender::IsRenderExecutionOwner(execution, dmRender::EXECUTION_OWNER_GAME));
        ComponentRenderContext* context = (ComponentRenderContext*)calloc(1, sizeof(ComponentRenderContext));

        if (!context)
        {
            return 0;
        }

        context->m_Register = regist;
        context->m_Execution = execution;
        context->m_Limits = limits;
        context->m_NextWorldId = 1;
        context->m_Manager = dmRender::NewRenderComponentContext(execution);

        if (!context->m_Manager)
        {
            free(context);
            return 0;
        }

        return context;
    }

    dmGameObject::Result RegisterComponentRenderType(HComponentRenderContext context,
        uint32_t component_index, const ComponentRenderType& type)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (!type.m_Extract || !type.m_RenderCallbacks.m_Initialize ||
            !type.m_RenderCallbacks.m_Consume || !type.m_RenderCallbacks.m_Destroy ||
            component_index >= dmGameObject::GetNumComponentTypes(context->m_Register))
        {
            return dmGameObject::RESULT_INVALID_OPERATION;
        }

        ComponentRenderTypeBinding* binding = (ComponentRenderTypeBinding*)malloc(sizeof(ComponentRenderTypeBinding));
        if (!binding)
        {
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        bool created_queue = !context->m_Queue;
        if (created_queue)
        {
            dmRender::RenderFrameQueueParams params = {};
            params.m_Execution = context->m_Execution;
            params.m_Limits = context->m_Limits;
            params.m_Consume = dmRender::ConsumeRenderComponents;
            params.m_ConsumeContext = context->m_Manager;
            params.m_Service = dmRender::ServiceRenderComponents;
            params.m_ServiceContext = context->m_Manager;
            context->m_Queue = dmRender::NewRenderFrameQueue(params);

            if (!context->m_Queue)
            {
                free(binding);
                return dmGameObject::RESULT_UNKNOWN_ERROR;
            }

            dmRender::SetRenderComponentQueue(context->m_Manager, context->m_Queue);
        }

        binding->m_Context = context;
        binding->m_Type = type;
        dmGameObject::ComponentRenderCallbacks callbacks;
        callbacks.m_Context = binding;
        callbacks.m_CreateWorld = CreateWorld;
        callbacks.m_DeleteWorld = DeleteWorld;
        callbacks.m_Extract = Extract;
        dmGameObject::Result result = dmGameObject::RegisterComponentRenderCallbacks(context->m_Register, component_index, callbacks);

        if (result != dmGameObject::RESULT_OK)
        {
            free(binding);
            if (created_queue)
            {
                // This unpublished queue has never been exposed to an executor and has
                // no frames or worlds. Failed registration must not allocate an idle pipeline.
                dmRender::SetRenderComponentQueue(context->m_Manager, 0);
                dmRender::StopRenderFrameQueue(context->m_Queue);
                dmRender::DeleteRenderFrameQueue(context->m_Queue);
                context->m_Queue = 0;
            }
            return result;
        }

        binding->m_Next = context->m_Types;
        context->m_Types = binding;
        return dmGameObject::RESULT_OK;
    }

    bool RenderComponentsInline(HComponentRenderContext context, dmGameObject::HCollection collection)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        assert(dmRender::GetRenderExecutionMode(context->m_Execution) == dmRender::EXECUTION_MODE_INLINE);

        if (!context->m_Types)
        {
            return dmGameObject::Render(collection);
        }
        if (dmRender::GetRenderComponentResult(context->m_Manager) != dmRender::RENDER_FRAME_OK)
        {
            return false;
        }

        dmRender::RenderFrameTicket ticket;
        dmRender::RenderFrameResult result = dmRender::BeginRenderFrameCapture(context->m_Queue, &ticket);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            return false;
        }

        if (!dmGameObject::ExtractRenderData(collection, ticket.m_Frame))
        {
            dmRender::CancelRenderFrameCapture(context->m_Queue, ticket);
            return false;
        }

        result = dmRender::PublishRenderFrame(context->m_Queue, ticket);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            dmRender::CancelRenderFrameCapture(context->m_Queue, ticket);
            return false;
        }
        return dmRender::GetRenderComponentResult(context->m_Manager) == dmRender::RENDER_FRAME_OK;
    }

    void RetireComponentRenderFrames(HComponentRenderContext context)
    {
        if (context->m_Queue)
            dmRender::RetireRenderFrames(context->m_Queue);
    }

    void ServiceComponentRenderContext(HComponentRenderContext context)
    {
        if (context->m_Queue)
            dmRender::PumpRenderFrameQueue(context->m_Queue, false);
    }

    dmRender::HRenderFrameQueue GetComponentRenderQueue(HComponentRenderContext context)
    {
        return context->m_Queue;
    }

    dmRender::HRenderComponentContext GetComponentRenderManager(HComponentRenderContext context)
    {
        return context->m_Manager;
    }

    void DeleteComponentRenderContext(HComponentRenderContext context)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (context->m_Queue && dmRender::GetRenderExecutionMode(context->m_Execution) == dmRender::EXECUTION_MODE_INLINE)
        {
            dmRender::RetireRenderFrames(context->m_Queue);
            // Final reference releases must still be allowed to wake renderer cleanup.
            dmRender::PumpRenderFrameQueue(context->m_Queue, false);
            dmRender::StopRenderFrameQueue(context->m_Queue);
            dmRender::PumpRenderFrameQueue(context->m_Queue, false);
        }

        dmRender::DeleteRenderComponentContext(context->m_Manager);
        if (context->m_Queue)
        {
            dmRender::DeleteRenderFrameQueue(context->m_Queue);
        }

        while (context->m_Types)
        {
            ComponentRenderTypeBinding* binding = context->m_Types;
            context->m_Types = binding->m_Next;
            free(binding);
        }
        free(context);
    }
}
