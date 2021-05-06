// Copyright 2020 The Defold Foundation
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
#include "android_joystick.h"
#include "android_log.h"

static int _glfwJoystickPresent( int joy )
{
    if (joy >= 0 && joy <= GLFW_JOYSTICK_LAST && _glfwJoy[joy].Present)
    {
        return GL_TRUE;
    }
    return GL_FALSE;
}

int _glfwInitJoystick(int joy)
{
    if (joy < 0 || joy > GLFW_JOYSTICK_LAST)
    {
        return GL_FALSE;
    }
    if (!_glfwJoy[joy].Present)
    {
        _glfwJoy[joy].Present = 1;
        _glfwJoy[joy].fd = 0;
        _glfwJoy[joy].NumAxes = GLFW_ANDROID_GAMEPAD_NUMAXIS;
        _glfwJoy[joy].NumButtons = GLFW_ANDROID_GAMEPAD_NUMBUTTONS;
        _glfwJoy[joy].Axis = (float *)malloc(sizeof(float) * _glfwJoy[joy].NumAxes);
        _glfwJoy[joy].Button = (unsigned char *)malloc(sizeof(char) * _glfwJoy[joy].NumButtons );
        _glfwWin.gamepadCallback(joy, 1);
    }
    return GL_TRUE;
}


//========================================================================
// Update joystick axis
//========================================================================

static int _glfwAndroidMotionToJoystickAxis(int motionAxis)
{
    switch(motionAxis)
    {
        case AMOTION_EVENT_AXIS_X:  return GLFW_ANDROID_GAMEPAD_AXIS_X;
        case AMOTION_EVENT_AXIS_Y:  return GLFW_ANDROID_GAMEPAD_AXIS_Y;
        case AMOTION_EVENT_AXIS_Z:  return GLFW_ANDROID_GAMEPAD_AXIS_Z;
        case AMOTION_EVENT_AXIS_RZ:  return GLFW_ANDROID_GAMEPAD_AXIS_RZ;
        default: return -1;
    }
}

static void _glfwUpdateAxisValue(const AInputEvent* event, int motionAxis)
{
    const int32_t deviceId = AInputEvent_getDeviceId(event);
    float axisValue = AMotionEvent_getAxisValue(event, motionAxis, 0);
    if (axisValue != 0.0f)
    {
        int axis = _glfwAndroidMotionToJoystickAxis(motionAxis);
        _glfwJoy[deviceId].Axis[axis] = axisValue;
    }
}

static void _glfwUpdateAxis(const AInputEvent* event)
{
    const int32_t deviceId = AInputEvent_getDeviceId(event);
    if (_glfwInitJoystick(deviceId))
    {
        _glfwUpdateAxisValue(event, AMOTION_EVENT_AXIS_X);
        _glfwUpdateAxisValue(event, AMOTION_EVENT_AXIS_Y);
        _glfwUpdateAxisValue(event, AMOTION_EVENT_AXIS_Z);
        _glfwUpdateAxisValue(event, AMOTION_EVENT_AXIS_RZ);
    }
}


//========================================================================
// Update joystick dpad
//========================================================================

static void _glfwUpdateDpad(const AInputEvent* event)
{
    const int32_t deviceId = AInputEvent_getDeviceId(event);
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t action_action = action & AMOTION_EVENT_ACTION_MASK;
    if (_glfwInitJoystick(deviceId))
    {
        float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        if (hatX == 1.0f)
        {
            _glfwJoy[deviceId].Button[GLFW_ANDROID_GAMEPAD_DPAD_RIGHT] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
        }
        else if (hatX == -1.0f)
        {
            _glfwJoy[deviceId].Button[GLFW_ANDROID_GAMEPAD_DPAD_LEFT] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
        }
        float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
        if (hatY == 1.0f)
        {
            _glfwJoy[deviceId].Button[GLFW_ANDROID_GAMEPAD_DPAD_UP] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
        }
        else if (hatY == -1.0f)
        {
            _glfwJoy[deviceId].Button[GLFW_ANDROID_GAMEPAD_DPAD_DOWN] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
        }
    }
}


//========================================================================
// Update joystick buttons
//========================================================================

