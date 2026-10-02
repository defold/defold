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

#ifndef DM_GRAPHICS_PACKET_H
#define DM_GRAPHICS_PACKET_H
#include "graphics.h"
namespace dmGraphics
{
    // Internal PoC: 0 = direct, 1 = recorded inline, 2 = recorded worker.
    struct GraphicsPacketStats
    {
        uint64_t m_Submitted, m_Completed, m_Commands, m_CopiedBytes;
        uint64_t m_WaitUs, m_ExecuteUs, m_SynchronousCalls, m_OverlapFrames;
        uint64_t m_CapacityBytes, m_GrowthPeakBytes, m_LastFrameBytes, m_MaxFrameBytes;
        uint32_t m_Mode, m_MaxOutstanding, m_StackReservedBytes, m_BufferMetadataBytes;
    };
    typedef void (*GraphicsOwnerTask)(void* data);
    typedef void (*GraphicsOwnerCompletion)(void* producer_context);
    // Render-layer mode reuses only the owner/resource service. Draws execute
    // directly on that owner while the producer records an immutable RenderFrame.
    typedef void (*ExternalGraphicsDispatch)(GraphicsOwnerTask execute, void* data);
    // Install on browser main after bootstrap. Attach the single producer before
    // stepping; detach after its last request completes, before main-side teardown.
    bool StartExternalGraphicsOwner(HContext context, ExternalGraphicsDispatch dispatch);
    void AttachExternalGraphicsProducer();
    void DetachExternalGraphicsProducer();
    bool IsExternalGraphicsProducer();
    bool DispatchExternalGraphics(GraphicsOwnerTask execute, void* data);
    void UpdateExternalGraphicsWindow(HContext context);
    bool GetExternalGraphicsWindow(uint32_t* width, uint32_t* height, uint32_t* iconified, float* scale);
    bool StartRenderGraphicsOwner(HContext context, bool threaded);
    bool SubmitGraphicsOwnerFrame(GraphicsOwnerTask consume, void* owned_frame);
    bool QueueGraphicsOwnerRequest(GraphicsOwnerTask execute, const void* data, uint32_t size,
                                    GraphicsOwnerCompletion complete, void* producer_context);
    void RunGraphicsOwnerControl(GraphicsOwnerTask execute, void* data);
    bool IsRenderGraphicsOwnerActive();
    bool StartGraphicsPackets(HContext context, uint32_t mode, uint32_t delay_us = 0);
    void StopGraphicsPackets();
    bool SetGraphicsPacketsPaused(bool paused);
    bool AreGraphicsPacketsPaused();
    void FlushGraphicsPackets(); // CPU readers only; also services partial frames.
    void PrepareGraphicsPacketSurface(); // Main-thread platform work after CPU drain.
    void GetGraphicsPacketStats(GraphicsPacketStats* stats);
    bool GetGraphicsPacketBufferSize(uintptr_t buffer, uint32_t* size);
    void ForgetGraphicsPacketBuffer(uintptr_t buffer);
    bool IsGraphicsPacketOwner();
    // Adapter-table installation is internal, also used by the null tests.
    struct GraphicsAdapterFunctionTable;
    bool InstallGraphicsPackets(GraphicsAdapterFunctionTable* table, HContext context, uint32_t mode, uint32_t delay_us, ExternalGraphicsDispatch external = 0);
}
#endif
