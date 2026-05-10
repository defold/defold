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

#include "../hid_private.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/endian.h>

#if defined(ANDROID) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#include <glfw/glfw.h>

#if !defined(GLFW_JOYSTICK_DEVICE_GUID_LENGTH)
extern "C" void glfwCreateJoystickDeviceGuid(unsigned short bus,
                                             unsigned short vendor,
                                             unsigned short product,
                                             unsigned short version,
                                             const char* vendor_name,
                                             const char* product_name,
                                             unsigned char driver_signature,
                                             unsigned char driver_data,
                                             char guid[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1]);
#endif
#endif

class HIDTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Context = dmHID::NewContext(dmHID::NewContextParams());
        dmHID::Init(m_Context);

        m_Keyboard = dmHID::GetKeyboard(m_Context, 0);
        m_Mouse = dmHID::GetMouse(m_Context, 0);
        m_TouchDevice = dmHID::GetTouchDevice(m_Context, 0);

        ASSERT_NE(dmHID::INVALID_KEYBOARD_HANDLE, m_Keyboard);
        ASSERT_NE(dmHID::INVALID_MOUSE_HANDLE, m_Mouse);
        ASSERT_NE(dmHID::INVALID_TOUCH_DEVICE_HANDLE, m_TouchDevice);

    }

    void TearDown() override
    {
        dmHID::Final(m_Context);
        dmHID::DeleteContext(m_Context);
    }

    HWindow m_Window;
    dmHID::HContext     m_Context;
    dmHID::HKeyboard    m_Keyboard;
    dmHID::HMouse       m_Mouse;
    dmHID::HTouchDevice m_TouchDevice;
};

TEST_F(HIDTest, Connectedness)
{
    dmHID::Update(m_Context);

    ASSERT_TRUE(dmHID::IsKeyboardConnected(m_Keyboard));
    ASSERT_TRUE(dmHID::IsMouseConnected(m_Mouse));
    dmHID::HGamepad gamepad = dmHID::GetGamepad(m_Context, 0);
    ASSERT_TRUE(dmHID::IsGamepadConnected(gamepad));
    ASSERT_TRUE(dmHID::IsTouchDeviceConnected(m_TouchDevice));
}

TEST_F(HIDTest, Keyboard)
{
    dmHID::Update(m_Context);
    dmHID::KeyboardPacket packet;

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Keyboard, &packet));

    // Assert init values
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Press keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey(m_Keyboard, (dmHID::Key)i, true);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Keyboard, &packet));

    // Assert pressed
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Release keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey(m_Keyboard, (dmHID::Key)i, false);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Keyboard, &packet));

    // Assert released
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));
}

TEST_F(HIDTest, Mouse)
{
    dmHID::Update(m_Context);
    dmHID::MousePacket packet;

    ASSERT_TRUE(dmHID::GetMousePacket(m_Mouse, &packet));

    // Assert init state
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(0, packet.m_PositionX);
    ASSERT_EQ(0, packet.m_PositionY);
    ASSERT_EQ(0, packet.m_Wheel);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton(m_Mouse, (dmHID::MouseButton)i, true);
    dmHID::SetMousePosition(m_Mouse, 1, 2);
    dmHID::SetMouseWheel(m_Mouse, 3);

    ASSERT_TRUE(dmHID::GetMousePacket(m_Mouse, &packet));

    // Assert inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(1, packet.m_PositionX);
    ASSERT_EQ(2, packet.m_PositionY);
    ASSERT_EQ(3, packet.m_Wheel);

    // Release buttons
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton(m_Mouse, (dmHID::MouseButton)i, false);

    ASSERT_TRUE(dmHID::GetMousePacket(m_Mouse, &packet));

    // Assert released
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
}

