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

enum
{
    ANDROID_SDL_BUTTON_RECORD = 15,
    ANDROID_BUTTON_BACK = NATIVE_ANDROID_GAMEPAD_NUMBUTTONS,
    ANDROID_BUTTON_MENU,
    ANDROID_BUTTON_RECORD,
};

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

static const int ANDROID_LEGACY_AXES[] = {
    AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y, AMOTION_EVENT_AXIS_Z, AMOTION_EVENT_AXIS_RZ,
    AMOTION_EVENT_AXIS_LTRIGGER, AMOTION_EVENT_AXIS_RTRIGGER, AMOTION_EVENT_AXIS_HAT_X, AMOTION_EVENT_AXIS_HAT_Y
};

static int dmNativeAndroidDeviceAxisIndex(int joy, int axis)
{
    for (int i = 0; i < NATIVE_ANDROID_GAMEPAD_NUMAXIS; ++i)
    {
        if (dmNativeJoy[joy].AxisIds[axis] == ANDROID_LEGACY_AXES[i])
            return i;
    }
    return NATIVE_ANDROID_GAMEPAD_NUMAXIS + axis;
}

static int32_t dmNativeAndroidConnectJoystick(int32_t deviceId, const char* deviceName, const char* deviceGuid,
                                             unsigned int buttonMask, const float* axes, int axisCount)
{
    int32_t joystickIndex = -1;
    for (joystickIndex = 0; joystickIndex <= NATIVE_JOYSTICK_LAST; joystickIndex++)
    {
        if (dmNativeJoy[joystickIndex].State == NATIVE_ANDROID_GAMEPAD_DISCONNECTED)
        {
            dmNativeJoy[joystickIndex].State = NATIVE_ANDROID_GAMEPAD_CONNECTED;
            dmNativeJoy[joystickIndex].DeviceId = deviceId;
            strncpy(dmNativeJoy[joystickIndex].DeviceName, deviceName, DEVICE_NAME_LENGTH - 1);
            dmNativeJoy[joystickIndex].DeviceName[DEVICE_NAME_LENGTH - 1] = '\0';
            strncpy(dmNativeJoy[joystickIndex].DeviceGuid, deviceGuid, DEVICE_GUID_LENGTH);
            dmNativeJoy[joystickIndex].DeviceGuid[DEVICE_GUID_LENGTH] = '\0';
            memset(dmNativeJoy[joystickIndex].Axis, 0, sizeof(dmNativeJoy[joystickIndex].Axis));
            memset(dmNativeJoy[joystickIndex].Button, 0, sizeof(dmNativeJoy[joystickIndex].Button));
            dmNativeJoy[joystickIndex].Hats = NATIVE_HAT_CENTERED;
            dmNativeJoy[joystickIndex].LayoutLegacy = 1;
            dmNativeJoy[joystickIndex].NumAxes = axisCount;
            dmNativeJoy[joystickIndex].NumButtons = 0;
            dmNativeJoy[joystickIndex].AxisEventMask = 0;
            dmNativeJoy[joystickIndex].AxisInitialMask = 0;
            for (int i = 0; i < axisCount; ++i)
            {
                dmNativeJoy[joystickIndex].AxisIds[i] = (int)axes[i * 3];
                dmNativeJoy[joystickIndex].AxisMin[i] = axes[i * 3 + 1];
                dmNativeJoy[joystickIndex].AxisRange[i] = axes[i * 3 + 2];
                int axis = dmNativeAndroidDeviceAxisIndex(joystickIndex, i);
                dmNativeJoy[joystickIndex].Axis[axis] = axes[i * 3 + 1] + axes[i * 3 + 2] * 0.5f;
            }
            for (int i = 0; i < 32; ++i)
            {
                if (buttonMask & (1U << i))
                    dmNativeJoy[joystickIndex].NumButtons = i + 1;
            }
            if (buttonMask == 0xffffffffU)
                dmNativeJoy[joystickIndex].NumButtons = NATIVE_ANDROID_GAMEPAD_NUMBUTTONS;

            dmNativeWin.gamepadCallback(joystickIndex, 1);
            break;
        }
    }
    return joystickIndex;
}

void dmNativeAndroidSetJoystickLayoutLegacy(int joy, int legacy)
{
    dmNativeJoy[joy].LayoutLegacy = legacy;
}

