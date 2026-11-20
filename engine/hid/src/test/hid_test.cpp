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

#include "hid_test.hpp"
#include "../hid_private.h"
#include <assert.h>

namespace dmHID
{
    void TestSetDeviceIgnored(HContext context, InputSource source, bool ignored)
    {
        switch (source)
        {
            case INPUT_SOURCE_KEYBOARD:
            {
                context->m_IgnoreKeyboard = ignored;
                break;
            }
            case INPUT_SOURCE_MOUSE:
            {
                context->m_IgnoreMouse = ignored;
                break;
            }
            case INPUT_SOURCE_TOUCH:
            {
                context->m_IgnoreTouchDevice = ignored;
                break;
            }
            case INPUT_SOURCE_GAMEPAD:
            {
                context->m_IgnoreGamepads = ignored;
                break;
            }
            case INPUT_SOURCE_ACCELEROMETER:
            {
                context->m_IgnoreAcceleration = ignored;
                break;
            }
            default:
            {
                assert(false);
                break;
            }
        }
    }

    void TestSetMouseLeftButtonFromTouch(HMouse mouse, bool from_touch)
    {
        mouse->m_Packet.m_LeftButtonFromTouch = from_touch;
    }

    void TestSetMousePositionFromTouch(HMouse mouse, bool from_touch)
    {
        mouse->m_Packet.m_PositionFromTouch = from_touch;
    }

    void TestSetGamepadConnected(HGamepad gamepad, bool connected)
    {
        gamepad->m_Connected = connected;
    }

    void TestSetAccelerometerConnected(HContext context, bool connected)
    {
        context->m_AccelerometerConnected = connected;
    }

    void TestSetAcceleration(HContext context, float x, float y, float z)
    {
        context->m_AccelerationPacket.m_X = x;
        context->m_AccelerationPacket.m_Y = y;
        context->m_AccelerationPacket.m_Z = z;
    }
}