TEST_F(HIDTest, Gamepad)
{
    dmHID::Update(m_Context);
    dmHID::GamepadPacket packet;
    dmHID::HGamepad gamepad = dmHID::GetGamepad(m_Context, 0);
    ASSERT_NE(dmHID::INVALID_GAMEPAD_HANDLE, gamepad);

    uint32_t button_count = dmHID::GetGamepadButtonCount(gamepad);
    ASSERT_LT(0U, button_count);
    uint32_t axis_count = dmHID::GetGamepadAxisCount(gamepad);
    ASSERT_LT(0U, axis_count);

    ASSERT_TRUE(dmHID::GetGamepadPacket(gamepad, &packet));

    // Assert init state
    for (uint32_t i = 0; i < button_count; ++i)
        ASSERT_FALSE(dmHID::GetGamepadButton(&packet, i));
    for (uint32_t i = 0; i < axis_count; ++i)
        ASSERT_FALSE(dmHID::GetGamepadButton(&packet, i));

    // Touch all inputs
    for (uint32_t i = 0; i < button_count; ++i)
        dmHID::SetGamepadButton(gamepad, i, true);
    for (uint32_t i = 0; i < axis_count; ++i)
        dmHID::SetGamepadAxis(gamepad, i, 1.0f);

    ASSERT_TRUE(dmHID::GetGamepadPacket(gamepad, &packet));

    // Assert inputs
    for (uint32_t i = 0; i < button_count; ++i)
        ASSERT_TRUE(dmHID::GetGamepadButton(&packet, i));
    for (uint32_t i = 0; i < axis_count; ++i)
        ASSERT_EQ(1.0f, packet.m_Axis[i]);

    // Release buttons
    for (uint32_t i = 0; i < button_count; ++i)
        dmHID::SetGamepadButton(gamepad, i, false);

    ASSERT_TRUE(dmHID::GetGamepadPacket(gamepad, &packet));

    // Assert released
    for (uint32_t i = 0; i < button_count; ++i)
        ASSERT_FALSE(dmHID::GetGamepadButton(&packet, i));
}

TEST_F(HIDTest, GamepadMetadata)
{
    dmHID::Update(m_Context);

    dmHID::HGamepad gamepad = dmHID::GetGamepad(m_Context, 0);
    ASSERT_NE(dmHID::INVALID_GAMEPAD_HANDLE, gamepad);
    ASSERT_TRUE(dmHID::IsGamepadConnected(gamepad));

    char device_name[dmHID::MAX_GAMEPAD_NAME_LENGTH];
    dmHID::GetGamepadDeviceName(m_Context, gamepad, device_name);
    ASSERT_STREQ("null_device", device_name);

    dmHID::GamepadGuid device_guid;
    ASSERT_TRUE(dmHID::GetGamepadDeviceGuid(m_Context, gamepad, &device_guid));
    char device_guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1];
    dmHID::FormatGamepadGuid(&device_guid, device_guid_string);
    ASSERT_STREQ("00000000000000000000000000000000", device_guid_string);
}

TEST_F(HIDTest, TouchDevice)
{
    dmHID::Update(m_Context);
    dmHID::TouchDevicePacket packet;

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_TouchDevice, &packet));

    // Assert init state
    ASSERT_EQ(0u, packet.m_TouchCount);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_TOUCH_COUNT; ++i)
    {
        dmHID::AddTouch(m_TouchDevice, i*2, i*2+1, 0, dmHID::PHASE_BEGAN);
    }

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_TouchDevice, &packet));

    // Assert inputs
    int32_t x, y;
    uint32_t id;
    bool pressed;
    bool released;
    for (uint32_t i = 0; i < dmHID::MAX_TOUCH_COUNT; ++i)
    {
        ASSERT_TRUE(dmHID::GetTouch(&packet, i, &x, &y, &id, &pressed, &released));
        ASSERT_EQ((int32_t)i*2, x);
        ASSERT_EQ((int32_t)i*2+1, y);
        ASSERT_EQ(0, id);
        ASSERT_EQ(true, pressed);
        ASSERT_EQ(false, released);
    }

    // Clear touches
    dmHID::ClearTouches(m_TouchDevice);

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_TouchDevice, &packet));

    ASSERT_EQ(0u, packet.m_TouchCount);
}

