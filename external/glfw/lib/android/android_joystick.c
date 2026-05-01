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
#include "android_joystick.h"
#include "android_jni.h"
#include "android_log.h"

#define DEVICE_ID_NONE (-1000)

static int glfwAndroidJoystickPresent( int joy )
{
    if (joy >= 0 && joy <= GLFW_JOYSTICK_LAST && _glfwJoy[joy].State == GLFW_ANDROID_GAMEPAD_CONNECTED)
    {
        return GL_TRUE;
    }
    return GL_FALSE;
}

static int glfwAndroidFindJoystick(const int32_t deviceId)
{
    int32_t joystickIndex;
    for (joystickIndex = 0; joystickIndex <= GLFW_JOYSTICK_LAST; joystickIndex++)
    {
        if (_glfwJoy[joystickIndex].DeviceId == deviceId)
        {
            return joystickIndex;
        }
    }
    return -1;
}

static int32_t glfwAndroidConnectJoystick(int32_t deviceId, char* deviceName)
{
    int n;
    int32_t joystickIndex = -1;
    for (joystickIndex = 0; joystickIndex <= GLFW_JOYSTICK_LAST; joystickIndex++)
    {
        if (_glfwJoy[joystickIndex].State == GLFW_ANDROID_GAMEPAD_DISCONNECTED)
        {
            _glfwJoy[joystickIndex].State = GLFW_ANDROID_GAMEPAD_CONNECTED;
            _glfwJoy[joystickIndex].DeviceId = deviceId;
            _glfwJoy[joystickIndex].NumAxes = GLFW_ANDROID_GAMEPAD_NUMAXIS;
            _glfwJoy[joystickIndex].NumButtons = GLFW_ANDROID_GAMEPAD_NUMBUTTONS;
            strncpy(&_glfwJoy[joystickIndex].DeviceName, deviceName, DEVICE_NAME_LENGTH);
            memset(_glfwJoy[joystickIndex].Axis, 0, sizeof(_glfwJoy[joystickIndex].Axis) * GLFW_ANDROID_GAMEPAD_NUMAXIS);
            memset(_glfwJoy[joystickIndex].Button, 0, sizeof(_glfwJoy[joystickIndex].Button) * GLFW_ANDROID_GAMEPAD_NUMBUTTONS);

            _glfwWin.gamepadCallback(joystickIndex, 1);
            break;
        }
    }
    return joystickIndex;
}

static void glfwAndroidDisconnectJoystick(const int joystickIndex)
{
    if (_glfwJoy[joystickIndex].State != GLFW_ANDROID_GAMEPAD_DISCONNECTED)
    {
        _glfwJoy[joystickIndex].State = GLFW_ANDROID_GAMEPAD_DISCONNECTED;
        _glfwJoy[joystickIndex].DeviceId = DEVICE_ID_NONE;
        _glfwJoy[joystickIndex].NumAxes = 0;
        _glfwJoy[joystickIndex].NumButtons = 0;
        _glfwWin.gamepadCallback(joystickIndex, 0);
    }
}

//========================================================================
// Update joystick axis
//========================================================================

static int glfwAndroidMotionToJoystickAxis(int motionAxis)
{
    switch(motionAxis)
    {
        case AMOTION_EVENT_AXIS_X:          return 0;
        case AMOTION_EVENT_AXIS_Y:          return 1;
        case AMOTION_EVENT_AXIS_Z:          return 2;
        case AMOTION_EVENT_AXIS_RZ:         return 3;
        case AMOTION_EVENT_AXIS_LTRIGGER:   return 4;
        case AMOTION_EVENT_AXIS_RTRIGGER:   return 5;
        case AMOTION_EVENT_AXIS_HAT_X:      return 6;
        case AMOTION_EVENT_AXIS_HAT_Y:      return 7;
        default: return -1;
    }
}

static void glfwAndroidUpdateAxisValue(const int joystickIndex, const AInputEvent* event, int motionAxis)
{
    float axisValue = AMotionEvent_getAxisValue(event, motionAxis, 0);


    int axis = glfwAndroidMotionToJoystickAxis(motionAxis);
    if (axis >= 0)
    {
        _glfwJoy[joystickIndex].Axis[axis] = axisValue;
    }
}

static void glfwAndroidUpdateAxis(const AInputEvent* event)
{
    const int32_t joystickIndex = glfwAndroidFindJoystick(AInputEvent_getDeviceId(event));
    if (joystickIndex >= 0)
    {
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_X);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_Y);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_Z);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_RZ);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_LTRIGGER);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_RTRIGGER);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_HAT_X);
        glfwAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_HAT_Y);
    }
}


//========================================================================
// Update joystick buttons
//========================================================================

