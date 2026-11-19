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

#include "window_win32.h"
#include "joystick_packet.h"

#include <dbt.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>

// The Windows SDK hides these runtime capability values at the engine's minimum WINVER.
#ifndef XINPUT_DEVSUBTYPE_WHEEL
#define XINPUT_DEVSUBTYPE_WHEEL 0x02
#define XINPUT_DEVSUBTYPE_ARCADE_STICK 0x03
#define XINPUT_DEVSUBTYPE_FLIGHT_STICK 0x04
#define XINPUT_DEVSUBTYPE_DANCE_PAD 0x05
#define XINPUT_DEVSUBTYPE_GUITAR 0x06
#define XINPUT_DEVSUBTYPE_DRUM_KIT 0x08
#define XINPUT_CAPS_WIRELESS 0x0002
#endif

namespace dmPlatform
{
    struct Joystick
    {
        IDirectInputDevice8W* m_Direct;
        GUID                  m_Instance;
        DWORD                 m_User;
        Win32DirectControls   m_Controls;
        Win32JoystickPacket   m_Packet;
        char                  m_Name[MAX_PATH * 3 + 1];
        char                  m_Guid[33];
        bool                  m_Connected;
    };

    struct ExtendedCapabilities
    {
        XINPUT_CAPABILITIES m_Capabilities;
        WORD                m_Vendor, m_Product, m_Version, m_Reserved;
        DWORD               m_Reserved2;
    };

    static struct
    {
        Joystick        m_Devices[16];
        IDirectInput8W* m_Direct;
        HWND            m_Listener;
        HDEVNOTIFY      m_Notification;
        HMODULE         m_XInput;
        DWORD(WINAPI * m_State)(DWORD, XINPUT_STATE*);
        DWORD(WINAPI * m_Capabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
        DWORD(WINAPI * m_Extended)(DWORD, DWORD, DWORD, ExtendedCapabilities*);
        bool m_Initialized, m_Rescan;
    } g_Joysticks;

    static int FreeIndex()
    {
        for (int i = 0; i < 16; ++i)
        {
            if (!g_Joysticks.m_Devices[i].m_Connected)
            {
                return i;
            }
        }

        return -1;
    }

    static void Disconnect(int slot, bool notify)
    {
        Joystick& device = g_Joysticks.m_Devices[slot];
        if (device.m_Direct)
        {
            device.m_Direct->Unacquire();
            device.m_Direct->Release();
        }

        device = Joystick();
        if (notify)
        {
            Win32NotifyGamepad(slot, WINDOW_GAMEPAD_EVENT_DISCONNECTED);
        }
    }

    static void HardwareGuid(char* guid, WORD vendor, WORD product)
    {
        dmSnPrintf(guid, 33, "03000000%02x%02x0000%02x%02x000000000000",
                   vendor & 255, vendor >> 8, product & 255, product >> 8);
    }

    static const char* XInputName(const XINPUT_CAPABILITIES& caps)
    {
        switch (caps.SubType)
        {
            case XINPUT_DEVSUBTYPE_GAMEPAD:
            {
                return (caps.Flags & XINPUT_CAPS_WIRELESS) ? "Wireless Xbox Controller" : "Xbox Controller";
            }
            case XINPUT_DEVSUBTYPE_WHEEL:
            {
                return "XInput Wheel";
            }
            case XINPUT_DEVSUBTYPE_ARCADE_STICK:
            {
                return "XInput Arcade Stick";
            }
            case XINPUT_DEVSUBTYPE_FLIGHT_STICK:
            {
                return "XInput Flight Stick";
            }
            case XINPUT_DEVSUBTYPE_DANCE_PAD:
            {
                return "XInput Dance Pad";
            }
            case XINPUT_DEVSUBTYPE_GUITAR:
            {
                return "XInput Guitar";
            }
            case XINPUT_DEVSUBTYPE_DRUM_KIT:
            {
                return "XInput Drum Kit";
            }
            default:
            {
                return "Unknown XInput Device";
            }
        }
    }

