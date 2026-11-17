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

#include "window_macos.h"
#import <IOKit/hid/IOHIDManager.h>
#include <dlib/dstrings.h>
#include <stdlib.h>

namespace dmPlatform
{
    enum ControlKind
    {
        AXIS, BUTTON, HAT, UNSUPPORTED
    };
    struct Control
    {
        IOHIDElementRef m_Element;
        uint32_t        m_Usage, m_Order;
        CFIndex         m_Min, m_Max;
    };
    struct RawJoystick
    {
        IOHIDDeviceRef   m_Device;
        CFArrayRef       m_Elements;
        dmArray<Control> m_Controls[3];
        char             m_Name[256];
        char             m_Guid[33];
    };

    static IOHIDManagerRef g_Manager;
    static RawJoystick* g_Devices[16];
    static HWindow g_CallbackWindow;

    static ControlKind Kind(unsigned page, unsigned usage)
    {
        if (page == kHIDPage_Button || page == kHIDPage_Consumer)
        {
            return BUTTON;
        }

        if (page == kHIDPage_GenericDesktop)
        {
            if (usage >= kHIDUsage_GD_X && usage <= kHIDUsage_GD_Wheel)
            {
                return AXIS;
            }

            if (usage == kHIDUsage_GD_Hatswitch)
            {
                return HAT;
            }

            if ((usage >= kHIDUsage_GD_DPadUp && usage <= kHIDUsage_GD_DPadLeft) ||
                usage == kHIDUsage_GD_SystemMainMenu || usage == kHIDUsage_GD_Select || usage == kHIDUsage_GD_Start)
            {
                return BUTTON;
            }
        }

        if (page == kHIDPage_Simulation)
        {
            switch (usage)
            {
                case kHIDUsage_Sim_Throttle:
                case kHIDUsage_Sim_Rudder:
                case kHIDUsage_Sim_Accelerator:
                case kHIDUsage_Sim_Brake:
                case kHIDUsage_Sim_Steering:
                {
                    return AXIS;
                }
            }
        }

        return UNSUPPORTED;
    }

    static int CompareControls(const void* left, const void* right)
    {
        const Control& a = *(const Control*)left;
        const Control& b = *(const Control*)right;
        if (a.m_Usage != b.m_Usage)
        {
            return a.m_Usage < b.m_Usage ? -1 : 1;
        }

        return (a.m_Order > b.m_Order) - (a.m_Order < b.m_Order);
    }

    static uint32_t Property(IOHIDDeviceRef device, CFStringRef key)
    {
        CFTypeRef property = IOHIDDeviceGetProperty(device, key);
        uint32_t number = 0;
        if (property && CFGetTypeID(property) == CFNumberGetTypeID())
        {
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &number);
        }

