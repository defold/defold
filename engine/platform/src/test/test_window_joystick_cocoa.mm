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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../window.hpp"
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDManager.h>

// Supply descriptor properties, reports and hotplug notifications at the IOKit boundary.
// This standalone executable can exercise the registry without a signed virtual-device entitlement.
static IOHIDDeviceCallback g_Arrive, g_Remove;
static void* g_ArriveContext;
static void* g_RemoveContext;
static NSMutableDictionary* g_First;
static NSMutableDictionary* g_Second;

void IOHIDManagerRegisterDeviceMatchingCallback(IOHIDManagerRef, IOHIDDeviceCallback callback, void* context)
{
    g_Arrive = callback;
    g_ArriveContext = context;
}

void IOHIDManagerRegisterDeviceRemovalCallback(IOHIDManagerRef, IOHIDDeviceCallback callback, void* context)
{
    g_Remove = callback;
    g_RemoveContext = context;
}

IOReturn IOHIDManagerOpen(IOHIDManagerRef manager, IOOptionBits)
{
    if (g_Arrive && g_First)
    {
        g_Arrive(g_ArriveContext, kIOReturnSuccess, manager, (IOHIDDeviceRef)g_First);
    }

    return kIOReturnSuccess;
}

CFTypeRef IOHIDDeviceGetProperty(IOHIDDeviceRef device, CFStringRef key)
{
    return [(NSDictionary*)device objectForKey:(NSString*)key];
}

CFArrayRef IOHIDDeviceCopyMatchingElements(IOHIDDeviceRef device, CFDictionaryRef, IOOptionBits)
{
    return (CFArrayRef)CFRetain([(NSDictionary*)device objectForKey:@"controls"]);
}

// Dictionaries stand in for opaque IOHID elements, including their runtime type identity.
CFTypeID IOHIDElementGetTypeID()
{
    return CFDictionaryGetTypeID();
}

IOHIDElementType IOHIDElementGetType(IOHIDElementRef element)
{
    return kIOHIDElementTypeInput_Misc;
}

uint32_t IOHIDElementGetUsagePage(IOHIDElementRef element)
{
    return [[(NSDictionary*)element objectForKey:@"page"] unsignedIntValue];
}

uint32_t IOHIDElementGetUsage(IOHIDElementRef element)
{
    return [[(NSDictionary*)element objectForKey:@"usage"] unsignedIntValue];
}

CFIndex IOHIDElementGetLogicalMin(IOHIDElementRef element)
{
    return [[(NSDictionary*)element objectForKey:@"min"] longValue];
}

CFIndex IOHIDElementGetLogicalMax(IOHIDElementRef element)
{
    return [[(NSDictionary*)element objectForKey:@"max"] longValue];
}

IOReturn IOHIDDeviceGetValue(IOHIDDeviceRef, IOHIDElementRef element, IOHIDValueRef* value)
{
    *value = (IOHIDValueRef)[(NSDictionary*)element objectForKey:@"value"];
    return kIOReturnSuccess;
}

CFIndex IOHIDValueGetIntegerValue(IOHIDValueRef value)
{
    return [(NSNumber*)value longValue];
}

static void Add(NSMutableArray* array, int page, int usage, int low, int high, int initial)
{
    [array addObject:@{ @"page": @(page), @"usage": @(usage), @"min": @(low), @"max": @(high), @"value": @(initial) }];
}

struct Events
{
    int                count;
    int                slot;
    WindowGamepadEvent last;
};
static void Gamepad(void* data, int slot, WindowGamepadEvent event)
{
    Events* events = (Events*)data;
    ++events->count;
    events->slot = slot;
    events->last = event;
}

