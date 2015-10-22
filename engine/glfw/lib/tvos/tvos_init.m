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
#import <UIKit/UIKit.h>

#include "internal.h"

//========================================================================
// Terminate GLFW when exiting application
//========================================================================

static void glfw_atexit( void )
{
    glfwTerminate();
}

//========================================================================
// Initialize GLFW thread package
//========================================================================

static void initThreads( void )
{
    // Initialize critical section handle
    (void) pthread_mutex_init( &_glfwThrd.CriticalSection, NULL );

    // The first thread (the main thread) has ID 0
    _glfwThrd.NextID = 0;

    // Fill out information about the main thread (this thread)
    _glfwThrd.First.ID       = _glfwThrd.NextID ++;
    _glfwThrd.First.Function = NULL;
    _glfwThrd.First.PosixID  = pthread_self();
    _glfwThrd.First.Previous = NULL;
    _glfwThrd.First.Next     = NULL;
}

//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Initialize the GLFW library
//========================================================================

int _glfwPlatformInit( void )
{
    _glfwLibrary.AutoreleasePool = [[NSAutoreleasePool alloc] init];

    atexit( glfw_atexit );

    initThreads();

    _glfwPlatformSetTime( 0.0 );

    return GL_TRUE;
}

//========================================================================
// Close window, if open, and shut down GLFW
//========================================================================

int _glfwPlatformTerminate( void )
{
    glfwCloseWindow();

    [_glfwLibrary.AutoreleasePool release];
    _glfwLibrary.AutoreleasePool = nil;

    return GL_TRUE;
}
