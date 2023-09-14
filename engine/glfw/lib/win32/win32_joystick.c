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

#define GLFW_XINPUT 1

#if GLFW_XINPUT

// NOTE: We put this information here instead of in _glfwInput as
// _glfwInput is cleared when opening a new window
int g_ControllerPresent[GLFW_MAX_XINPUT_CONTROLLERS] = { 0 };
int g_ControllerPresent_prev[GLFW_MAX_XINPUT_CONTROLLERS] = { 0 };

void _update_joystick(int joy)
{
    DWORD dwResult;
    XINPUT_STATE state;

    dwResult = XInputGetState(joy, &state);
    g_ControllerPresent[joy] = dwResult == ERROR_SUCCESS;

    int state_now = g_ControllerPresent[joy];
    int state_prev = g_ControllerPresent_prev[joy];

    if (state_now != state_prev)
    {
        _glfwWin.gamepadCallback(joy, g_ControllerPresent[joy]);
        g_ControllerPresent_prev[joy] = g_ControllerPresent[joy];
    }
}

void _glfwPlatformDiscoverJoysticks()
{
    int i;
    DWORD dwResult;
    XINPUT_STATE state;

    for (i = 0; i < GLFW_MAX_XINPUT_CONTROLLERS; i++)
    {
        dwResult = XInputGetState(i, &state);
        g_ControllerPresent_prev[i] = g_ControllerPresent[i];
        g_ControllerPresent[i] = dwResult == ERROR_SUCCESS;
    }
}

static int _glfwJoystickPresent( int joy )
{
    if (joy >= 0 && joy < GLFW_MAX_XINPUT_CONTROLLERS && g_ControllerPresent[joy])
    {
        return GL_TRUE;
    }
    return GL_FALSE;
}

int _glfwPlatformGetJoystickParam(int joy, int param)
{
    if (joy < 0 || joy >= GLFW_MAX_XINPUT_CONTROLLERS)
    {
        return 0;
    }

    if( param == GLFW_PRESENT )
    {
        _update_joystick(joy);
    }

    // Is joystick present?
    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    // We got this far, the joystick is present
    if( param == GLFW_PRESENT )
    {
        return GL_TRUE;
    }

    switch( param )
    {
    case GLFW_AXES:
        // Return number of joystick axes
        return 6;

    case GLFW_BUTTONS:
        // Return number of joystick buttons
        // NOTE: We fake 16 buttons. The actual number is 14 but the button-mask is sparse in XInput. bit 0-9 + bit 12-15
        return 16;
    case GLFW_HATS:
        // Return number of hats
        return 1;
    default:
        break;
    }

    return 0;
}

int _glfwPlatformGetJoystickPos(int joy, float *pos, int numaxes)
{
    int axis;
    DWORD dwResult;
    XINPUT_STATE state;

    if (!_glfwJoystickPresent(joy))
    {
        return 0;
    }

    axis = 0;
    dwResult = XInputGetState(joy, &state);
    if (dwResult != ERROR_SUCCESS)
    {
        return 0;
    }

#define AXIS_VAL(x) 2.0f * ((x) + 32768) / 65535.0f - 1.0f;
#define TRIGGER_VAL(x) ((x)/ 255.0f);

    if (axis < numaxes)
    {
        pos[axis++] = AXIS_VAL(state.Gamepad.sThumbLX);
    }
    if (axis < numaxes)
    {
        pos[axis++] = AXIS_VAL(state.Gamepad.sThumbLY);
    }
    if (axis < numaxes)
    {
        pos[axis++] = AXIS_VAL(state.Gamepad.sThumbRX);
    }
    if (axis < numaxes)
    {
        pos[axis++] = AXIS_VAL(state.Gamepad.sThumbRY);
    }
    if (axis < numaxes)
    {
        pos[axis++] = TRIGGER_VAL(state.Gamepad.bLeftTrigger);
    }
    if (axis < numaxes)
    {
        pos[axis++] = TRIGGER_VAL(state.Gamepad.bRightTrigger);
    }

#undef AXIS_VAL
#undef TRIGGER_VAL
    return axis;
}

//========================================================================
// Get joystick button states
//========================================================================

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons,
    int numbuttons )
{
    int       button;
    DWORD dwResult;
    XINPUT_STATE state;

    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    dwResult = XInputGetState( joy, &state );
    if(dwResult != ERROR_SUCCESS)
    {
        return 0;
    }
    button = 0;
    while( button < numbuttons && button < 16 )
    {
        buttons[ button ] = (unsigned char)
            (state.Gamepad.wButtons & (1UL << button) ? GLFW_PRESS : GLFW_RELEASE);
        button++;
    }

    return button;
}

//========================================================================
// _glfwPlatformGetJoystickDeviceId() - Get joystick device id
//========================================================================

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    static char name[] = "XBox 360 Controller";

    if( !_glfwJoystickPresent( joy ) )
    {
        return GL_FALSE;
    }

    *device_id = (char*) name;
    return GL_TRUE;
}

int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    DWORD result;
    XINPUT_STATE state;

    if( !_glfwJoystickPresent(joy) )
    {
        return 0;
    }

    result = XInputGetState(joy, &state);

    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
        hats[0] |= GLFW_HAT_UP;
    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
        hats[0] |= GLFW_HAT_RIGHT;
    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
        hats[0] |= GLFW_HAT_DOWN;
    if (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
        hats[0] |= GLFW_HAT_LEFT;

    return 1;
}

