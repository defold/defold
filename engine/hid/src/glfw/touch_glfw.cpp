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

#include <platform/window.hpp>

#include "hid.h"
#include "hid_private.h"

namespace dmHID
{
    bool TouchInitialize(HContext context)
    {
        (void) context;
        return true;
    }

    void TouchFinalize(HContext context)
    {
        (void) context;
    }

    void TouchUpdate(HContext context)
    {
        for (uint32_t t = 0; t < MAX_TOUCH_DEVICE_COUNT; ++t)
        {
            TouchDevice* device = &context->m_TouchDevices[t];
            TouchDevicePacket* packet = &device->m_Packet;

            WindowTouchData touch_data[dmHID::MAX_TOUCH_COUNT] = {};
            packet->m_TouchCount = dmPlatform::GetTouchData(context->m_Window, touch_data, dmHID::MAX_TOUCH_COUNT);

            if (packet->m_TouchCount > 0)
            {
                device->m_Connected = 1;
                for (uint32_t i = 0; i < packet->m_TouchCount; ++i)
                {
                    packet->m_Touches[i].m_TapCount = touch_data[i].m_TapCount;
                    packet->m_Touches[i].m_Id       = touch_data[i].m_Id;
                    packet->m_Touches[i].m_Phase    = (dmHID::Phase) touch_data[i].m_Phase;
                    packet->m_Touches[i].m_X        = touch_data[i].m_X;
                    packet->m_Touches[i].m_Y        = touch_data[i].m_Y;
                    packet->m_Touches[i].m_DX       = touch_data[i].m_DX;
                    packet->m_Touches[i].m_DY       = touch_data[i].m_DY;
                }
            }
        }
    }
}
