// Copyright 2020-2023 The Defold Foundation
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

#include "internal.h"
#include "ios/app/BaseView.h"

extern _GLFWwin g_Savewin;

// Additionally we postpone startup sequence until we have swapped gl-buffers twice in
// order to avoid black screen between launch image and game content.

//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    // Save window as glfw clears the memory on close
    g_Savewin = _glfwWin;
}

int _glfwPlatformGetDefaultFramebuffer( )
{
    return _glfwWin.frameBuffer; // non zero only if OpenGLES
}

//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void _glfwPlatformSetWindowSize( int width, int height )
{
}

//========================================================================
// Set the window position
//========================================================================

void _glfwPlatformSetWindowPos( int x, int y )
{
}

//========================================================================
// Iconify the window
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
}

//========================================================================
// Restore (un-iconify) the window
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
}

//========================================================================
// Swap buffers
//========================================================================

void _glfwPlatformSwapBuffers( void )
{
    BaseView* view = (BaseView*)_glfwWin.view;
    [view swapBuffers];
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    BaseView* view = (BaseView*)_glfwWin.view;
    [view setSwapInterval: interval];
}

//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
}

//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
}

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
GLFWAPI id glfwGetiOSUIWindow(void)
{
    return _glfwWin.window;
};
GLFWAPI id glfwGetiOSUIView(void)
{
    return _glfwWin.view;
};
GLFWAPI id glfwGetiOSEAGLContext(void)
{
    return _glfwWin.context;
};


