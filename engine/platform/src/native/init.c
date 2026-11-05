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


// Modified for Defold: private mobile/web backend, without the GLFW API.
#define _init_c_
#include "internal.h"


//************************************************************************
//****                    Native backend functions                         ****
//************************************************************************

//========================================================================
// Initialize various Native state
//========================================================================

int dmNativeInit( void )
{
    // Is Native already initialized?
    if( dmNativeInitialized )
    {
        return GL_TRUE;
    }

#if defined(DM_PLATFORM_IOS)
    memset( &dmNativeLibrary, 0, sizeof( dmNativeLibrary ) );
#endif
    memset( &dmNativeWin, 0, sizeof( dmNativeWin ) );

    // Window is not yet opened
    dmNativeWin.opened = GL_FALSE;

    // Default enable/disable settings
    dmNativeWin.sysKeysDisabled = GL_FALSE;

    // Clear window hints


    // Platform specific initialization
    if( !dmNativeOSInit() )
    {
        return GL_FALSE;
    }

    // Form now on, Native state is valid
    dmNativeInitialized = GL_TRUE;

    return GL_TRUE;
}


//========================================================================
// Close window and kill all threads.
//========================================================================

void dmNativeTerminate( void )
{
    // Is Native initialized?
    if( !dmNativeInitialized )
    {
        return;
    }

    // Platform specific termination
    if( !dmNativeOSTerminate() )
    {
        return;
    }

    // Native is no longer initialized
    dmNativeInitialized = GL_FALSE;
}


//========================================================================
// Get Native version
//========================================================================
