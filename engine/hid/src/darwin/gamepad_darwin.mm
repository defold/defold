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
#include <stdint.h>
#include <string.h>

#import <Foundation/Foundation.h>
#import <GameController/GameController.h>

#include <TargetConditionals.h>
#if TARGET_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <Kernel/IOKit/hidsystem/IOHIDUsageTables.h>
#endif

#include <dlib/dstrings.h>

#include <platform/window.hpp>
#include <platform/platform_window_constants.h>

#include "../external/sdl/joystick/usb_ids.h"

#include "hid_private.h"
#include "hid_native_private.h"

namespace dmHID
{

enum AppleGamepadHatState
{
    APPLE_GAMEPAD_HAT_CENTERED   = 0,
    APPLE_GAMEPAD_HAT_UP         = 1,
    APPLE_GAMEPAD_HAT_RIGHT      = 2,
    APPLE_GAMEPAD_HAT_DOWN       = 4,
    APPLE_GAMEPAD_HAT_LEFT       = 8,
    APPLE_GAMEPAD_HAT_RIGHT_UP   = APPLE_GAMEPAD_HAT_RIGHT | APPLE_GAMEPAD_HAT_UP,
    APPLE_GAMEPAD_HAT_RIGHT_DOWN = APPLE_GAMEPAD_HAT_RIGHT | APPLE_GAMEPAD_HAT_DOWN,
    APPLE_GAMEPAD_HAT_LEFT_UP    = APPLE_GAMEPAD_HAT_LEFT  | APPLE_GAMEPAD_HAT_UP,
    APPLE_GAMEPAD_HAT_LEFT_DOWN  = APPLE_GAMEPAD_HAT_LEFT  | APPLE_GAMEPAD_HAT_DOWN,
};

struct AppleGamepadDevice
{
    int          m_Id;
    GCController* m_Controller;
    Gamepad*     m_Gamepad;
    char         m_Name[MAX_GAMEPAD_NAME_LENGTH];
    GamepadGuid  m_Guid;
    uint8_t      m_AxisCount;
    uint8_t      m_ButtonCount;
    uint8_t      m_HatCount;
    uint8_t      m_HasLeftThumbstickButton : 1;
    uint8_t      m_HasRightThumbstickButton: 1;
    uint8_t      m_HasBackButton           : 1;
    uint8_t      m_HasStartButton          : 1;
    uint8_t                               : 4;
};

struct AppleGamepadDriver : GamepadDriver
{
    HContext                    m_HidContext;
    dmArray<AppleGamepadDevice> m_Devices;
    bool                        m_ObserversInstalled;
#if TARGET_OS_OSX
    IOHIDManagerRef             m_HidManager;
    CFRunLoopRef                m_RunLoop;
#endif
};

static id g_AppleGamepadConnectObserver = nil;
static id g_AppleGamepadDisconnectObserver = nil;
static AppleGamepadDriver* g_AppleGamepadDriver = 0;

static void GetGamepadDeviceNameInternal(HContext context, int gamepad_id, char name[MAX_GAMEPAD_NAME_LENGTH]);
static bool GetGamepadDeviceGuidInternal(HContext context, int gamepad_id, GamepadGuid* guid);

static bool IsControllerPS4(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"DualShock 4"])
            return true;
    }
    else if ([controller.vendorName containsString:@"DUALSHOCK"])
    {
        return true;
    }

    return false;
}

static bool IsControllerPS5(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"DualSense"])
            return true;
    }
    else if ([controller.vendorName containsString:@"DualSense"])
    {
        return true;
    }

    return false;
}

static bool IsControllerXbox(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"Xbox One"])
            return true;
    }
    else if ([controller.vendorName containsString:@"Xbox"])
    {
        return true;
    }

    return false;
}

static bool IsControllerSwitchPro(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"Switch Pro Controller"])
            return true;
    }

    return false;
}

static bool IsControllerSwitchJoyConL(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"Nintendo Switch Joy-Con (L)"])
            return true;
    }

    return false;
}

static bool IsControllerSwitchJoyConR(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"Nintendo Switch Joy-Con (R)"])
            return true;
    }

    return false;
}

