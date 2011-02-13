//========================================================================
// GLFW - An OpenGL framework
// File:        window.c
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

#include "internal.h"



//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Clear all open window hints
//========================================================================

void _glfwClearWindowHints( void )
{
    _glfwWinHints.RefreshRate    = 0;
    _glfwWinHints.AccumRedBits   = 0;
    _glfwWinHints.AccumGreenBits = 0;
    _glfwWinHints.AccumBlueBits  = 0;
    _glfwWinHints.AccumAlphaBits = 0;
    _glfwWinHints.AuxBuffers     = 0;
    _glfwWinHints.Stereo         = 0;
    _glfwWinHints.WindowNoResize = 0;
    _glfwWinHints.Samples        = 0;
}


//========================================================================
// Handle the input tracking part of window deactivation
//========================================================================

void _glfwInputDeactivation( void )
{
    int i;

    // Release all keyboard keys
    for( i = 0; i <= GLFW_KEY_LAST; i ++ )
    {
        if( _glfwInput.Key[ i ] == GLFW_PRESS )
	{
	    _glfwInputKey( i, GLFW_RELEASE );
	}
    }

    // Release all mouse buttons
    for( i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i ++ )
    {
        if( _glfwInput.MouseButton[ i ] == GLFW_PRESS )
	{
	    _glfwInputMouseClick( i, GLFW_RELEASE );
	}
    }
}


//========================================================================
// _glfwClearInput() - Clear all input state
//========================================================================

void _glfwClearInput( void )
{
    int i;

    // Release all keyboard keys
    for( i = 0; i <= GLFW_KEY_LAST; i ++ )
    {
        _glfwInput.Key[ i ] = GLFW_RELEASE;
    }

    // Clear last character
    _glfwInput.LastChar = 0;

    // Release all mouse buttons
    for( i = 0; i <= GLFW_MOUSE_BUTTON_LAST; i ++ )
    {
        _glfwInput.MouseButton[ i ] = GLFW_RELEASE;
    }

    // Set mouse position to (0,0)
    _glfwInput.MousePosX = 0;
    _glfwInput.MousePosY = 0;

    // Set mouse wheel position to 0
    _glfwInput.WheelPos = 0;

    // The default is to use non sticky keys and mouse buttons
    _glfwInput.StickyKeys = GL_FALSE;
    _glfwInput.StickyMouseButtons = GL_FALSE;

    // The default is to disable key repeat
    _glfwInput.KeyRepeat = GL_FALSE;
}


//========================================================================
// _glfwInputKey() - Register keyboard activity
//========================================================================

void _glfwInputKey( int key, int action )
{
    int keyrepeat = 0;

    if( key < 0 || key > GLFW_KEY_LAST )
    {
	return;
    }

    // Are we trying to release an already released key?
    if( action == GLFW_RELEASE && _glfwInput.Key[ key ] != GLFW_PRESS )
    {
	return;
    }

    // Register key action
    if( action == GLFW_RELEASE && _glfwInput.StickyKeys )
    {
	_glfwInput.Key[ key ] = GLFW_STICK;
    }
    else
    {
	keyrepeat = (_glfwInput.Key[ key ] == GLFW_PRESS) &&
		    (action == GLFW_PRESS);
	_glfwInput.Key[ key ] = (char) action;
    }

    // Call user callback function
    if( _glfwWin.KeyCallback && (_glfwInput.KeyRepeat || !keyrepeat) )
    {
	_glfwWin.KeyCallback( key, action );
    }
}


//========================================================================
// _glfwInputChar() - Register (keyboard) character activity
//========================================================================

void _glfwInputChar( int character, int action )
{
    int keyrepeat = 0;

    // Valid Unicode (ISO 10646) character?
    if( !( (character >= 32 && character <= 126) || character >= 160 ) )
    {
        return;
    }

    // Is this a key repeat?
    if( action == GLFW_PRESS && _glfwInput.LastChar == character )
    {
        keyrepeat = 1;
    }

    // Store this character as last character (or clear it, if released)
    if( action == GLFW_PRESS )
    {
        _glfwInput.LastChar = character;
    }
    else
    {
        _glfwInput.LastChar = 0;
    }

    // Call user callback function
    if( _glfwWin.CharCallback && (_glfwInput.KeyRepeat || !keyrepeat) )
    {
        _glfwWin.CharCallback( character, action );
    }
}


//========================================================================
// _glfwInputMouseClick() - Register mouse button clicks
//========================================================================

void _glfwInputMouseClick( int button, int action )
{
    if( button >= 0 && button <= GLFW_MOUSE_BUTTON_LAST )
    {
        // Register mouse button action
        if( action == GLFW_RELEASE && _glfwInput.StickyMouseButtons )
        {
            _glfwInput.MouseButton[ button ] = GLFW_STICK;
        }
        else
        {
            _glfwInput.MouseButton[ button ] = (char) action;
        }

        // Call user callback function
        if( _glfwWin.MouseButtonCallback )
        {
            _glfwWin.MouseButtonCallback( button, action );
        }
    }
}



