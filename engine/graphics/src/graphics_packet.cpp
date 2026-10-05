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

#include "graphics_packet.h"
#include "graphics_adapter.h"
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/log.h>
#include <dlib/hashtable.h>
#include <dlib/math.h>
#include <stdlib.h>
#include <string.h>

namespace dmGraphics
{
    typedef void (*PacketExecute)(void*);
    struct PacketCommand { PacketExecute m_Execute; uint32_t m_Size; uint32_t m_Padding; };
    struct GraphicsFramePacket
    {
        dmArray<uint8_t> m_Bytes;
        uint64_t m_Commands;
        bool m_EndFrame;
        GraphicsFramePacket() : m_Commands(0), m_EndFrame(false) {}
    };
    struct OwnerJob { GraphicsOwnerTask m_Execute; void* m_Data; uint32_t m_Size; GraphicsOwnerCompletion m_Complete; void* m_Context; };
    struct OwnerCompletion { GraphicsOwnerCompletion m_Callback; void* m_Context; };
    struct PacketService
    {
        GraphicsAdapterFunctionTable m_Backend;
        GraphicsAdapterFunctionTable* m_Frontend;
        HContext m_Context;
        dmThread::ThreadId m_Producer, m_Owner;
        dmThread::Thread m_Thread;
        dmMutex::HMutex m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        GraphicsFramePacket m_Packets[2];
        GraphicsPacketStats m_Stats;
        dmArray<OwnerCompletion> m_Completions;
        OwnerJob m_Jobs[256];
        uint32_t m_JobRead, m_JobCount, m_JobBytes;
        bool m_JobRunning, m_Closing;
        bool m_RenderLayer, m_OwnerFrame;
        ExternalGraphicsDispatch m_External;
        uint32_t m_WindowWidth, m_WindowHeight, m_Iconified, m_WindowOpened;
        bool m_CacheWindowOpened;
        float m_DisplayScale;
        dmHashTable64<uint32_t> m_BufferSizes;
        PipelineState m_Pipeline;
        int32_t m_ViewportX, m_ViewportY;
        uint32_t m_ViewportWidth, m_ViewportHeight;
        uint32_t m_BuildSlot, m_ReadSlot, m_DelayUs;
        uint64_t m_BuildFrameBytes;
        PacketExecute m_Control;
        void* m_ControlData;
        bool m_Busy, m_Stop, m_Ready, m_FrameOpen, m_ExecutingInline, m_Paused;
        PacketService() : m_BuildSlot(0), m_ReadSlot(0), m_DelayUs(0), m_BuildFrameBytes(0),
            m_Control(0), m_ControlData(0), m_Busy(false), m_Stop(false), m_Ready(false), m_FrameOpen(false), m_ExecutingInline(false), m_Paused(false)
        { m_External = 0; m_CacheWindowOpened = false; memset(&m_Stats, 0, sizeof(m_Stats)); m_RenderLayer = m_OwnerFrame = m_JobRunning = m_Closing = false; m_JobRead = m_JobCount = m_JobBytes = 0; }
    };
    static PacketService* g_Packets = 0;
    static const uint32_t MAX_PACKET_BYTES = 32 * 1024 * 1024;
    static const uint32_t MAX_BUFFER_RECORDS = 65536;

    static void PacketError(const char* reason)
    {
        dmLogFatal("Graphics packet PoC: %s", reason);
        abort(); // Never silently drop commands or execute an unsafe direct fallback.
    }

    bool IsGraphicsPacketOwner()
    {
        return g_Packets && dmThread::GetCurrentThreadId() == g_Packets->m_Owner &&
            (g_Packets->m_Stats.m_Mode == 2 || g_Packets->m_ExecutingInline);
    }

    bool IsExternalGraphicsProducer()
    {
        return g_Packets && g_Packets->m_External && dmThread::GetCurrentThreadId() == g_Packets->m_Producer && g_Packets->m_Producer != g_Packets->m_Owner;
    }

    bool DispatchExternalGraphics(GraphicsOwnerTask execute, void* data)
    {
        if (!IsExternalGraphicsProducer()) return false;
        // Mesh updates can queue vertex uploads before component rendering or
        // render-script consumption. Complete those uploads before handing the
        // live worlds to the owner; do not rely on an incidental window query.
        FlushGraphicsPackets();
        g_Packets->m_External(execute, data);
        return true;
    }

    void AttachExternalGraphicsProducer()
    {
        assert(g_Packets && g_Packets->m_External);
        g_Packets->m_Producer = dmThread::GetCurrentThreadId();
    }

    void DetachExternalGraphicsProducer()
    {
        assert(g_Packets && g_Packets->m_External && IsGraphicsPacketOwner());
        g_Packets->m_Producer = g_Packets->m_Owner;
        g_Packets->m_External = 0;
        g_Packets->m_Stats.m_Mode = 1;
    }

    void UpdateExternalGraphicsWindow(HContext context, bool cache_opened)
    {
        assert(g_Packets && g_Packets->m_External && IsGraphicsPacketOwner());
        g_Packets->m_WindowWidth = GetWindowWidth(context);
        g_Packets->m_WindowHeight = GetWindowHeight(context);
        g_Packets->m_Iconified = GetWindowStateParam(context, WINDOW_STATE_ICONIFIED);
        g_Packets->m_DisplayScale = GetDisplayScaleFactor(context);
        g_Packets->m_CacheWindowOpened = cache_opened;
        if (cache_opened)
            g_Packets->m_WindowOpened = GetWindowStateParam(context, WINDOW_STATE_OPENED);
    }

    bool GetExternalGraphicsWindowOpened(uint32_t* opened)
    {
        if (!IsExternalGraphicsProducer() || !g_Packets->m_CacheWindowOpened) return false;
        *opened = g_Packets->m_WindowOpened;
        return true;
    }

    bool GetExternalGraphicsWindow(uint32_t* width, uint32_t* height, uint32_t* iconified, float* scale)
    {
        if (!IsExternalGraphicsProducer()) return false;
        *width = g_Packets->m_WindowWidth;
        *height = g_Packets->m_WindowHeight;
        *iconified = g_Packets->m_Iconified;
        *scale = g_Packets->m_DisplayScale;
        return true;
    }

    static bool ProducerCall()
    {
        if (g_Packets->m_Stats.m_Mode == 1 && g_Packets->m_ExecutingInline) return false;
        if (dmThread::GetCurrentThreadId() == g_Packets->m_Producer) return true;
        if (IsGraphicsPacketOwner()) return false; // Backend helper re-entry.
        PacketError("only the game producer and graphics owner are admitted");
        return false;
    }

    static uint32_t Align16(uint32_t n) { return (n + 15) & ~15u; }

    static void* Record(PacketExecute execute, uint32_t object_size, const void* data = 0, uint64_t data_size = 0)
    {
        PacketService* s = g_Packets;
        GraphicsFramePacket& p = s->m_Packets[s->m_BuildSlot];
        if (s->m_RenderLayer) dmMutex::Lock(s->m_Mutex);
        uint32_t limit = s->m_RenderLayer ? 4 * 1024 * 1024 : MAX_PACKET_BYTES;
        if (s->m_RenderLayer && p.m_Commands + s->m_JobCount + s->m_Completions.Size() + (uint32_t)s->m_JobRunning >= 256) PacketError("owner request count exceeded");
        uint64_t size64 = Align16(sizeof(PacketCommand)) + Align16(object_size) + data_size;
        if (size64 > limit || p.m_Bytes.Size() + size64 + 15 > limit)
            PacketError("32 MiB packet capacity exceeded");
        uint32_t size = Align16((uint32_t)size64);
        uint32_t begin = p.m_Bytes.Size(), needed = begin + size;
        if (needed > p.m_Bytes.Capacity())
        {
            uint32_t capacity = dmMath::Max(needed, dmMath::Max(4096u, p.m_Bytes.Capacity() * 2));
            capacity = dmMath::Min(capacity, limit);
            uint64_t growth = s->m_Packets[0].m_Bytes.Capacity() + s->m_Packets[1].m_Bytes.Capacity() + (uint64_t)capacity;
            if (s->m_RenderLayer && s->m_Packets[1-s->m_BuildSlot].m_Bytes.Capacity() + (uint64_t)capacity + s->m_JobBytes > 8 * 1024 * 1024)
                PacketError("8 MiB owner request capacity exceeded");
            growth += s->m_JobBytes;
            s->m_Stats.m_GrowthPeakBytes = dmMath::Max(s->m_Stats.m_GrowthPeakBytes, growth);
            p.m_Bytes.SetCapacity(capacity);
        }
        p.m_Bytes.SetSize(needed);
        PacketCommand* command = (PacketCommand*)(p.m_Bytes.Begin() + begin);
        command->m_Execute = execute;
        command->m_Size = size;
        void* object = (uint8_t*)command + Align16(sizeof(PacketCommand));
        if (data_size) memcpy((uint8_t*)object + Align16(object_size), data, (size_t)data_size);
        ++p.m_Commands;
        ++s->m_Stats.m_Commands;
        s->m_Stats.m_CopiedBytes += data_size;
        s->m_BuildFrameBytes += size;
        if (s->m_RenderLayer) dmMutex::Unlock(s->m_Mutex);
        return object;
    }

    static void ExecutePacket(GraphicsFramePacket& p)
    {
        for (uint32_t offset = 0; offset < p.m_Bytes.Size();)
        {
            PacketCommand* command = (PacketCommand*)(p.m_Bytes.Begin() + offset);
            command->m_Execute((uint8_t*)command + Align16(sizeof(PacketCommand)));
            offset += command->m_Size;
        }
    }

