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

#include <string.h>

#include <dlib/hashtable.h>
#include <dlib/dstrings.h>

#include <platform/platform_window.h>

#include "hid_private.h"
#include "hid.h"

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
            memset(context->m_Gamepads, 0, sizeof(Gamepad) * MAX_GAMEPAD_COUNT);
            g_DummyData->Put((uintptr_t)context, new char);
            return true;
        }
        return false;
    }

    void Final(HContext context)
    {
        if (g_DummyData)
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
    }

    bool Update(HContext context)
    {
        dmPlatform::PollEvents(context->m_Window);
        context->m_Keyboards[0].m_Connected = !context->m_IgnoreKeyboard;
        context->m_Mice[0].m_Connected = !context->m_IgnoreMouse;
        context->m_TouchDevices[0].m_Connected = !context->m_IgnoreTouchDevice;
        context->m_Gamepads[0].m_Connected = !context->m_IgnoreGamepads;
        context->m_Gamepads[0].m_ButtonCount = MAX_GAMEPAD_BUTTON_COUNT;
        context->m_Gamepads[0].m_AxisCount = MAX_GAMEPAD_AXIS_COUNT;

        dmhash_t prev_state_hash = context->m_StateHash;
        context->m_StateHash = CalcStateHash(context);
        return prev_state_hash != context->m_StateHash;
    }

    void GetGamepadDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        dmStrlCpy(name, "null_device", MAX_GAMEPAD_NAME_LENGTH);
    }

    // platform implementations
    bool GetPlatformGamepadUserId(HContext context, HGamepad gamepad, uint32_t* out)
    {
        return false;
    }

    void ResetKeyboard(HContext context)
    {
    }
}