//************************************************************************
//****                    GLFW user functions                         ****
//************************************************************************

//========================================================================
// glfwOpenWindow() - Here is where the window is created, and the OpenGL
// rendering context is created
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwOpenWindow( int width, int height,
    int redbits, int greenbits, int bluebits, int alphabits,
    int depthbits, int stencilbits, int mode )
{
    int x;
    _GLFWhints hints;

    // Is GLFW initialized?
    if( !_glfwInitialized || _glfwWin.Opened )
    {
        return GL_FALSE;
    }

    // Copy and clear window hints
    hints = _glfwWinHints;
    _glfwClearWindowHints();

    // Check input arguments
    if( mode != GLFW_WINDOW && mode != GLFW_FULLSCREEN )
    {
        return GL_FALSE;
    }

    // Clear GLFW window state
    _glfwWin.Active            = GL_TRUE;
    _glfwWin.Iconified         = GL_FALSE;
    _glfwWin.MouseLock         = GL_FALSE;
    _glfwWin.AutoPollEvents    = GL_TRUE;
    _glfwClearInput();

    // Unregister all callback functions
    _glfwWin.WindowSizeCallback    = NULL;
    _glfwWin.WindowCloseCallback   = NULL;
    _glfwWin.WindowRefreshCallback = NULL;
    _glfwWin.KeyCallback           = NULL;
    _glfwWin.CharCallback          = NULL;
    _glfwWin.MousePosCallback      = NULL;
    _glfwWin.MouseButtonCallback   = NULL;
    _glfwWin.MouseWheelCallback    = NULL;

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
    _glfwWin.Width          = width;
    _glfwWin.Height         = height;
    _glfwWin.Fullscreen     = (mode == GLFW_FULLSCREEN ? 1 : 0);

    // Platform specific window opening routine
    if( !_glfwPlatformOpenWindow( width, height, redbits, greenbits,
            bluebits, alphabits, depthbits, stencilbits, mode, &hints ) )
    {
        return GL_FALSE;
    }

    // Flag that window is now opened
    _glfwWin.Opened = GL_TRUE;

    // Get window parameters (such as color buffer bits etc)
    _glfwPlatformRefreshWindowParams();

    // Get OpenGL version
    glfwGetGLVersion( &_glfwWin.GLVerMajor, &_glfwWin.GLVerMinor, &x );

    // Do we have non-power-of-two textures?
    _glfwWin.Has_GL_ARB_texture_non_power_of_two =
        glfwExtensionSupported( "GL_ARB_texture_non_power_of_two" );

    // Do we have automatic mipmap generation?
    _glfwWin.Has_GL_SGIS_generate_mipmap =
        (_glfwWin.GLVerMajor >= 2) || (_glfwWin.GLVerMinor >= 4) ||
        glfwExtensionSupported( "GL_SGIS_generate_mipmap" );

    // If full-screen mode was requested, disable mouse cursor
    if( mode == GLFW_FULLSCREEN )
    {
        glfwDisable( GLFW_MOUSE_CURSOR );
    }

    return GL_TRUE;
}


//========================================================================
// glfwOpenWindowHint() - Set hints for opening the window
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwOpenWindowHint( int target, int hint )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return;
    }

    switch( target )
    {
        case GLFW_REFRESH_RATE:
            _glfwWinHints.RefreshRate = hint;
            break;
        case GLFW_ACCUM_RED_BITS:
            _glfwWinHints.AccumRedBits = hint;
            break;
        case GLFW_ACCUM_GREEN_BITS:
            _glfwWinHints.AccumGreenBits = hint;
            break;
        case GLFW_ACCUM_BLUE_BITS:
            _glfwWinHints.AccumBlueBits = hint;
            break;
        case GLFW_ACCUM_ALPHA_BITS:
            _glfwWinHints.AccumAlphaBits = hint;
            break;
        case GLFW_AUX_BUFFERS:
            _glfwWinHints.AuxBuffers = hint;
            break;
        case GLFW_STEREO:
            _glfwWinHints.Stereo = hint;
            break;
        case GLFW_WINDOW_NO_RESIZE:
            _glfwWinHints.WindowNoResize = hint;
            break;
	case GLFW_FSAA_SAMPLES:
            _glfwWinHints.Samples = hint;
            break;
        default:
            break;
    }
}


//========================================================================
// glfwCloseWindow() - Properly kill the window / video display
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwCloseWindow( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return;
    }

    // Show mouse pointer again (if hidden)
    glfwEnable( GLFW_MOUSE_CURSOR );

    // Close window
    _glfwPlatformCloseWindow();

    // Window is no longer opened
    _glfwWin.Opened = GL_FALSE;
}


//========================================================================
// glfwSetWindowTitle() - Set the window title
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowTitle( const char *title )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Set window title
    _glfwPlatformSetWindowTitle( title );
}


//========================================================================
// glfwGetWindowSize() - Get the window size
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwGetWindowSize( int *width, int *height )
{
    if( width != NULL )
    {
        *width = _glfwWin.Width;
    }
    if( height != NULL )
    {
        *height = _glfwWin.Height;
    }
}


