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

#include <string.h>
#include <stdlib.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

extern "C"
{
#include "../native/internal.h"
#include "../native/android/android_joystick.h"
    int32_t                   dmNativeAndroidHandleInput(struct android_app* app, AInputEvent* event);
    extern struct InputEvent* g_AppInputEvents;
    extern int                g_MaxAppInputEvents;
    extern int                g_NumAppInputEvents;
}
#include "../platform_window_android.h"

struct TestInputEvent
{
    int   m_Type;
    int   m_Source;
    int   m_Action;
    int   m_KeyCode;
    float m_Axes[48];
};

// CMake's --wrap=<symbol> redirects the backend's Android event accessor calls
// to these __wrap_<symbol> functions. The fixture passes TestInputEvent as the
// opaque AInputEvent, so the real joystick and input queue code can process
// synthetic key and motion events without reading Android's live event queue.
extern "C" int32_t __wrap_AInputEvent_getDeviceId(const AInputEvent*)
{
    return 10;
}
extern "C" int32_t __wrap_AInputEvent_getType(const AInputEvent* event)
{
    return ((const TestInputEvent*)event)->m_Type;
}
extern "C" int32_t __wrap_AInputEvent_getSource(const AInputEvent* event)
{
    return ((const TestInputEvent*)event)->m_Source;
}
extern "C" int32_t __wrap_AKeyEvent_getKeyCode(const AInputEvent* event)
{
    return ((const TestInputEvent*)event)->m_KeyCode;
}
extern "C" int32_t __wrap_AKeyEvent_getAction(const AInputEvent* event)
{
    return ((const TestInputEvent*)event)->m_Action;
}
extern "C" int32_t __wrap_AKeyEvent_getFlags(const AInputEvent*)
{
    return 0;
}
extern "C" int32_t __wrap_AKeyEvent_getMetaState(const AInputEvent*)
{
    return 0;
}
extern "C" int32_t __wrap_AKeyEvent_getScanCode(const AInputEvent*)
{
    return 0;
}
extern "C" int32_t __wrap_AKeyEvent_getRepeatCount(const AInputEvent*)
{
    return 0;
}
extern "C" int64_t __wrap_AKeyEvent_getDownTime(const AInputEvent*)
{
    return 0;
}
extern "C" int64_t __wrap_AKeyEvent_getEventTime(const AInputEvent*)
{
    return 0;
}
extern "C" int32_t __wrap_AMotionEvent_getAction(const AInputEvent* event)
{
    return ((const TestInputEvent*)event)->m_Action;
}
extern "C" float __wrap_AMotionEvent_getAxisValue(const AInputEvent* event, int32_t axis, size_t)
{
    return ((const TestInputEvent*)event)->m_Axes[axis];
}

class AndroidJoystickTest : public jc_test_base_class
{
    protected:
    void SetUp()
    {
        memset(dmNativeJoy, 0, sizeof(dmNativeJoy));
        dmNativeJoy[0].State = NATIVE_ANDROID_GAMEPAD_CONNECTED;
        dmNativeJoy[0].DeviceId = 10;
        dmNativeJoy[0].NumButtons = NATIVE_ANDROID_GAMEPAD_NUMBUTTONS;
        dmNativeJoy[0].NumAxes = 6;
        const int ids[] = { AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y, AMOTION_EVENT_AXIS_RX, AMOTION_EVENT_AXIS_RY, AMOTION_EVENT_AXIS_Z, AMOTION_EVENT_AXIS_RZ };
        for (int i = 0; i < 6; ++i)
        {
            dmNativeJoy[0].AxisIds[i] = ids[i];
            dmNativeJoy[0].AxisMin[i] = -1.0f;
            dmNativeJoy[0].AxisRange[i] = 2.0f;
        }
        memset(&m_Event, 0, sizeof(m_Event));
    }

    void TearDown()
    {
        free(g_AppInputEvents);
        g_AppInputEvents = 0;
        g_MaxAppInputEvents = 0;
        g_NumAppInputEvents = 0;
    }

