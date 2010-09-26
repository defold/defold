#include <gtest/gtest.h>

#include <assert.h>

#include <dlib/configfile.h>
#include <dlib/hash.h>

#include <ddf/ddf.h>

#include "../input.h"
#include "../input_private.h"

#include "input_ddf.h"

class InputTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmHID::Initialize();
        m_Context = dmInput::NewContext();
        dmInputDDF::GamepadMaps* gamepad_maps;
        assert(dmDDF::RESULT_OK == dmDDF::LoadMessageFromFile("build/default/src/test/test.gamepadsc", dmInputDDF::GamepadMaps::m_DDFDescriptor, (void**)&gamepad_maps));
        dmInput::RegisterGamepads(m_Context, gamepad_maps);
        dmDDF::FreeMessage(gamepad_maps);
        dmDDF::LoadMessageFromFile("build/default/src/test/test.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_TestDDF);
        dmDDF::LoadMessageFromFile("build/default/src/test/combinations.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_ComboDDF);
    }

    virtual void TearDown()
    {
        dmDDF::FreeMessage(m_TestDDF);
        dmDDF::FreeMessage(m_ComboDDF);
        dmInput::DeleteContext(m_Context);
        dmHID::Finalize();
    }

    dmInput::HContext m_Context;
    dmInputDDF::InputBinding* m_TestDDF;
    dmInputDDF::InputBinding* m_ComboDDF;
};

TEST_F(InputTest, CreateContext)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_TestDDF);
    ASSERT_NE(dmInput::INVALID_BINDING, binding);
    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Keyboard)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_TestDDF);

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();

    uint32_t key_0_id = dmHashString32("KEY_0");

    float v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(0.0f, v);

    dmInput::UpdateBinding(binding);

    v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(1.0f, v);
    ASSERT_TRUE(dmInput::Pressed(binding, key_0_id));
    ASSERT_FALSE(dmInput::Released(binding, key_0_id));

    dmHID::SetKey(dmHID::KEY_0, false);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_FALSE(dmInput::Pressed(binding, key_0_id));
    ASSERT_TRUE(dmInput::Released(binding, key_0_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_FALSE(dmInput::Pressed(binding, key_0_id));
    ASSERT_FALSE(dmInput::Released(binding, key_0_id));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Mouse)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_TestDDF);

    dmHID::SetMousePosition(0, 1);

    dmHID::Update();

    uint32_t mouse_up_id = dmHashString32("MOUSE_UP");

    float v = dmInput::GetValue(binding, mouse_up_id);
    ASSERT_EQ(0.0f, v);

    dmInput::UpdateBinding(binding);

    v = dmInput::GetValue(binding, mouse_up_id);
    ASSERT_EQ(1.0f, v);
    ASSERT_TRUE(dmInput::Pressed(binding, mouse_up_id));
    ASSERT_FALSE(dmInput::Released(binding, mouse_up_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_FALSE(dmInput::Pressed(binding, mouse_up_id));
    ASSERT_TRUE(dmInput::Released(binding, mouse_up_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_FALSE(dmInput::Pressed(binding, mouse_up_id));
    ASSERT_FALSE(dmInput::Released(binding, mouse_up_id));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Gamepad)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_TestDDF);

    uint32_t action_id = dmHashString32("GAMEPAD_LSTICK_UP");
    dmInputDDF::Gamepad input = dmInputDDF::GAMEPAD_LSTICK_UP;

    dmInput::GamepadConfig* map = m_Context->m_GamepadMaps.Get(dmHashString32("null_device"));
    ASSERT_NE((void*)0x0, (void*)map);

    uint32_t index = map->m_Inputs[input].m_Index;

    dmHID::Update();

    ASSERT_GT(dmHID::GetGamepadAxisCount(binding->m_GamepadBinding->m_Gamepad), index);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action_id));
    ASSERT_TRUE(dmInput::Pressed(binding, action_id));
    ASSERT_FALSE(dmInput::Released(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 0.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_FALSE(dmInput::Pressed(binding, action_id));
    ASSERT_TRUE(dmInput::Released(binding, action_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_FALSE(dmInput::Pressed(binding, action_id));
    ASSERT_FALSE(dmInput::Released(binding, action_id));

    // Test modifiers

    uint32_t up_id = dmHashString32("GAMEPAD_RSTICK_UP");
    uint32_t down_id = dmHashString32("GAMEPAD_RSTICK_DOWN");

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, 2, 1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, up_id));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, down_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, 2, -1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, up_id));
    ASSERT_EQ(0.0f, dmInput::GetValue(binding, down_id));

    dmInput::DeleteBinding(binding);
}

void ActionCallback(uint32_t action_id, dmInput::Action* action, void* user_data)
{
    float* value = (float*)user_data;
    *value = action->m_Value;
}

TEST_F(InputTest, ForEachActive)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_TestDDF);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    float value = 0.0f;

    dmInput::ForEachActive(binding, ActionCallback, &value);

    ASSERT_EQ(0.0f, value);

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    dmInput::ForEachActive(binding, ActionCallback, &value);
    ASSERT_EQ(1.0f, value);

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Combinations)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_ComboDDF);

    uint32_t action0 = dmHashString32("Action0");
    uint32_t action1 = dmHashString32("Action1");

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action0));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action1));

    dmHID::SetKey(dmHID::KEY_0, false);
    dmHID::SetKey(dmHID::KEY_1, true);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action0));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action1));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, DeadZone)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context, m_TestDDF);

    uint32_t action_id = dmHashString32("GAMEPAD_LSTICK_UP");
    dmInputDDF::Gamepad input = dmInputDDF::GAMEPAD_LSTICK_UP;

    dmInput::GamepadConfig* config = m_Context->m_GamepadMaps.Get(dmHashString32("null_device"));
    ASSERT_NE((void*)0x0, (void*)config);

    uint32_t index = config->m_Inputs[input].m_Index;

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 0.05f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 0.1f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action_id));

    dmInput::DeleteBinding(binding);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
