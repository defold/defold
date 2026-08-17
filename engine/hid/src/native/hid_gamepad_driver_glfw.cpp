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

#include "hid.h"

#include <assert.h>
#include <string.h>

#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/math.h>
#include <dlib/static_assert.h>

#include <platform/window.hpp>
#include <platform/platform_window_constants.h>

#include "hid_private.h"

#include "hid_native_private.h"

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace dmHID
{
    static int GLFW_JOYSTICKS[MAX_GAMEPAD_COUNT] =
    {
        dmPlatform::PLATFORM_JOYSTICK_1,
        dmPlatform::PLATFORM_JOYSTICK_2,
        dmPlatform::PLATFORM_JOYSTICK_3,
        dmPlatform::PLATFORM_JOYSTICK_4,
        dmPlatform::PLATFORM_JOYSTICK_5,
        dmPlatform::PLATFORM_JOYSTICK_6,
        dmPlatform::PLATFORM_JOYSTICK_7,
        dmPlatform::PLATFORM_JOYSTICK_8,
        dmPlatform::PLATFORM_JOYSTICK_9,
        dmPlatform::PLATFORM_JOYSTICK_10,
        dmPlatform::PLATFORM_JOYSTICK_11,
        dmPlatform::PLATFORM_JOYSTICK_12,
        dmPlatform::PLATFORM_JOYSTICK_13,
        dmPlatform::PLATFORM_JOYSTICK_14,
        dmPlatform::PLATFORM_JOYSTICK_15,
        dmPlatform::PLATFORM_JOYSTICK_16
    };

    enum GamepadRemapStrategy
    {
        GAMEPAD_REMAP_STRATEGY_NONE          = 0,
        GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT = 1,
        GAMEPAD_REMAP_STRATEGY_LEGACY_DINPUT = 2,
        GAMEPAD_REMAP_STRATEGY_LEGACY_LINUX  = 3,
    };

    struct GLFWGamepadDevice
    {
        int                  m_Id;
        Gamepad*             m_Gamepad;
        GamepadRemapStrategy m_RemapStrategy;
#if defined(_WIN32)
        GamepadGuid          m_AutomaticGuid;
        char                 m_AutomaticName[MAX_GAMEPAD_NAME_LENGTH];
        bool                 m_HasAutomaticIdentity;
#endif
    };


    // The GLFW driver stores an indirect table to map a GLFW index to our internal representation,
    // this is needed because multiple gamepad drivers can exist at the same time.
    struct GLFWGamepadDriver : GamepadDriver
    {
        HContext                   m_HidContext;
        dmArray<GLFWGamepadDevice> m_Devices;
    };

    static void GetGamepadDeviceNameInternal(HContext context, int glfw_id, char name[MAX_GAMEPAD_NAME_LENGTH]);
    static bool GetGamepadDeviceGuidInternal(HContext context, int glfw_id, GamepadGuid* guid);

    static GLFWGamepadDriver* g_GLFWGamepadDriver = 0;

#if defined(_WIN32)
    typedef BOOLEAN (WINAPI *HidDGetStringFn)(HANDLE device, PVOID buffer, ULONG buffer_length);

    static HidDGetStringFn g_HidDGetManufacturerString = 0;
    static HidDGetStringFn g_HidDGetProductString = 0;
    static bool g_HidFunctionsResolved = false;

    static uint16_t CRC16ForByte(uint8_t value)
    {
        uint16_t crc = value;
        for (uint32_t i = 0; i < 8; ++i)
            crc = (uint16_t) ((crc & 1) ? (crc >> 1) ^ 0xa001 : crc >> 1);
        return crc;
    }

    static uint16_t UpdateCRC16(uint16_t crc, const char* string)
    {
        while (string && *string)
            crc = (uint16_t) (CRC16ForByte((uint8_t) crc ^ (uint8_t) *string++) ^ (crc >> 8));
        return crc;
    }

    static void CopyAutomaticGamepadName(const char* product_name, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        static const char* prefix = "Controller (";
        const size_t prefix_length = strlen(prefix);
        const size_t product_length = strlen(product_name);
        if (product_length > prefix_length &&
            strncmp(product_name, prefix, prefix_length) == 0 &&
            product_name[product_length - 1] == ')')
        {
            dmSnPrintf(name, MAX_GAMEPAD_NAME_LENGTH, "%.*s",
                       (int) (product_length - prefix_length - 1), product_name + prefix_length);
            return;
        }
        dmStrlCpy(name, product_name, MAX_GAMEPAD_NAME_LENGTH);
    }

    static bool ConvertWideStringToUTF8(const wchar_t* source, char* destination, uint32_t destination_size)
    {
        if (!source || !source[0] || destination_size == 0)
            return false;
        return WideCharToMultiByte(CP_UTF8, 0, source, -1, destination, destination_size, 0, 0) > 0;
    }

    static void ResolveHidFunctions()
    {
        if (g_HidFunctionsResolved)
            return;

        g_HidFunctionsResolved = true;
        HMODULE hid_module = LoadLibraryW(L"hid.dll");
        if (!hid_module)
            return;

        g_HidDGetManufacturerString = (HidDGetStringFn) GetProcAddress(hid_module, "HidD_GetManufacturerString");
        g_HidDGetProductString = (HidDGetStringFn) GetProcAddress(hid_module, "HidD_GetProductString");
    }

    static bool GetRawInputDeviceIdentity(uint16_t vendor, uint16_t product, uint16_t* version,
                                          char manufacturer[128], char product_name[MAX_GAMEPAD_NAME_LENGTH])
    {
        ResolveHidFunctions();
        if (!g_HidDGetManufacturerString || !g_HidDGetProductString)
            return false;

        UINT device_count = 0;
        if (GetRawInputDeviceList(0, &device_count, sizeof(RAWINPUTDEVICELIST)) == (UINT) -1 || device_count == 0)
            return false;

        RAWINPUTDEVICELIST* devices = new RAWINPUTDEVICELIST[device_count];
        if (GetRawInputDeviceList(devices, &device_count, sizeof(RAWINPUTDEVICELIST)) == (UINT) -1)
        {
            delete[] devices;
            return false;
        }

        bool found = false;
        for (UINT i = 0; i < device_count && !found; ++i)
        {
            if (devices[i].dwType != RIM_TYPEHID)
                continue;

            RID_DEVICE_INFO device_info = {};
            device_info.cbSize = sizeof(device_info);
            UINT info_size = sizeof(device_info);
            if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICEINFO, &device_info, &info_size) == (UINT) -1 ||
                device_info.hid.dwVendorId != vendor || device_info.hid.dwProductId != product ||
                device_info.hid.usUsagePage != 0x01 || device_info.hid.usUsage != 0x05)
            {
                continue;
            }

            wchar_t device_path[1024];
            UINT path_size = sizeof(device_path) / sizeof(device_path[0]);
            if (GetRawInputDeviceInfoW(devices[i].hDevice, RIDI_DEVICENAME, device_path, &path_size) == (UINT) -1 ||
                wcsstr(device_path, L"IG_") == 0)
            {
                continue;
            }

            HANDLE device = CreateFileW(device_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        0, OPEN_EXISTING, 0, 0);
            if (device == INVALID_HANDLE_VALUE)
                continue;

            wchar_t wide_string[128] = {};
            if (g_HidDGetManufacturerString(device, wide_string, sizeof(wide_string)))
                ConvertWideStringToUTF8(wide_string, manufacturer, 128);

            wide_string[0] = 0;
            if (g_HidDGetProductString(device, wide_string, sizeof(wide_string)))
                ConvertWideStringToUTF8(wide_string, product_name, MAX_GAMEPAD_NAME_LENGTH);

            CloseHandle(device);
            *version = (uint16_t) device_info.hid.dwVersionNumber;
            found = product_name[0] != 0;
        }

        delete[] devices;
        return found;
    }

    // SDL's RawInput GUID is useful for diagnostics even though GLFW supplies
    // the live XInput packet. Keep that compatibility detail local to this
    // driver, together with the automatic packet-layout decision.
    static bool EnsureAutomaticXInputIdentity(HContext context, GLFWGamepadDevice* device)
    {
        if (device->m_HasAutomaticIdentity)
            return true;
        if (device->m_RemapStrategy != GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT)
            return false;

        GamepadGuid guid = {};
        if (!GetGamepadDeviceGuidInternal(context, device->m_Id, &guid) || guid.m_Vendor == 0 || guid.m_Product == 0)
            return false;

        char manufacturer[128] = {};
        char product_name[MAX_GAMEPAD_NAME_LENGTH] = {};
        uint16_t version = guid.m_Version;
        if (!GetRawInputDeviceIdentity(guid.m_Vendor, guid.m_Product, &version, manufacturer, product_name))
            return false;

        uint16_t crc = 0;
        if (manufacturer[0])
        {
            crc = UpdateCRC16(crc, manufacturer);
            crc = UpdateCRC16(crc, " ");
        }
        crc = UpdateCRC16(crc, product_name);

        device->m_AutomaticGuid = guid;
        device->m_AutomaticGuid.m_CRC16 = crc;
        device->m_AutomaticGuid.m_Version = version;
        device->m_AutomaticGuid.m_DriverSignature = 'r';
        device->m_AutomaticGuid.m_DriverData = 0;
        CopyAutomaticGamepadName(product_name, device->m_AutomaticName);
        device->m_HasAutomaticIdentity = true;
        return true;
    }
