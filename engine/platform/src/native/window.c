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
#include "internal.h"

#include <limits.h>


#ifndef GL_VERSION_3_2

#define GL_CONTEXT_CORE_PROFILE_BIT       0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#define GL_CONTEXT_PROFILE_MASK           0x9126

#endif /*GL_VERSION_3_2*/


//************************************************************************
//****                  Native internal functions                       ****
//************************************************************************


//========================================================================
// Handle the input tracking part of window deactivation
//========================================================================

void dmNativeInputDeactivation( void )
{
    int i;

    // Release all keyboard keys
    for( i = 0; i <= NATIVE_KEY_LAST; i ++ )
    {
        if( dmNativeInput.Key[ i ] == NATIVE_PRESS )
        {
            dmNativeInputKey( i, NATIVE_RELEASE );
        }
    }

    // Release all mouse buttons
    for( i = 0; i <= NATIVE_MOUSE_BUTTON_LAST; i ++ )
    {
        if( dmNativeInput.MouseButton[ i ] == NATIVE_PRESS )
        {
            dmNativeInputMouseClick( i, NATIVE_RELEASE );
        }
    }
}


//========================================================================
// Clear all input state
//========================================================================

void dmNativeClearInput( void )
{
    int i;

    // Release all keyboard keys
    for( i = 0; i <= NATIVE_KEY_LAST; i ++ )
    {
        dmNativeInput.Key[ i ] = NATIVE_RELEASE;
    }

    // Clear last character
    dmNativeInput.LastChar = 0;

    // Release all mouse buttons
    for( i = 0; i <= NATIVE_MOUSE_BUTTON_LAST; i ++ )
    {
        dmNativeInput.MouseButton[ i ] = NATIVE_RELEASE;
    }

    // Set mouse position to (0,0)
    dmNativeInput.MousePosX = 0;
    dmNativeInput.MousePosY = 0;
    dmNativeInput.MouseLeftButtonFromTouch = 0;
    dmNativeInput.MousePositionFromTouch = 0;

    // Set mouse wheel position to 0
    dmNativeInput.WheelPos = 0;

    // The default is to use non sticky keys and mouse buttons
    dmNativeInput.StickyKeys = GL_FALSE;
    dmNativeInput.StickyMouseButtons = GL_FALSE;

    for (i = 0; i < NATIVE_MAX_TOUCH; ++i) {
        memset(&dmNativeInput.Touch[i], 0, sizeof(dmNativeInput.Touch[i]));
        dmNativeInput.Touch[i].Id = i;
        dmNativeInput.Touch[i].Reference = 0x0;
        dmNativeInput.Touch[i].Phase = NATIVE_PHASE_IDLE;
    }

    // The default is to disable key repeat
    dmNativeInput.KeyRepeat = GL_FALSE;
}


//========================================================================
// Register keyboard activity
//========================================================================

void dmNativeInputKey( int key, int action )
{
    int keyrepeat = 0;

    if( key < 0 || key > NATIVE_KEY_LAST )
    {
        return;
    }

    // Are we trying to release an already released key?
    if( action == NATIVE_RELEASE && dmNativeInput.Key[ key ] != NATIVE_PRESS )
    {
        return;
    }

    // Register key action
    if( action == NATIVE_RELEASE && dmNativeInput.StickyKeys )
    {
        dmNativeInput.Key[ key ] = NATIVE_STICK;
    }
    else
    {
        keyrepeat = (dmNativeInput.Key[ key ] == NATIVE_PRESS) &&
                    (action == NATIVE_PRESS);
        dmNativeInput.Key[ key ] = (char) action;
    }

    // Call user callback function
    if( dmNativeWin.keyCallback && (dmNativeInput.KeyRepeat || !keyrepeat) )
    {
        dmNativeWin.keyCallback( key, action );
    }
}


//========================================================================
// Register (keyboard) character activity
//========================================================================

void dmNativeInputChar( int character, int action )
{
    // Valid Unicode (ISO 10646) character?
    if( !( (character >= 32 && character <= 126) || character >= 160 ) )
    {
        return;
    }

    if( action != NATIVE_PRESS )
    {
        // This intentionally breaks release notifications for Unicode
        // characters, partly to see if anyone cares but mostly because it's
        // a nonsensical concept to begin with
        //
        // It will remain broken either until its removal in the 3.0 API or
        // until someone explains, in a way that makes sense to people outside
        // the US and Scandinavia, what "Unicode character up" actually means
        //
        // If what you want is "physical key up" then you should be using the
        // key functions and/or the key callback, NOT the Unicode input
        //
        // However, if your particular application uses this misfeature for...
        // something, you can re-enable it by removing this if-statement
        return;
    }

    if( dmNativeWin.charCallback )
    {
        dmNativeWin.charCallback( character, action );
    }
}