void dmNativeAndroidInitializeJoystickAxes(int joy, const float* axes, int num_axes)
{
    if (num_axes > dmNativeJoy[joy].NumAxes)
        num_axes = dmNativeJoy[joy].NumAxes;
    // Mapping values are normalized; the shared state keeps physical device values.
    for (int i = 0; i < num_axes; ++i)
    {
        int axis = dmNativeAndroidDeviceAxisIndex(joy, i);
        uint64_t axis_bit = (uint64_t)1 << axis;
        if (dmNativeJoy[joy].AxisEventMask & axis_bit)
            continue;
        // Neutral axes start centered on connection; avoid resetting valid zero-valued input.
        if (axes[i] == 0.0f && !(dmNativeJoy[joy].AxisInitialMask & axis_bit))
            continue;
        dmNativeJoy[joy].Axis[axis] = dmNativeJoy[joy].AxisMin[i] + (axes[i] + 1.0f) * 0.5f * dmNativeJoy[joy].AxisRange[i];
        if (axes[i] != 0.0f)
            dmNativeJoy[joy].AxisInitialMask |= axis_bit;
        else
            dmNativeJoy[joy].AxisInitialMask &= ~axis_bit;
    }
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

static void dmNativeAndroidUpdateAxisValue(int joy, int axis, float value, int zero_is_endpoint)
{
    uint64_t axis_bit = (uint64_t)1 << axis;
    // Keep signed-axis startup zero placeholders from replacing mapping defaults.
    // A zero endpoint must be accepted, including a fully pressed inverted trigger.
    if (value == 0.0f && !zero_is_endpoint && (dmNativeJoy[joy].AxisInitialMask & axis_bit))
        return;
    dmNativeJoy[joy].Axis[axis] = value;
    if (value != 0.0f || zero_is_endpoint)
    {
        dmNativeJoy[joy].AxisEventMask |= axis_bit;
        dmNativeJoy[joy].AxisInitialMask &= ~axis_bit;
    }
}

static void dmNativeAndroidUpdateAxis(const AInputEvent* event)
{
    const int32_t joystickIndex = dmNativeAndroidFindJoystick(AInputEvent_getDeviceId(event));
    if (joystickIndex >= 0)
    {
        unsigned int updated_legacy_axes = 0;
        for (int i = 0; i < dmNativeJoy[joystickIndex].NumAxes; ++i)
        {
            int   axis = dmNativeAndroidDeviceAxisIndex(joystickIndex, i);
            float min = dmNativeJoy[joystickIndex].AxisMin[i];
            float max = min + dmNativeJoy[joystickIndex].AxisRange[i];
            int   zero_is_endpoint = min == 0.0f || max == 0.0f;
            dmNativeAndroidUpdateAxisValue(joystickIndex, axis, AMotionEvent_getAxisValue(event, dmNativeJoy[joystickIndex].AxisIds[i], 0), zero_is_endpoint);
            if (axis < NATIVE_ANDROID_GAMEPAD_NUMAXIS)
                updated_legacy_axes |= 1U << axis;
        }
        // Preserve fixed legacy slots for axes absent from the advertised ranges.
        for (int i = 0; i < NATIVE_ANDROID_GAMEPAD_NUMAXIS; ++i)
        {
            if (!(updated_legacy_axes & (1U << i)))
                dmNativeAndroidUpdateAxisValue(joystickIndex, i, AMotionEvent_getAxisValue(event, ANDROID_LEGACY_AXES[i], 0), 0);
        }
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

static int dmNativeAndroidUpdateButton(const AInputEvent* event)
{
    const int32_t keyCode = AKeyEvent_getKeyCode(event);
    const int32_t joystickIndex = dmNativeAndroidFindJoystick(AInputEvent_getDeviceId(event));
    const int32_t action = AKeyEvent_getAction(event);
    if (joystickIndex >= 0)
    {
        int button = dmNativeAndroidKeycodeToJoystickButton(keyCode);
        if (keyCode == AKEYCODE_BACK)
            button = ANDROID_BUTTON_BACK;
        else if (keyCode == AKEYCODE_MENU)
            button = ANDROID_BUTTON_MENU;
        else if (keyCode == AKEYCODE_MEDIA_RECORD)
            button = ANDROID_BUTTON_RECORD;
        if (button >= 0)
        {
            dmNativeJoy[joystickIndex].Button[button] = (action == AKEY_EVENT_ACTION_DOWN) ? NATIVE_PRESS : NATIVE_RELEASE;
            return 1;
        }
    }
    return 0;
}


//========================================================================
// Update joystick dpad
//========================================================================

static void dmNativeAndroidUpdateDpad(const AInputEvent* event)
{
    const int32_t joystickIndex = dmNativeAndroidFindJoystick(AInputEvent_getDeviceId(event));
    if (joystickIndex >= 0)
    {
        dmNativeJoy[joystickIndex].Hats = NATIVE_HAT_CENTERED;
        float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
        if (hatX == 1.0f)
        {
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_RIGHT;
        }
        else if (hatX == -1.0f)
        {
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_LEFT;
        }
        float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);
        if (hatY == 1.0f)
        {
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_DOWN;
        }
        else if (hatY == -1.0f)
        {
            dmNativeJoy[joystickIndex].Hats |= NATIVE_HAT_UP;
        }
        dmNativeJoy[joystickIndex].Axis[6] = hatX;
        dmNativeJoy[joystickIndex].Axis[7] = hatY;
    }
}


//========================================================================
// Update joystick from input event
// Called from android_init.c when input events are processed
//========================================================================

int dmNativeAndroidUpdateJoystick(const AInputEvent* event)
{
    const int32_t source = AInputEvent_getSource(event);
    const int32_t type = AInputEvent_getType(event);
    const int32_t action = AKeyEvent_getAction(event);

    if ((source & AINPUT_SOURCE_GAMEPAD) != 0)
    {
        if (type == AINPUT_EVENT_TYPE_KEY)
        {
            return dmNativeAndroidUpdateButton(event);
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
    return 0;
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
        jmethodID get_game_controller_device_button_mask = 0;
        jmethodID get_game_controller_device_axes = 0;
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
        get_game_controller_device_button_mask = JNIGetMethodID(env, native_activity, "getGameControllerDeviceButtonMask", "(I)I");
        if (get_game_controller_device_button_mask == 0)
            goto cleanup_and_early_exit;
        get_game_controller_device_axes = JNIGetMethodID(env, native_activity, "getGameControllerDeviceAxes", "(I)[F");
        if (get_game_controller_device_axes == 0)
            goto cleanup_and_early_exit;

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
                jfloatArray jni_axes = 0;
                float axes[NATIVE_ANDROID_GAMEPAD_MAX_AXES * 3];
                int axis_count = 0;
                unsigned int axis_mask = 0;
                unsigned int button_mask = 0;
                int has_z = 0;
                int has_rx_ry = 0;
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

                button_mask = (unsigned int)(*env)->CallIntMethod(env, native_activity, get_game_controller_device_button_mask, jni_device_id);
                if (JNICheckAndClearException(env))
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }
                jni_axes = (*env)->CallObjectMethod(env, native_activity, get_game_controller_device_axes, jni_device_id);
                if (JNICheckAndClearException(env) || jni_axes == 0)
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }
                axis_count = (*env)->GetArrayLength(env, jni_axes) / 3;
                if (axis_count > NATIVE_ANDROID_GAMEPAD_MAX_AXES)
                    axis_count = NATIVE_ANDROID_GAMEPAD_MAX_AXES;
                (*env)->GetFloatArrayRegion(env, jni_axes, 0, axis_count * 3, axes);
                if (JNICheckAndClearException(env))
                {
                    discovery_succeeded = 0;
                    goto cleanup_and_break;
                }
                if (axis_count >= 2) axis_mask |= 0x03;
                if (axis_count >= 4) axis_mask |= 0x0c;
                if (axis_count >= 6) axis_mask |= 0x30;
                for (int axis = 0; axis < axis_count; ++axis)
                {
                    int id = (int)axes[axis * 3];
                    has_z |= id == AMOTION_EVENT_AXIS_Z;
                    has_rx_ry |= id > AMOTION_EVENT_AXIS_Z && id < AMOTION_EVENT_AXIS_RZ;
                }
                if (has_z && has_rx_ry)
                    axis_mask |= 0x8000;

                char deviceGuid[DEVICE_GUID_LENGTH + 1];
                dmNativeAndroidCreateJoystickDeviceGuid((unsigned short)vendor_id, (unsigned short)product_id,
                    deviceDescriptor, button_mask, axis_mask, deviceGuid);
                deviceIndex = dmNativeAndroidConnectJoystick(deviceId, deviceName, deviceGuid, button_mask, axes, axis_count);
cleanup_and_break:
                if (jni_axes != 0)
                    (*env)->DeleteLocalRef(env, jni_axes);
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
        return dmNativeJoy[joy].LayoutLegacy ? NATIVE_ANDROID_GAMEPAD_NUMAXIS : dmNativeJoy[joy].NumAxes;

    case NATIVE_BUTTONS:
        if (dmNativeJoy[joy].LayoutLegacy)
            return NATIVE_ANDROID_GAMEPAD_NUMBUTTONS;
        // Reserve Record's alias slot without adding it to SDL's GUID capability mask.
        return dmNativeJoy[joy].NumButtons > ANDROID_SDL_BUTTON_RECORD
            ? dmNativeJoy[joy].NumButtons : ANDROID_SDL_BUTTON_RECORD + 1;

    case NATIVE_HATS:
        return dmNativeJoy[joy].LayoutLegacy ? 1 : 0;

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
    int axis_count = dmNativeOSGetJoystickParam(joy, NATIVE_AXES);
    if( axis_count < numaxes )
    {
        numaxes = axis_count;
    }

    // Store physical values once; the selected layout controls order and normalization.
    for( i = 0; i < numaxes; ++ i )
    {
        if (dmNativeJoy[joy].LayoutLegacy)
        {
            // Mapping initialization must not seed the legacy axes before actual motion.
            pos[i] = (dmNativeJoy[joy].AxisEventMask & ((uint64_t)1 << i)) || i >= 6 ? dmNativeJoy[joy].Axis[i] : 0.0f;
        }
        else
        {
            int axis = dmNativeAndroidDeviceAxisIndex(joy, i);
            float range = dmNativeJoy[joy].AxisRange[i];
            pos[i] = range > 0.0f
                ? (dmNativeJoy[joy].Axis[axis] - dmNativeJoy[joy].AxisMin[i]) / range * 2.0f - 1.0f : 0.0f;
        }
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
    int button_count = dmNativeOSGetJoystickParam(joy, NATIVE_BUTTONS);
    if( button_count < numbuttons )
    {
        numbuttons = button_count;
    }

    if (dmNativeJoy[joy].LayoutLegacy)
    {
        memcpy(buttons, dmNativeJoy[joy].Button, numbuttons);
    }
    else
    {
        // SDL slots merge key aliases and hat directions without duplicating stored state.
        static const unsigned char legacy_to_sdl[] = { 0, 1, 17, 2, 9, 10, 3, 18, 15, 16, 0, 12, 13, 14, 11, 6, 4, 7, 8, 5 };
        memset(buttons, 0, numbuttons);
        for (i = 0; i < NATIVE_ANDROID_GAMEPAD_NUMBUTTONS + 3; ++i)
        {
            int button = i;
            if (i < 20)
                button = legacy_to_sdl[i];
            else if (i == ANDROID_BUTTON_BACK)
                button = 4;
            else if (i == ANDROID_BUTTON_MENU)
                button = 6;
            else if (i == ANDROID_BUTTON_RECORD)
                button = ANDROID_SDL_BUTTON_RECORD;
            if (button < numbuttons)
                buttons[button] |= dmNativeJoy[joy].Button[i];
        }
        static const unsigned char hat_masks[] = { NATIVE_HAT_UP, NATIVE_HAT_DOWN, NATIVE_HAT_LEFT, NATIVE_HAT_RIGHT };
        for (i = 0; i < 4; ++i)
        {
            if (11 + i < numbuttons && (dmNativeJoy[joy].Hats & hat_masks[i]))
                buttons[11 + i] = NATIVE_PRESS;
        }
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
    if (dmNativeJoy[joy].LayoutLegacy && numhats > 0)
    {
        hats[0] |= dmNativeJoy[joy].Hats;
        return 1;
    }
    return 0;
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
