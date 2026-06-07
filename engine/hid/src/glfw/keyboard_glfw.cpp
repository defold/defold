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

#include <dlib/log.h>

#include "hid.h"
#include "hid_private.h"

namespace dmHID
{
    static void GLFWAddKeyboardChar(void* ctx, int chr)
    {
        AddKeyboardChar((HContext) ctx, chr);
    }

    static void GLFWSetMarkedText(void* ctx, char* text)
    {
        SetMarkedText((HContext) ctx, text);
    }

#if !defined(__APPLE__)
    static void GLFWDeviceChangedCallback(void* ctx, int status)
    {
        (void) status;
        GamepadDetectDevices((HContext) ctx);
    }
#endif

    bool KeyboardInitialize(HContext context)
    {
        if (context == 0x0)
        {
            return false;
        }

        if (!context->m_Window)
        {
            dmLogFatal("No window has been created.");
            return false;
        }

        dmPlatform::SetKeyboardCharCallback(context->m_Window, GLFWAddKeyboardChar, (void*) context);
        dmPlatform::SetKeyboardMarkedTextCallback(context->m_Window, GLFWSetMarkedText, (void*) context);
#if !defined(__APPLE__)
        dmPlatform::SetKeyboardDeviceChangedCallback(context->m_Window, GLFWDeviceChangedCallback, (void*) context);
#endif

        return true;
    }

    void KeyboardFinalize(HContext context)
    {
        (void) context;
    }

    void KeyboardUpdate(HContext context)
    {
        for (uint32_t k = 0; k < MAX_KEYBOARD_COUNT; ++k)
        {
            Keyboard* keyboard = &context->m_Keyboards[k];
            keyboard->m_Connected = 1; // TODO: Actually detect if the keyboard is present

            for (uint32_t i = 0; i < MAX_KEY_COUNT; ++i)
            {
                Key key        = (Key) i;
                int key_value  = GetKeyValue(key);
                int state      = dmPlatform::GetKey(context->m_Window, key_value);
                uint32_t mask  = 1 << (i % 32);

                if (state)
                    keyboard->m_Packet.m_Keys[i / 32] |= mask;
                else
                    keyboard->m_Packet.m_Keys[i / 32] &= ~mask;
            }
        }
    }

    void KeyboardReset(HContext context)
    {
        dmPlatform::SetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_KEYBOARD_RESET, true);
    }
}
