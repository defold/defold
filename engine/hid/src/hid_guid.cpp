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

#include "hid_private.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <dlib/dstrings.h>

#include <sdl/joystick/usb_ids.h>

namespace dmHID
{

static void EncodeGamepadGUID(const uint8_t guid_data[16], char guid[MAX_GAMEPAD_GUID_LENGTH + 1]);
static void CreateSDLGUID(uint16_t bus, uint16_t crc, uint16_t vendor, uint16_t product, uint16_t version, char guid[MAX_GAMEPAD_GUID_LENGTH + 1]);

static_assert(sizeof(GamepadGuid) == 16, "GamepadGuid must match the 16-byte SDL GUID layout");
static_assert(offsetof(GamepadGuid, m_Bus) == 0, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_CRC16) == 2, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_Vendor) == 4, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_Reserved0) == 6, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_Product) == 8, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_Reserved1) == 10, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_Version) == 12, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_DriverSignature) == 14, "GamepadGuid layout mismatch");
static_assert(offsetof(GamepadGuid, m_DriverData) == 15, "GamepadGuid layout mismatch");

static int HexCharToInt(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

bool ParseGamepadGuid(const char* guid_string, GamepadGuid* guid)
{
    memset(guid, 0, sizeof(*guid));

    if (guid_string == 0 || strlen(guid_string) != MAX_GAMEPAD_GUID_LENGTH)
        return false;

    uint8_t guid_data[16];
    for (uint32_t i = 0; i < 16; ++i)
    {
        const int hi = HexCharToInt(guid_string[i * 2 + 0]);
        const int lo = HexCharToInt(guid_string[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return false;
        guid_data[i] = (uint8_t) ((hi << 4) | lo);
    }

    memcpy(guid, guid_data, sizeof(*guid));
    return true;
}

void FormatGamepadGuid(const GamepadGuid* guid, char buffer[MAX_GAMEPAD_GUID_LENGTH + 1])
{
    EncodeGamepadGUID((const uint8_t*)guid, buffer);
}

static uint16_t UpdateCRC16(uint16_t crc, const uint8_t* data, uint32_t data_length)
{
    for (uint32_t i = 0; i < data_length; ++i)
    {
        uint8_t r = (uint8_t) crc ^ data[i];
        uint16_t byte_crc = 0;

        for (uint32_t bit = 0; bit < 8; ++bit)
        {
            byte_crc = (uint16_t) ((((byte_crc ^ r) & 1) ? 0xA001 : 0) ^ (byte_crc >> 1));
            r >>= 1;
        }

        crc = (uint16_t) (byte_crc ^ (crc >> 8));
    }

    return crc;
}

static uint16_t UpdateCRC16(uint16_t crc, const char* string)
{
    if (string == 0)
        return crc;

    return UpdateCRC16(crc, (const uint8_t*) string, (uint32_t) strlen(string));
}

static void EncodeGamepadGUID(const uint8_t guid_data[16], char guid[MAX_GAMEPAD_GUID_LENGTH + 1])
{
    static const char hex[] = "0123456789abcdef";

    for (uint32_t i = 0; i < 16; ++i)
    {
        guid[i * 2 + 0] = hex[(guid_data[i] >> 4) & 0x0f];
        guid[i * 2 + 1] = hex[guid_data[i] & 0x0f];
    }

    guid[MAX_GAMEPAD_GUID_LENGTH] = '\0';
}

static void CreateSDLGUID(uint16_t bus, uint16_t crc, uint16_t vendor, uint16_t product, uint16_t version, char guid[MAX_GAMEPAD_GUID_LENGTH + 1])
{
    uint8_t guid_data[16] = {};

    guid_data[0] = (uint8_t) (bus & 0xff);
    guid_data[1] = (uint8_t) (bus >> 8);
    guid_data[2] = (uint8_t) (crc & 0xff);
    guid_data[3] = (uint8_t) (crc >> 8);
    guid_data[4] = (uint8_t) (vendor & 0xff);
    guid_data[5] = (uint8_t) (vendor >> 8);
    guid_data[8] = (uint8_t) (product & 0xff);
    guid_data[9] = (uint8_t) (product >> 8);
    guid_data[12] = (uint8_t) (version & 0xff);
    guid_data[13] = (uint8_t) (version >> 8);

    EncodeGamepadGUID(guid_data, guid);
}

static const char* GetSDLCompatibleControllerName(const GamepadIdentity& identity, const char* fallback_name)
{
    if (identity.m_Vendor == USB_VENDOR_NINTENDO)
    {
        switch (identity.m_Product)
        {
            case USB_PRODUCT_NINTENDO_SWITCH_PRO:          return "Nintendo Switch Pro Controller";
            case USB_PRODUCT_NINTENDO_SWITCH_JOYCON_PAIR:  return "Nintendo Switch Joy-Con (L/R)";
            case USB_PRODUCT_NINTENDO_SWITCH_JOYCON_LEFT:  return "Nintendo Switch Joy-Con (L)";
            case USB_PRODUCT_NINTENDO_SWITCH_JOYCON_RIGHT: return "Nintendo Switch Joy-Con (R)";
            default: break;
        }
    }

    if (identity.m_Vendor == USB_VENDOR_SONY)
    {
        switch (identity.m_Product)
        {
            case USB_PRODUCT_SONY_DS4_SLIM:
                if (identity.m_HasDualshockTouchpad)
                    return "PS4 Controller";
                return "DualShock 4 Wireless Controller";
            case USB_PRODUCT_SONY_DS5:
                return "DualSense Wireless Controller";
            default: break;
        }
    }

    if (identity.m_Vendor == USB_VENDOR_MICROSOFT)
    {
        switch (identity.m_Product)
        {
            case USB_PRODUCT_XBOX360_WIRED_CONTROLLER:          return "Xbox 360 Controller";
            case USB_PRODUCT_XBOX360_WIRELESS_RECEIVER:         return "Xbox 360 Controller";
            case USB_PRODUCT_XBOX_ONE_ELITE_SERIES_2_BLUETOOTH: return "Xbox Elite Wireless Controller Series 2";
            case USB_PRODUCT_XBOX_SERIES_X_BLE:                 return "Xbox Wireless Controller";
            case USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH:         return "Xbox Wireless Controller";
            default: break;
        }
    }

    if (identity.m_Vendor == USB_VENDOR_BACKBONE)
    {
        if (identity.m_Product == USB_PRODUCT_BACKBONE_ONE_IOS_PS5)
            return "Backbone One PlayStation Edition";
        if (identity.m_Product == USB_PRODUCT_BACKBONE_ONE_IOS)
            return "Backbone One";
    }

    if (fallback_name && fallback_name[0] != '\0')
        return fallback_name;

    return "Game Controller";
}

void CreateGUIDFromProduct(uint16_t vendor, uint16_t product, uint16_t version, char guid[MAX_GAMEPAD_GUID_LENGTH + 1])
{
    CreateSDLGUID(0x0003, 0, vendor, product, version, guid);
}

void CreateGUIDFromIdentity(const GamepadIdentity& identity, const char* fallback_name, const char** axis_keys, uint32_t axis_count, const char** button_keys, uint32_t button_count, uint16_t button_mask, char guid[MAX_GAMEPAD_GUID_LENGTH + 1])
{
    const char* name_for_guid = GetSDLCompatibleControllerName(identity, fallback_name);
    uint16_t signature = 0;

    if (axis_count > 0 || button_count > 0)
    {
        signature = UpdateCRC16(signature, name_for_guid);

        for (uint32_t i = 0; i < axis_count; ++i)
        {
            signature = UpdateCRC16(signature, axis_keys[i]);
        }

        for (uint32_t i = 0; i < button_count; ++i)
        {
            signature = UpdateCRC16(signature, button_keys[i]);
        }
    }
    else
    {
        signature = button_mask;
    }

    CreateSDLGUID(0x0005, 0, identity.m_Vendor, identity.m_Product, signature, guid);
}

void GetGamepadDeviceNameSDL(HContext context, HGamepad gamepad, char device_name[MAX_GAMEPAD_NAME_LENGTH])
{
    device_name[0] = '\0';

    char fallback_name[MAX_GAMEPAD_NAME_LENGTH];
    GetGamepadDeviceName(context, gamepad, fallback_name);

    GamepadGuid guid;
    if (!GetGamepadDeviceGuid(context, gamepad, &guid))
    {
        dmStrlCpy(device_name, fallback_name, MAX_GAMEPAD_NAME_LENGTH);
        return;
    }

    GamepadIdentity identity = {};
    identity.m_Vendor  = guid.m_Vendor;
    identity.m_Product = guid.m_Product;

    const char* sdl_name = GetSDLCompatibleControllerName(identity, fallback_name);
    dmStrlCpy(device_name, sdl_name, MAX_GAMEPAD_NAME_LENGTH);
}

} // namespace dmHID
