//========================================================================
// GLFW - An OpenGL framework
// File:        joystick.c
// Platform:    Any
// API version: 2.6
// WWW:         http://glfw.sourceforge.net
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Camilla Berglund
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

//========================================================================
// glfwGetJoystickParam() - Determine joystick capabilities
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickParam( int joy, int param )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return 0;
    }

    return _glfwPlatformGetJoystickParam( joy, param );
}


//========================================================================
// glfwGetJoystickPos() - Get joystick axis positions
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickPos( int joy, float *pos,
    int numaxes )
{
    int       i;

    // Is GLFW initialized?
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
// glfwGetJoystickButtons() - Get joystick button states
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetJoystickButtons( int joy,
    unsigned char *buttons, int numbuttons )
{
    int       i;

    // Is GLFW initialized?
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