//========================================================================
// Register unfinished (marked) keyboard input
//========================================================================

void dmNativeSetMarkedText( char* text )
{
    if( dmNativeWin.markedTextCallback )
    {
        dmNativeWin.markedTextCallback( text );
    }
}


//========================================================================
// Register mouse button clicks
//========================================================================

void dmNativeInputMouseClick( int button, int action )
{
    if( button >= 0 && button <= NATIVE_MOUSE_BUTTON_LAST )
    {
        if (dmNativeInput.MouseButton[ button ] == NATIVE_CLICKED) {
            return;
        }

        // Capture the press source before later movement or a sticky release can change it.
        if( button == NATIVE_MOUSE_BUTTON_LEFT && action == NATIVE_PRESS )
        {
            dmNativeInput.MouseLeftButtonFromTouch = dmNativeInput.MousePositionFromTouch;
        }

        if( action == NATIVE_RELEASE && dmNativeInput.MouseButton[ button ] == NATIVE_PRESS )
        {
            dmNativeInput.MouseButton[ button ] = NATIVE_CLICKED;
        } else if( action == NATIVE_RELEASE && dmNativeInput.StickyMouseButtons )
        {
            // Register mouse button action
            dmNativeInput.MouseButton[ button ] = NATIVE_STICK;
        }
        else
        {
            dmNativeInput.MouseButton[ button ] = (char) action;
        }

        // Call user callback function
        if( dmNativeWin.mouseButtonCallback )
        {
            dmNativeWin.mouseButtonCallback( button, action );
        }
    }
}


//========================================================================
// Return the available framebuffer config closest to the desired values
// This is based on the manual GLX Visual selection from 2.6
//========================================================================


//************************************************************************
//****                    Native backend functions                         ****
//************************************************************************

//========================================================================
// Create the Native window and its associated context
//========================================================================

int dmNativeOpenWindow(const WindowCreateParams* params)
{
    if (!dmNativeInitialized || dmNativeWin.opened)
        return 0;

    int width = params->m_Width;
    int height = params->m_Height;
    int mode = params->m_Fullscreen ? NATIVE_FULLSCREEN : NATIVE_WINDOW;
    int opengl = params->m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGL || params->m_GraphicsApi == WINDOW_GRAPHICS_API_OPENGLES;
    Nativefbconfig fbconfig = {0};
    Nativewndconfig wndconfig = {0};
    fbconfig.redBits = fbconfig.greenBits = fbconfig.blueBits = 8;
    fbconfig.alphaBits = params->m_ContextAlphabits;
    fbconfig.depthBits = 32;
    fbconfig.stencilBits = 8;
    fbconfig.samples = params->m_Samples;
    wndconfig.mode = mode;
    wndconfig.highDPI = params->m_HighDPI;
    wndconfig.clientAPI = opengl ? NATIVE_OPENGL_API : NATIVE_NO_API;
#if defined(DM_PLATFORM_IOS)
    wndconfig.glMajor = 3;
    dmNativeOSSetViewType(wndconfig.clientAPI);
#else
    wndconfig.glMajor = 1;
#endif

    // Clear Native window state
    dmNativeWin.active         = GL_TRUE;
    dmNativeWin.iconified      = GL_FALSE;
    dmNativeWin.mouseLock      = GL_FALSE;
    dmNativeWin.autoPollEvents = GL_TRUE;
    dmNativeClearInput();

    // Unregister all callback functions
    dmNativeWin.windowSizeCallback    = NULL;
    dmNativeWin.windowCloseCallback   = NULL;
    dmNativeWin.windowRefreshCallback = NULL;
    dmNativeWin.keyCallback           = NULL;
    dmNativeWin.charCallback          = NULL;
    dmNativeWin.markedTextCallback    = NULL;
    dmNativeWin.mousePosCallback      = NULL;
    dmNativeWin.mouseButtonCallback   = NULL;
    dmNativeWin.mouseWheelCallback    = NULL;

    // Check width & height
    if( width > 0 && height <= 0 )
    {
        // Set the window aspect ratio to 4:3
        height = (width * 3) / 4;
    }
    else if( width <= 0 && height > 0 )
    {
        // Set the window aspect ratio to 4:3
        width = (height * 4) / 3;
    }
    else if( width <= 0 && height <= 0 )
    {
        // Default window size
        width  = 640;
        height = 480;
    }

    // Remember window settings
    dmNativeWin.width      = width;
    dmNativeWin.height     = height;
    dmNativeWin.fullscreen = (mode == NATIVE_FULLSCREEN ? GL_TRUE : GL_FALSE);

    // Platform specific window opening routine
    if( !dmNativeOSOpenWindow( width, height, &wndconfig, &fbconfig ) )
    {
        dmNativeCloseWindow();
        return GL_FALSE;
    }

    // Flag that window is now opened
    dmNativeWin.opened = GL_TRUE;

    // Read back window and context parameters
    dmNativeOSRefreshWindowParams();

    if (wndconfig.clientAPI != NATIVE_NO_API)
    {
        dmNativeRefreshContextParams();

        if( dmNativeWin.glMajor < wndconfig.glMajor ||
            ( dmNativeWin.glMajor == wndconfig.glMajor &&
              dmNativeWin.glMinor < wndconfig.glMinor ) )
        {
            dmNativeCloseWindow();
            return GL_FALSE;
        }

    }
    return GL_TRUE;
}


