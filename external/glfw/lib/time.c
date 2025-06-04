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

//========================================================================
// Return timer value in seconds
//========================================================================

GLFWAPI double GLFWAPIENTRY glfwGetTime( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return 0.0;
    }

    return _glfwPlatformGetTime();
}


//========================================================================
// Set timer value in seconds
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetTime( double time )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return;
    }

    _glfwPlatformSetTime( time );
}


//========================================================================
// Put a thread to sleep for a specified amount of time
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSleep( double time )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return;
    }

    _glfwPlatformSleep( time );
}