static bool IsControllerSwitchJoyConPair(GCController* controller)
{
    if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if ([controller.productCategory isEqualToString:@"Nintendo Switch Joy-Con (L/R)"])
            return true;
    }

    return false;
}

static bool IsControllerBackboneOne(GCController* controller)
{
    return [controller.vendorName hasPrefix:@"Backbone One"];
}

static bool ElementAlreadyHandled(const GamepadIdentity& identity, NSString* element, NSDictionary<NSString*, GCControllerElement*>* elements)
{
    if ([element isEqualToString:@"Left Thumbstick Left"] ||
        [element isEqualToString:@"Left Thumbstick Right"])
    {
        if (elements[@"Left Thumbstick X Axis"])
            return true;
    }
    if ([element isEqualToString:@"Left Thumbstick Up"] ||
        [element isEqualToString:@"Left Thumbstick Down"])
    {
        if (elements[@"Left Thumbstick Y Axis"])
            return true;
    }
    if ([element isEqualToString:@"Right Thumbstick Left"] ||
        [element isEqualToString:@"Right Thumbstick Right"])
    {
        if (elements[@"Right Thumbstick X Axis"])
            return true;
    }
    if ([element isEqualToString:@"Right Thumbstick Up"] ||
        [element isEqualToString:@"Right Thumbstick Down"])
    {
        if (elements[@"Right Thumbstick Y Axis"])
            return true;
    }
    if ([element isEqualToString:@"Direction Pad X Axis"])
    {
        if (elements[@"Direction Pad Left"] && elements[@"Direction Pad Right"])
            return true;
    }
    if ([element isEqualToString:@"Direction Pad Y Axis"])
    {
        if (elements[@"Direction Pad Up"] && elements[@"Direction Pad Down"])
            return true;
    }
    if ([element isEqualToString:@"Cardinal Direction Pad X Axis"])
    {
        if (elements[@"Cardinal Direction Pad Left"] && elements[@"Cardinal Direction Pad Right"])
            return true;
    }
    if ([element isEqualToString:@"Cardinal Direction Pad Y Axis"])
    {
        if (elements[@"Cardinal Direction Pad Up"] && elements[@"Cardinal Direction Pad Down"])
            return true;
    }
    if ([element isEqualToString:@"Touchpad 1 X Axis"] ||
        [element isEqualToString:@"Touchpad 1 Y Axis"] ||
        [element isEqualToString:@"Touchpad 1 Left"] ||
        [element isEqualToString:@"Touchpad 1 Right"] ||
        [element isEqualToString:@"Touchpad 1 Up"] ||
        [element isEqualToString:@"Touchpad 1 Down"] ||
        [element isEqualToString:@"Touchpad 2 X Axis"] ||
        [element isEqualToString:@"Touchpad 2 Y Axis"] ||
        [element isEqualToString:@"Touchpad 2 Left"] ||
        [element isEqualToString:@"Touchpad 2 Right"] ||
        [element isEqualToString:@"Touchpad 2 Up"] ||
        [element isEqualToString:@"Touchpad 2 Down"])
    {
        return true;
    }
    if ([element isEqualToString:@"Button Home"] && identity.m_IsSwitchJoyConPair)
    {
        return true;
    }
    if ([element isEqualToString:@"Button Share"] && identity.m_IsBackboneOne)
    {
        return true;
    }

    return false;
}