//========================================================================
// glfwSetWindowSize() - Set the window size
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowSize( int width, int height )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened || _glfwWin.Iconified )
    {
        return;
    }

    // Don't do anything if the window size did not change
    if( width == _glfwWin.Width && height == _glfwWin.Height )
    {
        return;
    }

    // Change window size
    _glfwPlatformSetWindowSize( width, height );

    // Refresh window parameters (may have changed due to changed video
    // modes)
    _glfwPlatformRefreshWindowParams();
}


//========================================================================
// glfwSetWindowPos() - Set the window position
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowPos( int x, int y )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened || _glfwWin.Fullscreen ||
        _glfwWin.Iconified )
    {
        return;
    }

    // Set window position
    _glfwPlatformSetWindowPos( x, y );
}


//========================================================================
// glfwIconfyWindow() - Window iconification
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwIconifyWindow( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened || _glfwWin.Iconified )
    {
        return;
    }

    // Iconify window
    _glfwPlatformIconifyWindow();
}


//========================================================================
// glfwRestoreWindow() - Window un-iconification
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwRestoreWindow( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened || !_glfwWin.Iconified )
    {
        return;
    }

    // Restore iconified window
    _glfwPlatformRestoreWindow();

    // Refresh window parameters
    _glfwPlatformRefreshWindowParams();
}


//========================================================================
// glfwSwapBuffers() - Swap buffers (double-buffering) and poll any new
// events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSwapBuffers( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Check for window messages
    if( _glfwWin.AutoPollEvents )
    {
        glfwPollEvents();
    }

    // Update display-buffer
    if( _glfwWin.Opened )
    {
        _glfwPlatformSwapBuffers();
    }
}


//========================================================================
// glfwSwapInterval() - Set double buffering swap interval (0 = vsync off)
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSwapInterval( int interval )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Set double buffering swap interval
    _glfwPlatformSwapInterval( interval );
}


//========================================================================
// glfwGetWindowParam() - Get window parameter
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetWindowParam( int param )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return 0;
    }

    // Is the window opened?
    if( !_glfwWin.Opened )
    {
        if( param == GLFW_OPENED )
        {
            return GL_FALSE;
        }
        return 0;
    }

    // Window parameters
    switch( param )
    {
        case GLFW_OPENED:
            return GL_TRUE;
        case GLFW_ACTIVE:
            return _glfwWin.Active;
        case GLFW_ICONIFIED:
            return _glfwWin.Iconified;
        case GLFW_ACCELERATED:
            return _glfwWin.Accelerated;
        case GLFW_RED_BITS:
            return _glfwWin.RedBits;
        case GLFW_GREEN_BITS:
            return _glfwWin.GreenBits;
        case GLFW_BLUE_BITS:
            return _glfwWin.BlueBits;
        case GLFW_ALPHA_BITS:
            return _glfwWin.AlphaBits;
        case GLFW_DEPTH_BITS:
            return _glfwWin.DepthBits;
        case GLFW_STENCIL_BITS:
            return _glfwWin.StencilBits;
        case GLFW_ACCUM_RED_BITS:
            return _glfwWin.AccumRedBits;
        case GLFW_ACCUM_GREEN_BITS:
            return _glfwWin.AccumGreenBits;
        case GLFW_ACCUM_BLUE_BITS:
            return _glfwWin.AccumBlueBits;
        case GLFW_ACCUM_ALPHA_BITS:
            return _glfwWin.AccumAlphaBits;
        case GLFW_AUX_BUFFERS:
            return _glfwWin.AuxBuffers;
        case GLFW_STEREO:
            return _glfwWin.Stereo;
        case GLFW_REFRESH_RATE:
            return _glfwWin.RefreshRate;
        case GLFW_WINDOW_NO_RESIZE:
            return _glfwWin.WindowNoResize;
	case GLFW_FSAA_SAMPLES:
	    return _glfwWin.Samples;
        default:
            return 0;
    }
}


//========================================================================
// glfwSetWindowSizeCallback() - Set callback function for window size
// changes
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowSizeCallback( GLFWwindowsizefun cbfun )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.WindowSizeCallback = cbfun;

    // Call the callback function to let the application know the current
    // window size
    if( cbfun )
    {
        cbfun( _glfwWin.Width, _glfwWin.Height );
    }
}

//========================================================================
// glfwSetWindowCloseCallback() - Set callback function for window close
// events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowCloseCallback( GLFWwindowclosefun cbfun )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.WindowCloseCallback = cbfun;
}


//========================================================================
// glfwSetWindowRefreshCallback() - Set callback function for window
// refresh events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowRefreshCallback( GLFWwindowrefreshfun cbfun )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.WindowRefreshCallback = cbfun;
}


//========================================================================
// glfwPollEvents() - Poll for new window and input events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwPollEvents( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Poll for new events
    _glfwPlatformPollEvents();
}


//========================================================================
// glfwWaitEvents() - Wait for new window and input events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwWaitEvents( void )
{
    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.Opened )
    {
        return;
    }

    // Poll for new events
    _glfwPlatformWaitEvents();
}

