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
#include <math.h>
#include <stdlib.h>
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
#include <dlib/math.h>

#include <platform/window.hpp>
#include <platform/platform_window_constants.h>

#include "../external/sdl/joystick/usb_ids.h"

#include "hid_private.h"
#include "hid_native_private.h"

#if TARGET_OS_OSX
#define _GLFW_COCOA 1
#define _glfwDefaultMappings dmHIDAppleGamepadDefaultMappings
#include "../external/glfw/mappings.h"
#undef _glfwDefaultMappings
#undef _GLFW_COCOA
#endif

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

enum AppleGamepadSemanticAxis
{
    APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_X = 0,
    APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_Y,
    APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_X,
    APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_Y,
    APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_TRIGGER,
    APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_TRIGGER,
    APPLE_GAMEPAD_SEMANTIC_AXIS_COUNT
};

enum AppleGamepadSemanticButton
{
    APPLE_GAMEPAD_SEMANTIC_BUTTON_A = 0,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_B,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_X,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_Y,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_BACK,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_START,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_GUIDE,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_THUMB,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_THUMB,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_SHOULDER,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_SHOULDER,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_UP,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_DOWN,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_LEFT,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_RIGHT,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_CAPTURE,
    APPLE_GAMEPAD_SEMANTIC_BUTTON_COUNT
};

enum AppleGamepadSemanticHat
{
    APPLE_GAMEPAD_SEMANTIC_HAT_DPAD = 0,
    APPLE_GAMEPAD_SEMANTIC_HAT_COUNT
};

enum AppleGamepadPacketLayout
{
    APPLE_GAMEPAD_PACKET_LAYOUT_SDL = 0,
    APPLE_GAMEPAD_PACKET_LAYOUT_GLFW,
};

enum AppleGamepadLegacyElementType
{
    APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_NONE = 0,
    APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS,
    APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON,
    APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT,
};

static const uint8_t APPLE_GAMEPAD_REMAP_INVALID = 0xff;

struct AppleGamepadLegacyElement
{
    uint8_t m_Type;
    uint8_t m_Index;
    int8_t  m_Minimum;
    int8_t  m_Maximum;
};

struct AppleGamepadDevice
{
    int           m_Id;
    GCController* m_Controller;
    Gamepad*      m_Gamepad;
    char          m_Name[MAX_GAMEPAD_NAME_LENGTH];
    GamepadGuid   m_Guid;
    uint8_t       m_AxisCount;
    uint8_t       m_ButtonCount;
    uint8_t       m_HatCount;
    uint8_t       m_LegacyAxisCount;
    uint8_t       m_LegacyButtonCount;
    uint8_t       m_LegacyHatCount;
    uint8_t       m_PacketLayout;
    uint8_t       m_AxisRemap[MAX_GAMEPAD_AXIS_COUNT];
    uint8_t       m_ButtonRemap[MAX_GAMEPAD_BUTTON_COUNT];
    uint8_t       m_HatRemap[MAX_GAMEPAD_HAT_COUNT];
    AppleGamepadLegacyElement m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_COUNT];
    AppleGamepadLegacyElement m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_COUNT];
    uint8_t       m_HasLeftThumbstickButton : 1;
    uint8_t      m_HasRightThumbstickButton: 1;
    uint8_t      m_HasBackButton           : 1;
    uint8_t      m_HasStartButton          : 1;
    uint8_t      m_HasGuideButton          : 1;
    uint8_t      m_HasCaptureButton        : 1;
    uint8_t      m_HasLegacyMapping       : 1;
    uint8_t                               : 1;
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

// Controller-family predicates used to normalize Apple's product categories into
// stable vendor/product pairs for GUID generation and legacy remapping.
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

// Filters out duplicate or synthetic GameController elements so the GUID
// signature only includes the canonical controls for the device.
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

// Resolves the guide/home button across the different APIs and string aliases
// Apple has used over time.
static GCControllerButtonInput* GetGuideButton(GCController* controller)
{
    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
    {
        GCExtendedGamepad* gamepad = controller.extendedGamepad;
        if (gamepad != nil && gamepad.buttonHome != nil)
            return gamepad.buttonHome;

        GCPhysicalInputProfile* profile = controller.physicalInputProfile;
        if (profile != nil)
        {
            GCControllerButtonInput* button = profile.buttons[GCInputButtonHome];
            if (button != nil)
                return button;

            button = profile.buttons[@"Button Home"];
            if (button != nil)
                return button;
        }
    }

    return nil;
}

// Resolves the capture/share button when the platform exposes one.
static GCControllerButtonInput* GetCaptureButton(GCController* controller)
{
    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
    {
        GCPhysicalInputProfile* profile = controller.physicalInputProfile;
        if (profile != nil)
        {
            GCControllerButtonInput* button = nil;
            if (@available(macOS 12.0, iOS 15.0, tvOS 15.0, *))
            {
                button = profile.buttons[GCInputButtonShare];
            }
            if (button == nil)
                button = profile.buttons[@"Button Share"];
            if (button != nil)
                return button;
        }
    }

    return nil;
}

// Converts Apple's controller metadata into the SDL-style identity used by the
// GUID builder and controller-name normalization.
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

