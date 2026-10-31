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

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>

#include "render_frame_queue_private.h"

namespace dmRender
{
    static void Changed(HRenderFrameQueue queue)
    {
        ++queue->m_Revision;
        if (queue->m_Changed)
            dmConditionVariable::Broadcast(queue->m_Changed);
    }

    static bool TryLock(HRenderFrameQueue queue)
    {
        return !queue->m_Mutex || dmMutex::TryLock(queue->m_Mutex);
    }

    static void Unlock(HRenderFrameQueue queue)
    {
        if (queue->m_Mutex)
            dmMutex::Unlock(queue->m_Mutex);
    }

    static int32_t FindBuildingSlot(HRenderFrameQueue queue, const RenderFrameTicket& ticket)
    {
        int32_t index = queue->m_Building;
        if (index == -1)
            return -1;
        const RenderFrameSlot& slot = queue->m_Slots[index];
        return slot.m_Frame == ticket.m_Frame && slot.m_FrameId == ticket.m_FrameId ? index : -1;
    }

    HRenderFrameQueue NewRenderFrameQueue(const RenderFrameQueueParams& params)
    {
        assert(IsRenderExecutionOwner(params.m_Execution, EXECUTION_OWNER_GAME));
        if (!params.m_Consume)
            return 0;

        RenderFrameQueueContext* queue = (RenderFrameQueueContext*)calloc(1, sizeof(RenderFrameQueueContext));
        if (!queue)
            return 0;

        queue->m_Params = params;
        queue->m_NextFrameId = 1;
        queue->m_Revision = 1;
        queue->m_Building = -1;
        queue->m_Active = -1;
        queue->m_Completed = -1;

        for (uint32_t i = 0; i < 2; ++i)
        {
            queue->m_Slots[i].m_Frame = NewRenderFrame(params.m_Limits);
            if (!queue->m_Slots[i].m_Frame)
            {
                for (uint32_t j = 0; j < i; ++j)
                {
                    DeleteRenderFrame(queue->m_Slots[j].m_Frame);
                }
                free(queue);
                return 0;
            }
            queue->m_Slots[i].m_State = RENDER_FRAME_FREE;
        }

        if (GetRenderExecutionMode(params.m_Execution) == EXECUTION_MODE_THREADED)
        {
            queue->m_Mutex = dmMutex::New();
            queue->m_Changed = dmConditionVariable::New();
        }

        return queue;
    }

    uint64_t GetRenderFrameQueueStorageSize(const RenderFrameLimits& limits)
    {
        // Synchronization handles and the execution context are not frame-storage allocations.
        return sizeof(RenderFrameQueueContext) + 2 * GetRenderFrameStorageSize(limits);
    }

