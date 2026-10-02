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

#include "render_thread.h"
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>
#include <dlib/time.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#if defined(__APPLE__)
#include <pthread.h>
#include <pthread/qos.h>
#endif

namespace dmRender
{
    FrameTrace* NewFrameTrace(uint32_t capacity)
    {
        FrameTrace* trace = new FrameTrace;
        trace->m_Records = new FrameTraceRecord[capacity];
        memset(trace->m_Records, 0, sizeof(FrameTraceRecord) * capacity);
        trace->m_Count = trace->m_Dropped = 0;
        trace->m_Capacity = capacity;
        return trace;
    }

    FrameTraceRecord* BeginFrameTrace(FrameTrace* trace)
    {
        if (trace->m_Count == trace->m_Capacity)
        {
            ++trace->m_Dropped;
            return 0;
        }
        FrameTraceRecord* record = &trace->m_Records[trace->m_Count++];
        record->m_InputBegin = dmTime::GetMonotonicTime();
        return record;
    }

    bool DeleteFrameTrace(FrameTrace* trace, const char* path)
    {
        bool ok = true;
        if (path)
        {
            FILE* file = fopen(path, "w");
            ok = file != 0;
            if (file)
            {
                ok = fprintf(file, "# dropped=%u; CPU timestamps in monotonic microseconds; submit is NOT GPU completion or display\n", trace->m_Dropped) >= 0;
                ok = fprintf(file, "frame,input_begin_us,update_end_us,prepare_end_us,publish_us,render_begin_us,submit_end_us,pace_begin_us,pace_end_us,pace_deadline_us,queue_drain_end_us,surface_begin_us,surface_end_us,slot_wait_begin_us,slot_wait_end_us,drawable_begin_us,drawable_end_us,encode_begin_us,commit_begin_us,commit_end_us,gpu_duration_us,gpu_completed\n") >= 0 && ok;
                for (uint32_t i = 0; i < trace->m_Count; ++i)
                {
                    const FrameTraceRecord& r = trace->m_Records[i];
                    ok = fprintf(file, "%u,%llu,%llu,%llu,%llu,%llu,%llu", i + 1,
                        (unsigned long long)r.m_InputBegin, (unsigned long long)r.m_UpdateEnd,
                        (unsigned long long)r.m_PrepareEnd, (unsigned long long)r.m_Publish,
                        (unsigned long long)r.m_RenderBegin, (unsigned long long)r.m_SubmitEnd) >= 0 && ok;
                    ok = fprintf(file, ",%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
                        (unsigned long long)r.m_PaceBegin,
                        (unsigned long long)r.m_PaceEnd,
                        (unsigned long long)r.m_PaceDeadline,
                        (unsigned long long)r.m_QueueDrainEnd,
                        (unsigned long long)r.m_SurfaceBegin,
                        (unsigned long long)r.m_SurfaceEnd,
                        (unsigned long long)r.m_Graphics.m_SlotWaitBegin,
                        (unsigned long long)r.m_Graphics.m_SlotWaitEnd,
                        (unsigned long long)r.m_Graphics.m_DrawableBegin,
                        (unsigned long long)r.m_Graphics.m_DrawableEnd,
                        (unsigned long long)r.m_Graphics.m_EncodeBegin,
                        (unsigned long long)r.m_Graphics.m_CommitBegin,
                        (unsigned long long)r.m_Graphics.m_CommitEnd,
                        (unsigned long long)r.m_Graphics.m_GpuDurationUs,
                        (unsigned long long)r.m_Graphics.m_GpuCompleted) >= 0 && ok;
                }
                ok = fclose(file) == 0 && ok;
            }
        }
        delete[] trace->m_Records;
        delete trace;
        return ok;
    }

    enum SlotState { SLOT_FREE, SLOT_BUILDING, SLOT_READY, SLOT_READING };
    struct RenderThread
    {
        dmMutex::HMutex m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        dmThread::Thread m_Thread;
        RenderThreadFunction m_Render;
        void* m_Context;
        RenderControlFunction m_Control;
        void* m_ControlContext;
        SlotState m_Slots[2];
        uint64_t m_FrameId[2];
        uint64_t m_BeginTime[2];
        RenderThreadStats m_Stats;
        bool m_External;
        bool m_ExternalRetiring;
        uint64_t m_ExternalBegin, m_ExternalEnd;
        bool m_Stop;
        bool m_Building;
        bool m_ControlPending;
        bool m_InteractiveQos;
    };

