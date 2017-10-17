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

#include "internal.h"

#include <limits.h>


#ifndef GL_VERSION_3_2

#define GL_CONTEXT_CORE_PROFILE_BIT       0x00000001
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT 0x00000002
#define GL_CONTEXT_PROFILE_MASK           0x9126

#endif /*GL_VERSION_3_2*/



//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

static int Max(int a, int b)
{
    return (a > b) ? a : b;
}

//========================================================================
// Clear all open window hints
//========================================================================

void _glfwClearWindowHints( void )
{
    memset( &_glfwLibrary.hints, 0, sizeof( _glfwLibrary.hints ) );
    _glfwLibrary.hints.glMajor = 1;
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
// Clear all input state
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

    for (i = 0; i < GLFW_MAX_TOUCH; ++i) {
        memset(&_glfwInput.Touch[i], 0, sizeof(_glfwInput.Touch[i]));
        _glfwInput.Touch[i].Id = i;
    }

    // The default is to disable key repeat
    _glfwInput.KeyRepeat = GL_FALSE;
}


//========================================================================
// Register keyboard activity
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
    if( _glfwWin.keyCallback && (_glfwInput.KeyRepeat || !keyrepeat) )
    {
        _glfwWin.keyCallback( key, action );
    }
}


//========================================================================
// Register (keyboard) character activity
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

    if( action != GLFW_PRESS )
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

    if( _glfwWin.charCallback && (_glfwInput.KeyRepeat || !keyrepeat) )
    {
        _glfwWin.charCallback( character, action );
    }
}


//========================================================================
// Register unfinished (marked) keyboard input
//========================================================================

void _glfwSetMarkedText( char* text )
{
    if( _glfwWin.markedTextCallback )
    {
        _glfwWin.markedTextCallback( text );
    }
}


//========================================================================
// Register mouse button clicks
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
        if( _glfwWin.mouseButtonCallback )
        {
            _glfwWin.mouseButtonCallback( button, action );
        }
    }
}


//========================================================================
// Return the available framebuffer config closest to the desired values
// This is based on the manual GLX Visual selection from 2.6
//========================================================================

const _GLFWfbconfig *_glfwChooseFBConfig( const _GLFWfbconfig *desired,
                                          const _GLFWfbconfig *alternatives,
                                          unsigned int count )
{
    unsigned int i;
    unsigned int missing, leastMissing = UINT_MAX;
    unsigned int colorDiff, leastColorDiff = UINT_MAX;
    unsigned int extraDiff, leastExtraDiff = UINT_MAX;
    const _GLFWfbconfig *current;
    const _GLFWfbconfig *closest = NULL;

    for( i = 0;  i < count;  i++ )
    {
        current = alternatives + i;

        if( desired->stereo > 0 && current->stereo == 0 )
        {
            // Stereo is a hard constraint
            continue;
        }

        // Count number of missing buffers
        {
            missing = 0;

            if( desired->alphaBits > 0 && current->alphaBits == 0 )
            {
                missing++;
            }

            if( desired->depthBits > 0 && current->depthBits == 0 )
            {
                missing++;
            }

            if( desired->stencilBits > 0 && current->stencilBits == 0 )
            {
                missing++;
            }

            if( desired->auxBuffers > 0 && current->auxBuffers < desired->auxBuffers )
            {
                missing += desired->auxBuffers - current->auxBuffers;
            }

            if( desired->samples > 0 && current->samples == 0 )
            {
                // Technically, several multisampling buffers could be
                // involved, but that's a lower level implementation detail and
                // not important to us here, so we count them as one
                missing++;
            }
        }

        // These polynomials make many small channel size differences matter
        // less than one large channel size difference

        // Calculate color channel size difference value
        {
            colorDiff = 0;

            if ( desired->redBits > 0 )
            {
                colorDiff += ( desired->redBits - current->redBits ) *
                             ( desired->redBits - current->redBits );
            }

            if ( desired->greenBits > 0 )
            {
                colorDiff += ( desired->greenBits - current->greenBits ) *
                             ( desired->greenBits - current->greenBits );
            }

            if ( desired->blueBits > 0 )
            {
                colorDiff += ( desired->blueBits - current->blueBits ) *
                             ( desired->blueBits - current->blueBits );
            }
        }

        // Calculate non-color channel size difference value
        {
            extraDiff = 0;

            if( desired->alphaBits > 0 )
            {
                extraDiff += ( desired->alphaBits - current->alphaBits ) *
                             ( desired->alphaBits - current->alphaBits );
            }

            if( desired->depthBits > 0 )
            {
                extraDiff += ( desired->depthBits - current->depthBits ) *
                             ( desired->depthBits - current->depthBits );
            }

            if( desired->stencilBits > 0 )
            {
                extraDiff += ( desired->stencilBits - current->stencilBits ) *
                             ( desired->stencilBits - current->stencilBits );
            }

            if( desired->accumRedBits > 0 )
            {
                extraDiff += ( desired->accumRedBits - current->accumRedBits ) *
                             ( desired->accumRedBits - current->accumRedBits );
            }

            if( desired->accumGreenBits > 0 )
            {
                extraDiff += ( desired->accumGreenBits - current->accumGreenBits ) *
                             ( desired->accumGreenBits - current->accumGreenBits );
            }

            if( desired->accumBlueBits > 0 )
            {
                extraDiff += ( desired->accumBlueBits - current->accumBlueBits ) *
                             ( desired->accumBlueBits - current->accumBlueBits );
            }

            if( desired->accumAlphaBits > 0 )
            {
                extraDiff += ( desired->accumAlphaBits - current->accumAlphaBits ) *
                             ( desired->accumAlphaBits - current->accumAlphaBits );
            }

            if( desired->samples > 0 )
            {
                extraDiff += ( desired->samples - current->samples ) *
                             ( desired->samples - current->samples );
            }
        }

        // Figure out if the current one is better than the best one found so far

        if( missing < leastMissing )
        {
            closest = current;
        }
        else if( missing == leastMissing )
        {
            if( ( colorDiff < leastColorDiff ) ||
                ( colorDiff == leastColorDiff && extraDiff < leastExtraDiff ) )
            {
                closest = current;
            }
        }

        if( current == closest )
        {
            leastMissing = missing;
            leastColorDiff = colorDiff;
            leastExtraDiff = extraDiff;
        }
    }

    return closest;
}