// Builds a legacy button presence mask for platforms where the detailed
// physical-input profile is not available to seed the GUID signature.
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
// Reads a numeric IOHID property into an unsigned integer.
static bool GetDeviceNumberProperty(IOHIDDeviceRef device_ref, CFStringRef key, uint32_t* value)
{
    CFTypeRef property = IOHIDDeviceGetProperty(device_ref, key);
    if (property == 0 || CFGetTypeID(property) != CFNumberGetTypeID())
    {
        return false;
    }

    return CFNumberGetValue((CFNumberRef) property, kCFNumberSInt32Type, value);
}

// Extracts the raw USB identity fields from a HID device for GUID generation.
static bool GetDeviceIdentity(IOHIDDeviceRef device_ref, uint32_t* vendor, uint32_t* product, uint32_t* version)
{
    *vendor = 0;
    *product = 0;
    *version = 0;

    GetDeviceNumberProperty(device_ref, CFSTR(kIOHIDVendorIDKey), vendor);
    GetDeviceNumberProperty(device_ref, CFSTR(kIOHIDProductIDKey), product);
    GetDeviceNumberProperty(device_ref, CFSTR(kIOHIDVersionNumberKey), version);

    return *vendor != 0 && *product != 0;
}

// Converts raw vendor/product/version values to the parsed SDL GUID struct.
static bool BuildGamepadGUID(uint32_t vendor, uint32_t product, uint32_t version, GamepadGuid* guid)
{
    char guid_string[MAX_GAMEPAD_GUID_LENGTH + 1];
    CreateGUIDFromProduct((uint16_t) vendor, (uint16_t) product, (uint16_t) version, guid_string);
    return ParseGamepadGuid(guid_string, guid);
}

// Checks whether a candidate HID device identity is plausible for the supplied
// GCController before we trust it as that controller's GUID source.
static bool MatchesControllerHIDDevice(GCController* controller, uint32_t vendor, uint32_t product)
{
    if (IsControllerBackboneOne(controller))
    {
        if (vendor != USB_VENDOR_BACKBONE)
            return false;

        if (IsControllerPS5(controller))
            return product == USB_PRODUCT_BACKBONE_ONE_IOS_PS5;

        return product == USB_PRODUCT_BACKBONE_ONE_IOS;
    }

    if (IsControllerPS4(controller))
        return vendor == USB_VENDOR_SONY && (product == USB_PRODUCT_SONY_DS4 || product == USB_PRODUCT_SONY_DS4_SLIM);

    if (IsControllerPS5(controller))
        return vendor == USB_VENDOR_SONY && (product == USB_PRODUCT_SONY_DS5 || product == USB_PRODUCT_SONY_DS5_EDGE);

    if (IsControllerXbox(controller))
        return vendor == USB_VENDOR_MICROSOFT &&
            (product == USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH ||
             product == USB_PRODUCT_XBOX_SERIES_X_BLE ||
             product == USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH);

    if (IsControllerSwitchPro(controller))
        return vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_PRO;

    if (IsControllerSwitchJoyConPair(controller))
        return vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_PAIR;

    if (IsControllerSwitchJoyConL(controller))
        return vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_LEFT;

    if (IsControllerSwitchJoyConR(controller))
        return vendor == USB_VENDOR_NINTENDO && product == USB_PRODUCT_NINTENDO_SWITCH_JOYCON_RIGHT;

    return false;
}

// Creates an IOHID matching dictionary for the requested usage page/usage pair.
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

// On macOS, attempts to build the controller GUID from the backing IOHID
// device. The match must be unique; otherwise the caller falls back to the
// GameController-profile-based GUID path.
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
    bool found_guid = false;
    uint32_t matching_device_count = 0;
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
                    uint32_t vendor = 0;
                    uint32_t product = 0;
                    uint32_t version = 0;
                    if (!GetDeviceIdentity(device_ref, &vendor, &product, &version))
                        continue;

                    if (!MatchesControllerHIDDevice(controller, vendor, product))
                        continue;

                    ++matching_device_count;
                    if (matching_device_count > 1)
                    {
                        found_guid = false;
                        break;
                    }

                    found_guid = BuildGamepadGUID(vendor, product, version, guid);
                }
            }
        }

        delete[] device_refs;
    }

    CFRelease(devices);
    return found_guid;
}
#endif

// Converts Apple's pressed-state dpad to the packed hat representation used by
// the engine packet.
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

// Resets all legacy remap state before applying either a database mapping or
// the default semantic layout.
static void ResetLegacyMapping(AppleGamepadDevice* device)
{
    memset(device->m_AxisRemap, APPLE_GAMEPAD_REMAP_INVALID, sizeof(device->m_AxisRemap));
    memset(device->m_ButtonRemap, APPLE_GAMEPAD_REMAP_INVALID, sizeof(device->m_ButtonRemap));
    memset(device->m_HatRemap, APPLE_GAMEPAD_REMAP_INVALID, sizeof(device->m_HatRemap));
    memset(device->m_LegacyAxisMap, 0, sizeof(device->m_LegacyAxisMap));
    memset(device->m_LegacyButtonMap, 0, sizeof(device->m_LegacyButtonMap));

    device->m_LegacyAxisCount = device->m_AxisCount;
    device->m_LegacyButtonCount = 0;
    device->m_LegacyHatCount = device->m_HatCount;
    device->m_HasLegacyMapping = 0;
}

