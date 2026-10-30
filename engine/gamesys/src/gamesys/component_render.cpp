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
#include <string.h>

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <gameobject/component.h>

#include "component_render.h"
#include "component_render_state_private.h"

namespace dmGameSystem
{
    struct ComponentRenderCollection;

    // Game-owned adapter between generic world notifications and the render manager.
    // Type descriptors, collection associations and capture scope belong only to gamesys.
    struct ComponentRenderContext
    {
        dmHashTable32<ComponentRenderCollection*> m_Collections;
        // Indexed by actual gameobject component index; null capture means legacy type.
        dmArray<ComponentRenderType>              m_Types;
        dmRender::RenderFrameTicket               m_Capture;
        dmRender::RenderFrameLimits               m_Limits;
        uint64_t                                  m_NextWorldId;
        dmGameObject::HRegister                   m_Register;
        dmRender::HRenderExecution                m_Execution;
        HComponentRenderStateContext              m_StateContext;
        dmRender::HRenderFrameQueue               m_Queue;
        // Active only during traversal, not while producer credit is merely reserved.
        dmRender::HRenderFrame                    m_ActiveFrame;
    };

    // Game-owner association for one generational collection handle. Lifetime slots
    // are indexed by component type and allocated during world creation, not per frame.
    struct ComponentRenderCollection
    {
        dmArray<HComponentRenderWorld> m_Worlds;
    };

    // Stack-only trampoline data, never retained by the renderer or by the frame.
    struct ComponentRenderCaptureContext
    {
        ComponentRenderCaptureParams m_Params;
        FComponentRenderCapture      m_Capture;
    };

    static ComponentRenderType* GetRenderType(ComponentRenderContext* context, uint32_t component_index)
    {
        return component_index < context->m_Types.Size() && context->m_Types[component_index].m_Capture
            ? &context->m_Types[component_index] : 0;
    }

    static bool OnWorldCreated(void* context, const dmGameObject::ComponentWorldCreateParams& params)
    {
        ComponentRenderContext* owner = (ComponentRenderContext*)context;
        ComponentRenderType* type = GetRenderType(owner, params.m_ComponentIndex);
        if (!type)
            return true;
        if (!owner->m_NextWorldId)
            return false;

        ComponentRenderCollection** entry = owner->m_Collections.Get(params.m_Collection);
        ComponentRenderCollection* collection = entry ? *entry : 0;
        if (!collection)
        {
            collection = new ComponentRenderCollection;
            collection->m_Worlds.EnsureSize(owner->m_Types.Size());
            memset(collection->m_Worlds.Begin(), 0, collection->m_Worlds.Size() * sizeof(HComponentRenderWorld));
        }

        ComponentRenderWorldInfo info = {};
        info.m_WorldId                = owner->m_NextWorldId++;
        info.m_ComponentIndex         = params.m_ComponentIndex;
        info.m_MaxInstances           = params.m_MaxInstances;
        info.m_MaxComponentInstances  = params.m_MaxComponentInstances;

        HComponentRenderWorld world = NewComponentRenderWorld(owner->m_StateContext, type->m_RenderCallbacks, type->m_RenderContext, &info, sizeof(info));
        if (!world)
        {
            if (!entry)
                delete collection;
            return false;
        }

        if (!entry)
        {
            if (owner->m_Collections.Full())
            {
                uint32_t capacity = owner->m_Collections.Capacity() + 8;
                owner->m_Collections.SetCapacity(capacity * 2, capacity);
            }
            owner->m_Collections.Put(params.m_Collection, collection);
        }
        collection->m_Worlds[params.m_ComponentIndex] = world;
        return true;
    }

    static void OnWorldDeleted(void* context, dmGameObject::HCollection handle, uint32_t component_index)
    {
        ComponentRenderContext* owner = (ComponentRenderContext*)context;
        if (!GetRenderType(owner, component_index))
            return;
        ComponentRenderCollection** entry = owner->m_Collections.Get(handle);
        // Failed creation still receives deletion notifications for attempted worlds.
        if (!entry || component_index >= (*entry)->m_Worlds.Size() || !(*entry)->m_Worlds[component_index])
            return;
        ComponentRenderCollection* collection = *entry;
        DeleteComponentRenderWorld(collection->m_Worlds[component_index]);
        collection->m_Worlds[component_index] = 0;
        // Trim empty trailing slots without moving the remaining component indices.
        // Each slot is removed once, and Empty replaces a separate live-world counter.
        while (!collection->m_Worlds.Empty() && !collection->m_Worlds.Back())
            collection->m_Worlds.SetSize(collection->m_Worlds.Size() - 1);
        if (collection->m_Worlds.Empty())
        {
            owner->m_Collections.Erase(handle);
            delete collection;
        }
    }

    static dmRender::RenderFrameResult WriteInputs(dmRender::HRenderFrame frame, void* context)
    {
        ComponentRenderCaptureContext* capture = (ComponentRenderCaptureContext*)context;
        capture->m_Params.m_Frame = frame;
        return capture->m_Capture(capture->m_Params);
    }