//************************************************************************
//****                    GLFW user functions                         ****
//************************************************************************

//========================================================================
// Create the GLFW window and its associated context
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwOpenWindow( int width, int height,
    int redbits, int greenbits, int bluebits, int alphabits,
    int depthbits, int stencilbits, int mode )
{
    _GLFWfbconfig fbconfig;
    _GLFWwndconfig wndconfig;

    if( !_glfwInitialized || _glfwWin.opened )
    {
        return GL_FALSE;
    }

    // Set up desired framebuffer config
    fbconfig.redBits        = Max( redbits, 0 );
    fbconfig.greenBits      = Max( greenbits, 0 );
    fbconfig.blueBits       = Max( bluebits, 0 );
    fbconfig.alphaBits      = Max( alphabits, 0 );
    fbconfig.depthBits      = Max( depthbits, 0 );
    fbconfig.stencilBits    = Max( stencilbits, 0 );
    fbconfig.accumRedBits   = Max( _glfwLibrary.hints.accumRedBits, 0 );
    fbconfig.accumGreenBits = Max( _glfwLibrary.hints.accumGreenBits, 0 );
    fbconfig.accumBlueBits  = Max( _glfwLibrary.hints.accumBlueBits, 0 );
    fbconfig.accumAlphaBits = Max( _glfwLibrary.hints.accumAlphaBits, 0 );
    fbconfig.auxBuffers     = Max( _glfwLibrary.hints.auxBuffers, 0 );
    fbconfig.stereo         = _glfwLibrary.hints.stereo ? GL_TRUE : GL_FALSE;
    fbconfig.samples        = Max( _glfwLibrary.hints.samples, 0 );

    // Set up desired window config
    wndconfig.mode           = mode;
    wndconfig.refreshRate    = Max( _glfwLibrary.hints.refreshRate, 0 );
    wndconfig.windowNoResize = _glfwLibrary.hints.windowNoResize ? GL_TRUE : GL_FALSE;
    wndconfig.glMajor        = Max( _glfwLibrary.hints.glMajor, 1 );
    wndconfig.glMinor        = Max( _glfwLibrary.hints.glMinor, 0 );
    wndconfig.glForward      = _glfwLibrary.hints.glForward ? GL_TRUE : GL_FALSE;
    wndconfig.glDebug        = _glfwLibrary.hints.glDebug ? GL_TRUE : GL_FALSE;
    wndconfig.glProfile      = _glfwLibrary.hints.glProfile;
    wndconfig.highDPI        = _glfwLibrary.hints.highDPI;

    if( wndconfig.glMajor == 1 && wndconfig.glMinor > 5 )
    {
        // OpenGL 1.x series ended with version 1.5
        return GL_FALSE;
    }
    else if( wndconfig.glMajor == 2 && wndconfig.glMinor > 1 )
    {
        // OpenGL 2.x series ended with version 2.1
        return GL_FALSE;
    }
    else if( wndconfig.glMajor == 3 && wndconfig.glMinor > 3 )
    {
        // OpenGL 3.x series ended with version 3.3
        return GL_FALSE;
    }
    else
    {
        // For now, let everything else through
    }

    if( wndconfig.glProfile &&
        ( wndconfig.glMajor < 3 || ( wndconfig.glMajor == 3 && wndconfig.glMinor < 2 ) ) )
    {
        // Context profiles are only defined for OpenGL version 3.2 and above
        return GL_FALSE;
    }

    if( wndconfig.glForward && wndconfig.glMajor < 3 )
    {
        // Forward-compatible contexts are only defined for OpenGL version 3.0 and above
        return GL_FALSE;
    }

    // Clear for next open call
    _glfwClearWindowHints();

    // Check input arguments
    if( mode != GLFW_WINDOW && mode != GLFW_FULLSCREEN )
    {
        return GL_FALSE;
    }

    // Clear GLFW window state
    _glfwWin.active         = GL_TRUE;
    _glfwWin.iconified      = GL_FALSE;
    _glfwWin.mouseLock      = GL_FALSE;
    _glfwWin.autoPollEvents = GL_TRUE;
    _glfwClearInput();

    // Unregister all callback functions
    _glfwWin.windowSizeCallback    = NULL;
    _glfwWin.windowCloseCallback   = NULL;
    _glfwWin.windowRefreshCallback = NULL;
    _glfwWin.keyCallback           = NULL;
    _glfwWin.charCallback          = NULL;
    _glfwWin.markedTextCallback    = NULL;
    _glfwWin.mousePosCallback      = NULL;
    _glfwWin.mouseButtonCallback   = NULL;
    _glfwWin.mouseWheelCallback    = NULL;

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
    _glfwWin.width      = width;
    _glfwWin.height     = height;
    _glfwWin.fullscreen = (mode == GLFW_FULLSCREEN ? GL_TRUE : GL_FALSE);

    // Platform specific window opening routine
    if( !_glfwPlatformOpenWindow( width, height, &wndconfig, &fbconfig ) )
    {
        glfwCloseWindow();
        return GL_FALSE;
    }

    // Flag that window is now opened
    _glfwWin.opened = GL_TRUE;

    // Read back window and context parameters
    _glfwPlatformRefreshWindowParams();
    _glfwRefreshContextParams();

    if( _glfwWin.glMajor < wndconfig.glMajor ||
        ( _glfwWin.glMajor == wndconfig.glMajor &&
          _glfwWin.glMinor < wndconfig.glMinor ) )
    {
        glfwCloseWindow();
        return GL_FALSE;
    }

    // Do we have non-power-of-two textures (added to core in version 2.0)?
    _glfwWin.has_GL_ARB_texture_non_power_of_two =
        ( _glfwWin.glMajor >= 2 ) ||
        glfwExtensionSupported( "GL_ARB_texture_non_power_of_two" );

    // Do we have automatic mipmap generation (added to core in version 1.4)?
    _glfwWin.has_GL_SGIS_generate_mipmap =
        ( _glfwWin.glMajor >= 2 ) || ( _glfwWin.glMinor >= 4 ) ||
        glfwExtensionSupported( "GL_SGIS_generate_mipmap" );

    //
    // The following check for glGetString(i) is a modification to improve compatibility
    // with Linux. We have encountered cases where GL_VERSION is 3.0 or higher but where
    // the function glGetStringi could not be found. In these cases we fallback to the
    // deprecated function glGetString. This should not change the control-flow for any
    // other platform.
    //
    // 2016-04-29
    // Jakob Pogulis <jakob.pogulis@king.com>
    // Ragnar Svensson <ragnar.svensson@king.com>
    //
    _glfwWin.GetStringi = NULL;
    if (_glfwWin.glMajor > 2) {
        _glfwWin.GetStringi = (PFNGLGETSTRINGIPROC) glfwGetProcAddress("glGetStringi");
        if (!_glfwWin.GetStringi) {
            if (glfwGetProcAddress("glGetString") == NULL) {
                glfwCloseWindow();
                return GL_FALSE;
            }
        }
    }

    // If full-screen mode was requested, disable mouse cursor
    if( mode == GLFW_FULLSCREEN )
    {
        glfwDisable( GLFW_MOUSE_CURSOR );
    }

    // DEFOLD
    // NOTE: Clear and swap was removed by Defold. It should be up to the application
    // when to clear the buffers

    // Start by clearing the front buffer to black (avoid ugly desktop
    // remains in our OpenGL window)
    // glClear( GL_COLOR_BUFFER_BIT );
    //_glfwPlatformSwapBuffers();

    return GL_TRUE;
}


