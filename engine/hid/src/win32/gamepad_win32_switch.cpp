/*
  Copyright (C) 2026 The Defold Foundation

  This implementation is based on the Nintendo Switch HIDAPI driver in SDL: src/joystick/hidapi/SDL_hidapi_switch.c

  SDL's Switch driver includes code and logic contributed by Valve Corporation under the SDL zlib license.

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#include "gamepad_win32_private.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/safe_windows.h>

#include <hidsdi.h>
#include <hidpi.h>
#include <setupapi.h>

#include "../hid_private.h"

namespace dmHID
{
    static const uint32_t INVALID_INDEX = 0xffffffff;
    static const uint16_t NINTENDO_VENDOR_ID = 0x057e;
    static const uint16_t SWITCH_JOYCON_LEFT_PRODUCT_ID = 0x2006;
    static const uint16_t SWITCH_JOYCON_RIGHT_PRODUCT_ID = 0x2007;
    static const uint16_t SWITCH_PRO_PRODUCT_ID = 0x2009;
    static const uint16_t USB_BUS = 0x0003;
    static const uint16_t BLUETOOTH_BUS = 0x0005;
    static const uint32_t SWITCH_HID_MAX_DEVICES = MAX_GAMEPAD_COUNT;
    static const uint32_t SWITCH_HID_PATH_LENGTH = 512;
    static const uint32_t SWITCH_READ_BUFFER_SIZE = 512;
    static const uint32_t SWITCH_WRITE_BUFFER_SIZE = 128;
    static const uint32_t SWITCH_USB_REPORT_SIZE = 64;
    static const uint32_t SWITCH_BLUETOOTH_REPORT_SIZE = 49;
    static const uint64_t SWITCH_READ_FAILURE_DISCONNECT_TIMEOUT_MS = 2000;
    static const DWORD SWITCH_COMMAND_TIMEOUT_MS = 100;

    enum SwitchInputReportID
    {
        SWITCH_INPUT_REPORT_SUBCOMMAND_REPLY = 0x21,
        SWITCH_INPUT_REPORT_FULL_STATE = 0x30,
        SWITCH_INPUT_REPORT_FULL_STATE_MCU = 0x31,
        SWITCH_INPUT_REPORT_SIMPLE_STATE = 0x3f,
        SWITCH_INPUT_REPORT_COMMAND_ACK = 0x81,
    };

    enum SwitchOutputReportID
    {
        SWITCH_OUTPUT_REPORT_RUMBLE_AND_SUBCOMMAND = 0x01,
        SWITCH_OUTPUT_REPORT_PROPRIETARY = 0x80,
    };

    enum SwitchSubcommandID
    {
        SWITCH_SUBCOMMAND_SET_INPUT_REPORT_MODE = 0x03,
    };

    enum SwitchProprietaryCommandID
    {
        SWITCH_PROPRIETARY_HANDSHAKE = 0x02,
        SWITCH_PROPRIETARY_HIGH_SPEED = 0x03,
        SWITCH_PROPRIETARY_FORCE_USB = 0x04,
    };

    enum SwitchButtonIndex
    {
        SWITCH_BUTTON_MENU = 1,
        SWITCH_BUTTON_VIEW,
        SWITCH_BUTTON_A,
        SWITCH_BUTTON_B,
        SWITCH_BUTTON_X,
        SWITCH_BUTTON_Y,
        SWITCH_BUTTON_DPAD_UP,
        SWITCH_BUTTON_DPAD_DOWN,
        SWITCH_BUTTON_DPAD_LEFT,
        SWITCH_BUTTON_DPAD_RIGHT,
        SWITCH_BUTTON_LEFT_SHOULDER,
        SWITCH_BUTTON_RIGHT_SHOULDER,
        SWITCH_BUTTON_LEFT_STICK,
        SWITCH_BUTTON_RIGHT_STICK,
        SWITCH_BUTTON_CAPTURE,
        SWITCH_BUTTON_HOME,
        SWITCH_BUTTON_LEFT_SL,
        SWITCH_BUTTON_LEFT_SR,
        SWITCH_BUTTON_RIGHT_SL,
        SWITCH_BUTTON_RIGHT_SR,
        SWITCH_BUTTON_MAX
    };

    enum SwitchDeviceType
    {
        SWITCH_DEVICE_TYPE_UNKNOWN,
        SWITCH_DEVICE_TYPE_JOYCON_LEFT,
        SWITCH_DEVICE_TYPE_JOYCON_RIGHT,
        SWITCH_DEVICE_TYPE_PRO_CONTROLLER,
    };

    struct SwitchStickCalibrationAxis
    {
        int32_t m_Center;
        int32_t m_Min;
        int32_t m_Max;
    };

    struct SwitchHIDDevice
    {
        wchar_t                   m_Path[SWITCH_HID_PATH_LENGTH];
        HANDLE                    m_Handle;
        OVERLAPPED*               m_ReadOverlapped;
        uint8_t*                  m_ReadBuffer;
        uint16_t                  m_InputReportLength;
        uint16_t                  m_OutputReportLength;
        uint16_t                  m_FeatureReportLength;
        uint16_t                  m_UsagePage;
        uint16_t                  m_Usage;
        uint16_t                  m_ProductID;
        SwitchDeviceType          m_DeviceType;
        bool                      m_IsBluetooth;
        bool                      m_ReadPending;
        bool                      m_Seen;
        bool                      m_LoggedFirstPacket;
        uint64_t                  m_LastSeenTick;
        uint64_t                  m_LastPacketTick;
        uint64_t                  m_FirstReadFailureTick;
        uint8_t                   m_CommandNumber;
        Gamepad*                  m_Gamepad;
        GamepadGuid               m_Guid;
        SwitchStickCalibrationAxis m_StickCalibration[2][2];
    };

    struct SwitchHIDContext
    {
        HContext        m_HidContext;
        SwitchHIDDevice m_Devices[SWITCH_HID_MAX_DEVICES];
        wchar_t         m_FailedPaths[SWITCH_HID_MAX_DEVICES][SWITCH_HID_PATH_LENGTH];
        uint32_t        m_FailedPathCount;
    };

    struct SwitchHIDDeviceInfo
    {
        uint16_t m_VendorID;
        uint16_t m_ProductID;
        uint16_t m_InputReportLength;
        uint16_t m_OutputReportLength;
        uint16_t m_FeatureReportLength;
        uint16_t m_UsagePage;
        uint16_t m_Usage;
        SwitchDeviceType m_DeviceType;
    };

    static SwitchHIDContext* g_SwitchHIDContext = 0;

    struct SwitchWriteOperation
    {
        OVERLAPPED m_Overlapped;
        uint8_t    m_Output[SWITCH_WRITE_BUFFER_SIZE];
    };

    struct SwitchReadOperation
    {
        OVERLAPPED m_Overlapped;
        uint8_t    m_Report[SWITCH_READ_BUFFER_SIZE];
    };

    static void SetButton(GamepadPacket& packet, uint32_t button, bool pressed)
    {
        if (button >= MAX_GAMEPAD_BUTTON_COUNT)
        {
            return;
        }

        if (pressed)
        {
            packet.m_Buttons[button / 32] |= 1 << (button % 32);
        }
        else
        {
            packet.m_Buttons[button / 32] &= ~(1 << (button % 32));
        }
    }

    static uint8_t DPadToHat(bool up, bool down, bool left, bool right)
    {
        if (up && right) return 2;
        if (down && right) return 4;
        if (down && left) return 6;
        if (up && left) return 8;
        if (up) return 1;
        if (right) return 3;
        if (down) return 5;
        if (left) return 7;
        return 0;
    }

    static float NormalizeStickAxis(const SwitchStickCalibrationAxis& calibration, int32_t raw)
    {
        int32_t delta = raw - calibration.m_Center;
        int32_t extent = delta >= 0 ? calibration.m_Max : calibration.m_Min;
        if (extent <= 0)
        {
            return 0.0f;
        }

        float value = (float) delta / (float) extent;
        if (value > 1.0f) return 1.0f;
        if (value < -1.0f) return -1.0f;
        return value;
    }

    static void SetDefaultStickCalibration(SwitchHIDDevice* device)
    {
        for (uint32_t stick = 0; stick < 2; ++stick)
        {
            for (uint32_t axis = 0; axis < 2; ++axis)
            {
                device->m_StickCalibration[stick][axis].m_Center = 2048;
                device->m_StickCalibration[stick][axis].m_Min = 1600;
                device->m_StickCalibration[stick][axis].m_Max = 1600;
            }
        }
    }

    static uint16_t ReadSwitchStickAxisX(const uint8_t* data)
    {
        return (uint16_t) (data[0] | ((data[1] & 0x0f) << 8));
    }

    static uint16_t ReadSwitchStickAxisY(const uint8_t* data)
    {
        return (uint16_t) (((data[1] & 0xf0) >> 4) | (data[2] << 4));
    }

    static const char* GetSwitchDeviceName(SwitchDeviceType device_type)
    {
        switch (device_type)
        {
            case SWITCH_DEVICE_TYPE_JOYCON_LEFT:      return "Nintendo Switch Joy-Con (L)";
            case SWITCH_DEVICE_TYPE_JOYCON_RIGHT:     return "Nintendo Switch Joy-Con (R)";
            case SWITCH_DEVICE_TYPE_PRO_CONTROLLER:   return "Nintendo Switch Pro Controller";
            default:                                  return "Nintendo Switch Controller";
        }
    }

    static SwitchDeviceType GetSwitchDeviceType(uint16_t product_id)
    {
        switch (product_id)
        {
            case SWITCH_JOYCON_LEFT_PRODUCT_ID:   return SWITCH_DEVICE_TYPE_JOYCON_LEFT;
            case SWITCH_JOYCON_RIGHT_PRODUCT_ID:  return SWITCH_DEVICE_TYPE_JOYCON_RIGHT;
            case SWITCH_PRO_PRODUCT_ID:           return SWITCH_DEVICE_TYPE_PRO_CONTROLLER;
            default:                              return SWITCH_DEVICE_TYPE_UNKNOWN;
        }
    }

    static bool IsSupportedSwitchDevice(uint16_t vendor_id, uint16_t product_id)
    {
        return vendor_id == NINTENDO_VENDOR_ID && GetSwitchDeviceType(product_id) != SWITCH_DEVICE_TYPE_UNKNOWN;
    }

    static void ClearGamepadPacket(GamepadPacket& packet)
    {
        memset(packet.m_Axis, 0, sizeof(packet.m_Axis));
        memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
        memset(packet.m_Hat, 0, sizeof(packet.m_Hat));
    }

    static void ParseProFullStatePacket(SwitchHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        if (report_size < 13 || device->m_Gamepad == 0)
        {
            return;
        }

        Gamepad* gamepad = device->m_Gamepad;
        GamepadPacket& packet = gamepad->m_Packet;

        ClearGamepadPacket(packet);

        const uint8_t buttons_right = report[3];
        const uint8_t buttons_shared = report[4];
        const uint8_t buttons_left = report[5];
        const uint8_t* left_stick = &report[6];
        const uint8_t* right_stick = &report[9];

        SetButton(packet, SWITCH_BUTTON_B, (buttons_right & 0x04) != 0);
        SetButton(packet, SWITCH_BUTTON_A, (buttons_right & 0x08) != 0);
        SetButton(packet, SWITCH_BUTTON_Y, (buttons_right & 0x01) != 0);
        SetButton(packet, SWITCH_BUTTON_X, (buttons_right & 0x02) != 0);
        SetButton(packet, SWITCH_BUTTON_RIGHT_SHOULDER, (buttons_right & 0x40) != 0);

        SetButton(packet, SWITCH_BUTTON_VIEW, (buttons_shared & 0x01) != 0);
        SetButton(packet, SWITCH_BUTTON_MENU, (buttons_shared & 0x02) != 0);
        SetButton(packet, SWITCH_BUTTON_RIGHT_STICK, (buttons_shared & 0x04) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_STICK, (buttons_shared & 0x08) != 0);
        SetButton(packet, SWITCH_BUTTON_CAPTURE, (buttons_shared & 0x20) != 0);

        const bool dpad_down = (buttons_left & 0x01) != 0;
        const bool dpad_up = (buttons_left & 0x02) != 0;
        const bool dpad_right = (buttons_left & 0x04) != 0;
        const bool dpad_left = (buttons_left & 0x08) != 0;
        SetButton(packet, SWITCH_BUTTON_DPAD_UP, dpad_up);
        SetButton(packet, SWITCH_BUTTON_DPAD_DOWN, dpad_down);
        SetButton(packet, SWITCH_BUTTON_DPAD_LEFT, dpad_left);
        SetButton(packet, SWITCH_BUTTON_DPAD_RIGHT, dpad_right);
        SetButton(packet, SWITCH_BUTTON_LEFT_SHOULDER, (buttons_left & 0x40) != 0);

        packet.m_Hat[0] = DPadToHat(dpad_up, dpad_down, dpad_left, dpad_right);

        packet.m_Axis[0] = NormalizeStickAxis(device->m_StickCalibration[0][0], ReadSwitchStickAxisX(left_stick));
        packet.m_Axis[1] = -NormalizeStickAxis(device->m_StickCalibration[0][1], ReadSwitchStickAxisY(left_stick));
        packet.m_Axis[2] = NormalizeStickAxis(device->m_StickCalibration[1][0], ReadSwitchStickAxisX(right_stick));
        packet.m_Axis[3] = -NormalizeStickAxis(device->m_StickCalibration[1][1], ReadSwitchStickAxisY(right_stick));
        packet.m_Axis[4] = (buttons_left & 0x80) != 0 ? 1.0f : 0.0f;
        packet.m_Axis[5] = (buttons_right & 0x80) != 0 ? 1.0f : 0.0f;

        gamepad->m_AxisCount = 6;
        gamepad->m_ButtonCount = SWITCH_BUTTON_MAX;
        gamepad->m_HatCount = 1;

        if (!device->m_LoggedFirstPacket)
        {
            device->m_LoggedFirstPacket = true;
            dmLogInfo("Switch Pro HID first state: buttons=%02x/%02x/%02x left=%u/%u right=%u/%u",
                buttons_right,
                buttons_shared,
                buttons_left,
                ReadSwitchStickAxisX(left_stick),
                ReadSwitchStickAxisY(left_stick),
                ReadSwitchStickAxisX(right_stick),
                ReadSwitchStickAxisY(right_stick));
        }
    }

    static void ParseJoyConLeftFullStatePacket(SwitchHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        if (report_size < 13 || device->m_Gamepad == 0)
        {
            return;
        }

        Gamepad* gamepad = device->m_Gamepad;
        GamepadPacket& packet = gamepad->m_Packet;

        ClearGamepadPacket(packet);

        const uint8_t buttons_shared = report[4];
        const uint8_t buttons_left = report[5];
        const uint8_t* left_stick = &report[6];

        SetButton(packet, SWITCH_BUTTON_MENU, (buttons_shared & 0x01) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_STICK, (buttons_shared & 0x08) != 0);
        SetButton(packet, SWITCH_BUTTON_CAPTURE, (buttons_shared & 0x20) != 0);

        SetButton(packet, SWITCH_BUTTON_B, (buttons_left & 0x08) != 0);
        SetButton(packet, SWITCH_BUTTON_A, (buttons_left & 0x01) != 0);
        SetButton(packet, SWITCH_BUTTON_Y, (buttons_left & 0x02) != 0);
        SetButton(packet, SWITCH_BUTTON_X, (buttons_left & 0x04) != 0);
        SetButton(packet, SWITCH_BUTTON_RIGHT_SHOULDER, (buttons_left & 0x10) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_SHOULDER, (buttons_left & 0x20) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_SL, (buttons_left & 0x40) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_SR, (buttons_left & 0x80) != 0);

        packet.m_Axis[0] = -NormalizeStickAxis(device->m_StickCalibration[0][1], ReadSwitchStickAxisY(left_stick));
        packet.m_Axis[1] = -NormalizeStickAxis(device->m_StickCalibration[0][0], ReadSwitchStickAxisX(left_stick));

        gamepad->m_AxisCount = 2;
        gamepad->m_ButtonCount = SWITCH_BUTTON_MAX;
        gamepad->m_HatCount = 0;

        if (!device->m_LoggedFirstPacket)
        {
            device->m_LoggedFirstPacket = true;
            dmLogInfo("Switch Joy-Con (L) HID first state: buttons=%02x/%02x left=%u/%u",
                buttons_shared,
                buttons_left,
                ReadSwitchStickAxisX(left_stick),
                ReadSwitchStickAxisY(left_stick));
        }
    }

    static void ParseJoyConRightFullStatePacket(SwitchHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        if (report_size < 13 || device->m_Gamepad == 0)
        {
            return;
        }

        Gamepad* gamepad = device->m_Gamepad;
        GamepadPacket& packet = gamepad->m_Packet;

        ClearGamepadPacket(packet);

        const uint8_t buttons_right = report[3];
        const uint8_t buttons_shared = report[4];
        const uint8_t* right_stick = &report[9];

        SetButton(packet, SWITCH_BUTTON_MENU, (buttons_shared & 0x02) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_STICK, (buttons_shared & 0x04) != 0);
        SetButton(packet, SWITCH_BUTTON_HOME, (buttons_shared & 0x10) != 0);

        SetButton(packet, SWITCH_BUTTON_B, (buttons_right & 0x08) != 0);
        SetButton(packet, SWITCH_BUTTON_A, (buttons_right & 0x02) != 0);
        SetButton(packet, SWITCH_BUTTON_Y, (buttons_right & 0x04) != 0);
        SetButton(packet, SWITCH_BUTTON_X, (buttons_right & 0x01) != 0);
        SetButton(packet, SWITCH_BUTTON_RIGHT_SHOULDER, (buttons_right & 0x10) != 0);
        SetButton(packet, SWITCH_BUTTON_LEFT_SHOULDER, (buttons_right & 0x20) != 0);
        SetButton(packet, SWITCH_BUTTON_RIGHT_SL, (buttons_right & 0x40) != 0);
        SetButton(packet, SWITCH_BUTTON_RIGHT_SR, (buttons_right & 0x80) != 0);

        packet.m_Axis[0] = NormalizeStickAxis(device->m_StickCalibration[1][1], ReadSwitchStickAxisY(right_stick));
        packet.m_Axis[1] = NormalizeStickAxis(device->m_StickCalibration[1][0], ReadSwitchStickAxisX(right_stick));

        gamepad->m_AxisCount = 2;
        gamepad->m_ButtonCount = SWITCH_BUTTON_MAX;
        gamepad->m_HatCount = 0;

        if (!device->m_LoggedFirstPacket)
        {
            device->m_LoggedFirstPacket = true;
            dmLogInfo("Switch Joy-Con (R) HID first state: buttons=%02x/%02x right=%u/%u",
                buttons_right,
                buttons_shared,
                ReadSwitchStickAxisX(right_stick),
                ReadSwitchStickAxisY(right_stick));
        }
    }

    static void ParseFullStatePacket(SwitchHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        device->m_LastPacketTick = GetTickCount64();
        device->m_FirstReadFailureTick = 0;

        switch (device->m_DeviceType)
        {
            case SWITCH_DEVICE_TYPE_JOYCON_LEFT:
                ParseJoyConLeftFullStatePacket(device, report, report_size);
                break;
            case SWITCH_DEVICE_TYPE_JOYCON_RIGHT:
                ParseJoyConRightFullStatePacket(device, report, report_size);
                break;
            case SWITCH_DEVICE_TYPE_PRO_CONTROLLER:
            default:
                ParseProFullStatePacket(device, report, report_size);
                break;
        }
    }

    static uint32_t GetInputReportLength(SwitchHIDDevice* device)
    {
        if (device->m_InputReportLength > 0 && device->m_InputReportLength <= SWITCH_READ_BUFFER_SIZE)
        {
            return device->m_InputReportLength;
        }
        return SWITCH_USB_REPORT_SIZE;
    }

    static uint32_t GetOutputPacketLength(SwitchHIDDevice* device)
    {
        return device->m_IsBluetooth ? SWITCH_BLUETOOTH_REPORT_SIZE : SWITCH_USB_REPORT_SIZE;
    }

    static void CancelOverlappedOperation(HANDLE handle, OVERLAPPED* overlapped)
    {
        typedef BOOL (WINAPI *CancelIoExFn)(HANDLE, LPOVERLAPPED);
        HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
        CancelIoExFn cancel_io_ex = kernel32 != 0 ? (CancelIoExFn) GetProcAddress(kernel32, "CancelIoEx") : 0;
        if (cancel_io_ex != 0)
        {
            cancel_io_ex(handle, overlapped);
        }
        else
        {
            CancelIo(handle);
        }
    }

    static bool CompleteOverlappedOperation(HANDLE handle, OVERLAPPED* overlapped, DWORD timeout_ms, DWORD* bytes_transferred, DWORD* last_error, BOOL* result)
    {
        DWORD wait_result = WaitForSingleObject(overlapped->hEvent, timeout_ms);
        if (wait_result != WAIT_OBJECT_0)
        {
            *result = FALSE;
            *last_error = wait_result == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
            return false;
        }

        *result = GetOverlappedResult(handle, overlapped, bytes_transferred, FALSE);
        *last_error = *result ? ERROR_SUCCESS : GetLastError();
        return *last_error != ERROR_IO_INCOMPLETE;
    }

    static bool CancelAndCompleteOverlappedOperation(HANDLE handle, OVERLAPPED* overlapped, DWORD timeout_ms, DWORD* bytes_transferred, DWORD* last_error, BOOL* result)
    {
        CancelOverlappedOperation(handle, overlapped);
        return CompleteOverlappedOperation(handle, overlapped, timeout_ms, bytes_transferred, last_error, result);
    }

    static bool WriteReport(SwitchHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        uint32_t write_size = report_size;
        if (device->m_OutputReportLength > write_size)
        {
            write_size = device->m_OutputReportLength;
        }

        if (write_size > SWITCH_WRITE_BUFFER_SIZE)
        {
            dmLogWarning("Switch HID write failed: output report too large (%u)", write_size);
            return false;
        }

        SwitchWriteOperation* operation = new SwitchWriteOperation();
        memset(operation, 0, sizeof(*operation));
        memcpy(operation->m_Output, report, report_size);

        DWORD bytes_written = 0;
        operation->m_Overlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (operation->m_Overlapped.hEvent == 0)
        {
            delete operation;
            return false;
        }

        DWORD last_error = ERROR_SUCCESS;
        bool completed = true;
        BOOL result = WriteFile(device->m_Handle, operation->m_Output, write_size, &bytes_written, &operation->m_Overlapped);
        if (!result)
        {
            last_error = GetLastError();
            if (last_error == ERROR_IO_PENDING)
            {
                completed = CompleteOverlappedOperation(device->m_Handle, &operation->m_Overlapped, SWITCH_COMMAND_TIMEOUT_MS, &bytes_written, &last_error, &result);
                if (!completed)
                {
                    completed = CancelAndCompleteOverlappedOperation(device->m_Handle, &operation->m_Overlapped, SWITCH_COMMAND_TIMEOUT_MS, &bytes_written, &last_error, &result);
                    if (!completed)
                    {
                        dmLogWarning("Switch HID write cancellation still pending: report=0x%02x output_report=%u", report[0], device->m_OutputReportLength);
                    }
                }
            }
        }

        if (completed)
        {
            CloseHandle(operation->m_Overlapped.hEvent);
            delete operation;
        }

        if (!result || bytes_written != write_size)
        {
            dmLogWarning("Switch HID write failed: report=0x%02x requested=%u wrote=%u error=%lu output_report=%u",
                report[0],
                write_size,
                bytes_written,
                last_error,
                device->m_OutputReportLength);
            return false;
        }

        return true;
    }

    static bool ReadReportBlocking(SwitchHIDDevice* device, uint8_t* report, uint32_t report_size, DWORD timeout_ms, DWORD* bytes_read)
    {
        *bytes_read = 0;

        SwitchReadOperation* operation = new SwitchReadOperation();
        memset(operation, 0, sizeof(*operation));
        operation->m_Overlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (operation->m_Overlapped.hEvent == 0)
        {
            delete operation;
            return false;
        }

        DWORD last_error = ERROR_SUCCESS;
        bool completed = true;
        BOOL result = ReadFile(device->m_Handle, operation->m_Report, report_size, bytes_read, &operation->m_Overlapped);
        if (!result)
        {
            last_error = GetLastError();
            if (last_error == ERROR_IO_PENDING)
            {
                completed = CompleteOverlappedOperation(device->m_Handle, &operation->m_Overlapped, timeout_ms, bytes_read, &last_error, &result);
                if (!completed)
                {
                    completed = CancelAndCompleteOverlappedOperation(device->m_Handle, &operation->m_Overlapped, SWITCH_COMMAND_TIMEOUT_MS, bytes_read, &last_error, &result);
                    if (!completed)
                    {
                        dmLogWarning("Switch HID read cancellation still pending: input_report=%u", report_size);
                    }
                }
            }
        }

        if (result && *bytes_read > 0)
        {
            memcpy(report, operation->m_Report, *bytes_read);
        }

        if (completed)
        {
            CloseHandle(operation->m_Overlapped.hEvent);
            delete operation;
        }

        return result != FALSE;
    }

    static bool ReadCommandAck(SwitchHIDDevice* device, uint8_t expected_report, uint8_t expected_command)
    {
        uint8_t report[SWITCH_READ_BUFFER_SIZE] = {};
        DWORD bytes_read = 0;
        uint32_t attempts = 0;
        uint32_t read_failures = 0;
        uint8_t last_report_id = 0;
        uint8_t last_report_command = 0;
        DWORD last_bytes_read = 0;
        uint32_t report_size = GetInputReportLength(device);

        while (attempts++ < 8)
        {
            if (!ReadReportBlocking(device, report, report_size, SWITCH_COMMAND_TIMEOUT_MS, &bytes_read))
            {
                ++read_failures;
                continue;
            }

            if (bytes_read == 0)
            {
                continue;
            }

            last_bytes_read = bytes_read;
            last_report_id = report[0];
            last_report_command = bytes_read >= 2 ? report[1] : 0;

            if (expected_report == SWITCH_INPUT_REPORT_COMMAND_ACK)
            {
                if (bytes_read >= 2 && report[0] == SWITCH_INPUT_REPORT_COMMAND_ACK && report[1] == expected_command)
                {
                    return true;
                }
            }
            else if (expected_report == SWITCH_INPUT_REPORT_SUBCOMMAND_REPLY)
            {
                if (bytes_read >= 15 &&
                    report[0] == SWITCH_INPUT_REPORT_SUBCOMMAND_REPLY &&
                    (report[13] & 0x80) != 0 &&
                    report[14] == expected_command)
                {
                    return true;
                }
            }

            if (report[0] == SWITCH_INPUT_REPORT_FULL_STATE || report[0] == SWITCH_INPUT_REPORT_FULL_STATE_MCU)
            {
                ParseFullStatePacket(device, report, bytes_read);
            }
        }

        dmLogWarning("Switch HID ack timeout: expected_report=0x%02x expected_command=0x%02x reads=%u failures=%u last_report=0x%02x last_command=0x%02x last_size=%lu input_report=%u",
            expected_report,
            expected_command,
            attempts - 1,
            read_failures,
            last_report_id,
            last_report_command,
            last_bytes_read,
            report_size);
        return false;
    }

    static bool WriteProprietaryCommand(SwitchHIDDevice* device, uint8_t command, bool wait_for_reply)
    {
        uint8_t report[SWITCH_WRITE_BUFFER_SIZE] = {};
        report[0] = SWITCH_OUTPUT_REPORT_PROPRIETARY;
        report[1] = command;

        if (!WriteReport(device, report, GetOutputPacketLength(device)))
        {
            return false;
        }

        return !wait_for_reply || ReadCommandAck(device, SWITCH_INPUT_REPORT_COMMAND_ACK, command);
    }

    static bool WriteSubcommand(SwitchHIDDevice* device, uint8_t command, const uint8_t* data, uint32_t data_size)
    {
        uint8_t report[SWITCH_WRITE_BUFFER_SIZE] = {};
        report[0] = SWITCH_OUTPUT_REPORT_RUMBLE_AND_SUBCOMMAND;
        report[1] = device->m_CommandNumber;
        device->m_CommandNumber = (device->m_CommandNumber + 1) & 0x0f;

        // Neutral rumble values, matching SDL's default neutral packet.
        report[2] = 0x00; report[3] = 0x01; report[4] = 0x40; report[5] = 0x40;
        report[6] = 0x00; report[7] = 0x01; report[8] = 0x40; report[9] = 0x40;
        report[10] = command;

        if (data != 0 && data_size > 0)
        {
            if (data_size > sizeof(report) - 11)
            {
                return false;
            }
            memcpy(&report[11], data, data_size);
        }

        if (!WriteReport(device, report, GetOutputPacketLength(device)))
        {
            return false;
        }

        return ReadCommandAck(device, SWITCH_INPUT_REPORT_SUBCOMMAND_REPLY, command);
    }

    static bool InitializeSwitchUSBController(SwitchHIDDevice* device)
    {
        if (!WriteProprietaryCommand(device, SWITCH_PROPRIETARY_HANDSHAKE, true))
        {
            return false;
        }

        // Some compatible controllers do not ack this, so match SDL and tolerate failure.
        WriteProprietaryCommand(device, SWITCH_PROPRIETARY_HIGH_SPEED, true);
        WriteProprietaryCommand(device, SWITCH_PROPRIETARY_HANDSHAKE, true);

        if (!WriteProprietaryCommand(device, SWITCH_PROPRIETARY_FORCE_USB, false))
        {
            return false;
        }

        return true;
    }

    static bool InitializeSwitchBluetoothController(SwitchHIDDevice* device)
    {
        uint8_t input_mode = SWITCH_INPUT_REPORT_FULL_STATE;
        if (!WriteSubcommand(device, SWITCH_SUBCOMMAND_SET_INPUT_REPORT_MODE, &input_mode, sizeof(input_mode)))
        {
            return false;
        }
        return true;
    }

    static bool InitializeSwitchController(SwitchHIDDevice* device)
    {
        SetDefaultStickCalibration(device);

        if (!device->m_IsBluetooth && InitializeSwitchUSBController(device))
        {
            uint8_t input_mode = SWITCH_INPUT_REPORT_FULL_STATE;
            if (!WriteSubcommand(device, SWITCH_SUBCOMMAND_SET_INPUT_REPORT_MODE, &input_mode, sizeof(input_mode)))
            {
                dmLogWarning("Switch HID init failed: set USB input report mode 0x%02x", input_mode);
                return false;
            }

            WriteProprietaryCommand(device, SWITCH_PROPRIETARY_FORCE_USB, false);
            return true;
        }

        if (!device->m_IsBluetooth)
        {
            dmLogInfo("Switch HID USB setup did not respond; trying Bluetooth setup");
            device->m_IsBluetooth = true;
        }

        if (!InitializeSwitchBluetoothController(device))
        {
            dmLogWarning("Switch HID init failed: set Bluetooth input report mode");
            return false;
        }

        return true;
    }

    static bool BeginAsyncRead(SwitchHIDDevice* device)
    {
        if (device->m_ReadPending)
        {
            return true;
        }

        if (device->m_ReadOverlapped == 0 || device->m_ReadOverlapped->hEvent == 0 || device->m_ReadBuffer == 0)
        {
            return false;
        }

        ResetEvent(device->m_ReadOverlapped->hEvent);
        DWORD bytes_read = 0;
        BOOL result = ReadFile(device->m_Handle, device->m_ReadBuffer, GetInputReportLength(device), &bytes_read, device->m_ReadOverlapped);
        if (result)
        {
            if (bytes_read > 0)
            {
                ParseFullStatePacket(device, device->m_ReadBuffer, bytes_read);
            }
            return true;
        }

        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING)
        {
            device->m_ReadPending = true;
            return true;
        }

        return false;
    }

    static bool PollAsyncRead(SwitchHIDDevice* device)
    {
        if (!device->m_ReadPending)
        {
            return BeginAsyncRead(device);
        }

        DWORD bytes_read = 0;
        if (!GetOverlappedResult(device->m_Handle, device->m_ReadOverlapped, &bytes_read, FALSE))
        {
            DWORD error = GetLastError();
            if (error == ERROR_IO_INCOMPLETE)
            {
                return true;
            }

            device->m_ReadPending = false;
            return BeginAsyncRead(device);
        }

        device->m_ReadPending = false;
        if (bytes_read > 0)
        {
            ParseFullStatePacket(device, device->m_ReadBuffer, bytes_read);
        }

        return BeginAsyncRead(device);
    }

    static void CloseSwitchDevice(SwitchHIDDevice* device, HContext context)
    {
        OVERLAPPED* read_overlapped = device->m_ReadOverlapped;
        uint8_t* read_buffer = device->m_ReadBuffer;
        bool free_read_operation = true;

        if (device->m_Handle != INVALID_HANDLE_VALUE && device->m_Handle != 0)
        {
            if (device->m_ReadPending && read_overlapped != 0)
            {
                DWORD bytes_read = 0;
                DWORD last_error = ERROR_SUCCESS;
                BOOL result = FALSE;
                free_read_operation = CancelAndCompleteOverlappedOperation(device->m_Handle, read_overlapped, SWITCH_COMMAND_TIMEOUT_MS, &bytes_read, &last_error, &result);
                if (!free_read_operation)
                {
                    dmLogWarning("Switch HID async read cancellation still pending");
                }
                device->m_ReadPending = false;
            }
            CloseHandle(device->m_Handle);
        }

        if (free_read_operation)
        {
            if (read_overlapped != 0)
            {
                if (read_overlapped->hEvent != 0)
                {
                    CloseHandle(read_overlapped->hEvent);
                }
                delete read_overlapped;
            }
            delete[] read_buffer;
        }

        if (device->m_Gamepad != 0)
        {
            SetGamepadConnectionStatus(context, device->m_Gamepad, false);
            ReleaseGamepad(context, device->m_Gamepad);
        }

        memset(device, 0, sizeof(*device));
    }

    static uint32_t FindDeviceByPath(SwitchHIDContext* context, const wchar_t* path)
    {
        for (uint32_t i = 0; i < SWITCH_HID_MAX_DEVICES; ++i)
        {
            if (context->m_Devices[i].m_Handle != 0 && wcscmp(context->m_Devices[i].m_Path, path) == 0)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static uint32_t FindFreeDeviceIndex(SwitchHIDContext* context)
    {
        for (uint32_t i = 0; i < SWITCH_HID_MAX_DEVICES; ++i)
        {
            if (context->m_Devices[i].m_Handle == 0)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static uint32_t FindDeviceByGamepad(SwitchHIDContext* context, Gamepad* gamepad)
    {
        for (uint32_t i = 0; i < SWITCH_HID_MAX_DEVICES; ++i)
        {
            if (context->m_Devices[i].m_Gamepad == gamepad)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static bool IsBluetoothPath(const wchar_t* path)
    {
        return wcsstr(path, L"bth") != 0 ||
            wcsstr(path, L"BTH") != 0 ||
            wcsstr(path, L"bluetooth") != 0 ||
            wcsstr(path, L"BLUETOOTH") != 0;
    }

    static bool IsBluetoothReportShape(const SwitchHIDDeviceInfo& info)
    {
        return (info.m_OutputReportLength >= SWITCH_BLUETOOTH_REPORT_SIZE && info.m_OutputReportLength < SWITCH_USB_REPORT_SIZE) ||
            info.m_InputReportLength > SWITCH_USB_REPORT_SIZE * 2;
    }

    static bool IsFailedPath(SwitchHIDContext* context, const wchar_t* path)
    {
        for (uint32_t i = 0; i < context->m_FailedPathCount; ++i)
        {
            if (wcscmp(context->m_FailedPaths[i], path) == 0)
            {
                return true;
            }
        }
        return false;
    }

    static void AddFailedPath(SwitchHIDContext* context, const wchar_t* path)
    {
        if (IsFailedPath(context, path) || context->m_FailedPathCount >= SWITCH_HID_MAX_DEVICES)
        {
            return;
        }
        wcsncpy_s(context->m_FailedPaths[context->m_FailedPathCount], SWITCH_HID_PATH_LENGTH, path, _TRUNCATE);
        ++context->m_FailedPathCount;
    }

    static bool QuerySwitchHIDDeviceInfo(HANDLE handle, SwitchHIDDeviceInfo* info)
    {
        memset(info, 0, sizeof(*info));

        HIDD_ATTRIBUTES attributes = {};
        attributes.Size = sizeof(attributes);
        if (!HidD_GetAttributes(handle, &attributes))
        {
            return false;
        }

        info->m_VendorID = attributes.VendorID;
        info->m_ProductID = attributes.ProductID;
        info->m_DeviceType = GetSwitchDeviceType(attributes.ProductID);

        PHIDP_PREPARSED_DATA preparsed_data = 0;
        if (HidD_GetPreparsedData(handle, &preparsed_data))
        {
            HIDP_CAPS caps = {};
            if (HidP_GetCaps(preparsed_data, &caps) == HIDP_STATUS_SUCCESS)
            {
                info->m_InputReportLength = caps.InputReportByteLength;
                info->m_OutputReportLength = caps.OutputReportByteLength;
                info->m_FeatureReportLength = caps.FeatureReportByteLength;
                info->m_UsagePage = caps.UsagePage;
                info->m_Usage = caps.Usage;
            }
            HidD_FreePreparsedData(preparsed_data);
        }

        return true;
    }

    static bool IsSwitchControllerPath(const wchar_t* path, SwitchHIDDeviceInfo* info)
    {
        HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        bool result = QuerySwitchHIDDeviceInfo(handle, info) &&
            IsSupportedSwitchDevice(info->m_VendorID, info->m_ProductID);

        CloseHandle(handle);
        return result;
    }

    static bool OpenSwitchDevice(SwitchHIDContext* context, const wchar_t* path, const SwitchHIDDeviceInfo& info)
    {
        uint32_t index = FindFreeDeviceIndex(context);
        if (index == INVALID_INDEX)
        {
            return false;
        }

        HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        SwitchHIDDevice* device = &context->m_Devices[index];
        memset(device, 0, sizeof(*device));
        device->m_Handle = handle;
        device->m_InputReportLength = info.m_InputReportLength;
        device->m_OutputReportLength = info.m_OutputReportLength;
        device->m_FeatureReportLength = info.m_FeatureReportLength;
        device->m_UsagePage = info.m_UsagePage;
        device->m_Usage = info.m_Usage;
        device->m_ProductID = info.m_ProductID;
        device->m_DeviceType = info.m_DeviceType;
        device->m_IsBluetooth = IsBluetoothPath(path) ||
            IsBluetoothReportShape(info);
        device->m_LastSeenTick = GetTickCount64();
        wcsncpy_s(device->m_Path, SWITCH_HID_PATH_LENGTH, path, _TRUNCATE);

        dmLogInfo("%s HID candidate: usage=0x%04x/0x%04x reports input=%u output=%u feature=%u bus=%s",
            GetSwitchDeviceName(device->m_DeviceType),
            device->m_UsagePage,
            device->m_Usage,
            device->m_InputReportLength,
            device->m_OutputReportLength,
            device->m_FeatureReportLength,
            device->m_IsBluetooth ? "bluetooth" : "unknown/usb");

        if (device->m_OutputReportLength != 0 && device->m_OutputReportLength < GetOutputPacketLength(device))
        {
            dmLogWarning("Skipping %s HID candidate: output report too small (%u)", GetSwitchDeviceName(device->m_DeviceType), device->m_OutputReportLength);
            AddFailedPath(context, path);
            CloseSwitchDevice(device, context->m_HidContext);
            return false;
        }
        if (device->m_InputReportLength > SWITCH_READ_BUFFER_SIZE || device->m_OutputReportLength > SWITCH_WRITE_BUFFER_SIZE)
        {
            dmLogWarning("Skipping %s HID candidate: report length unsupported input=%u output=%u",
                GetSwitchDeviceName(device->m_DeviceType),
                device->m_InputReportLength,
                device->m_OutputReportLength);
            AddFailedPath(context, path);
            CloseSwitchDevice(device, context->m_HidContext);
            return false;
        }

        device->m_ReadBuffer = new uint8_t[SWITCH_READ_BUFFER_SIZE];
        memset(device->m_ReadBuffer, 0, SWITCH_READ_BUFFER_SIZE);
        device->m_ReadOverlapped = new OVERLAPPED();
        memset(device->m_ReadOverlapped, 0, sizeof(*device->m_ReadOverlapped));
        device->m_ReadOverlapped->hEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (device->m_ReadOverlapped->hEvent == 0)
        {
            CloseSwitchDevice(device, context->m_HidContext);
            return false;
        }

        HidD_SetNumInputBuffers(handle, 64);

        if (!InitializeSwitchController(device))
        {
            dmLogWarning("Failed to initialize %s HID fallback", GetSwitchDeviceName(device->m_DeviceType));
            CloseSwitchDevice(device, context->m_HidContext);
            return false;
        }

        device->m_Gamepad = CreateGamepad(context->m_HidContext);
        if (device->m_Gamepad == 0)
        {
            CloseSwitchDevice(device, context->m_HidContext);
            return false;
        }

        const char* device_name = GetSwitchDeviceName(device->m_DeviceType);
        device->m_Guid = CreateGUID(device->m_IsBluetooth ? BLUETOOTH_BUS : USB_BUS, NINTENDO_VENDOR_ID, device->m_ProductID, 0, 0, device_name, 0, 0);
        device->m_Gamepad->m_AxisCount = device->m_DeviceType == SWITCH_DEVICE_TYPE_PRO_CONTROLLER ? 6 : 2;
        device->m_Gamepad->m_ButtonCount = SWITCH_BUTTON_MAX;
        device->m_Gamepad->m_HatCount = device->m_DeviceType == SWITCH_DEVICE_TYPE_PRO_CONTROLLER ? 1 : 0;
        SetGamepadConnectionStatus(context->m_HidContext, device->m_Gamepad, true);

        BeginAsyncRead(device);

        dmLogInfo("%s HID fallback connected bus=%s", device_name, device->m_IsBluetooth ? "bluetooth" : "usb");
        return true;
    }

    bool GamepadSwitchInitialize(HContext context)
    {
        assert(g_SwitchHIDContext == 0);
        g_SwitchHIDContext = new SwitchHIDContext();
        memset(g_SwitchHIDContext, 0, sizeof(*g_SwitchHIDContext));
        g_SwitchHIDContext->m_HidContext = context;
        return true;
    }

    void GamepadSwitchFinalize(HContext context)
    {
        (void) context;
        if (g_SwitchHIDContext == 0)
        {
            return;
        }

        for (uint32_t i = 0; i < SWITCH_HID_MAX_DEVICES; ++i)
        {
            if (g_SwitchHIDContext->m_Devices[i].m_Handle != 0)
            {
                CloseSwitchDevice(&g_SwitchHIDContext->m_Devices[i], g_SwitchHIDContext->m_HidContext);
            }
        }

        delete g_SwitchHIDContext;
        g_SwitchHIDContext = 0;
    }

    void GamepadSwitchDetectDevices(HContext context)
    {
        (void) context;
        SwitchHIDContext* switch_context = g_SwitchHIDContext;
        if (switch_context == 0)
        {
            return;
        }

        for (uint32_t i = 0; i < SWITCH_HID_MAX_DEVICES; ++i)
        {
            switch_context->m_Devices[i].m_Seen = false;
        }

        GUID hid_guid;
        HidD_GetHidGuid(&hid_guid);
        HDEVINFO device_info = SetupDiGetClassDevsW(&hid_guid, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (device_info == INVALID_HANDLE_VALUE)
        {
            return;
        }

        for (DWORD index = 0;; ++index)
        {
            SP_DEVICE_INTERFACE_DATA interface_data = {};
            interface_data.cbSize = sizeof(interface_data);
            if (!SetupDiEnumDeviceInterfaces(device_info, 0, &hid_guid, index, &interface_data))
            {
                break;
            }

            DWORD required_size = 0;
            SetupDiGetDeviceInterfaceDetailW(device_info, &interface_data, 0, 0, &required_size, 0);
            if (required_size == 0)
            {
                continue;
            }

            uint8_t detail_buffer[1024] = {};
            if (required_size > sizeof(detail_buffer))
            {
                continue;
            }

            SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail_data = (SP_DEVICE_INTERFACE_DETAIL_DATA_W*) detail_buffer;
            detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if (!SetupDiGetDeviceInterfaceDetailW(device_info, &interface_data, detail_data, required_size, 0, 0))
            {
                continue;
            }

            if (IsFailedPath(switch_context, detail_data->DevicePath))
            {
                continue;
            }

            SwitchHIDDeviceInfo switch_device_info = {};
            if (!IsSwitchControllerPath(detail_data->DevicePath, &switch_device_info))
            {
                continue;
            }

            uint32_t existing_index = FindDeviceByPath(switch_context, detail_data->DevicePath);
            if (existing_index != INVALID_INDEX)
            {
                switch_context->m_Devices[existing_index].m_Seen = true;
                switch_context->m_Devices[existing_index].m_LastSeenTick = GetTickCount64();
            }
            else
            {
                OpenSwitchDevice(switch_context, detail_data->DevicePath, switch_device_info);
                existing_index = FindDeviceByPath(switch_context, detail_data->DevicePath);
                if (existing_index != INVALID_INDEX)
                {
                    switch_context->m_Devices[existing_index].m_Seen = true;
                    switch_context->m_Devices[existing_index].m_LastSeenTick = GetTickCount64();
                }
            }
        }

        SetupDiDestroyDeviceInfoList(device_info);

    }

    bool GamepadSwitchUpdate(HContext context, Gamepad* gamepad)
    {
        (void) context;
        SwitchHIDContext* switch_context = g_SwitchHIDContext;
        if (switch_context == 0)
        {
            return false;
        }

        uint32_t index = FindDeviceByGamepad(switch_context, gamepad);
        if (index == INVALID_INDEX)
        {
            return false;
        }

        SwitchHIDDevice* device = &switch_context->m_Devices[index];
        if (!PollAsyncRead(device))
        {
            uint64_t now = GetTickCount64();
            uint64_t last_packet_tick = device->m_LastPacketTick != 0 ? device->m_LastPacketTick : device->m_LastSeenTick;
            if (device->m_FirstReadFailureTick == 0)
            {
                device->m_FirstReadFailureTick = now;
            }

            if (now >= device->m_FirstReadFailureTick + SWITCH_READ_FAILURE_DISCONNECT_TIMEOUT_MS &&
                now >= last_packet_tick + SWITCH_READ_FAILURE_DISCONNECT_TIMEOUT_MS)
            {
                dmLogWarning("Lost %s HID fallback", GetSwitchDeviceName(device->m_DeviceType));
                CloseSwitchDevice(device, switch_context->m_HidContext);
            }
        }
        else
        {
            device->m_FirstReadFailureTick = 0;
        }

        return true;
    }

    bool GamepadSwitchGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        (void) context;
        SwitchHIDContext* switch_context = g_SwitchHIDContext;
        if (switch_context == 0)
        {
            return false;
        }

        uint32_t index = FindDeviceByGamepad(switch_context, gamepad);
        if (index != INVALID_INDEX)
        {
            dmStrlCpy(name, GetSwitchDeviceName(switch_context->m_Devices[index].m_DeviceType), MAX_GAMEPAD_NAME_LENGTH);
            return true;
        }
        return false;
    }

    bool GamepadSwitchGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid)
    {
        (void) context;
        SwitchHIDContext* switch_context = g_SwitchHIDContext;
        if (switch_context == 0 || guid == 0)
        {
            return false;
        }

        uint32_t index = FindDeviceByGamepad(switch_context, gamepad);
        if (index == INVALID_INDEX)
        {
            return false;
        }

        *guid = switch_context->m_Devices[index].m_Guid;
        return true;
    }

    bool GamepadSwitchOwnsGamepad(HContext context, Gamepad* gamepad)
    {
        (void) context;
        SwitchHIDContext* switch_context = g_SwitchHIDContext;
        return switch_context != 0 && FindDeviceByGamepad(switch_context, gamepad) != INVALID_INDEX;
    }
}