// Builds the default semantic-to-packet remap used when no legacy database
// mapping is available.
static void BuildDefaultDeviceRemap(AppleGamepadDevice* device)
{
    device->m_AxisRemap[0] = APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_X;
    device->m_AxisRemap[1] = APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_Y;
    device->m_AxisRemap[2] = APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_TRIGGER;
    device->m_AxisRemap[3] = APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_X;
    device->m_AxisRemap[4] = APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_Y;
    device->m_AxisRemap[5] = APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_TRIGGER;

    uint32_t button_index = 0;
    device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_A;
    device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_B;
    device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_X;
    device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_Y;
    device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_SHOULDER;
    device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_SHOULDER;

    if (device->m_HasLeftThumbstickButton)
        device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_THUMB;
    if (device->m_HasRightThumbstickButton)
        device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_THUMB;
    if (device->m_HasBackButton)
        device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_BACK;
    if (device->m_HasStartButton)
        device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_START;

    if (device->m_HasGuideButton)
        device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_GUIDE;
    if (device->m_HasCaptureButton)
        device->m_ButtonRemap[button_index++] = APPLE_GAMEPAD_SEMANTIC_BUTTON_CAPTURE;

    if (device->m_HatCount > 0)
        device->m_HatRemap[0] = APPLE_GAMEPAD_SEMANTIC_HAT_DPAD;

    device->m_LegacyButtonCount = device->m_ButtonCount + device->m_HatCount * 4;
}

// Writes a button bit into the packet if the index is in range.
static void SetButtonValue(GamepadPacket& packet, uint32_t button_index, bool value)
{
    if (button_index >= MAX_GAMEPAD_BUTTON_COUNT)
        return;

    if (value)
        packet.m_Buttons[button_index / 32] |= 1 << (button_index % 32);
}

// Returns the current packet counts for the active layout, accounting for
// loaded legacy mappings.
static uint8_t GetLegacyButtonCount(const AppleGamepadDevice* device)
{
    return device->m_HasLegacyMapping ? device->m_LegacyButtonCount : (device->m_ButtonCount + device->m_HatCount * 4);
}

static uint8_t GetLegacyAxisCount(const AppleGamepadDevice* device)
{
    return device->m_HasLegacyMapping ? device->m_LegacyAxisCount : device->m_AxisCount;
}

static uint8_t GetLegacyHatCount(const AppleGamepadDevice* device)
{
    return device->m_HasLegacyMapping ? device->m_LegacyHatCount : device->m_HatCount;
}

// Reads or writes button bits without exposing the packet bitset layout at call
// sites.
static bool GetButtonValue(const GamepadPacket& packet, uint32_t button_index)
{
    if (button_index >= MAX_GAMEPAD_BUTTON_COUNT)
        return false;

    return (packet.m_Buttons[button_index / 32] & (1u << (button_index % 32))) != 0;
}

static void SetButtonState(GamepadPacket& packet, uint32_t button_index, bool value)
{
    if (button_index >= MAX_GAMEPAD_BUTTON_COUNT)
        return;

    const uint32_t mask = 1u << (button_index % 32);
    if (value)
        packet.m_Buttons[button_index / 32] |= mask;
    else
        packet.m_Buttons[button_index / 32] &= ~mask;
}

// Decodes the low nibble of a legacy hat mapping token into the engine hat bit.
static uint8_t GetLegacyHatBitValue(uint8_t index)
{
    switch (index & 0x0f)
    {
        case 0x1: return APPLE_GAMEPAD_HAT_UP;
        case 0x2: return APPLE_GAMEPAD_HAT_RIGHT;
        case 0x4: return APPLE_GAMEPAD_HAT_DOWN;
        case 0x8: return APPLE_GAMEPAD_HAT_LEFT;
        default: return APPLE_GAMEPAD_HAT_CENTERED;
    }
}

// Converts a normalized semantic axis value back to the raw axis range expected
// by a legacy mapping target.
static float InverseMapLegacyAxis(const AppleGamepadLegacyElement& element, float semantic_value)
{
    const float minimum = (float) element.m_Minimum;
    const float maximum = (float) element.m_Maximum;
    const float scale = maximum - minimum;
    if (scale == 0.0f)
        return semantic_value;

    const float raw_value = (semantic_value * scale + minimum + maximum) * 0.5f;
    return fminf(fmaxf(raw_value, -1.0f), 1.0f);
}

// Merges multiple semantic sources into a single legacy axis while keeping the
// strongest non-conflicting input.
static void SetMappedAxisValue(float axis_values[MAX_GAMEPAD_AXIS_COUNT], bool axis_written[MAX_GAMEPAD_AXIS_COUNT], uint8_t axis_index, float value)
{
    if (axis_index >= MAX_GAMEPAD_AXIS_COUNT)
        return;

    if (!axis_written[axis_index])
    {
        axis_values[axis_index] = value;
        axis_written[axis_index] = true;
        return;
    }

    const float current = axis_values[axis_index];
    if (current == 0.0f || fabsf(value) > fabsf(current))
    {
        axis_values[axis_index] = value;
    }
    else if ((current > 0.0f && value < 0.0f) || (current < 0.0f && value > 0.0f))
    {
        axis_values[axis_index] = 0.0f;
    }
}

// Applies a semantic button to whichever raw target a legacy mapping points at:
// button, axis-as-button, or hat bit.
static void SetMappedButtonValue(GamepadPacket& packet, float axis_values[MAX_GAMEPAD_AXIS_COUNT], bool axis_written[MAX_GAMEPAD_AXIS_COUNT], const AppleGamepadLegacyElement& element, bool pressed)
{
    switch (element.m_Type)
    {
        case APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON:
            SetButtonValue(packet, element.m_Index, pressed);
            break;

        case APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT:
            if (pressed)
            {
                const uint8_t hat_index = element.m_Index >> 4;
                if (hat_index < MAX_GAMEPAD_HAT_COUNT)
                    packet.m_Hat[hat_index] |= GetLegacyHatBitValue(element.m_Index);
            }
            break;

        case APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS:
        {
            const float raw_value = pressed ? (element.m_Maximum > 0 ? 1.0f : -1.0f) : 0.0f;
            SetMappedAxisValue(axis_values, axis_written, element.m_Index, raw_value);
            break;
        }

        default:
            break;
    }
}

