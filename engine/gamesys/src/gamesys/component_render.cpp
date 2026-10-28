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
#include "component_render_state_private.h"

namespace dmGameSystem
{
    struct ComponentRenderTypeBinding;

    // Game-owned adapter between collection lifecycle and the independent render manager.
    // Bindings remain stable until all worlds/frames and renderer callbacks have drained.
    struct ComponentRenderContext
    {
        dmRender::RenderFrameTicket       m_Capture;
        dmRender::RenderFrameLimits       m_Limits;
        uint64_t                          m_NextWorldId;
        dmGameObject::HRegister           m_Register;
        dmRender::HRenderExecution        m_Execution;
        HComponentRenderStateContext      m_StateContext;
        dmRender::HRenderFrameQueue       m_Queue;
        ComponentRenderTypeBinding*       m_Types;
    };

    // One game-side binding per registered component type. Passed as the context to
    // gameobject's bridge callbacks so they can reach the type's capture/render callbacks
    // and owning ComponentRenderContext. Kept in m_Types until context teardown.
    struct ComponentRenderTypeBinding
    {
        ComponentRenderType         m_Type;
        ComponentRenderContext*     m_Context;
        ComponentRenderTypeBinding* m_Next;
    };

    // Stack-only trampoline data, never retained by the renderer or by the frame.
    struct ComponentRenderCaptureContext
    {
        ComponentRenderCaptureParams m_Params;
        FComponentRenderCapture      m_Capture;
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
        *render_world                 = NewComponentRenderWorld(owner->m_StateContext, binding->m_Type.m_RenderCallbacks, binding->m_Type.m_RenderContext, &info, sizeof(info));
        return *render_world != 0;
    }

    static void DeleteWorld(void* context, void* render_world)
    {
        (void)context;
        DeleteComponentRenderWorld((HComponentRenderWorld)render_world);
    }

    static dmRender::RenderFrameResult WriteInputs(dmRender::HRenderFrame frame, void* context)
    {
        ComponentRenderCaptureContext* capture = (ComponentRenderCaptureContext*)context;
        capture->m_Params.m_Frame = frame;
        return capture->m_Capture(capture->m_Params);
    }

    static bool CaptureWorld(void* context, void* render_world, const dmGameObject::ComponentRenderCaptureParams& params)
    {
        ComponentRenderTypeBinding* binding = (ComponentRenderTypeBinding*)context;
        ComponentRenderCaptureContext capture;
        capture.m_Capture = binding->m_Type.m_Capture;
        capture.m_Params.m_Collection = params.m_Collection;
        capture.m_Params.m_World = params.m_World;
        capture.m_Params.m_Context = params.m_Context;
        return CaptureComponentRenderWorld((HComponentRenderWorld)render_world, (dmRender::HRenderFrame)params.m_Frame, WriteInputs, &capture) == dmRender::RENDER_FRAME_OK;
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
        context->m_StateContext = NewComponentRenderStateContext(execution);

        if (!context->m_StateContext)
        {
            free(context);
            return 0;
        }

        return context;
    }

