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

#include <dlib/condition_variable.h>
#include <dlib/memory.h>
#include <dlib/mutex.h>

#include "render_graphics_service.h"

namespace dmRender
{
    static const uint32_t INVALID_REQUEST = 0xffffffffu;

    // A slot remains charged to both budgets until its game-owner callback returns.
    struct GraphicsRequest
    {
        FGraphicsTaskExecute  m_Execute;
        FGraphicsTaskComplete m_Complete;
        void*                 m_CompleteContext;
        void*                 m_Payload;
        uint32_t              m_PayloadSize;
        uint32_t              m_Next;
        int32_t               m_Result;
    };

    // Persistent requests are independent of frame storage and outlive component worlds.
    // Queue links/counters use the mutex; only the render owner touches the active payload.
    struct GraphicsService
    {
        GraphicsServiceParams                   m_Params;
        uint64_t                                m_Revision;
        uint64_t                                m_Accepted;
        uint64_t                                m_Executed;
        uint64_t                                m_Cancelled;
        uint64_t                                m_Completed;
        HRenderExecution                        m_Execution;
        GraphicsRequest*                        m_Requests;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        uint32_t                                m_FreeHead;
        uint32_t                                m_QueueHead;
        uint32_t                                m_QueueTail;
        uint32_t                                m_CompleteHead;
        uint32_t                                m_CompleteTail;
        uint32_t                                m_Active;
        uint32_t                                m_QueuedCount;
        uint32_t                                m_CompleteCount;
        uint32_t                                m_PendingCount;
        uint32_t                                m_PayloadBytes;
        // Full bytes: game/render owners write these independently; bitfields could
        // share a read-modify-write storage unit and introduce a data race.
        uint8_t                                 m_InPump;
        uint8_t                                 m_InDispatch;
        uint8_t                                 m_AcknowledgementPending;
        uint8_t                                 m_Stopping;
    };

    static void Changed(HGraphicsService service)
    {
        ++service->m_Revision;
        if (service->m_Changed)
        {
            dmConditionVariable::Broadcast(service->m_Changed);
        }
    }

    static void Wake(HGraphicsService service)
    {
        if (service->m_Params.m_Wake)
        {
            service->m_Params.m_Wake(service->m_Params.m_WakeContext);
        }
    }

    static bool TryLock(HGraphicsService service)
    {
        return !service->m_Mutex || dmMutex::TryLock(service->m_Mutex);
    }

    static void Unlock(HGraphicsService service)
    {
        if (service->m_Mutex)
        {
            dmMutex::Unlock(service->m_Mutex);
        }
    }

    HGraphicsService NewGraphicsService(const GraphicsServiceParams& params, ExecutionSelection* selection)
    {
        HRenderExecution execution = NewRenderExecution(params.m_Mode, params.m_ExecutorAvailable, selection);
        if (!execution)
        {
            return 0;
        }
        if (!params.m_MaxRequests)
        {
            DeleteRenderExecution(execution);
            return 0;
        }

        GraphicsService* service = (GraphicsService*)calloc(1, sizeof(GraphicsService));
        GraphicsRequest* requests = (GraphicsRequest*)calloc(params.m_MaxRequests, sizeof(GraphicsRequest));
        if (!service || !requests)
        {
            free(requests);
            free(service);
            DeleteRenderExecution(execution);
            return 0;
        }

        service->m_Params = params;
        service->m_Execution = execution;
        service->m_Requests = requests;
        service->m_Revision = 1;
        service->m_FreeHead = 0;
        service->m_QueueHead = INVALID_REQUEST;
        service->m_QueueTail = INVALID_REQUEST;
        service->m_CompleteHead = INVALID_REQUEST;
        service->m_CompleteTail = INVALID_REQUEST;
        service->m_Active = INVALID_REQUEST;
        for (uint32_t i = 0; i < params.m_MaxRequests; ++i)
        {
            requests[i].m_Next = i + 1 < params.m_MaxRequests ? i + 1 : INVALID_REQUEST;
        }
        if (GetRenderExecutionMode(execution) == EXECUTION_MODE_THREADED)
        {
            service->m_Mutex = dmMutex::New();
            service->m_Changed = dmConditionVariable::New();
        }
        return service;
    }

