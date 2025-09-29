//========================================================================
// GLFW - An OpenGL framework
// Platform:    Win32/WGL
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
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Initialise timer
//========================================================================

void _glfwInitTimer( void )
{
    __int64 freq;

    // Check if we have a performance counter
    if( QueryPerformanceFrequency( (LARGE_INTEGER *)&freq ) )
    {
        // Performance counter is available => use it!
        _glfwLibrary.Timer.HasPerformanceCounter = GL_TRUE;

        // Counter resolution is 1 / counter frequency
        _glfwLibrary.Timer.Resolution = 1.0 / (double)freq;

        // Set start time for timer
        QueryPerformanceCounter( (LARGE_INTEGER *)&_glfwLibrary.Timer.t0_64 );
    }
    else
    {
        // No performace counter available => use the tick counter
        _glfwLibrary.Timer.HasPerformanceCounter = GL_FALSE;

        // Counter resolution is 1 ms
        _glfwLibrary.Timer.Resolution = 0.001;

        // Set start time for timer
        _glfwLibrary.Timer.t0_32 = _glfw_timeGetTime();
    }
}


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Return timer value in seconds
//========================================================================

double _glfwPlatformGetTime( void )
{
    double  t;
    __int64 t_64;

    if( _glfwLibrary.Timer.HasPerformanceCounter )
    {
        QueryPerformanceCounter( (LARGE_INTEGER *)&t_64 );
        t =  (double)(t_64 - _glfwLibrary.Timer.t0_64);
    }
    else
    {
        t = (double)(_glfw_timeGetTime() - _glfwLibrary.Timer.t0_32);
    }

    // Calculate the current time in seconds
    return t * _glfwLibrary.Timer.Resolution;
}


//========================================================================
// Set timer value in seconds
//========================================================================

void _glfwPlatformSetTime( double t )
{
    __int64 t_64;

    if( _glfwLibrary.Timer.HasPerformanceCounter )
    {
        QueryPerformanceCounter( (LARGE_INTEGER *)&t_64 );
        _glfwLibrary.Timer.t0_64 = t_64 - (__int64)(t/_glfwLibrary.Timer.Resolution);
    }
    else
    {
        _glfwLibrary.Timer.t0_32 = _glfw_timeGetTime() - (int)(t*1000.0);
    }
}


//========================================================================
// Put a thread to sleep for a specified amount of time
//========================================================================

void _glfwPlatformSleep( double time )
{
    DWORD t;

    if( time == 0.0 )
    {
	t = 0;
    }
    else if( time < 0.001 )
    {
        t = 1;
    }
    else if( time > 2147483647.0 )
    {
        t = 2147483647;
    }
    else
    {
        t = (DWORD)(time*1000.0 + 0.5);
    }
    Sleep( t );
}