static void ClassifyController(GCController* controller, GamepadIdentity* identity)
{
    memset(identity, 0, sizeof(*identity));

    const bool is_ps4               = IsControllerPS4(controller);
    const bool is_ps5               = IsControllerPS5(controller);
    const bool is_xbox              = IsControllerXbox(controller);
    const bool is_switch_pro        = IsControllerSwitchPro(controller);
    const bool is_switch_joycon_l   = IsControllerSwitchJoyConL(controller);
    const bool is_switch_joycon_r   = IsControllerSwitchJoyConR(controller);
    const bool is_switch_joycon_pair= IsControllerSwitchJoyConPair(controller);
    const bool is_backbone_one      = IsControllerBackboneOne(controller);

    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
    {
        GCPhysicalInputProfile* profile = controller.physicalInputProfile;
        if (profile != nil)
        {
            if (profile.buttons[GCInputDualShockTouchpadButton] != nil)
                identity->m_HasDualshockTouchpad = 1;
            if (profile.buttons[GCInputXboxPaddleOne] != nil)
                identity->m_HasXboxPaddles = 1;
            if (profile.buttons[@"Button Share"] != nil)
                identity->m_HasXboxShareButton = 1;
        }
    }

    identity->m_IsBackboneOne = is_backbone_one ? 1 : 0;
    identity->m_IsSwitchJoyConPair = is_switch_joycon_pair ? 1 : 0;

    if (is_backbone_one)
    {
        identity->m_Vendor = USB_VENDOR_BACKBONE;
        identity->m_Product = is_ps5 ? USB_PRODUCT_BACKBONE_ONE_IOS_PS5 : USB_PRODUCT_BACKBONE_ONE_IOS;
        return;
    }

    if (is_xbox)
    {
        identity->m_Vendor = USB_VENDOR_MICROSOFT;
        if (identity->m_HasXboxPaddles)
            identity->m_Product = USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH;
        else if (identity->m_HasXboxShareButton)
            identity->m_Product = USB_PRODUCT_XBOX_SERIES_X_BLE;
        else
            identity->m_Product = USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH;
        return;
    }

    if (is_ps4)
    {
        identity->m_Vendor = USB_VENDOR_SONY;
        identity->m_Product = USB_PRODUCT_SONY_DS4_SLIM;
        return;
    }

    if (is_ps5)
    {
        identity->m_Vendor = USB_VENDOR_SONY;
        identity->m_Product = USB_PRODUCT_SONY_DS5;
        return;
    }

    if (is_switch_pro)
    {
        identity->m_Vendor = USB_VENDOR_NINTENDO;
        identity->m_Product = USB_PRODUCT_NINTENDO_SWITCH_PRO;
        return;
    }

    if (is_switch_joycon_pair)
    {
        identity->m_Vendor = USB_VENDOR_NINTENDO;
        identity->m_Product = USB_PRODUCT_NINTENDO_SWITCH_JOYCON_PAIR;
        return;
    }

    if (is_switch_joycon_l)
    {
        identity->m_Vendor = USB_VENDOR_NINTENDO;
        identity->m_Product = USB_PRODUCT_NINTENDO_SWITCH_JOYCON_LEFT;
        return;
    }

    if (is_switch_joycon_r)
    {
        identity->m_Vendor = USB_VENDOR_NINTENDO;
        identity->m_Product = USB_PRODUCT_NINTENDO_SWITCH_JOYCON_RIGHT;
        return;
    }

    identity->m_Vendor = USB_VENDOR_APPLE;
    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
        identity->m_Product = 4;
    else
        identity->m_Product = 1;
}

static uint16_t GetLegacyButtonMask(GCController* controller)
{
    uint16_t button_mask = 0;
    button_mask |= (1u << 0);
    button_mask |= (1u << 1);
    button_mask |= (1u << 2);
    button_mask |= (1u << 3);
    button_mask |= (1u << 9);
    button_mask |= (1u << 10);

    GCExtendedGamepad* gamepad = controller.extendedGamepad;
    if (gamepad != nil)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
        {
            if (gamepad.leftThumbstickButton)
                button_mask |= (1u << 7);
            if (gamepad.rightThumbstickButton)
                button_mask |= (1u << 8);
        }
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            if (gamepad.buttonOptions)
                button_mask |= (1u << 4);
        }
    }

    button_mask |= (1u << 6);
    return button_mask;
}

#if TARGET_OS_OSX
static bool GetDeviceNumberProperty(IOHIDDeviceRef device_ref, CFStringRef key, uint32_t* value)
{
    CFTypeRef property = IOHIDDeviceGetProperty(device_ref, key);
    if (property == 0 || CFGetTypeID(property) != CFNumberGetTypeID())
    {
        return false;
    }

    return CFNumberGetValue((CFNumberRef) property, kCFNumberSInt32Type, value);
}

