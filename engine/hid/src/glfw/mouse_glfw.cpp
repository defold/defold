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
    bool MouseInitialize(HContext context)
    {
        (void) context;
        return true;
    }

    void MouseFinalize(HContext context)
    {
        (void) context;
    }

    void MouseUpdate(HContext context)
    {
        for (uint32_t m = 0; m < MAX_MOUSE_COUNT; ++m)
        {
            Mouse* mouse = &context->m_Mice[m];
            // TODO: Actually detect if the mouse is present,
            // this is important for mouse input and touch input to not interfere
            mouse->m_Connected = 1;

            MousePacket& packet = mouse->m_Packet;
            for (uint32_t i = 0; i < MAX_MOUSE_BUTTON_COUNT; ++i)
            {
                uint32_t mask = 1;
                mask <<= i % 32;

                int button_value = GetMouseButtonValue((MouseButton) i);
                int state        = dmPlatform::GetMouseButton(context->m_Window, button_value);

                if (state)
                    packet.m_Buttons[i / 32] |= mask;
                else
                    packet.m_Buttons[i / 32] &= ~mask;
            }
            int32_t wheel = dmPlatform::GetMouseWheel(context->m_Window);

            if (context->m_FlipScrollDirection)
            {
                wheel *= -1;
            }

            packet.m_Wheel = wheel;

            dmPlatform::GetMousePosition(context->m_Window, &packet.m_PositionX, &packet.m_PositionY);
        }
    }
}