    void SendKey(int key, int action)
    {
        m_Event.m_Type = AINPUT_EVENT_TYPE_KEY;
        m_Event.m_Source = AINPUT_SOURCE_GAMEPAD;
        m_Event.m_KeyCode = key;
        m_Event.m_Action = action;
        dmNativeAndroidUpdateJoystick((const AInputEvent*)&m_Event);
    }

    void SendMotion()
    {
        m_Event.m_Type = AINPUT_EVENT_TYPE_MOTION;
        m_Event.m_Source = AINPUT_SOURCE_JOYSTICK;
        m_Event.m_Action = AMOTION_EVENT_ACTION_MOVE;
        dmNativeAndroidUpdateJoystick((const AInputEvent*)&m_Event);
    }

    TestInputEvent m_Event;
};

// Verifies the measured Bluetooth DualSense GUID includes SDL's capability masks and byte order.
TEST(AndroidJoystickGuid, DualSenseCapabilities)
{
    char guid[33];
    dmNativeAndroidCreateJoystickDeviceGuid(0x054c, 0x0ce6, "536c4f12bd89b3918b7c1364c0eef6ffb9045255", 0x0007feff, 0x803f, guid);
    ASSERT_STREQ("0500eec34c050000e60c0000fffe3f80", guid);
}

// Verifies capability words also replace the name tail when Android cannot report USB ids.
TEST(AndroidJoystickGuid, CapabilitiesWithoutVendor)
{
    char base[33];
    char guid[33];
    dmNativeCreateJoystickDeviceGuid(5, 0, 0, 0, 0, "Android Controller", 0, 0, base);
    dmNativeAndroidCreateJoystickDeviceGuid(0, 0, "Android Controller", 0xffff, 3, guid);
    ASSERT_EQ(0, strncmp(base, guid, 24));
    ASSERT_STREQ("ffff0300", guid + 24);
}

// Verifies database button slots match SDL while switching back preserves legacy mappings and releases.
TEST_F(AndroidJoystickTest, ButtonLayouts)
{
    const int cases[][3] = {
        { AKEYCODE_BUTTON_A, 0, 0 }, { AKEYCODE_BUTTON_B, 1, 1 }, { AKEYCODE_BUTTON_C, 2, 17 }, { AKEYCODE_BUTTON_X, 3, 2 }, { AKEYCODE_BUTTON_Y, 6, 3 }, { AKEYCODE_BUTTON_Z, 7, 18 }, { AKEYCODE_BUTTON_L1, 4, 9 }, { AKEYCODE_BUTTON_R1, 5, 10 }, { AKEYCODE_BUTTON_L2, 8, 15 }, { AKEYCODE_BUTTON_R2, 9, 16 }, { AKEYCODE_BUTTON_START, 15, 6 }, { AKEYCODE_BUTTON_SELECT, 16, 4 }, { AKEYCODE_BUTTON_THUMBL, 17, 7 }, { AKEYCODE_BUTTON_THUMBR, 18, 8 }, { AKEYCODE_BUTTON_MODE, 19, 5 }, { AKEYCODE_BUTTON_12, 31, 31 }
    };
    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        SendKey(cases[i][0], AKEY_EVENT_ACTION_DOWN);
        unsigned char buttons[36] = {};
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
        ASSERT_EQ(36, dmNativeOSGetJoystickButtons(0, buttons, 36));
        ASSERT_EQ(1, buttons[cases[i][1]]);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        for (int button = 0; button < 36; ++button)
            ASSERT_EQ(button == cases[i][2] ? 1 : 0, buttons[button]);
        SendKey(cases[i][0], AKEY_EVENT_ACTION_UP);
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(0, buttons[cases[i][2]]);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(0, buttons[cases[i][1]]);
    }
}

