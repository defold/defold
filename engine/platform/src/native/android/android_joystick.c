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
#include "android_joystick.h"
#include "android_jni.h"
#include "android_log.h"

#include <stdint.h>

#define DEVICE_ID_NONE (-1000)
#define SDL_ANDROID_GAMEPAD_BUS 0x0005 // Bluetooth

static int dmNativeAndroidJoystickPresent( int joy )
{
    if (joy >= 0 && joy <= NATIVE_JOYSTICK_LAST && dmNativeJoy[joy].State == NATIVE_ANDROID_GAMEPAD_CONNECTED)
    {
        return GL_TRUE;
    }
    return GL_FALSE;
}

static int dmNativeAndroidFindJoystick(const int32_t deviceId)
{
    int32_t joystickIndex;
    for (joystickIndex = 0; joystickIndex <= NATIVE_JOYSTICK_LAST; joystickIndex++)
    {
        if (dmNativeJoy[joystickIndex].DeviceId == deviceId)
        {
            return joystickIndex;
        }
    }
    return -1;
}

static int32_t dmNativeAndroidConnectJoystick(int32_t deviceId, const char* deviceName, const char* deviceGuid)
{
    int32_t joystickIndex = -1;
    for (joystickIndex = 0; joystickIndex <= NATIVE_JOYSTICK_LAST; joystickIndex++)
    {
        if (dmNativeJoy[joystickIndex].State == NATIVE_ANDROID_GAMEPAD_DISCONNECTED)
        {
            dmNativeJoy[joystickIndex].State = NATIVE_ANDROID_GAMEPAD_CONNECTED;
            dmNativeJoy[joystickIndex].DeviceId = deviceId;
            dmNativeJoy[joystickIndex].NumAxes = NATIVE_ANDROID_GAMEPAD_NUMAXIS;
            dmNativeJoy[joystickIndex].NumButtons = NATIVE_ANDROID_GAMEPAD_NUMBUTTONS;
            strncpy(dmNativeJoy[joystickIndex].DeviceName, deviceName, DEVICE_NAME_LENGTH - 1);
            dmNativeJoy[joystickIndex].DeviceName[DEVICE_NAME_LENGTH - 1] = '\0';
            strncpy(dmNativeJoy[joystickIndex].DeviceGuid, deviceGuid, DEVICE_GUID_LENGTH);
            dmNativeJoy[joystickIndex].DeviceGuid[DEVICE_GUID_LENGTH] = '\0';
            memset(dmNativeJoy[joystickIndex].Axis, 0, sizeof(dmNativeJoy[joystickIndex].Axis));
            memset(dmNativeJoy[joystickIndex].Button, 0, sizeof(dmNativeJoy[joystickIndex].Button));

            dmNativeWin.gamepadCallback(joystickIndex, 1);
            break;
        }
    }
    return joystickIndex;
}

static void dmNativeAndroidDisconnectJoystick(const int joystickIndex)
{
    if (dmNativeJoy[joystickIndex].State != NATIVE_ANDROID_GAMEPAD_DISCONNECTED)
    {
        dmNativeJoy[joystickIndex].State = NATIVE_ANDROID_GAMEPAD_DISCONNECTED;
        dmNativeJoy[joystickIndex].DeviceId = DEVICE_ID_NONE;
        dmNativeJoy[joystickIndex].NumAxes = 0;
        dmNativeJoy[joystickIndex].NumButtons = 0;
        dmNativeWin.gamepadCallback(joystickIndex, 0);
    }
}

//========================================================================
// Update joystick axis
//========================================================================

static int dmNativeAndroidMotionToJoystickAxis(int motionAxis)
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

static void dmNativeAndroidUpdateAxisValue(const int joystickIndex, const AInputEvent* event, int motionAxis)
{
    float axisValue = AMotionEvent_getAxisValue(event, motionAxis, 0);


    int axis = dmNativeAndroidMotionToJoystickAxis(motionAxis);
    if (axis >= 0)
    {
        dmNativeJoy[joystickIndex].Axis[axis] = axisValue;
    }
}

static void dmNativeAndroidUpdateAxis(const AInputEvent* event)
{
    const int32_t joystickIndex = dmNativeAndroidFindJoystick(AInputEvent_getDeviceId(event));
    if (joystickIndex >= 0)
    {
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_X);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_Y);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_Z);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_RZ);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_LTRIGGER);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_RTRIGGER);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_HAT_X);
        dmNativeAndroidUpdateAxisValue(joystickIndex, event, AMOTION_EVENT_AXIS_HAT_Y);
    }
}


//========================================================================
// Update joystick buttons
//========================================================================

static int dmNativeAndroidKeycodeToJoystickButton(int keyCode)
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