TEST_F(HIDTest, IgnoredDevices)
{
    dmHID::NewContextParams params;

    m_Keyboard = dmHID::GetKeyboard(m_Context, 0);

    m_TouchDevice = dmHID::GetTouchDevice(m_Context, 0);


    params.m_IgnoreMouse = 1;
    dmHID::HContext context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsMouseConnected(dmHID::GetMouse(m_Context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreMouse = 0;
    params.m_IgnoreKeyboard = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsKeyboardConnected(dmHID::GetKeyboard(m_Context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreKeyboard = 0;
    params.m_IgnoreGamepads = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsGamepadConnected(dmHID::GetGamepad(context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreGamepads = 0;
    params.m_IgnoreTouchDevice = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsTouchDeviceConnected(dmHID::GetTouchDevice(m_Context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);
}

static uint16_t ByteSwapLE16(uint16_t value)
{
#if DM_ENDIAN == DM_ENDIAN_BIG
    return EndianSwap16(value);
#else
    return value;
#endif
}

static uint16_t CRC16ForByte(uint8_t value)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < 8; ++i)
    {
        crc = (uint16_t) ((((crc ^ value) & 1) ? 0xA001 : 0) ^ (crc >> 1));
        value >>= 1;
    }
    return crc;
}

static uint16_t UpdateCRC16(uint16_t crc, const char* string)
{
    while (*string)
    {
        crc = (uint16_t) (CRC16ForByte((uint8_t) crc ^ (uint8_t) *string++) ^ (crc >> 8));
    }
    return crc;
}

// Reference implementation from SDL_joystick.c, using Defold types and helpers.
static dmHID::GamepadGuid SDLCreateJoystickGUIDReference(uint16_t bus, uint16_t vendor, uint16_t product, uint16_t version, const char* vendor_name, const char* product_name, uint8_t driver_signature, uint8_t driver_data)
{
    dmHID::GamepadGuid guid = {};
    uint16_t crc = 0;

    if (vendor_name && *vendor_name && product_name && *product_name)
    {
        crc = UpdateCRC16(crc, vendor_name);
        crc = UpdateCRC16(crc, " ");
        crc = UpdateCRC16(crc, product_name);
    }
    else if (product_name)
    {
        crc = UpdateCRC16(crc, product_name);
    }

    // SDL GUID words are stored little-endian, unlike Defold's network-order helpers.
    guid.m_Bus = ByteSwapLE16(bus);
    guid.m_CRC16 = ByteSwapLE16(crc);

    if (vendor)
    {
        guid.m_Vendor = ByteSwapLE16(vendor);
        guid.m_Product = ByteSwapLE16(product);
        guid.m_Version = ByteSwapLE16(version);
        guid.m_DriverSignature = driver_signature;
        guid.m_DriverData = driver_data;
    }
    else
    {
        size_t available_space = sizeof(guid) - offsetof(dmHID::GamepadGuid, m_Vendor);

        if (driver_signature)
        {
            available_space -= 2;
            guid.m_DriverSignature = driver_signature;
            guid.m_DriverData = driver_data;
        }
        if (product_name)
        {
            dmStrlCpy((char*) &guid.m_Vendor, product_name, available_space);
        }
    }
    return guid;
}

static void AssertGamepadGuidEqual(const dmHID::GamepadGuid& expected, const dmHID::GamepadGuid& actual)
{
    ASSERT_EQ(expected.m_Bus,             actual.m_Bus);
    ASSERT_EQ(expected.m_CRC16,           actual.m_CRC16);
    ASSERT_EQ(expected.m_Vendor,          actual.m_Vendor);
    ASSERT_EQ(expected.m_Product,         actual.m_Product);
    ASSERT_EQ(expected.m_Version,         actual.m_Version);
    ASSERT_EQ(expected.m_DriverSignature, actual.m_DriverSignature);
    ASSERT_EQ(expected.m_DriverData,      actual.m_DriverData);
}

TEST(HIDGuidTest, CreateGUID)
{
    struct GuidTestCase
    {
        const char*    m_Name;
        uint16_t       m_Bus;
        uint16_t       m_Vendor;
        uint16_t       m_Product;
        uint16_t       m_Version;
        const char*    m_VendorName;
        const char*    m_ProductName;
        uint8_t        m_DriverSignature;
        uint8_t        m_DriverData;
        const char*    m_Expected;
    };

    const GuidTestCase cases[] = {
        { "standard",         0x0003, 0x045e, 0x028e, 0x0114, 0,           "Xbox 360 Controller",           0,   0, "030003f05e0400008e02000014010000" },
        { "vendor_name",      0x0003, 0x045e, 0x028e, 0x0114, "Microsoft", "Xbox 360 Controller",           'h', 1, "0300c6a95e0400008e02000014016801" },
        { "name_only",        0x0000, 0x0000, 0x0000, 0x0000, 0,           "Nintendo 3DS",                  0,   0, "000010324e696e74656e646f20334400" },
        { "name_only_driver", 0x0000, 0x0000, 0x0000, 0x0000, 0,           "PSP builtin joypad",            'v', 2, "000086bb505350206275696c74007602" },
        { "android",          0x0005, 0x045e, 0x02fd, 0x0000, 0,           "Xbox Wireless Controller",      0,   0, "050018dc5e040000fd02000000000000" },
        { "apple",            0x0005, 0x054c, 0x0ce6, 0x1234, 0,           "DualSense Wireless Controller", 'm', 3, "050057564c050000e60c000034126d03" },
    };

    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        char guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1];
        char reference_guid[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1];
        dmHID::GamepadGuid reference = SDLCreateJoystickGUIDReference(cases[i].m_Bus, cases[i].m_Vendor, cases[i].m_Product, cases[i].m_Version,
            cases[i].m_VendorName, cases[i].m_ProductName, cases[i].m_DriverSignature, cases[i].m_DriverData);
        dmHID::FormatGamepadGuid(&reference, reference_guid);

        dmHID::GamepadGuid guid = dmHID::CreateGUID(cases[i].m_Bus,
                                                    cases[i].m_Vendor,
                                                    cases[i].m_Product,
                                                    cases[i].m_Version,
                                                    cases[i].m_VendorName,
                                                    cases[i].m_ProductName,
                                                    cases[i].m_DriverSignature,
                                                    cases[i].m_DriverData);
        dmHID::FormatGamepadGuid(&guid, guid_string);

        AssertGamepadGuidEqual(reference, guid);
        ASSERT_STREQ(reference_guid, guid_string);
        ASSERT_STREQ(cases[i].m_Expected, guid_string);

#if defined(ANDROID) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
        char glfw_guid[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1];
        glfwCreateJoystickDeviceGuid(cases[i].m_Bus,
                                     cases[i].m_Vendor,
                                     cases[i].m_Product,
                                     cases[i].m_Version,
                                     cases[i].m_VendorName,
                                     cases[i].m_ProductName,
                                     cases[i].m_DriverSignature,
                                     cases[i].m_DriverData,
                                     glfw_guid);

        dmHID::GamepadGuid parsed_glfw_guid;
        ASSERT_TRUE(dmHID::ParseGamepadGuid(glfw_guid, &parsed_glfw_guid));
        AssertGamepadGuidEqual(reference, parsed_glfw_guid);
        ASSERT_STREQ(reference_guid, glfw_guid);
#endif
    }

    dmHID::GamepadIdentity identity = {};
    identity.m_Vendor  = 0x045e;
    identity.m_Product = 0x02fd;

    dmHID::GamepadGuid usb_guid = dmHID::CreateGUIDFromIdentity(0x0003, identity, "Xbox Wireless Controller", 0, 0, 0, 0, 0);
    dmHID::GamepadGuid bluetooth_guid = dmHID::CreateGUIDFromIdentity(0x0005, identity, "Xbox Wireless Controller", 0, 0, 0, 0, 0);
    ASSERT_EQ(ByteSwapLE16(0x0003), usb_guid.m_Bus);
    ASSERT_EQ(ByteSwapLE16(0x0005), bluetooth_guid.m_Bus);

    char usb_guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1];
    char bluetooth_guid_string[dmHID::MAX_GAMEPAD_GUID_LENGTH + 1];
    dmHID::FormatGamepadGuid(&usb_guid, usb_guid_string);
    dmHID::FormatGamepadGuid(&bluetooth_guid, bluetooth_guid_string);
    ASSERT_STREQ("030018dc5e040000fd02000000000000", usb_guid_string);
    ASSERT_STREQ("050018dc5e040000fd02000000000000", bluetooth_guid_string);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
