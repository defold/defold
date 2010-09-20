#include <gtest/gtest.h>

#include "../hid.h"

class HIDTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmHID::Initialize();
    }

    virtual void TearDown()
    {
        dmHID::Finalize();
    }
};

TEST_F(HIDTest, Connectedness)
{
    dmHID::Update();

    ASSERT_TRUE(dmHID::IsKeyboardConnected());
    ASSERT_TRUE(dmHID::IsMouseConnected());
    dmHID::HGamepad gamepad = dmHID::GetGamepad(0);
    ASSERT_TRUE(dmHID::IsGamepadConnected(gamepad));
}

TEST_F(HIDTest, Keyboard)
{
    dmHID::Update();
    dmHID::KeyboardPacket packet;

    ASSERT_TRUE(dmHID::GetKeyboardPacket(&packet));

    // Assert init values
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Press keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey((dmHID::Key)i, true);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(&packet));

    // Assert pressed
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetKey(&packet, (dmHID::Key)i));

    // Release keys
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        dmHID::SetKey((dmHID::Key)i, false);

    ASSERT_TRUE(dmHID::GetKeyboardPacket(&packet));

    // Asser released
    for (uint32_t i = 0; i < dmHID::MAX_KEY_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetKey(&packet, (dmHID::Key)i));
}

TEST_F(HIDTest, Mouse)
{
    dmHID::Update();
    dmHID::MousePacket packet;

    ASSERT_TRUE(dmHID::GetMousePacket(&packet));

    // Assert init state
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(0, packet.m_PositionX);
    ASSERT_EQ(0, packet.m_PositionY);
    ASSERT_EQ(0, packet.m_Wheel);

    // Touch all inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton((dmHID::MouseButton)i, true);
    dmHID::SetMousePosition(1, 2);
    dmHID::SetMouseWheel(3);

    ASSERT_TRUE(dmHID::GetMousePacket(&packet));

    // Assert inputs
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_TRUE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
    ASSERT_EQ(1, packet.m_PositionX);
    ASSERT_EQ(2, packet.m_PositionY);
    ASSERT_EQ(3, packet.m_Wheel);

    // Release buttons
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        dmHID::SetMouseButton((dmHID::MouseButton)i, false);

    ASSERT_TRUE(dmHID::GetMousePacket(&packet));

    // Assert released
    for (uint32_t i = 0; i < dmHID::MAX_MOUSE_BUTTON_COUNT; ++i)
        ASSERT_FALSE(dmHID::GetMouseButton(&packet, (dmHID::MouseButton)i));
}

TEST_F(HIDTest, Gamepad)
{
    dmHID::Update();
    dmHID::GamepadPacket packet;
    dmHID::HGamepad gamepad = dmHID::GetGamepad(0);
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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
