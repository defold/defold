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

#include "window_mobile_web_backend.h"
#include <string.h>

static unsigned short dmNativeCrc16ForByte(unsigned char value)
{
    unsigned short crc;
    unsigned int bit;

    crc = 0;
    for ( bit = 0; bit < 8; ++bit )
    {
        crc = (unsigned short)((((crc ^ value) & 1) ? 0xA001 : 0) ^ (crc >> 1));
        value >>= 1;
    }

    return crc;
}


static unsigned short dmNativeCrc16(unsigned short crc, const char* string)
{
    if (string)
    {
        while ( *string)
        {
            crc = (unsigned short)(dmNativeCrc16ForByte( (unsigned char)crc ^ (unsigned char)*string++) ^ (crc >> 8));
        }
    }

    return crc;
}


static void dmNativeSetGuidWord(unsigned char guid_data[16], unsigned int offset, unsigned short value)
{
    guid_data[offset + 0] = (unsigned char)(value & 0xff);
    guid_data[offset + 1] = (unsigned char)(value >> 8);
}


static void dmNativeEncodeGuid(const unsigned char guid_data[16], char guid[NATIVE_JOYSTICK_DEVICE_GUID_LENGTH + 1])
{
    static const char hex[] = "0123456789abcdef";
    unsigned int i;

    for ( i = 0; i < 16; ++i )
    {
        guid[i * 2 + 0] = hex[(guid_data[i] >> 4) & 0x0f];
        guid[i * 2 + 1] = hex[guid_data[i] & 0x0f];
    }

    guid[NATIVE_JOYSTICK_DEVICE_GUID_LENGTH] = '\0';
}


//========================================================================
// Create an SDL-compatible joystick device guid
//========================================================================

void dmNativeCreateJoystickDeviceGuid(unsigned short bus,
                                      unsigned short vendor,
                                      unsigned short product,
                                      unsigned short version,
                                      const char* vendor_name,
                                      const char* product_name,
                                      unsigned char driver_signature,
                                      unsigned char driver_data,
                                      char guid[NATIVE_JOYSTICK_DEVICE_GUID_LENGTH + 1])
{
    unsigned char guid_data[16];
    unsigned short crc;
    unsigned int i;

    memset(guid_data, 0, sizeof(guid_data) );
    crc = 0;

    if (vendor_name && *vendor_name && product_name && *product_name)
    {
        crc = dmNativeCrc16(crc, vendor_name);
        crc = dmNativeCrc16(crc, " ");
        crc = dmNativeCrc16(crc, product_name);
    }
    else if (product_name)
    {
        crc = dmNativeCrc16(crc, product_name);
    }

    dmNativeSetGuidWord(guid_data, 0, bus);
    dmNativeSetGuidWord(guid_data, 2, crc);

    if (vendor)
    {
        dmNativeSetGuidWord(guid_data, 4, vendor);
        dmNativeSetGuidWord(guid_data, 8, product);
        dmNativeSetGuidWord(guid_data, 12, version);
        guid_data[14] = driver_signature;
        guid_data[15] = driver_data;
    }
    else
    {
        unsigned int available_space = sizeof(guid_data) - 4;

        if (driver_signature)
        {
            available_space -= 2;
            guid_data[14] = driver_signature;
            guid_data[15] = driver_data;
        }

        if (product_name)
        {
            for ( i = 0; i + 1 < available_space && product_name[i]; ++i )
            {
                guid_data[4 + i] = (unsigned char)product_name[i];
            }
        }
    }

    dmNativeEncodeGuid(guid_data, guid);
}
