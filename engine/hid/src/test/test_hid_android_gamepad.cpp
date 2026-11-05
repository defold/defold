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
#include <stdio.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include "../hid_private.h"
#include "../native/hid_native_private.h"
#include <dlib/hash.h>
#include <input.h>
#include <input_private.h>
#include <platform/window.hpp>
#include <platform/platform_window_constants.h>

extern "C"
{
#include "internal.h"
}

// CMake links this test with --wrap=<symbol>, redirecting calls from the real
// backend to __wrap_<symbol>. __real_<symbol> calls the original implementation.
// This lets the tests control the Android SDK version and window lifecycle
// without creating an Android app surface or polling its live event queue.
static int g_AndroidSDKVersion = 29;
// On our minimum SDK, android_get_device_api_level() reads this system property.
// Override only the SDK version and forward all other properties to Android.
extern "C" int __real___system_property_get(const char*, char*);
extern "C" int __wrap___system_property_get(const char* name, char* value)
{
    if (strcmp(name, "ro.build.version.sdk") == 0)
        return snprintf(value, 92, "%d", g_AndroidSDKVersion);
    return __real___system_property_get(name, value);
}
// The fixture supplies joystick state, so skip Android app initialization and termination.
extern "C" int __wrap_dmNativeOSInit() { return 1; }
extern "C" int __wrap_dmNativeOSTerminate() { return 1; }
// These C++ symbols are dmPlatform::GetWindowStateParam and dmPlatform::PollEvents.
// Report an open window and leave event delivery under the fixture's control.
extern "C" uint32_t __wrap__ZN10dmPlatform19GetWindowStateParamEP8dmWindow11WindowState(HWindow, WindowState state)
{
    return state == WINDOW_STATE_OPENED;
}
extern "C" void __wrap__ZN10dmPlatform10PollEventsEP8dmWindow(HWindow) {}

class AndroidGamepadTest : public jc_test_base_class
{
protected:
    void SetUp()
    {
        g_AndroidSDKVersion = 29;
        m_Window = dmPlatform::NewWindow();
        ASSERT_NE((void*)0, (void*)m_Window);
        memset(dmNativeJoy, 0, sizeof(dmNativeJoy));
        dmNativeJoy[0].State = 1;
        dmNativeJoy[0].DeviceId = 11;
        dmNativeJoy[0].LayoutLegacy = 1;
        dmNativeJoy[0].NumAxes = 7;
        dmNativeJoy[0].NumButtons = 16;
        const int axis_ids[] = { AMOTION_EVENT_AXIS_X, AMOTION_EVENT_AXIS_Y, AMOTION_EVENT_AXIS_Z, AMOTION_EVENT_AXIS_RZ,
                                 AMOTION_EVENT_AXIS_BRAKE, AMOTION_EVENT_AXIS_GAS, AMOTION_EVENT_AXIS_GENERIC_1 };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(axis_ids); ++i)
        {
            dmNativeJoy[0].AxisIds[i] = axis_ids[i];
            dmNativeJoy[0].AxisMin[i] = -1.0f;
            dmNativeJoy[0].AxisRange[i] = 2.0f;
        }
        strcpy(dmNativeJoy[0].DeviceGuid, "0500da4ac82d00001b300000ffff3f00");
        strcpy(dmNativeJoy[0].DeviceName, "8BitDo Ultimate 2C Wireless");
        dmHID::NewContextParams params;
        params.m_IgnoreKeyboard = 1;
        params.m_IgnoreMouse = 1;
        params.m_IgnoreTouchDevice = 1;
        params.m_IgnoreAcceleration = 1;
        m_Context = dmHID::NewContext(params);
        dmHID::SetWindow(m_Context, m_Window);
        ASSERT_TRUE(dmHID::Init(m_Context));
        m_Gamepad = dmHID::GetGamepad(m_Context, 0);
    }

    void TearDown()
    {
        dmHID::Final(m_Context);
        dmHID::DeleteContext(m_Context);
        dmPlatform::DeleteWindow(m_Window);
    }

    void SelectAutomaticMapping()
    {
        ASSERT_EQ((uint32_t)dmHID::GAMEPAD_MAPPING_SUPPORT_AUTOMATIC, dmHID::GetGamepadMappingSupport(m_Context, m_Gamepad));
        dmHID::SetGamepadLayoutLegacy(m_Gamepad, false);
        dmHID::SetGamepadMapping(m_Context, m_Gamepad, 0);
    }

    HWindow m_Window;
    dmHID::HContext m_Context;
    dmHID::HGamepad m_Gamepad;
};