//========================================================================
// Set hints for opening the window
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwOpenWindowHint( int target, int hint )
{
    if( !_glfwInitialized )
    {
        return;
    }

    switch( target )
    {
        case GLFW_REFRESH_RATE:
            _glfwLibrary.hints.refreshRate = hint;
            break;
        case GLFW_ACCUM_RED_BITS:
            _glfwLibrary.hints.accumRedBits = hint;
            break;
        case GLFW_ACCUM_GREEN_BITS:
            _glfwLibrary.hints.accumGreenBits = hint;
            break;
        case GLFW_ACCUM_BLUE_BITS:
            _glfwLibrary.hints.accumBlueBits = hint;
            break;
        case GLFW_ACCUM_ALPHA_BITS:
            _glfwLibrary.hints.accumAlphaBits = hint;
            break;
        case GLFW_AUX_BUFFERS:
            _glfwLibrary.hints.auxBuffers = hint;
            break;
        case GLFW_STEREO:
            _glfwLibrary.hints.stereo = hint;
            break;
        case GLFW_WINDOW_NO_RESIZE:
            _glfwLibrary.hints.windowNoResize = hint;
            break;
        case GLFW_FSAA_SAMPLES:
            _glfwLibrary.hints.samples = hint;
            break;
        case GLFW_OPENGL_VERSION_MAJOR:
            _glfwLibrary.hints.glMajor = hint;
            break;
        case GLFW_OPENGL_VERSION_MINOR:
            _glfwLibrary.hints.glMinor = hint;
            break;
        case GLFW_OPENGL_FORWARD_COMPAT:
            _glfwLibrary.hints.glForward = hint;
            break;
        case GLFW_OPENGL_DEBUG_CONTEXT:
            _glfwLibrary.hints.glDebug = hint;
            break;
        case GLFW_OPENGL_PROFILE:
            _glfwLibrary.hints.glProfile = hint;
            break;
        case GLFW_WINDOW_HIGH_DPI:
            _glfwLibrary.hints.highDPI = hint;
            break;
        default:
            break;
    }
}