// Verifies the input queue acknowledges gamepad keys, preventing Android's fallback Square/Back presses.
TEST_F(AndroidJoystickTest, GamepadKeysAreHandled)
{
    const int keys[] = { AKEYCODE_BUTTON_A, AKEYCODE_BUTTON_B, AKEYCODE_BUTTON_C, AKEYCODE_BUTTON_Z, AKEYCODE_BUTTON_START };
    m_Event.m_Type = AINPUT_EVENT_TYPE_KEY;
    m_Event.m_Source = AINPUT_SOURCE_GAMEPAD;
    for (unsigned int i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        m_Event.m_KeyCode = keys[i];
        m_Event.m_Action = AKEY_EVENT_ACTION_DOWN;
        ASSERT_EQ(1, dmNativeAndroidHandleInput(0, (AInputEvent*)&m_Event));
        m_Event.m_Action = AKEY_EVENT_ACTION_UP;
        ASSERT_EQ(1, dmNativeAndroidHandleInput(0, (AInputEvent*)&m_Event));
    }
    m_Event.m_KeyCode = AKEYCODE_A;
    ASSERT_EQ(0, dmNativeAndroidHandleInput(0, (AInputEvent*)&m_Event));
}

// Verifies Android Back/Menu aliases are available to SDL mappings without changing legacy button state.
TEST_F(AndroidJoystickTest, ButtonAliases)
{
    const int keys[] = { AKEYCODE_BACK, AKEYCODE_MENU, AKEYCODE_DPAD_CENTER, AKEYCODE_MEDIA_RECORD };
    const int slots[] = { 4, 6, 0, 15 };
    for (int i = 0; i < 4; ++i)
    {
        SendKey(keys[i], AKEY_EVENT_ACTION_DOWN);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
        unsigned char buttons[36] = {};
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(1, buttons[slots[i]]);
        SendKey(keys[i], AKEY_EVENT_ACTION_UP);
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(0, buttons[slots[i]]);
    }
}

// Verifies the SDL count includes Record's alias when the capability mask only exposes slots 0 through 14.
TEST_F(AndroidJoystickTest, RecordAliasWithCapabilityCount)
{
    dmNativeJoy[0].NumButtons = 15; // The SDL capability mask 0x7fff does not include MEDIA_RECORD.
    dmNativeAndroidCreateJoystickDeviceGuid(0x045e, 0x0b12, "Xbox Controller", 0x7fff, 0x3f, dmNativeJoy[0].DeviceGuid);
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    ASSERT_STREQ("ff7f3f00", dmNativeJoy[0].DeviceGuid + 24);
    ASSERT_EQ(16, dmNativeOSGetJoystickParam(0, NATIVE_BUTTONS));
    unsigned char buttons[17];
    memset(buttons, 0x7b, sizeof(buttons));
    SendKey(AKEYCODE_MEDIA_RECORD, AKEY_EVENT_ACTION_DOWN);
    ASSERT_EQ(16, dmNativeOSGetJoystickButtons(0, buttons, sizeof(buttons)));
    for (int i = 0; i < 15; ++i)
        ASSERT_EQ(0, buttons[i]);
    ASSERT_EQ(1, buttons[15]);
    ASSERT_EQ(0x7b, buttons[16]);
    SendKey(AKEYCODE_MEDIA_RECORD, AKEY_EVENT_ACTION_UP);
    ASSERT_EQ(16, dmNativeOSGetJoystickButtons(0, buttons, sizeof(buttons)));
    ASSERT_EQ(0, buttons[15]);
}