#endif

    static Gamepad* GLFWGetGamepad(GLFWGamepadDriver* driver, int gamepad_id)
    {
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            if (driver->m_Devices[i].m_Id == gamepad_id)
            {
                return driver->m_Devices[i].m_Gamepad;
            }
        }

        return 0;
    }

    // Returns the glfw gamepad ID
    static int GLFWUnpackGamepad(GLFWGamepadDriver* driver, Gamepad* gamepad, GLFWGamepadDevice** glfw_gamepad_device_out)
    {
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            if (driver->m_Devices[i].m_Gamepad == gamepad)
            {
                if (glfw_gamepad_device_out)
                    *glfw_gamepad_device_out = &driver->m_Devices[i];
                return driver->m_Devices[i].m_Id;
            }
        }

        return -1;
    }

    static Gamepad* GLFWEnsureAllocatedGamepad(GLFWGamepadDriver* driver, int gamepad_id)
    {
        Gamepad* gp = GLFWGetGamepad(driver, gamepad_id);
        if (gp != 0)
        {
            return gp;
        }

        gp = CreateGamepad(driver->m_HidContext, driver);
        if (gp == 0)
        {
            return 0;
        }

        GLFWGamepadDevice new_device = {};
        new_device.m_Id              = gamepad_id;
        new_device.m_Gamepad         = gp;
        new_device.m_RemapStrategy   = GAMEPAD_REMAP_STRATEGY_NONE;

    #if defined(_WIN32)
        // For windows, we need to be able to remap button layouts according to
        // how it was created (xinput or dinput) in order to comply with old button mappings.
        new_device.m_RemapStrategy = GAMEPAD_REMAP_STRATEGY_LEGACY_DINPUT;

        char gamepad_name[MAX_GAMEPAD_NAME_LENGTH];
        GetGamepadDeviceNameInternal(driver->m_HidContext, gamepad_id, gamepad_name);

        if (strcmp(gamepad_name, "Wireless Xbox Controller") == 0 ||
            strcmp(gamepad_name, "Xbox Controller") == 0 ||
            strstr(gamepad_name, "XInput"))
        {
            new_device.m_RemapStrategy = GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT;
        }
    #elif defined(__linux__)
        // Same with linux, the order for gamepad buttons have changed since GLFW 2.7, so we need to adhere to that.
        new_device.m_RemapStrategy = GAMEPAD_REMAP_STRATEGY_LEGACY_LINUX;
    #endif

        if (driver->m_Devices.Full())
        {
            driver->m_Devices.OffsetCapacity(1);
        }

        driver->m_Devices.Push(new_device);

        SetGamepadConnectionStatus(driver->m_HidContext, gp, true);

        return gp;
    }

    static void GLFWRemoveGamepad(GLFWGamepadDriver* driver, int gamepad_id)
    {
        for (int i = 0; i < driver->m_Devices.Size(); ++i)
        {
            if (driver->m_Devices[i].m_Id == gamepad_id)
            {
                SetGamepadConnectionStatus(driver->m_HidContext, driver->m_Devices[i].m_Gamepad, false);
                ReleaseGamepad(driver->m_HidContext, driver->m_Devices[i].m_Gamepad);
                driver->m_Devices.EraseSwap(i);
                return;
            }
        }
    }


    // Note: For windows and Linux we will get callbacks here on init when
    // we detect devices using GLFWGamepadDriverDetectDevices. If a joystick
    // is not present when checking a joystick id glfw will generate a
    // disconnect callback.
    // https://github.com/glfw/glfw/blob/3.4/src/win32_joystick.c#L627
    // https://github.com/glfw/glfw/blob/3.4/src/linux_joystick.c#L398
    static void GLFWGamepadCallback(void* user_Data, int gamepad_id, WindowGamepadEvent event)
    {
        Gamepad* gp = 0;
        if (event == WINDOW_GAMEPAD_EVENT_CONNECTED)
        {
            gp = GLFWEnsureAllocatedGamepad(g_GLFWGamepadDriver, gamepad_id);
        }
        else if (event == WINDOW_GAMEPAD_EVENT_DISCONNECTED)
        {
            gp = GLFWGetGamepad(g_GLFWGamepadDriver, gamepad_id);
        }

        if (gp != 0)
        {
            SetGamepadConnectionStatus(g_GLFWGamepadDriver->m_HidContext, gp, event == WINDOW_GAMEPAD_EVENT_CONNECTED ? 1 : 0);
        }
    }

    static void RemapGamepadAxis(GLFWGamepadDevice* glfw_gamepad, float* axis, uint8_t* buttons)
    {
        if (!glfw_gamepad->m_Gamepad->m_LayoutLegacy)
        {
            return;
        }

        if (glfw_gamepad->m_RemapStrategy != GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT &&
            glfw_gamepad->m_RemapStrategy != GAMEPAD_REMAP_STRATEGY_LEGACY_LINUX)
        {
            return;
        }

        // The new XInput mapping has positive Y axis downwards and negative upwards, so we need to reverse those.
        // XInput devices will always have 6 axis values (left X, left Y, right X, right Y, left Trigger, right Trigger),
        // so it's safe to not bounds check here:
        axis[1] *= -1.0f;
        axis[3] *= -1.0f;

        if (glfw_gamepad->m_RemapStrategy == GAMEPAD_REMAP_STRATEGY_LEGACY_LINUX)
        {
            // For linux, the old GLFW 2.7.7 version represents hats as axes, but in newer
            // glfw version they are actually represented as buttons instead.

            Gamepad* gamepad          = glfw_gamepad->m_Gamepad;
            uint8_t axis_count        = gamepad->m_HatCount * 2;
            uint8_t axis_start        = 6;
            int32_t hats_button_count = gamepad->m_HatCount * 4;
            int32_t hats_start        = gamepad->m_ButtonCount - hats_button_count;
            gamepad->m_AxisCount      = dmMath::Max(gamepad->m_AxisCount, (uint8_t) (axis_start + axis_count));

            for (int i = 0; i < gamepad->m_HatCount; ++i)
            {
                uint32_t hats_offset = i * 4 + hats_start;
                uint32_t axis_offset = i * 2 + axis_start;

                uint8_t hat_up    = buttons[hats_offset];
                uint8_t hat_right = buttons[hats_offset+1];
                uint8_t hat_down  = buttons[hats_offset+2];
                uint8_t hat_left  = buttons[hats_offset+3];

                if (hat_right)
                {
                    axis[axis_offset] = 1.0;
                }
                else if (hat_left)
                {
                    axis[axis_offset] = -1.0;
                }
                else
                {
                    axis[axis_offset] = 0.0;
                }

                axis_offset++;

                if (hat_up)
                {
                    axis[axis_offset] = 1.0;
                }
                else if (hat_down)
                {
                    axis[axis_offset] = -1.0;
                }
                else
                {
                    axis[axis_offset] = 0.0;
                }
            }
        }
    }

    static uint8_t* RemapGamepadButtons(GLFWGamepadDevice* glfw_gamepad, uint8_t* buttons, uint8_t* buttons_remapped)
    {
        if (!glfw_gamepad->m_Gamepad->m_LayoutLegacy)
        {
#if defined(_WIN32)
            if (glfw_gamepad->m_RemapStrategy == GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT)
            {
                // GLFW exposes XInput buttons as A, B, X, Y, shoulders,
                // back, start and stick clicks. The automatic mapping consumes
                // the canonical SDL-style order declared by GamepadMappedButton.
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_A]              = buttons[0];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_B]              = buttons[1];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_X]              = buttons[2];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_Y]              = buttons[3];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_BACK]           = buttons[6];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_START]          = buttons[7];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_LEFT_THUMB]     = buttons[8];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_RIGHT_THUMB]    = buttons[9];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_LEFT_SHOULDER]  = buttons[4];
                buttons_remapped[GAMEPAD_MAPPED_BUTTON_RIGHT_SHOULDER] = buttons[5];

                // XInput through GLFW doesn't expose Guide or Capture. Keep
                // those slots clear instead of reading GLFW's appended hat
                // buttons as ordinary buttons.
                glfw_gamepad->m_Gamepad->m_ButtonCount = GAMEPAD_MAPPED_BUTTON_COUNT;
                return buttons_remapped;
            }