static int glfwAndroidKeycodeToJoystickButton(int keyCode)
{
    switch(keyCode)
    {
        case AKEYCODE_BUTTON_A:         return 0;
        case AKEYCODE_BUTTON_B:         return 1;
        case AKEYCODE_BUTTON_C:         return 2;
        case AKEYCODE_BUTTON_X:         return 3;
        case AKEYCODE_BUTTON_L1:        return 4;
        case AKEYCODE_BUTTON_R1:        return 5;
        case AKEYCODE_BUTTON_Y:         return 6;
        case AKEYCODE_BUTTON_Z:         return 7;
        case AKEYCODE_BUTTON_L2:        return 8;
        case AKEYCODE_BUTTON_R2:        return 9;
        case AKEYCODE_DPAD_CENTER:      return 10;
        case AKEYCODE_DPAD_DOWN:        return 11;
        case AKEYCODE_DPAD_LEFT:        return 12;
        case AKEYCODE_DPAD_RIGHT:       return 13;
        case AKEYCODE_DPAD_UP:          return 14;
        case AKEYCODE_BUTTON_START:     return 15;
        case AKEYCODE_BUTTON_SELECT:    return 16;
        case AKEYCODE_BUTTON_THUMBL:    return 17;
        case AKEYCODE_BUTTON_THUMBR:    return 18;
        case AKEYCODE_BUTTON_MODE:      return 19;
        case AKEYCODE_BUTTON_1:         return 20;
        case AKEYCODE_BUTTON_2:         return 21;
        case AKEYCODE_BUTTON_3:         return 22;
        case AKEYCODE_BUTTON_4:         return 23;
        case AKEYCODE_BUTTON_5:         return 24;
        case AKEYCODE_BUTTON_6:         return 25;
        case AKEYCODE_BUTTON_7:         return 26;
        case AKEYCODE_BUTTON_8:         return 27;
        case AKEYCODE_BUTTON_9:         return 28;
        case AKEYCODE_BUTTON_10:        return 29;
        case AKEYCODE_BUTTON_11:        return 30;
        case AKEYCODE_BUTTON_12:        return 31;
        case AKEYCODE_BUTTON_13:        return 32;
        case AKEYCODE_BUTTON_14:        return 33;
        case AKEYCODE_BUTTON_15:        return 34;
        case AKEYCODE_BUTTON_16:        return 35;
        default: return -1;
    }
}

static void glfwAndroidUpdateButton(const AInputEvent* event)
{
    const int32_t keyCode = AKeyEvent_getKeyCode(event);
    const int32_t joystickIndex = glfwAndroidFindJoystick(AInputEvent_getDeviceId(event));
    const int32_t action = AKeyEvent_getAction(event);
    if (joystickIndex >= 0)
    {
        int button = glfwAndroidKeycodeToJoystickButton(keyCode);
        if (button != -1)
        {
            _glfwJoy[joystickIndex].Button[button] = (action == AKEY_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
        }
    }
}


//========================================================================
// Update joystick dpad
//========================================================================

static void glfwAndroidUpdateDpad(const AInputEvent* event)
{
    const int32_t joystickIndex = glfwAndroidFindJoystick(AInputEvent_getDeviceId(event));
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t action_action = action & AMOTION_EVENT_ACTION_MASK;
    if (joystickIndex >= 0)
    {
        _glfwJoy[joystickIndex].Hats = GLFW_HAT_CENTERED;
        float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        if (hatX == 1.0f)
        {
            _glfwJoy[joystickIndex].Button[glfwAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_RIGHT)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
            _glfwJoy[joystickIndex].Hats |= GLFW_HAT_RIGHT;
        }
        else if (hatX == -1.0f)
        {
            _glfwJoy[joystickIndex].Button[glfwAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_LEFT)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
            _glfwJoy[joystickIndex].Hats |= GLFW_HAT_LEFT;
        }
        float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
        if (hatY == 1.0f)
        {
            _glfwJoy[joystickIndex].Button[glfwAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_DOWN)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
            _glfwJoy[joystickIndex].Hats |= GLFW_HAT_DOWN;
        }
        else if (hatY == -1.0f)
        {
            _glfwJoy[joystickIndex].Button[glfwAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_UP)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? GLFW_PRESS : GLFW_RELEASE;
            _glfwJoy[joystickIndex].Hats |= GLFW_HAT_UP;
        }
    }
}


//========================================================================
// Update joystick from input event
// Called from android_init.c when input events are processed
//========================================================================

void glfwAndroidUpdateJoystick(const AInputEvent* event)
{
    const int32_t source = AInputEvent_getSource(event);
    const int32_t type = AInputEvent_getType(event);
    const int32_t action = AKeyEvent_getAction(event);

    if ((source & AINPUT_SOURCE_GAMEPAD) != 0)
    {
        if (type == AINPUT_EVENT_TYPE_KEY)
        {
            glfwAndroidUpdateButton(event);
        }
    }
    else if ((source & AINPUT_SOURCE_JOYSTICK) != 0)
    {
        if ((type == AINPUT_EVENT_TYPE_MOTION) && ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_MOVE))
        {
            glfwAndroidUpdateAxis(event);
            glfwAndroidUpdateDpad(event);
        }
    }
    else if ((source & AINPUT_SOURCE_DPAD) != 0)
    {
        if ((type == AINPUT_EVENT_TYPE_MOTION) && ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_MOVE))
        {
            glfwAndroidUpdateDpad(event);
        }
    }
}





