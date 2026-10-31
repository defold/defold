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

#ifndef DM_RENDER_FRAME_QUEUE_H
#define DM_RENDER_FRAME_QUEUE_H

#include "render_execution.h"
#include "render_frame.h"

namespace dmRender
{
    struct RenderFrameQueueContext;
    typedef RenderFrameQueueContext* HRenderFrameQueue;

    /** CPU slot ownership. Retirement permits CPU storage reuse, not GPU resource deletion. */
    enum RenderFrameState
    {
        RENDER_FRAME_FREE,     // Reusable storage, with no capture or CPU reader.
        RENDER_FRAME_BUILDING, // Game-owner capture; unpublished work can be cancelled.
        RENDER_FRAME_READY,    // Accepted immutable work waiting for the render owner.
        RENDER_FRAME_READING,  // Consumer owns inputs, or completion acknowledgement is pending.
        RENDER_FRAME_RETIRED,  // CPU readers finished; game owner must release references/reset.
    };

    /**
     * Borrowed capture access, valid until publication/cancellation. The ID detects stale
     * tickets when storage is reused. Never retain a mutable pointer after publication.
     */
    struct RenderFrameTicket
    {
        RenderFrameId m_FrameId;
        HRenderFrame  m_Frame;
    };

    /**
     * Consume on the render owner, finishing every CPU reader before returning. Inputs must
     * not contain mutable game/Lua pointers; retained resources need a read-safe contract.
     * Do not reenter queue APIs or block browser main on game work/GPU completion.
     * @param context Context supplied in RenderFrameQueueParams
     * @param frame Borrowed immutable inputs, valid only for this call
     */
    typedef void (*FRenderFrameConsume)(void* context, const RenderFrameView& frame);
    /**
     * Service persistent render-owner work, even without a frame or explicit wake request.
     * Not a draw callback or owned task queue. Do not reenter queue APIs or block browser
     * main on game work/GPU completion.
     * @param context Context supplied in RenderFrameQueueParams
     */
    typedef void (*FRenderFrameService)(void* context);

    /**
     * Quiesce the executor after draining, on the game owner and outside the queue lock.
     * Join a native thread, or detach the external/browser-main pump and wait for its
     * acknowledgement. Return only when no pump can access the queue or callback state.
     * Never wait on browser main or reenter queue shutdown from this callback.
     * @param context Context supplied when registering the executor
     */
    typedef void (*FRenderFrameExecutorShutdown)(void* context);

    /** Copied at creation; execution outlives queue and callback contexts outlive all pumps. */
    struct RenderFrameQueueParams
    {
        RenderFrameLimits   m_Limits;
        HRenderExecution    m_Execution;
        FRenderFrameConsume m_Consume;
        void*               m_ConsumeContext;
        FRenderFrameService m_Service;
        void*               m_ServiceContext;
    };

    /** Game-owner diagnostic snapshot; revision supports event-driven waits without lost wakes. */
    struct RenderFrameQueueStatus
    {
        RenderFrameId    m_FrameIds[2];
        uint64_t         m_Revision;
        uint64_t         m_Published;
        uint64_t         m_Consumed;
        uint64_t         m_Cancelled;
        uint64_t         m_Retired;
        RenderFrameState m_States[2];
        uint8_t          m_Stopping : 1;
    };

    /**
     * A bounded pump consumes at most one accepted frame. BUSY never waits for a producer
     * lock; completion acknowledgement may be deferred until the next pump.
     */
    enum RenderFramePumpResult
    {
        RENDER_FRAME_PUMP_IDLE,
        RENDER_FRAME_PUMP_CONSUMED,
        RENDER_FRAME_PUMP_BUSY,
        RENDER_FRAME_PUMP_DRAINED,
    };

    /**
     * Create two preallocated CPU slots on the game owner. Inline creates no queue mutex,
     * condition variable or thread. Platform executors own thread binding and scheduling.
     * @param params Capacities, execution and callbacks; copied at creation
     * @return Queue, or null for a missing consumer, invalid capacity or allocation failure
     */
    HRenderFrameQueue NewRenderFrameQueue(const RenderFrameQueueParams& params);

