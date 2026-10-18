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

    // CPU ownership only; Retired is NOT GPU completion.
    enum RenderFrameState
    {
        RENDER_FRAME_FREE,
        RENDER_FRAME_BUILDING,
        RENDER_FRAME_READY,
        RENDER_FRAME_READING,
        RENDER_FRAME_RETIRED,
    };

    // Borrowed capture access, valid only until publication/cancellation. The ID detects
    // stale tickets when the same storage is reused. Never retain a mutable data pointer.
    struct RenderFrameTicket
    {
        RenderFrameId m_FrameId;
        HRenderFrame  m_Frame;
    };

    // Consumer must finish every CPU reader before returning. No mutable game/Lua pointers
    // in inputs; retained resources need their own immutable/read-safe access contract.
    typedef void (*RenderFrameConsume)(void* context, const RenderFrameView& frame);
    // Optional persistent owner service (e.g. future resource completions), even without
    // a frame. Not a draw callback or an owned task queue. Callbacks must not reenter queue APIs.
    // On browser main, callbacks must also avoid blocking on game work or GPU completion.
    typedef void (*RenderFrameService)(void* context);

    // Stable until deletion; execution outlives queue, callback contexts outlive all pumps.
    struct RenderFrameQueueParams
    {
        RenderFrameLimits  m_Limits;
        HRenderExecution   m_Execution;
        RenderFrameConsume m_Consume;
        void*              m_ConsumeContext;
        RenderFrameService m_Service;
        void*              m_ServiceContext;
    };

    // Game-owner diagnostic snapshot; revision supports event-driven waits without lost wakes.
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

    // A bounded pump consumes at most one accepted frame; BUSY never waits for a producer
    // lock. Completion acknowledgement may be deferred until the next pump.
    enum RenderFramePumpResult
    {
        RENDER_FRAME_PUMP_IDLE,
        RENDER_FRAME_PUMP_CONSUMED,
        RENDER_FRAME_PUMP_BUSY,
        RENDER_FRAME_PUMP_DRAINED,
    };

    // Creates exactly two preallocated CPU slots. Inline creates no queue mutex/CV/thread.
    // No production thread is started here; platform executors own binding and scheduling.
    HRenderFrameQueue NewRenderFrameQueue(const RenderFrameQueueParams& params);
    // Game owner: stop, finish pumping, stop/join the executor, retire, then delete.
    void DeleteRenderFrameQueue(HRenderFrameQueue queue);
    uint64_t GetRenderFrameQueueStorageSize(const RenderFrameLimits& limits);

    // Game owner: reserve BEFORE the simulation update to bound its lead, not just at capture.
    // At most one Building plus one Ready/Reading frame. Retired slots need explicit retirement.
    // Cancelling an update built ahead does not restore its credit until the previous reader
    // completes; otherwise repeated cancellation could allow unbounded simulation lead.
    RenderFrameResult BeginRenderFrameCapture(HRenderFrameQueue queue, RenderFrameTicket* ticket);
    // Nonblocking; WOULD_BLOCK keeps Building ownership intact. Seals only on acceptance.
    // Inline immediately uses the same pump, but retirement remains a separate game-owner step.
    RenderFrameResult PublishRenderFrame(HRenderFrameQueue queue, const RenderFrameTicket& ticket);
    // Only unpublished capture can be cancelled. Releases partial capture on the game owner.
    RenderFrameResult CancelRenderFrameCapture(HRenderFrameQueue queue, const RenderFrameTicket& ticket);
    // Game owner: release retained CPU references outside the handoff lock, then reuse storage.
    uint32_t RetireRenderFrames(HRenderFrameQueue queue);

    // Render owner: TryLock only, never waits for readiness, the game owner or GPU completion.
    // Invoke on browser rAF for frame work; service-only calls may be made outside rAF by
    // passing consume_frame=false. Those calls never issue a frame consume callback.
    RenderFramePumpResult PumpRenderFrameQueue(HRenderFrameQueue queue, bool consume_frame);
    // Threaded: non-render producers; inline: game owner only (workers relay via that owner).
    // Other callers get INVALID_STATE. Coalesced wake for the service hook, not task submission.
    // Requests accepted before Stop are serviced during drain; requests after Stop are rejected.
    RenderFrameResult RequestRenderFrameService(HRenderFrameQueue queue);

    // Game owner: close admission, cancel partial capture, preserve/drain all accepted frames.
    // Nonblocking; continue pumping until DRAINED, join/release owner, retire and delete.
    void StopRenderFrameQueue(HRenderFrameQueue queue);
    RenderFrameQueueStatus GetRenderFrameQueueStatus(HRenderFrameQueue queue);

    // Blocking helpers for dedicated native executor/game pthread ONLY, never browser main.
    // Sample revision BEFORE a failed operation, then wait for a change and retry.
    void WaitForRenderFrameProgress(HRenderFrameQueue queue, uint64_t revision);
    // Native render loop: pump, then wait if IDLE/BUSY. Pending acknowledgements never
    // sleep on the condition; native mutex acquisition lets a contended producer finish.
    void WaitForRenderFrameWork(HRenderFrameQueue queue);
}

#endif // DM_RENDER_FRAME_QUEUE_H