    static void DetectXInput(DWORD user)
    {
        for (int i = 0; i < 16; ++i)
        {
            const Joystick& existing = g_Joysticks.m_Devices[i];
            if (existing.m_Connected && !existing.m_Direct && existing.m_User == user)
            {
                return;
            }
        }

        int slot = FreeIndex();
        XINPUT_CAPABILITIES caps = {};
        XINPUT_STATE state;
        if (slot < 0 || g_Joysticks.m_Capabilities(user, 0, &caps) != ERROR_SUCCESS ||
            g_Joysticks.m_State(user, &state) != ERROR_SUCCESS)
        {
            return;
        }

        Joystick& device = g_Joysticks.m_Devices[slot];
        device.m_Connected = true;
        device.m_User = user;
        DecodeWin32XInput(state.Gamepad, &device.m_Packet);
        dmStrlCpy(device.m_Name, XInputName(caps), sizeof(device.m_Name));
        ExtendedCapabilities ex = {};
        if (g_Joysticks.m_Extended && g_Joysticks.m_Extended(1, user, 0, &ex) == ERROR_SUCCESS && ex.m_Vendor && ex.m_Product)
        {
            HardwareGuid(device.m_Guid, ex.m_Vendor, ex.m_Product);
        }
        else
        {
            dmSnPrintf(device.m_Guid, sizeof(device.m_Guid), "78696e707574%02x000000000000000000", caps.SubType);
        }

        Win32NotifyGamepad(slot, WINDOW_GAMEPAD_EVENT_CONNECTED);
    }

    static bool IsXInputProduct(DWORD product)
    {
        if (!g_Joysticks.m_State)
        {
            return false;
        }

        UINT count = 0;
        if (GetRawInputDeviceList(0, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || !count)
        {
            return false;
        }

        dmArray<RAWINPUTDEVICELIST> devices;
        devices.SetCapacity(count);
        count = GetRawInputDeviceList(devices.Begin(), &count, sizeof(RAWINPUTDEVICELIST));
        if (count == (UINT)-1)
        {
            return false;
        }

        devices.SetSize(count);
        for (unsigned i = 0; i < count; ++i)
        {
            RID_DEVICE_INFO info = { sizeof(info) };
            UINT size = sizeof(info);
            if (devices[i].dwType != RIM_TYPEHID || GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICEINFO, &info, &size) == (UINT)-1 ||
                MAKELONG(info.hid.dwVendorId, info.hid.dwProductId) != product)
            {
                continue;
            }

            wchar_t path[1024] = {};
            size = sizeof(path) / sizeof(path[0]);
            if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICENAME, path, &size) != (UINT)-1 && wcsstr(path, L"IG_"))
            {
                return true;
            }
        }

