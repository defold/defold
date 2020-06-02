// Copyright 2020 The Defold Foundation
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

#include "hid.h"

#include <string.h>

#include <dlib/hashtable.h>

#include "hid_private.h"

namespace dmHID
{
    // detect sloppy init/final usage
    dmHashTable<uintptr_t, char*>* g_DummyData = 0x0;

    bool Init(HContext context)
    {
        if (g_DummyData == 0x0)
        {
            g_DummyData = new dmHashTable<uintptr_t, char*>();
            g_DummyData->SetCapacity(32, 24);
        }
        if (context != 0x0)
        {
            context->m_KeyboardConnected = 0;
            context->m_MouseConnected = 0;
            for (uint32_t i = 0; i < MAX_GAMEPAD_COUNT; ++i)
            {
                Gamepad& gamepad = context->m_Gamepads[i];
                gamepad.m_Index = i;
                gamepad.m_Connected = 0;
                gamepad.m_AxisCount = 0;
                gamepad.m_ButtonCount = 0;
                memset(&gamepad.m_Packet, 0, sizeof(GamepadPacket));
            }
            context->m_TouchDeviceConnected = 0;

            g_DummyData->Put((uintptr_t)context, new char);
            return true;
        }
        return false;
    }

    void Final(HContext context)
    {
        char** dummy_data = g_DummyData->Get((uintptr_t)context);
        delete *dummy_data;
        g_DummyData->Erase((uintptr_t)context);
        if (g_DummyData->Empty())
        {
            delete g_DummyData;
            g_DummyData = 0;
        }
    }

    void Update(HContext context)
    {
        context->m_KeyboardConnected = !context->m_IgnoreKeyboard;
        context->m_MouseConnected = !context->m_IgnoreMouse;
        context->m_Gamepads[0].m_Connected = !context->m_IgnoreGamepads;
        context->m_Gamepads[0].m_ButtonCount = MAX_GAMEPAD_BUTTON_COUNT;
        context->m_Gamepads[0].m_AxisCount = MAX_GAMEPAD_AXIS_COUNT;
        context->m_TouchDeviceConnected = !context->m_IgnoreTouchDevice;
    }

    void GetGamepadDeviceName(HGamepad gamepad, const char** device_name)
    {
        *device_name = "null_device";
    }

    void ShowKeyboard(HContext context, KeyboardType type, bool autoclose)
    {
    }

    void HideKeyboard(HContext context)
    {
    }

    void ResetKeyboard(HContext context)
    {
    }

    void EnableAccelerometer()
    {
    }
}