//========================================================================
// Discover connected joysticks
// Called from android_window.c each frame
//========================================================================

void glfwAndroidDiscoverJoysticks()
{
    int32_t joystickIndex;

    // prepare all connected gamepads to be refreshed
    for (joystickIndex = 0; joystickIndex <= GLFW_JOYSTICK_LAST; joystickIndex++)
    {
        if (_glfwJoy[joystickIndex].State == GLFW_ANDROID_GAMEPAD_CONNECTED)
        {
            _glfwJoy[joystickIndex].State = GLFW_ANDROID_GAMEPAD_REFRESHING;
        }
    }

    JNIEnv* env = JNIAttachCurrentThread();
    if (env)
    {
        jobject native_activity = g_AndroidApp->activity->clazz;
        jmethodID get_game_controller_device_ids = JNIGetMethodID(env, native_activity, "getGameControllerDeviceIds", "()[I");
        jmethodID get_game_controller_device_name = JNIGetMethodID(env, native_activity, "getGameControllerDeviceName", "(I)Ljava/lang/String;");
        jintArray device_ids = (*env)->CallObjectMethod(env, native_activity, get_game_controller_device_ids);
        jsize len = (*env)->GetArrayLength(env, device_ids);
        jint *elements = (*env)->GetIntArrayElements(env, device_ids, 0);
        for (int i=0; i<len; i++)
        {
            int32_t deviceId = elements[i];
            int deviceIndex = glfwAndroidFindJoystick(deviceId);
            if (deviceIndex == -1)
            {
                jint jni_device_id = deviceId;
                jstring jni_device_name = (*env)->CallObjectMethod(env, native_activity, get_game_controller_device_name, jni_device_id);
                const char *deviceName = (*env)->GetStringUTFChars(env, jni_device_name, 0);
                deviceIndex = glfwAndroidConnectJoystick(deviceId, deviceName);
                (*env)->ReleaseStringUTFChars(env, jni_device_name, deviceName);
            }
            else
            {
                _glfwJoy[deviceIndex].State = GLFW_ANDROID_GAMEPAD_CONNECTED;
            }
        }
        (*env)->ReleaseIntArrayElements(env, device_ids, elements, 0);
        JNIDetachCurrentThread();
    }

    // disconnect gamepads that we failed to refresh
    for (joystickIndex = 0; joystickIndex <= GLFW_JOYSTICK_LAST; joystickIndex++)
    {
        if (_glfwJoy[joystickIndex].State == GLFW_ANDROID_GAMEPAD_REFRESHING)
        {
            glfwAndroidDisconnectJoystick(joystickIndex);
        }
    }
}


//========================================================================
// Determine joystick capabilities
// Called by glfw
//========================================================================

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    if( !glfwAndroidJoystickPresent( joy ) )
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

    case GLFW_HATS:
        return 1;

    default:
        break;
    }

    return 0;
}


//========================================================================
// Get joystick axis positions
// Called by glfw
//========================================================================

int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
    int i;

    if( !glfwAndroidJoystickPresent( joy ) )
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
// Called by glfw
//========================================================================

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    int i;

    if( !glfwAndroidJoystickPresent( joy ) )
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
// Called by glfw
//========================================================================

int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    if( !glfwAndroidJoystickPresent( joy ) )
    {
        return 0;
    }
    hats[0] |= _glfwJoy[ joy ].Hats;
    return 1;
}


//========================================================================
// _glfwPlatformGetJoystickDeviceId() - Get joystick device id
// Called by glfw
//========================================================================

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    if( !glfwAndroidJoystickPresent( joy ) )
    {
        return GL_FALSE;
    }
    else
    {
        *device_id = (char*) _glfwJoy[ joy ].DeviceName;
        return GL_TRUE;
    }
}

void _glfwTerminateJoysticks( void )
{
    LOGI("_glfwTerminateJoysticks");
    int32_t joystickIndex = -1;
    for( joystickIndex = 0; joystickIndex <= GLFW_JOYSTICK_LAST; joystickIndex++ )
    {
        _glfwJoy[joystickIndex].State = GLFW_ANDROID_GAMEPAD_DISCONNECTED;
        _glfwJoy[joystickIndex].DeviceId = DEVICE_ID_NONE;
        _glfwJoy[joystickIndex].NumAxes = 0;
        _glfwJoy[joystickIndex].NumButtons = 0;
    }

}

#undef DEVICE_ID_NONE