static void dmNativeAndroidUpdateButton(const AInputEvent* event)
{
    const int32_t keyCode = AKeyEvent_getKeyCode(event);
    const int32_t joystickIndex = dmNativeAndroidFindJoystick(AInputEvent_getDeviceId(event));
    const int32_t action = AKeyEvent_getAction(event);
    if (joystickIndex >= 0)
    {
        int button = dmNativeAndroidKeycodeToJoystickButton(keyCode);
        if (button != -1)
        {
            dmNativeJoy[joystickIndex].Button[button] = (action == AKEY_EVENT_ACTION_DOWN) ? NATIVE_PRESS : NATIVE_RELEASE;
        }
    }
}


//========================================================================
// Update joystick dpad
//========================================================================

static void dmNativeAndroidUpdateDpad(const AInputEvent* event)
{
    const int32_t joystickIndex = dmNativeAndroidFindJoystick(AInputEvent_getDeviceId(event));
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t action_action = action & AMOTION_EVENT_ACTION_MASK;
    if (joystickIndex >= 0)
    {
        dmNativeJoy[joystickIndex].Hats = NATIVE_HAT_CENTERED;
        float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        if (hatX == 1.0f)
        {
            dmNativeJoy[joystickIndex].Button[dmNativeAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_RIGHT)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? NATIVE_PRESS : NATIVE_RELEASE;
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_RIGHT;
        }
        else if (hatX == -1.0f)
        {
            dmNativeJoy[joystickIndex].Button[dmNativeAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_LEFT)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? NATIVE_PRESS : NATIVE_RELEASE;
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_LEFT;
        }
        float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
        if (hatY == 1.0f)
        {
            dmNativeJoy[joystickIndex].Button[dmNativeAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_DOWN)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? NATIVE_PRESS : NATIVE_RELEASE;
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_DOWN;
        }
        else if (hatY == -1.0f)
        {
            dmNativeJoy[joystickIndex].Button[dmNativeAndroidKeycodeToJoystickButton(AKEYCODE_DPAD_UP)] = (action_action == AMOTION_EVENT_ACTION_DOWN) ? NATIVE_PRESS : NATIVE_RELEASE;
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_UP;
        }
    }
}


//========================================================================
// Update joystick from input event
// Called from android_init.c when input events are processed
//========================================================================

void dmNativeAndroidUpdateJoystick(const AInputEvent* event)
{
    const int32_t source = AInputEvent_getSource(event);
    const int32_t type = AInputEvent_getType(event);
    const int32_t action = AKeyEvent_getAction(event);

    if ((source & AINPUT_SOURCE_GAMEPAD) != 0)
    {
        if (type == AINPUT_EVENT_TYPE_KEY)
        {
            dmNativeAndroidUpdateButton(event);
        }
    }
    else if ((source & AINPUT_SOURCE_JOYSTICK) != 0)
    {
        if ((type == AINPUT_EVENT_TYPE_MOTION) && ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_MOVE))
        {
            dmNativeAndroidUpdateAxis(event);
            dmNativeAndroidUpdateDpad(event);
        }
    }
    else if ((source & AINPUT_SOURCE_DPAD) != 0)
    {
        if ((type == AINPUT_EVENT_TYPE_MOTION) && ((action & AMOTION_EVENT_ACTION_MASK) == AMOTION_EVENT_ACTION_MOVE))
        {
            dmNativeAndroidUpdateDpad(event);
        }
    }
}


//========================================================================
// Discover connected joysticks
// Called from android_window.c each frame
//========================================================================