//========================================================================
// Properly kill the window / video display
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwCloseWindow( void )
{
    if( !_glfwInitialized )
    {
        return;
    }

    // Show mouse pointer again (if hidden)
    glfwEnable( GLFW_MOUSE_CURSOR );

    _glfwPlatformCloseWindow();

    memset( &_glfwWin, 0, sizeof(_glfwWin) );
}

GLFWAPI int GLFWAPIENTRY glfwGetDefaultFramebuffer( void )
{
    return _glfwPlatformGetDefaultFramebuffer();
}

//========================================================================
// Set the window title
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowTitle( const char *title )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Set window title
    _glfwPlatformSetWindowTitle( title );
}


//========================================================================
// Get the window size
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwGetWindowSize( int *width, int *height )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    if( width != NULL )
    {
        *width = _glfwWin.width;
    }
    if( height != NULL )
    {
        *height = _glfwWin.height;
    }
}


//========================================================================
// Set the window size
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowSize( int width, int height )
{
    if( !_glfwInitialized || !_glfwWin.opened || _glfwWin.iconified )
    {
        return;
    }

    // Don't do anything if the window size did not change
    if( width == _glfwWin.width && height == _glfwWin.height )
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
// Set the window position
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowPos( int x, int y )
{
    if( !_glfwInitialized || !_glfwWin.opened || _glfwWin.fullscreen ||
        _glfwWin.iconified )
    {
        return;
    }

    // Set window position
    _glfwPlatformSetWindowPos( x, y );
}


//========================================================================
// Window iconification
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwIconifyWindow( void )
{
    if( !_glfwInitialized || !_glfwWin.opened || _glfwWin.iconified )
    {
        return;
    }

    // Iconify window
    _glfwPlatformIconifyWindow();
}


//========================================================================
// Window un-iconification
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwRestoreWindow( void )
{
    if( !_glfwInitialized || !_glfwWin.opened || !_glfwWin.iconified )
    {
        return;
    }

    // Restore iconified window
    _glfwPlatformRestoreWindow();

    // Refresh window parameters
    _glfwPlatformRefreshWindowParams();
}


//========================================================================
// Swap buffers (double-buffering) and poll any new events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSwapBuffers( void )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    _glfwPlatformSwapBuffers();

    // Check for window messages
    if( _glfwWin.autoPollEvents )
    {
        glfwPollEvents();
    }
}