    static uint64_t GetProgressRevision(HRenderFrameQueue queue)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
        return queue->m_Revision;
    }

    RenderFrameResult AcquireRenderFrame(HRenderFrameQueue queue, RenderFrameTicket* ticket)
    {
        for (;;)
        {
            // Sample before retirement/reservation so a completion between those steps
            // cannot be missed when the threaded producer goes to sleep.
            uint64_t revision = GetProgressRevision(queue);
            RetireRenderFrames(queue);
            RenderFrameResult result = BeginRenderFrameCapture(queue, ticket);
            if (result != RENDER_FRAME_WOULD_BLOCK || !queue->m_Mutex)
                return result;
            WaitForRenderFrameProgress(queue, revision);
        }
    }

    RenderFrameResult SubmitRenderFrame(HRenderFrameQueue queue, const RenderFrameTicket& ticket)
    {
        for (;;)
        {
            uint64_t revision = GetProgressRevision(queue);
            RenderFrameResult result = PublishRenderFrame(queue, ticket);
            if (result != RENDER_FRAME_WOULD_BLOCK || !queue->m_Mutex)
                return result;
            WaitForRenderFrameProgress(queue, revision);
        }
    }

    RenderFrameResult ServiceRenderFrameQueue(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        RenderFrameResult result = RequestRenderFrameService(queue);
        if (result == RENDER_FRAME_OK && !queue->m_Mutex)
            PumpRenderFrameQueue(queue, false);
        return result;
    }

    static bool AllSlotsFree(HRenderFrameQueue queue)
    {
        return queue->m_Slots[0].m_State == RENDER_FRAME_FREE &&
               queue->m_Slots[1].m_State == RENDER_FRAME_FREE;
    }

    RenderFrameResult DrainRenderFrameQueue(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(!queue->m_InRetire);
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
            // Legacy callers may already have drained and joined their executor. Never
            // request new work or wait on that stopped executor.
            if (queue->m_Stopping)
                return AllSlotsFree(queue) && queue->m_ServiceRequested == queue->m_ServiceAcknowledged ?
                    RENDER_FRAME_OK : RENDER_FRAME_INVALID_STATE;
            if (queue->m_Building != -1)
            {
                queue->m_Slots[queue->m_Building].m_State = RENDER_FRAME_RETIRED;
                queue->m_Building = -1;
                ++queue->m_Cancelled;
                Changed(queue);
            }
        }

        for (;;)
        {
            uint64_t revision = GetProgressRevision(queue);
            // Keep requests open: releasing the last reference can enqueue renderer
            // destruction, which must complete before the executor is stopped.
            RetireRenderFrames(queue);
            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
                if (AllSlotsFree(queue))
                    break;
            }
            if (queue->m_Mutex)
                WaitForRenderFrameProgress(queue, revision);
            else
                PumpRenderFrameQueue(queue, true);
        }

        if (queue->m_Params.m_Service)
            RequestRenderFrameService(queue);
        for (;;)
        {
            uint64_t revision = GetProgressRevision(queue);
            if (!queue->m_Mutex && queue->m_Params.m_Service)
                PumpRenderFrameQueue(queue, false);
            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
                if (queue->m_ServiceRequested == queue->m_ServiceAcknowledged)
                {
                    // Close under the same lock as the final completion check so every
                    // request accepted before Stop has finished, not merely started.
                    queue->m_Stopping = true;
                    Changed(queue);
                    return RENDER_FRAME_OK;
                }
            }
            WaitForRenderFrameProgress(queue, revision);
        }
    }

    RenderFrameResult SetRenderFrameExecutorShutdown(HRenderFrameQueue queue,
        FRenderFrameExecutorShutdown shutdown, void* context)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
        if (!queue->m_Mutex || !shutdown || queue->m_ExecutorShutdown || queue->m_Stopping ||
            queue->m_NextFrameId != 1 || queue->m_ServiceRequested != 0)
            return RENDER_FRAME_INVALID_STATE;
        queue->m_ExecutorShutdown = shutdown;
        queue->m_ExecutorShutdownContext = context;
        return RENDER_FRAME_OK;
    }

    RenderFrameResult ShutdownRenderFrameQueue(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        if (queue->m_ExecutorShutdownComplete)
            return RENDER_FRAME_OK;
        // Drain completion acknowledges work, not the executor's final queue access.
        // Require the owner handshake before changing any state on this higher-level path.
        if (queue->m_Mutex && !queue->m_ExecutorShutdown)
            return RENDER_FRAME_INVALID_STATE;
        RenderFrameResult result = DrainRenderFrameQueue(queue);
        if (result != RENDER_FRAME_OK)
            return result;
        if (queue->m_ExecutorShutdown)
            queue->m_ExecutorShutdown(queue->m_ExecutorShutdownContext);
        queue->m_ExecutorShutdownComplete = true;
        return RENDER_FRAME_OK;
    }

    void DeleteRenderFrameQueue(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        // Registered owners must complete their join/detach hook before it is discarded.
        assert(!queue->m_ExecutorShutdown || queue->m_ExecutorShutdownComplete);
        assert(queue->m_Stopping && !queue->m_InRetire && !queue->m_InPump);
        // Caller has stopped/joined the render executor; no concurrent queue access remains.
        for (uint32_t i = 0; i < 2; ++i)
        {
            assert(queue->m_Slots[i].m_State == RENDER_FRAME_FREE);
            DeleteRenderFrame(queue->m_Slots[i].m_Frame);
        }
        if (queue->m_Changed)
            dmConditionVariable::Delete(queue->m_Changed);
        if (queue->m_Mutex)
            dmMutex::Delete(queue->m_Mutex);
        free(queue);
    }

    RenderFrameResult BeginRenderFrameCapture(HRenderFrameQueue queue, RenderFrameTicket* ticket)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(!queue->m_InRetire);
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
        if (queue->m_Stopping)
            return RENDER_FRAME_STOPPED;
        if (queue->m_Building != -1 || queue->m_LeadReserved)
            return RENDER_FRAME_WOULD_BLOCK;
        if (!queue->m_NextFrameId)
            return RENDER_FRAME_CAPACITY_EXCEEDED;
        for (uint32_t i = 0; i < 2; ++i)
        {
            RenderFrameSlot& slot = queue->m_Slots[i];
            if (slot.m_State != RENDER_FRAME_FREE)
                continue;
            slot.m_FrameId = queue->m_NextFrameId++;
            BeginRenderFrame(slot.m_Frame, slot.m_FrameId);
            slot.m_State = RENDER_FRAME_BUILDING;
            queue->m_Building = (int32_t)i;
            queue->m_LeadReserved = queue->m_Active != -1;
            ticket->m_Frame = slot.m_Frame;
            ticket->m_FrameId = slot.m_FrameId;
            Changed(queue);
            return RENDER_FRAME_OK;
        }
        return RENDER_FRAME_WOULD_BLOCK;
    }

    RenderFrameResult PublishRenderFrame(HRenderFrameQueue queue, const RenderFrameTicket& ticket)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(!queue->m_InRetire);
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
            if (queue->m_Stopping)
                return RENDER_FRAME_STOPPED;
            int32_t index = FindBuildingSlot(queue, ticket);
            if (index == -1)
                return RENDER_FRAME_INVALID_STATE;
            // This acknowledgement gate, not the number of free slots alone, bounds lead.
            if (queue->m_Active != -1)
                return RENDER_FRAME_WOULD_BLOCK;
            RenderFrameResult result = SealRenderFrame(ticket.m_Frame);
            if (result != RENDER_FRAME_OK)
                return result;
            queue->m_Slots[index].m_State = RENDER_FRAME_READY;
            queue->m_Building = -1;
            queue->m_Active = index;
            ++queue->m_Published;
            Changed(queue);
        }
        if (!queue->m_Mutex)
        {
            PumpRenderFrameQueue(queue, true);
        }
        return RENDER_FRAME_OK;
    }

    RenderFrameResult CancelRenderFrameCapture(HRenderFrameQueue queue, const RenderFrameTicket& ticket)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(!queue->m_InRetire);
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
            int32_t index = FindBuildingSlot(queue, ticket);
            if (index == -1)
                return RENDER_FRAME_INVALID_STATE;
            queue->m_Slots[index].m_State = RENDER_FRAME_RETIRED;
            queue->m_Building = -1;
            ++queue->m_Cancelled;
            Changed(queue);
        }
        RetireRenderFrames(queue);
        return RENDER_FRAME_OK;
    }

    uint32_t RetireRenderFrames(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(!queue->m_InRetire);
        queue->m_InRetire = true;
        uint32_t count = 0;
        for (uint32_t i = 0; i < 2; ++i)
        {
            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
                if (queue->m_Slots[i].m_State != RENDER_FRAME_RETIRED)
                    continue;
            }
            // Only the game owner can recycle this slot; the consumer has relinquished it.
            // Resource/factory callbacks may take other locks, so never hold the handoff lock.
            ResetRenderFrame(queue->m_Slots[i].m_Frame);
            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
                queue->m_Slots[i].m_State = RENDER_FRAME_FREE;
                ++queue->m_Retired;
                ++count;
                Changed(queue);
            }
        }
        queue->m_InRetire = false;
        return count;
    }

    static void AcknowledgeCompleted(HRenderFrameQueue queue)
    {
        if (queue->m_Completed != -1)
        {
            queue->m_Slots[queue->m_Completed].m_State = RENDER_FRAME_RETIRED;
            queue->m_Completed = -1;
            queue->m_Active = -1;
            queue->m_LeadReserved = false;
            ++queue->m_Consumed;
            Changed(queue);
        }
        if (queue->m_ServiceCompletionPending)
        {
            queue->m_ServiceAcknowledged = queue->m_CompletedService;
            queue->m_ServiceCompletionPending = false;
            Changed(queue);
        }
    }

    RenderFramePumpResult PumpRenderFrameQueue(HRenderFrameQueue queue, bool consume_frame)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_RENDER));
        assert(!queue->m_InPump);
        if (!TryLock(queue))
            return RENDER_FRAME_PUMP_BUSY;
        AcknowledgeCompleted(queue);
        // Clear before invoking the service: a concurrent new request remains pending.
        uint64_t service_generation = queue->m_ServiceRequested;
        bool acknowledge_service = service_generation != queue->m_ServiceAcknowledged;
        queue->m_ServicePending = false;
        int32_t index = consume_frame ? queue->m_Active : -1;
        if (index != -1)
        {
            queue->m_Slots[index].m_State = RENDER_FRAME_READING;
            Changed(queue);
        }
        bool drained = queue->m_Stopping && queue->m_Active == -1;
        Unlock(queue);

        queue->m_InPump = true;
        if (queue->m_Params.m_Service)
            queue->m_Params.m_Service(queue->m_Params.m_ServiceContext);
        if (acknowledge_service)
        {
            queue->m_CompletedService = service_generation;
            queue->m_ServiceCompletionPending = true;
        }
        if (index != -1)
        {
            const RenderFrameView* frame = GetRenderFrameView(queue->m_Slots[index].m_Frame);
            queue->m_Params.m_Consume(queue->m_Params.m_ConsumeContext, *frame);
            queue->m_Completed = index;
        }
        if (index != -1 || acknowledge_service)
        {
            // Browser main must not wait even to acknowledge completion. Keep Reading
            // and service completion pending until a later pump can publish them.
            if (TryLock(queue))
            {
                AcknowledgeCompleted(queue);
                Unlock(queue);
            }
        }
        queue->m_InPump = false;
        if (index != -1)
            return RENDER_FRAME_PUMP_CONSUMED;
        if (drained && queue->m_ServiceCompletionPending)
            return RENDER_FRAME_PUMP_BUSY;
        return drained ? RENDER_FRAME_PUMP_DRAINED : RENDER_FRAME_PUMP_IDLE;
    }

    RenderFrameResult RequestRenderFrameService(HRenderFrameQueue queue)
    {
        // Inline has no synchronization objects. Worker requests must be relayed through
        // the game owner; reject them before touching any mutable queue state.
        if (!queue->m_Mutex && !IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME))
            return RENDER_FRAME_INVALID_STATE;
        // A browser render owner must use the nonblocking pump, never this locking path.
        if (queue->m_Mutex && IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_RENDER))
            return RENDER_FRAME_INVALID_STATE;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
        if (queue->m_Stopping)
            return RENDER_FRAME_STOPPED;
        if (!queue->m_Params.m_Service)
            return RENDER_FRAME_INVALID_STATE;
        if (!queue->m_ServicePending)
        {
            ++queue->m_ServiceRequested;
            queue->m_ServicePending = true;
        }
        Changed(queue);
        return RENDER_FRAME_OK;
    }

    void StopRenderFrameQueue(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(!queue->m_InRetire);
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
            queue->m_Stopping = true;
            if (queue->m_Building != -1)
            {
                queue->m_Slots[queue->m_Building].m_State = RENDER_FRAME_RETIRED;
                queue->m_Building = -1;
                ++queue->m_Cancelled;
            }
            Changed(queue);
        }
        RetireRenderFrames(queue);
    }

    RenderFrameQueueStatus GetRenderFrameQueueStatus(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(queue->m_Mutex);
        RenderFrameQueueStatus status;
        for (uint32_t i = 0; i < 2; ++i)
        {
            status.m_States[i] = queue->m_Slots[i].m_State;
            status.m_FrameIds[i] = queue->m_Slots[i].m_FrameId;
        }
        status.m_Revision = queue->m_Revision;
        status.m_Published = queue->m_Published;
        status.m_Consumed = queue->m_Consumed;
        status.m_Cancelled = queue->m_Cancelled;
        status.m_Retired = queue->m_Retired;
        status.m_Stopping = queue->m_Stopping;
        return status;
    }

    void WaitForRenderFrameProgress(HRenderFrameQueue queue, uint64_t revision)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_GAME));
        assert(queue->m_Mutex); // Never wait in inline mode or on browser main.
        DM_MUTEX_SCOPED_LOCK(queue->m_Mutex);
        while (!queue->m_Stopping && queue->m_Revision == revision)
            dmConditionVariable::Wait(queue->m_Changed, queue->m_Mutex);
    }

    void WaitForRenderFrameWork(HRenderFrameQueue queue)
    {
        assert(IsRenderExecutionOwner(queue->m_Params.m_Execution, EXECUTION_OWNER_RENDER));
        assert(queue->m_Mutex); // Dedicated native executor only, not the external-owner pump.
        DM_MUTEX_SCOPED_LOCK(queue->m_Mutex);
        while (!queue->m_Stopping && !queue->m_ServicePending && !queue->m_ServiceCompletionPending && queue->m_Active == -1)
            dmConditionVariable::Wait(queue->m_Changed, queue->m_Mutex);
    }
}