    static void RenderWorker(void* data)
    {
        RenderThread* t = (RenderThread*)data;
#if defined(__APPLE__)
        pthread_override_t qos_override = 0;
        if (t->m_InteractiveQos)
        {
            qos_override = pthread_override_qos_class_start_np(pthread_self(), QOS_CLASS_USER_INTERACTIVE, 0);
            fprintf(stderr, "%s\n", qos_override ? "PoC worker QoS: enabled" : "ERROR: PoC worker QoS failed");
        }
#endif
        dmMutex::Lock(t->m_Mutex);
#if defined(__APPLE__)
        t->m_Stats.m_InteractiveQosApplied = qos_override != 0;
#endif
        for (;;)
        {
            if (t->m_ControlPending)
            {
                RenderControlFunction fn = t->m_Control;
                void* context = t->m_ControlContext;
                dmMutex::Unlock(t->m_Mutex);
                uint64_t begin = dmTime::GetMonotonicTime();
                fn(context);
                dmMutex::Lock(t->m_Mutex);
                t->m_Stats.m_ControlUs += dmTime::GetMonotonicTime() - begin;
                ++t->m_Stats.m_Controls;
                t->m_ControlPending = false;
                dmConditionVariable::Broadcast(t->m_Changed);
                continue;
            }
            uint32_t slot = (uint32_t)(t->m_Stats.m_Completed % 2);
            if (t->m_Slots[slot] == SLOT_READY)
            {
                assert(t->m_FrameId[slot] == t->m_Stats.m_Completed + 1);
                t->m_Slots[slot] = SLOT_READING;
                uint64_t id = t->m_FrameId[slot];
                dmMutex::Unlock(t->m_Mutex);
                uint64_t begin = dmTime::GetMonotonicTime();
                t->m_Render(t->m_Context, slot, id);
                uint64_t end = dmTime::GetMonotonicTime();
                dmMutex::Lock(t->m_Mutex);
                t->m_Stats.m_RenderUs += end - begin;
                t->m_Stats.m_FrameAgeUs += end - t->m_BeginTime[slot];
                t->m_Stats.m_Completed = id;
                t->m_Slots[slot] = SLOT_FREE;
                dmConditionVariable::Broadcast(t->m_Changed);
                continue;
            }
            if (t->m_Stop)
                break;
            dmConditionVariable::Wait(t->m_Changed, t->m_Mutex);
        }
        dmMutex::Unlock(t->m_Mutex);
#if defined(__APPLE__)
        if (qos_override && pthread_override_qos_class_end_np(qos_override) != 0)
            fprintf(stderr, "ERROR: PoC worker QoS cleanup failed\n");
#endif
    }

    static HRenderThread CreateRenderThread(RenderThreadFunction render, void* context, bool interactive_qos, bool external)
    {
        RenderThread* t = new RenderThread;
        memset(t, 0, sizeof(*t));
        t->m_Mutex = dmMutex::New();
        t->m_Changed = dmConditionVariable::New();
        t->m_Render = render;
        t->m_Context = context;
        t->m_InteractiveQos = interactive_qos;
        t->m_Stats.m_SlotCount = 2;
        t->m_Stats.m_ControlCapacity = 1;
        t->m_Stats.m_QueueBytes = sizeof(*t);
        t->m_External = external;
        if (!external) t->m_Thread = dmThread::New(RenderWorker, 0x80000, t, "sprite-render");
        return t;
    }

    HRenderThread NewRenderThread(RenderThreadFunction render, void* context, bool interactive_qos)
    {
        return CreateRenderThread(render, context, interactive_qos, false);
    }

    HRenderThread NewExternalRenderThread(RenderThreadFunction render, void* context)
    {
        return CreateRenderThread(render, context, false, true);
    }

    static void RetireExternalFrame(RenderThread* t)
    {
        uint32_t slot = (uint32_t)(t->m_Stats.m_Completed % 2);
        t->m_Stats.m_RenderUs += t->m_ExternalEnd - t->m_ExternalBegin;
        t->m_Stats.m_FrameAgeUs += t->m_ExternalEnd - t->m_BeginTime[slot];
        t->m_Stats.m_Completed = t->m_FrameId[slot];
        t->m_Slots[slot] = SLOT_FREE;
        t->m_ExternalRetiring = false;
        dmConditionVariable::Broadcast(t->m_Changed);
    }

