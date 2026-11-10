// Copyright 2020-2026 The Defold Foundation
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

extern Nativewin g_Savewin;

// Additionally we postpone startup sequence until we have swapped gl-buffers twice in
// order to avoid black screen between launch image and game content.

//========================================================================
// Properly kill the window / video display
//========================================================================

void dmNativeOSCloseWindow( void )
{
    // Save window as dmNative clears the memory on close
    g_Savewin = dmNativeWin;
}

int dmNativeOSGetDefaultFramebuffer( )
{
    return dmNativeWin.frameBuffer; // non zero only if OpenGLES
}

//========================================================================
// Set the window title
//========================================================================

void dmNativeOSSetWindowTitle( const char *title )
{
}

//========================================================================
// Set the window size
//========================================================================

void dmNativeOSSetWindowSize( int width, int height )
{
}

//========================================================================
// Set the window position
//========================================================================

void dmNativeOSSetWindowPos( int x, int y )
{
}

//========================================================================
// Iconify the window
//========================================================================

void dmNativeOSIconifyWindow( void )
{
}

//========================================================================
// Restore (un-iconify) the window
//========================================================================

void dmNativeOSRestoreWindow( void )
{
}

//========================================================================
// Swap buffers
//========================================================================

void dmNativeOSSwapBuffers( void )
{
    BaseView* view = (BaseView*)dmNativeWin.view;
    [view swapBuffers];
}

//========================================================================
// Set double buffering swap interval
//========================================================================

void dmNativeOSSwapInterval( int interval )
{
    BaseView* view = (BaseView*)dmNativeWin.view;
    [view setSwapInterval: interval];
}

//========================================================================
// Write back window parameters into Native window structure
//========================================================================

void dmNativeOSRefreshWindowParams( void )
{
}

//========================================================================
// Wait for new window and input events
//========================================================================

void dmNativeOSWaitEvents( void )
{
}

//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void dmNativeOSHideMouseCursor( void )
{
}

//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void dmNativeOSShowMouseCursor( void )
{
}

//========================================================================
// Set physical mouse cursor position
//========================================================================

void dmNativeOSSetMouseCursorPos( int x, int y )
{
}

//========================================================================
// Defold extension: Get native references (window, view and context)
//========================================================================
id dmNativeGetiOSUIWindow(void)
{
    return dmNativeWin.window;
};
id dmNativeGetiOSUIView(void)
{
    return dmNativeWin.view;
};
id dmNativeGetiOSEAGLContext(void)
{
    return dmNativeWin.context;
};


