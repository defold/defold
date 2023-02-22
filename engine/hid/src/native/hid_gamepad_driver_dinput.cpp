#include "hid_private.h"
#include "hid_native_private.h"

#include <dmsdk/graphics/graphics_native.h>
#include <dmsdk/graphics/glfw/glfw.h>

#include <dlib/log.h>

#include <stdio.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#ifndef SAFE_RELEASE
    #define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = 0; } }
#endif

namespace dmHID
{
    enum DInputDeviceObjectType
    {
        OBJECT_TYPE_SLIDER = 1,
        OBJECT_TYPE_AXIS   = 2,
        OBJECT_TYPE_BUTTON = 3,
        OBJECT_TYPE_POV    = 4,
    };

    struct DInputDeviceObject
    {
        uint32_t               m_Offset;
        DInputDeviceObjectType m_Type;
    };

    struct DInputDevice
    {
        char                 m_Name[128];
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

    static void PrintDebugDeviceStatistics(DInputDevice* device)
    {
        dmLogInfo("Device");
        dmLogInfo("  Obj count    : %d", device->m_ObjectCount);
        dmLogInfo("  Slider count : %d", device->m_SliderCount);
        dmLogInfo("  Axis count   : %d", device->m_AxisCount);
        dmLogInfo("  Button count : %d", device->m_ButtonCount);
        dmLogInfo("  POV count    : %d", device->m_POVCount);
    }

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

    static BOOL CALLBACK DInputEnumerateDevices(const DIDEVICEINSTANCE* instance, void* di_enum_ctx_ptr)
    {
        DInputGamepadDriver* driver = (DInputGamepadDriver*) di_enum_ctx_ptr;
        HContext hid_context        = driver->m_HidContext;
        DInputDevice new_device     = {};

        // Check if this device is already in-use
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            DInputDevice& device = driver->m_Devices[i];
            Gamepad* gp          = device.m_Gamepad;
            if (gp->m_Connected && memcmp(&device.m_GUID, &instance->guidInstance, sizeof(GUID)) == 0)
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

        // Set the data format to "simple joystick" - a predefined data format
        //
        // A data format specifies which controls on a device we are interested in,
        // and how they should be reported. This tells DInput that we will be
        // passing a DIJOYSTATE2 structure to IDirectInputDevice::GetDeviceState().
        if (FAILED(new_device.m_DeviceHandle->SetDataFormat(&c_dfDIJoystick2)))
        {
            dmLogError("Failed to set data format");
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_CONTINUE;
        }

        HWND hwnd = dmGraphics::GetNativeWindowsHWND();
        // Set the cooperative level to let DInput know how this device should
        // interact with the system and with other DInput applications.
        if (hwnd && FAILED(new_device.m_DeviceHandle->SetCooperativeLevel(hwnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))
        {
            dmLogError("Failed to set cooperation level");
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

        new_device.m_Objects = (DInputDeviceObject*) malloc((device_caps.dwAxes + device_caps.dwButtons + device_caps.dwPOVs) * sizeof(DInputDeviceObject));

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

        // Maybe just return the index?
        Gamepad* gp = CreateGamepad(hid_context, driver);

        if (gp == 0)
        {
            dmLogError("No free gamepads available");
            RELEASE_AND_FREE_DEVICE(new_device);
            return DIENUM_STOP;
        }
    #undef RELEASE_AND_FREE_DEVICE

        new_device.m_GUID    = instance->guidInstance;
        new_device.m_Gamepad = gp;

        driver->m_Devices.OffsetCapacity(1);
        driver->m_Devices.Push(new_device);

        gp->m_AxisCount   = new_device.m_AxisCount;
        gp->m_ButtonCount = new_device.m_ButtonCount;
        gp->m_HatCount    = new_device.m_POVCount;
        gp->m_Connected   = 1;

        SetGamepadConnectionStatus(hid_context, gp, true);

        PrintDebugDeviceStatistics(&new_device);

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

    // TODO: Add RegisterDeviceNotification somewhere in GLFW

    // NOTE: this requires us to call glfwPollEvents() before updating gamepads,
    //       which is done in hid_native.cpp
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

        // Poll the device for changes and try to re-acquire if the device was lost
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

        int button_index = 0;
        int axis_index   = 0;
        int hat_index    = 0;

        for (int j = 0; j < dinput_device->m_ObjectCount; j++)
        {
            const void* data = (char*) &state + dinput_device->m_Objects[j].m_Offset;

            switch(dinput_device->m_Objects[j].m_Type)
            {
                case OBJECT_TYPE_SLIDER:
                    break;
                case OBJECT_TYPE_AXIS:
                {
                    const float value         = (*((LONG*) data) + 0.5f) / 32767.5f;
                    packet.m_Axis[axis_index] = value;
                    axis_index++;
                }  break;
                case OBJECT_TYPE_BUTTON:
                {
                    const char value = (*((BYTE*) data) & 0x80) != 0;

                    if (value == GLFW_PRESS)
                    {
                        packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32);
                    }
                    else
                    {
                        packet.m_Buttons[button_index / 32] &= ~(1 << (button_index % 32));
                    }

                    button_index++;
                } break;
                case OBJECT_TYPE_POV:
                {
                    int stateIndex = LOWORD(*(DWORD*) data) / (45 * DI_DEGREES);
                    if (stateIndex < 0 || stateIndex > 8)
                    {
                        stateIndex = 8;
                    }
                    packet.m_Hat[hat_index] = hat_states[stateIndex];
                    hat_index++;
                } break;
            }
        }
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
                strcpy_s(buffer, buffer_length, dinput_driver->m_Devices[i].m_Name);
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