        return false;
    }

    static BOOL CALLBACK CollectControl(const DIDEVICEOBJECTINSTANCEW* object, void* data)
    {
        Joystick& device = *(Joystick*)data;
        Win32DirectControls& controls = device.m_Controls;
        if (object->dwType & DIDFT_AXIS)
        {
            static const GUID* axes[] = { &GUID_XAxis, &GUID_YAxis, &GUID_ZAxis, &GUID_RxAxis, &GUID_RyAxis, &GUID_RzAxis, &GUID_Slider, &GUID_Slider };
            for (unsigned i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i)
            {
                if (!(controls.m_AxisMask & (1 << i)) && IsEqualGUID(*axes[i], object->guidType))
                {
                    DIPROPRANGE range = {};
                    range.diph.dwSize = sizeof(range);
                    range.diph.dwHeaderSize = sizeof(range.diph);
                    range.diph.dwObj = object->dwType;
                    range.diph.dwHow = DIPH_BYID;
                    range.lMin = -32768;
                    range.lMax = 32767;
                    if (SUCCEEDED(device.m_Direct->SetProperty(DIPROP_RANGE, &range.diph)))
                    {
                        controls.m_AxisMask |= 1 << i;
                    }

                    break;
                }
            }
        }
        else if ((object->dwType & DIDFT_BUTTON) && controls.m_ButtonCount < 32)
        {
            ++controls.m_ButtonCount;
        }
        else if ((object->dwType & DIDFT_POV) && controls.m_HatCount < 4)
        {
            ++controls.m_HatCount;
        }

        return DIENUM_CONTINUE;
    }

    static BOOL CALLBACK DetectDirectInput(const DIDEVICEINSTANCEW* instance, void*)
    {
        int slot = FreeIndex();
        if (slot < 0)
        {
            return DIENUM_STOP;
        }

        for (int i = 0; i < 16; ++i)
        {
            const Joystick& known = g_Joysticks.m_Devices[i];
            if (known.m_Direct && IsEqualGUID(known.m_Instance, instance->guidInstance))
            {
                return DIENUM_CONTINUE;
            }
        }

        if (IsXInputProduct(instance->guidProduct.Data1))
        {
            return DIENUM_CONTINUE;
        }

        Joystick& device = g_Joysticks.m_Devices[slot];
        if (FAILED(g_Joysticks.m_Direct->CreateDevice(instance->guidInstance, &device.m_Direct, 0)))
        {
            return DIENUM_CONTINUE;
        }

        if (FAILED(device.m_Direct->SetDataFormat(&c_dfDIJoystick)) ||
            FAILED(device.m_Direct->SetCooperativeLevel(g_Joysticks.m_Listener, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)) ||
            FAILED(device.m_Direct->EnumObjects(CollectControl, &device, DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV)) ||
            !WideCharToMultiByte(CP_UTF8, 0, instance->tszInstanceName, -1, device.m_Name, sizeof(device.m_Name), 0, 0))
        {
            Disconnect(slot, false);
            return DIENUM_CONTINUE;
        }

        if (memcmp(instance->guidProduct.Data4 + 2, "PIDVID", 6) == 0)
        {
            HardwareGuid(device.m_Guid, LOWORD(instance->guidProduct.Data1), HIWORD(instance->guidProduct.Data1));
        }
        else
        {
            dmStrlCpy(device.m_Guid, "05000000000000000000000000000000", sizeof(device.m_Guid));
            const char* digits = "0123456789abcdef";
            for (unsigned i = 0; i < 11 && device.m_Name[i]; ++i)
            {
                uint8_t c = (uint8_t)device.m_Name[i];
                device.m_Guid[8 + i * 2] = digits[c >> 4];
                device.m_Guid[9 + i * 2] = digits[c & 15];
            }
        }

        device.m_Instance = instance->guidInstance;
        device.m_Connected = true;
        device.m_Direct->Acquire();
        Win32NotifyGamepad(slot, WINDOW_GAMEPAD_EVENT_CONNECTED);
        return DIENUM_CONTINUE;
    }

    static void PollDevice(int slot)
    {
        Joystick& device = g_Joysticks.m_Devices[slot];
        if (device.m_Direct)
        {
            DIJOYSTATE state;
            device.m_Direct->Poll();
            HRESULT hr = device.m_Direct->GetDeviceState(sizeof(state), &state);
            if (hr == DIERR_NOTACQUIRED || hr == DIERR_INPUTLOST)
            {
                device.m_Direct->Acquire();
                device.m_Direct->Poll();
                hr = device.m_Direct->GetDeviceState(sizeof(state), &state);
            }

            if (SUCCEEDED(hr))
            {
                DecodeWin32DirectInput(state, device.m_Controls, &device.m_Packet);
            }
            else if (FAILED(g_Joysticks.m_Direct->GetDeviceStatus(device.m_Instance)))
            {
                Disconnect(slot, true);
            }
        }
        else
        {
            XINPUT_STATE state;
            DWORD result = g_Joysticks.m_State(device.m_User, &state);
            if (result == ERROR_SUCCESS)
            {
                DecodeWin32XInput(state.Gamepad, &device.m_Packet);
            }
            else if (result == ERROR_DEVICE_NOT_CONNECTED)
            {
                Disconnect(slot, true);
            }
        }
    }

    void Win32JoysticksChanged()
    {
        g_Joysticks.m_Rescan = true;
    }

    void Win32PollJoysticks()
    {
        if (!g_Joysticks.m_Initialized)
        {
            return;
        }

        for (int i = 0; i < 16; ++i)
        {
            if (g_Joysticks.m_Devices[i].m_Connected)
            {
                PollDevice(i);
            }
        }

        if (g_Joysticks.m_Rescan)
        {
            g_Joysticks.m_Rescan = false;
            if (g_Joysticks.m_State)
            {
                for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user)
                {
                    DetectXInput(user);
                }
            }

            if (g_Joysticks.m_Direct)
            {
                g_Joysticks.m_Direct->EnumDevices(DI8DEVCLASS_GAMECTRL, DetectDirectInput, 0, DIEDFL_ATTACHEDONLY);
            }
        }
    }

    void Win32InitJoysticks()
    {
        if (g_Joysticks.m_Initialized)
        {
            return;
        }

        g_Joysticks.m_Initialized = true;
        static const wchar_t* libraries[] = { L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
        for (unsigned i = 0; i < sizeof(libraries) / sizeof(libraries[0]); ++i)
        {
            HMODULE dll = LoadLibraryW(libraries[i]);
            if (!dll)
            {
                continue;
            }

            g_Joysticks.m_State = (DWORD(WINAPI*)(DWORD, XINPUT_STATE*))::GetProcAddress(dll, "XInputGetState");
            g_Joysticks.m_Capabilities = (DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*))::GetProcAddress(dll, "XInputGetCapabilities");
            if (g_Joysticks.m_State && g_Joysticks.m_Capabilities)
            {
                g_Joysticks.m_Extended = (DWORD(WINAPI*)(DWORD, DWORD, DWORD, ExtendedCapabilities*))::GetProcAddress(dll, MAKEINTRESOURCEA(108));
                g_Joysticks.m_XInput = dll;
                break;
            }

            g_Joysticks.m_State = 0;
            FreeLibrary(dll);
        }

        HINSTANCE instance = GetModuleHandleW(0);
        g_Joysticks.m_Listener = CreateWindowExW(0, WIN32_WINDOW_CLASS, L"", WS_POPUP, 0, 0, 1, 1, 0, 0, instance, 0);
        if (g_Joysticks.m_Listener)
        {
            DEV_BROADCAST_DEVICEINTERFACE_W filter = {};
            filter.dbcc_size = sizeof(filter);
            filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            static const GUID hid_interface = { 0x4d1e55b2, 0xf16f, 0x11cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
            filter.dbcc_classguid = hid_interface;
            g_Joysticks.m_Notification = RegisterDeviceNotificationW(g_Joysticks.m_Listener, &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
            DirectInput8Create(instance, DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&g_Joysticks.m_Direct, 0);
        }

        Win32JoysticksChanged();
        Win32PollJoysticks();
    }

    void Win32ShutdownJoysticks()
    {
        for (int i = 0; i < 16; ++i)
        {
            Disconnect(i, false);
        }

        if (g_Joysticks.m_Direct)
        {
            g_Joysticks.m_Direct->Release();
        }

        if (g_Joysticks.m_Notification)
        {
            UnregisterDeviceNotification(g_Joysticks.m_Notification);
        }

        if (g_Joysticks.m_Listener)
        {
            DestroyWindow(g_Joysticks.m_Listener);
        }

        if (g_Joysticks.m_XInput)
        {
            FreeLibrary(g_Joysticks.m_XInput);
        }

        memset(&g_Joysticks, 0, sizeof(g_Joysticks));
    }

    bool Win32JoystickPresent(int index)
    {
        Win32InitJoysticks();
        return (unsigned)index < 16 && g_Joysticks.m_Devices[index].m_Connected;
    }

    const char* GetJoystickDeviceName(HWindow, uint32_t index)
    {
        return Win32JoystickPresent(index) ? g_Joysticks.m_Devices[index].m_Name : 0;
    }

    const char* GetJoystickDeviceGuid(HWindow, uint32_t index)
    {
        return Win32JoystickPresent(index) ? g_Joysticks.m_Devices[index].m_Guid : 0;
    }

    uint32_t GetJoystickAxes(HWindow, uint32_t index, float* values, uint32_t capacity)
    {
        if (!Win32JoystickPresent(index))
        {
            return 0;
        }

        const Win32JoystickPacket& packet = g_Joysticks.m_Devices[index].m_Packet;
        uint32_t count = dmMath::Min(capacity, (uint32_t)packet.m_AxisCount);
        if (count)
        {
            memcpy(values, packet.m_Axes, count * sizeof(float));
        }

        return count;
    }

    uint32_t GetJoystickHats(HWindow, uint32_t index, uint8_t* values, uint32_t capacity)
    {
        if (!Win32JoystickPresent(index))
        {
            return 0;
        }

        const Win32JoystickPacket& packet = g_Joysticks.m_Devices[index].m_Packet;
        uint32_t count = dmMath::Min(capacity, (uint32_t)packet.m_HatCount);
        if (count)
        {
            memcpy(values, packet.m_Hats, count);
        }

        return count;
    }

    uint32_t GetJoystickButtons(HWindow, uint32_t index, uint8_t* values, uint32_t capacity)
    {
        if (!Win32JoystickPresent(index))
        {
            return 0;
        }

        const Win32JoystickPacket& packet = g_Joysticks.m_Devices[index].m_Packet;
        uint32_t count = dmMath::Min(capacity, (uint32_t)packet.m_ButtonCount);
        if (count)
        {
            memcpy(values, packet.m_Buttons, count);
        }

        return count;
    }

}