// Verifies Android capabilities enable the automatic fallback only for supported generic gamepads.
TEST_F(AndroidGamepadTest, MappingSupport)
{
    ASSERT_EQ((uint32_t)dmHID::GAMEPAD_MAPPING_SUPPORT_AUTOMATIC, dmHID::GetGamepadMappingSupport(m_Context, m_Gamepad));
    const char* unsupported[] = {
        "05000000341200007856000000003f00", // No known face buttons.
        "050000007e05000006200000ffff3f00", // Nintendo requires a dedicated profile.
        "030000003412000078560000ffff3f00", // Not an Android capability GUID.
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(unsupported); ++i)
    {
        strcpy(dmNativeJoy[0].DeviceGuid, unsupported[i]);
        ASSERT_EQ((uint32_t)dmHID::GAMEPAD_MAPPING_SUPPORT_NONE, dmHID::GetGamepadMappingSupport(m_Context, m_Gamepad));
    }
}

// Verifies SDL Android button slots become the existing canonical HID buttons and hat without duplicate presses.
TEST_F(AndroidGamepadTest, AutomaticButtons)
{
    SelectAutomaticMapping();
    const uint8_t raw_buttons[] = { 0, 1, 3, 6, 16, 15, 17, 18, 4, 5 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(raw_buttons); ++i)
    {
        dmNativeJoy[0].Button[raw_buttons[i]] = 1;
        dmHID::Update(m_Context);
        ASSERT_EQ((uint32_t)dmHID::GAMEPAD_MAPPED_BUTTON_COUNT, dmHID::GetGamepadButtonCount(m_Gamepad));
        for (uint32_t j = 0; j < dmHID::MAX_GAMEPAD_BUTTON_COUNT; ++j)
            ASSERT_EQ(i == j, dmHID::GetGamepadButton(&m_Gamepad->m_Packet, j));
        dmNativeJoy[0].Button[raw_buttons[i]] = 0;
        dmHID::Update(m_Context);
        ASSERT_FALSE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, i));
    }
    const uint8_t hats[] = { 1, 4, 8, 2 };
    const uint8_t dpad_buttons[] = { 14, 11, 12, 13 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(hats); ++i)
    {
        dmNativeJoy[0].Button[dpad_buttons[i]] = 1;
        dmHID::Update(m_Context);
        ASSERT_EQ(hats[i], m_Gamepad->m_Packet.m_Hat[0]);
        ASSERT_EQ(1U, m_Gamepad->m_HatCount);
        dmNativeJoy[0].Button[dpad_buttons[i]] = 0;
        dmHID::Update(m_Context);
        ASSERT_EQ(0U, m_Gamepad->m_Packet.m_Hat[0]);
    }
}

// Verifies automatic triggers start released and real normalized axis events survive mapping reselection.
TEST_F(AndroidGamepadTest, AutomaticAxes)
{
    SelectAutomaticMapping();
    dmHID::Update(m_Context);
    ASSERT_EQ(-1.0f, m_Gamepad->m_Packet.m_Axis[4]);
    ASSERT_EQ(-1.0f, m_Gamepad->m_Packet.m_Axis[5]);
    const float axes[] = { -1.0f, 0.5f, 1.0f, -0.5f, -0.4f, 1.0f };
    memcpy(dmNativeJoy[0].Axis, axes, 4 * sizeof(float));
    // Brake and Gas are device-only axes, stored after the fixed legacy slots.
    dmNativeJoy[0].Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + 4] = axes[4];
    dmNativeJoy[0].Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + 5] = axes[5];
    dmNativeJoy[0].AxisEventMask = ~(uint64_t)0;
    dmNativeJoy[0].AxisInitialMask = 0;
    SelectAutomaticMapping();
    dmHID::Update(m_Context);
    ASSERT_EQ((uint32_t)dmHID::GAMEPAD_MAPPED_AXIS_COUNT, m_Gamepad->m_AxisCount);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(axes); ++i)
        ASSERT_EQ(axes[i], m_Gamepad->m_Packet.m_Axis[i]);
}