static bool BuildGamepadGUID(IOHIDDeviceRef device_ref, GamepadGuid* guid)
{
    uint32_t vendor = 0;
    uint32_t product = 0;
    uint32_t version = 0;

    GetDeviceNumberProperty(device_ref, CFSTR(kIOHIDVendorIDKey), &vendor);
    GetDeviceNumberProperty(device_ref, CFSTR(kIOHIDProductIDKey), &product);
    GetDeviceNumberProperty(device_ref, CFSTR(kIOHIDVersionNumberKey), &version);

    if (vendor && product)
    {
        char guid_string[MAX_GAMEPAD_GUID_LENGTH + 1];
        CreateGUIDFromProduct((uint16_t) vendor, (uint16_t) product, (uint16_t) version, guid_string);
        return ParseGamepadGuid(guid_string, guid);
    }

    return false;
}

static CFMutableDictionaryRef CreateMatchingDictionary(long usage_page, long usage)
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    if (dict == 0)
    {
        return 0;
    }

    CFNumberRef page_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &usage_page);
    CFNumberRef usage_ref = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &usage);
    if (page_ref && usage_ref)
    {
        CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsagePageKey), page_ref);
        CFDictionarySetValue(dict, CFSTR(kIOHIDDeviceUsageKey), usage_ref);
    }

    if (page_ref)
        CFRelease(page_ref);
    if (usage_ref)
        CFRelease(usage_ref);

    return dict;
}

static bool CreateGamePadGuid(AppleGamepadDriver* driver, GCController* controller, const char* fallback_name, GamepadGuid* guid)
{
    (void) fallback_name;

    if (driver->m_HidManager == 0)
    {
        return false;
    }

    if (@available(macOS 11.0, *))
    {
    }
    else
    {
        return false;
    }

    CFSetRef devices = IOHIDManagerCopyDevices(driver->m_HidManager);
    if (devices == 0)
    {
        return false;
    }

    CFIndex count = CFSetGetCount(devices);
    if (count > 0)
    {
        IOHIDDeviceRef* device_refs = new IOHIDDeviceRef[count];
        memset(device_refs, 0, sizeof(IOHIDDeviceRef) * count);
        CFSetGetValues(devices, (const void**) device_refs);

        for (CFIndex i = 0; i < count; ++i)
        {
            IOHIDDeviceRef device_ref = device_refs[i];
            if (!device_ref)
            {
                continue;
            }

            if (@available(macOS 11.0, *))
            {
                if ([GCController supportsHIDDevice:device_ref])
                {
                    if (BuildGamepadGUID(device_ref, guid))
                        break;
                }
            }
        }

        delete[] device_refs;
    }

    CFRelease(devices);
    return true;
}
#endif

static uint8_t GetHatValue(GCControllerDirectionPad* dpad)
{
    uint8_t hat = APPLE_GAMEPAD_HAT_CENTERED;

    if (dpad.up.isPressed)
        hat |= APPLE_GAMEPAD_HAT_UP;
    else if (dpad.down.isPressed)
        hat |= APPLE_GAMEPAD_HAT_DOWN;

    if (dpad.left.isPressed)
        hat |= APPLE_GAMEPAD_HAT_LEFT;
    else if (dpad.right.isPressed)
        hat |= APPLE_GAMEPAD_HAT_RIGHT;

    return hat;
}

static bool SupportsController(GCController* controller, AppleGamepadDevice* device)
{
    memset(device, 0, sizeof(*device));

    if (controller.extendedGamepad)
    {
        GCExtendedGamepad* gamepad = controller.extendedGamepad;

        device->m_AxisCount = 6;
        device->m_HatCount = 1;
        device->m_ButtonCount = 6;

        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
        {
            device->m_HasLeftThumbstickButton = gamepad.leftThumbstickButton != nil;
            device->m_HasRightThumbstickButton = gamepad.rightThumbstickButton != nil;
            device->m_ButtonCount += device->m_HasLeftThumbstickButton ? 1 : 0;
            device->m_ButtonCount += device->m_HasRightThumbstickButton ? 1 : 0;
        }

        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            device->m_HasBackButton = gamepad.buttonOptions != nil;
            device->m_HasStartButton = gamepad.buttonMenu != nil;
            device->m_ButtonCount += device->m_HasBackButton ? 1 : 0;
            device->m_ButtonCount += device->m_HasStartButton ? 1 : 0;
        }

        return true;
    }

    return false;
}