// Applies a semantic axis to the mapped raw packet element type.
static void SetMappedAxisSemanticValue(GamepadPacket& packet, float axis_values[MAX_GAMEPAD_AXIS_COUNT], bool axis_written[MAX_GAMEPAD_AXIS_COUNT], const AppleGamepadLegacyElement& element, float semantic_value)
{
    switch (element.m_Type)
    {
        case APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS:
            SetMappedAxisValue(axis_values, axis_written, element.m_Index, InverseMapLegacyAxis(element, semantic_value));
            break;

        case APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON:
            SetButtonValue(packet, element.m_Index, semantic_value >= 0.0f);
            break;

        case APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT:
            if (semantic_value >= 0.0f)
            {
                const uint8_t hat_index = element.m_Index >> 4;
                if (hat_index < MAX_GAMEPAD_HAT_COUNT)
                    packet.m_Hat[hat_index] |= GetLegacyHatBitValue(element.m_Index);
            }
            break;

        default:
            break;
    }
}

// Finds the raw packet index that currently represents a semantic axis/button.
static int32_t FindLegacyAxisIndex(const AppleGamepadDevice* device, uint8_t semantic_axis)
{
    if (device->m_HasLegacyMapping)
    {
        const AppleGamepadLegacyElement& element = device->m_LegacyAxisMap[semantic_axis];
        if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS && element.m_Index < MAX_GAMEPAD_AXIS_COUNT)
            return element.m_Index;
        return -1;
    }

    for (uint32_t i = 0; i < device->m_AxisCount; ++i)
    {
        if (device->m_AxisRemap[i] == semantic_axis)
            return i;
    }

    return -1;
}

static int32_t FindLegacyButtonIndex(const AppleGamepadDevice* device, uint8_t semantic_button)
{
    if (device->m_HasLegacyMapping)
    {
        const AppleGamepadLegacyElement& element = device->m_LegacyButtonMap[semantic_button];
        if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON && element.m_Index < MAX_GAMEPAD_BUTTON_COUNT)
            return element.m_Index;
        return -1;
    }

    for (uint32_t i = 0; i < device->m_ButtonCount; ++i)
    {
        if (device->m_ButtonRemap[i] == semantic_button)
            return i;
    }

    return -1;
}

// Applies platform-specific compatibility adjustments for legacy packets after
// the generic mapping step.
static void PostProcessLegacyPacket(AppleGamepadDevice* apple_device, GCController* controller, GamepadPacket& packet)
{
    if (!IsControllerPS4(controller) && !IsControllerPS5(controller))
        return;

    const int32_t left_y_index = FindLegacyAxisIndex(apple_device, APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_Y);
    const int32_t right_y_index = FindLegacyAxisIndex(apple_device, APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_Y);
    if (left_y_index >= 0)
        packet.m_Axis[left_y_index] *= -1.0f;
    if (right_y_index >= 0)
        packet.m_Axis[right_y_index] *= -1.0f;

    const int32_t back_button_index = FindLegacyButtonIndex(apple_device, APPLE_GAMEPAD_SEMANTIC_BUTTON_BACK);
    const int32_t start_button_index = FindLegacyButtonIndex(apple_device, APPLE_GAMEPAD_SEMANTIC_BUTTON_START);
    if (back_button_index >= 0 && start_button_index >= 0)
    {
        const bool back_value = GetButtonValue(packet, back_button_index);
        const bool start_value = GetButtonValue(packet, start_button_index);
        SetButtonState(packet, back_button_index, start_value);
        SetButtonState(packet, start_button_index, back_value);
    }
}

#if TARGET_OS_OSX
// Parses a single SDL mapping target token such as "a3", "+a2", "b5", or
// "h0.4" into a raw packet destination.
static bool ParseLegacyMappingTarget(const char* target, AppleGamepadLegacyElement* element)
{
    AppleGamepadLegacyElement parsed = {};
    parsed.m_Minimum = -1;
    parsed.m_Maximum = 1;

    if (*target == '+')
    {
        parsed.m_Minimum = 0;
        ++target;
    }
    else if (*target == '-')
    {
        parsed.m_Maximum = 0;
        ++target;
    }

    if (*target == 'a')
        parsed.m_Type = APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS;
    else if (*target == 'b')
        parsed.m_Type = APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON;
    else if (*target == 'h')
        parsed.m_Type = APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT;
    else
        return false;

    ++target;

    char* end = 0;
    long index = strtol(target, &end, 10);
    if (end == target || index < 0 || index > 255)
        return false;

    if (parsed.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT)
    {
        if (*end != '.')
            return false;

        target = end + 1;
        long bit = strtol(target, &end, 10);
        if (end == target || bit < 0 || bit > 15)
            return false;

        parsed.m_Index = (uint8_t) (((uint8_t) index << 4) | (uint8_t) bit);
    }
    else
    {
        parsed.m_Index = (uint8_t) index;
    }

    *element = parsed;
    return true;
}

