//========================================================================
// GLFW - An OpenGL framework
// Platform:    Any
// API version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"


//************************************************************************
//****                    GLFW user functions                         ****
//************************************************************************

static unsigned short glfwCrc16ForByte( unsigned char value )
{
    unsigned short crc;
    unsigned int bit;

    crc = 0;
    for( bit = 0; bit < 8; ++bit )
    {
        crc = (unsigned short)((((crc ^ value) & 1) ? 0xA001 : 0) ^ (crc >> 1));
        value >>= 1;
    }
    return crc;
}


static unsigned short glfwCrc16( unsigned short crc, const char* string )
{
    if( string )
    {
        while( *string )
        {
            crc = (unsigned short)(glfwCrc16ForByte( (unsigned char)crc ^ (unsigned char)*string++ ) ^ (crc >> 8));
        }
    }

    return crc;
}


static void glfwSetGuidWord( unsigned char guid_data[16], unsigned int offset, unsigned short value )
{
    guid_data[offset + 0] = (unsigned char)(value & 0xff);
    guid_data[offset + 1] = (unsigned char)(value >> 8);
}


static void glfwEncodeGuid( const unsigned char guid_data[16], char guid[GLFW_JOYSTICK_DEVICE_GUID_LENGTH + 1] )
{
    static const char hex[] = "0123456789abcdef";
    unsigned int i;

    for( i = 0; i < 16; ++i )
    {
        guid[i * 2 + 0] = hex[(guid_data[i] >> 4) & 0x0f];
        guid[i * 2 + 1] = hex[guid_data[i] & 0x0f];
    }

    guid[GLFW_JOYSTICK_DEVICE_GUID_LENGTH] = '\0';
}


//========================================================================
// Create an SDL-compatible joystick device guid
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwCreateJoystickDeviceGuid( unsigned short bus,
                                                        unsigned short vendor,
                                                        unsigned short product,
                                                        unsigned short version,
                                                        const char* vendor_name,
                                                        const char* product_name,
                                                        unsigned char driver_signature,
                                                        unsigned char driver_data,
                                                        char guid[GLFW_JOYSTICK_DEVICE_GUID_LENGTH + 1] )
{
    unsigned char guid_data[16];
    unsigned short crc;
    unsigned int i;

    memset( guid_data, 0, sizeof( guid_data ) );
    crc = 0;

    if( vendor_name && *vendor_name && product_name && *product_name )
    {
        crc = glfwCrc16( crc, vendor_name );
        crc = glfwCrc16( crc, " " );
        crc = glfwCrc16( crc, product_name );
    }
    else if( product_name )
    {
        crc = glfwCrc16( crc, product_name );
    }

    glfwSetGuidWord( guid_data, 0, bus );
    glfwSetGuidWord( guid_data, 2, crc );

    if( vendor )
    {
        glfwSetGuidWord( guid_data, 4, vendor );
        glfwSetGuidWord( guid_data, 8, product );
        glfwSetGuidWord( guid_data, 12, version );
        guid_data[14] = driver_signature;
        guid_data[15] = driver_data;
    }
    else
    {
        unsigned int available_space = sizeof( guid_data ) - 4;

        if( driver_signature )
        {
            available_space -= 2;
            guid_data[14] = driver_signature;
            guid_data[15] = driver_data;
        }

        if( product_name )
        {
            for( i = 0; i + 1 < available_space && product_name[i]; ++i )
            {
                guid_data[4 + i] = (unsigned char)product_name[i];
            }
        }
    }

    glfwEncodeGuid( guid_data, guid );
}


//========================================================================
// Determine joystick capabilities
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickParam( int joy, int param )
{
    if( !_glfwInitialized )
    {
        return 0;
    }

    return _glfwPlatformGetJoystickParam( joy, param );
}


//========================================================================
// Get joystick axis positions
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickPos( int joy, float *pos, int numaxes )
{
    int i;

    if( !_glfwInitialized )
    {
        return 0;
    }

    // Clear positions
    for( i = 0; i < numaxes; i++ )
    {
        pos[ i ] = 0.0f;
    }

    return _glfwPlatformGetJoystickPos( joy, pos, numaxes );
}


//========================================================================
// Get joystick button states
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickButtons( int joy,
                                                 unsigned char *buttons,
                                                 int numbuttons )
{
    int i;

    if( !_glfwInitialized )
    {
        return 0;
    }

    // Clear button states
    for( i = 0; i < numbuttons; i++ )
    {
        buttons[ i ] = GLFW_RELEASE;
    }

    return _glfwPlatformGetJoystickButtons( joy, buttons, numbuttons );
}

//========================================================================
// Get joystick hat states
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickHats( int joy,
                                              unsigned char *hats,
                                              int numhats )
{
    int i;

    if( !_glfwInitialized )
    {
        return 0;
    }

    // Clear hat states, ie all centered.
    for( i = 0; i < numhats; i++ )
    {
        hats[ i ] = GLFW_HAT_CENTERED;
    }

    return _glfwPlatformGetJoystickHats( joy, hats, numhats );
}

//========================================================================
// Get joystick device id
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickDeviceId( int joy, char** device_id )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return 0;
    }

    return _glfwPlatformGetJoystickDeviceId( joy, device_id );
}