static void CreateAppleGameControllerGUID(GCController* controller, const char* fallback_name, GamepadGuid* guid)
{
    GamepadIdentity identity = {};
    ClassifyController(controller, &identity);

    const char* axis_keys[64];
    const char* button_keys[64];
    uint32_t axis_count = 0;
    uint32_t button_count = 0;

    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
    {
        GCPhysicalInputProfile* profile = controller.physicalInputProfile;
        if (profile != nil)
        {
            NSDictionary<NSString*, GCControllerElement*>* elements = profile.elements;
            if (elements != nil)
            {
                NSArray<NSString*>* keys = [[elements allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
                for (NSString* key in keys)
                {
                    if (ElementAlreadyHandled(identity, key, elements))
                        continue;

                    GCControllerElement* element = elements[key];
                    if (element.analog)
                    {
                        if ([element isKindOfClass:[GCControllerAxisInput class]] ||
                            [element isKindOfClass:[GCControllerButtonInput class]])
                        {
                            if (axis_count < sizeof(axis_keys) / sizeof(axis_keys[0]))
                                axis_keys[axis_count++] = key.UTF8String;
                        }
                    }
                }

                for (NSString* key in keys)
                {
                    if (ElementAlreadyHandled(identity, key, elements))
                        continue;

                    GCControllerElement* element = elements[key];
                    if ([element isKindOfClass:[GCControllerButtonInput class]])
                    {
                        if (button_count < sizeof(button_keys) / sizeof(button_keys[0]))
                            button_keys[button_count++] = key.UTF8String;
                    }
                }
            }
        }
    }

    const uint16_t button_mask = GetLegacyButtonMask(controller);
    char guid_string[MAX_GAMEPAD_GUID_LENGTH + 1];
    CreateGUIDFromIdentity(identity, fallback_name, axis_keys, axis_count, button_keys, button_count, button_mask, guid_string);
    ParseGamepadGuid(guid_string, guid);
}

static AppleGamepadDevice* GetAppleGamepadDevice(AppleGamepadDriver* driver, int gamepad_id)
{
    for (int i = 0; i < driver->m_Devices.Size(); ++i)
    {
        if (driver->m_Devices[i].m_Id == gamepad_id)
            return &driver->m_Devices[i];
    }

    return 0;
}

static AppleGamepadDevice* GetAppleGamepadDevice(AppleGamepadDriver* driver, GCController* controller)
{
    for (int i = 0; i < driver->m_Devices.Size(); ++i)
    {
        if (driver->m_Devices[i].m_Controller == controller)
            return &driver->m_Devices[i];
    }

    return 0;
}

static Gamepad* GetGamepad(AppleGamepadDriver* driver, int gamepad_id)
{
    AppleGamepadDevice* device = GetAppleGamepadDevice(driver, gamepad_id);
    return device ? device->m_Gamepad : 0;
}

static int UnpackGamepad(AppleGamepadDriver* driver, Gamepad* gamepad, AppleGamepadDevice** gamepad_device_out)
{
    for (int i = 0; i < driver->m_Devices.Size(); ++i)
    {
        if (driver->m_Devices[i].m_Gamepad == gamepad)
        {
            if (gamepad_device_out)
                *gamepad_device_out = &driver->m_Devices[i];
            return driver->m_Devices[i].m_Id;
        }
    }

    return -1;
}

static int AllocateGamepadId(AppleGamepadDriver* driver)
{
    for (int gamepad_id = 0; gamepad_id < MAX_GAMEPAD_COUNT; ++gamepad_id)
    {
        if (GetAppleGamepadDevice(driver, gamepad_id) == 0)
            return gamepad_id;
    }

    return -1;
}

static Gamepad* EnsureAllocatedGamepad(AppleGamepadDriver* driver, int gamepad_id, GCController* controller)
{
    Gamepad* gp = GetGamepad(driver, gamepad_id);
    if (gp != 0)
        return gp;

    gp = CreateGamepad(driver->m_HidContext, driver);
    if (gp == 0)
        return 0;

    AppleGamepadDevice new_device = {};
    if (!SupportsController(controller, &new_device))
    {
        ReleaseGamepad(driver->m_HidContext, gp);
        return 0;
    }

    new_device.m_Id = gamepad_id;
    new_device.m_Gamepad = gp;
    new_device.m_Controller = (__bridge GCController*) CFBridgingRetain(controller);

    const char* name = 0;
    if (controller.vendorName)
    {
        name = controller.vendorName.UTF8String;
    }
    else if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
    {
        if (controller.productCategory)
            name = controller.productCategory.UTF8String;
    }

    if (!name)
        name = "Game Controller";

    dmStrlCpy(new_device.m_Name, name, sizeof(new_device.m_Name));

    if (!CreateGamePadGuid(driver, controller, new_device.m_Name, &new_device.m_Guid))
    {
        CreateAppleGameControllerGUID(controller, new_device.m_Name, &new_device.m_Guid);
    }

    if (driver->m_Devices.Full())
    {
        driver->m_Devices.OffsetCapacity(1);
    }

    driver->m_Devices.Push(new_device);
    SetGamepadConnectionStatus(driver->m_HidContext, gp, true);
    return gp;
}

static void RemoveGamepad(AppleGamepadDriver* driver, int gamepad_id)
{
    for (int i = 0; i < driver->m_Devices.Size(); ++i)
    {
        if (driver->m_Devices[i].m_Id == gamepad_id)
        {
            SetGamepadConnectionStatus(driver->m_HidContext, driver->m_Devices[i].m_Gamepad, false);
            ReleaseGamepad(driver->m_HidContext, driver->m_Devices[i].m_Gamepad);

            if (driver->m_Devices[i].m_Controller)
            {
                CFBridgingRelease((__bridge CFTypeRef) driver->m_Devices[i].m_Controller);
                driver->m_Devices[i].m_Controller = nil;
            }

            driver->m_Devices.EraseSwap(i);
            return;
        }
    }
}

static void AppleGamepadControllerConnected(GCController* controller)
{
    AppleGamepadDriver* driver = g_AppleGamepadDriver;
    if (driver == 0 || GetAppleGamepadDevice(driver, controller) != 0)
    {
        return;
    }

    int gamepad_id = AllocateGamepadId(driver);
    if (gamepad_id == -1)
    {
        return;
    }

    EnsureAllocatedGamepad(driver, gamepad_id, controller);
}

static void AppleGamepadControllerDisconnected(GCController* controller)
{
    AppleGamepadDriver* driver = g_AppleGamepadDriver;
    if (driver == 0)
    {
        return;
    }

    AppleGamepadDevice* device = GetAppleGamepadDevice(driver, controller);
    if (device)
    {
        RemoveGamepad(driver, device->m_Id);
    }
}

static void AppleGamepadDriverUpdate(HContext context, GamepadDriver* driver, Gamepad* gamepad)
{
    (void) context;

    AppleGamepadDevice* apple_device = 0;
    int id = UnpackGamepad((AppleGamepadDriver*) driver, gamepad, &apple_device);
    assert(id != -1);

    GamepadPacket& packet = gamepad->m_Packet;
    memset(packet.m_Axis, 0, sizeof(packet.m_Axis));
    memset(packet.m_Buttons, 0, sizeof(packet.m_Buttons));
    memset(packet.m_Hat, 0, sizeof(packet.m_Hat));

    gamepad->m_AxisCount = apple_device->m_AxisCount;
    gamepad->m_ButtonCount = apple_device->m_ButtonCount;
    gamepad->m_HatCount = apple_device->m_HatCount;

    GCController* controller = apple_device->m_Controller;
    if (controller == nil)
    {
        return;
    }

    GCExtendedGamepad* extended_gamepad = controller.extendedGamepad;
    if (extended_gamepad == nil)
    {
        return;
    }

    packet.m_Axis[0] = extended_gamepad.leftThumbstick.xAxis.value;
    packet.m_Axis[1] = -extended_gamepad.leftThumbstick.yAxis.value;
    packet.m_Axis[2] = extended_gamepad.leftTrigger.value * 2.0f - 1.0f;
    packet.m_Axis[3] = extended_gamepad.rightThumbstick.xAxis.value;
    packet.m_Axis[4] = -extended_gamepad.rightThumbstick.yAxis.value;
    packet.m_Axis[5] = extended_gamepad.rightTrigger.value * 2.0f - 1.0f;

    uint32_t button_index = 0;
    if (extended_gamepad.buttonA.isPressed)             packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32); ++button_index;
    if (extended_gamepad.buttonB.isPressed)             packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32); ++button_index;
    if (extended_gamepad.buttonX.isPressed)             packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32); ++button_index;
    if (extended_gamepad.buttonY.isPressed)             packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32); ++button_index;
    if (extended_gamepad.leftShoulder.isPressed)        packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32); ++button_index;
    if (extended_gamepad.rightShoulder.isPressed)       packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32); ++button_index;

    if (apple_device->m_HasLeftThumbstickButton)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
        {
            if (extended_gamepad.leftThumbstickButton.isPressed)
                packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32);
        }
        ++button_index;
    }

    if (apple_device->m_HasRightThumbstickButton)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
        {
            if (extended_gamepad.rightThumbstickButton.isPressed)
                packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32);
        }
        ++button_index;
    }

    if (apple_device->m_HasBackButton)
    {
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            if (extended_gamepad.buttonOptions.isPressed)
                packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32);
        }
        ++button_index;
    }

    if (apple_device->m_HasStartButton)
    {
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            if (extended_gamepad.buttonMenu.isPressed)
                packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32);
        }
        ++button_index;
    }

    packet.m_Hat[0] = GetHatValue(extended_gamepad.dpad);
}