// Parses one SDL controller database line into semantic-to-raw remap tables for
// this device when the GUID matches.
static bool ParseLegacyMappingLine(const char* mapping, const char guid_string[MAX_GAMEPAD_GUID_LENGTH + 1], AppleGamepadDevice* device)
{
    if (strncmp(mapping, guid_string, MAX_GAMEPAD_GUID_LENGTH) != 0 || mapping[MAX_GAMEPAD_GUID_LENGTH] != ',')
        return false;

    struct LegacyField
    {
        const char* m_Name;
        AppleGamepadLegacyElement* m_Element;
    };

    LegacyField fields[] =
    {
        { "a",             &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_A] },
        { "b",             &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_B] },
        { "x",             &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_X] },
        { "y",             &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_Y] },
        { "back",          &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_BACK] },
        { "start",         &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_START] },
        { "guide",         &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_GUIDE] },
        { "leftshoulder",  &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_SHOULDER] },
        { "rightshoulder", &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_SHOULDER] },
        { "leftstick",     &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_THUMB] },
        { "rightstick",    &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_THUMB] },
        { "dpup",          &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_UP] },
        { "dpright",       &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_RIGHT] },
        { "dpdown",        &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_DOWN] },
        { "dpleft",        &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_LEFT] },
        { "misc1",         &device->m_LegacyButtonMap[APPLE_GAMEPAD_SEMANTIC_BUTTON_CAPTURE] },
        { "lefttrigger",   &device->m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_TRIGGER] },
        { "righttrigger",  &device->m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_TRIGGER] },
        { "leftx",         &device->m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_X] },
        { "lefty",         &device->m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_Y] },
        { "rightx",        &device->m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_X] },
        { "righty",        &device->m_LegacyAxisMap[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_Y] },
    };

    const char* cursor = strchr(mapping, ',');
    if (cursor == 0)
        return false;
    cursor = strchr(cursor + 1, ',');
    if (cursor == 0)
        return false;
    ++cursor;

    bool found_element = false;

    while (*cursor)
    {
        const char* token_end = strchr(cursor, ',');
        if (token_end == 0)
            token_end = cursor + strlen(cursor);

        const char* separator = (const char*) memchr(cursor, ':', token_end - cursor);
        if (separator != 0)
        {
            for (uint32_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
            {
                const size_t key_length = strlen(fields[i].m_Name);
                if ((size_t) (separator - cursor) == key_length && strncmp(cursor, fields[i].m_Name, key_length) == 0)
                {
                    char target[32];
                    const size_t target_length = (size_t) (token_end - separator - 1);
                    if (target_length >= sizeof(target))
                        break;

                    memcpy(target, separator + 1, target_length);
                    target[target_length] = '\0';

                    AppleGamepadLegacyElement element = {};
                    if (ParseLegacyMappingTarget(target, &element))
                    {
                        *fields[i].m_Element = element;
                        found_element = true;
                    }
                    break;
                }
            }
        }

        cursor = *token_end ? token_end + 1 : token_end;
    }

    if (!found_element)
        return false;

    uint8_t axis_count = 0;
    uint8_t button_count = 0;
    uint8_t hat_count = 0;

    for (uint32_t i = 0; i < APPLE_GAMEPAD_SEMANTIC_AXIS_COUNT; ++i)
    {
        const AppleGamepadLegacyElement& element = device->m_LegacyAxisMap[i];
        if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS)
            axis_count = dmMath::Max(axis_count, (uint8_t) (element.m_Index + 1));
        else if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON)
            button_count = dmMath::Max(button_count, (uint8_t) (element.m_Index + 1));
        else if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT)
            hat_count = dmMath::Max(hat_count, (uint8_t) ((element.m_Index >> 4) + 1));
    }

    for (uint32_t i = 0; i < APPLE_GAMEPAD_SEMANTIC_BUTTON_COUNT; ++i)
    {
        const AppleGamepadLegacyElement& element = device->m_LegacyButtonMap[i];
        if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_AXIS)
            axis_count = dmMath::Max(axis_count, (uint8_t) (element.m_Index + 1));
        else if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_BUTTON)
            button_count = dmMath::Max(button_count, (uint8_t) (element.m_Index + 1));
        else if (element.m_Type == APPLE_GAMEPAD_LEGACY_ELEMENT_TYPE_HATBIT)
            hat_count = dmMath::Max(hat_count, (uint8_t) ((element.m_Index >> 4) + 1));
    }

    if (axis_count > MAX_GAMEPAD_AXIS_COUNT || button_count > MAX_GAMEPAD_BUTTON_COUNT || hat_count > MAX_GAMEPAD_HAT_COUNT)
        return false;

    device->m_LegacyAxisCount = axis_count;
    device->m_LegacyButtonCount = button_count;
    device->m_LegacyHatCount = hat_count;
    device->m_HasLegacyMapping = 1;
    return true;
}

// Looks up the device GUID in the baked SDL mapping database.
static bool BuildLegacyMappingFromGuid(AppleGamepadDevice* device, const char guid_string[MAX_GAMEPAD_GUID_LENGTH + 1])
{
    const uint32_t mapping_count = sizeof(dmHIDAppleGamepadDefaultMappings) / sizeof(dmHIDAppleGamepadDefaultMappings[0]);
    for (uint32_t i = 0; i < mapping_count; ++i)
    {
        if (ParseLegacyMappingLine(dmHIDAppleGamepadDefaultMappings[i], guid_string, device))
            return true;
    }

    return false;
}
#endif