    static dmGameObject::Result PrepareComponentRenderType(HComponentRenderContext context,
        const ComponentRenderType& type, ComponentRenderTypeBinding** out_binding, bool* created_queue)
    {
        if (!type.m_Capture || !type.m_RenderCallbacks.m_Initialize ||
            !type.m_RenderCallbacks.m_Consume || !type.m_RenderCallbacks.m_Destroy)
        {
            return dmGameObject::RESULT_INVALID_OPERATION;
        }

        ComponentRenderTypeBinding* binding = (ComponentRenderTypeBinding*)malloc(sizeof(ComponentRenderTypeBinding));
        if (!binding)
        {
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        *created_queue = !context->m_Queue;
        if (*created_queue)
        {
            dmRender::RenderFrameQueueParams params = {};
            params.m_Execution = context->m_Execution;
            params.m_Limits = context->m_Limits;
            params.m_Consume = ConsumeComponentRenderFrame;
            params.m_ConsumeContext = context->m_StateContext;
            params.m_Service = ServiceComponentRenderState;
            params.m_ServiceContext = context->m_StateContext;
            context->m_Queue = dmRender::NewRenderFrameQueue(params);

            if (!context->m_Queue)
            {
                free(binding);
                return dmGameObject::RESULT_UNKNOWN_ERROR;
            }

            SetComponentRenderStateQueue(context->m_StateContext, context->m_Queue);
        }

        binding->m_Context = context;
        binding->m_Type = type;
        *out_binding = binding;
        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::ComponentRenderCallbacks GetBridgeCallbacks(ComponentRenderTypeBinding* binding)
    {
        dmGameObject::ComponentRenderCallbacks callbacks;
        callbacks.m_Context = binding;
        callbacks.m_CreateWorld = CreateWorld;
        callbacks.m_DeleteWorld = DeleteWorld;
        callbacks.m_Capture = CaptureWorld;
        return callbacks;
    }

    static dmGameObject::Result FinishComponentRenderRegistration(HComponentRenderContext context,
        ComponentRenderTypeBinding* binding, bool created_queue, dmGameObject::Result result)
    {
        if (result != dmGameObject::RESULT_OK)
        {
            free(binding);
            if (created_queue)
            {
                // This unpublished queue has never been exposed to an executor and has
                // no frames or worlds. Failed registration must not allocate an idle pipeline.
                SetComponentRenderStateQueue(context->m_StateContext, 0);
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

    dmGameObject::Result RegisterComponentRenderType(HComponentRenderContext context,
        uint32_t component_index, const ComponentRenderType& type)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (component_index >= dmGameObject::GetNumComponentTypes(context->m_Register))
        {
            return dmGameObject::RESULT_INVALID_OPERATION;
        }

        ComponentRenderTypeBinding* binding;
        bool created_queue;
        dmGameObject::Result result = PrepareComponentRenderType(context, type, &binding, &created_queue);
        if (result != dmGameObject::RESULT_OK)
        {
            return result;
        }

        result = dmGameObject::RegisterComponentRenderCallbacks(context->m_Register, component_index, GetBridgeCallbacks(binding));
        return FinishComponentRenderRegistration(context, binding, created_queue, result);
    }

    dmGameObject::Result RegisterComponentType(HComponentRenderContext context,
        const dmGameObject::ComponentType& type, const ComponentRenderType* render_type)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (!render_type)
        {
            return dmGameObject::RegisterComponentType(context->m_Register, type);
        }

        ComponentRenderTypeBinding* binding;
        bool created_queue;
        dmGameObject::Result result = PrepareComponentRenderType(context, *render_type, &binding, &created_queue);
        if (result != dmGameObject::RESULT_OK)
        {
            return result;
        }

        result = dmGameObject::RegisterComponentType(context->m_Register, type, GetBridgeCallbacks(binding));
        return FinishComponentRenderRegistration(context, binding, created_queue, result);
    }

    dmRender::RenderFrameResult BeginComponentRenderFrame(HComponentRenderContext context)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        dmRender::RenderFrameResult result = GetComponentRenderResult(context);
        if (result != dmRender::RENDER_FRAME_OK)
            return result;
        if (!context->m_Queue)
            return dmRender::RENDER_FRAME_OK;
        if (context->m_Capture.m_Frame)
            return dmRender::RENDER_FRAME_INVALID_STATE;

        // Reserve before simulation. The renderer owns any wait for producer credit;
        // gamesys uses the same admission path with inline and external consumers.
        result = dmRender::AcquireRenderFrame(context->m_Queue, &context->m_Capture);
        if (result != dmRender::RENDER_FRAME_OK)
            return result;
        // The previous consumer may have failed while admission waited for its slot.
        result = GetComponentRenderResult(context);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            dmRender::CancelRenderFrameCapture(context->m_Queue, context->m_Capture);
            context->m_Capture.m_Frame = 0;
        }
        return result;
    }

    bool RenderComponents(HComponentRenderContext context, dmGameObject::HCollection collection)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (!context->m_Types)
            return dmGameObject::Render(collection);
        if (!context->m_Capture.m_Frame)
            return false;

        if (!dmGameObject::CaptureRenderData(collection, context->m_Capture.m_Frame))
        {
            dmRender::CancelRenderFrameCapture(context->m_Queue, context->m_Capture);
            context->m_Capture.m_Frame = 0;
            return false;
        }

        // A slow reader delays publication of this exact snapshot. Never recapture
        // or discard it just because the previous accepted frame is still in use.
        dmRender::RenderFrameResult result = dmRender::SubmitRenderFrame(context->m_Queue, context->m_Capture);
        if (result != dmRender::RENDER_FRAME_OK)
            dmRender::CancelRenderFrameCapture(context->m_Queue, context->m_Capture);
        context->m_Capture.m_Frame = 0;
        return result == dmRender::RENDER_FRAME_OK && GetComponentRenderResult(context) == dmRender::RENDER_FRAME_OK;
    }

    void EndComponentRenderFrame(HComponentRenderContext context)
    {
        // Non-drawing updates still acquired producer credit before simulation.
        if (context->m_Capture.m_Frame)
        {
            dmRender::CancelRenderFrameCapture(context->m_Queue, context->m_Capture);
            context->m_Capture.m_Frame = 0;
        }
        if (context->m_Queue)
            dmRender::RetireRenderFrames(context->m_Queue);
    }

    dmRender::RenderFrameResult ServiceComponentRenderContext(HComponentRenderContext context)
    {
        if (context->m_Queue)
        {
            dmRender::RenderFrameResult result = dmRender::ServiceRenderFrameQueue(context->m_Queue);
            if (result != dmRender::RENDER_FRAME_OK)
                return result;
        }
        return GetComponentRenderResult(context);
    }

    dmRender::RenderFrameResult ShutdownComponentRenderContext(HComponentRenderContext context)
    {
        if (!context->m_Queue)
            return dmRender::RENDER_FRAME_OK;
        dmRender::RenderFrameResult result = dmRender::ShutdownRenderFrameQueue(context->m_Queue);
        if (result == dmRender::RENDER_FRAME_OK)
            context->m_Capture.m_Frame = 0;
        return result;
    }

    dmRender::HRenderFrameQueue GetComponentRenderQueue(HComponentRenderContext context)
    {
        return context->m_Queue;
    }

    dmRender::RenderFrameResult GetComponentRenderResult(HComponentRenderContext context)
    {
        return GetComponentRenderStateResult(context->m_StateContext);
    }

    uint32_t GetComponentRenderWorldCount(HComponentRenderContext context)
    {
        return GetComponentRenderStateWorldCount(context->m_StateContext);
    }

    void DeleteComponentRenderContext(HComponentRenderContext context)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        DeleteComponentRenderStateContext(context->m_StateContext);
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