// Verifies releasing one alias preserves another held key that maps to the same SDL button.
TEST_F(AndroidJoystickTest, HeldButtonAliases)
{
    const int keys[][3] = {
        { AKEYCODE_BUTTON_A, AKEYCODE_DPAD_CENTER, 0 },
        { AKEYCODE_BUTTON_SELECT, AKEYCODE_BACK, 4 },
        { AKEYCODE_BUTTON_START, AKEYCODE_MENU, 6 },
        { AKEYCODE_BUTTON_L2, AKEYCODE_MEDIA_RECORD, 15 },
    };
    for (uint32_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
    {
        SendKey(keys[i][0], AKEY_EVENT_ACTION_DOWN);
        SendKey(keys[i][1], AKEY_EVENT_ACTION_DOWN);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
        unsigned char buttons[36] = {};
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(1, buttons[keys[i][2]]);
        SendKey(keys[i][1], AKEY_EVENT_ACTION_UP);
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(1, buttons[keys[i][2]]);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
        SendKey(keys[i][0], AKEY_EVENT_ACTION_UP);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
        dmNativeOSGetJoystickButtons(0, buttons, 36);
        ASSERT_EQ(0, buttons[keys[i][2]]);
    }
}

// Verifies RX/RY triggers and SDL's sorted axes coexist with the unchanged eight-axis legacy packet.
TEST_F(AndroidJoystickTest, AxisLayouts)
{
    m_Event.m_Axes[AMOTION_EVENT_AXIS_X] = 0.25f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_Y] = -0.5f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = -1.0f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RY] = 0.5f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_Z] = 0.75f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RZ] = -0.25f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_LTRIGGER] = 0.4f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RTRIGGER] = 0.8f;
    SendMotion();
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    float values[8] = {};
    ASSERT_EQ(6, dmNativeOSGetJoystickParam(0, NATIVE_AXES));
    dmNativeOSGetJoystickPos(0, values, 8);
    const float sdl[] = { 0.25f, -0.5f, -1.0f, 0.5f, 0.75f, -0.25f };
    for (int i = 0; i < 6; ++i)
        ASSERT_EQ(sdl[i], values[i]);
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
    ASSERT_EQ(8, dmNativeOSGetJoystickParam(0, NATIVE_AXES));
    dmNativeOSGetJoystickPos(0, values, 8);
    const float legacy[] = { 0.25f, -0.5f, 0.75f, -0.25f, 0.4f, 0.8f, 0.0f, 0.0f };
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ(legacy[i], values[i]);
}

// Verifies mapped trigger rest values apply before motion and never overwrite actual controller input.
TEST_F(AndroidJoystickTest, InitialAxisValues)
{
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    const float initial[] = { 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    for (int i = 0; i < 6; ++i)
        ASSERT_EQ(initial[i], values[i]);

    dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
    dmNativeOSGetJoystickPos(0, values, 6);
    for (int i = 0; i < 6; ++i)
        ASSERT_EQ(0.0f, values[i]);
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    dmNativeOSGetJoystickPos(0, values, 6);
    for (int i = 0; i < 6; ++i)
        ASSERT_EQ(initial[i], values[i]);

    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = 0.25f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RY] = 0.75f;
    SendMotion();
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(0.25f, values[2]);
    ASSERT_EQ(0.75f, values[3]);
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(0.0f, values[2]);
    ASSERT_EQ(0.0f, values[3]);
}

// Verifies stick motion cannot replace untouched triggers with Android's initial zero placeholders.
TEST_F(AndroidJoystickTest, StickMotionPreservesInitialTriggers)
{
    const float initial[] = { 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    m_Event.m_Axes[AMOTION_EVENT_AXIS_Y] = -1.0f;
    SendMotion();
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[1]);
    ASSERT_EQ(-1.0f, values[2]);
    ASSERT_EQ(-1.0f, values[3]);

    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = -0.75f;
    SendMotion();
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-0.75f, values[2]);
    ASSERT_EQ(-1.0f, values[3]);
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = 0.0f;
    SendMotion();
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(0.0f, values[2]);
    ASSERT_EQ(-1.0f, values[3]);

    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(0.0f, values[2]);
    ASSERT_EQ(-1.0f, values[3]);

    dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[1]);
    ASSERT_EQ(0.0f, values[2]);
    ASSERT_EQ(0.0f, values[3]);
}

// Verifies trigger initialization still applies after another axis moves, without resetting that axis.
TEST_F(AndroidJoystickTest, MappingAfterStickMotionInitializesTriggers)
{
    m_Event.m_Axes[AMOTION_EVENT_AXIS_Y] = -1.0f;
    SendMotion();
    const float initial[] = { 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f };
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[1]);
    ASSERT_EQ(-1.0f, values[2]);
    ASSERT_EQ(-1.0f, values[3]);
}

