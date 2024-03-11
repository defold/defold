// Copyright 2020-2024 The Defold Foundation
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

#include <stdio.h>
#include <stdlib.h> // qsort
#include <cmath> // fabs

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <dlib/log.h>
#include <dlib/dstrings.h>

#include <dmsdk/graphics/graphics_native.h>

#include "hid_private.h"
#include "hid_native_private.h"

#ifndef SAFE_RELEASE
    #define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = 0; } }
#endif

#include <glfw/glfw.h>

// NOTE: This implementation is inspired by both the GLFW3 and SDL sources,
//       with a heavier emphasis on the GLFW3 setup since we already use GLFW
//       for our other platform functionality.
namespace dmHID
{
    // The order of these types differs slightly between our implementation and GLFW3.
    // We need to fake the hats as buttons, so we need to store their objects first
    // in the list of device data objects. PS4 and PS5 controllers have different amount of buttons,
    // which means we can't use the same mapping for them unless we store the hat buttons first.
    enum DInputDeviceObjectType
    {
        OBJECT_TYPE_SLIDER = 0, // This is currently not supported
        OBJECT_TYPE_POV    = 1,
        OBJECT_TYPE_AXIS   = 2,
        OBJECT_TYPE_BUTTON = 3,
    };

    struct DInputDeviceObject
    {
        uint32_t               m_Offset;
        DInputDeviceObjectType m_Type;
    };

    struct DInputDevice
    {
        char                 m_Name[128];
        char                 m_ProductName[128];
        Gamepad*             m_Gamepad;
        GUID                 m_GUID;
        LPDIRECTINPUTDEVICE8 m_DeviceHandle;
        DInputDeviceObject*  m_Objects;
        uint8_t              m_ObjectCount;
        uint8_t              m_SliderCount;
        uint8_t              m_AxisCount;
        uint8_t              m_ButtonCount;
        uint8_t              m_POVCount;
    };

    struct DInputGamepadDriver : GamepadDriver
    {
        HContext              m_HidContext;
        dmArray<DInputDevice> m_Devices;
        LPDIRECTINPUT8        m_DirectInputHandle;
        RAWINPUTDEVICELIST*   m_RawDeviceList;
        uint32_t              m_RawDeviceListCount;
    };

    static bool IsXInputDevice(DInputGamepadDriver* driver, const GUID* guid)
    {
        uint32_t raw_device_count = 0;
        if (GetRawInputDeviceList(NULL, &raw_device_count, sizeof(RAWINPUTDEVICELIST)) != 0)
        {
            return false;
        }

        if (raw_device_count == 0)
        {
            return false;
        }

        if (driver->m_RawDeviceListCount != raw_device_count)
        {
            driver->m_RawDeviceList      = (RAWINPUTDEVICELIST*) malloc(sizeof(RAWINPUTDEVICELIST) * raw_device_count);
            driver->m_RawDeviceListCount = raw_device_count;
        }

        if (GetRawInputDeviceList(driver->m_RawDeviceList, &raw_device_count, sizeof(RAWINPUTDEVICELIST)) == (UINT) -1)
        {
            return false;
        }

        bool result = false;
        for (uint32_t i = 0;  i < raw_device_count;  i++)
        {
            RID_DEVICE_INFO rdi = {};
            char name[256];
            UINT size;

            if (driver->m_RawDeviceList[i].dwType != RIM_TYPEHID)
            {
                continue;
            }

            rdi.cbSize = sizeof(rdi);
            size       = sizeof(rdi);

            if ((INT) GetRawInputDeviceInfoA(driver->m_RawDeviceList[i].hDevice, RIDI_DEVICEINFO, &rdi, &size) == -1)
            {
                continue;
            }

            if (MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) != (LONG) guid->Data1)
            {
                continue;
            }

            memset(name, 0, sizeof(name));
            size = sizeof(name);

            if ((INT) GetRawInputDeviceInfoA(driver->m_RawDeviceList[i].hDevice, RIDI_DEVICENAME, name, &size) == -1)
            {
                break;
            }

            name[sizeof(name) - 1] = '\0';
            if (strstr(name, "IG_"))
            {
                result = true;
                break;
            }
        }