    /**
     * Delete on the game owner after stopping/draining, stopping or joining the executor,
     * and retiring every slot. A registered executor must have completed Shutdown first.
     * No callback or other owner may still access the queue.
     * @param queue Stopped queue with all slots Free
     */
    void DeleteRenderFrameQueue(HRenderFrameQueue queue);

    /**
     * Calculate queue metadata and two slots' storage without allocating. Excludes the
     * execution context, synchronization object allocations and allocator overhead.
     * @param limits Requested per-slot capacities
     * @return Fixed storage budget in bytes
     */
    uint64_t GetRenderFrameQueueStorageSize(const RenderFrameLimits& limits);

    /**
     * Acquire capture storage before simulation using the same call in either mode. Retires
     * completed slots first; threaded waits for progress without dropping frames, while
     * inline returns WOULD_BLOCK if no slot is available. No capture may already be open.
     * @param queue Game-owner queue with a running executor in threaded mode
     * @param ticket Output capture access, assigned only on success
     * @return OK, WOULD_BLOCK inline, STOPPED, or a capacity error
     */
    RenderFrameResult AcquireRenderFrame(HRenderFrameQueue queue, RenderFrameTicket* ticket);

    /**
     * Submit the captured frame using the same call in either mode. Threaded waits until
     * the earlier reader completes, then retries the same ticket; inline consumes before
     * returning. Does not cancel, replace or recapture work when the queue is full.
     * @param queue Game-owner queue with a running executor in threaded mode
     * @param ticket Current capture; all mutable access ends on success
     * @return OK, WOULD_BLOCK inline, STOPPED, or a capture/state error
     */
    RenderFrameResult SubmitRenderFrame(HRenderFrameQueue queue, const RenderFrameTicket& ticket);

    /**
     * Service persistent renderer work from the game owner. Inline pumps service without
     * consuming a frame; threaded requests a coalesced executor wake and returns immediately.
     * @param queue Queue with a service callback
     * @return OK, STOPPED, or INVALID_STATE if no service callback is installed
     */
    RenderFrameResult ServiceRenderFrameQueue(HRenderFrameQueue queue);

    /**
     * Finish game-owner shutdown with the same call in either mode: cancel only unpublished
     * capture, consume every accepted frame, retire its references, and finish requested
     * service callbacks before closing admission. Threaded waits on progress; its executor
     * must keep pumping until this returns. Stop external request producers before draining.
     * Does not join an executor: stop/join all pumps before deleting the queue or callbacks.
     * A previously stopped, fully retired and service-acknowledged queue is already drained.
     * @param queue Queue owned by this game owner, not browser main in threaded mode
     * @return OK, or INVALID_STATE if previously stopped with unfinished work
     */
    RenderFrameResult DrainRenderFrameQueue(HRenderFrameQueue queue);

    /**
     * Register the threaded executor's shutdown hook once, on the game owner during
     * setup, before starting its pump or accepting capture/service work. The context
     * must remain valid until Shutdown returns. Inline execution needs no registration.
     * @param queue Threaded queue owned by this game owner
     * @param shutdown Non-null hook that joins or quiesces the executor
     * @param context Executor-owned context passed to the hook
     * @return OK, or INVALID_STATE for inline, duplicate, missing or late registration
     */
    RenderFrameResult SetRenderFrameExecutorShutdown(HRenderFrameQueue queue,
        FRenderFrameExecutorShutdown shutdown, void* context);

    /**
     * Drain work and then quiesce its executor before callback state can be deleted.
     * Threaded requires a registered shutdown hook; missing registration fails before
     * touching queued work. Inline drains directly without synchronization or a hook.
     * Stop external request producers first. Successful repeated calls have no effect.
     * @param queue Queue owned by this game owner, not browser main in threaded mode
     * @return OK, or INVALID_STATE for missing executor registration or unfinished Stop
     */
    RenderFrameResult ShutdownRenderFrameQueue(HRenderFrameQueue queue);

    /**
     * Reserve on the game owner BEFORE simulation to bound its lead: at most one Building
     * plus one Ready/Reading frame. Retired slots require retirement. Cancellation does not
     * restore lead credit until the previous reader completes. Does not wait for a slot or
     * reader, but threaded metadata access takes a mutex.
     * @param queue Queue accepting game-owner capture
     * @param ticket Output capture access, assigned only on success
     * @return OK, WOULD_BLOCK, STOPPED, or CAPACITY_EXCEEDED if frame IDs are exhausted
     */
    RenderFrameResult BeginRenderFrameCapture(HRenderFrameQueue queue, RenderFrameTicket* ticket);