// Chooses the packet remap strategy for the device: database-driven legacy
// mapping on macOS when available, otherwise the default semantic layout.
static void BuildDeviceRemap(AppleGamepadDevice* device, const GamepadGuid* guid)
{
    ResetLegacyMapping(device);

#if TARGET_OS_OSX
    char guid_string[MAX_GAMEPAD_GUID_LENGTH + 1];
    FormatGamepadGuid(guid, guid_string);
    if (BuildLegacyMappingFromGuid(device, guid_string))
        return;
#else
    (void) guid;
#endif

    BuildDefaultDeviceRemap(device);
}

// Prevents the OS from consuming guide/menu button presses that the engine
// wants to observe itself.
static void DisableSystemGesture(GCControllerButtonInput* button)
{
    if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
    {
        if (button != nil)
            button.preferredSystemGestureState = GCSystemGestureStateDisabled;
    }
}

// Writes the modern SDL-style packet layout directly from the extended gamepad
// profile.
static void AppleGamepadDriverUpdateSDL(AppleGamepadDevice* apple_device, GCController* controller, GCExtendedGamepad* extended_gamepad, GamepadPacket& packet)
{
    // Keep the raw packet order aligned with SDL's gamepad axis order:
    // leftx, lefty, rightx, righty, lefttrigger, righttrigger.
    packet.m_Axis[0] = extended_gamepad.leftThumbstick.xAxis.value;
    packet.m_Axis[1] = -extended_gamepad.leftThumbstick.yAxis.value;
    packet.m_Axis[2] = extended_gamepad.rightThumbstick.xAxis.value;
    packet.m_Axis[3] = -extended_gamepad.rightThumbstick.yAxis.value;
    packet.m_Axis[4] = extended_gamepad.leftTrigger.value * 2.0f - 1.0f;
    packet.m_Axis[5] = extended_gamepad.rightTrigger.value * 2.0f - 1.0f;

    uint32_t button_index = 0;
    SetButtonValue(packet, button_index++, extended_gamepad.buttonA.isPressed);
    SetButtonValue(packet, button_index++, extended_gamepad.buttonB.isPressed);
    SetButtonValue(packet, button_index++, extended_gamepad.buttonX.isPressed);
    SetButtonValue(packet, button_index++, extended_gamepad.buttonY.isPressed);

    if (apple_device->m_HasBackButton)
    {
        bool pressed = false;
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            DisableSystemGesture(extended_gamepad.buttonOptions);
            pressed = extended_gamepad.buttonOptions.isPressed;
        }
        SetButtonValue(packet, button_index++, pressed);
    }

    if (apple_device->m_HasStartButton)
    {
        bool pressed = false;
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            DisableSystemGesture(extended_gamepad.buttonMenu);
            pressed = extended_gamepad.buttonMenu.isPressed;
        }
        SetButtonValue(packet, button_index++, pressed);
    }

    if (apple_device->m_HasLeftThumbstickButton)
    {
        bool pressed = false;
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
            pressed = extended_gamepad.leftThumbstickButton.isPressed;
        SetButtonValue(packet, button_index++, pressed);
    }

    if (apple_device->m_HasRightThumbstickButton)
    {
        bool pressed = false;
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
            pressed = extended_gamepad.rightThumbstickButton.isPressed;
        SetButtonValue(packet, button_index++, pressed);
    }

    SetButtonValue(packet, button_index++, extended_gamepad.leftShoulder.isPressed);
    SetButtonValue(packet, button_index++, extended_gamepad.rightShoulder.isPressed);

    if (apple_device->m_HasGuideButton)
    {
        GCControllerButtonInput* guide_button = GetGuideButton(controller);
        DisableSystemGesture(guide_button);
        SetButtonValue(packet, button_index++, guide_button != nil && guide_button.isPressed);
    }

    if (apple_device->m_HasCaptureButton)
    {
        GCControllerButtonInput* capture_button = GetCaptureButton(controller);
        SetButtonValue(packet, button_index++, capture_button != nil && capture_button.isPressed);
    }

    packet.m_Hat[0] = GetHatValue(extended_gamepad.dpad);
}