//========================================================================
// Properly kill the window / video display
//========================================================================

void dmNativeCloseWindow( void )
{
    if( !dmNativeInitialized )
    {
        return;
    }

    // Show mouse pointer again (if hidden)
    dmNativeWin.mouseLock = GL_FALSE;

    dmNativeOSCloseWindow();

    memset( &dmNativeWin, 0, sizeof(dmNativeWin) );
}

int dmNativeGetDefaultFramebuffer( void )
{
    return dmNativeOSGetDefaultFramebuffer();
}

//========================================================================
// Set the window title
//========================================================================

void dmNativeSetWindowTitle( const char *title )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Set window title
    dmNativeOSSetWindowTitle( title );
}


//========================================================================
// Get the window size
//========================================================================

void dmNativeGetWindowSize( int *width, int *height )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    if( width != NULL )
    {
        *width = dmNativeWin.width;
    }
    if( height != NULL )
    {
        *height = dmNativeWin.height;
    }
}


//========================================================================
// Set the window size
//========================================================================

void dmNativeSetWindowSize( int width, int height )
{
    if( !dmNativeInitialized || !dmNativeWin.opened || dmNativeWin.iconified )
    {
        return;
    }

    // Don't do anything if the window size did not change
    if( width == dmNativeWin.width && height == dmNativeWin.height )
    {
        return;
    }

    // Change window size
    dmNativeOSSetWindowSize( width, height );

    // Refresh window parameters (may have changed due to changed video
    // modes)
    dmNativeOSRefreshWindowParams();
}


//========================================================================
// Set the window position
//========================================================================

void dmNativeSetWindowPos( int x, int y )
{
    if( !dmNativeInitialized || !dmNativeWin.opened || dmNativeWin.fullscreen ||
        dmNativeWin.iconified )
    {
        return;
    }

    // Set window position
    dmNativeOSSetWindowPos( x, y );
}


//========================================================================
// Window iconification
//========================================================================

void dmNativeIconifyWindow( void )
{
    if( !dmNativeInitialized || !dmNativeWin.opened || dmNativeWin.iconified )
    {
        return;
    }

    // Iconify window
    dmNativeOSIconifyWindow();
}


//========================================================================
// Window un-iconification
//========================================================================


int dmNativeGetWindowRefreshRate( void )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return 0;
    }

    return dmNativeOSGetWindowRefreshRate();
}

//========================================================================
// Swap buffers (double-buffering) and poll any new events
//========================================================================

void dmNativeSwapBuffers( void )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    dmNativeOSSwapBuffers();

    // Check for window messages
    if( dmNativeWin.autoPollEvents )
    {
        dmNativePollEvents();
    }
}


//========================================================================
// Set double buffering swap interval (0 = vsync off)
//========================================================================

void dmNativeSwapInterval( int interval )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Set double buffering swap interval
    dmNativeOSSwapInterval( interval );
}


//========================================================================
// Get window parameter
//========================================================================