    HRenderExecution GetGraphicsServiceExecution(HGraphicsService service)
    {
        return service->m_Execution;
    }

    dmGraphics::HContext GetGraphicsServiceContext(HGraphicsService service)
    {
        return service->m_Params.m_GraphicsContext;
    }

    GraphicsServiceResult EnqueueGraphicsTask(HGraphicsService service, const GraphicsTask& task)
    {
        return EnqueueGraphicsTask(service, task, 0, 0);
    }

    static GraphicsServiceResult EnqueueLocked(HGraphicsService service, const GraphicsTask& task, const void* data, uint32_t data_size, uint32_t payload_size)
    {
        if (service->m_Stopping)
        {
            return GRAPHICS_SERVICE_STOPPED;
        }
        if (service->m_FreeHead == INVALID_REQUEST || (uint64_t)payload_size + service->m_PayloadBytes > service->m_Params.m_MaxPayloadBytes)
        {
            return GRAPHICS_SERVICE_WOULD_BLOCK;
        }

        // Allocate only after admission. The owner uses TryLock while this producer
        // copies, so browser main never waits for an upload copy or allocator.
        void* payload = 0;
        if (payload_size)
        {
            dmMemory::Result result = dmMemory::AlignedMalloc(&payload, 16, payload_size);
            if (result != dmMemory::RESULT_OK)
            {
                return GRAPHICS_SERVICE_OUT_OF_MEMORY;
            }
        }
        if (task.m_PayloadSize)
        {
            memcpy(payload, task.m_Payload, task.m_PayloadSize);
        }
        if (data_size)
        {
            memcpy((uint8_t*)payload + task.m_PayloadSize, data, data_size);
        }

        uint32_t index = service->m_FreeHead;
        GraphicsRequest& request = service->m_Requests[index];
        service->m_FreeHead = request.m_Next;
        request.m_Execute = task.m_Execute;
        request.m_Complete = task.m_Complete;
        request.m_CompleteContext = task.m_CompleteContext;
        request.m_Payload = payload;
        request.m_PayloadSize = payload_size;
        request.m_Next = INVALID_REQUEST;
        if (service->m_QueueTail == INVALID_REQUEST)
        {
            service->m_QueueHead = index;
        }
        else
        {
            service->m_Requests[service->m_QueueTail].m_Next = index;
        }
        service->m_QueueTail = index;
        ++service->m_QueuedCount;
        ++service->m_PendingCount;
        ++service->m_Accepted;
        service->m_PayloadBytes += payload_size;
        Changed(service);
        return GRAPHICS_SERVICE_OK;
    }