        return number;
    }

    static void ReadIdentity(RawJoystick* joystick)
    {
        IOHIDDeviceRef device = joystick->m_Device;
        CFTypeRef name = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey));
        if (!name || CFGetTypeID(name) != CFStringGetTypeID() ||
            !CFStringGetCString((CFStringRef)name, joystick->m_Name, sizeof(joystick->m_Name), kCFStringEncodingUTF8))
        {
            dmStrlCpy(joystick->m_Name, "Unknown", sizeof(joystick->m_Name));
        }

        uint8_t bytes[16] = {};
        uint32_t vendor = Property(device, CFSTR(kIOHIDVendorIDKey));
        uint32_t product = Property(device, CFSTR(kIOHIDProductIDKey));
        uint32_t version = Property(device, CFSTR(kIOHIDVersionNumberKey));
        if (vendor && product)
        {
            bytes[0] = 3;
            bytes[4] = vendor;
            bytes[5] = vendor >> 8;
            bytes[8] = product;
            bytes[9] = product >> 8;
            bytes[12] = version;
            bytes[13] = version >> 8;
        }
        else
        {
            bytes[0] = 5;
            memcpy(bytes + 4, joystick->m_Name, strnlen(joystick->m_Name, 11));
        }

        for (unsigned i = 0; i < sizeof(bytes); ++i)
        {
            dmSnPrintf(joystick->m_Guid + i * 2, 3, "%02x", bytes[i]);
        }
    }

    static void Connected(void*, IOReturn result, void*, IOHIDDeviceRef device)
    {
        int slot = -1;
        for (unsigned i = 0; i < 16; ++i)
        {
            if (g_Devices[i] && g_Devices[i]->m_Device == device)
            {
                return;
            }

            if (slot < 0 && !g_Devices[i])
            {
                slot = i;
            }
        }

        if (result != kIOReturnSuccess || slot < 0)
        {
            return;
        }

        CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
        if (!elements)
        {
            return;
        }

        RawJoystick* joystick = new RawJoystick();
        joystick->m_Elements = elements;
        joystick->m_Device = (IOHIDDeviceRef)CFRetain(device);
        ReadIdentity(joystick);
        for (CFIndex i = 0; i < CFArrayGetCount(elements); ++i)
        {
            IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
            if (CFGetTypeID(element) != IOHIDElementGetTypeID())
            {
                continue;
            }

            IOHIDElementType type = IOHIDElementGetType(element);
            if (type != kIOHIDElementTypeInput_Misc && type != kIOHIDElementTypeInput_Button && type != kIOHIDElementTypeInput_Axis)
            {
                continue;
            }

            uint32_t usage = IOHIDElementGetUsage(element);
            ControlKind kind = Kind(IOHIDElementGetUsagePage(element), usage);
            if (kind == UNSUPPORTED)
            {
                continue;
            }

            Control control = { element, usage, (uint32_t)i, IOHIDElementGetLogicalMin(element), IOHIDElementGetLogicalMax(element) };
            dmArray<Control>& controls = joystick->m_Controls[kind];
            if (controls.Full())
            {
                controls.OffsetCapacity(8);
            }

            controls.Push(control);
        }

        for (unsigned i = 0; i < 3; ++i)
        {
            dmArray<Control>& controls = joystick->m_Controls[i];
            if (controls.Size() > 1)
            {
                qsort(controls.Begin(), controls.Size(), sizeof(Control), CompareControls);
            }
        }

        g_Devices[slot] = joystick;
        if (g_CallbackWindow)
        {
            g_CallbackWindow->m_InputCallbacks.m_GamepadCallback(g_CallbackWindow->m_InputCallbacks.m_GamepadUserData, slot, WINDOW_GAMEPAD_EVENT_CONNECTED);
        }
    }

    static void ReleaseDevice(unsigned index)
    {
        RawJoystick* joystick = g_Devices[index];
        g_Devices[index] = 0;
        CFRelease(joystick->m_Elements);
        CFRelease(joystick->m_Device);
        delete joystick;
    }

    static void Disconnected(void*, IOReturn, void*, IOHIDDeviceRef device)
    {
        for (unsigned i = 0; i < 16; ++i)
        {
            if (g_Devices[i] && g_Devices[i]->m_Device == device)
            {
                ReleaseDevice(i);
                if (g_CallbackWindow)
                {
                    g_CallbackWindow->m_InputCallbacks.m_GamepadCallback(g_CallbackWindow->m_InputCallbacks.m_GamepadUserData, i, WINDOW_GAMEPAD_EVENT_DISCONNECTED);
                }

                return;
            }
        }
    }

    static void InitJoysticks()
    {
        if (g_Manager)
        {
            return;
        }

        @autoreleasepool
        {
            g_Manager = IOHIDManagerCreate(NULL, kIOHIDOptionsTypeNone);
            if (!g_Manager)
            {
                return;
            }

            NSMutableArray* matches = [NSMutableArray array];
            const int usages[] = { kHIDUsage_GD_Joystick, kHIDUsage_GD_GamePad, kHIDUsage_GD_MultiAxisController };
            for (unsigned i = 0; i < 3; ++i)
            {
                [matches addObject:@{ @kIOHIDDeviceUsagePageKey: @(kHIDPage_GenericDesktop), @kIOHIDDeviceUsageKey: @(usages[i]) }];
            }

            IOHIDManagerSetDeviceMatchingMultiple(g_Manager, (CFArrayRef)matches);
            IOHIDManagerRegisterDeviceMatchingCallback(g_Manager, Connected, 0);
            IOHIDManagerRegisterDeviceRemovalCallback(g_Manager, Disconnected, 0);
            IOHIDManagerScheduleWithRunLoop(g_Manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
            IOHIDManagerOpen(g_Manager, kIOHIDOptionsTypeNone);
            // Raw queries may precede both the first frame and initialization of the gamepad driver.
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
        }
    }

    void ShutdownJoysticks()
    {
        g_CallbackWindow = 0;
        if (g_Manager)
        {
            IOHIDManagerUnscheduleFromRunLoop(g_Manager, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
            IOHIDManagerClose(g_Manager, kIOHIDOptionsTypeNone);
            CFRelease(g_Manager);
            g_Manager = 0;
        }

        for (unsigned i = 0; i < 16; ++i)
        {
            if (g_Devices[i])
            {
                ReleaseDevice(i);
            }
        }
    }

    static RawJoystick* Device(uint32_t slot)
    {
        InitJoysticks();
        return slot < 16 ? g_Devices[slot] : 0;
    }

    bool JoystickPresent(int index)
    {
        return Device(index) != 0;
    }

    void SetGamepadEventCallback(HWindow window, FWindowGamepadEventCallback cb, void* data)
    {
        if (cb)
        {
            InitJoysticks();
            g_CallbackWindow = window;
        }
        else if (g_CallbackWindow == window)
        {
            g_CallbackWindow = 0;
        }

        window->m_InputCallbacks.m_GamepadCallback = cb;
        window->m_InputCallbacks.m_GamepadUserData = data;
    }

    const char* GetJoystickDeviceName(HWindow, uint32_t index)
    {
        RawJoystick* joystick = Device(index);
        return joystick ? joystick->m_Name : 0;
    }

    const char* GetJoystickDeviceGuid(HWindow, uint32_t index)
    {
        RawJoystick* joystick = Device(index);
        return joystick ? joystick->m_Guid : 0;
    }

    static CFIndex ReadValue(RawJoystick* joystick, const Control& control)
    {
        IOHIDValueRef value;
        if (IOHIDDeviceGetValue(joystick->m_Device, control.m_Element, &value) != kIOReturnSuccess || !value)
        {
            return 0;
        }

        return IOHIDValueGetIntegerValue(value);
    }

    uint32_t GetJoystickAxes(HWindow, uint32_t index, float* values, uint32_t capacity)
    {
        RawJoystick* joystick = Device(index);
        if (!joystick)
        {
            return 0;
        }

        dmArray<Control>& axes = joystick->m_Controls[AXIS];
        uint32_t count = MIN(capacity, axes.Size());
        for (unsigned i = 0; i < count; ++i)
        {
            Control& axis = axes[i];
            CFIndex value = ReadValue(joystick, axis);
            axis.m_Min = MIN(value, axis.m_Min);
            axis.m_Max = MAX(value, axis.m_Max);
            double range = (double)axis.m_Max - axis.m_Min;
            values[i] = range ? (float)(2.0 * ((double)value - axis.m_Min) / range - 1.0) : 0.0f;
        }

        return count;
    }

    static uint8_t HatValue(RawJoystick* joystick, const Control& hat)
    {
        static const uint8_t directions[] = { 1, 3, 2, 6, 4, 12, 8, 9 };
        CFIndex direction = ReadValue(joystick, hat) - hat.m_Min;
        return (unsigned long)direction < sizeof(directions) ? directions[direction] : 0;
    }

    uint32_t GetJoystickHats(HWindow, uint32_t index, uint8_t* values, uint32_t capacity)
    {
        RawJoystick* joystick = Device(index);
        if (!joystick)
        {
            return 0;
        }

        dmArray<Control>& hats = joystick->m_Controls[HAT];
        unsigned count = MIN(capacity, hats.Size());
        for (unsigned i = 0; i < count; ++i)
        {
            values[i] = HatValue(joystick, hats[i]);
        }

        return count;
    }

    uint32_t GetJoystickButtons(HWindow, uint32_t index, uint8_t* values, uint32_t capacity)
    {
        RawJoystick* joystick = Device(index);
        if (!joystick)
        {
            return 0;
        }

        dmArray<Control>& buttons = joystick->m_Controls[BUTTON];
        dmArray<Control>& hats = joystick->m_Controls[HAT];
        uint32_t count = MIN(capacity, buttons.Size());
        for (unsigned i = 0; i < count; ++i)
        {
            values[i] = ReadValue(joystick, buttons[i]) > buttons[i].m_Min;
        }

        for (unsigned i = 0; i < hats.Size() && count < capacity; ++i)
        {
            uint8_t hat = HatValue(joystick, hats[i]);
            for (unsigned bit = 0; bit < 4 && count < capacity; ++bit)
            {
                values[count++] = (hat >> bit) & 1;
            }
        }

        return count;
    }

}
