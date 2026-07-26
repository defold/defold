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

#import <UIKit/UIKit.h>

extern _GLFWwin g_Savewin;

// Host-injected UIView for embed mode (survives _glfwWin memset / terminate).
static UIView* g_ExternalView = nil;
static int g_IosEmbedHost = 0;

GLFWAPI void glfwIosSetExternalView(void* view)
{
    UIView* next = (UIView*)view;
    if (g_ExternalView == next)
        return;

    if (next)
    {
        [next retain];
        // Sticky until Terminate/_glfwIosClearEmbedHost — clearing the view
        // before dmEngineDestroy must not flip this off (Metal teardown checks it).
        g_IosEmbedHost = 1;
    }

    UIView* prev = g_ExternalView;
    g_ExternalView = next;

    // Keep _glfwWin.view in sync. PollEvents/SwapBuffers message _glfwWin.view
    // directly; if the host remounts the UIView and we only update g_ExternalView,
    // the old pointer becomes dangling and crashes on isKindOfClass:.
    _glfwWin.view = next;

    if (prev)
        [prev release];

    NSLog(@"glfwIosSetExternalView: %p embed=%d", (void*)next, g_IosEmbedHost);
}

GLFWAPI void* glfwIosGetExternalView(void)
{
    return (void*)g_ExternalView;
}

int _glfwIosIsEmbedHost(void)
{
    return g_IosEmbedHost;
}

void _glfwIosClearEmbedHost(void)
{
    g_IosEmbedHost = 0;
}

UIView* _glfwIosGetActiveView(void)
{
    if (g_ExternalView)
        return g_ExternalView;
    return (UIView*)_glfwWin.view;
}

// Additionally we postpone startup sequence until we have swapped gl-buffers twice in
// order to avoid black screen between launch image and game content.

//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
    // Save window as glfw clears the memory on close
    g_Savewin = _glfwWin;
    // Do not release host external view here — embed host owns it.
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
    id view = _glfwWin.view;
    if ([view isKindOfClass:[BaseView class]])
        [(BaseView*)view swapBuffers];
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
    id view = _glfwWin.view;
    if ([view isKindOfClass:[BaseView class]])
        [(BaseView*)view setSwapInterval: interval];
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
    if (_glfwWin.window)
        return _glfwWin.window;
    UIView* view = _glfwIosGetActiveView();
    return view ? view.window : nil;
};
GLFWAPI id glfwGetiOSUIView(void)
{
    return _glfwIosGetActiveView();
};
GLFWAPI id glfwGetiOSEAGLContext(void)
{
    return _glfwWin.context;
};


