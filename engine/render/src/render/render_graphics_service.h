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

#ifndef DM_RENDER_GRAPHICS_SERVICE_H
#define DM_RENDER_GRAPHICS_SERVICE_H

#include <graphics/graphics.h>

#include "render_execution.h"

namespace dmRender
{
    struct GraphicsService;
    typedef GraphicsService* HGraphicsService;

    /** Admission result. Rejected requests keep all their data and receive no completion. */
    enum GraphicsServiceResult
    {
        GRAPHICS_SERVICE_OK,
        GRAPHICS_SERVICE_WOULD_BLOCK,
        GRAPHICS_SERVICE_STOPPED,
        GRAPHICS_SERVICE_INVALID_ARGUMENT,
        GRAPHICS_SERVICE_OUT_OF_MEMORY,
    };

    /** Reserved completion result when shutdown cannot execute queued backend work. */
    enum GraphicsTaskResult
    {
        GRAPHICS_TASK_CANCELLED = -1,
    };

    /** Pump progress. DRAINED means execution finished, not completion delivery or GPU retirement. */
    enum GraphicsServicePumpResult
    {
        GRAPHICS_SERVICE_PUMP_IDLE,
        GRAPHICS_SERVICE_PUMP_PROGRESSED,
        GRAPHICS_SERVICE_PUMP_BUSY,
        GRAPHICS_SERVICE_PUMP_DRAINED,
    };

    /**
     * Execute on the render owner. The writable payload belongs to this request until
     * its game-owner completion returns. Return an operation-specific result; returning
     * does not imply GPU completion. Do not retain the payload for asynchronous GPU work.
     * Callbacks may enqueue more work, but must not wait for game work or shut down the service.
     * @param context Graphics context supplied at service creation
     * @param payload Owned, 16-byte-aligned copy of the submitted bytes, or null for zero bytes
     * @param size Payload size in bytes
     * @return Result delivered unchanged to the completion callback
     */
    typedef int32_t (*FGraphicsTaskExecute)(dmGraphics::HContext context, void* payload, uint32_t size);

    /**
     * Deliver a result on the game owner, outside service locks. May enqueue new work;
     * nested completion dispatch does nothing. Must not wait for another service completion,
     * synchronously load resources, delete or stop the service.
     * @param context Caller-owned context that remains alive until this callback returns
     * @param result Result returned by the execute callback
     * @param payload Executed request data, borrowed only for this callback
     * @param size Payload size in bytes
     */
    typedef void (*FGraphicsTaskComplete)(void* context, int32_t result, const void* payload, uint32_t size);

    /**
     * Schedule a future render-owner pump. Called outside locks, possibly on a worker.
     * Must not pump synchronously, wait, or access game state. The callback and its context
     * must remain alive until producers and the executor have stopped.
     * @param context Wake context supplied at service creation
     */
    typedef void (*FGraphicsServiceWake)(void* context);

    /** Service configuration. Limits cover queued, executing and undelivered requests together. */
    struct GraphicsServiceParams
    {
        dmGraphics::HContext m_GraphicsContext;
        FGraphicsServiceWake m_Wake;
        void*                m_WakeContext;
        ExecutionMode        m_Mode;
        uint32_t             m_MaxRequests;
        uint32_t             m_MaxPayloadBytes;
        uint8_t              m_ExecutorAvailable : 1;
    };

    /**
     * One request. Payloads contain values and byte offsets, not borrowed pointers to
     * mutable data. The completion context is game-owned and never passed to execution.
     */
    struct GraphicsTask
    {
        FGraphicsTaskExecute  m_Execute;
        FGraphicsTaskComplete m_Complete;
        void*                 m_CompleteContext;
        const void*           m_Payload;
        uint32_t              m_PayloadSize;
    };

    /** Synchronized game-owner snapshot. Pending requests include undelivered completions. */
    struct GraphicsServiceStatus
    {
        uint64_t m_Revision;
        uint64_t m_Accepted;
        uint64_t m_Executed;
        uint64_t m_Cancelled;
        uint64_t m_Completed;
        uint32_t m_PendingRequests;
        uint32_t m_QueuedRequests;
        uint32_t m_ExecutingRequests;
        uint32_t m_PayloadBytes;
        uint8_t  m_Stopping : 1;
    };

    /**
     * Create service and execution ownership before resource realization. Does not create
     * a thread or transfer a backend context. A threaded executor binds the returned
     * execution context and establishes backend ownership before pumping. Inline creates
     * no service mutex or condition variable. The graphics context outlives the service.
     * @param params Configuration, with a nonzero request capacity
     * @param selection Required execution-mode selection output
     * @return Service, or null for invalid configuration, unsupported mode or allocation failure
     */
    HGraphicsService NewGraphicsService(const GraphicsServiceParams& params, ExecutionSelection* selection);

