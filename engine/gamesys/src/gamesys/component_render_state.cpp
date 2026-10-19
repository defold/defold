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

#include <dlib/atomic.h>
#include <dlib/memory.h>
#include <dlib/mutex.h>

#include "component_render_state_private.h"

namespace dmGameSystem
{
    // The producer publishes zero-reference records to this reaper. Renderer state is
    // otherwise renderer-owned; only diagnostics cross owners through atomic counters.
    struct ComponentRenderStateContext
    {
        dmRender::HRenderExecution   m_Execution;
        dmRender::HRenderFrameQueue  m_Queue;
        dmMutex::HMutex             m_ReaperMutex;
        // Head of the m_Next-linked list awaiting render-owner destruction. The game
        // owner enqueues worlds after their last reference is released; the render
        // owner detaches the list under m_ReaperMutex and destroys it outside the lock.
        HComponentRenderWorld       m_Reaper;
        int32_atomic_t              m_WorldCount;
        int32_atomic_t              m_Result;
        // Use full bytes, not bitfields: game/render owners access these independently,
        // and bitfield updates could race through a shared read/modify/write byte.
        uint8_t                     m_HasWorlds;
        uint8_t                     m_InCapture;
        uint8_t                     m_InConsume;
        uint8_t                     m_InService;
    };

    // One lifetime record, independent of the mutable component world. References and
    // Closed are touched only by game; renderer touches only Initialized/State/InitResult.
    // Immutable callbacks/context/init bytes are published through the frame handoff.
    struct ComponentRenderWorld
    {
        ComponentRenderCallbacks     m_Callbacks;
        HComponentRenderStateContext m_Context;
        void*                        m_RendererContext;
        void*                        m_InitData;
        void*                        m_State;
        HComponentRenderWorld        m_Next;
        uint32_t                     m_InitSize;
        uint32_t                     m_References;
        dmRender::RenderFrameResult   m_InitResult;
        // Use full bytes, not bitfields: Closed is game-owned and Initialized is
        // render-owned, so concurrent updates must not read/modify/write the same byte.
        uint8_t                      m_Closed;
        uint8_t                      m_Initialized;
    };

    // Internal dispatch packet in the immutable arena. The world pointer is protected by
    // a frame reference; producer pointers are never stored. Payload tables follow it.
    struct ComponentRenderDispatch
    {
        HComponentRenderWorld m_World;
        uint32_t              m_FirstTable;
        uint32_t              m_TableCount;
    };

    static void RecordError(HComponentRenderStateContext context, dmRender::RenderFrameResult result)
    {
        if (result == dmRender::RENDER_FRAME_OK)
        {
            return;
        }
        if (context->m_ReaperMutex)
        {
            dmAtomicCompareStore32(&context->m_Result, (int32_t)result, dmRender::RENDER_FRAME_OK);
        }
        else if (context->m_Result == dmRender::RENDER_FRAME_OK)
        {
            context->m_Result = result;
        }
    }

    static void ChangeWorldCount(HComponentRenderStateContext context, int32_t change)
    {
        if (context->m_ReaperMutex)
            dmAtomicAdd32(&context->m_WorldCount, change);
        else
            context->m_WorldCount += change;
    }

    HComponentRenderStateContext NewComponentRenderStateContext(dmRender::HRenderExecution execution)
    {
        assert(dmRender::IsRenderExecutionOwner(execution, dmRender::EXECUTION_OWNER_GAME));
        HComponentRenderStateContext context = (HComponentRenderStateContext)calloc(1, sizeof(ComponentRenderStateContext));
        if (!context)
            return 0;

        context->m_Execution = execution;
        if (dmRender::GetRenderExecutionMode(execution) == dmRender::EXECUTION_MODE_THREADED)
        {
            context->m_ReaperMutex = dmMutex::New();
            if (!context->m_ReaperMutex)
            {
                free(context);
                return 0;
            }
        }
        return context;
    }