    GraphicsServiceResult EnqueueGraphicsTask(HGraphicsService service, const GraphicsTask& task, const void* data, uint32_t data_size)
    {
        assert(service->m_Mutex || IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME));
        uint64_t payload_size = (uint64_t)task.m_PayloadSize + data_size;
        if (!task.m_Execute || (task.m_PayloadSize && !task.m_Payload) || (data_size && !data) || payload_size > service->m_Params.m_MaxPayloadBytes)
        {
            return GRAPHICS_SERVICE_INVALID_ARGUMENT;
        }
        if (IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_RENDER))
        {
            // An execute callback may enqueue more work, including on browser main.
            // Re-entry must not wait for a producer that currently owns the queue lock.
            if (!TryLock(service))
            {
                return GRAPHICS_SERVICE_WOULD_BLOCK;
            }
        }
        else if (service->m_Mutex)
        {
            dmMutex::Lock(service->m_Mutex);
        }
        GraphicsServiceResult result = EnqueueLocked(service, task, data, data_size, (uint32_t)payload_size);
        Unlock(service);
        if (result == GRAPHICS_SERVICE_OK)
        {
            Wake(service);
        }
        return result;
    }

    static void AcknowledgeExecution(HGraphicsService service)
    {
        uint32_t index = service->m_Active;
        service->m_Requests[index].m_Next = INVALID_REQUEST;
        if (service->m_CompleteTail == INVALID_REQUEST)
        {
            service->m_CompleteHead = index;
        }
        else
        {
            service->m_Requests[service->m_CompleteTail].m_Next = index;
        }
        service->m_CompleteTail = index;
        service->m_Active = INVALID_REQUEST;
        service->m_AcknowledgementPending = false;
        ++service->m_CompleteCount;
        ++service->m_Executed;
        Changed(service);
    }

    GraphicsServicePumpResult PumpGraphicsService(HGraphicsService service, uint32_t max_tasks)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_RENDER));
        if (service->m_InPump)
        {
            return GRAPHICS_SERVICE_PUMP_BUSY;
        }
        service->m_InPump = true;
        uint32_t executed = 0;
        bool progressed = false;
        for (;;)
        {
            if (!TryLock(service))
            {
                // Keep the active result private until a later pump can publish it.
                // An external executor needs another callback even if no new work arrives.
                service->m_InPump = false;
                Wake(service);
                return GRAPHICS_SERVICE_PUMP_BUSY;
            }
            if (service->m_AcknowledgementPending)
            {
                AcknowledgeExecution(service);
                progressed = true;
            }
            if (executed == max_tasks || service->m_QueueHead == INVALID_REQUEST)
            {
                bool queued = service->m_QueueHead != INVALID_REQUEST;
                bool drained = service->m_Stopping && !queued;
                Unlock(service);
                service->m_InPump = false;
                // An external executor may have coalesced every enqueue into this pump.
                // Preserve a future wake when the finite budget leaves accepted work.
                if (queued && max_tasks)
                {
                    Wake(service);
                }
                return drained ? GRAPHICS_SERVICE_PUMP_DRAINED : progressed ? GRAPHICS_SERVICE_PUMP_PROGRESSED : GRAPHICS_SERVICE_PUMP_IDLE;
            }

            uint32_t index = service->m_QueueHead;
            GraphicsRequest& request = service->m_Requests[index];
            service->m_QueueHead = request.m_Next;
            if (service->m_QueueHead == INVALID_REQUEST)
            {
                service->m_QueueTail = INVALID_REQUEST;
            }
            --service->m_QueuedCount;
            service->m_Active = index;
            Changed(service);
            Unlock(service);

            request.m_Result = request.m_Execute(service->m_Params.m_GraphicsContext, request.m_Payload, request.m_PayloadSize);
            service->m_AcknowledgementPending = true;
            ++executed;
        }
    }

    uint32_t DispatchGraphicsCompletions(HGraphicsService service)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME));
        if (service->m_InDispatch || (!service->m_Mutex && service->m_InPump))
        {
            return 0;
        }
        service->m_InDispatch = true;
        uint32_t count;
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(service->m_Mutex);
            count = service->m_CompleteCount;
        }
        // Freeze the batch before calling user code. Callback-generated work can finish
        // immediately inline, but it belongs to a later game-owner service boundary.
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t index;
            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(service->m_Mutex);
                index = service->m_CompleteHead;
                service->m_CompleteHead = service->m_Requests[index].m_Next;
                if (service->m_CompleteHead == INVALID_REQUEST)
                {
                    service->m_CompleteTail = INVALID_REQUEST;
                }
                --service->m_CompleteCount;
            }

            GraphicsRequest& request = service->m_Requests[index];
            if (request.m_Complete)
            {
                request.m_Complete(request.m_CompleteContext, request.m_Result, request.m_Payload, request.m_PayloadSize);
            }
            dmMemory::AlignedFree(request.m_Payload);

            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(service->m_Mutex);
                service->m_PayloadBytes -= request.m_PayloadSize;
                --service->m_PendingCount;
                ++service->m_Completed;
                request.m_Next = service->m_FreeHead;
                service->m_FreeHead = index;
                Changed(service);
            }
        }
        service->m_InDispatch = false;
        return count;
    }

    void StopGraphicsService(HGraphicsService service)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME));
        assert(!service->m_InDispatch && (service->m_Mutex || !service->m_InPump));
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(service->m_Mutex);
            if (service->m_Stopping)
            {
                return;
            }
            service->m_Stopping = true;
            Changed(service);
        }
        Wake(service);
    }

    void CancelGraphicsService(HGraphicsService service, int32_t result)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME));
        assert(!service->m_InDispatch && (service->m_Mutex || !service->m_InPump));
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(service->m_Mutex);
            if (service->m_Stopping && !service->m_QueuedCount)
            {
                return;
            }
            service->m_Stopping = true;
            // Move queued requests only. The render owner may still be using its active
            // payload, so it publishes that result through the ordinary acknowledgement.
            if (service->m_QueueHead != INVALID_REQUEST)
            {
                for (uint32_t index = service->m_QueueHead; index != INVALID_REQUEST; index = service->m_Requests[index].m_Next)
                {
                    service->m_Requests[index].m_Result = result;
                }
                if (service->m_CompleteTail == INVALID_REQUEST)
                {
                    service->m_CompleteHead = service->m_QueueHead;
                }
                else
                {
                    service->m_Requests[service->m_CompleteTail].m_Next = service->m_QueueHead;
                }
                service->m_CompleteTail = service->m_QueueTail;
                service->m_CompleteCount += service->m_QueuedCount;
                service->m_Cancelled += service->m_QueuedCount;
                service->m_QueuedCount = 0;
                service->m_QueueHead = INVALID_REQUEST;
                service->m_QueueTail = INVALID_REQUEST;
            }
            Changed(service);
        }
        Wake(service);
    }

    GraphicsServiceStatus GetGraphicsServiceStatus(HGraphicsService service)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME));
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(service->m_Mutex);
        GraphicsServiceStatus status;
        status.m_Revision = service->m_Revision;
        status.m_Accepted = service->m_Accepted;
        status.m_Executed = service->m_Executed;
        status.m_Cancelled = service->m_Cancelled;
        status.m_Completed = service->m_Completed;
        status.m_PendingRequests = service->m_PendingCount;
        status.m_QueuedRequests = service->m_QueuedCount;
        status.m_ExecutingRequests = service->m_Active != INVALID_REQUEST;
        status.m_PayloadBytes = service->m_PayloadBytes;
        status.m_Stopping = service->m_Stopping;
        return status;
    }

    void WaitForGraphicsServiceWork(HGraphicsService service)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_RENDER) && service->m_Mutex);
        if (service->m_AcknowledgementPending)
        {
            return;
        }
        DM_MUTEX_SCOPED_LOCK(service->m_Mutex);
        while (!service->m_Stopping && service->m_QueueHead == INVALID_REQUEST)
        {
            dmConditionVariable::Wait(service->m_Changed, service->m_Mutex);
        }
    }

    void WaitForGraphicsServiceProgress(HGraphicsService service, uint64_t revision)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME) && service->m_Mutex);
        DM_MUTEX_SCOPED_LOCK(service->m_Mutex);
        while (service->m_Revision == revision && !(service->m_Stopping && !service->m_PendingCount))
        {
            dmConditionVariable::Wait(service->m_Changed, service->m_Mutex);
        }
    }

    void DeleteGraphicsService(HGraphicsService service)
    {
        assert(IsRenderExecutionOwner(service->m_Execution, EXECUTION_OWNER_GAME));
        assert(service->m_Stopping && !service->m_PendingCount && !service->m_InPump && !service->m_InDispatch);
        if (service->m_Changed)
        {
            dmConditionVariable::Delete(service->m_Changed);
            dmMutex::Delete(service->m_Mutex);
        }
        DeleteRenderExecution(service->m_Execution);
        free(service->m_Requests);
        free(service);
    }
}