static void InstallObservers(void)
{
    if (g_AppleGamepadConnectObserver != nil || g_AppleGamepadDisconnectObserver != nil)
    {
        return;
    }

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    g_AppleGamepadConnectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                                        object:nil
                                                         queue:nil
                                                    usingBlock:^(NSNotification* note) {
        AppleGamepadControllerConnected((GCController*) note.object);
    }];

    g_AppleGamepadDisconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                           object:nil
                                                            queue:nil
                                                       usingBlock:^(NSNotification* note) {
        AppleGamepadControllerDisconnected((GCController*) note.object);
    }];
}

static void RemoveObservers(void)
{
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    if (g_AppleGamepadConnectObserver != nil)
    {
        [center removeObserver:g_AppleGamepadConnectObserver name:GCControllerDidConnectNotification object:nil];
        g_AppleGamepadConnectObserver = nil;
    }

    if (g_AppleGamepadDisconnectObserver != nil)
    {
        [center removeObserver:g_AppleGamepadDisconnectObserver name:GCControllerDidDisconnectNotification object:nil];
        g_AppleGamepadDisconnectObserver = nil;
    }
}

static void AppleGamepadDriverDetectDevices(HContext context, GamepadDriver* _driver)
{
    (void) context;

    AppleGamepadDriver* driver = (AppleGamepadDriver*) _driver;
    if (!driver->m_ObserversInstalled)
    {
        InstallObservers();
        driver->m_ObserversInstalled = true;
    }

    @autoreleasepool
    {
        for (GCController* controller in [GCController controllers])
        {
            AppleGamepadControllerConnected(controller);
        }
    }
}

