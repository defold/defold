/*
  Copyright (C) 2026 The Defold Foundation

  This implementation is based on the Sony DualSense HIDAPI driver in SDL: src/joystick/hidapi/SDL_hidapi_ps5.c

  SDL's DualSense driver includes code and logic contributed under the SDL zlib license.

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
    static const uint16_t SONY_VENDOR_ID = 0x054c;
    static const uint16_t SONY_DUALSENSE_PRODUCT_ID = 0x0ce6;
    static const uint16_t SONY_DUALSENSE_EDGE_PRODUCT_ID = 0x0df2;
    static const uint16_t HID_USAGE_PAGE_GENERIC_DESKTOP = 0x01;
    static const uint16_t HID_USAGE_GAMEPAD = 0x05;
    static const uint16_t USB_BUS = 0x0003;
    static const uint16_t BLUETOOTH_BUS = 0x0005;
    static const uint32_t DUALSENSE_HID_MAX_DEVICES = MAX_GAMEPAD_COUNT;
    static const uint32_t DUALSENSE_HID_PATH_LENGTH = 512;
    static const uint32_t DUALSENSE_READ_BUFFER_SIZE = 1024;
    static const uint32_t DUALSENSE_BLUETOOTH_EFFECTS_REPORT_SIZE = 78;
    static const DWORD DUALSENSE_CANCEL_TIMEOUT_MS = 100;
    static const uint64_t DUALSENSE_BLUETOOTH_TICKLE_TIMEOUT_MS = 500;

    enum DualSenseInputReportID
    {
        DUALSENSE_INPUT_REPORT_USB_STATE = 0x01,
        DUALSENSE_INPUT_REPORT_BLUETOOTH_STATE = 0x31,
    };

    enum DualSenseButtonIndex
    {
        DUALSENSE_BUTTON_PS = HID_GameInputGamepadNone,
        DUALSENSE_BUTTON_MENU = HID_GameInputGamepadMenu,
        DUALSENSE_BUTTON_VIEW = HID_GameInputGamepadView,
        DUALSENSE_BUTTON_CROSS = HID_GameInputGamepadA,
        DUALSENSE_BUTTON_CIRCLE = HID_GameInputGamepadB,
        DUALSENSE_BUTTON_SQUARE = HID_GameInputGamepadX,
        DUALSENSE_BUTTON_TRIANGLE = HID_GameInputGamepadY,
        DUALSENSE_BUTTON_DPAD_UP = HID_GameInputGamepadDPadUp,
        DUALSENSE_BUTTON_DPAD_DOWN = HID_GameInputGamepadDPadDown,
        DUALSENSE_BUTTON_DPAD_LEFT = HID_GameInputGamepadDPadLeft,
        DUALSENSE_BUTTON_DPAD_RIGHT = HID_GameInputGamepadDPadRight,
        DUALSENSE_BUTTON_LEFT_SHOULDER = HID_GameInputGamepadLeftShoulder,
        DUALSENSE_BUTTON_RIGHT_SHOULDER = HID_GameInputGamepadRightShoulder,
        DUALSENSE_BUTTON_LEFT_STICK = HID_GameInputGamepadLeftThumbstick,
        DUALSENSE_BUTTON_RIGHT_STICK = HID_GameInputGamepadRightThumbstick,
        DUALSENSE_BUTTON_TOUCHPAD = HID_GameInputGamepad_Max,
        DUALSENSE_BUTTON_MICROPHONE,
        DUALSENSE_BUTTON_LEFT_FUNCTION,
        DUALSENSE_BUTTON_RIGHT_FUNCTION,
        DUALSENSE_BUTTON_LEFT_PADDLE,
        DUALSENSE_BUTTON_RIGHT_PADDLE,
        DUALSENSE_BUTTON_MAX
    };

    struct DualSenseHIDDevice
    {
        wchar_t     m_Path[DUALSENSE_HID_PATH_LENGTH];
        HANDLE      m_Handle;
        OVERLAPPED* m_ReadOverlapped;
        uint8_t*    m_ReadBuffer;
        uint16_t    m_InputReportLength;
        uint16_t    m_OutputReportLength;
        uint16_t    m_FeatureReportLength;
        uint16_t    m_UsagePage;
        uint16_t    m_Usage;
        uint16_t    m_ProductID;
        bool        m_IsBluetooth;
        bool        m_CanWrite;
        bool        m_ReadPending;
        bool        m_Seen;
        bool        m_LoggedFirstPacket;
        uint64_t    m_LastPacketTick;
        uint64_t    m_LastTickleTick;
        Gamepad*    m_Gamepad;
        GamepadGuid m_Guid;
    };

    struct DualSenseHIDContext
    {
        HContext           m_HidContext;
        DualSenseHIDDevice m_Devices[DUALSENSE_HID_MAX_DEVICES];
        wchar_t            m_FailedPaths[DUALSENSE_HID_MAX_DEVICES][DUALSENSE_HID_PATH_LENGTH];
        uint32_t           m_FailedPathCount;
    };

    struct DualSenseHIDDeviceInfo
    {
        uint16_t m_VendorID;
        uint16_t m_ProductID;
        uint16_t m_InputReportLength;
        uint16_t m_OutputReportLength;
        uint16_t m_FeatureReportLength;
        uint16_t m_UsagePage;
        uint16_t m_Usage;
    };

    struct DualSenseWriteOperation
    {
        OVERLAPPED m_Overlapped;
        uint8_t    m_Output[DUALSENSE_BLUETOOTH_EFFECTS_REPORT_SIZE];
    };

    static DualSenseHIDContext* g_DualSenseHIDContext = 0;

    static const char* GetDualSenseDeviceName(uint16_t product_id)
    {
        return product_id == SONY_DUALSENSE_EDGE_PRODUCT_ID ? "DualSense Edge Wireless Controller" : "DualSense Wireless Controller";
    }

    static bool IsSupportedDualSenseDevice(uint16_t vendor_id, uint16_t product_id)
    {
        return vendor_id == SONY_VENDOR_ID &&
            (product_id == SONY_DUALSENSE_PRODUCT_ID || product_id == SONY_DUALSENSE_EDGE_PRODUCT_ID);
    }

    static bool IsBluetoothPath(const wchar_t* path)
    {
        return wcsstr(path, L"bth") != 0 ||
            wcsstr(path, L"BTH") != 0 ||
            wcsstr(path, L"bluetooth") != 0 ||
            wcsstr(path, L"BLUETOOTH") != 0;
    }

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

    static float NormalizeAxis(uint8_t value)
    {
        return ((float) value / 127.5f) - 1.0f;
    }

    static float NormalizeTrigger(uint8_t value)
    {
        return (float) value / 255.0f;
    }

    static uint8_t DualSenseDPadToHat(uint8_t dpad)
    {
        switch (dpad & 0x0f)
        {
            case 0:  return 1;
            case 1:  return 2;
            case 2:  return 3;
            case 3:  return 4;
            case 4:  return 5;
            case 5:  return 6;
            case 6:  return 7;
            case 7:  return 8;
            default: return 0;
        }
    }

    static void ReadDualSenseCommonState(DualSenseHIDDevice* device, const uint8_t* state, uint32_t state_size)
    {
        if (state_size < 9 || device->m_Gamepad == 0)
        {
            return;
        }

        Gamepad* gamepad = device->m_Gamepad;
        GamepadPacket& packet = gamepad->m_Packet;
        memset(packet.m_Axis, 0, sizeof(packet.m_Axis));
        memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
        memset(packet.m_Hat, 0, sizeof(packet.m_Hat));

        packet.m_Axis[0] = NormalizeAxis(state[0]);
        packet.m_Axis[1] = NormalizeAxis(state[1]);
        packet.m_Axis[2] = NormalizeAxis(state[2]);
        packet.m_Axis[3] = NormalizeAxis(state[3]);

        uint8_t buttons_hat_0;
        uint8_t buttons_1;
        uint8_t buttons_2;
        if (state_size >= 11)
        {
            packet.m_Axis[4] = NormalizeTrigger(state[4]);
            packet.m_Axis[5] = NormalizeTrigger(state[5]);
            buttons_hat_0 = state[7];
            buttons_1 = state[8];
            buttons_2 = state[9];
        }
        else
        {
            buttons_hat_0 = state[4];
            buttons_1 = state[5];
            buttons_2 = state[6];
            packet.m_Axis[4] = (buttons_1 & 0x04) ? 1.0f : NormalizeTrigger(state[7]);
            packet.m_Axis[5] = (buttons_1 & 0x08) ? 1.0f : NormalizeTrigger(state[8]);
        }

        uint8_t face_buttons = buttons_hat_0 >> 4;
        SetButton(packet, DUALSENSE_BUTTON_SQUARE, (face_buttons & 0x01) != 0);
        SetButton(packet, DUALSENSE_BUTTON_CROSS, (face_buttons & 0x02) != 0);
        SetButton(packet, DUALSENSE_BUTTON_CIRCLE, (face_buttons & 0x04) != 0);
        SetButton(packet, DUALSENSE_BUTTON_TRIANGLE, (face_buttons & 0x08) != 0);

        packet.m_Hat[0] = DualSenseDPadToHat(buttons_hat_0);

        SetButton(packet, DUALSENSE_BUTTON_LEFT_SHOULDER, (buttons_1 & 0x01) != 0);
        SetButton(packet, DUALSENSE_BUTTON_RIGHT_SHOULDER, (buttons_1 & 0x02) != 0);
        SetButton(packet, DUALSENSE_BUTTON_VIEW, (buttons_1 & 0x10) != 0);
        SetButton(packet, DUALSENSE_BUTTON_MENU, (buttons_1 & 0x20) != 0);
        SetButton(packet, DUALSENSE_BUTTON_LEFT_STICK, (buttons_1 & 0x40) != 0);
        SetButton(packet, DUALSENSE_BUTTON_RIGHT_STICK, (buttons_1 & 0x80) != 0);

        SetButton(packet, DUALSENSE_BUTTON_PS, (buttons_2 & 0x01) != 0);
        SetButton(packet, DUALSENSE_BUTTON_TOUCHPAD, (buttons_2 & 0x02) != 0);
        SetButton(packet, DUALSENSE_BUTTON_MICROPHONE, (buttons_2 & 0x04) != 0);
        SetButton(packet, DUALSENSE_BUTTON_LEFT_FUNCTION, (buttons_2 & 0x10) != 0);
        SetButton(packet, DUALSENSE_BUTTON_RIGHT_FUNCTION, (buttons_2 & 0x20) != 0);
        SetButton(packet, DUALSENSE_BUTTON_LEFT_PADDLE, (buttons_2 & 0x40) != 0);
        SetButton(packet, DUALSENSE_BUTTON_RIGHT_PADDLE, (buttons_2 & 0x80) != 0);

        gamepad->m_AxisCount = 6;
        gamepad->m_ButtonCount = DUALSENSE_BUTTON_MAX;
        gamepad->m_HatCount = 1;
    }

    static bool ProcessDualSenseInputReport(DualSenseHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        if (report_size < 2)
        {
            return false;
        }

        if (!device->m_LoggedFirstPacket)
        {
            device->m_LoggedFirstPacket = true;
            dmLogInfo("%s HID first report: id=0x%02x size=%u", GetDualSenseDeviceName(device->m_ProductID), report[0], report_size);
        }

        device->m_LastPacketTick = GetTickCount64();

        switch (report[0])
        {
            case DUALSENSE_INPUT_REPORT_USB_STATE:
                ReadDualSenseCommonState(device, &report[1], report_size - 1);
                return true;

            case DUALSENSE_INPUT_REPORT_BLUETOOTH_STATE:
                if (report_size > 2)
                {
                    ReadDualSenseCommonState(device, &report[2], report_size - 2);
                    return true;
                }
                return false;

            default:
                return false;
        }
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

    static bool WriteReport(DualSenseHIDDevice* device, const uint8_t* report, uint32_t report_size)
    {
        if (!device->m_CanWrite)
        {
            return false;
        }

        uint32_t write_size = report_size;
        if (device->m_OutputReportLength != 0)
        {
            write_size = device->m_OutputReportLength;
        }

        if (write_size > DUALSENSE_BLUETOOTH_EFFECTS_REPORT_SIZE || report_size > DUALSENSE_BLUETOOTH_EFFECTS_REPORT_SIZE)
        {
            return false;
        }

        DualSenseWriteOperation* operation = new DualSenseWriteOperation();
        memset(operation, 0, sizeof(*operation));
        memcpy(operation->m_Output, report, report_size);

        operation->m_Overlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (operation->m_Overlapped.hEvent == 0)
        {
            delete operation;
            return false;
        }

        DWORD bytes_written = 0;
        DWORD last_error = ERROR_SUCCESS;
        BOOL result = WriteFile(device->m_Handle, operation->m_Output, write_size, &bytes_written, &operation->m_Overlapped);
        bool completed = true;
        if (!result)
        {
            last_error = GetLastError();
            if (last_error == ERROR_IO_PENDING)
            {
                completed = CompleteOverlappedOperation(device->m_Handle, &operation->m_Overlapped, DUALSENSE_CANCEL_TIMEOUT_MS, &bytes_written, &last_error, &result);
                if (!completed)
                {
                    completed = CancelAndCompleteOverlappedOperation(device->m_Handle, &operation->m_Overlapped, DUALSENSE_CANCEL_TIMEOUT_MS, &bytes_written, &last_error, &result);
                }
            }
        }

        if (completed)
        {
            CloseHandle(operation->m_Overlapped.hEvent);
            delete operation;
        }
        else
        {
            dmLogWarning("DualSense HID Bluetooth tickle cancellation still pending");
        }

        return result != FALSE && bytes_written == write_size;
    }

    static void TickleBluetooth(DualSenseHIDDevice* device)
    {
        if (!device->m_IsBluetooth || !device->m_CanWrite)
        {
            return;
        }

        uint64_t now = GetTickCount64();
        uint64_t last_packet = device->m_LastPacketTick != 0 ? device->m_LastPacketTick : now;
        if (now < last_packet + DUALSENSE_BLUETOOTH_TICKLE_TIMEOUT_MS ||
            now < device->m_LastTickleTick + DUALSENSE_BLUETOOTH_TICKLE_TIMEOUT_MS)
        {
            return;
        }

        uint8_t report[DUALSENSE_BLUETOOTH_EFFECTS_REPORT_SIZE] = {};
        report[0] = DUALSENSE_INPUT_REPORT_BLUETOOTH_STATE;
        report[1] = 0x02;
        WriteReport(device, report, sizeof(report));
        device->m_LastTickleTick = now;
    }

    static uint32_t GetInputReportLength(DualSenseHIDDevice* device)
    {
        return device->m_InputReportLength != 0 ? device->m_InputReportLength : 64;
    }

    static bool BeginAsyncRead(DualSenseHIDDevice* device)
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
                ProcessDualSenseInputReport(device, device->m_ReadBuffer, bytes_read);
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

    static bool PollAsyncRead(DualSenseHIDDevice* device)
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
                TickleBluetooth(device);
                return true;
            }
            return error == ERROR_IO_INCOMPLETE;
        }

        device->m_ReadPending = false;
        if (bytes_read > 0)
        {
            ProcessDualSenseInputReport(device, device->m_ReadBuffer, bytes_read);
        }
        return BeginAsyncRead(device);
    }

    static void CloseDualSenseDevice(DualSenseHIDDevice* device, HContext context)
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
                free_read_operation = CancelAndCompleteOverlappedOperation(device->m_Handle, read_overlapped, DUALSENSE_CANCEL_TIMEOUT_MS, &bytes_read, &last_error, &result);
                if (!free_read_operation)
                {
                    dmLogWarning("DualSense HID async read cancellation still pending");
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

    static uint32_t FindDeviceByPath(DualSenseHIDContext* context, const wchar_t* path)
    {
        for (uint32_t i = 0; i < DUALSENSE_HID_MAX_DEVICES; ++i)
        {
            if (context->m_Devices[i].m_Handle != 0 && wcscmp(context->m_Devices[i].m_Path, path) == 0)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static uint32_t FindFreeDeviceIndex(DualSenseHIDContext* context)
    {
        for (uint32_t i = 0; i < DUALSENSE_HID_MAX_DEVICES; ++i)
        {
            if (context->m_Devices[i].m_Handle == 0)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static uint32_t FindDeviceByGamepad(DualSenseHIDContext* context, Gamepad* gamepad)
    {
        for (uint32_t i = 0; i < DUALSENSE_HID_MAX_DEVICES; ++i)
        {
            if (context->m_Devices[i].m_Gamepad == gamepad)
            {
                return i;
            }
        }
        return INVALID_INDEX;
    }

    static bool IsFailedPath(DualSenseHIDContext* context, const wchar_t* path)
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

    static void AddFailedPath(DualSenseHIDContext* context, const wchar_t* path)
    {
        if (IsFailedPath(context, path) || context->m_FailedPathCount >= DUALSENSE_HID_MAX_DEVICES)
        {
            return;
        }
        wcsncpy_s(context->m_FailedPaths[context->m_FailedPathCount], DUALSENSE_HID_PATH_LENGTH, path, _TRUNCATE);
        ++context->m_FailedPathCount;
    }

    static bool QueryDualSenseHIDDeviceInfo(HANDLE handle, DualSenseHIDDeviceInfo* info)
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

    static bool IsDualSenseControllerPath(const wchar_t* path, DualSenseHIDDeviceInfo* info)
    {
        HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
        if (handle == INVALID_HANDLE_VALUE)
        {
            handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
            if (handle == INVALID_HANDLE_VALUE)
            {
                return false;
            }
        }

        bool result = QueryDualSenseHIDDeviceInfo(handle, info) &&
            IsSupportedDualSenseDevice(info->m_VendorID, info->m_ProductID) &&
            info->m_UsagePage == HID_USAGE_PAGE_GENERIC_DESKTOP &&
            info->m_Usage == HID_USAGE_GAMEPAD;

        CloseHandle(handle);
        return result;
    }

    static bool OpenDualSenseDevice(DualSenseHIDContext* context, const wchar_t* path, const DualSenseHIDDeviceInfo& info)
    {
        uint32_t index = FindFreeDeviceIndex(context);
        if (index == INVALID_INDEX)
        {
            return false;
        }

        HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
        bool can_write = handle != INVALID_HANDLE_VALUE;
        if (handle == INVALID_HANDLE_VALUE)
        {
            handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
            if (handle == INVALID_HANDLE_VALUE)
            {
                return false;
            }
        }

        DualSenseHIDDevice* device = &context->m_Devices[index];
        memset(device, 0, sizeof(*device));
        device->m_Handle = handle;
        device->m_InputReportLength = info.m_InputReportLength;
        device->m_OutputReportLength = info.m_OutputReportLength;
        device->m_FeatureReportLength = info.m_FeatureReportLength;
        device->m_UsagePage = info.m_UsagePage;
        device->m_Usage = info.m_Usage;
        device->m_ProductID = info.m_ProductID;
        device->m_IsBluetooth = IsBluetoothPath(path);
        device->m_CanWrite = can_write;
        device->m_LastPacketTick = GetTickCount64();
        wcsncpy_s(device->m_Path, DUALSENSE_HID_PATH_LENGTH, path, _TRUNCATE);

        const char* device_name = GetDualSenseDeviceName(device->m_ProductID);
        dmLogInfo("%s HID candidate: usage=0x%04x/0x%04x reports input=%u output=%u feature=%u bus=%s",
            device_name,
            device->m_UsagePage,
            device->m_Usage,
            device->m_InputReportLength,
            device->m_OutputReportLength,
            device->m_FeatureReportLength,
            device->m_IsBluetooth ? "bluetooth" : "unknown/usb");

        if (device->m_InputReportLength > DUALSENSE_READ_BUFFER_SIZE)
        {
            dmLogWarning("Skipping %s HID candidate: report length unsupported input=%u", device_name, device->m_InputReportLength);
            AddFailedPath(context, path);
            CloseDualSenseDevice(device, context->m_HidContext);
            return false;
        }

        device->m_ReadBuffer = new uint8_t[DUALSENSE_READ_BUFFER_SIZE];
        memset(device->m_ReadBuffer, 0, DUALSENSE_READ_BUFFER_SIZE);
        device->m_ReadOverlapped = new OVERLAPPED();
        memset(device->m_ReadOverlapped, 0, sizeof(*device->m_ReadOverlapped));
        device->m_ReadOverlapped->hEvent = CreateEventW(0, TRUE, FALSE, 0);
        if (device->m_ReadOverlapped->hEvent == 0)
        {
            CloseDualSenseDevice(device, context->m_HidContext);
            return false;
        }

        HidD_SetNumInputBuffers(handle, 64);

        device->m_Gamepad = CreateGamepad(context->m_HidContext);
        if (device->m_Gamepad == 0)
        {
            CloseDualSenseDevice(device, context->m_HidContext);
            return false;
        }

        device->m_Guid = CreateGUID(device->m_IsBluetooth ? BLUETOOTH_BUS : USB_BUS, SONY_VENDOR_ID, device->m_ProductID, 0, 0, device_name, 0, 0);
        device->m_Gamepad->m_AxisCount = 6;
        device->m_Gamepad->m_ButtonCount = DUALSENSE_BUTTON_MAX;
        device->m_Gamepad->m_HatCount = 1;
        SetGamepadConnectionStatus(context->m_HidContext, device->m_Gamepad, true);

        BeginAsyncRead(device);

        dmLogInfo("%s HID fallback connected", device_name);
        return true;
    }

    bool GamepadDualSenseInitialize(HContext context)
    {
        assert(g_DualSenseHIDContext == 0);
        g_DualSenseHIDContext = new DualSenseHIDContext();
        memset(g_DualSenseHIDContext, 0, sizeof(*g_DualSenseHIDContext));
        g_DualSenseHIDContext->m_HidContext = context;
        return true;
    }

    void GamepadDualSenseFinalize(HContext context)
    {
        (void) context;
        if (g_DualSenseHIDContext == 0)
        {
            return;
        }

        for (uint32_t i = 0; i < DUALSENSE_HID_MAX_DEVICES; ++i)
        {
            if (g_DualSenseHIDContext->m_Devices[i].m_Handle != 0)
            {
                CloseDualSenseDevice(&g_DualSenseHIDContext->m_Devices[i], g_DualSenseHIDContext->m_HidContext);
            }
        }

        delete g_DualSenseHIDContext;
        g_DualSenseHIDContext = 0;
    }

    void GamepadDualSenseDetectDevices(HContext context)
    {
        (void) context;
        DualSenseHIDContext* dualsense_context = g_DualSenseHIDContext;
        if (dualsense_context == 0)
        {
            return;
        }

        for (uint32_t i = 0; i < DUALSENSE_HID_MAX_DEVICES; ++i)
        {
            dualsense_context->m_Devices[i].m_Seen = false;
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

            if (IsFailedPath(dualsense_context, detail_data->DevicePath))
            {
                continue;
            }

            DualSenseHIDDeviceInfo dualsense_device_info = {};
            if (!IsDualSenseControllerPath(detail_data->DevicePath, &dualsense_device_info))
            {
                continue;
            }

            uint32_t existing_index = FindDeviceByPath(dualsense_context, detail_data->DevicePath);
            if (existing_index != INVALID_INDEX)
            {
                dualsense_context->m_Devices[existing_index].m_Seen = true;
            }
            else
            {
                OpenDualSenseDevice(dualsense_context, detail_data->DevicePath, dualsense_device_info);
                existing_index = FindDeviceByPath(dualsense_context, detail_data->DevicePath);
                if (existing_index != INVALID_INDEX)
                {
                    dualsense_context->m_Devices[existing_index].m_Seen = true;
                }
            }
        }

        SetupDiDestroyDeviceInfoList(device_info);

        for (uint32_t i = 0; i < DUALSENSE_HID_MAX_DEVICES; ++i)
        {
            DualSenseHIDDevice* device = &dualsense_context->m_Devices[i];
            if (device->m_Handle != 0 && !device->m_Seen)
            {
                dmLogInfo("%s HID fallback disconnected", GetDualSenseDeviceName(device->m_ProductID));
                CloseDualSenseDevice(device, dualsense_context->m_HidContext);
            }
        }
    }

    bool GamepadDualSenseUpdate(HContext context, Gamepad* gamepad)
    {
        (void) context;
        DualSenseHIDContext* dualsense_context = g_DualSenseHIDContext;
        if (dualsense_context == 0)
        {
            return false;
        }

        uint32_t index = FindDeviceByGamepad(dualsense_context, gamepad);
        if (index == INVALID_INDEX)
        {
            return false;
        }

        DualSenseHIDDevice* device = &dualsense_context->m_Devices[index];
        if (!PollAsyncRead(device))
        {
            dmLogWarning("Lost %s HID fallback", GetDualSenseDeviceName(device->m_ProductID));
            CloseDualSenseDevice(device, dualsense_context->m_HidContext);
        }

        return true;
    }

    bool GamepadDualSenseGetDeviceName(HContext context, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
    {
        (void) context;
        DualSenseHIDContext* dualsense_context = g_DualSenseHIDContext;
        if (dualsense_context == 0)
        {
            return false;
        }

        uint32_t index = FindDeviceByGamepad(dualsense_context, gamepad);
        if (index != INVALID_INDEX)
        {
            dmStrlCpy(name, GetDualSenseDeviceName(dualsense_context->m_Devices[index].m_ProductID), MAX_GAMEPAD_NAME_LENGTH);
            return true;
        }
        return false;
    }

    bool GamepadDualSenseGetDeviceGuid(HContext context, HGamepad gamepad, GamepadGuid* guid)
    {
        (void) context;
        DualSenseHIDContext* dualsense_context = g_DualSenseHIDContext;
        if (dualsense_context == 0 || guid == 0)
        {
            return false;
        }

        uint32_t index = FindDeviceByGamepad(dualsense_context, gamepad);
        if (index == INVALID_INDEX)
        {
            return false;
        }

        *guid = dualsense_context->m_Devices[index].m_Guid;
        return true;
    }

    bool GamepadDualSenseOwnsGamepad(HContext context, Gamepad* gamepad)
    {
        (void) context;
        DualSenseHIDContext* dualsense_context = g_DualSenseHIDContext;
        return dualsense_context != 0 && FindDeviceByGamepad(dualsense_context, gamepad) != INVALID_INDEX;
    }
}