    static void ExecuteOwnerJob(PacketService* s)
    {
        OwnerJob job = s->m_Jobs[s->m_JobRead];
        s->m_JobRead = (s->m_JobRead + 1) % 256;
        --s->m_JobCount;
        s->m_JobRunning = true;
        dmMutex::Unlock(s->m_Mutex);
        if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_METAL) DrainRenderThreadGpu(s->m_Context);
        job.m_Execute(job.m_Data);
        free(job.m_Data);
        dmMutex::Lock(s->m_Mutex);
        s->m_JobBytes -= job.m_Size;
        OwnerCompletion completion = {job.m_Complete, job.m_Context};
        s->m_Completions.Push(completion);
        s->m_JobRunning = false;
        dmConditionVariable::Broadcast(s->m_Changed);
    }

    static void PacketWorker(void* data)
    {
        PacketService* s = (PacketService*)data;
        dmMutex::Lock(s->m_Mutex);
        s->m_Owner = dmThread::GetCurrentThreadId();
        s->m_Ready = true;
        dmConditionVariable::Broadcast(s->m_Changed);
        for (;;)
        {
            if (s->m_Busy)
            {
                uint32_t slot = s->m_ReadSlot;
                PacketExecute control = s->m_Control;
                void* control_data = s->m_ControlData;
                dmMutex::Unlock(s->m_Mutex);
                uint64_t begin = dmTime::GetMonotonicTime();
                if (control) control(control_data);
                else
                {
                    if (s->m_DelayUs) dmTime::Sleep(s->m_DelayUs);
                    ExecutePacket(s->m_Packets[slot]);
                }
                dmMutex::Lock(s->m_Mutex);
                s->m_Stats.m_ExecuteUs += dmTime::GetMonotonicTime() - begin;
                if (s->m_OwnerFrame || (!control && s->m_Packets[slot].m_EndFrame)) ++s->m_Stats.m_Completed;
                s->m_OwnerFrame = false;
                s->m_Control = 0;
                s->m_Busy = false;
                dmConditionVariable::Broadcast(s->m_Changed);
            }
            else if (s->m_JobCount) ExecuteOwnerJob(s);
            else if (s->m_Stop) break;
            else dmConditionVariable::Wait(s->m_Changed, s->m_Mutex);
        }
        dmMutex::Unlock(s->m_Mutex);
    }

    static void WaitIdle()
    {
        PacketService* s = g_Packets;
        if (s->m_External) return; // External resource calls complete before returning.
        uint64_t begin = dmTime::GetMonotonicTime();
        DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
        if (s->m_Stats.m_Mode == 1)
        {
            s->m_ExecutingInline = true;
            while (s->m_JobCount) ExecuteOwnerJob(s);
            s->m_ExecutingInline = false;
        }
        while (s->m_Busy || s->m_JobCount || s->m_JobRunning) dmConditionVariable::Wait(s->m_Changed, s->m_Mutex);
        s->m_Stats.m_WaitUs += dmTime::GetMonotonicTime() - begin;
    }

    static void ExecuteExternalPacket(void* data) { ExecutePacket(*(GraphicsFramePacket*)data); }

    static void Submit(bool end_frame)
    {
        PacketService* s = g_Packets;
        GraphicsFramePacket& p = s->m_Packets[s->m_BuildSlot];
        p.m_EndFrame = end_frame;
        if (end_frame)
        {
            DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
            s->m_Stats.m_LastFrameBytes = s->m_BuildFrameBytes;
            s->m_Stats.m_MaxFrameBytes = dmMath::Max(s->m_Stats.m_MaxFrameBytes, s->m_BuildFrameBytes);
            s->m_BuildFrameBytes = 0;
        }
        if (!p.m_Bytes.Size()) return;
        WaitIdle();
        if (s->m_External)
        {
            s->m_External(ExecuteExternalPacket, &p);
        }
        else if (s->m_Stats.m_Mode == 1)
        {
            uint64_t begin = dmTime::GetMonotonicTime();
            s->m_ExecutingInline = true;
            ExecutePacket(p);
            s->m_ExecutingInline = false;
            s->m_Stats.m_ExecuteUs += dmTime::GetMonotonicTime() - begin;
            if (end_frame) { ++s->m_Stats.m_Submitted; ++s->m_Stats.m_Completed; }
        }
        else
        {
            DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
            if (end_frame) ++s->m_Stats.m_Submitted;
            s->m_ReadSlot = s->m_BuildSlot;
            s->m_Busy = true;
            s->m_Stats.m_MaxOutstanding = 1;
            dmConditionVariable::Broadcast(s->m_Changed);
        }
        if (s->m_RenderLayer) dmMutex::Lock(s->m_Mutex);
        s->m_BuildSlot = 1 - s->m_BuildSlot;
        GraphicsFramePacket& next = s->m_Packets[s->m_BuildSlot];
        next.m_Bytes.SetSize(0);
        next.m_Commands = 0;
        next.m_EndFrame = false;
        if (s->m_RenderLayer) dmMutex::Unlock(s->m_Mutex);
    }

    void FlushGraphicsPackets()
    {
        if (!g_Packets || !ProducerCall()) return;
        Submit(false);
        WaitIdle();
        // Completion callbacks are producer-owned and run once, after execution.
        // Remove each record before invoking it so a callback may service requests.
        for (;;)
        {
            dmMutex::Lock(g_Packets->m_Mutex);
            if (!g_Packets->m_Completions.Size()) { dmMutex::Unlock(g_Packets->m_Mutex); break; }
            OwnerCompletion completion = g_Packets->m_Completions[0];
            memmove(g_Packets->m_Completions.Begin(), g_Packets->m_Completions.Begin() + 1, (g_Packets->m_Completions.Size() - 1) * sizeof(OwnerCompletion));
            g_Packets->m_Completions.Pop();
            dmMutex::Unlock(g_Packets->m_Mutex);
            if (completion.m_Callback) completion.m_Callback(completion.m_Context);
        }
    }

    static void Synchronous(PacketExecute execute, void* data)
    {
        FlushGraphicsPackets();
        PacketService* s = g_Packets;
        ++s->m_Stats.m_SynchronousCalls;
        if (s->m_External)
        {
            s->m_External(execute, data);
        }
        else if (s->m_Stats.m_Mode == 1)
        {
            uint64_t begin = dmTime::GetMonotonicTime();
            s->m_ExecutingInline = true;
            execute(data);
            s->m_ExecutingInline = false;
            s->m_Stats.m_ExecuteUs += dmTime::GetMonotonicTime() - begin;
        }
        else
        {
            dmMutex::Lock(s->m_Mutex);
            s->m_Control = execute;
            s->m_ControlData = data; // Caller blocks, including the lifetime of all pointer operands.
            s->m_Busy = true;
            dmConditionVariable::Broadcast(s->m_Changed);
            while (s->m_Busy) dmConditionVariable::Wait(s->m_Changed, s->m_Mutex);
            dmMutex::Unlock(s->m_Mutex);
        }
    }

    bool IsRenderGraphicsOwnerActive() { return g_Packets && g_Packets->m_RenderLayer; }

    bool StartRenderGraphicsOwner(HContext context, bool threaded)
    {
        if (!StartGraphicsPackets(context, threaded ? 2 : 1)) return false;
        g_Packets->m_RenderLayer = true;
        return true;
    }

    void RunGraphicsOwnerControl(GraphicsOwnerTask execute, void* data)
    {
        if (!IsRenderGraphicsOwnerActive() || !ProducerCall()) PacketError("owner control outside producer");
        Synchronous(execute, data);
    }

    bool SubmitGraphicsOwnerFrame(GraphicsOwnerTask consume, void* owned_frame)
    {
        if (!IsRenderGraphicsOwnerActive() || !ProducerCall() || !consume || g_Packets->m_Paused) return false;
        FlushGraphicsPackets();
        PacketService* s = g_Packets;
        if (s->m_Stats.m_Mode == 1)
        {
            ++s->m_Stats.m_Submitted;
            s->m_ExecutingInline = true;
            uint64_t begin = dmTime::GetMonotonicTime();
            consume(owned_frame);
            s->m_Stats.m_ExecuteUs += dmTime::GetMonotonicTime() - begin;
            s->m_ExecutingInline = false;
            ++s->m_Stats.m_Completed;
        }
        else
        {
            DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
            ++s->m_Stats.m_Submitted;
            s->m_Stats.m_MaxOutstanding = 1;
            s->m_Control = consume;
            s->m_ControlData = owned_frame; // Retired by producer only after drain.
            s->m_OwnerFrame = true;
            s->m_Busy = true;
            dmConditionVariable::Broadcast(s->m_Changed);
        }
        return true;
    }

    bool QueueGraphicsOwnerRequest(GraphicsOwnerTask execute, const void* data, uint32_t size,
                                    GraphicsOwnerCompletion complete, void* producer_context)
    {
        // Resource/extension jobs have their own copied records, never an open
        // graphics-state stream. Main consumes completions at its next barrier.
        if (!IsRenderGraphicsOwnerActive() || !execute || (size && !data) || g_Packets->m_External) return false;
        PacketService* s = g_Packets;
        DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
        if (s->m_Closing || s->m_Stop || size > 8 * 1024 * 1024 || size + (uint64_t)s->m_JobBytes + s->m_Packets[0].m_Bytes.Capacity() + s->m_Packets[1].m_Bytes.Capacity() > 8 * 1024 * 1024 ||
            s->m_JobCount + s->m_Completions.Size() + (uint32_t)s->m_JobRunning + s->m_Packets[s->m_BuildSlot].m_Commands >= 256) return false;
        OwnerJob& job = s->m_Jobs[(s->m_JobRead + s->m_JobCount) % 256];
        job.m_Data = malloc(size ? size : 1);
        if (!job.m_Data) return false;
        if (size) memcpy(job.m_Data, data, size);
        job.m_Size = size;
        job.m_Execute = execute;
        job.m_Complete = complete;
        job.m_Context = producer_context;
        s->m_JobBytes += size;
        ++s->m_JobCount;
        dmConditionVariable::Broadcast(s->m_Changed);
        return true;
    }

    static void RememberBuffer(uintptr_t buffer, uint32_t size)
    {
        dmHashTable64<uint32_t>& table = g_Packets->m_BufferSizes;
        if (!table.Get(buffer) && table.Full())
        {
            uint32_t capacity = dmMath::Max(64u, table.Capacity() * 2);
            if (capacity > MAX_BUFFER_RECORDS) PacketError("buffer metadata capacity exceeded");
            table.SetCapacity(capacity / 2, capacity);
        }
        table.Put(buffer, size);
    }

    void RememberCapturedGraphicsBuffer(uintptr_t buffer, uint32_t size)
    {
        if (g_Packets && ProducerCall()) RememberBuffer(buffer, size);
    }

    void RememberCapturedGraphicsViewport(int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        if (!g_Packets || !ProducerCall()) return;
        g_Packets->m_ViewportX = x; g_Packets->m_ViewportY = y;
        g_Packets->m_ViewportWidth = width; g_Packets->m_ViewportHeight = height;
    }

    bool GetGraphicsPacketBufferSize(uintptr_t buffer, uint32_t* size)
    {
        if (!g_Packets || !ProducerCall()) return false;
        uint32_t* cached = g_Packets->m_BufferSizes.Get(buffer);
        if (!cached) return false; // Immutable preloaded buffer, not yet modified by a packet.
        *size = *cached;
        return true;
    }

    void ForgetGraphicsPacketBuffer(uintptr_t buffer)
    {
        if (g_Packets && ProducerCall() && g_Packets->m_BufferSizes.Get(buffer)) g_Packets->m_BufferSizes.Erase(buffer);
    }

    void GetGraphicsPacketStats(GraphicsPacketStats* stats)
    {
        memset(stats, 0, sizeof(*stats));
        if (!g_Packets) return;
        PacketService* s = g_Packets;
        DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
        *stats = s->m_Stats;
        uint32_t capacity = s->m_BufferSizes.Capacity();
        stats->m_BufferMetadataBytes = capacity ? dmHashTable64<uint32_t>::GetMemorySize(capacity / 2, capacity) : 0;
        stats->m_StackReservedBytes = s->m_Stats.m_Mode == 2 && !s->m_External ? 0x80000 : 0;
        stats->m_CapacityBytes = stats->m_BufferMetadataBytes + sizeof(*s) + s->m_Packets[0].m_Bytes.Capacity() + s->m_Packets[1].m_Bytes.Capacity() + s->m_JobBytes + s->m_Completions.Capacity() * sizeof(OwnerCompletion);
    }

    void PrepareGraphicsPacketSurface()
    {
        if (!g_Packets) return;
        // This is called at publication, after recording N+1 overlapped execution N.
        {
            DM_MUTEX_SCOPED_LOCK(g_Packets->m_Mutex);
            if (g_Packets->m_Busy) ++g_Packets->m_Stats.m_OverlapFrames;
        }
        WaitIdle();
        if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_METAL)
            PrepareRenderThreadSurface(g_Packets->m_Context, true);
    }

    static void DrainPacketGpu(void* data)
    {
        if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_METAL) DrainRenderThreadGpu((HContext)data);
    }

    bool AreGraphicsPacketsPaused()
    {
        return g_Packets && g_Packets->m_Paused;
    }

    bool SetGraphicsPacketsPaused(bool paused)
    {
        if (!g_Packets || !ProducerCall() || g_Packets->m_FrameOpen) return false;
        Synchronous(DrainPacketGpu, g_Packets->m_Context);
        g_Packets->m_Paused = paused;
        return true;
    }

    void StopGraphicsPackets()
    {
        if (!g_Packets) return;
        PacketService* s = g_Packets;
        if (s->m_FrameOpen) PacketError("shutdown with an unfinished frame");
        {
            DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
            s->m_Closing = true; // Close admission before callbacks can enqueue follow-up work.
        }
        Synchronous(DrainPacketGpu, s->m_Context);
        if (s->m_Stats.m_Mode == 2)
        {
            dmMutex::Lock(s->m_Mutex);
            s->m_Stop = true;
            dmConditionVariable::Broadcast(s->m_Changed);
            dmMutex::Unlock(s->m_Mutex);
            dmThread::Join(s->m_Thread);
        }
        *s->m_Frontend = s->m_Backend;
        if (GetInstalledAdapterFamily() == ADAPTER_FAMILY_METAL) PrepareRenderThreadSurface(s->m_Context, false);
        dmConditionVariable::Delete(s->m_Changed);
        dmMutex::Delete(s->m_Mutex);
        g_Packets = 0;
        delete s;
    }

    static HContext PacketNewContext(const ContextParams& params)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewContext(params); }
        PacketError("NewContext is not admitted");
        return (HContext)0;
    }

    static void PacketDeleteContext(HContext context)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteContext(context); return; }
        PacketError("DeleteContext is not admitted");
    }

    static void PacketFinalize()
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_Finalize(); return; }
        PacketError("Finalize is not admitted");
    }

    struct WindowArgsCloseWindow { HContext context; };
    static void ExecuteWindowCloseWindow(void* data)
    {
        WindowArgsCloseWindow* a = (WindowArgsCloseWindow*)data;
        g_Packets->m_Backend.m_CloseWindow(a->context);
    }
    static void PacketCloseWindow(HContext context)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_CloseWindow(context); return; }
        WindowArgsCloseWindow a = {context};
        Synchronous(ExecuteWindowCloseWindow, &a);
    }

    static HWindow PacketGetWindow(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetWindow(context); }
        return g_Packets->m_Backend.m_GetWindow(context);
    }

    static uint32_t PacketGetDisplayDpi(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetDisplayDpi(context); }
        return g_Packets->m_Backend.m_GetDisplayDpi(context);
    }

    struct WindowArgsSetWindowSize { HContext context; uint32_t width, height; };
    static void ExecuteWindowSetWindowSize(void* data)
    {
        WindowArgsSetWindowSize* a = (WindowArgsSetWindowSize*)data;
        g_Packets->m_Backend.m_SetWindowSize(a->context, a->width, a->height);
    }
    static void PacketSetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetWindowSize(context, width, height); return; }
        WindowArgsSetWindowSize a = {context, width, height};
        Synchronous(ExecuteWindowSetWindowSize, &a);
    }

    struct WindowArgsResizeWindow { HContext context; uint32_t width, height; };
    static void ExecuteWindowResizeWindow(void* data)
    {
        WindowArgsResizeWindow* a = (WindowArgsResizeWindow*)data;
        g_Packets->m_Backend.m_ResizeWindow(a->context, a->width, a->height);
    }
    static void PacketResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_ResizeWindow(context, width, height); return; }
        WindowArgsResizeWindow a = {context, width, height};
        Synchronous(ExecuteWindowResizeWindow, &a);
    }

    struct ArgsBeginFrame
    {
        HContext context;
    };
    static void ExecuteBeginFrame(void* object)
    {
        ArgsBeginFrame* a = (ArgsBeginFrame*)object;
        g_Packets->m_Backend.m_BeginFrame(a->context);
    }
    static void PacketBeginFrame(HContext context)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_BeginFrame(context); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        if (g_Packets->m_FrameOpen) PacketError("nested BeginFrame");
        g_Packets->m_FrameOpen = true;
        ArgsBeginFrame* a = (ArgsBeginFrame*)Record(ExecuteBeginFrame, sizeof(ArgsBeginFrame));
        a->context = context;
    }

    struct ArgsFlip
    {
        HContext context;
    };
    static void ExecuteFlip(void* object)
    {
        ArgsFlip* a = (ArgsFlip*)object;
        g_Packets->m_Backend.m_Flip(a->context);
    }
    static void PacketFlip(HContext context)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_Flip(context); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        if (!g_Packets->m_FrameOpen) PacketError("Flip without BeginFrame");
        g_Packets->m_FrameOpen = false;
        ArgsFlip* a = (ArgsFlip*)Record(ExecuteFlip, sizeof(ArgsFlip));
        a->context = context;
        PrepareGraphicsPacketSurface();
        Submit(true);
    }

    struct ArgsClear
    {
        HContext context;
        uint32_t flags;
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;
        float depth;
        uint32_t stencil;
    };
    static void ExecuteClear(void* object)
    {
        ArgsClear* a = (ArgsClear*)object;
        g_Packets->m_Backend.m_Clear(a->context, a->flags, a->red, a->green, a->blue, a->alpha, a->depth, a->stencil);
    }
    static void PacketClear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_Clear(context, flags, red, green, blue, alpha, depth, stencil); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        ArgsClear* a = (ArgsClear*)Record(ExecuteClear, sizeof(ArgsClear));
        a->context = context;
        a->flags = flags;
        a->red = red;
        a->green = green;
        a->blue = blue;
        a->alpha = alpha;
        a->depth = depth;
        a->stencil = stencil;
    }

    struct ArgsNewVertexBuffer
    {
        HContext context;
        uint32_t size;
        const void* data;
        BufferUsage buffer_usage;
        HVertexBuffer result;
    };
    static void ExecuteNewVertexBuffer(void* object)
    {
        ArgsNewVertexBuffer* a = (ArgsNewVertexBuffer*)object;
        a->result = g_Packets->m_Backend.m_NewVertexBuffer(a->context, a->size, a->data, a->buffer_usage);
    }
    static HVertexBuffer PacketNewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewVertexBuffer(context, size, data, buffer_usage); }
        ArgsNewVertexBuffer a;
        a.context = context;
        a.size = size;
        a.data = data;
        a.buffer_usage = buffer_usage;
        Synchronous(ExecuteNewVertexBuffer, &a);
        RememberBuffer(a.result, size);
        return a.result;
    }

    struct ArgsDeleteVertexBuffer
    {
        HVertexBuffer buffer;
    };
    static void ExecuteDeleteVertexBuffer(void* object)
    {
        ArgsDeleteVertexBuffer* a = (ArgsDeleteVertexBuffer*)object;
        g_Packets->m_Backend.m_DeleteVertexBuffer(a->buffer);
    }
    static void PacketDeleteVertexBuffer(HVertexBuffer buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteVertexBuffer(buffer); return; }
        ArgsDeleteVertexBuffer a;
        a.buffer = buffer;
        Synchronous(ExecuteDeleteVertexBuffer, &a);
        ForgetGraphicsPacketBuffer(buffer);
    }

    struct ArgsSetVertexBufferData
    {
        HVertexBuffer buffer;
        uint32_t size;
        bool has_data;
        BufferUsage buffer_usage;
    };
    static void ExecuteSetVertexBufferData(void* object)
    {
        ArgsSetVertexBufferData* a = (ArgsSetVertexBufferData*)object;
        g_Packets->m_Backend.m_SetVertexBufferData(a->buffer, a->size, a->has_data ? (const void*)((uint8_t*)object + Align16(sizeof(ArgsSetVertexBufferData))) : 0, a->buffer_usage);
    }
    static void PacketSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetVertexBufferData(buffer, size, data, buffer_usage); return; }
        RememberBuffer(buffer, size);
        ArgsSetVertexBufferData* a = (ArgsSetVertexBufferData*)Record(ExecuteSetVertexBufferData, sizeof(ArgsSetVertexBufferData), data, data ? size : 0);
        a->buffer = buffer;
        a->size = size;
        a->has_data = data != 0;
        a->buffer_usage = buffer_usage;
    }

    struct ArgsSetVertexBufferSubData
    {
        HVertexBuffer buffer;
        uint32_t offset;
        uint32_t size;
        bool has_data;
    };
    static void ExecuteSetVertexBufferSubData(void* object)
    {
        ArgsSetVertexBufferSubData* a = (ArgsSetVertexBufferSubData*)object;
        g_Packets->m_Backend.m_SetVertexBufferSubData(a->buffer, a->offset, a->size, a->has_data ? (const void*)((uint8_t*)object + Align16(sizeof(ArgsSetVertexBufferSubData))) : 0);
    }
    static void PacketSetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetVertexBufferSubData(buffer, offset, size, data); return; }
        ArgsSetVertexBufferSubData* a = (ArgsSetVertexBufferSubData*)Record(ExecuteSetVertexBufferSubData, sizeof(ArgsSetVertexBufferSubData), data, data ? size : 0);
        a->buffer = buffer;
        a->offset = offset;
        a->size = size;
        a->has_data = data != 0;
    }

    static uint32_t PacketGetMaxElementsVertices(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetMaxElementsVertices(context); }
        return g_Packets->m_Backend.m_GetMaxElementsVertices(context);
    }

    struct ArgsNewIndexBuffer
    {
        HContext context;
        uint32_t size;
        const void* data;
        BufferUsage buffer_usage;
        HIndexBuffer result;
    };
    static void ExecuteNewIndexBuffer(void* object)
    {
        ArgsNewIndexBuffer* a = (ArgsNewIndexBuffer*)object;
        a->result = g_Packets->m_Backend.m_NewIndexBuffer(a->context, a->size, a->data, a->buffer_usage);
    }
    static HIndexBuffer PacketNewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewIndexBuffer(context, size, data, buffer_usage); }
        ArgsNewIndexBuffer a;
        a.context = context;
        a.size = size;
        a.data = data;
        a.buffer_usage = buffer_usage;
        Synchronous(ExecuteNewIndexBuffer, &a);
        RememberBuffer(a.result, size);
        return a.result;
    }

    struct ArgsDeleteIndexBuffer
    {
        HIndexBuffer buffer;
    };
    static void ExecuteDeleteIndexBuffer(void* object)
    {
        ArgsDeleteIndexBuffer* a = (ArgsDeleteIndexBuffer*)object;
        g_Packets->m_Backend.m_DeleteIndexBuffer(a->buffer);
    }
    static void PacketDeleteIndexBuffer(HIndexBuffer buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteIndexBuffer(buffer); return; }
        ArgsDeleteIndexBuffer a;
        a.buffer = buffer;
        Synchronous(ExecuteDeleteIndexBuffer, &a);
        ForgetGraphicsPacketBuffer(buffer);
    }

    struct ArgsSetIndexBufferData
    {
        HIndexBuffer buffer;
        uint32_t size;
        bool has_data;
        BufferUsage buffer_usage;
    };
    static void ExecuteSetIndexBufferData(void* object)
    {
        ArgsSetIndexBufferData* a = (ArgsSetIndexBufferData*)object;
        g_Packets->m_Backend.m_SetIndexBufferData(a->buffer, a->size, a->has_data ? (const void*)((uint8_t*)object + Align16(sizeof(ArgsSetIndexBufferData))) : 0, a->buffer_usage);
    }
    static void PacketSetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetIndexBufferData(buffer, size, data, buffer_usage); return; }
        RememberBuffer(buffer, size);
        ArgsSetIndexBufferData* a = (ArgsSetIndexBufferData*)Record(ExecuteSetIndexBufferData, sizeof(ArgsSetIndexBufferData), data, data ? size : 0);
        a->buffer = buffer;
        a->size = size;
        a->has_data = data != 0;
        a->buffer_usage = buffer_usage;
    }

    struct ArgsSetIndexBufferSubData
    {
        HIndexBuffer buffer;
        uint32_t offset;
        uint32_t size;
        bool has_data;
    };
    static void ExecuteSetIndexBufferSubData(void* object)
    {
        ArgsSetIndexBufferSubData* a = (ArgsSetIndexBufferSubData*)object;
        g_Packets->m_Backend.m_SetIndexBufferSubData(a->buffer, a->offset, a->size, a->has_data ? (const void*)((uint8_t*)object + Align16(sizeof(ArgsSetIndexBufferSubData))) : 0);
    }
    static void PacketSetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetIndexBufferSubData(buffer, offset, size, data); return; }
        ArgsSetIndexBufferSubData* a = (ArgsSetIndexBufferSubData*)Record(ExecuteSetIndexBufferSubData, sizeof(ArgsSetIndexBufferSubData), data, data ? size : 0);
        a->buffer = buffer;
        a->offset = offset;
        a->size = size;
        a->has_data = data != 0;
    }

    static bool PacketIsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_IsIndexBufferFormatSupported(context, format); }
        return g_Packets->m_Backend.m_IsIndexBufferFormatSupported(context, format);
    }

    static uint32_t PacketGetMaxElementsIndices(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetMaxElementsIndices(context); }
        return g_Packets->m_Backend.m_GetMaxElementsIndices(context);
    }

    struct ArgsNewVertexDeclaration
    {
        HContext context;
        HVertexStreamDeclaration stream_declaration;
        HVertexDeclaration result;
    };
    static void ExecuteNewVertexDeclaration(void* object)
    {
        ArgsNewVertexDeclaration* a = (ArgsNewVertexDeclaration*)object;
        a->result = g_Packets->m_Backend.m_NewVertexDeclaration(a->context, a->stream_declaration);
    }
    static HVertexDeclaration PacketNewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewVertexDeclaration(context, stream_declaration); }
        ArgsNewVertexDeclaration a;
        a.context = context;
        a.stream_declaration = stream_declaration;
        Synchronous(ExecuteNewVertexDeclaration, &a);
        return a.result;
    }

    struct ArgsNewVertexDeclarationStride
    {
        HContext context;
        HVertexStreamDeclaration stream_declaration;
        uint32_t stride;
        HVertexDeclaration result;
    };
    static void ExecuteNewVertexDeclarationStride(void* object)
    {
        ArgsNewVertexDeclarationStride* a = (ArgsNewVertexDeclarationStride*)object;
        a->result = g_Packets->m_Backend.m_NewVertexDeclarationStride(a->context, a->stream_declaration, a->stride);
    }
    static HVertexDeclaration PacketNewVertexDeclarationStride(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewVertexDeclarationStride(context, stream_declaration, stride); }
        ArgsNewVertexDeclarationStride a;
        a.context = context;
        a.stream_declaration = stream_declaration;
        a.stride = stride;
        Synchronous(ExecuteNewVertexDeclarationStride, &a);
        return a.result;
    }

    struct ArgsEnableVertexDeclaration
    {
        HContext context;
        HVertexDeclaration vertex_declaration;
        uint32_t binding_index;
        uint32_t base_offset;
        HProgram program;
    };
    static void ExecuteEnableVertexDeclaration(void* object)
    {
        ArgsEnableVertexDeclaration* a = (ArgsEnableVertexDeclaration*)object;
        g_Packets->m_Backend.m_EnableVertexDeclaration(a->context, a->vertex_declaration, a->binding_index, a->base_offset, a->program);
    }
    static void PacketEnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableVertexDeclaration(context, vertex_declaration, binding_index, base_offset, program); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        ArgsEnableVertexDeclaration* a = (ArgsEnableVertexDeclaration*)Record(ExecuteEnableVertexDeclaration, sizeof(ArgsEnableVertexDeclaration));
        a->context = context;
        a->vertex_declaration = vertex_declaration;
        a->binding_index = binding_index;
        a->base_offset = base_offset;
        a->program = program;
    }

    struct ArgsDisableVertexDeclaration
    {
        HContext context;
        HVertexDeclaration vertex_declaration;
    };
    static void ExecuteDisableVertexDeclaration(void* object)
    {
        ArgsDisableVertexDeclaration* a = (ArgsDisableVertexDeclaration*)object;
        g_Packets->m_Backend.m_DisableVertexDeclaration(a->context, a->vertex_declaration);
    }
    static void PacketDisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableVertexDeclaration(context, vertex_declaration); return; }
        ArgsDisableVertexDeclaration* a = (ArgsDisableVertexDeclaration*)Record(ExecuteDisableVertexDeclaration, sizeof(ArgsDisableVertexDeclaration));
        a->context = context;
        a->vertex_declaration = vertex_declaration;
    }

    struct ArgsEnableVertexBuffer
    {
        HContext context;
        HVertexBuffer vertex_buffer;
        uint32_t binding_index;
    };
    static void ExecuteEnableVertexBuffer(void* object)
    {
        ArgsEnableVertexBuffer* a = (ArgsEnableVertexBuffer*)object;
        g_Packets->m_Backend.m_EnableVertexBuffer(a->context, a->vertex_buffer, a->binding_index);
    }
    static void PacketEnableVertexBuffer(HContext context, HVertexBuffer vertex_buffer, uint32_t binding_index)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableVertexBuffer(context, vertex_buffer, binding_index); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        ArgsEnableVertexBuffer* a = (ArgsEnableVertexBuffer*)Record(ExecuteEnableVertexBuffer, sizeof(ArgsEnableVertexBuffer));
        a->context = context;
        a->vertex_buffer = vertex_buffer;
        a->binding_index = binding_index;
    }

    struct ArgsDisableVertexBuffer
    {
        HContext context;
        HVertexBuffer vertex_buffer;
    };
    static void ExecuteDisableVertexBuffer(void* object)
    {
        ArgsDisableVertexBuffer* a = (ArgsDisableVertexBuffer*)object;
        g_Packets->m_Backend.m_DisableVertexBuffer(a->context, a->vertex_buffer);
    }
    static void PacketDisableVertexBuffer(HContext context, HVertexBuffer vertex_buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableVertexBuffer(context, vertex_buffer); return; }
        ArgsDisableVertexBuffer* a = (ArgsDisableVertexBuffer*)Record(ExecuteDisableVertexBuffer, sizeof(ArgsDisableVertexBuffer));
        a->context = context;
        a->vertex_buffer = vertex_buffer;
    }

    struct ArgsDrawElements
    {
        HContext context;
        PrimitiveType prim_type;
        uint32_t first;
        uint32_t count;
        Type type;
        HIndexBuffer index_buffer;
        uint32_t instance_count;
    };
    static void ExecuteDrawElements(void* object)
    {
        ArgsDrawElements* a = (ArgsDrawElements*)object;
        g_Packets->m_Backend.m_DrawElements(a->context, a->prim_type, a->first, a->count, a->type, a->index_buffer, a->instance_count);
    }
    static void PacketDrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DrawElements(context, prim_type, first, count, type, index_buffer, instance_count); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        g_Packets->m_Pipeline.m_PrimtiveType = prim_type;
        ArgsDrawElements* a = (ArgsDrawElements*)Record(ExecuteDrawElements, sizeof(ArgsDrawElements));
        a->context = context;
        a->prim_type = prim_type;
        a->first = first;
        a->count = count;
        a->type = type;
        a->index_buffer = index_buffer;
        a->instance_count = instance_count;
    }

    struct ArgsDraw
    {
        HContext context;
        PrimitiveType prim_type;
        uint32_t first;
        uint32_t count;
        uint32_t instance_count;
    };
    static void ExecuteDraw(void* object)
    {
        ArgsDraw* a = (ArgsDraw*)object;
        g_Packets->m_Backend.m_Draw(a->context, a->prim_type, a->first, a->count, a->instance_count);
    }
    static void PacketDraw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_Draw(context, prim_type, first, count, instance_count); return; }
        if (g_Packets->m_RenderLayer) PacketError("render command must execute on render owner");
        g_Packets->m_Pipeline.m_PrimtiveType = prim_type;
        ArgsDraw* a = (ArgsDraw*)Record(ExecuteDraw, sizeof(ArgsDraw));
        a->context = context;
        a->prim_type = prim_type;
        a->first = first;
        a->count = count;
        a->instance_count = instance_count;
    }

    static void PacketDispatchCompute(HContext context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DispatchCompute(context, group_count_x, group_count_y, group_count_z); return; }
        PacketError("DispatchCompute is not admitted");
    }

    struct ArgsNewProgram
    {
        HContext context;
        ShaderDesc* ddf;
        char* error_buffer;
        uint32_t error_buffer_size;
        HProgram result;
    };
    static void ExecuteNewProgram(void* object)
    {
        ArgsNewProgram* a = (ArgsNewProgram*)object;
        a->result = g_Packets->m_Backend.m_NewProgram(a->context, a->ddf, a->error_buffer, a->error_buffer_size);
    }
    static HProgram PacketNewProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewProgram(context, ddf, error_buffer, error_buffer_size); }
        ArgsNewProgram a;
        a.context = context;
        a.ddf = ddf;
        a.error_buffer = error_buffer;
        a.error_buffer_size = error_buffer_size;
        Synchronous(ExecuteNewProgram, &a);
        return a.result;
    }

    struct ArgsDeleteProgram
    {
        HContext context;
        HProgram program;
    };
    static void ExecuteDeleteProgram(void* object)
    {
        ArgsDeleteProgram* a = (ArgsDeleteProgram*)object;
        g_Packets->m_Backend.m_DeleteProgram(a->context, a->program);
    }
    static void PacketDeleteProgram(HContext context, HProgram program)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteProgram(context, program); return; }
        ArgsDeleteProgram a;
        a.context = context;
        a.program = program;
        Synchronous(ExecuteDeleteProgram, &a);
    }

    static ShaderDesc::Language PacketGetProgramLanguage(HProgram program)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetProgramLanguage(program); }
        return g_Packets->m_Backend.m_GetProgramLanguage(program);
    }

    static bool PacketIsShaderLanguageSupported(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_IsShaderLanguageSupported(context, language, shader_type); }
        return g_Packets->m_Backend.m_IsShaderLanguageSupported(context, language, shader_type);
    }

    struct ArgsEnableProgram
    {
        HContext context;
        HProgram program;
    };
    static void ExecuteEnableProgram(void* object)
    {
        ArgsEnableProgram* a = (ArgsEnableProgram*)object;
        g_Packets->m_Backend.m_EnableProgram(a->context, a->program);
    }
    static void PacketEnableProgram(HContext context, HProgram program)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableProgram(context, program); return; }
        ArgsEnableProgram* a = (ArgsEnableProgram*)Record(ExecuteEnableProgram, sizeof(ArgsEnableProgram));
        a->context = context;
        a->program = program;
    }

    struct ArgsDisableProgram
    {
        HContext context;
    };
    static void ExecuteDisableProgram(void* object)
    {
        ArgsDisableProgram* a = (ArgsDisableProgram*)object;
        g_Packets->m_Backend.m_DisableProgram(a->context);
    }
    static void PacketDisableProgram(HContext context)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableProgram(context); return; }
        ArgsDisableProgram* a = (ArgsDisableProgram*)Record(ExecuteDisableProgram, sizeof(ArgsDisableProgram));
        a->context = context;
    }

    struct ArgsReloadProgram
    {
        HContext context;
        HProgram program;
        ShaderDesc* ddf;
        char* error_buffer;
        uint32_t error_buffer_size;
        bool result;
    };
    static void ExecuteReloadProgram(void* object)
    {
        ArgsReloadProgram* a = (ArgsReloadProgram*)object;
        a->result = g_Packets->m_Backend.m_ReloadProgram(a->context, a->program, a->ddf, a->error_buffer, a->error_buffer_size);
    }
    static bool PacketReloadProgram(HContext context, HProgram program, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_ReloadProgram(context, program, ddf, error_buffer, error_buffer_size); }
        ArgsReloadProgram a;
        a.context = context;
        a.program = program;
        a.ddf = ddf;
        a.error_buffer = error_buffer;
        a.error_buffer_size = error_buffer_size;
        Synchronous(ExecuteReloadProgram, &a);
        return a.result;
    }

    static uint32_t PacketGetAttributeCount(HProgram prog)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetAttributeCount(prog); }
        return g_Packets->m_Backend.m_GetAttributeCount(prog);
    }

    static void PacketGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_GetAttribute(prog, index, name_hash, type, element_count, num_values, location); return; }
        g_Packets->m_Backend.m_GetAttribute(prog, index, name_hash, type, element_count, num_values, location); return;
    }

    struct ArgsSetConstantV4
    {
        HContext context;
        bool has_data;
        int count;
        HUniformLocation base_location;
    };
    static void ExecuteSetConstantV4(void* object)
    {
        ArgsSetConstantV4* a = (ArgsSetConstantV4*)object;
        g_Packets->m_Backend.m_SetConstantV4(a->context, a->has_data ? (const dmVMath::Vector4*)((uint8_t*)object + Align16(sizeof(ArgsSetConstantV4))) : 0, a->count, a->base_location);
    }
    static void PacketSetConstantV4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetConstantV4(context, data, count, base_location); return; }
        ArgsSetConstantV4* a = (ArgsSetConstantV4*)Record(ExecuteSetConstantV4, sizeof(ArgsSetConstantV4), data, data ? (uint64_t)count * sizeof(dmVMath::Vector4) : 0);
        a->context = context;
        a->has_data = data != 0;
        a->count = count;
        a->base_location = base_location;
    }

    struct ArgsSetConstantM4
    {
        HContext context;
        bool has_data;
        int count;
        HUniformLocation base_location;
    };
    static void ExecuteSetConstantM4(void* object)
    {
        ArgsSetConstantM4* a = (ArgsSetConstantM4*)object;
        g_Packets->m_Backend.m_SetConstantM4(a->context, a->has_data ? (const dmVMath::Vector4*)((uint8_t*)object + Align16(sizeof(ArgsSetConstantM4))) : 0, a->count, a->base_location);
    }
    static void PacketSetConstantM4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetConstantM4(context, data, count, base_location); return; }
        ArgsSetConstantM4* a = (ArgsSetConstantM4*)Record(ExecuteSetConstantM4, sizeof(ArgsSetConstantM4), data, data ? (uint64_t)count * sizeof(dmVMath::Vector4) * 4 : 0);
        a->context = context;
        a->has_data = data != 0;
        a->count = count;
        a->base_location = base_location;
    }

    struct ArgsSetSampler
    {
        HContext context;
        HUniformLocation location;
        int32_t unit;
    };
    static void ExecuteSetSampler(void* object)
    {
        ArgsSetSampler* a = (ArgsSetSampler*)object;
        g_Packets->m_Backend.m_SetSampler(a->context, a->location, a->unit);
    }
    static void PacketSetSampler(HContext context, HUniformLocation location, int32_t unit)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetSampler(context, location, unit); return; }
        ArgsSetSampler* a = (ArgsSetSampler*)Record(ExecuteSetSampler, sizeof(ArgsSetSampler));
        a->context = context;
        a->location = location;
        a->unit = unit;
    }

    struct ArgsSetViewport
    {
        HContext context;
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    };
    static void ExecuteSetViewport(void* object)
    {
        ArgsSetViewport* a = (ArgsSetViewport*)object;
        g_Packets->m_Backend.m_SetViewport(a->context, a->x, a->y, a->width, a->height);
    }
    static void PacketSetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetViewport(context, x, y, width, height); return; }
        g_Packets->m_ViewportX = x; g_Packets->m_ViewportY = y; g_Packets->m_ViewportWidth = width; g_Packets->m_ViewportHeight = height;
        ArgsSetViewport* a = (ArgsSetViewport*)Record(ExecuteSetViewport, sizeof(ArgsSetViewport));
        a->context = context;
        a->x = x;
        a->y = y;
        a->width = width;
        a->height = height;
    }

    struct ArgsEnableState
    {
        HContext context;
        State state;
    };
    static void ExecuteEnableState(void* object)
    {
        ArgsEnableState* a = (ArgsEnableState*)object;
        g_Packets->m_Backend.m_EnableState(a->context, a->state);
    }
    static void PacketEnableState(HContext context, State state)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableState(context, state); return; }
        SetPipelineStateValue(g_Packets->m_Pipeline, state, 1);
        ArgsEnableState* a = (ArgsEnableState*)Record(ExecuteEnableState, sizeof(ArgsEnableState));
        a->context = context;
        a->state = state;
    }

    struct ArgsDisableState
    {
        HContext context;
        State state;
    };
    static void ExecuteDisableState(void* object)
    {
        ArgsDisableState* a = (ArgsDisableState*)object;
        g_Packets->m_Backend.m_DisableState(a->context, a->state);
    }
    static void PacketDisableState(HContext context, State state)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableState(context, state); return; }
        SetPipelineStateValue(g_Packets->m_Pipeline, state, 0);
        ArgsDisableState* a = (ArgsDisableState*)Record(ExecuteDisableState, sizeof(ArgsDisableState));
        a->context = context;
        a->state = state;
    }

    struct ArgsSetBlendFunc
    {
        HContext context;
        BlendFactor source_factor;
        BlendFactor destinaton_factor;
    };
    static void ExecuteSetBlendFunc(void* object)
    {
        ArgsSetBlendFunc* a = (ArgsSetBlendFunc*)object;
        g_Packets->m_Backend.m_SetBlendFunc(a->context, a->source_factor, a->destinaton_factor);
    }
    static void PacketSetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetBlendFunc(context, source_factor, destinaton_factor); return; }
        g_Packets->m_Pipeline.m_BlendSrcFactor      = source_factor;
        g_Packets->m_Pipeline.m_BlendDstFactor      = destinaton_factor;
        g_Packets->m_Pipeline.m_BlendSrcFactorAlpha = source_factor;
        g_Packets->m_Pipeline.m_BlendDstFactorAlpha = destinaton_factor;
        g_Packets->m_Pipeline.m_BlendEquationColor  = BLEND_EQUATION_ADD;
        g_Packets->m_Pipeline.m_BlendEquationAlpha  = BLEND_EQUATION_ADD;
        ArgsSetBlendFunc* a = (ArgsSetBlendFunc*)Record(ExecuteSetBlendFunc, sizeof(ArgsSetBlendFunc));
        a->context = context;
        a->source_factor = source_factor;
        a->destinaton_factor = destinaton_factor;
    }

    struct ArgsSetBlendFuncSeparate
    {
        HContext context;
        BlendFactor src_factor_color;
        BlendFactor dst_factor_color;
        BlendFactor src_factor_alpha;
        BlendFactor dst_factor_alpha;
    };
    static void ExecuteSetBlendFuncSeparate(void* object)
    {
        ArgsSetBlendFuncSeparate* a = (ArgsSetBlendFuncSeparate*)object;
        g_Packets->m_Backend.m_SetBlendFuncSeparate(a->context, a->src_factor_color, a->dst_factor_color, a->src_factor_alpha, a->dst_factor_alpha);
    }
    static void PacketSetBlendFuncSeparate(HContext context, BlendFactor src_factor_color, BlendFactor dst_factor_color, BlendFactor src_factor_alpha, BlendFactor dst_factor_alpha)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetBlendFuncSeparate(context, src_factor_color, dst_factor_color, src_factor_alpha, dst_factor_alpha); return; }
        g_Packets->m_Pipeline.m_BlendSrcFactor      = src_factor_color;
        g_Packets->m_Pipeline.m_BlendDstFactor      = dst_factor_color;
        g_Packets->m_Pipeline.m_BlendSrcFactorAlpha = src_factor_alpha;
        g_Packets->m_Pipeline.m_BlendDstFactorAlpha = dst_factor_alpha;
        ArgsSetBlendFuncSeparate* a = (ArgsSetBlendFuncSeparate*)Record(ExecuteSetBlendFuncSeparate, sizeof(ArgsSetBlendFuncSeparate));
        a->context = context;
        a->src_factor_color = src_factor_color;
        a->dst_factor_color = dst_factor_color;
        a->src_factor_alpha = src_factor_alpha;
        a->dst_factor_alpha = dst_factor_alpha;
    }

    struct ArgsSetBlendEquationSeparate
    {
        HContext context;
        BlendEquation equation_color;
        BlendEquation equation_alpha;
    };
    static void ExecuteSetBlendEquationSeparate(void* object)
    {
        ArgsSetBlendEquationSeparate* a = (ArgsSetBlendEquationSeparate*)object;
        g_Packets->m_Backend.m_SetBlendEquationSeparate(a->context, a->equation_color, a->equation_alpha);
    }
    static void PacketSetBlendEquationSeparate(HContext context, BlendEquation equation_color, BlendEquation equation_alpha)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetBlendEquationSeparate(context, equation_color, equation_alpha); return; }
        g_Packets->m_Pipeline.m_BlendEquationColor  = equation_color;
        g_Packets->m_Pipeline.m_BlendEquationAlpha  = equation_alpha;
        ArgsSetBlendEquationSeparate* a = (ArgsSetBlendEquationSeparate*)Record(ExecuteSetBlendEquationSeparate, sizeof(ArgsSetBlendEquationSeparate));
        a->context = context;
        a->equation_color = equation_color;
        a->equation_alpha = equation_alpha;
    }

    struct ArgsSetColorMask
    {
        HContext context;
        bool red;
        bool green;
        bool blue;
        bool alpha;
    };
    static void ExecuteSetColorMask(void* object)
    {
        ArgsSetColorMask* a = (ArgsSetColorMask*)object;
        g_Packets->m_Backend.m_SetColorMask(a->context, a->red, a->green, a->blue, a->alpha);
    }
    static void PacketSetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetColorMask(context, red, green, blue, alpha); return; }
        g_Packets->m_Pipeline.m_WriteColorMask = (red ? DM_GRAPHICS_STATE_WRITE_R : 0) | (green ? DM_GRAPHICS_STATE_WRITE_G : 0) | (blue ? DM_GRAPHICS_STATE_WRITE_B : 0) | (alpha ? DM_GRAPHICS_STATE_WRITE_A : 0);
        ArgsSetColorMask* a = (ArgsSetColorMask*)Record(ExecuteSetColorMask, sizeof(ArgsSetColorMask));
        a->context = context;
        a->red = red;
        a->green = green;
        a->blue = blue;
        a->alpha = alpha;
    }

    struct ArgsSetDepthMask
    {
        HContext context;
        bool enable_mask;
    };
    static void ExecuteSetDepthMask(void* object)
    {
        ArgsSetDepthMask* a = (ArgsSetDepthMask*)object;
        g_Packets->m_Backend.m_SetDepthMask(a->context, a->enable_mask);
    }
    static void PacketSetDepthMask(HContext context, bool enable_mask)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetDepthMask(context, enable_mask); return; }
        g_Packets->m_Pipeline.m_WriteDepth = enable_mask;
        ArgsSetDepthMask* a = (ArgsSetDepthMask*)Record(ExecuteSetDepthMask, sizeof(ArgsSetDepthMask));
        a->context = context;
        a->enable_mask = enable_mask;
    }

    struct ArgsSetDepthFunc
    {
        HContext context;
        CompareFunc func;
    };
    static void ExecuteSetDepthFunc(void* object)
    {
        ArgsSetDepthFunc* a = (ArgsSetDepthFunc*)object;
        g_Packets->m_Backend.m_SetDepthFunc(a->context, a->func);
    }
    static void PacketSetDepthFunc(HContext context, CompareFunc func)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetDepthFunc(context, func); return; }
        g_Packets->m_Pipeline.m_DepthTestFunc = func;
        ArgsSetDepthFunc* a = (ArgsSetDepthFunc*)Record(ExecuteSetDepthFunc, sizeof(ArgsSetDepthFunc));
        a->context = context;
        a->func = func;
    }

    struct ArgsSetScissor
    {
        HContext context;
        int32_t x;
        int32_t y;
        int32_t width;
        int32_t height;
    };
    static void ExecuteSetScissor(void* object)
    {
        ArgsSetScissor* a = (ArgsSetScissor*)object;
        g_Packets->m_Backend.m_SetScissor(a->context, a->x, a->y, a->width, a->height);
    }
    static void PacketSetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetScissor(context, x, y, width, height); return; }
        ArgsSetScissor* a = (ArgsSetScissor*)Record(ExecuteSetScissor, sizeof(ArgsSetScissor));
        a->context = context;
        a->x = x;
        a->y = y;
        a->width = width;
        a->height = height;
    }

    struct ArgsSetStencilMask
    {
        HContext context;
        uint32_t mask;
    };
    static void ExecuteSetStencilMask(void* object)
    {
        ArgsSetStencilMask* a = (ArgsSetStencilMask*)object;
        g_Packets->m_Backend.m_SetStencilMask(a->context, a->mask);
    }
    static void PacketSetStencilMask(HContext context, uint32_t mask)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStencilMask(context, mask); return; }
        g_Packets->m_Pipeline.m_StencilWriteMask = mask;
        ArgsSetStencilMask* a = (ArgsSetStencilMask*)Record(ExecuteSetStencilMask, sizeof(ArgsSetStencilMask));
        a->context = context;
        a->mask = mask;
    }

    struct ArgsSetStencilFunc
    {
        HContext context;
        CompareFunc func;
        uint32_t ref;
        uint32_t mask;
    };
    static void ExecuteSetStencilFunc(void* object)
    {
        ArgsSetStencilFunc* a = (ArgsSetStencilFunc*)object;
        g_Packets->m_Backend.m_SetStencilFunc(a->context, a->func, a->ref, a->mask);
    }
    static void PacketSetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStencilFunc(context, func, ref, mask); return; }
        g_Packets->m_Pipeline.m_StencilFrontTestFunc = (uint8_t) func;
        g_Packets->m_Pipeline.m_StencilBackTestFunc  = (uint8_t) func;
        g_Packets->m_Pipeline.m_StencilReference     = (uint8_t) ref;
        g_Packets->m_Pipeline.m_StencilCompareMask   = (uint8_t) mask;
        ArgsSetStencilFunc* a = (ArgsSetStencilFunc*)Record(ExecuteSetStencilFunc, sizeof(ArgsSetStencilFunc));
        a->context = context;
        a->func = func;
        a->ref = ref;
        a->mask = mask;
    }

    struct ArgsSetStencilFuncSeparate
    {
        HContext context;
        FaceType face_type;
        CompareFunc func;
        uint32_t ref;
        uint32_t mask;
    };
    static void ExecuteSetStencilFuncSeparate(void* object)
    {
        ArgsSetStencilFuncSeparate* a = (ArgsSetStencilFuncSeparate*)object;
        g_Packets->m_Backend.m_SetStencilFuncSeparate(a->context, a->face_type, a->func, a->ref, a->mask);
    }
    static void PacketSetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStencilFuncSeparate(context, face_type, func, ref, mask); return; }
        if (face_type == FACE_TYPE_BACK)
        {
            g_Packets->m_Pipeline.m_StencilBackTestFunc  = (uint8_t) func;
        }
        else
        {
            g_Packets->m_Pipeline.m_StencilFrontTestFunc = (uint8_t) func;
        }
        g_Packets->m_Pipeline.m_StencilReference     = (uint8_t) ref;
        g_Packets->m_Pipeline.m_StencilCompareMask   = (uint8_t) mask;
        ArgsSetStencilFuncSeparate* a = (ArgsSetStencilFuncSeparate*)Record(ExecuteSetStencilFuncSeparate, sizeof(ArgsSetStencilFuncSeparate));
        a->context = context;
        a->face_type = face_type;
        a->func = func;
        a->ref = ref;
        a->mask = mask;
    }

    struct ArgsSetStencilOp
    {
        HContext context;
        StencilOp sfail;
        StencilOp dpfail;
        StencilOp dppass;
    };
    static void ExecuteSetStencilOp(void* object)
    {
        ArgsSetStencilOp* a = (ArgsSetStencilOp*)object;
        g_Packets->m_Backend.m_SetStencilOp(a->context, a->sfail, a->dpfail, a->dppass);
    }
    static void PacketSetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStencilOp(context, sfail, dpfail, dppass); return; }
        g_Packets->m_Pipeline.m_StencilFrontOpFail      = sfail;
        g_Packets->m_Pipeline.m_StencilFrontOpDepthFail = dpfail;
        g_Packets->m_Pipeline.m_StencilFrontOpPass      = dppass;
        g_Packets->m_Pipeline.m_StencilBackOpFail       = sfail;
        g_Packets->m_Pipeline.m_StencilBackOpDepthFail  = dpfail;
        g_Packets->m_Pipeline.m_StencilBackOpPass       = dppass;
        ArgsSetStencilOp* a = (ArgsSetStencilOp*)Record(ExecuteSetStencilOp, sizeof(ArgsSetStencilOp));
        a->context = context;
        a->sfail = sfail;
        a->dpfail = dpfail;
        a->dppass = dppass;
    }

    struct ArgsSetStencilOpSeparate
    {
        HContext context;
        FaceType face_type;
        StencilOp sfail;
        StencilOp dpfail;
        StencilOp dppass;
    };
    static void ExecuteSetStencilOpSeparate(void* object)
    {
        ArgsSetStencilOpSeparate* a = (ArgsSetStencilOpSeparate*)object;
        g_Packets->m_Backend.m_SetStencilOpSeparate(a->context, a->face_type, a->sfail, a->dpfail, a->dppass);
    }
    static void PacketSetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStencilOpSeparate(context, face_type, sfail, dpfail, dppass); return; }
        if (face_type == FACE_TYPE_BACK)
        {
            g_Packets->m_Pipeline.m_StencilBackOpFail       = sfail;
            g_Packets->m_Pipeline.m_StencilBackOpDepthFail  = dpfail;
            g_Packets->m_Pipeline.m_StencilBackOpPass       = dppass;
        }
        else
        {
            g_Packets->m_Pipeline.m_StencilFrontOpFail      = sfail;
            g_Packets->m_Pipeline.m_StencilFrontOpDepthFail = dpfail;
            g_Packets->m_Pipeline.m_StencilFrontOpPass      = dppass;
        }
        ArgsSetStencilOpSeparate* a = (ArgsSetStencilOpSeparate*)Record(ExecuteSetStencilOpSeparate, sizeof(ArgsSetStencilOpSeparate));
        a->context = context;
        a->face_type = face_type;
        a->sfail = sfail;
        a->dpfail = dpfail;
        a->dppass = dppass;
    }

    struct ArgsSetCullFace
    {
        HContext context;
        FaceType face_type;
    };
    static void ExecuteSetCullFace(void* object)
    {
        ArgsSetCullFace* a = (ArgsSetCullFace*)object;
        g_Packets->m_Backend.m_SetCullFace(a->context, a->face_type);
    }
    static void PacketSetCullFace(HContext context, FaceType face_type)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetCullFace(context, face_type); return; }
        g_Packets->m_Pipeline.m_CullFaceType = face_type;
        ArgsSetCullFace* a = (ArgsSetCullFace*)Record(ExecuteSetCullFace, sizeof(ArgsSetCullFace));
        a->context = context;
        a->face_type = face_type;
    }

    struct ArgsSetFaceWinding
    {
        HContext context;
        FaceWinding face_winding;
    };
    static void ExecuteSetFaceWinding(void* object)
    {
        ArgsSetFaceWinding* a = (ArgsSetFaceWinding*)object;
        g_Packets->m_Backend.m_SetFaceWinding(a->context, a->face_winding);
    }
    static void PacketSetFaceWinding(HContext context, FaceWinding face_winding)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetFaceWinding(context, face_winding); return; }
        g_Packets->m_Pipeline.m_FaceWinding = face_winding;
        ArgsSetFaceWinding* a = (ArgsSetFaceWinding*)Record(ExecuteSetFaceWinding, sizeof(ArgsSetFaceWinding));
        a->context = context;
        a->face_winding = face_winding;
    }

    struct ArgsSetPolygonOffset
    {
        HContext context;
        float factor;
        float units;
    };
    static void ExecuteSetPolygonOffset(void* object)
    {
        ArgsSetPolygonOffset* a = (ArgsSetPolygonOffset*)object;
        g_Packets->m_Backend.m_SetPolygonOffset(a->context, a->factor, a->units);
    }
    static void PacketSetPolygonOffset(HContext context, float factor, float units)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetPolygonOffset(context, factor, units); return; }
        ArgsSetPolygonOffset* a = (ArgsSetPolygonOffset*)Record(ExecuteSetPolygonOffset, sizeof(ArgsSetPolygonOffset));
        a->context = context;
        a->factor = factor;
        a->units = units;
    }

    static HRenderTarget PacketNewRenderTarget(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewRenderTarget(context, buffer_type_flags, params); }
        PacketError("NewRenderTarget is not admitted");
        return (HRenderTarget)0;
    }

    static void PacketDeleteRenderTarget(HContext context, HRenderTarget render_target)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteRenderTarget(context, render_target); return; }
        PacketError("DeleteRenderTarget is not admitted");
    }

    struct ArgsSetRenderTarget
    {
        HContext context;
        HRenderTarget render_target;
        RenderTargetBindingParams params;
    };
    static void ExecuteSetRenderTarget(void* object)
    {
        ArgsSetRenderTarget* a = (ArgsSetRenderTarget*)object;
        g_Packets->m_Backend.m_SetRenderTarget(a->context, a->render_target, a->params);
    }
    static void PacketSetRenderTarget(HContext context, HRenderTarget render_target, const RenderTargetBindingParams& params)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetRenderTarget(context, render_target, params); return; }
        if (render_target) PacketError("custom render targets are not admitted");
        ArgsSetRenderTarget* a = (ArgsSetRenderTarget*)Record(ExecuteSetRenderTarget, sizeof(ArgsSetRenderTarget));
        a->context = context;
        a->render_target = render_target;
        a->params = params;
    }

    static void PacketSetRenderTargetSize(HContext context, HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetRenderTargetSize(context, render_target, width, height); return; }
        PacketError("SetRenderTargetSize is not admitted");
    }

    struct ArgsNewTexture
    {
        HContext context;
        TextureCreationParams params;
        HTexture result;
    };
    static void ExecuteNewTexture(void* object)
    {
        ArgsNewTexture* a = (ArgsNewTexture*)object;
        a->result = g_Packets->m_Backend.m_NewTexture(a->context, a->params);
    }
    static HTexture PacketNewTexture(HContext context, const TextureCreationParams& params)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewTexture(context, params); }
        ArgsNewTexture a;
        a.context = context;
        a.params = params;
        Synchronous(ExecuteNewTexture, &a);
        return a.result;
    }

    struct ArgsDeleteTexture
    {
        HContext context;
        HTexture t;
    };
    static void ExecuteDeleteTexture(void* object)
    {
        ArgsDeleteTexture* a = (ArgsDeleteTexture*)object;
        g_Packets->m_Backend.m_DeleteTexture(a->context, a->t);
    }
    static void PacketDeleteTexture(HContext context, HTexture t)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteTexture(context, t); return; }
        ArgsDeleteTexture a;
        a.context = context;
        a.t = t;
        Synchronous(ExecuteDeleteTexture, &a);
    }

    struct ArgsSetTexture
    {
        HContext context;
        HTexture texture;
        TextureParams params;
    };
    static void ExecuteSetTexture(void* object)
    {
        ArgsSetTexture* a = (ArgsSetTexture*)object;
        g_Packets->m_Backend.m_SetTexture(a->context, a->texture, a->params);
    }
    static void PacketSetTexture(HContext context, HTexture texture, const TextureParams& params)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetTexture(context, texture, params); return; }
        ArgsSetTexture a;
        a.context = context;
        a.texture = texture;
        a.params = params;
        Synchronous(ExecuteSetTexture, &a);
    }

    static void PacketSetTextureAsync(HContext context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetTextureAsync(context, texture, params, callback, user_data); return; }
        PacketSetTexture(context, texture, params);
        if (callback) callback(texture, user_data); // Explicit synchronous PoC completion on producer.
    }

    struct ArgsSetTextureParams
    {
        HContext context;
        HTexture texture;
        TextureFilter minfilter;
        TextureFilter magfilter;
        TextureWrap uwrap;
        TextureWrap vwrap;
        TextureWrap wwrap;
        float max_anisotropy;
    };
    static void ExecuteSetTextureParams(void* object)
    {
        ArgsSetTextureParams* a = (ArgsSetTextureParams*)object;
        g_Packets->m_Backend.m_SetTextureParams(a->context, a->texture, a->minfilter, a->magfilter, a->uwrap, a->vwrap, a->wwrap, a->max_anisotropy);
    }
    static void PacketSetTextureParams(HContext context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, TextureWrap wwrap, float max_anisotropy)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetTextureParams(context, texture, minfilter, magfilter, uwrap, vwrap, wwrap, max_anisotropy); return; }
        ArgsSetTextureParams* a = (ArgsSetTextureParams*)Record(ExecuteSetTextureParams, sizeof(ArgsSetTextureParams));
        a->context = context;
        a->texture = texture;
        a->minfilter = minfilter;
        a->magfilter = magfilter;
        a->uwrap = uwrap;
        a->vwrap = vwrap;
        a->wwrap = wwrap;
        a->max_anisotropy = max_anisotropy;
    }

    struct ArgsEnableTexture
    {
        HContext context;
        uint32_t unit;
        uint8_t id_index;
        HTexture texture;
    };
    static void ExecuteEnableTexture(void* object)
    {
        ArgsEnableTexture* a = (ArgsEnableTexture*)object;
        g_Packets->m_Backend.m_EnableTexture(a->context, a->unit, a->id_index, a->texture);
    }
    static void PacketEnableTexture(HContext context, uint32_t unit, uint8_t id_index, HTexture texture)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableTexture(context, unit, id_index, texture); return; }
        ArgsEnableTexture* a = (ArgsEnableTexture*)Record(ExecuteEnableTexture, sizeof(ArgsEnableTexture));
        a->context = context;
        a->unit = unit;
        a->id_index = id_index;
        a->texture = texture;
    }

    struct ArgsDisableTexture
    {
        HContext context;
        uint32_t unit;
        HTexture texture;
    };
    static void ExecuteDisableTexture(void* object)
    {
        ArgsDisableTexture* a = (ArgsDisableTexture*)object;
        g_Packets->m_Backend.m_DisableTexture(a->context, a->unit, a->texture);
    }
    static void PacketDisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableTexture(context, unit, texture); return; }
        ArgsDisableTexture* a = (ArgsDisableTexture*)Record(ExecuteDisableTexture, sizeof(ArgsDisableTexture));
        a->context = context;
        a->unit = unit;
        a->texture = texture;
    }

    static uint32_t PacketGetMaxTextureSize(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetMaxTextureSize(context); }
        return g_Packets->m_Backend.m_GetMaxTextureSize(context);
    }

    struct ArgsReadPixels
    {
        HContext context;
        int32_t x;
        int32_t y;
        uint32_t width;
        uint32_t height;
        void* buffer;
        uint32_t buffer_size;
    };
    static void ExecuteReadPixels(void* object)
    {
        ArgsReadPixels* a = (ArgsReadPixels*)object;
        g_Packets->m_Backend.m_ReadPixels(a->context, a->x, a->y, a->width, a->height, a->buffer, a->buffer_size);
    }
    static void PacketReadPixels(HContext context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_ReadPixels(context, x, y, width, height, buffer, buffer_size); return; }
        ArgsReadPixels a;
        a.context = context;
        a.x = x;
        a.y = y;
        a.width = width;
        a.height = height;
        a.buffer = buffer;
        a.buffer_size = buffer_size;
        Synchronous(ExecuteReadPixels, &a);
    }

    static void PacketRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_RunApplicationLoop(user_data, step_method, is_running); return; }
        PacketError("RunApplicationLoop is not admitted");
    }

    static HandleResult PacketGetTextureHandle(HTexture texture, void** out_handle)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetTextureHandle(texture, out_handle); }
        PacketError("GetTextureHandle is not admitted");
        return (HandleResult)0;
    }

    static bool PacketIsExtensionSupported(HContext context, const char* extension)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_IsExtensionSupported(context, extension); }
        return g_Packets->m_Backend.m_IsExtensionSupported(context, extension);
    }

    static uint32_t PacketGetNumSupportedExtensions(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetNumSupportedExtensions(context); }
        return g_Packets->m_Backend.m_GetNumSupportedExtensions(context);
    }

    static const char* PacketGetSupportedExtension(HContext context, uint32_t index)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetSupportedExtension(context, index); }
        return g_Packets->m_Backend.m_GetSupportedExtension(context, index);
    }

    static PipelineState PacketGetPipelineState(HContext context)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetPipelineState(context); }
        return g_Packets->m_Pipeline;
    }

    static void PacketInvalidateGraphicsHandles(HContext context)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_InvalidateGraphicsHandles(context); return; }
        PacketError("InvalidateGraphicsHandles is not admitted");
    }

    static void PacketGetViewport(HContext context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_GetViewport(context, x, y, width, height); return; }
        *x = g_Packets->m_ViewportX; *y = g_Packets->m_ViewportY; *width = g_Packets->m_ViewportWidth; *height = g_Packets->m_ViewportHeight;
    }

    static HUniformBuffer PacketNewUniformBuffer(HContext context, UniformBufferLayout layout, uint32_t size)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewUniformBuffer(context, layout, size); }
        PacketError("NewUniformBuffer is not admitted");
        return (HUniformBuffer)0;
    }

    static void PacketDeleteUniformBuffer(HContext context, HUniformBuffer uniform_buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteUniformBuffer(context, uniform_buffer); return; }
        PacketError("DeleteUniformBuffer is not admitted");
    }

    static void PacketSetUniformBuffer(HContext context, HUniformBuffer uniform_buffer, uint32_t offset, uint32_t size, const void* data)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetUniformBuffer(context, uniform_buffer, offset, size, data); return; }
        PacketError("SetUniformBuffer is not admitted");
    }

    static void PacketEnableUniformBuffer(HContext context, HUniformBuffer uniform_buffer, uint32_t binding, uint32_t set)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableUniformBuffer(context, uniform_buffer, binding, set); return; }
        PacketError("EnableUniformBuffer is not admitted");
    }

    static void PacketDisableUniformBuffer(HContext context, HUniformBuffer uniform_buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableUniformBuffer(context, uniform_buffer); return; }
        PacketError("DisableUniformBuffer is not admitted");
    }

    static HStorageBuffer PacketNewStorageBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_NewStorageBuffer(context, size, data, buffer_usage); }
        PacketError("NewStorageBuffer is not admitted");
        return (HStorageBuffer)0;
    }

    static void PacketDeleteStorageBuffer(HContext context, HStorageBuffer storage_buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DeleteStorageBuffer(context, storage_buffer); return; }
        PacketError("DeleteStorageBuffer is not admitted");
    }

    static void PacketSetStorageBufferData(HContext context, HStorageBuffer storage_buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStorageBufferData(context, storage_buffer, size, data, buffer_usage); return; }
        PacketError("SetStorageBufferData is not admitted");
    }

    static void PacketSetStorageBufferSubData(HContext context, HStorageBuffer storage_buffer, uint32_t offset, uint32_t size, const void* data)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetStorageBufferSubData(context, storage_buffer, offset, size, data); return; }
        PacketError("SetStorageBufferSubData is not admitted");
    }

    static uint32_t PacketGetStorageBufferSize(HContext context, HStorageBuffer storage_buffer)
    {
        if (!ProducerCall()) { return g_Packets->m_Backend.m_GetStorageBufferSize(context, storage_buffer); }
        PacketError("GetStorageBufferSize is not admitted");
        return (uint32_t)0;
    }

    static void PacketEnableStorageBuffer(HContext context, HStorageBuffer storage_buffer, uint32_t binding, uint32_t set)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_EnableStorageBuffer(context, storage_buffer, binding, set); return; }
        PacketError("EnableStorageBuffer is not admitted");
    }

    static void PacketDisableStorageBuffer(HContext context, HStorageBuffer storage_buffer)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_DisableStorageBuffer(context, storage_buffer); return; }
        PacketError("DisableStorageBuffer is not admitted");
    }

    static void PacketSetSwapInterval(HContext context, uint32_t swap_interval)
    {
        if (!ProducerCall()) { g_Packets->m_Backend.m_SetSwapInterval(context, swap_interval); return; }
        FlushGraphicsPackets();
        g_Packets->m_Backend.m_SetSwapInterval(context, swap_interval); return;
    }

    bool InstallGraphicsPackets(GraphicsAdapterFunctionTable* table, HContext context, uint32_t mode, uint32_t delay_us, ExternalGraphicsDispatch external)
    {
        if (g_Packets || mode < 1 || mode > 2) return false;
        PacketService* s = new PacketService;
        s->m_Backend = *table;
        s->m_Frontend = table;
        s->m_Context = context;
        s->m_Stats.m_Mode = mode;
        s->m_External = external;
        s->m_RenderLayer = external != 0;
        s->m_DelayUs = delay_us;
        s->m_Producer = s->m_Owner = dmThread::GetCurrentThreadId();
        s->m_Mutex = dmMutex::New();
        s->m_Changed = dmConditionVariable::New();
        s->m_Completions.SetCapacity(256);
        s->m_Pipeline = table->m_GetPipelineState(context);
        table->m_GetViewport(context, &s->m_ViewportX, &s->m_ViewportY, &s->m_ViewportWidth, &s->m_ViewportHeight);
        g_Packets = s;
        table->m_NewContext = PacketNewContext;
        table->m_DeleteContext = PacketDeleteContext;
        table->m_Finalize = PacketFinalize;
        table->m_CloseWindow = PacketCloseWindow;
        table->m_GetWindow = PacketGetWindow;
        table->m_GetDisplayDpi = PacketGetDisplayDpi;
        table->m_SetWindowSize = PacketSetWindowSize;
        table->m_ResizeWindow = PacketResizeWindow;
        table->m_BeginFrame = PacketBeginFrame;
        table->m_Flip = PacketFlip;
        table->m_Clear = PacketClear;
        table->m_NewVertexBuffer = PacketNewVertexBuffer;
        table->m_DeleteVertexBuffer = PacketDeleteVertexBuffer;
        table->m_SetVertexBufferData = PacketSetVertexBufferData;
        table->m_SetVertexBufferSubData = PacketSetVertexBufferSubData;
        table->m_GetMaxElementsVertices = PacketGetMaxElementsVertices;
        table->m_NewIndexBuffer = PacketNewIndexBuffer;
        table->m_DeleteIndexBuffer = PacketDeleteIndexBuffer;
        table->m_SetIndexBufferData = PacketSetIndexBufferData;
        table->m_SetIndexBufferSubData = PacketSetIndexBufferSubData;
        table->m_IsIndexBufferFormatSupported = PacketIsIndexBufferFormatSupported;
        table->m_GetMaxElementsIndices = PacketGetMaxElementsIndices;
        table->m_NewVertexDeclaration = PacketNewVertexDeclaration;
        table->m_NewVertexDeclarationStride = PacketNewVertexDeclarationStride;
        table->m_EnableVertexDeclaration = PacketEnableVertexDeclaration;
        table->m_DisableVertexDeclaration = PacketDisableVertexDeclaration;
        table->m_EnableVertexBuffer = PacketEnableVertexBuffer;
        table->m_DisableVertexBuffer = PacketDisableVertexBuffer;
        table->m_DrawElements = PacketDrawElements;
        table->m_Draw = PacketDraw;
        table->m_DispatchCompute = PacketDispatchCompute;
        table->m_NewProgram = PacketNewProgram;
        table->m_DeleteProgram = PacketDeleteProgram;
        table->m_GetProgramLanguage = PacketGetProgramLanguage;
        table->m_IsShaderLanguageSupported = PacketIsShaderLanguageSupported;
        table->m_EnableProgram = PacketEnableProgram;
        table->m_DisableProgram = PacketDisableProgram;
        table->m_ReloadProgram = PacketReloadProgram;
        table->m_GetAttributeCount = PacketGetAttributeCount;
        table->m_GetAttribute = PacketGetAttribute;
        table->m_SetConstantV4 = PacketSetConstantV4;
        table->m_SetConstantM4 = PacketSetConstantM4;
        table->m_SetSampler = PacketSetSampler;
        table->m_SetViewport = PacketSetViewport;
        table->m_EnableState = PacketEnableState;
        table->m_DisableState = PacketDisableState;
        table->m_SetBlendFunc = PacketSetBlendFunc;
        table->m_SetBlendFuncSeparate = PacketSetBlendFuncSeparate;
        table->m_SetBlendEquationSeparate = PacketSetBlendEquationSeparate;
        table->m_SetColorMask = PacketSetColorMask;
        table->m_SetDepthMask = PacketSetDepthMask;
        table->m_SetDepthFunc = PacketSetDepthFunc;
        table->m_SetScissor = PacketSetScissor;
        table->m_SetStencilMask = PacketSetStencilMask;
        table->m_SetStencilFunc = PacketSetStencilFunc;
        table->m_SetStencilFuncSeparate = PacketSetStencilFuncSeparate;
        table->m_SetStencilOp = PacketSetStencilOp;
        table->m_SetStencilOpSeparate = PacketSetStencilOpSeparate;
        table->m_SetCullFace = PacketSetCullFace;
        table->m_SetFaceWinding = PacketSetFaceWinding;
        table->m_SetPolygonOffset = PacketSetPolygonOffset;
        table->m_NewRenderTarget = PacketNewRenderTarget;
        table->m_DeleteRenderTarget = PacketDeleteRenderTarget;
        table->m_SetRenderTarget = PacketSetRenderTarget;
        table->m_SetRenderTargetSize = PacketSetRenderTargetSize;
        table->m_NewTexture = PacketNewTexture;
        table->m_DeleteTexture = PacketDeleteTexture;
        table->m_SetTexture = PacketSetTexture;
        table->m_SetTextureAsync = PacketSetTextureAsync;
        table->m_SetTextureParams = PacketSetTextureParams;
        table->m_EnableTexture = PacketEnableTexture;
        table->m_DisableTexture = PacketDisableTexture;
        table->m_GetMaxTextureSize = PacketGetMaxTextureSize;
        table->m_ReadPixels = PacketReadPixels;
        table->m_RunApplicationLoop = PacketRunApplicationLoop;
        table->m_GetTextureHandle = PacketGetTextureHandle;
        table->m_IsExtensionSupported = PacketIsExtensionSupported;
        table->m_GetNumSupportedExtensions = PacketGetNumSupportedExtensions;
        table->m_GetSupportedExtension = PacketGetSupportedExtension;
        table->m_GetPipelineState = PacketGetPipelineState;
        table->m_InvalidateGraphicsHandles = PacketInvalidateGraphicsHandles;
        table->m_GetViewport = PacketGetViewport;
        table->m_NewUniformBuffer = PacketNewUniformBuffer;
        table->m_DeleteUniformBuffer = PacketDeleteUniformBuffer;
        table->m_SetUniformBuffer = PacketSetUniformBuffer;
        table->m_EnableUniformBuffer = PacketEnableUniformBuffer;
        table->m_DisableUniformBuffer = PacketDisableUniformBuffer;
        table->m_NewStorageBuffer = PacketNewStorageBuffer;
        table->m_DeleteStorageBuffer = PacketDeleteStorageBuffer;
        table->m_SetStorageBufferData = PacketSetStorageBufferData;
        table->m_SetStorageBufferSubData = PacketSetStorageBufferSubData;
        table->m_GetStorageBufferSize = PacketGetStorageBufferSize;
        table->m_EnableStorageBuffer = PacketEnableStorageBuffer;
        table->m_DisableStorageBuffer = PacketDisableStorageBuffer;
        if (table->m_SetSwapInterval) table->m_SetSwapInterval = PacketSetSwapInterval;
        if (mode == 2 && !external)
        {
            s->m_Thread = dmThread::New(PacketWorker, 0x80000, s, "graphics-packet");
            dmMutex::Lock(s->m_Mutex);
            while (!s->m_Ready) dmConditionVariable::Wait(s->m_Changed, s->m_Mutex);
            dmMutex::Unlock(s->m_Mutex);
        }
        return true;
    }
}