static void GetGamepadDeviceNameInternal(HContext context, int gamepad_id, char name[MAX_GAMEPAD_NAME_LENGTH])
{
    (void) context;

    AppleGamepadDevice* device = GetAppleGamepadDevice(g_AppleGamepadDriver, gamepad_id);
    if (device)
    {
        dmStrlCpy(name, device->m_Name, MAX_GAMEPAD_NAME_LENGTH);
    }
    else
    {
        dmStrlCpy(name, "_", MAX_GAMEPAD_NAME_LENGTH);
    }
}

static void AppleGamepadDriverGetGamepadDeviceName(HContext context, GamepadDriver* driver, HGamepad gamepad, char name[MAX_GAMEPAD_NAME_LENGTH])
{
    uint32_t gamepad_index = UnpackGamepad((AppleGamepadDriver*) driver, gamepad, 0);
    GetGamepadDeviceNameInternal(context, gamepad_index, name);
}

static bool GetGamepadDeviceGuidInternal(HContext context, int gamepad_id, GamepadGuid* guid)
{
    (void) context;

    AppleGamepadDevice* device = GetAppleGamepadDevice(g_AppleGamepadDriver, gamepad_id);
    if (device)
    {
        *guid = device->m_Guid;
        return true;
    }

    return false;
}