// Verifies mapping initialization preserves zero-valued input on unsigned axes in legacy and device-only storage.
TEST_F(AndroidJoystickTest, ZeroAxisSamplesSurviveMappingInitialization)
{
    dmNativeJoy[0].AxisMin[0] = 0.0f;
    dmNativeJoy[0].AxisRange[0] = 255.0f;
    dmNativeJoy[0].AxisMin[2] = 0.0f;
    dmNativeJoy[0].AxisRange[2] = 255.0f;
    // Match the physical centers established when the controller connects.
    dmNativeJoy[0].Axis[0] = 127.5f;
    dmNativeJoy[0].Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + 2] = 127.5f;
    SendMotion();
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[0]);
    ASSERT_EQ(-1.0f, values[2]);

    const float initial[] = { 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f };
    for (int i = 0; i < 2; ++i)
    {
        dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
        dmNativeOSGetJoystickPos(0, values, 6);
        ASSERT_EQ(-1.0f, values[0]);
        ASSERT_EQ(-1.0f, values[2]);
        ASSERT_EQ(-1.0f, values[3]);
        SendMotion();
        dmNativeOSGetJoystickPos(0, values, 6);
        ASSERT_EQ(-1.0f, values[0]);
        ASSERT_EQ(-1.0f, values[2]);
        ASSERT_EQ(-1.0f, values[3]);
    }
}

// Verifies removing signed or inverted trigger defaults restores neutral without treating placeholder zeros as input.
TEST_F(AndroidJoystickTest, NeutralMappingReplacesPendingTriggerDefaults)
{
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    const float initial[] = { 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f };
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    SendMotion();
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[2]);
    ASSERT_EQ(1.0f, values[3]);

    const float neutral[6] = {};
    dmPlatform::InitializeAndroidJoystickAxes(0, neutral, 6);
    SendMotion();
    dmNativeOSGetJoystickPos(0, values, 6);
    for (int i = 0; i < 6; ++i)
        ASSERT_EQ(0.0f, values[i]);

    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[2]);
    ASSERT_EQ(1.0f, values[3]);
}

// Verifies first zero endpoints reach inverted and normal triggers without exposing signed-axis startup placeholders.
TEST_F(AndroidJoystickTest, ZeroEndpointsPreserveFirstTriggerPress)
{
    dmNativeJoy[0].AxisMin[2] = 0.0f;
    dmNativeJoy[0].AxisRange[2] = 1.0f;
    dmNativeJoy[0].AxisRange[4] = 1.0f;
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    const float initial[] = { 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f };
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(1.0f, values[2]);
    ASSERT_EQ(-1.0f, values[4]);

    for (int i = 0; i < 2; ++i)
    {
        SendMotion();
        dmNativeOSGetJoystickPos(0, values, 6);
        ASSERT_EQ(-1.0f, values[2]);
        ASSERT_EQ(1.0f, values[4]);
        ASSERT_EQ(-1.0f, values[3]);
        dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
        dmNativeOSGetJoystickPos(0, values, 6);
        ASSERT_EQ(-1.0f, values[2]);
        ASSERT_EQ(1.0f, values[4]);
    }

    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = 1.0f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_Z] = -1.0f;
    SendMotion();
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(1.0f, values[2]);
    ASSERT_EQ(-1.0f, values[4]);
}

// Verifies zero endpoints received before mapping initialization survive defaults in legacy and device-only slots.
TEST_F(AndroidJoystickTest, ZeroEndpointsBeforeMappingInitialization)
{
    dmNativeJoy[0].AxisMin[2] = 0.0f;
    dmNativeJoy[0].AxisRange[2] = 1.0f;
    dmNativeJoy[0].AxisRange[4] = 1.0f;
    dmNativeJoy[0].Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + 2] = 0.5f;
    dmNativeJoy[0].Axis[2] = -0.5f;
    SendMotion();
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    const float initial[] = { 0.0f, 0.0f, 1.0f, -1.0f, -1.0f, 0.0f };
    dmPlatform::InitializeAndroidJoystickAxes(0, initial, 6);
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[2]);
    ASSERT_EQ(1.0f, values[4]);
    ASSERT_EQ(-1.0f, values[3]);
}