// Builds a GLFW/legacy-style packet using the default semantic remap when no
// explicit SDL database mapping exists.
static void AppleGamepadDriverUpdateLegacyFallback(AppleGamepadDevice* apple_device, GCController* controller, GCExtendedGamepad* extended_gamepad, GamepadPacket& packet)
{
    float semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_COUNT] = {};
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_X] = extended_gamepad.leftThumbstick.xAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_Y] = extended_gamepad.leftThumbstick.yAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_X] = extended_gamepad.rightThumbstick.xAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_Y] = extended_gamepad.rightThumbstick.yAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_TRIGGER] = extended_gamepad.leftTrigger.value * 2.0f - 1.0f;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_TRIGGER] = extended_gamepad.rightTrigger.value * 2.0f - 1.0f;

    for (uint32_t i = 0; i < apple_device->m_AxisCount; ++i)
    {
        uint8_t semantic_index = apple_device->m_AxisRemap[i];
        if (semantic_index != APPLE_GAMEPAD_REMAP_INVALID)
            packet.m_Axis[i] = semantic_axis[semantic_index];
    }

    bool semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_COUNT] = {};
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_A] = extended_gamepad.buttonA.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_B] = extended_gamepad.buttonB.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_X] = extended_gamepad.buttonX.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_Y] = extended_gamepad.buttonY.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_SHOULDER] = extended_gamepad.leftShoulder.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_SHOULDER] = extended_gamepad.rightShoulder.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_UP] = extended_gamepad.dpad.up.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_DOWN] = extended_gamepad.dpad.down.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_LEFT] = extended_gamepad.dpad.left.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_RIGHT] = extended_gamepad.dpad.right.isPressed;

    if (apple_device->m_HasBackButton)
    {
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            DisableSystemGesture(extended_gamepad.buttonOptions);
            DisableSystemGesture(extended_gamepad.buttonMenu);
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_BACK] = extended_gamepad.buttonMenu.isPressed;
        }
    }

    if (apple_device->m_HasStartButton)
    {
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            DisableSystemGesture(extended_gamepad.buttonMenu);
            DisableSystemGesture(extended_gamepad.buttonOptions);
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_START] = extended_gamepad.buttonOptions.isPressed;
        }
    }

    if (apple_device->m_HasLeftThumbstickButton)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_THUMB] = extended_gamepad.leftThumbstickButton.isPressed;
    }

    if (apple_device->m_HasRightThumbstickButton)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_THUMB] = extended_gamepad.rightThumbstickButton.isPressed;
    }

    if (apple_device->m_HasGuideButton)
    {
        GCControllerButtonInput* guide_button = GetGuideButton(controller);
        DisableSystemGesture(guide_button);
        semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_GUIDE] = guide_button != nil && guide_button.isPressed;
    }

    if (apple_device->m_HasCaptureButton)
    {
        GCControllerButtonInput* capture_button = GetCaptureButton(controller);
        semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_CAPTURE] = capture_button != nil && capture_button.isPressed;
    }

    for (uint32_t i = 0; i < apple_device->m_ButtonCount; ++i)
    {
        uint8_t semantic_index = apple_device->m_ButtonRemap[i];
        if (semantic_index != APPLE_GAMEPAD_REMAP_INVALID)
            SetButtonValue(packet, i, semantic_buttons[semantic_index]);
    }

    for (uint32_t i = 0; i < apple_device->m_HatCount; ++i)
    {
        uint8_t semantic_index = apple_device->m_HatRemap[i];
        if (semantic_index == APPLE_GAMEPAD_SEMANTIC_HAT_DPAD)
        {
            packet.m_Hat[i] = GetHatValue(extended_gamepad.dpad);
            uint32_t button_base = apple_device->m_ButtonCount + i * 4;
            SetButtonValue(packet, button_base + 0, extended_gamepad.dpad.up.isPressed);
            SetButtonValue(packet, button_base + 1, extended_gamepad.dpad.down.isPressed);
            SetButtonValue(packet, button_base + 2, extended_gamepad.dpad.left.isPressed);
            SetButtonValue(packet, button_base + 3, extended_gamepad.dpad.right.isPressed);
        }
    }
}

// Builds a GLFW/legacy-style packet using the parsed SDL database mapping for
// the device GUID.
static void AppleGamepadDriverUpdateLegacyMapped(AppleGamepadDevice* apple_device, GCController* controller, GCExtendedGamepad* extended_gamepad, GamepadPacket& packet)
{
    float axis_values[MAX_GAMEPAD_AXIS_COUNT] = {};
    bool axis_written[MAX_GAMEPAD_AXIS_COUNT] = {};

    float semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_COUNT] = {};
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_X] = extended_gamepad.leftThumbstick.xAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_Y] = extended_gamepad.leftThumbstick.yAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_X] = extended_gamepad.rightThumbstick.xAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_Y] = extended_gamepad.rightThumbstick.yAxis.value;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_LEFT_TRIGGER] = extended_gamepad.leftTrigger.value * 2.0f - 1.0f;
    semantic_axis[APPLE_GAMEPAD_SEMANTIC_AXIS_RIGHT_TRIGGER] = extended_gamepad.rightTrigger.value * 2.0f - 1.0f;

    bool semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_COUNT] = {};
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_A] = extended_gamepad.buttonA.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_B] = extended_gamepad.buttonB.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_X] = extended_gamepad.buttonX.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_Y] = extended_gamepad.buttonY.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_SHOULDER] = extended_gamepad.leftShoulder.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_SHOULDER] = extended_gamepad.rightShoulder.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_UP] = extended_gamepad.dpad.up.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_DOWN] = extended_gamepad.dpad.down.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_LEFT] = extended_gamepad.dpad.left.isPressed;
    semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_DPAD_RIGHT] = extended_gamepad.dpad.right.isPressed;

    if (apple_device->m_HasBackButton)
    {
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            DisableSystemGesture(extended_gamepad.buttonOptions);
            DisableSystemGesture(extended_gamepad.buttonMenu);
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_BACK] = extended_gamepad.buttonMenu.isPressed;
        }
    }

    if (apple_device->m_HasStartButton)
    {
        if (@available(macOS 10.15, iOS 13.0, tvOS 13.0, *))
        {
            DisableSystemGesture(extended_gamepad.buttonMenu);
            DisableSystemGesture(extended_gamepad.buttonOptions);
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_START] = extended_gamepad.buttonOptions.isPressed;
        }
    }

    if (apple_device->m_HasLeftThumbstickButton)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_LEFT_THUMB] = extended_gamepad.leftThumbstickButton.isPressed;
    }

    if (apple_device->m_HasRightThumbstickButton)
    {
        if (@available(macOS 10.14.1, iOS 12.1, tvOS 12.1, *))
            semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_RIGHT_THUMB] = extended_gamepad.rightThumbstickButton.isPressed;
    }

    if (apple_device->m_HasGuideButton)
    {
        GCControllerButtonInput* guide_button = GetGuideButton(controller);
        DisableSystemGesture(guide_button);
        semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_GUIDE] = guide_button != nil && guide_button.isPressed;
    }

    if (apple_device->m_HasCaptureButton)
    {
        GCControllerButtonInput* capture_button = GetCaptureButton(controller);
        semantic_buttons[APPLE_GAMEPAD_SEMANTIC_BUTTON_CAPTURE] = capture_button != nil && capture_button.isPressed;
    }

    for (uint32_t i = 0; i < APPLE_GAMEPAD_SEMANTIC_AXIS_COUNT; ++i)
        SetMappedAxisSemanticValue(packet, axis_values, axis_written, apple_device->m_LegacyAxisMap[i], semantic_axis[i]);

    for (uint32_t i = 0; i < APPLE_GAMEPAD_SEMANTIC_BUTTON_COUNT; ++i)
        SetMappedButtonValue(packet, axis_values, axis_written, apple_device->m_LegacyButtonMap[i], semantic_buttons[i]);

    for (uint32_t i = 0; i < apple_device->m_LegacyAxisCount; ++i)
        packet.m_Axis[i] = axis_values[i];
}