// Raw input must keep control indices, GUIDs and slots compatible before the engine HID driver starts.
TEST(CocoaRawJoystick, IdentityControlsAndReconnect)
{
    @autoreleasepool
    {
        NSMutableArray* controls = [NSMutableArray arrayWithObject:[NSNull null]];
        Add(controls, kHIDPage_GenericDesktop, kHIDUsage_GD_Y, -100, 100, -100);
        Add(controls, kHIDPage_Button, 2, 0, 1, 0);
        Add(controls, kHIDPage_GenericDesktop, kHIDUsage_GD_Hatswitch, 1, 8, 4);
        Add(controls, kHIDPage_GenericDesktop, kHIDUsage_GD_X, -100, 100, 100);
        Add(controls, kHIDPage_Button, 1, 0, 1, 1);
        g_First = [NSMutableDictionary dictionaryWithDictionary:@{ @"controls": controls, @kIOHIDProductKey: @"Defold Gamepad",
                                                                   @kIOHIDVendorIDKey: @0x1234, @kIOHIDProductIDKey: @0x5678, @kIOHIDVersionNumberKey: @0x9abc }];
        NSMutableArray* axes = [NSMutableArray array];
        Add(axes, kHIDPage_Simulation, kHIDUsage_Sim_Throttle, 0, 100, 50);
        Add(axes, kHIDPage_Simulation, kHIDUsage_Sim_Steering, 0, 255, 127);
        g_Second = [NSMutableDictionary dictionaryWithDictionary:@{ @"controls": axes, @kIOHIDProductKey: @"Generic Joystick" }];
        HWindow window = dmPlatform::NewWindow();
        WindowCreateParams params;
        WindowCreateParamsInitialize(&params);
        params.m_GraphicsApi = WINDOW_GRAPHICS_API_METAL;
        params.m_Hidden = true;
        ASSERT_EQ(WINDOW_RESULT_OK, dmPlatform::OpenWindow(window, params));
        ASSERT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
        ASSERT_STREQ("Defold Gamepad", dmPlatform::GetJoystickDeviceName(window, 0));
        ASSERT_STREQ("030000003412000078560000bc9a0000", dmPlatform::GetJoystickDeviceGuid(window, 0));
        float values[3];
        ASSERT_EQ(2u, dmPlatform::GetJoystickAxes(window, 0, values, 3));
        ASSERT_EQ(1.0f, values[0]);
        ASSERT_EQ(-1.0f, values[1]);
        uint8_t buttons[8] = {}, hats[2] = {};
        ASSERT_EQ(1u, dmPlatform::GetJoystickHats(window, 0, hats, 2));
        ASSERT_EQ(6, hats[0]);
        ASSERT_EQ(6u, dmPlatform::GetJoystickButtons(window, 0, buttons, 8));
        const uint8_t expected[] = { 1, 0, 0, 1, 1, 0 };
        ASSERT_EQ(0, memcmp(buttons, expected, sizeof(expected)));
        ASSERT_EQ(3u, dmPlatform::GetJoystickButtons(window, 0, buttons, 3));
        Events events = {};
        dmPlatform::SetGamepadEventCallback(window, Gamepad, &events);
        g_Arrive(g_ArriveContext, 0, 0, (IOHIDDeviceRef)g_Second);
        ASSERT_EQ(1, events.count);
        ASSERT_EQ(1, events.slot);
        ASSERT_EQ(WINDOW_GAMEPAD_EVENT_CONNECTED, events.last);
        ASSERT_STREQ("0500000047656e65726963204a6f7900", dmPlatform::GetJoystickDeviceGuid(window, 1));
        ASSERT_EQ(2u, dmPlatform::GetJoystickAxes(window, 1, values, 3));
        ASSERT_EQ(0.0f, values[0]);
        // An 8-bit stick close to its center exposes the single-precision rounding of the raw API.
        const float expected_axis = 2.0f * 127 / 255 - 1.0f;
        ASSERT_TRUE(values[1] == expected_axis);
        g_Arrive(g_ArriveContext, 0, 0, (IOHIDDeviceRef)g_Second);
        ASSERT_EQ(1, events.count);
        g_Remove(g_RemoveContext, 0, 0, (IOHIDDeviceRef)g_First);
        ASSERT_EQ(2, events.count);
        ASSERT_EQ(0, events.slot);
        ASSERT_EQ(WINDOW_GAMEPAD_EVENT_DISCONNECTED, events.last);
        ASSERT_FALSE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 0));
        ASSERT_TRUE(dmPlatform::GetDeviceState(window, WINDOW_DEVICE_STATE_JOYSTICK_PRESENT, 1));
        g_Arrive(g_ArriveContext, 0, 0, (IOHIDDeviceRef)g_First);
        ASSERT_EQ(3, events.count);
        ASSERT_EQ(0, events.slot);
        dmPlatform::SetGamepadEventCallback(window, 0, 0);
        dmPlatform::CloseWindow(window);
        dmPlatform::DeleteWindow(window);
        g_First = g_Second = nil;
    }
}
int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