    /**
     * Publish on the game owner without waiting for an earlier reader; threaded metadata
     * access takes a mutex. Seals only on acceptance; WOULD_BLOCK keeps Building ownership.
     * Accepted frames are never dropped/replaced. Inline consumes through the same pump
     * before returning; retirement remains a separate game-owner step.
     * @param queue Queue containing the capture
     * @param ticket Current capture; all mutable access ends on success
     * @return OK, WOULD_BLOCK, STOPPED, or a capture/state error
     */
    RenderFrameResult PublishRenderFrame(HRenderFrameQueue queue, const RenderFrameTicket& ticket);

    /**
     * Cancel unpublished Building work and release partial capture on the game owner.
     * Published work cannot be cancelled, including during shutdown.
     * @param queue Queue containing the capture
     * @param ticket Current unpublished capture
     * @return OK, or INVALID_STATE for a stale/non-building ticket
     */
    RenderFrameResult CancelRenderFrameCapture(HRenderFrameQueue queue, const RenderFrameTicket& ticket);

    /**
     * Release retained references on the game owner, outside the handoff lock, then reuse
     * storage. Release callbacks must not reenter capture, publication or retirement.
     * CPU retirement does not establish GPU completion.
     * @param queue Queue with completed or cancelled slots
     * @return Number of slots reset to Free
     */
    uint32_t RetireRenderFrames(HRenderFrameQueue queue);

    /**
     * Pump on the render owner using TryLock only: never wait for readiness, game work or
     * GPU completion. Consume at most one frame; acknowledgement may defer to a later pump.
     * Browser frame work runs on rAF; service-only calls may run outside rAF.
     * @param queue Queue bound to this render owner
     * @param consume_frame Whether to consume a Ready frame; false runs service only
     * @return IDLE, CONSUMED, BUSY or DRAINED; DRAINED is not reference/GPU retirement
     */
    RenderFramePumpResult PumpRenderFrameQueue(HRenderFrameQueue queue, bool consume_frame);

    /**
     * Request a coalesced service wake, not task submission. Threaded non-render producers
     * may call; inline requires the game owner. Metadata access may take a mutex. Pre-stop
     * requests are serviced during drain; requests after Stop are rejected.
     * @param queue Queue with a service callback
     * @return OK, STOPPED, or INVALID_STATE for the wrong owner/missing service callback
     */
    RenderFrameResult RequestRenderFrameService(HRenderFrameQueue queue);

    /**
     * Close admission on the game owner, cancel Building work and retire available slots.
     * Idempotent; never wait for or interrupt accepted work. Continue pumping to DRAINED,
     * stop/join the executor, retire remaining slots, then delete. Metadata access takes a
     * mutex in threaded mode. Drain lifetimes requiring new service requests BEFORE Stop,
     * since those requests are rejected after Stop.
     * @param queue Queue to stop
     */
    void StopRenderFrameQueue(HRenderFrameQueue queue);

    /**
     * Read a synchronized game-owner snapshot.
     * @param queue Queue to inspect
     * @return Slot states/IDs, counters, stopping state and progress revision
     */
    RenderFrameQueueStatus GetRenderFrameQueueStatus(HRenderFrameQueue queue);

    /**
     * Block the threaded game owner/game pthread until progress or stopping. Never call
     * inline or on browser main. Sample revision BEFORE an operation that may WOULD_BLOCK,
     * then wait and retry, to avoid lost wakes.
     * @param queue Threaded queue owned by this game owner
     * @param revision Previously sampled progress revision
     */
    void WaitForRenderFrameProgress(HRenderFrameQueue queue, uint64_t revision);

    /**
     * Block a dedicated native render owner after an IDLE/BUSY pump. Never call inline or
     * on browser main. Pending acknowledgements do not sleep on the condition; mutex
     * acquisition can wait for a contended producer to finish.
     * @param queue Threaded queue bound to this native render owner
     */
    void WaitForRenderFrameWork(HRenderFrameQueue queue);
}

#endif // DM_RENDER_FRAME_QUEUE_H
