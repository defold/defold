#include <gtest/gtest.h>

#include "../hid.h"

class HIDTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmHID::NewContext(dmHID::NewContextParams());
        dmHID::Init(m_Context);
    }

    virtual void TearDown()
    {
        dmHID::Final(m_Context);
        dmHID::DeleteContext(m_Context);
    }

    dmHID::HContext m_Context;
};

TEST_F(HIDTest, Connectedness)
{
    dmHID::Update(m_Context);

    ASSERT_TRUE(dmHID::IsKeyboardConnected(m_Context));
    ASSERT_TRUE(dmHID::IsMouseConnected(m_Context));
    dmHID::HGamepad gamepad = dmHID::GetGamepad(m_Context, 0);
    ASSERT_TRUE(dmHID::IsGamepadConnected(gamepad));
    ASSERT_TRUE(dmHID::IsTouchDeviceConnected(m_Context));
}

TEST_F(HIDTest, Keyboard)
{
    dmHID::Update(m_Context);
    dmHID::KeyboardPacket packet;

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Context, &packet));

    // Assert init values
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Press keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey(m_Context, (dmHID::Key)i, true);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Context, &packet));

    // Assert pressed
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Release keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey(m_Context, (dmHID::Key)i, false);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(m_Context, &packet));

    // Assert released
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));
}

TEST_F(HIDTest, Mouse)
{
    dmHID::Update(m_Context);
    dmHID::MousePacket packet;

    ASSERT_TRUE(dmHID::GetMousePacket(m_Context, &packet));

    // Assert init state
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(0, packet.m_PositionX);
    ASSERT_EQ(0, packet.m_PositionY);
    ASSERT_EQ(0, packet.m_Wheel);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton(m_Context, (dmHID::MouseButton)i, true);
    dmHID::SetMousePosition(m_Context, 1, 2);
    dmHID::SetMouseWheel(m_Context, 3);

    ASSERT_TRUE(dmHID::GetMousePacket(m_Context, &packet));

    // Assert inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(1, packet.m_PositionX);
    ASSERT_EQ(2, packet.m_PositionY);
    ASSERT_EQ(3, packet.m_Wheel);

    // Release buttons
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton(m_Context, (dmHID::MouseButton)i, false);

    ASSERT_TRUE(dmHID::GetMousePacket(m_Context, &packet));

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

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_Context, &packet));

    // Assert init state
    ASSERT_EQ(0u, packet.m_TouchCount);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_TOUCH_COUNT; ++i)
    {
        dmHID::AddTouch(m_Context, i*2, i*2+1, 0, dmHID::PHASE_BEGAN);
    }

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_Context, &packet));

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
    dmHID::ClearTouches(m_Context);

    ASSERT_TRUE(dmHID::GetTouchDevicePacket(m_Context, &packet));

    ASSERT_EQ(0u, packet.m_TouchCount);
}

TEST_F(HIDTest, IgnoredDevices)
{
    dmHID::NewContextParams params;

    params.m_IgnoreMouse = 1;
    dmHID::HContext context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsMouseConnected(context));
    dmHID::Final(context);
    dmHID::DeleteContext(context);

    params.m_IgnoreMouse = 0;
    params.m_IgnoreKeyboard = 1;
    context = dmHID::NewContext(params);
    ASSERT_TRUE(dmHID::Init(context));
    dmHID::Update(context);
    ASSERT_FALSE(dmHID::IsKeyboardConnected(context));
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
    ASSERT_FALSE(dmHID::IsTouchDeviceConnected(context));
    dmHID::Final(context);
    dmHID::DeleteContext(context);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
