//========================================================================
// GLFW - An OpenGL framework
// File:        init.c
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

#define _init_c_
#include "internal.h"


//************************************************************************
//****                    GLFW user functions                         ****
//************************************************************************

//========================================================================
// glfwInit() - Initialize various GLFW state
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwInit( void )
{
    // Is GLFW already initialized?
    if( _glfwInitialized )
    {
        return GL_TRUE;
    }

    // Window is not yet opened
    _glfwWin.Opened = GL_FALSE;

    // Default enable/disable settings
    _glfwWin.SysKeysDisabled = GL_FALSE;

    // Clear window hints
    _glfwClearWindowHints();

    // Platform specific initialization
    if( !_glfwPlatformInit() )
    {
        return GL_FALSE;
    }

    // Form now on, GLFW state is valid
    _glfwInitialized = GL_TRUE;

    return GL_TRUE;
}



//========================================================================
// glfwTerminate() - Close window and kill all threads.
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwTerminate( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return;
    }

    // Platform specific termination
    if( !_glfwPlatformTerminate() )
    {
        return;
    }

    // GLFW is no longer initialized
    _glfwInitialized = GL_FALSE;
}


//========================================================================
// glfwGetVersion() - Get GLFW version
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwGetVersion( int *major, int *minor,
    int *rev )
{
    if( major != NULL ) *major = GLFW_VERSION_MAJOR;
    if( minor != NULL ) *minor = GLFW_VERSION_MINOR;
    if( rev   != NULL ) *rev   = GLFW_VERSION_REVISION;
}

