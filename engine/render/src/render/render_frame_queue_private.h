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

#ifndef DM_RENDER_FRAME_QUEUE_PRIVATE_H
#define DM_RENDER_FRAME_QUEUE_PRIVATE_H

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include "render_frame_queue.h"

// Implementation/test layout only. Consumers use the opaque queue API.
namespace dmRender
{
    // Fixed slot metadata protected by the queue mutex in threaded mode.
    struct RenderFrameSlot
    {
        HRenderFrame     m_Frame;
        RenderFrameId    m_FrameId;
        RenderFrameState m_State;
    };

    // One accepted Ready/Reading frame and one game-owned Building frame. No queue of
    // replaceable snapshots: accepted frames may carry side effects and must be consumed.
    struct RenderFrameQueueContext
    {
        RenderFrameQueueParams                  m_Params;
        RenderFrameSlot                         m_Slots[2];
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_Changed;
        RenderFrameId                           m_NextFrameId;
        uint64_t                                m_Revision;
        uint64_t                                m_Published;
        uint64_t                                m_Consumed;
        uint64_t                                m_Cancelled;
        uint64_t                                m_Retired;
        int32_t                                 m_Building;
        int32_t                                 m_Active;
        // Render-owner-only acknowledgement, kept until TryLock succeeds on a later pump.
        int32_t                                 m_Completed;
        // Cancellation does not restore an update credit while the previous reader is active.
        bool                                    m_LeadReserved;
        bool                                    m_Stopping;
        bool                                    m_ServicePending;
        bool                                    m_InPump;
        bool                                    m_InRetire;
    };

}
#endif // DM_RENDER_FRAME_QUEUE_PRIVATE_H