//========================================================================
// Set double buffering swap interval (0 = vsync off)
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSwapInterval( int interval )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Set double buffering swap interval
    _glfwPlatformSwapInterval( interval );
}


//========================================================================
// Get window parameter
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwGetWindowParam( int param )
{
    if( !_glfwInitialized )
    {
        return 0;
    }

    // Is the window opened?
    if( !_glfwWin.opened )
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
            return _glfwWin.active;
        case GLFW_ICONIFIED:
            return _glfwWin.iconified;
        case GLFW_ACCELERATED:
            return _glfwWin.accelerated;
        case GLFW_RED_BITS:
            return _glfwWin.redBits;
        case GLFW_GREEN_BITS:
            return _glfwWin.greenBits;
        case GLFW_BLUE_BITS:
            return _glfwWin.blueBits;
        case GLFW_ALPHA_BITS:
            return _glfwWin.alphaBits;
        case GLFW_DEPTH_BITS:
            return _glfwWin.depthBits;
        case GLFW_STENCIL_BITS:
            return _glfwWin.stencilBits;
        case GLFW_ACCUM_RED_BITS:
            return _glfwWin.accumRedBits;
        case GLFW_ACCUM_GREEN_BITS:
            return _glfwWin.accumGreenBits;
        case GLFW_ACCUM_BLUE_BITS:
            return _glfwWin.accumBlueBits;
        case GLFW_ACCUM_ALPHA_BITS:
            return _glfwWin.accumAlphaBits;
        case GLFW_AUX_BUFFERS:
            return _glfwWin.auxBuffers;
        case GLFW_STEREO:
            return _glfwWin.stereo;
        case GLFW_REFRESH_RATE:
            return _glfwWin.refreshRate;
        case GLFW_WINDOW_NO_RESIZE:
            return _glfwWin.windowNoResize;
        case GLFW_FSAA_SAMPLES:
            return _glfwWin.samples;
        case GLFW_OPENGL_VERSION_MAJOR:
            return _glfwWin.glMajor;
        case GLFW_OPENGL_VERSION_MINOR:
            return _glfwWin.glMinor;
        case GLFW_OPENGL_FORWARD_COMPAT:
            return _glfwWin.glForward;
        case GLFW_OPENGL_DEBUG_CONTEXT:
            return _glfwWin.glDebug;
        case GLFW_OPENGL_PROFILE:
            return _glfwWin.glProfile;
        case GLFW_WINDOW_HIGH_DPI:
            return _glfwWin.highDPI;
        default:
            return 0;
    }
}


//========================================================================
// Set callback function for window size changes
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowSizeCallback( GLFWwindowsizefun cbfun )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.windowSizeCallback = cbfun;

    // Call the callback function to let the application know the current
    // window size
    if( cbfun )
    {
        cbfun( _glfwWin.width, _glfwWin.height );
    }
}

//========================================================================
// Set callback function for window close events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowCloseCallback( GLFWwindowclosefun cbfun )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.windowCloseCallback = cbfun;
}


//========================================================================
// Set callback function for window refresh events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwSetWindowRefreshCallback( GLFWwindowrefreshfun cbfun )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.windowRefreshCallback = cbfun;
}

//========================================================================
// Set callback function for window focus events
//========================================================================
GLFWAPI void GLFWAPIENTRY glfwSetWindowFocusCallback( GLFWwindowfocusfun cbfun )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Set callback function
    _glfwWin.windowFocusCallback = cbfun;
}


//========================================================================
// Poll for new window and input events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwPollEvents( void )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Poll for new events
    _glfwPlatformPollEvents();
}


//========================================================================
// Wait for new window and input events
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwWaitEvents( void )
{
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return;
    }

    // Poll for new events
    _glfwPlatformWaitEvents();
}


//========================================================================
// Query auxillary context valid
//========================================================================
GLFWAPI int GLFWAPIENTRY glfwQueryAuxContext()
{
    return _glfwPlatformQueryAuxContext();
}

//========================================================================
// Acquire auxillary context for current thread
//========================================================================
GLFWAPI void* GLFWAPIENTRY glfwAcquireAuxContext()
{
    return _glfwPlatformAcquireAuxContext();
}

//========================================================================
// Unacquire auxillary context for current thread
//========================================================================
GLFWAPI void GLFWAPIENTRY glfwUnacquireAuxContext(void *context)
{
    _glfwPlatformUnacquireAuxContext(context);
}


GLFWAPI void glfwAccelerometerEnable()
{
    
}
