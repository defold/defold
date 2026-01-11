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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../hid.h"

class HIDTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_Context = dmHID::NewContext(dmHID::NewContextParams());
        dmHID::Init(m_Context);

        m_Keyboard = dmHID::GetKeyboard(m_Context, 0);
        m_Mouse = dmHID::GetMouse(m_Context, 0);
        m_TouchDevice = dmHID::GetTouchDevice(m_Context, 0);

        ASSERT_NE(dmHID::INVALID_KEYBOARD_HANDLE, m_Keyboard);
        ASSERT_NE(dmHID::INVALID_MOUSE_HANDLE, m_Mouse);
        ASSERT_NE(dmHID::INVALID_TOUCH_DEVICE_HANDLE, m_TouchDevice);

    }

    void TearDown() override
    {
        dmHID::Final(m_Context);
        dmHID::DeleteContext(m_Context);
    }

    dmPlatform::HWindow m_Window;
    dmHID::HContext     m_Context;
    dmHID::HKeyboard    m_Keyboard;
    dmHID::HMouse       m_Mouse;
    dmHID::HTouchDevice m_TouchDevice;
};

TEST_F(HIDTest, Connectedness)
{
    dmHID::Update(m_Context);

    ASSERT_TRUE(dmHID::IsKeyboardConnected(m_Keyboard));
    ASSERT_TRUE(dmHID::IsMouseConnected(m_Mouse));
    dmHID::HGamepad gamepad = dmHID::GetGamepad(m_Context, 0);
    ASSERT_TRUE(dmHID::IsGamepadConnected(gamepad));
    ASSERT_TRUE(dmHID::IsTouchDeviceConnected(m_TouchDevice));
}

TEST_F(HIDTest, Keyboard)
{
    dmHID::Update(m_Context);
    dmHID::KeyboardPacket packet;

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Keyboard, &packet));

    // Assert init values
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Press keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey(m_Keyboard, (dmHID::Key)i, true);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Keyboard, &packet));

    // Assert pressed
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Release keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey(m_Keyboard, (dmHID::Key)i, false);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Keyboard, &packet));

    // Assert released
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));
}

TEST_F(HIDTest, Mouse)
{
    dmHID::Update(m_Context);
    dmHID::MousePacket packet;

    ASSERT_TRUE(dmHID::GetMousePacket(m_Mouse, &packet));

    // Assert init state
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(0, packet.m_PositionX);
    ASSERT_EQ(0, packet.m_PositionY);
    ASSERT_EQ(0, packet.m_Wheel);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton(m_Mouse, (dmHID::MouseButton)i, true);
    dmHID::SetMousePosition(m_Mouse, 1, 2);
    dmHID::SetMouseWheel(m_Mouse, 3);

    ASSERT_TRUE(dmHID::GetMousePacket(m_Mouse, &packet));

    // Assert inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(1, packet.m_PositionX);
    ASSERT_EQ(2, packet.m_PositionY);
    ASSERT_EQ(3, packet.m_Wheel);

    // Release buttons
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton(m_Mouse, (dmHID::MouseButton)i, false);

    ASSERT_TRUE(dmHID::GetMousePacket(m_Mouse, &packet));

    // Assert released
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
}

TEST_F(HIDTest, Gamepad)
{
    dmHID::Update(m_Context);
    dmHID::GamepadPacket packet;
    dmHID::HGamepad gamepad = dmHID::GetGamepad(m_Context, 0);
    ASSERT_NE(dmHID::INVALID_GAMEPAD_HANDLE, gamepad);

    uint32_t button_count = dmHID::GetGamepadButtonCount(gamepad);
    ASSERT_LT(0U, button_count);
    uint32_t axis_count = dmHID::GetGamepadAxisCount(gamepad);
    ASSERT_LT(0U, axis_count);

    ASSERT_TRUE(dmHID::GetGamepadPacket(gamepad, &packet));

    // Assert init state
    for (uint32_t i = 0; i < button_count; ++i)
        ASSERT_FALSE(dmHID::GetGamepadButton(&packet, i));
    for (uint32_t i = 0; i < axis_count; ++i)
        ASSERT_FALSE(dmHID::GetGamepadButton(&packet, i));

    // Touch all inputs
    for (uint32_t i = 0; i < button_count; ++i)
        dmHID::SetGamepadButton(gamepad, i, true);
    for (uint32_t i = 0; i < axis_count; ++i)
        dmHID::SetGamepadAxis(gamepad, i, 1.0f);

    ASSERT_TRUE(dmHID::GetGamepadPacket(gamepad, &packet));

    // Assert inputs
    for (uint32_t i = 0; i < button_count; ++i)
        ASSERT_TRUE(dmHID::GetGamepadButton(&packet, i));
    for (uint32_t i = 0; i < axis_count; ++i)
        ASSERT_EQ(1.0f, packet.m_Axis[i]);

    // Release buttons
    for (uint32_t i = 0; i < button_count; ++i)
        dmHID::SetGamepadButton(gamepad, i, false);

    ASSERT_TRUE(dmHID::GetGamepadPacket(gamepad, &packet));

    // Assert released
    for (uint32_t i = 0; i < button_count; ++i)
        ASSERT_FALSE(dmHID::GetGamepadButton(&packet, i));
}

TEST_F(HIDTest, TouchDevice)
{
    dmHID::Update(m_Context);
    dmHID::TouchDevicePacket packet;

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_TouchDevice, &packet));

    // Assert init state
    ASSERT_EQ(0u, packet.m_TouchCount);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_TOUCH_COUNT; ++i)
    {
        dmHID::AddTouch(m_TouchDevice, i*2, i*2+1, 0, dmHID::PHASE_BEGAN);
    }

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_TouchDevice, &packet));

    // Assert inputs
    int32_t x, y;
    uint32_t id;
    bool pressed;
    bool released;
    for (uint32_t i = 0; i < dmHID::MAX_TOUCH_COUNT; ++i)
    {
        ASSERT_TRUE(dmHID::GetTouch(&packet, i, &x, &y, &id, &pressed, &released));
        ASSERT_EQ((int32_t)i*2, x);
        ASSERT_EQ((int32_t)i*2+1, y);
        ASSERT_EQ(0, id);
        ASSERT_EQ(true, pressed);
        ASSERT_EQ(false, released);
    }

    // Clear touches
    dmHID::ClearTouches(m_TouchDevice);

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_TouchDevice, &packet));

    ASSERT_EQ(0u, packet.m_TouchCount);
}

TEST_F(HIDTest, IgnoredDevices)
{
    dmHID::NewContextParams params;

    m_Keyboard = dmHID::GetKeyboard(m_Context, 0);

    m_TouchDevice = dmHID::GetTouchDevice(m_Context, 0);


    params.m_IgnoreMouse = 1;
    dmHID::HContext context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsMouseConnected(dmHID::GetMouse(m_Context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreMouse = 0;
    params.m_IgnoreKeyboard = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsKeyboardConnected(dmHID::GetKeyboard(m_Context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreKeyboard = 0;
    params.m_IgnoreGamepads = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsGamepadConnected(dmHID::GetGamepad(context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreGamepads = 0;
    params.m_IgnoreTouchDevice = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsTouchDeviceConnected(dmHID::GetTouchDevice(m_Context, 0)));
    dmHID::Final(context);
    dmHID::DeleteContext(context);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    return ret;
}