#endif
            return buttons;
        }

        if (glfw_gamepad->m_RemapStrategy != GAMEPAD_REMAP_STRATEGY_LEGACY_DINPUT &&
            glfw_gamepad->m_RemapStrategy != GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT)
        {
            return buttons;
        }

        // For win32, hats are placed at the END of the button array on later glfw version,
        // but old Defold expects the hats to be placed first. So to avoid forcing people to
        // do a new remapping for their gamepads, we just copy and adjust the hats from the new
        // glfw format to the old one.
        Gamepad* gamepad                = glfw_gamepad->m_Gamepad;
        int32_t hats_button_count       = gamepad->m_HatCount * 4;
        int32_t hats_start              = gamepad->m_ButtonCount - hats_button_count;
        uint8_t* buttons_remapped_start = buttons_remapped + hats_button_count;

        if (glfw_gamepad->m_RemapStrategy == GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT)
        {
            // In GLFW 2.7, the button count is hardcoded to 16 for XInput devices.
            gamepad->m_ButtonCount = 16;

            // For XInput devices, the button order has changed so we have to do an explicit remapping here.
            //
            // Q: How does this remapping work?
            // A: In the old glfw code, the button at index i is calculated as:
            //      for i=0; i < max_buttons do
            //          buttons[ button ] = (unsigned char) (ji.dwButtons & (1UL << button) ? GLFW_PRESS : GLFW_RELEASE);
            //
            //    The values of the ji.dwButtons mask is specified in xinput.h that we have in the glfw repo:
            //      #define XINPUT_GAMEPAD_DPAD_UP          0x0001 (button index: 0)
            //      #define XINPUT_GAMEPAD_DPAD_DOWN        0x0002 (button index: 1)
            //      #define XINPUT_GAMEPAD_DPAD_LEFT        0x0004 (button index: 2)
            //      #define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008 (button index: 3)
            //      #define XINPUT_GAMEPAD_START            0x0010 (button index: 4)
            //      #define XINPUT_GAMEPAD_BACK             0x0020 (button index: 5)
            //      #define XINPUT_GAMEPAD_LEFT_THUMB       0x0040 (button index: 6)
            //      #define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080 (button index: 7)
            //      #define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100 (button index: 8)
            //      #define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200 (button index: 9)
            //              (note! no index 10/11, 0x400 and 0x800 are missing!)
            //      #define XINPUT_GAMEPAD_A                0x1000 (button index: 12)
            //      #define XINPUT_GAMEPAD_B                0x2000 (button index: 13)
            //      #define XINPUT_GAMEPAD_X                0x4000 (button index: 14)
            //      #define XINPUT_GAMEPAD_Y                0x8000 (button index: 15)
            //
            //   In GLFW 3.4, the xinput button order has changed to:
            //      XINPUT_GAMEPAD_A,              (button index: 0)
            //      XINPUT_GAMEPAD_B,              (button index: 1)
            //      XINPUT_GAMEPAD_X,              (button index: 2)
            //      XINPUT_GAMEPAD_Y,              (button index: 3)
            //      XINPUT_GAMEPAD_LEFT_SHOULDER,  (button index: 4)
            //      XINPUT_GAMEPAD_RIGHT_SHOULDER, (button index: 5)
            //      XINPUT_GAMEPAD_BACK,           (button index: 6)
            //      XINPUT_GAMEPAD_START,          (button index: 7)
            //      XINPUT_GAMEPAD_LEFT_THUMB,     (button index: 8)
            //      XINPUT_GAMEPAD_RIGHT_THUMB     (button index: 9)
            //
            //   What we want to do now is to put the values from the new mapping into
            //   the same order as the old array would look like.
            //   For example, in the new order we find the "start" button at index 7,
            //   but in the old mapping it would have index 0.
            //
            //   Since we do the dpad mapping later, we skip the dpads here.
            //   The "buttons_remapped_start" pointer is pointer to the start of the
            //   button block (which is after all the dpads) so that's why it seems
            //   like the indices aren't the same as from the XInput header.

            buttons_remapped_start[0]  = buttons[7];
            buttons_remapped_start[1]  = buttons[6];
            buttons_remapped_start[2]  = buttons[8];
            buttons_remapped_start[3]  = buttons[9];
            buttons_remapped_start[4]  = buttons[4];
            buttons_remapped_start[5]  = buttons[5];
            // NO button data for index (6,7)
            buttons_remapped_start[8]  = buttons[0];
            buttons_remapped_start[9]  = buttons[1];
            buttons_remapped_start[10] = buttons[2];
            buttons_remapped_start[11] = buttons[3];
        }
        else if (glfw_gamepad->m_RemapStrategy == GAMEPAD_REMAP_STRATEGY_LEGACY_DINPUT)
        {
            // For direct input devices, we can take all buttons as is without explicit button remapping (except dpads)
            memcpy(buttons_remapped_start, buttons, hats_start);
        }

        // For both remap variants we put all the hat buttons in the front of the array
        for (int i = 0; i < gamepad->m_HatCount; ++i)
        {
            uint32_t hat_base = i * gamepad->m_HatCount;
            buttons_remapped[hat_base + 0] = buttons[hats_start + hat_base];
            buttons_remapped[hat_base + 1] = buttons[hats_start + hat_base + 2];
            buttons_remapped[hat_base + 2] = buttons[hats_start + hat_base + 3];
            buttons_remapped[hat_base + 3] = buttons[hats_start + hat_base + 1];
        }

        return buttons_remapped;
    }

    static void GLFWGamepadDriverUpdate(HContext context, GamepadDriver* driver, Gamepad* gamepad)
    {
        GLFWGamepadDevice* glfw_device;
        int id = GLFWUnpackGamepad((GLFWGamepadDriver*) driver, gamepad, &glfw_device);
        assert(id != -1);

        int glfw_joystick     = GLFW_JOYSTICKS[id];
        GamepadPacket& packet = gamepad->m_Packet;

        uint8_t buttons[MAX_GAMEPAD_BUTTON_COUNT] = {};
        uint8_t buttons_remapped[MAX_GAMEPAD_BUTTON_COUNT] = {};

        gamepad->m_AxisCount   = dmPlatform::GetJoystickAxes(context->m_Window, glfw_joystick, packet.m_Axis, MAX_GAMEPAD_AXIS_COUNT);
        gamepad->m_HatCount    = dmPlatform::GetJoystickHats(context->m_Window, glfw_joystick, packet.m_Hat, MAX_GAMEPAD_HAT_COUNT);
        gamepad->m_ButtonCount = dmPlatform::GetJoystickButtons(context->m_Window, glfw_joystick, buttons, MAX_GAMEPAD_BUTTON_COUNT);

        uint8_t* buttons_ptr = RemapGamepadButtons(glfw_device, buttons, buttons_remapped);
        RemapGamepadAxis(glfw_device, packet.m_Axis, buttons);

        for (uint32_t j = 0; j < gamepad->m_ButtonCount; ++j)
        {
            if (buttons_ptr[j])
            {
                packet.m_Buttons[j / 32] |= 1 << (j % 32);
            }
            else
            {
                packet.m_Buttons[j / 32] &= ~(1 << (j % 32));
            }
        }
    }

    static void GLFWGamepadDriverDetectDevices(HContext context, GamepadDriver* driver)
    {
        GLFWGamepadDriver* glfw_driver = (GLFWGamepadDriver*) driver;

        for (int i = 0; i < MAX_GAMEPAD_COUNT; ++i)
        {
            if (dmPlatform::GetDeviceState(context->m_Window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, i))
            {
                GLFWEnsureAllocatedGamepad(glfw_driver, i);
            }
            else
            {
                GLFWRemoveGamepad(glfw_driver, i);
            }
        }
    }

    static void GetGamepadDeviceNameInternal(HContext context, int glfw_id, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        const char* device_name = dmPlatform::GetJoystickDeviceName(context->m_Window, glfw_id);
        if (device_name != 0x0)
        {
            dmStrlCpy(name, device_name, MAX_GAMEPAD_NAME_LENGTH);
        }
        else
        {
            // the gamepad must have a non-empty string name
            // in order to use it (even as the a gamepad binding)
            dmStrlCpy(name, "_", MAX_GAMEPAD_NAME_LENGTH);
        }
    }

    static void GLFWGamepadDriverGetGamepadDeviceName(HContext context, GamepadDriver* driver, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        GLFWGamepadDevice* glfw_device = 0;
        uint32_t gamepad_index = GLFWUnpackGamepad((GLFWGamepadDriver*) driver, gamepad, &glfw_device);
#if defined(_WIN32)
        if (glfw_device && EnsureAutomaticXInputIdentity(context, glfw_device))
        {
            dmStrlCpy(name, glfw_device->m_AutomaticName, MAX_GAMEPAD_NAME_LENGTH);
            return;
        }
#endif
        GetGamepadDeviceNameInternal(context, gamepad_index, name);
    }

    static bool GetGamepadDeviceGuidInternal(HContext context, int glfw_id, GamepadGuid* guid)
    {
        const char* device_guid = dmPlatform::GetJoystickDeviceGuid(context->m_Window, glfw_id);
        if (device_guid != 0x0)
        {
            return ParseGamepadGuid(device_guid, guid);
        }
        return false;
    }

    static bool GLFWGamepadDriverGetGamepadDeviceGuid(HContext context, GamepadDriver* driver, HGamepad gamepad, GamepadGuid* guid)
    {
        GLFWGamepadDevice* glfw_device = 0;
        uint32_t gamepad_index = GLFWUnpackGamepad((GLFWGamepadDriver*) driver, gamepad, &glfw_device);
#if defined(_WIN32)
        if (glfw_device && EnsureAutomaticXInputIdentity(context, glfw_device))
        {
            *guid = glfw_device->m_AutomaticGuid;
            return true;
        }
#endif
        return GetGamepadDeviceGuidInternal(context, gamepad_index, guid);
    }

    static uint32_t GLFWGamepadDriverGetGamepadMappingSupport(HContext context, GamepadDriver* driver, HGamepad gamepad)
    {
        GLFWGamepadDevice* glfw_device = 0;
        if (GLFWUnpackGamepad((GLFWGamepadDriver*) driver, gamepad, &glfw_device) == -1)
            return GAMEPAD_MAPPING_SUPPORT_NONE;

#if defined(_WIN32)
        // The Windows XInput backend exposes a stable semantic packet layout,
        // so it can use the automatic mapping when no database row exists.
        // DirectInput devices expose their physical layout and still require a
        // database mapping (or the raw fallback).
        if (glfw_device->m_RemapStrategy == GAMEPAD_REMAP_STRATEGY_LEGACY_XINPUT)
        {
            EnsureAutomaticXInputIdentity(context, glfw_device);
            return GAMEPAD_MAPPING_SUPPORT_AUTOMATIC;
        }
#endif

        return GAMEPAD_MAPPING_SUPPORT_NONE;
    }

    static bool GLFWGamepadDriverInitialize(HContext context, GamepadDriver* driver)
    {
        if (!dmPlatform::GetWindowStateParam(context->m_Window, WINDOW_STATE_OPENED))
        {
            return false;
        }

        dmPlatform::SetGamepadEventCallback(context->m_Window, GLFWGamepadCallback, 0);
        return true;
    }

    static void GLFWGamepadDriverDestroy(HContext context, GamepadDriver* driver)
    {
        GLFWGamepadDriver* glfw_driver = (GLFWGamepadDriver*) driver;
        assert(g_GLFWGamepadDriver == glfw_driver);
        delete glfw_driver;
        g_GLFWGamepadDriver = 0;
    }

    GamepadDriver* CreateGamepadDriverGLFW(HContext context)
    {
        GLFWGamepadDriver* driver = new GLFWGamepadDriver();

        driver->m_Initialize                   = GLFWGamepadDriverInitialize;
        driver->m_Destroy                      = GLFWGamepadDriverDestroy;
        driver->m_Update                       = GLFWGamepadDriverUpdate;
        driver->m_DetectDevices                = GLFWGamepadDriverDetectDevices;
        driver->m_GetGamepadDeviceName         = GLFWGamepadDriverGetGamepadDeviceName;
        driver->m_GetGamepadDeviceGuid         = GLFWGamepadDriverGetGamepadDeviceGuid;
        driver->m_GetGamepadMappingSupport     = GLFWGamepadDriverGetGamepadMappingSupport;
        driver->m_SetGamepadMapping            = 0;

        assert(g_GLFWGamepadDriver == 0);
        g_GLFWGamepadDriver               = driver;
        g_GLFWGamepadDriver->m_HidContext = context;

        return driver;
    }
}