    bool PumpExternalRenderThread(HRenderThread t)
    {
        assert(t->m_External);
        if (!dmMutex::TryLock(t->m_Mutex)) return false;
        if (t->m_ExternalRetiring) RetireExternalFrame(t);
        uint32_t slot = (uint32_t)(t->m_Stats.m_Completed % 2);
        if (t->m_Slots[slot] != SLOT_READY)
        {
            dmMutex::Unlock(t->m_Mutex);
            return false;
        }
        t->m_Slots[slot] = SLOT_READING;
        uint64_t id = t->m_FrameId[slot];
        dmMutex::Unlock(t->m_Mutex);
        t->m_ExternalBegin = dmTime::GetMonotonicTime();
        t->m_Render(t->m_Context, slot, id);
        t->m_ExternalEnd = dmTime::GetMonotonicTime();
        t->m_ExternalRetiring = true;
        // A contended acknowledgment is deferred to the next pump; browser main
        // must never block, even briefly, behind the simulation producer.
        if (dmMutex::TryLock(t->m_Mutex))
        {
            RetireExternalFrame(t);
            dmMutex::Unlock(t->m_Mutex);
        }
        return true;
    }

    uint32_t BeginRenderThreadFrame(HRenderThread t)
    {
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        assert(!t->m_Building && !t->m_Stop);
        uint32_t slot = (uint32_t)(t->m_Stats.m_Submitted % 2);
        assert(t->m_Slots[slot] == SLOT_FREE);
        t->m_Slots[slot] = SLOT_BUILDING;
        t->m_BeginTime[slot] = dmTime::GetMonotonicTime();
        t->m_Building = true;
        return slot;
    }

    void DrainRenderThread(HRenderThread t)
    {
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        uint64_t begin = dmTime::GetMonotonicTime();
        while (t->m_Stats.m_Completed != t->m_Stats.m_Submitted || t->m_ControlPending)
            dmConditionVariable::Wait(t->m_Changed, t->m_Mutex);
        t->m_Stats.m_ProducerWaitUs += dmTime::GetMonotonicTime() - begin;
    }

    void MarkRenderThreadFrameCaptured(HRenderThread t)
    {
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        assert(t->m_Building);
        if (t->m_Stats.m_Submitted != t->m_Stats.m_Completed)
            ++t->m_Stats.m_CapturesWithConsumerOutstanding;
    }

    void PublishRenderThreadFrame(HRenderThread t, uint32_t slot, FrameTraceRecord* trace)
    {
        DrainRenderThread(t);
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        assert(t->m_Building && t->m_Slots[slot] == SLOT_BUILDING);
        t->m_FrameId[slot] = ++t->m_Stats.m_Submitted;
        if (trace) trace->m_Publish = dmTime::GetMonotonicTime();
        t->m_Slots[slot] = SLOT_READY;
        t->m_Building = false;
        uint32_t outstanding = (uint32_t)(t->m_Stats.m_Submitted - t->m_Stats.m_Completed);
        assert(outstanding == 1);
        if (outstanding > t->m_Stats.m_MaxOutstanding)
            t->m_Stats.m_MaxOutstanding = outstanding;
        dmConditionVariable::Broadcast(t->m_Changed);
    }

    void CancelRenderThreadFrame(HRenderThread t, uint32_t slot)
    {
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        assert(t->m_Building && t->m_Slots[slot] == SLOT_BUILDING);
        t->m_Slots[slot] = SLOT_FREE;
        t->m_Building = false;
    }

    void RunRenderThreadControl(HRenderThread t, RenderControlFunction fn, void* context)
    {
        assert(!t->m_External); // External owners use their browser request lane.
        DrainRenderThread(t);
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        t->m_Control = fn;
        t->m_ControlContext = context;
        t->m_ControlPending = true;
        dmConditionVariable::Broadcast(t->m_Changed);
        while (t->m_ControlPending)
            dmConditionVariable::Wait(t->m_Changed, t->m_Mutex);
    }

    void GetRenderThreadStats(HRenderThread t, RenderThreadStats* stats)
    {
        DM_MUTEX_SCOPED_LOCK(t->m_Mutex);
        *stats = t->m_Stats;
    }

    void DeleteRenderThread(HRenderThread t)
    {
        DrainRenderThread(t);
        dmMutex::Lock(t->m_Mutex);
        assert(!t->m_Building);
        t->m_Stop = true;
        dmConditionVariable::Broadcast(t->m_Changed);
        dmMutex::Unlock(t->m_Mutex);
        if (!t->m_External) dmThread::Join(t->m_Thread);
        dmConditionVariable::Delete(t->m_Changed);
        dmMutex::Delete(t->m_Mutex);
        delete t;
    }
}