// Selects the legacy packet builder and then applies controller-family fixups.
static void AppleGamepadDriverUpdateGlfw(AppleGamepadDevice* apple_device, GCController* controller, GCExtendedGamepad* extended_gamepad, GamepadPacket& packet)
{
    if (apple_device->m_HasLegacyMapping)
        AppleGamepadDriverUpdateLegacyMapped(apple_device, controller, extended_gamepad, packet);
    else
        AppleGamepadDriverUpdateLegacyFallback(apple_device, controller, extended_gamepad, packet);

    PostProcessLegacyPacket(apple_device, controller, packet);
}

// Probes whether this GCController exposes a profile we can translate and
// populates the static packet capabilities for the device.
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

        device->m_HasGuideButton = GetGuideButton(controller) != nil;
        device->m_ButtonCount += device->m_HasGuideButton ? 1 : 0;
        device->m_HasCaptureButton = GetCaptureButton(controller) != nil;
        device->m_ButtonCount += device->m_HasCaptureButton ? 1 : 0;
        device->m_PacketLayout = APPLE_GAMEPAD_PACKET_LAYOUT_SDL;
        ResetLegacyMapping(device);

        return true;
    }

    return false;
}

// Builds a synthetic GUID from the GameController profile when the platform
// does not expose a unique HID device match.
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

// Driver-local lookup helpers for devices and their owning Gamepad handles.
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

// Maps an engine gamepad handle back to the AppleGamepadDevice metadata entry.
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

// Allocates the lowest free logical gamepad slot.
static int AllocateGamepadId(AppleGamepadDriver* driver)
{
    for (int gamepad_id = 0; gamepad_id < MAX_GAMEPAD_COUNT; ++gamepad_id)
    {
        if (GetAppleGamepadDevice(driver, gamepad_id) == 0)
            return gamepad_id;
    }

    return -1;
}

// Creates the engine-side gamepad object and associated Apple metadata on first
// sight of a GCController.
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

    BuildDeviceRemap(&new_device, &new_device.m_Guid);

    if (driver->m_Devices.Full())
    {
        driver->m_Devices.OffsetCapacity(1);
    }

    driver->m_Devices.Push(new_device);
    SetGamepadConnectionStatus(driver->m_HidContext, gp, true);
    return gp;
}

// Tears down one connected gamepad and releases Objective-C ownership.
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

// Notification handlers keep the engine device table in sync with the
// GameController connection set.
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

// Per-frame driver update that converts the live GameController state into the
// requested packet layout for the engine.
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

    apple_device->m_PacketLayout = gamepad->m_LayoutLegacy ? APPLE_GAMEPAD_PACKET_LAYOUT_GLFW : APPLE_GAMEPAD_PACKET_LAYOUT_SDL;
    if (apple_device->m_PacketLayout == APPLE_GAMEPAD_PACKET_LAYOUT_GLFW)
    {
        gamepad->m_AxisCount = GetLegacyAxisCount(apple_device);
        gamepad->m_ButtonCount = GetLegacyButtonCount(apple_device);
        gamepad->m_HatCount = GetLegacyHatCount(apple_device);
    }
    else
    {
        gamepad->m_AxisCount = apple_device->m_AxisCount;
        gamepad->m_ButtonCount = apple_device->m_ButtonCount;
        gamepad->m_HatCount = apple_device->m_HatCount;
    }

    if (apple_device->m_PacketLayout == APPLE_GAMEPAD_PACKET_LAYOUT_GLFW)
        AppleGamepadDriverUpdateGlfw(apple_device, controller, extended_gamepad, packet);
    else
        AppleGamepadDriverUpdateSDL(apple_device, controller, extended_gamepad, packet);
}

// Registers GameController connect/disconnect observers once per driver.
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

// Removes any previously installed GameController observers.
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

// Polls the current controller list and installs observers so new connections
// arrive through notifications after the first scan.
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

// Internal accessors for the device name and GUID exported through the driver
// vtable.
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

// Initializes the Apple driver and, on macOS, the HID manager used for
// building stable USB-style GUIDs.
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

// Releases all device state, observers, and the macOS HID manager.
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

// Creates and wires up the GameController-backed gamepad driver instance.
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
