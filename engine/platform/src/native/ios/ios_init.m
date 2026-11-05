//========================================================================
// GLFW - An OpenGL framework
// Platform:    Cocoa/NSOpenGL
// API Version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2009-2010 Camilla Berglund <elmindreda@elmindreda.org>
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

// Needed for _NSGetProgname
//#include <crt_externs.h>

// Modified for Defold: private mobile/web backend, without the GLFW API.
#import <UIKit/UIKit.h>

#include "internal.h"

//========================================================================
// Terminate Native when exiting application
//========================================================================

static void dmNative_atexit( void )
{
    dmNativeTerminate();
}

//========================================================================
// Initialize Native thread package
//========================================================================


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Initialize the Native library
//========================================================================

int dmNativeOSInit( void )
{
    dmNativeLibrary.AutoreleasePool = [[NSAutoreleasePool alloc] init];

    atexit( dmNative_atexit );

    dmNativeLibrary.OpenGLFramework =
        CFBundleGetBundleWithIdentifier( CFSTR( "com.apple.opengles" ) );
    if( dmNativeLibrary.OpenGLFramework == NULL )
    {
        fprintf( stderr, "dmNativeInit failing because you aren't linked to OpenGL\n" );
        return GL_FALSE;
    }


    return GL_TRUE;
}

//========================================================================
// Close window, if open, and shut down Native
//========================================================================

int dmNativeOSTerminate( void )
{
    dmNativeCloseWindow();

    return GL_TRUE;
}