void dmNativeAndroidDiscoverJoysticks()
{
    int32_t joystickIndex;
    int discovery_succeeded = 0;

    JNIEnv* env = JNIAttachCurrentThread();
    if (env)
    {
        jobject native_activity = g_AndroidApp->activity->clazz;
        if (native_activity == 0) goto cleanup_and_early_exit;

        jmethodID get_game_controller_device_ids = 0;
        jmethodID get_game_controller_device_name = 0;
        jmethodID get_game_controller_device_descriptor = 0;
        jmethodID get_game_controller_device_vendor_id = 0;
        jmethodID get_game_controller_device_product_id = 0;
        jintArray device_ids = 0;
        jsize device_ids_len = 0;
        jint *device_ids_elements = 0;

        get_game_controller_device_ids = JNIGetMethodID(env, native_activity, "getGameControllerDeviceIds", "()[I");
        if (get_game_controller_device_ids == 0)
        {
            goto cleanup_and_early_exit;
        }
        get_game_controller_device_name = JNIGetMethodID(env, native_activity, "getGameControllerDeviceName", "(I)Ljava/lang/String;");
        if (get_game_controller_device_name == 0)
        {
            goto cleanup_and_early_exit;
        }
        get_game_controller_device_descriptor = JNIGetMethodID(env, native_activity, "getGameControllerDeviceDescriptor", "(I)Ljava/lang/String;");
        if (get_game_controller_device_descriptor == 0)
        {
            goto cleanup_and_early_exit;
        }
        get_game_controller_device_vendor_id = JNIGetMethodID(env, native_activity, "getGameControllerDeviceVendorId", "(I)I");
        if (get_game_controller_device_vendor_id == 0)
        {
            goto cleanup_and_early_exit;
        }
        get_game_controller_device_product_id = JNIGetMethodID(env, native_activity, "getGameControllerDeviceProductId", "(I)I");
        if (get_game_controller_device_product_id == 0)
        {
            goto cleanup_and_early_exit;
        }

        device_ids = (*env)->CallObjectMethod(env, native_activity, get_game_controller_device_ids);
        if (JNICheckAndClearException(env) || device_ids == 0)
        {
            goto cleanup_and_early_exit;
        }

        device_ids_len = (*env)->GetArrayLength(env, device_ids);
        if (JNICheckAndClearException(env))
        {
            goto cleanup_and_early_exit;
        }

        device_ids_elements = (*env)->GetIntArrayElements(env, device_ids, 0);
        if (JNICheckAndClearException(env) || device_ids_elements == 0)
        {
            goto cleanup_and_early_exit;
        }

        // Prepare connected gamepads to be refreshed only after discovery has
        // returned a valid array. Earlier failures must leave their state intact.
        for (joystickIndex = 0; joystickIndex <= NATIVE_JOYSTICK_LAST; joystickIndex++)
        {
            if (dmNativeJoy[joystickIndex].State == NATIVE_ANDROID_GAMEPAD_CONNECTED)
            {
                dmNativeJoy[joystickIndex].State = NATIVE_ANDROID_GAMEPAD_REFRESHING;
            }
        }
        discovery_succeeded = 1;

        for (int i=0; i<device_ids_len; i++)
        {
            int32_t deviceId = device_ids_elements[i];
            int deviceIndex = dmNativeAndroidFindJoystick(deviceId);
            if (deviceIndex == -1)
            {
                jint jni_device_id = deviceId;
                jint vendor_id = 0;
                jint product_id = 0;
                jstring jni_device_name = 0;
                jstring jni_device_descriptor = 0;
                char *deviceName = 0;
                char *deviceDescriptor = 0;
                
                jni_device_name = (*env)->CallObjectMethod(env, native_activity, get_game_controller_device_name, jni_device_id);
                if (JNICheckAndClearException(env) || jni_device_name == 0)
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }

                jni_device_descriptor = (*env)->CallObjectMethod(env, native_activity, get_game_controller_device_descriptor, jni_device_id);
                if (JNICheckAndClearException(env) || jni_device_descriptor == 0)
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }

                vendor_id = (*env)->CallIntMethod(env, native_activity, get_game_controller_device_vendor_id, jni_device_id);
                if (JNICheckAndClearException(env))
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }

                product_id = (*env)->CallIntMethod(env, native_activity, get_game_controller_device_product_id, jni_device_id);
                if (JNICheckAndClearException(env))
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }

                deviceName = (*env)->GetStringUTFChars(env, jni_device_name, 0);
                if (JNICheckAndClearException(env) || deviceName == 0)
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }

                deviceDescriptor = (*env)->GetStringUTFChars(env, jni_device_descriptor, 0);
                if (JNICheckAndClearException(env) || deviceDescriptor == 0)
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }

                char deviceGuid[DEVICE_GUID_LENGTH + 1];
                dmNativeCreateJoystickDeviceGuid(SDL_ANDROID_GAMEPAD_BUS, (unsigned short) vendor_id, (unsigned short) product_id,
                    0, 0, deviceDescriptor, 0, 0, deviceGuid);
                deviceIndex = dmNativeAndroidConnectJoystick(deviceId, deviceName, deviceGuid);
cleanup_and_break:
                if (deviceDescriptor != 0)
                {
                    (*env)->ReleaseStringUTFChars(env, jni_device_descriptor, deviceDescriptor);
                }
                if (deviceName != 0)
                {
                    (*env)->ReleaseStringUTFChars(env, jni_device_name, deviceName);
                }
                if (jni_device_descriptor != 0)
                {
                    (*env)->DeleteLocalRef(env, jni_device_descriptor);
                }
                if (jni_device_name != 0)
                {
                    (*env)->DeleteLocalRef(env, jni_device_name);
                }
                if (!discovery_succeeded)
                {
                    break;
                }
            }
            else
            {
                dmNativeJoy[deviceIndex].State = NATIVE_ANDROID_GAMEPAD_CONNECTED;
            }
        }