        return result;
    }

    static BOOL CALLBACK EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext) noexcept
    {
        DInputDevice* device       = (DInputDevice*) pContext;
        DInputDeviceObject* object = device->m_Objects + device->m_ObjectCount;

        if (DIDFT_GETTYPE(pdidoi->dwType) & DIDFT_AXIS)
        {
            DIPROPRANGE dipr = {};

            if (memcmp(&pdidoi->guidType, &GUID_Slider, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_SLIDER(device->m_SliderCount);
            }
            else if (memcmp(&pdidoi->guidType, &GUID_XAxis, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_X;
            }
            else if (memcmp(&pdidoi->guidType, &GUID_YAxis, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_Y;
            }
            else if (memcmp(&pdidoi->guidType, &GUID_ZAxis, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_Z;
            }
            else if (memcmp(&pdidoi->guidType, &GUID_RxAxis, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_RX;
            }
            else if (memcmp(&pdidoi->guidType, &GUID_RyAxis, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_RY;
            }
            else if (memcmp(&pdidoi->guidType, &GUID_RzAxis, sizeof(GUID)) == 0)
            {
                object->m_Offset = DIJOFS_RZ;
            }
            else
            {
                return DIENUM_CONTINUE;
            }

            dipr.diph.dwSize       = sizeof(dipr);
            dipr.diph.dwHeaderSize = sizeof(dipr.diph);
            dipr.diph.dwObj        = pdidoi->dwType;
            dipr.diph.dwHow        = DIPH_BYID;
            dipr.lMin              = -32768;
            dipr.lMax              =  32767;

            if (FAILED(device->m_DeviceHandle->SetProperty(DIPROP_RANGE, &dipr.diph)))
            {
                return DIENUM_CONTINUE;
            }

            if (memcmp(&pdidoi->guidType, &GUID_Slider, sizeof(GUID)) == 0)
            {
                object->m_Type = OBJECT_TYPE_SLIDER;
                device->m_SliderCount++;
            }
            else
            {
                object->m_Type = OBJECT_TYPE_AXIS;
                device->m_AxisCount++;
            }
        }
        else if (DIDFT_GETTYPE(pdidoi->dwType) & DIDFT_BUTTON)
        {
            object->m_Offset = DIJOFS_BUTTON(device->m_ButtonCount);
            object->m_Type   = OBJECT_TYPE_BUTTON;
            device->m_ButtonCount++;
        }
        else if (DIDFT_GETTYPE(pdidoi->dwType) & DIDFT_POV)
        {
            object->m_Offset = DIJOFS_POV(device->m_POVCount);
            object->m_Type   = OBJECT_TYPE_POV;
            device->m_POVCount++;
        }

        device->m_ObjectCount++;
        return DIENUM_CONTINUE;
    }

    static void DInputReleaseDevice(HContext context, DInputDevice* device)
    {
        SetGamepadConnectionStatus(context, device->m_Gamepad, false);
        ReleaseGamepad(context, device->m_Gamepad);

        device->m_DeviceHandle->Unacquire();
        SAFE_RELEASE(device->m_DeviceHandle);
        free(device->m_Objects);
    }

    inline bool SortDeviceObjects(const DInputDeviceObject& a, const DInputDeviceObject& b)
    {
        if (a.m_Type != b.m_Type)
        {
            return a.m_Type - b.m_Type;
        }

        return a.m_Offset - b.m_Offset;
    }

    static int compareJoystickObjects(const void* first, const void* second)
    {
        const DInputDeviceObject* fo = (DInputDeviceObject*) first;
        const DInputDeviceObject* so = (DInputDeviceObject*) second;

        if (fo->m_Type != so->m_Type)
        {
            return fo->m_Type - so->m_Type;
        }

        return fo->m_Offset - so->m_Offset;
    }

    static BOOL CALLBACK DInputEnumerateDevices(const DIDEVICEINSTANCE* instance, void* di_enum_ctx_ptr)
    {
        DInputGamepadDriver* driver = (DInputGamepadDriver*) di_enum_ctx_ptr;
        HContext hid_context        = driver->m_HidContext;
        DInputDevice new_device     = {};

        // Check if this device is already in-use
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            DInputDevice& device = driver->m_Devices[i];
            if (memcmp(&device.m_GUID, &instance->guidInstance, sizeof(GUID)) == 0)
            {
                return DIENUM_CONTINUE;
            }
        }

        if (IsXInputDevice(driver, &instance->guidProduct))
        {
            return DIENUM_CONTINUE;
        }

        // If it failed, then we can't use this joystick. (Maybe the user unplugged
        // it while we were in the middle of enumerating it.)
        if (FAILED(driver->m_DirectInputHandle->CreateDevice(instance->guidInstance, &new_device.m_DeviceHandle, 0)))
        {
            dmLogError("Failed to create device");
            return DIENUM_CONTINUE;
        }

    #define RELEASE_AND_FREE_DEVICE(dinput_device) \
        SAFE_RELEASE(dinput_device.m_DeviceHandle); \
        if (dinput_device.m_Objects) \
            free(dinput_device.m_Objects);

        if (FAILED(new_device.m_DeviceHandle->SetDataFormat(&c_dfDIJoystick2)))
        {
            dmLogError("Failed to set data format");
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_CONTINUE;
        }

        DIDEVCAPS device_caps = {};
        device_caps.dwSize    = sizeof(device_caps);

        if (FAILED(IDirectInputDevice8_GetCapabilities(new_device.m_DeviceHandle, &device_caps)))
        {
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_CONTINUE;
        }

        uint32_t object_count = device_caps.dwAxes + device_caps.dwButtons + device_caps.dwPOVs;
        new_device.m_Objects  = (DInputDeviceObject*) malloc(object_count * sizeof(DInputDeviceObject));

        if (FAILED(new_device.m_DeviceHandle->EnumObjects(EnumObjectsCallback, (VOID*) &new_device, DIDFT_AXIS | DIDFT_BUTTON | DIDFT_POV)))
        {
            dmLogError("Failed to enumerate objects for gamepad");
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_CONTINUE;
        }

        if (!WideCharToMultiByte(CP_UTF8, 0, instance->tszInstanceName, -1, new_device.m_Name, sizeof(new_device.m_Name), NULL, NULL))
        {
            dmLogError("Failed to convert gamepad name to UTF-8");
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_CONTINUE;
        }

        if (!WideCharToMultiByte(CP_UTF8, 0, instance->tszProductName, -1, new_device.m_ProductName, sizeof(new_device.m_ProductName), NULL, NULL))
        {
            dmStrlCpy(new_device.m_ProductName, "Unknown Product Name", sizeof(new_device.m_ProductName));
        }

        // JG: Maybe just return the index?
        Gamepad* gp = CreateGamepad(hid_context, driver);

        if (gp == 0)
        {
            dmLogError("No free gamepads available");
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_STOP;
        }
    #undef RELEASE_AND_FREE_DEVICE

        // Sort the objects based on offset and type so we can have more control in what order we process the objects
        qsort(new_device.m_Objects, new_device.m_ObjectCount, sizeof(DInputDeviceObject), compareJoystickObjects);

        // This will 'acquire' the internal gamepad pointer but not tell the engine
        // we have connected it yet, since the connection callback might have not been
        // properly setup yet..
        new_device.m_GUID    = instance->guidInstance;
        new_device.m_Gamepad = gp;

        driver->m_Devices.OffsetCapacity(1);
        driver->m_Devices.Push(new_device);

        return DIENUM_CONTINUE;
    }

    static void DInputDetectDevices(HContext context, GamepadDriver* driver)
    {
        DInputGamepadDriver* dinput_driver = (DInputGamepadDriver*) driver;
        if (FAILED(dinput_driver->m_DirectInputHandle->EnumDevices(DI8DEVCLASS_GAMECTRL, DInputEnumerateDevices, (void*) driver, DIEDFL_ALLDEVICES)))
        {
            dmLogError("Failed to enumerate input devices");
            return;
        }
    }

    // This fn requires us to call glfwPollEvents() before updating gamepads, which is done in hid_native.cpp
    static void DInputUpdate(HContext context, GamepadDriver* driver, Gamepad* gamepad)
    {
        const int hat_states[] =
        {
            GLFW_HAT_UP,
            GLFW_HAT_RIGHTUP,
            GLFW_HAT_RIGHT,
            GLFW_HAT_RIGHTDOWN,
            GLFW_HAT_DOWN,
            GLFW_HAT_LEFTDOWN,
            GLFW_HAT_LEFT,
            GLFW_HAT_LEFTUP,
            GLFW_HAT_CENTERED
        };

        DInputGamepadDriver* dinput_driver = (DInputGamepadDriver*) driver;
        DInputDevice* dinput_device = 0;

        int device_index = 0;
        for (int i = 0; i < dinput_driver->m_Devices.Size(); ++i)
        {
            if (dinput_driver->m_Devices[i].m_Gamepad == gamepad)
            {
                dinput_device = &dinput_driver->m_Devices[i];
                device_index = i;
                break;
            }
        }

        assert(dinput_device != 0);

        // Poll the device for changes and try to re-acquire if the device was lost,
        // if that fails we have to release the device and try again later..
        dinput_device->m_DeviceHandle->Poll();

        DIJOYSTATE2 state = {0};
        HRESULT hr = dinput_device->m_DeviceHandle->GetDeviceState(sizeof(state), &state);

        if (hr == DIERR_NOTACQUIRED || hr == DIERR_INPUTLOST)
        {
            dinput_device->m_DeviceHandle->Acquire();
            dinput_device->m_DeviceHandle->Poll();
            hr = dinput_device->m_DeviceHandle->GetDeviceState(sizeof(state), &state);
        }

        if (FAILED(hr))
        {
            DInputReleaseDevice(context, dinput_device);
            dinput_driver->m_Devices.EraseSwap(device_index);
            return;
        }

        Gamepad* gp           = dinput_device->m_Gamepad;
        GamepadPacket& packet = gp->m_Packet;

        // We need to let the engine know that this gamepad has been connected
        if (!gp->m_Connected)
        {
            // NOTE: we add 4 extra buttons for the hats here to conform with the GLFW gamepad driver!
            gp->m_AxisCount   = dinput_device->m_AxisCount;
            gp->m_ButtonCount = dinput_device->m_ButtonCount + dinput_device->m_POVCount * 4;
            gp->m_HatCount    = dinput_device->m_POVCount;
            SetGamepadConnectionStatus(context, gp, true);
        }

        int button_index = 0;
        int axis_index   = 0;
        int hat_index    = 0;

        const float CENTER_EPSILON = 0.05f;

        #define SET_BUTTON(ix, value) \
            if (value == GLFW_PRESS) \
                packet.m_Buttons[ix / 32] |= 1 << (ix % 32); \
            else \
                packet.m_Buttons[ix / 32] &= ~(1 << (ix % 32));

        for (int j = 0; j < dinput_device->m_ObjectCount; j++)
        {
            const void* data = (char*) &state + dinput_device->m_Objects[j].m_Offset;

            switch(dinput_device->m_Objects[j].m_Type)
            {
                case OBJECT_TYPE_SLIDER:
                    break;
                case OBJECT_TYPE_AXIS:
                {
                    // Convert axis to [-1, +1]
                    const float value         = (*((LONG*) data) + 0.5f) / 32767.5f;
                    packet.m_Axis[axis_index] = value;

                    // JG: There's no dead zone configuration in windows for dinupt devices afaik,
                    //     so to make the input a bit more stable we clamp the input here.
                    if (fabs(value) < CENTER_EPSILON)
                    {
                        packet.m_Axis[axis_index] = 0.0f;
                    }

                    axis_index++;
                }  break;
                case OBJECT_TYPE_BUTTON:
                {
                    const char value = (*((BYTE*) data) & 0x80) != 0;
                    SET_BUTTON(button_index, value);
                    button_index++;
                } break;
                case OBJECT_TYPE_POV:
                {
                    int stateIndex = LOWORD(*(DWORD*) data) / (45 * DI_DEGREES);
                    if (stateIndex < 0 || stateIndex > 8)
                    {
                        stateIndex = 8;
                    }

                    int hat_state           = hat_states[stateIndex];
                    packet.m_Hat[hat_index] = hat_state;
                    hat_index++;

                    // North pad
                    SET_BUTTON(button_index, (hat_state & GLFW_HAT_UP) == GLFW_HAT_UP);
                    button_index++;

                    // South pad
                    SET_BUTTON(button_index, (hat_state & GLFW_HAT_DOWN) == GLFW_HAT_DOWN);
                    button_index++;

                    // West pad
                    SET_BUTTON(button_index, (hat_state & GLFW_HAT_LEFT) == GLFW_HAT_LEFT);
                    button_index++;

                    // East pad
                    SET_BUTTON(button_index, (hat_state & GLFW_HAT_RIGHT) == GLFW_HAT_RIGHT);
                    button_index++;
                } break;
            }
        }

        #undef SET_BUTTON
    }

    static bool DInputInitialize(HContext context, GamepadDriver* driver)
    {
        DInputGamepadDriver* dinput_driver = (DInputGamepadDriver*) driver;

        if (FAILED(DirectInput8Create(GetModuleHandle(0), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&dinput_driver->m_DirectInputHandle, 0)))
        {
            dmLogError("Failed to create Direct Input context");
            return false;
        }

        return true;
    }

    static void DInputDestroy(HContext ctx, GamepadDriver* driver)
    {
        DInputGamepadDriver* dinput_driver = (DInputGamepadDriver*) driver;
        for (int i = 0; i < dinput_driver->m_Devices.Size(); ++i)
        {
            if (dinput_driver->m_Devices[i].m_DeviceHandle)
            {
                dinput_driver->m_Devices[i].m_DeviceHandle->Unacquire();
            }

            SAFE_RELEASE(dinput_driver->m_Devices[i].m_DeviceHandle);
        }

        SAFE_RELEASE(dinput_driver->m_DirectInputHandle);

        delete dinput_driver;
        dinput_driver = 0;
    }

    static void DInputGetGamepadName(HContext context, GamepadDriver* driver, Gamepad* gamepad, char* buffer, uint32_t buffer_length)
    {
        DInputGamepadDriver* dinput_driver = (DInputGamepadDriver*) driver;

        for (int i = 0; i < dinput_driver->m_Devices.Size(); ++i)
        {
            if (dinput_driver->m_Devices[i].m_Gamepad == gamepad)
            {
                dmStrlCpy(buffer, dinput_driver->m_Devices[i].m_Name, buffer_length);
                return;
            }
        }
    }

    GamepadDriver* CreateGamepadDriverDInput(HContext context)
    {
        DInputGamepadDriver* driver    = new DInputGamepadDriver();
        driver->m_Initialize           = DInputInitialize;
        driver->m_Destroy              = DInputDestroy;
        driver->m_Update               = DInputUpdate;
        driver->m_GetGamepadDeviceName = DInputGetGamepadName;
        driver->m_DetectDevices        = DInputDetectDevices;
        driver->m_HidContext           = context;

        return driver;
    }
}