    /**
     * Borrow the execution context owned by this service.
     * @param service Service that outlives users of the returned execution context
     * @return Shared game/render ownership context
     */
    HRenderExecution GetGraphicsServiceExecution(HGraphicsService service);

    /**
     * Borrow the graphics context used by task execution.
     * @param service Service to inspect
     * @return Graphics context supplied at creation
     */
    dmGraphics::HContext GetGraphicsServiceContext(HGraphicsService service);

    /**
     * Copy a request without executing or completing it, including inline. Threaded callers
     * may be the game owner or workers; inline requires the game owner. Render-owner callbacks
     * may also enqueue, but return WOULD_BLOCK instead of waiting for a producer's lock. Budgets
     * remain charged until completion delivery. WOULD_BLOCK requires retry after progress,
     * never a direct graphics fallback. Requests larger than the total budget are invalid.
     * @param service Service accepting work
     * @param task Execute callback and copied payload; completion may be null
     * @return Admission result; OK guarantees execution or explicit shutdown cancellation,
     *         followed by one completion delivery if provided
     */
    GraphicsServiceResult EnqueueGraphicsTask(HGraphicsService service, const GraphicsTask& task);

    /**
     * Copy a fixed request header followed by data into one allocation after admission.
     * The header author supplies any padding needed to align the trailing data. The combined
     * payload passed to execution/completion is 16-byte aligned and has the combined size.
     * @param service Service accepting work
     * @param task Fixed header and callbacks
     * @param data Trailing bytes, or null when data_size is zero
     * @param data_size Trailing byte count
     * @return Admission result, without copying or allocating payload bytes on rejection
     */
    GraphicsServiceResult EnqueueGraphicsTask(HGraphicsService service, const GraphicsTask& task, const void* data, uint32_t data_size);

    /**
     * Execute at most max_tasks requests on the render owner, outside locks. Never waits
     * for locks, work or completion delivery. Nested pumps return BUSY. A zero budget only
     * acknowledges earlier execution. Retry BUSY on a later pump; browser main must not wait.
     * This service does not order resource mutations against outstanding frame packets.
     * @param service Service whose backend context is usable by the calling owner
     * @param max_tasks Finite execution budget for this call
     * @return Progress, idle, contention, or stopped with all execution drained
     */
    GraphicsServicePumpResult PumpGraphicsService(HGraphicsService service, uint32_t max_tasks);

    /**
     * Deliver only completions available at entry, on the game owner. Work enqueued/executed
     * by callbacks waits for a later dispatch. Nested dispatch does nothing. Requests with
     * no callback still retire their payload here. Do not reenter from an execute callback.
     * @param service Service to update at a defined game-owner boundary, also during loading
     * @return Number of requests completed and released
     */
    uint32_t DispatchGraphicsCompletions(HGraphicsService service);

    /**
     * Close admission on the game owner. Accepted work is never cancelled: continue pumping
     * and dispatching completions before joining/quiescing the executor. Stop producers first.
     * @param service Service to stop; repeated calls have no effect
     */
    void StopGraphicsService(HGraphicsService service);

    /**
     * Close admission and cancel only work that has not started, without issuing graphics
     * calls. Use when shutdown cannot execute backend work, such as OS suspension. Active
     * execution is not interrupted and still needs acknowledgement/executor quiescence.
     * Cancelled requests keep their payload and receive one game-owner completion; those
     * completions may precede an earlier active request. Do not call from service callbacks.
     * @param service Service to shut down; queued requests are removed only once
     * @param result Cancellation result to deliver, normally GRAPHICS_TASK_CANCELLED
     */
    void CancelGraphicsService(HGraphicsService service, int32_t result);

    /**
     * Read progress and current budget use on the game owner.
     * @param service Service to inspect
     * @return Synchronized snapshot
     */
    GraphicsServiceStatus GetGraphicsServiceStatus(HGraphicsService service);

    /**
     * Wait for queued work on a dedicated native render owner. Pending acknowledgements
     * return immediately so the next pump can publish them. Never use on browser main/inline.
     * @param service Threaded service bound to this render owner
     */
    void WaitForGraphicsServiceWork(HGraphicsService service);

    /**
     * Wait for a changed revision on the threaded game owner. Sample before attempting work
     * to avoid lost wakes. Never call on browser main or while holding a resource/loader lock.
     * @param service Threaded service owned by this game owner
     * @param revision Previously sampled status revision
     */
    void WaitForGraphicsServiceProgress(HGraphicsService service, uint64_t revision);

    /**
     * Delete on the game owner after Stop, execution drain, completion delivery and executor
     * join/quiescence. The threaded owner must release its execution binding first; all other
     * users of that execution context must also be destroyed. Does not destroy graphics.
     * @param service Fully drained service, with no callbacks or producers still accessing it
     */
    void DeleteGraphicsService(HGraphicsService service);
}

#endif // DM_RENDER_GRAPHICS_SERVICE_H