#else

//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Return GL_TRUE if joystick is present, else return GL_FALSE.
//========================================================================

static int _glfwJoystickPresent( int joy )
{
    JOYINFO ji;

    // Windows NT 4.0 MMSYSTEM only supports 2 sticks (other Windows
    // versions support 16 sticks)
    if( _glfwLibrary.Sys.winVer == _GLFW_WIN_NT4 && joy > GLFW_JOYSTICK_2 )
    {
        return GL_FALSE;
    }

    // Is it a valid stick ID (Windows don't support more than 16 sticks)?
    if( joy < GLFW_JOYSTICK_1 || joy > GLFW_JOYSTICK_16 )
    {
        return GL_FALSE;
    }

    // Is the joystick present?
    if( _glfw_joyGetPos( joy - GLFW_JOYSTICK_1, &ji ) != JOYERR_NOERROR )
    {
        return GL_FALSE;
    }

    return GL_TRUE;
}


//========================================================================
// Calculate joystick position
//========================================================================

static float _glfwCalcJoystickPos( DWORD pos, DWORD min, DWORD max )
{
    float fpos = (float) pos;
    float fmin = (float) min;
    float fmax = (float) max;
    return (2.0f*(fpos - fmin) / (fmax - fmin)) - 1.0f;
}



//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Determine joystick capabilities
//========================================================================

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    JOYCAPS jc;

    // Is joystick present?
    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    // We got this far, the joystick is present
    if( param == GLFW_PRESENT )
    {
        return GL_TRUE;
    }

    // Get joystick capabilities
    _glfw_joyGetDevCaps( joy - GLFW_JOYSTICK_1, &jc, sizeof(JOYCAPS) );

    switch( param )
    {
    case GLFW_AXES:
        // Return number of joystick axes
        return jc.wNumAxes;

    case GLFW_BUTTONS:
        // Return number of joystick axes
        return jc.wNumButtons;

    default:
        break;
    }

    return 0;
}


//========================================================================
// Get joystick axis positions
//========================================================================

int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
    JOYCAPS   jc;
    JOYINFOEX ji;
    int       axis;

    // Is joystick present?
    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    // Get joystick capabilities
    _glfw_joyGetDevCaps( joy - GLFW_JOYSTICK_1, &jc, sizeof(JOYCAPS) );

    // Get joystick state
    ji.dwSize = sizeof( JOYINFOEX );
    ji.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ |
                 JOY_RETURNR | JOY_RETURNU | JOY_RETURNV;
    _glfw_joyGetPosEx( joy - GLFW_JOYSTICK_1, &ji );

    // Get position values for all axes
    axis = 0;
    if( axis < numaxes )
    {
        pos[ axis++ ] = _glfwCalcJoystickPos( ji.dwXpos, jc.wXmin,
                                              jc.wXmax );
    }
    if( axis < numaxes )
    {
        pos[ axis++ ] = -_glfwCalcJoystickPos( ji.dwYpos, jc.wYmin,
                                               jc.wYmax );
    }
    if( axis < numaxes && jc.wCaps & JOYCAPS_HASZ )
    {
        pos[ axis++ ] = _glfwCalcJoystickPos( ji.dwZpos, jc.wZmin,
                                              jc.wZmax );
    }
    if( axis < numaxes && jc.wCaps & JOYCAPS_HASR )
    {
        pos[ axis++ ] = _glfwCalcJoystickPos( ji.dwRpos, jc.wRmin,
                                              jc.wRmax );
    }
    if( axis < numaxes && jc.wCaps & JOYCAPS_HASU )
    {
        pos[ axis++ ] = _glfwCalcJoystickPos( ji.dwUpos, jc.wUmin,
                                              jc.wUmax );
    }
    if( axis < numaxes && jc.wCaps & JOYCAPS_HASV )
    {
        pos[ axis++ ] = -_glfwCalcJoystickPos( ji.dwVpos, jc.wVmin,
                                               jc.wVmax );
    }

    // Return number of returned axes
    return axis;
}


//========================================================================
// Get joystick button states
//========================================================================

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons,
    int numbuttons )
{
    JOYCAPS   jc;
    JOYINFOEX ji;
    int       button;

//  return 0;

    // Is joystick present?
    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    // Get joystick capabilities
    _glfw_joyGetDevCaps( joy - GLFW_JOYSTICK_1, &jc, sizeof(JOYCAPS) );

    // Get joystick state
    ji.dwSize = sizeof( JOYINFOEX );
    ji.dwFlags = JOY_RETURNBUTTONS;
    _glfw_joyGetPosEx( joy - GLFW_JOYSTICK_1, &ji );

    // Get states of all requested buttons
    button = 0;
    while( button < numbuttons && button < (int) jc.wNumButtons )
    {
        buttons[ button ] = (unsigned char)
            (ji.dwButtons & (1UL << button) ? GLFW_PRESS : GLFW_RELEASE);
        button ++;
    }

    return button;
}

//========================================================================
// _glfwPlatformGetJoystickDeviceId() - Get joystick device id
//========================================================================

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    JOYCAPS   jc;

    // Is joystick present?
    if( !_glfwJoystickPresent( joy ) )
    {
        return GL_FALSE;
    }

    // Get joystick capabilities
    _glfw_joyGetDevCaps( joy - GLFW_JOYSTICK_1, &jc, sizeof(JOYCAPS) );
    *device_id = (char*)jc.szPname;
    return GL_TRUE;
}

#endif