static bool AppleGamepadDriverGetGamepadDeviceGuid(HContext context, GamepadDriver* driver, HGamepad gamepad, GamepadGuid* guid)
{
    uint32_t gamepad_index = UnpackGamepad((AppleGamepadDriver*) driver, gamepad, 0);
    return GetGamepadDeviceGuidInternal(context, gamepad_index, guid);
}

static bool AppleGamepadDriverInitialize(HContext context, GamepadDriver* driver)
{
    if (!dmPlatform::GetWindowStateParam(context->m_Window, WINDOW_STATE_OPENED))
    {
        return false;
    }

    if (![GCController class])
    {
        return false;
    }

    AppleGamepadDriver* apple_driver = (AppleGamepadDriver*) driver;
    apple_driver->m_ObserversInstalled = false;

    // We do this in order to be able to create our GUID's on macOS
#if TARGET_OS_OSX
    apple_driver->m_RunLoop = CFRunLoopGetMain();
    apple_driver->m_HidManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (apple_driver->m_HidManager != 0)
    {
        const long usages[] =
        {
            kHIDUsage_GD_Joystick,
            kHIDUsage_GD_GamePad,
            kHIDUsage_GD_MultiAxisController
        };

        CFMutableArrayRef matching = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (matching != 0)
        {
            for (uint32_t i = 0; i < sizeof(usages) / sizeof(usages[0]); ++i)
            {
                CFMutableDictionaryRef dict = CreateMatchingDictionary(kHIDPage_GenericDesktop, usages[i]);
                if (dict)
                {
                    CFArrayAppendValue(matching, dict);
                    CFRelease(dict);
                }
            }

            IOHIDManagerSetDeviceMatchingMultiple(apple_driver->m_HidManager, matching);
            CFRelease(matching);
        }

        IOHIDManagerScheduleWithRunLoop(apple_driver->m_HidManager, apple_driver->m_RunLoop, kCFRunLoopDefaultMode);
        if (IOHIDManagerOpen(apple_driver->m_HidManager, kIOHIDOptionsTypeNone) != kIOReturnSuccess)
        {
            IOHIDManagerUnscheduleFromRunLoop(apple_driver->m_HidManager, apple_driver->m_RunLoop, kCFRunLoopDefaultMode);
            CFRelease(apple_driver->m_HidManager);
            apple_driver->m_HidManager = 0;
        }
    }
#endif
    return true;
}

static void AppleGamepadDriverDestroy(HContext context, GamepadDriver* _driver)
{
    (void) context;

    AppleGamepadDriver* driver = (AppleGamepadDriver*) _driver;
    assert(g_AppleGamepadDriver == driver);

    while (driver->m_Devices.Size() > 0)
    {
        RemoveGamepad(driver, driver->m_Devices[0].m_Id);
    }

    if (driver->m_ObserversInstalled)
    {
        RemoveObservers();
    }

#if TARGET_OS_OSX
    if (driver->m_HidManager)
    {
        IOHIDManagerUnscheduleFromRunLoop(driver->m_HidManager, driver->m_RunLoop, kCFRunLoopDefaultMode);
        IOHIDManagerClose(driver->m_HidManager, kIOHIDOptionsTypeNone);
        CFRelease(driver->m_HidManager);
        driver->m_HidManager = 0;
    }
#endif

    delete driver;
    g_AppleGamepadDriver = 0;
}

GamepadDriver* CreateGamepadDriverApple(HContext context)
{
    AppleGamepadDriver* driver = new AppleGamepadDriver();

    driver->m_Initialize           = AppleGamepadDriverInitialize;
    driver->m_Destroy              = AppleGamepadDriverDestroy;
    driver->m_Update               = AppleGamepadDriverUpdate;
    driver->m_DetectDevices        = AppleGamepadDriverDetectDevices;
    driver->m_GetGamepadDeviceName = AppleGamepadDriverGetGamepadDeviceName;
    driver->m_GetGamepadDeviceGuid = AppleGamepadDriverGetGamepadDeviceGuid;

    assert(g_AppleGamepadDriver == 0);
    g_AppleGamepadDriver = driver;
    g_AppleGamepadDriver->m_HidContext = context;

    return driver;
}

} // namespace dmHID
