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


//========================================================================
// Return key state
//========================================================================

int dmNativeGetKey( int key )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return NATIVE_RELEASE;
    }

    // Is it a valid key?
    if( key < 0 || key > NATIVE_KEY_LAST )
    {
        return NATIVE_RELEASE;
    }

    if( dmNativeInput.Key[ key ] == NATIVE_STICK )
    {
        // Sticky mode: release key now
        dmNativeInput.Key[ key ] = NATIVE_RELEASE;
        return NATIVE_PRESS;
    }

    return (int) dmNativeInput.Key[ key ];
}


//========================================================================
// Return mouse button state
//========================================================================

int dmNativeGetMouseButton( int button )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return NATIVE_RELEASE;
    }

    // Is it a valid mouse button?
    if( button < 0 || button > NATIVE_MOUSE_BUTTON_LAST )
    {
        return NATIVE_RELEASE;
    }

    if( dmNativeInput.MouseButton[ button ] == NATIVE_STICK || dmNativeInput.MouseButton[ button ] == NATIVE_CLICKED )
    {
        // Sticky mode: release mouse button now
        dmNativeInput.MouseButton[ button ] = NATIVE_RELEASE;
        return NATIVE_PRESS;
    }

    return (int) dmNativeInput.MouseButton[ button ];
}

int dmNativeIsMouseLeftButtonFromTouch( void )
{
    return dmNativeInput.MouseLeftButtonFromTouch;
}

int dmNativeIsMousePositionFromTouch( void )
{
    return dmNativeInput.MousePositionFromTouch;
}


//========================================================================
// Return mouse cursor position
//========================================================================

void dmNativeGetMousePos( int *xpos, int *ypos )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return;
    }

    // Return mouse position
    if( xpos != NULL )
    {
        *xpos = dmNativeInput.MousePosX;
    }
    if( ypos != NULL )
    {
        *ypos = dmNativeInput.MousePosY;
    }
}


//========================================================================
// Return mouse wheel position
//========================================================================

int dmNativeGetMouseWheel( void )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return 0;
    }

    // Return mouse wheel position
    return dmNativeInput.WheelPos;
}


//========================================================================
// Set callback function for keyboard input
// Returns 1 on success, 0 if Native is not initialised or not window open.
//========================================================================


//========================================================================
// Set callback function for character input
// Returns 1 on success, 0 if Native is not initialised or not window open.
//========================================================================

int dmNativeSetCharCallback( Nativecharfun cbfun )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return 0;
    }

    // Set callback function
    dmNativeWin.charCallback = cbfun;
    return 1;
}

//========================================================================
// Set callback function for uncommitted/marked text input
// Returns 1 on success, 0 if Native is not initialised or not window open.
//========================================================================

int dmNativeSetMarkedTextCallback( Nativemarkedtextfun cbfun )
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return 0;
    }

    // Set callback function
    dmNativeWin.markedTextCallback = cbfun;
    return 1;
}


//========================================================================
// Set callback function for mouse clicks
// Returns 1 on success, 0 if Native is not initialised or not window open.
//========================================================================


//========================================================================
// Set callback function for mouse moves
// Returns 1 on success, 0 if Native is not initialised or not window open.
//========================================================================


//========================================================================
// Set callback function for mouse wheel
// Returns 1 on success, 0 if Native is not initialised or not window open.
//========================================================================


int dmNativeGetAcceleration(float* x, float* y, float* z)
{
    return dmNativeOSGetAcceleration(x, y, z);
}

#if 0 // DEBUG
const char* PhaseToStr(int phase)
{
    switch (phase) {
        case NATIVE_PHASE_BEGAN: return "BEGAN";
        case NATIVE_PHASE_MOVED: return "MOVED";
        case NATIVE_PHASE_STATIONARY: return "STATIONARY";
        case NATIVE_PHASE_ENDED: return "ENDED";
        case NATIVE_PHASE_CANCELLED: return "CANCELLED";
        case NATIVE_PHASE_TAPPED: return "TAPPED";
        case NATIVE_PHASE_IDLE: return "IDLE";
        default:
            return "Unknown";
    }
}
#endif



int dmNativeGetTouch(NativeTouch* touch, int count, int* out_count)
{
    *out_count = dmNativeReadTouches(dmNativeInput.Touch, touch, count, 1);
    return 1;
}

int dmNativeSetGamepadCallback(Nativekeyfun cbfun)
{
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return 0;
    }

    // Set callback function
    dmNativeWin.gamepadCallback = cbfun;
    return 1;
}

// DEFOLD change (for win32 only)
int  dmNativeSetDeviceChangedCallback( Nativedevicechangedfun cbfun )
{
#if defined(_WIN32)
    if( !dmNativeInitialized || !dmNativeWin.opened )
    {
        return 0;
    }
    dmNativeWin.deviceChangeCallback = cbfun;
#endif
    return 1;
}

// DEFOLD change
// Since the locking of the mouse is done internally in dmNative,
// we need to query the current state here
int dmNativeGetMouseLocked()
{
    return dmNativeWin.mouseLock;
}