    static dmGameObject::UpdateResult RenderWorld(void* context, uint32_t component_index,
        const dmGameObject::ComponentsRenderParams& params, dmGameObject::ComponentsRender render)
    {
        ComponentRenderContext* owner = (ComponentRenderContext*)context;
        ComponentRenderType* type = GetRenderType(owner, component_index);
        if (!type)
            return render ? render(params) : dmGameObject::UPDATE_RESULT_OK;
        if (!owner->m_ActiveFrame)
        {
            dmLogError("Component %u requires a render data capture frame.", component_index);
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        ComponentRenderCollection* collection = *owner->m_Collections.Get(params.m_Collection);
        ComponentRenderCaptureContext capture;
        capture.m_Capture = type->m_Capture;
        capture.m_Params.m_Collection = params.m_Collection;
        capture.m_Params.m_World = params.m_World;
        capture.m_Params.m_Context = params.m_Context;
        dmRender::RenderFrameResult result = CaptureComponentRenderWorld(collection->m_Worlds[component_index], owner->m_ActiveFrame, WriteInputs, &capture);
        return result == dmRender::RENDER_FRAME_OK ? dmGameObject::UPDATE_RESULT_OK : dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }

    HComponentRenderContext NewComponentRenderContext(dmGameObject::HRegister regist,
        dmRender::HRenderExecution execution, const dmRender::RenderFrameLimits& limits)
    {
        assert(dmRender::IsRenderExecutionOwner(execution, dmRender::EXECUTION_OWNER_GAME));
        ComponentRenderContext* context = new ComponentRenderContext();

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
            delete context;
            return 0;
        }

        dmGameObject::ComponentWorldCallbacks callbacks;
        callbacks.m_Context = context;
        callbacks.m_OnWorldCreated = OnWorldCreated;
        callbacks.m_OnWorldDeleted = OnWorldDeleted;
        callbacks.m_Render = RenderWorld;
        if (dmGameObject::SetComponentWorldCallbacks(regist, callbacks) != dmGameObject::RESULT_OK)
        {
            DeleteComponentRenderStateContext(context->m_StateContext);
            delete context;
            return 0;
        }
        return context;
    }

    static dmGameObject::Result PrepareComponentRenderType(HComponentRenderContext context,
        const ComponentRenderType& type, bool* created_queue)
    {
        if (dmGameObject::HasComponentWorldCreationStarted(context->m_Register) ||
            !type.m_Capture || !type.m_RenderCallbacks.m_Initialize ||
            !type.m_RenderCallbacks.m_Consume || !type.m_RenderCallbacks.m_Destroy)
        {
            return dmGameObject::RESULT_INVALID_OPERATION;
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
                return dmGameObject::RESULT_UNKNOWN_ERROR;
            }

            SetComponentRenderStateQueue(context->m_StateContext, context->m_Queue);
        }

        return dmGameObject::RESULT_OK;
    }

    static void ReserveTypeSlot(HComponentRenderContext context, uint32_t component_index)
    {
        uint32_t size = context->m_Types.Size();
        if (component_index >= size)
        {
            context->m_Types.EnsureSize(component_index + 1);
            memset(context->m_Types.Begin() + size, 0, (context->m_Types.Size() - size) * sizeof(ComponentRenderType));
        }
    }

    static dmGameObject::Result FinishComponentRenderRegistration(HComponentRenderContext context,
        uint32_t component_index, const ComponentRenderType& type, bool created_queue, dmGameObject::Result result)
    {
        if (result != dmGameObject::RESULT_OK)
        {
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

        context->m_Types[component_index] = type;
        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result RegisterComponentRenderType(HComponentRenderContext context,
        uint32_t component_index, const ComponentRenderType& type)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (component_index >= dmGameObject::GetNumComponentTypes(context->m_Register) || dmGameObject::HasComponentWorldCreationStarted(context->m_Register))
        {
            return dmGameObject::RESULT_INVALID_OPERATION;
        }
        if (GetRenderType(context, component_index))
            return dmGameObject::RESULT_ALREADY_REGISTERED;

        bool created_queue;
        dmGameObject::Result result = PrepareComponentRenderType(context, type, &created_queue);
        if (result != dmGameObject::RESULT_OK)
        {
            return result;
        }

        ReserveTypeSlot(context, component_index);
        return FinishComponentRenderRegistration(context, component_index, type, created_queue, dmGameObject::RESULT_OK);
    }

    dmGameObject::Result RegisterComponentType(HComponentRenderContext context,
        const dmGameObject::ComponentType& type, const ComponentRenderType* render_type)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        if (!render_type)
        {
            return dmGameObject::RegisterComponentType(context->m_Register, type);
        }

        bool created_queue;
        dmGameObject::Result result = PrepareComponentRenderType(context, *render_type, &created_queue);
        if (result != dmGameObject::RESULT_OK)
        {
            return result;
        }

        uint32_t component_index = dmGameObject::GetNumComponentTypes(context->m_Register);
        ReserveTypeSlot(context, component_index);
        result = dmGameObject::RegisterComponentType(context->m_Register, type);
        // All allocation/validation precedes registration. Publishing the gamesys
        // binding after a successful game registration cannot fail.
        return FinishComponentRenderRegistration(context, component_index, *render_type, created_queue, result);
    }

    bool CaptureComponentRenderData(HComponentRenderContext context, dmGameObject::HCollection collection, dmRender::HRenderFrame frame)
    {
        if (!frame || context->m_ActiveFrame)
            return false;
        // Collection proxies recurse through gameobject's Render dispatcher and inherit
        // this scope. Only the root call owns submission; gameobject never sees a frame.
        context->m_ActiveFrame = frame;
        bool result = dmGameObject::Render(collection);
        context->m_ActiveFrame = 0;
        return result;
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
        // Recursive collection traversal uses gameobject Render, never another submit.
        if (context->m_ActiveFrame)
            return false;
        if (!context->m_Queue)
            return dmGameObject::Render(collection);
        if (!context->m_Capture.m_Frame)
            return false;

        if (!CaptureComponentRenderData(context, collection, context->m_Capture.m_Frame))
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

        dmGameObject::ClearComponentWorldCallbacks(context->m_Register);
        delete context;
    }
}