    void SetComponentRenderStateQueue(HComponentRenderStateContext context, dmRender::HRenderFrameQueue queue)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        assert(!context->m_HasWorlds);
        context->m_Queue = queue;
    }

    uint32_t GetComponentRenderStateWorldCount(HComponentRenderStateContext context)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        return (uint32_t)(context->m_ReaperMutex ? dmAtomicGet32(&context->m_WorldCount) : context->m_WorldCount);
    }

    dmRender::RenderFrameResult GetComponentRenderStateResult(HComponentRenderStateContext context)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        return (dmRender::RenderFrameResult)(context->m_ReaperMutex ? dmAtomicGet32(&context->m_Result) : context->m_Result);
    }

    HComponentRenderWorld NewComponentRenderWorld(HComponentRenderStateContext context, const ComponentRenderCallbacks& callbacks,
        void* renderer_context, const void* init_data, uint32_t init_size)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        assert(!context->m_InCapture);

        if (!callbacks.m_Initialize || !callbacks.m_Consume || !callbacks.m_Destroy ||
            (init_size && !init_data) || GetComponentRenderStateWorldCount(context) == INT32_MAX)
        {
            return 0;
        }

        HComponentRenderWorld world = (HComponentRenderWorld)calloc(1, sizeof(ComponentRenderWorld));
        if (!world)
        {
            return 0;
        }

        if (init_size)
        {
            if (dmMemory::AlignedMalloc(&world->m_InitData, 16, init_size) != dmMemory::RESULT_OK)
            {
                free(world);
                return 0;
            }
            memcpy(world->m_InitData, init_data, init_size);
        }

        world->m_Context = context;
        world->m_Callbacks = callbacks;
        world->m_RendererContext = renderer_context;
        world->m_InitSize = init_size;
        world->m_References = 1;
        context->m_HasWorlds = true;
        ChangeWorldCount(context, 1);
        return world;
    }

    static void ReleaseWorldReference(void* context_ptr, void* resource)
    {
        HComponentRenderStateContext context = (HComponentRenderStateContext)context_ptr;
        HComponentRenderWorld world = (HComponentRenderWorld)resource;
        assert(world->m_References);
        if (--world->m_References)
        {
            return;
        }

        assert(world->m_Closed);
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_ReaperMutex);
            world->m_Next = context->m_Reaper;
            context->m_Reaper = world;
        }

        // Wake after releasing the reaper lock. If the renderer's TryLock raced this
        // publication, the later wake guarantees another service opportunity.
        if (context->m_Queue)
        {
            RecordError(context, dmRender::RequestRenderFrameService(context->m_Queue));
        }
    }

    void DeleteComponentRenderWorld(HComponentRenderWorld world)
    {
        HComponentRenderStateContext context = world->m_Context;
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        assert(!context->m_InCapture && !world->m_Closed);
        world->m_Closed = true;
        ReleaseWorldReference(context, world);
    }

    dmRender::RenderFrameResult CaptureComponentRenderWorld(HComponentRenderWorld world, dmRender::HRenderFrame frame,
        ComponentRenderWriteInputs write_inputs, void* producer_context)
    {
        HComponentRenderStateContext context = world->m_Context;
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        assert(!context->m_InCapture);

        if (world->m_Closed)
        {
            return dmRender::FailRenderFrame(frame, dmRender::RENDER_FRAME_INVALID_STATE);
        }
        if (!write_inputs)
        {
            return dmRender::FailRenderFrame(frame, dmRender::RENDER_FRAME_INVALID_ARGUMENT);
        }
        if (world->m_References == UINT32_MAX)
        {
            return dmRender::FailRenderFrame(frame, dmRender::RENDER_FRAME_CAPACITY_EXCEEDED);
        }

        uint32_t dispatch_index;
        void* data;
        dmRender::RenderFrameResult result = dmRender::AllocateRenderFrameTable(frame, COMPONENT_RENDER_DISPATCH_TABLE_TYPE, 1, sizeof(ComponentRenderDispatch), 16, &dispatch_index, &data);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            return result;
        }

        ++world->m_References;
        result = dmRender::AddRenderFrameReference(frame, world, ReleaseWorldReference, context);
        if (result != dmRender::RENDER_FRAME_OK)
        {
            --world->m_References;
            return result;
        }

        ComponentRenderDispatch* dispatch = (ComponentRenderDispatch*)data;
        dispatch->m_World = world;
        dispatch->m_FirstTable = dispatch_index + 1;
        dispatch->m_TableCount = 0;
        context->m_InCapture = true;
        result = write_inputs(frame, producer_context);
        context->m_InCapture = false;

        if (result != dmRender::RENDER_FRAME_OK)
        {
            return dmRender::FailRenderFrame(frame, result);
        }

        dispatch->m_TableCount = dmRender::GetRenderFrameStatistics(frame).m_TableCount - dispatch->m_FirstTable;
        return dmRender::RENDER_FRAME_OK;
    }

    void ConsumeComponentRenderFrame(void* context_ptr, const dmRender::RenderFrameView& frame)
    {
        HComponentRenderStateContext context = (HComponentRenderStateContext)context_ptr;
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_RENDER));
        assert(!context->m_InConsume && !context->m_InService);

        context->m_InConsume = true;
        for (uint32_t i = 0; i < frame.m_Statistics.m_TableCount; ++i)
        {
            const dmRender::RenderFrameTable& table = frame.m_Tables[i];
            if (table.m_Type != COMPONENT_RENDER_DISPATCH_TABLE_TYPE)
            {
                continue;
            }

            // CaptureComponentRenderWorld writes one dispatch record per captured world;
            // component instances live in separate payload tables. A different count
            // would require changing this consumer to process multiple dispatch records.
            assert(table.m_Count == 1 && table.m_Stride == sizeof(ComponentRenderDispatch));
            const ComponentRenderDispatch* dispatch = (const ComponentRenderDispatch*)dmRender::GetRenderFrameTableData(frame, i);
            assert(dispatch->m_FirstTable == i + 1 && dispatch->m_TableCount <= frame.m_Statistics.m_TableCount - i - 1);

            HComponentRenderWorld world = dispatch->m_World;
            assert(world->m_Context == context);

            i += dispatch->m_TableCount;
            if (!world->m_Initialized)
            {
                world->m_Initialized = true;
                world->m_InitResult = world->m_Callbacks.m_Initialize(world->m_RendererContext, world->m_InitData, world->m_InitSize, &world->m_State);
                RecordError(context, world->m_InitResult);

                if (world->m_InitResult != dmRender::RENDER_FRAME_OK && world->m_State)
                {
                    world->m_Callbacks.m_Destroy(world->m_RendererContext, world->m_State);
                    world->m_State = 0;
                }
            }
            // Failed initialization is a surfaced fatal error, not a successful empty draw.
            // Never retry it; continue other accepted entries to preserve their side effects.
            if (world->m_InitResult == dmRender::RENDER_FRAME_OK)
                RecordError(context, world->m_Callbacks.m_Consume(world->m_RendererContext, world->m_State, frame, dispatch->m_FirstTable, dispatch->m_TableCount));
        }
        context->m_InConsume = false;
    }

    void ServiceComponentRenderState(void* context_ptr)
    {
        HComponentRenderStateContext context = (HComponentRenderStateContext)context_ptr;
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_RENDER));
        assert(!context->m_InConsume && !context->m_InService);

        if (context->m_ReaperMutex && !dmMutex::TryLock(context->m_ReaperMutex))
        {
            return;
        }

        HComponentRenderWorld world = context->m_Reaper;
        context->m_Reaper = 0;
        if (context->m_ReaperMutex)
        {
            dmMutex::Unlock(context->m_ReaperMutex);
        }

        context->m_InService = true;
        while (world)
        {
            HComponentRenderWorld next = world->m_Next;
            if (world->m_Initialized && world->m_InitResult == dmRender::RENDER_FRAME_OK)
            {
                world->m_Callbacks.m_Destroy(world->m_RendererContext, world->m_State);
            }
            dmMemory::AlignedFree(world->m_InitData);
            free(world);
            ChangeWorldCount(context, -1);
            world = next;
        }
        context->m_InService = false;
    }

    void DeleteComponentRenderStateContext(HComponentRenderStateContext context)
    {
        assert(dmRender::IsRenderExecutionOwner(context->m_Execution, dmRender::EXECUTION_OWNER_GAME));
        assert(GetComponentRenderStateWorldCount(context) == 0);

        // All pumps have stopped/joined; no queue callback can still reach this context.
        assert(!context->m_InCapture && !context->m_InConsume && !context->m_InService);
        if (context->m_ReaperMutex)
        {
            dmMutex::Delete(context->m_ReaperMutex);
        }
        free(context);
    }
}