// Verifies sparse capabilities omit absent controls and map a TV remote's Back button to B alone.
TEST_F(AndroidGamepadTest, SparseCapabilities)
{
    strcpy(dmNativeJoy[0].DeviceGuid, "05000000341200007856000011000300");
    SelectAutomaticMapping();
    memset(dmNativeJoy[0].Button, 1, sizeof(dmNativeJoy[0].Button));
    for (uint32_t i = 0; i < 7; ++i)
        dmNativeJoy[0].Axis[i] = 1.0f;
    dmHID::Update(m_Context);
    for (uint32_t i = 0; i < dmHID::GAMEPAD_MAPPED_BUTTON_COUNT; ++i)
        ASSERT_EQ(i < 2, dmHID::GetGamepadButton(&m_Gamepad->m_Packet, i));
    ASSERT_EQ(0U, m_Gamepad->m_Packet.m_Hat[0]);
    ASSERT_EQ(1.0f, m_Gamepad->m_Packet.m_Axis[0]);
    ASSERT_EQ(1.0f, m_Gamepad->m_Packet.m_Axis[1]);
    ASSERT_EQ(0.0f, m_Gamepad->m_Packet.m_Axis[2]);
    ASSERT_EQ(0.0f, m_Gamepad->m_Packet.m_Axis[3]);
    ASSERT_EQ(-1.0f, m_Gamepad->m_Packet.m_Axis[4]);
    ASSERT_EQ(-1.0f, m_Gamepad->m_Packet.m_Axis[5]);
}

// Verifies the Guide fallback follows SDL's Android 11 boundary without exposing this SDK rule to input.
TEST_F(AndroidGamepadTest, GuideSDKVersion)
{
    dmNativeJoy[0].Button[19] = 1;
    for (int sdk = 29; sdk <= 30; ++sdk)
    {
        g_AndroidSDKVersion = sdk;
        SelectAutomaticMapping();
        dmHID::Update(m_Context);
        ASSERT_EQ(sdk >= 30, dmHID::GetGamepadButton(&m_Gamepad->m_Packet, dmHID::GAMEPAD_MAPPED_BUTTON_GUIDE));
    }
}

// Verifies a selected database mapping retains physical indices and switching back clears old raw button bits.
TEST_F(AndroidGamepadTest, PhysicalMapping)
{
    SelectAutomaticMapping();
    dmHID::SetGamepadMapping(m_Context, m_Gamepad, "0500da4ac82d00001b300000ffff3f00,Test,a:b9,lefttrigger:a4,righttrigger:a5,");
    dmNativeJoy[0].NumButtons = 19;
    dmNativeJoy[0].Button[4] = 1;
    dmNativeJoy[0].Button[7] = 1;
    dmHID::Update(m_Context);
    ASSERT_TRUE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, 9));
    ASSERT_FALSE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, 8));
    ASSERT_TRUE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, 18));
    ASSERT_EQ(-1.0f, m_Gamepad->m_Packet.m_Axis[4]);
    ASSERT_EQ(0U, m_Gamepad->m_HatCount);
    SelectAutomaticMapping();
    dmHID::Update(m_Context);
    ASSERT_TRUE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, dmHID::GAMEPAD_MAPPED_BUTTON_LEFT_SHOULDER));
    ASSERT_FALSE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, 18));
}

// Verifies signed and inverted database triggers start at the endpoint that Defold scales to released.
TEST_F(AndroidGamepadTest, DatabaseTriggerRestValues)
{
    struct TriggerMapping
    {
        const char* m_Mapping;
        float m_Left;
        float m_Right;
    };
    const TriggerMapping mappings[] = {
        { "0500da4ac82d00001b300000ffff3f00,Test,lefttrigger:a4,righttrigger:a5,", -1.0f, -1.0f },
        { "0500da4ac82d00001b300000ffff3f00,Test,lefttrigger:+a4,righttrigger:-a5,", -1.0f, 1.0f },
        { "0500da4ac82d00001b300000ffff3f00,Test,lefttrigger:-a4,righttrigger:+a5,", 1.0f, -1.0f },
        { "0500da4ac82d00001b300000ffff3f00,Test,lefttrigger:a4~,righttrigger:a5~,", 1.0f, 1.0f },
        { "0500da4ac82d00001b300000ffff3f00,Test,lefttrigger:+a4~,righttrigger:-a5~,", 1.0f, -1.0f },
        { "0500da4ac82d00001b300000ffff3f00,Test,lefttrigger:-a4~,righttrigger:+a5~,", -1.0f, 1.0f },
    };
    dmHID::SetGamepadLayoutLegacy(m_Gamepad, false);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(mappings); ++i)
    {
        dmHID::SetGamepadMapping(m_Context, m_Gamepad, mappings[i].m_Mapping);
        dmHID::Update(m_Context);
        ASSERT_EQ(mappings[i].m_Left, m_Gamepad->m_Packet.m_Axis[4]);
        ASSERT_EQ(mappings[i].m_Right, m_Gamepad->m_Packet.m_Axis[5]);
    }
}