cleanup_and_early_exit:
        if (device_ids_elements != 0)
        {
            (*env)->ReleaseIntArrayElements(env, device_ids, device_ids_elements, JNI_ABORT); // JNI_ABORT because we are not modifying elements
        }
        if (device_ids != 0)
        {
            (*env)->DeleteLocalRef(env, device_ids);
        }
        JNIDetachCurrentThread();
    }

    // disconnect gamepads that we failed to refresh
    for (joystickIndex = 0; joystickIndex <= NATIVE_JOYSTICK_LAST; joystickIndex++)
    {
        if (dmNativeJoy[joystickIndex].State == NATIVE_ANDROID_GAMEPAD_REFRESHING)
        {
            if (discovery_succeeded)
            {
                dmNativeAndroidDisconnectJoystick(joystickIndex);
            }
            else
            {
                dmNativeJoy[joystickIndex].State = NATIVE_ANDROID_GAMEPAD_CONNECTED;
            }
        }
    }
}


//========================================================================
// Determine joystick capabilities
// Called by dmNative
//========================================================================

int dmNativeOSGetJoystickParam( int joy, int param )
{
    if( !dmNativeAndroidJoystickPresent( joy ) )
    {
        return 0;
    }

    switch( param )
    {
    case NATIVE_PRESENT:
        return GL_TRUE;

    case NATIVE_AXES:
        return dmNativeJoy[ joy ].NumAxes;

    case NATIVE_BUTTONS:
        return dmNativeJoy[ joy ].NumButtons;

    case NATIVE_HATS:
        return 1;

    default:
        break;
    }

    return 0;
}


//========================================================================
// Get joystick axis positions
// Called by dmNative
//========================================================================

int dmNativeOSGetJoystickPos( int joy, float *pos, int numaxes )
{
    int i;

    if( !dmNativeAndroidJoystickPresent( joy ) )
    {
        return 0;
    }

    // Does the joystick support less axes than requested?
    if( dmNativeJoy[ joy ].NumAxes < numaxes )
    {
        numaxes = dmNativeJoy[ joy ].NumAxes;
    }

    // Copy axis positions from internal state
    for( i = 0; i < numaxes; ++ i )
    {
        pos[ i ] = dmNativeJoy[ joy ].Axis[ i ];
    }

    return 0;
}


//========================================================================
// Get joystick button states
// Called by dmNative
//========================================================================

int dmNativeOSGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    int i;

    if( !dmNativeAndroidJoystickPresent( joy ) )
    {
        return 0;
    }

    // Does the joystick support less buttons than requested?
    if( dmNativeJoy[ joy ].NumButtons < numbuttons )
    {
        numbuttons = dmNativeJoy[ joy ].NumButtons;
    }

    // Copy button states from internal state
    for( i = 0; i < numbuttons; ++ i )
    {
        buttons[ i ] = dmNativeJoy[ joy ].Button[ i ];
    }

    return numbuttons;
}


//========================================================================
// Get joystick hats states
// Called by dmNative
//========================================================================

int dmNativeOSGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    if( !dmNativeAndroidJoystickPresent( joy ) )
    {
        return 0;
    }
    hats[0] |= dmNativeJoy[ joy ].Hats;
    return 1;
}


//========================================================================
// dmNativeOSGetJoystickDeviceId() - Get joystick device id
// Called by dmNative
//========================================================================

int dmNativeOSGetJoystickDeviceId( int joy, char** device_id )
{
    if( !dmNativeAndroidJoystickPresent( joy ) )
    {
        return GL_FALSE;
    }
    else
    {
        *device_id = (char*) dmNativeJoy[ joy ].DeviceName;
        return GL_TRUE;
    }
}

// DEFOLD
int dmNativeGetJoystickDeviceGuid( int joy, char** device_guid )
{
    if( !dmNativeAndroidJoystickPresent( joy ) )
    {
        return GL_FALSE;
    }

    *device_guid = (char*) dmNativeJoy[ joy ].DeviceGuid;
    return GL_TRUE;
}

void dmNativeTerminateJoysticks( void )
{
    LOGI("dmNativeTerminateJoysticks");
    int32_t joystickIndex = -1;
    for( joystickIndex = 0; joystickIndex <= NATIVE_JOYSTICK_LAST; joystickIndex++ )
    {
        dmNativeJoy[joystickIndex].State = NATIVE_ANDROID_GAMEPAD_DISCONNECTED;
        dmNativeJoy[joystickIndex].DeviceId = DEVICE_ID_NONE;
        dmNativeJoy[joystickIndex].NumAxes = 0;
        dmNativeJoy[joystickIndex].NumButtons = 0;
    }

}

#undef DEVICE_ID_NONE