static int _glfwAndroidKeycodeToJoystickButton(int keyCode)
{
    switch(keyCode)
    {
        case AKEYCODE_DPAD_CENTER:      return GLFW_ANDROID_GAMEPAD_DPAD_CENTER;
        case AKEYCODE_DPAD_DOWN:        return GLFW_ANDROID_GAMEPAD_DPAD_DOWN;
        case AKEYCODE_DPAD_LEFT:        return GLFW_ANDROID_GAMEPAD_DPAD_LEFT;
        case AKEYCODE_DPAD_RIGHT:       return GLFW_ANDROID_GAMEPAD_DPAD_RIGHT;
        case AKEYCODE_DPAD_UP:          return GLFW_ANDROID_GAMEPAD_DPAD_UP;
        case AKEYCODE_BUTTON_THUMBL:    return GLFW_ANDROID_GAMEPAD_BUTTON_THUMBL;
        case AKEYCODE_BUTTON_THUMBR:    return GLFW_ANDROID_GAMEPAD_BUTTON_THUMBR;
        case AKEYCODE_BUTTON_A:         return GLFW_ANDROID_GAMEPAD_BUTTON_A;
        case AKEYCODE_BUTTON_B:         return GLFW_ANDROID_GAMEPAD_BUTTON_B;
        case AKEYCODE_BUTTON_C:         return GLFW_ANDROID_GAMEPAD_BUTTON_C;
        case AKEYCODE_BUTTON_X:         return GLFW_ANDROID_GAMEPAD_BUTTON_X;
        case AKEYCODE_BUTTON_Y:         return GLFW_ANDROID_GAMEPAD_BUTTON_Y;
        case AKEYCODE_BUTTON_Z:         return GLFW_ANDROID_GAMEPAD_BUTTON_Z;
        case AKEYCODE_BUTTON_L1:        return GLFW_ANDROID_GAMEPAD_BUTTON_L1;
        case AKEYCODE_BUTTON_R1:        return GLFW_ANDROID_GAMEPAD_BUTTON_R1;
        case AKEYCODE_BUTTON_L2:        return GLFW_ANDROID_GAMEPAD_BUTTON_L2;
        case AKEYCODE_BUTTON_R2:        return GLFW_ANDROID_GAMEPAD_BUTTON_R2;
        default: return -1;
    }
}

static void _glfwUpdateButton(const AInputEvent* event)
{
    const int32_t keyCode = AKeyEvent_getKeyCode(event);
    const int32_t deviceId = AInputEvent_getDeviceId(event);
    const int32_t action = AKeyEvent_getAction(event);
    if (_glfwInitJoystick(deviceId))
    {
        int button = _glfwAndroidKeycodeToJoystickButton(keyCode);
        if (button != -1)
        {
            _glfwJoy[deviceId].Button[button] = (action == AKEY_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
        }
    }
}


//========================================================================
// Update joystick from input event
//========================================================================

void _glfwUpdateJoystick(const AInputEvent* event)
{
    const int32_t source = AInputEvent_getSource(event);
    const int32_t type = AInputEvent_getType(event);
    const int32_t action = AKeyEvent_getAction(event);

    if ((source & AINPUT_SOURCE_GAMEPAD) != 0)
    {
        if (type == AINPUT_EVENT_TYPE_KEY)
        {
            _glfwUpdateButton(event);
        }
    }
    else if ((source & AINPUT_SOURCE_JOYSTICK) != 0)
    {
        if ((type == AINPUT_EVENT_TYPE_MOTION) && ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_MOVE))
        {
            LOGV("_glfwUpdateJoystick AINPUT_SOURCE_JOYSTICK");
            _glfwUpdateAxis(event);
        }
    }
    else if ((source & AINPUT_SOURCE_DPAD) != 0)
    {
        if ((type == AINPUT_EVENT_TYPE_MOTION) && ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_MOVE))
        {
            LOGV("_glfwUpdateJoystick AINPUT_SOURCE_DPAD");
            _glfwUpdateDpad(event);
        }
    }
}


//========================================================================
// Determine joystick capabilities
//========================================================================

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    switch( param )
    {
    case GLFW_PRESENT:
        return GL_TRUE;

    case GLFW_AXES:
        return _glfwJoy[ joy ].NumAxes;

    case GLFW_BUTTONS:
        return _glfwJoy[ joy ].NumButtons;

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
    int i;

    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    // Does the joystick support less axes than requested?
    if( _glfwJoy[ joy ].NumAxes < numaxes )
    {
        numaxes = _glfwJoy[ joy ].NumAxes;
    }

    // Copy axis positions from internal state
    for( i = 0; i < numaxes; ++ i )
    {
        pos[ i ] = _glfwJoy[ joy ].Axis[ i ];
    }

    return 0;
}


//========================================================================
// Get joystick button states
//========================================================================

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    int i;

    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }

    // Does the joystick support less buttons than requested?
    if( _glfwJoy[ joy ].NumButtons < numbuttons )
    {
        numbuttons = _glfwJoy[ joy ].NumButtons;
    }

    // Copy button states from internal state
    for( i = 0; i < numbuttons; ++ i )
    {
        buttons[ i ] = _glfwJoy[ joy ].Button[ i ];
    }

    return numbuttons;
}


//========================================================================
// Get joystick hats states
//========================================================================

int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    if( !_glfwJoystickPresent( joy ) )
    {
        return 0;
    }
    return 0;
}


//========================================================================
// _glfwPlatformGetJoystickDeviceId() - Get joystick device id
//========================================================================

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    static char name[] = "Android Controller";
    if( !_glfwJoystickPresent( joy ) )
    {
        return GL_FALSE;
    }
    else
    {
        *device_id = (char*) name;
        return GL_TRUE;
    }
}
