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

#include "joystick_driver.h"

namespace dmPlatform
{
    void Win32OpenJoystickDrivers(HINSTANCE instance, Win32JoystickDrivers* drivers)
    {
        static const wchar_t* libraries[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
        for (unsigned i = 0; i < sizeof(libraries) / sizeof(libraries[0]); ++i)
        {
            HMODULE dll = LoadLibraryExW(libraries[i], 0, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!dll)
            {
                continue;
            }

            drivers->m_State = (DWORD(WINAPI*)(DWORD, XINPUT_STATE*))::GetProcAddress(dll, "XInputGetState");
            drivers->m_Capabilities = (DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*))::GetProcAddress(dll, "XInputGetCapabilities");
            if (drivers->m_State && drivers->m_Capabilities)
            {
                drivers->m_Extended = (DWORD(WINAPI*)(DWORD, DWORD, DWORD, Win32XInputCapabilities*))::GetProcAddress(dll, MAKEINTRESOURCEA(108));
                drivers->m_XInput = dll;
                break;
            }

            drivers->m_State = 0;
            FreeLibrary(dll);
        }

        DirectInput8Create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&drivers->m_Direct, 0);
    }

}
