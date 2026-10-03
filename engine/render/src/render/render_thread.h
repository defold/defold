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

#ifndef DM_RENDER_THREAD_H
#define DM_RENDER_THREAD_H

#include <stdint.h>
#include <graphics/graphics.h>

namespace dmRender
{
    // Experimental single-producer queue. Exactly two externally owned CPU slots;
    // admission permits building N+1 while N is reading, never N+2.
    typedef struct RenderThread* HRenderThread;
    typedef void (*RenderThreadFunction)(void* context, uint32_t slot, uint64_t id);
    typedef void (*RenderControlFunction)(void* context);
    struct RenderThreadStats
    {
        uint64_t m_Submitted;
        uint64_t m_Completed;
        uint64_t m_ProducerWaitUs;
        uint64_t m_RenderUs;
        uint64_t m_FrameAgeUs;
        uint64_t m_Controls;
        uint64_t m_ControlUs;
        uint64_t m_CapturesWithConsumerOutstanding;
        uint64_t m_SimulationsDuringRender;
        uint64_t m_SimulationOverlapUs;
        uint32_t m_MaxOutstanding;
        uint32_t m_SlotCount;
        uint32_t m_ControlCapacity;
        uint32_t m_QueueBytes;
        uint32_t m_InteractiveQosApplied;
    };
    // Opt-in bounded CPU trace. Producer allocates records; publication transfers
    // each record to the consumer; Metal completion writes only GPU fields.
    // Read/save only after joining the consumer AND draining GPU callbacks.
    struct FrameTraceRecord
    {
        uint64_t m_PaceBegin;
        uint64_t m_PaceEnd;
        uint64_t m_PaceDeadline;
        uint64_t m_QueueDrainEnd;
        uint64_t m_SurfaceBegin;
        uint64_t m_SurfaceEnd;
        dmGraphics::RenderFrameTimings m_Graphics;
        uint64_t m_InputBegin;
        uint64_t m_UpdateEnd;
        uint64_t m_PrepareEnd;
        uint64_t m_Publish;
        uint64_t m_RenderBegin;
        uint64_t m_SubmitEnd;
    };
    struct FrameTrace
    {
        FrameTraceRecord* m_Records;
        uint32_t m_Count;
        uint32_t m_Capacity;
        uint32_t m_Dropped;
    };
    FrameTrace* NewFrameTrace(uint32_t capacity);
    FrameTraceRecord* BeginFrameTrace(FrameTrace* trace);
    bool DeleteFrameTrace(FrameTrace* trace, const char* path);

    HRenderThread NewRenderThread(RenderThreadFunction render, void* context, bool interactive_qos = false);
    // Begin reserves the only building slot. Publish waits for the previous reader.
    // Browser main consumes only already-published work; the producer may wait.
    HRenderThread NewExternalRenderThread(RenderThreadFunction render, void* context);
    bool PumpExternalRenderThread(HRenderThread thread);
    uint32_t BeginRenderThreadFrame(HRenderThread thread);
    void MarkRenderThreadFrameCaptured(HRenderThread thread);
    // Counts complete simulation intervals contained in an active consumption.
    // This is stronger than counting a merely queued/outstanding frame.
    void MarkRenderThreadSimulationComplete(HRenderThread thread, uint64_t begin);
    void PublishRenderThreadFrame(HRenderThread thread, uint32_t slot, FrameTraceRecord* trace = 0);
    void CancelRenderThreadFrame(HRenderThread thread, uint32_t slot);
    void DrainRenderThread(HRenderThread thread);
    // Synchronous, one-entry control lane. Serviced even when no frames are submitted.
    void RunRenderThreadControl(HRenderThread thread, RenderControlFunction fn, void* context);
    void GetRenderThreadStats(HRenderThread thread, RenderThreadStats* stats);
    void DeleteRenderThread(HRenderThread thread);
}
#endif