// Verifies legacy mappings retain the original Android state after automatic mapping was previously selected.
TEST_F(AndroidGamepadTest, LegacyMapping)
{
    SelectAutomaticMapping();
    dmHID::SetGamepadLayoutLegacy(m_Gamepad, true);
    dmHID::SetGamepadMapping(m_Context, m_Gamepad, 0);
    dmNativeJoy[0].Button[3] = 1;
    dmNativeJoy[0].Axis[2] = 0.75f;
    dmNativeJoy[0].AxisEventMask = ~(uint64_t)0;
    dmNativeJoy[0].AxisInitialMask = 0;
    dmNativeJoy[0].Hats = 2;
    dmHID::Update(m_Context);
    ASSERT_TRUE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, 3));
    ASSERT_FALSE(dmHID::GetGamepadButton(&m_Gamepad->m_Packet, 2));
    ASSERT_EQ(0.75f, m_Gamepad->m_Packet.m_Axis[2]);
    ASSERT_EQ(2U, m_Gamepad->m_Packet.m_Hat[0]);
    ASSERT_EQ((uint32_t)dmHID::MAX_GAMEPAD_BUTTON_COUNT, dmHID::GetGamepadButtonCount(m_Gamepad));
}

// Verifies reconnect mapping precedes the first packet and preserves motion already received on reconnect.
TEST_F(AndroidGamepadTest, ReconnectTriggerActions)
{
    dmInput::NewContextParams params = {};
    params.m_HidContext = m_Context;
    dmInput::HContext input_context = dmInput::NewContext(params);
    dmInputDDF::GamepadMapsRuntime maps = {};
    dmInput::RegisterGamepads(input_context, &maps);
    dmHID::SetGamepadConnectivityCallback(m_Context, dmInput::GamepadConnectivityCallback, input_context);
    dmInput::HBinding binding = dmInput::NewBinding(input_context);
    dmInputDDF::GamepadTrigger triggers[] = {
        { dmInputDDF::GAMEPAD_LTRIGGER, "left_trigger" },
        { dmInputDDF::GAMEPAD_RTRIGGER, "right_trigger" },
    };
    dmInputDDF::InputBinding ddf = {};
    ddf.m_GamepadTrigger.m_Data = triggers;
    ddf.m_GamepadTrigger.m_Count = DM_ARRAY_SIZE(triggers);
    dmInput::SetBinding(binding, &ddf);
    ASSERT_EQ(1U, binding->m_GamepadBindings.Size());
    dmInput::HGamepadBinding gamepad_binding = binding->m_GamepadBindings[0];
    const dmhash_t left_trigger = dmHashString64("left_trigger");
    const dmhash_t right_trigger = dmHashString64("right_trigger");
    dmHID::Update(m_Context);
    dmInput::UpdateBinding(binding, 1.0f / 60.0f);
    ASSERT_EQ(0.0f, dmInput::GetValue(gamepad_binding, left_trigger));
    ASSERT_EQ(0.0f, dmInput::GetValue(gamepad_binding, right_trigger));

    for (int motion_received = 0; motion_received <= 1; ++motion_received)
    {
        dmNativeJoy[0].State = 0;
        dmHID::SetGamepadConnectionStatus(m_Context, m_Gamepad, false);
        dmHID::Update(m_Context);
        dmInput::UpdateBinding(binding, 1.0f / 60.0f);
        ASSERT_FALSE(dmHID::IsGamepadConnected(m_Gamepad));

        dmNativeJoy[0].State = 1;
        dmNativeJoy[0].LayoutLegacy = 1;
        dmNativeJoy[0].AxisEventMask = motion_received ? ~(uint64_t)0 : 0;
        dmNativeJoy[0].AxisInitialMask = 0;
        memset(dmNativeJoy[0].Axis, 0, sizeof(dmNativeJoy[0].Axis));
        if (motion_received)
        {
            dmNativeJoy[0].Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + 4] = -0.5f;
            dmNativeJoy[0].Axis[NATIVE_ANDROID_GAMEPAD_NUMAXIS + 5] = 0.5f;
        }
        dmHID::SetGamepadConnectionStatus(m_Context, m_Gamepad, true);
        dmHID::Update(m_Context);
        dmInput::UpdateBinding(binding, 1.0f / 60.0f);
        ASSERT_TRUE(dmHID::IsGamepadConnected(m_Gamepad));
        ASSERT_EQ(1U, binding->m_GamepadBindings.Size());
        ASSERT_TRUE(gamepad_binding == binding->m_GamepadBindings[0]);
        ASSERT_EQ(motion_received ? 0.25f : 0.0f, dmInput::GetValue(gamepad_binding, left_trigger));
        ASSERT_EQ(motion_received ? 0.75f : 0.0f, dmInput::GetValue(gamepad_binding, right_trigger));
    }
    dmInput::DeleteBinding(binding);
    dmInput::DeleteContext(input_context);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