int dmNativeGetWindowParam( WindowState param )
{
    if( !dmNativeInitialized )
    {
        return 0;
    }

    // Is the window opened?
    if( !dmNativeWin.opened )
    {
        if( param == WINDOW_STATE_OPENED )
        {
            return GL_FALSE;
        }
        return 0;
    }

    // Window parameters
    switch( param )
    {
        case WINDOW_STATE_OPENED:
            return GL_TRUE;
        case WINDOW_STATE_ACTIVE:
            return dmNativeWin.active;
        case WINDOW_STATE_ICONIFIED:
            return dmNativeWin.iconified;
        case WINDOW_STATE_ACCELERATED:
            return dmNativeWin.accelerated;
        case WINDOW_STATE_RED_BITS:
            return dmNativeWin.redBits;
        case WINDOW_STATE_GREEN_BITS:
            return dmNativeWin.greenBits;
        case WINDOW_STATE_BLUE_BITS:
            return dmNativeWin.blueBits;
        case WINDOW_STATE_ALPHA_BITS:
            return dmNativeWin.alphaBits;
        case WINDOW_STATE_DEPTH_BITS:
            return dmNativeWin.depthBits;
        case WINDOW_STATE_STENCIL_BITS:
            return dmNativeWin.stencilBits;
        case WINDOW_STATE_ACCUM_RED_BITS:
            return dmNativeWin.accumRedBits;
        case WINDOW_STATE_ACCUM_GREEN_BITS:
            return dmNativeWin.accumGreenBits;
        case WINDOW_STATE_ACCUM_BLUE_BITS:
            return dmNativeWin.accumBlueBits;
        case WINDOW_STATE_ACCUM_ALPHA_BITS:
            return dmNativeWin.accumAlphaBits;
        case WINDOW_STATE_AUX_BUFFERS:
            return dmNativeWin.auxBuffers;
        case WINDOW_STATE_STEREO:
            return dmNativeWin.stereo;
        case WINDOW_STATE_REFRESH_RATE:
            return dmNativeWin.refreshRate;
        case WINDOW_STATE_WINDOW_NO_RESIZE:
            return dmNativeWin.windowNoResize;
        case WINDOW_STATE_FSAA_SAMPLES:
            return dmNativeWin.samples;
        default:
            return 0;
    }
}


//========================================================================
// Set callback function for window size changes
//========================================================================

void dmNativeSetWindowSizeCallback( Nativewindowsizefun cbfun )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Set callback function
    dmNativeWin.windowSizeCallback = cbfun;

    // Call the callback function to let the application know the current
    // window size
    if( cbfun )
    {
        cbfun( dmNativeWin.width, dmNativeWin.height );
    }
}

//========================================================================
// Set callback function for window close events
//========================================================================

void dmNativeSetWindowCloseCallback( Nativewindowclosefun cbfun )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Set callback function
    dmNativeWin.windowCloseCallback = cbfun;
}


//========================================================================
// Set callback function for window focus events
//========================================================================
void dmNativeSetWindowFocusCallback( Nativewindowfocusfun cbfun )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Set callback function
    dmNativeWin.windowFocusCallback = cbfun;
}

//========================================================================
// Set callback function for window iconify events
//========================================================================
void dmNativeSetWindowIconifyCallback( Nativewindowiconifyfun cbfun )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Set callback function
    dmNativeWin.windowIconifyCallback = cbfun;
}


//========================================================================
// Poll for new window and input events
//========================================================================

void dmNativePollEvents( void )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Poll for new events
    dmNativeOSPollEvents();
}


//========================================================================
// Query auxillary context valid
//========================================================================
int dmNativeQueryAuxContext()
{
    return dmNativeOSQueryAuxContext();
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
void* dmNativeAcquireAuxContext()
{
    return dmNativeOSAcquireAuxContext();
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
void dmNativeUnacquireAuxContext(void *context)
{
    dmNativeOSUnacquireAuxContext(context);
}

//========================================================================
// Set view type (gl/vulkan)
//========================================================================
void dmNativeSetViewType(int view_type)
{
    dmNativeOSSetViewType(view_type);
}

//========================================================================
// Set window background color
//========================================================================
void dmNativeSetWindowBackgroundColor(unsigned int color)
{
    dmNativeOSSetWindowBackgroundColor(color);
}

//========================================================================
// Get display scale factor
//========================================================================
float dmNativeGetDisplayScaleFactor()
{
    return dmNativeOSGetDisplayScaleFactor();
}