// Verifies SDL normalization uses the advertised motion range rather than assuming every axis is -1..1.
TEST_F(AndroidJoystickTest, AxisNormalization)
{
    dmNativeJoy[0].AxisMin[2] = 0.0f;
    dmNativeJoy[0].AxisRange[2] = 255.0f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = 255.0f;
    SendMotion();
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    float values[6] = {};
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(1.0f, values[2]);
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = 0.0f;
    SendMotion();
    dmNativeOSGetJoystickPos(0, values, 6);
    ASSERT_EQ(-1.0f, values[2]);
}

// Verifies layout switches retain physical axis values and normalize only the SDL view.
TEST_F(AndroidJoystickTest, HeldAxesAcrossLayouts)
{
    dmNativeJoy[0].AxisMin[4] = 0.0f;
    dmNativeJoy[0].AxisRange[4] = 255.0f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_Z] = 127.5f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_RX] = -0.75f;
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
    SendMotion();
    for (int i = 0; i < 3; ++i)
    {
        float values[8] = {};
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
        dmNativeOSGetJoystickPos(0, values, 8);
        ASSERT_EQ(-0.75f, values[2]);
        ASSERT_EQ(0.0f, values[4]);
        dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
        dmNativeOSGetJoystickPos(0, values, 8);
        ASSERT_EQ(127.5f, values[2]);
    }
    ASSERT_EQ(127.5f, dmNativeJoy[0].Axis[2]);
}

// Verifies hat diagonals and centering become SDL buttons while legacy mappings retain their hat,
// and that key-based D-pad presses survive stick motion.
TEST_F(AndroidJoystickTest, HatLayouts)
{
    m_Event.m_Axes[AMOTION_EVENT_AXIS_HAT_X] = 1.0f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_HAT_Y] = -1.0f;
    SendMotion();
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    ASSERT_EQ(0, dmNativeOSGetJoystickParam(0, NATIVE_HATS));
    unsigned char buttons[36] = {};
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(1, buttons[11]);
    ASSERT_EQ(1, buttons[14]);
    ASSERT_EQ(0, buttons[12]);
    ASSERT_EQ(0, buttons[13]);
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, true);
    unsigned char hats[1] = {};
    ASSERT_EQ(1, dmNativeOSGetJoystickHats(0, hats, 1));
    ASSERT_EQ(NATIVE_HAT_UP | NATIVE_HAT_RIGHT, hats[0]);
    m_Event.m_Axes[AMOTION_EVENT_AXIS_HAT_X] = 0.0f;
    m_Event.m_Axes[AMOTION_EVENT_AXIS_HAT_Y] = 0.0f;
    SendMotion();
    dmPlatform::SetAndroidJoystickLayoutLegacy(0, false);
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    for (int i = 11; i <= 14; ++i)
        ASSERT_EQ(0, buttons[i]);
    SendKey(AKEYCODE_DPAD_DOWN, AKEY_EVENT_ACTION_DOWN);
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(1, buttons[12]);
    SendMotion();
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(1, buttons[12]);
    SendKey(AKEYCODE_DPAD_DOWN, AKEY_EVENT_ACTION_UP);
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(0, buttons[12]);

    SendKey(AKEYCODE_DPAD_UP, AKEY_EVENT_ACTION_DOWN);
    m_Event.m_Axes[AMOTION_EVENT_AXIS_HAT_X] = 1.0f;
    SendMotion();
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(1, buttons[11]);
    ASSERT_EQ(1, buttons[14]);
    m_Event.m_Axes[AMOTION_EVENT_AXIS_HAT_X] = 0.0f;
    SendMotion();
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(1, buttons[11]);
    ASSERT_EQ(0, buttons[14]);
    SendKey(AKEYCODE_DPAD_UP, AKEY_EVENT_ACTION_UP);
    dmNativeOSGetJoystickButtons(0, buttons, 36);
    ASSERT_EQ(0, buttons[11]);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
