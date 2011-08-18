#include <gtest/gtest.h>

#include <assert.h>

#include <dlib/configfile.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>

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
        m_Context = dmInput::NewContext(0.5f, 0.2f);
        dmInputDDF::GamepadMaps* gamepad_maps;
        dmDDF::Result result = dmDDF::LoadMessageFromFile("build/default/src/test/test.gamepadsc", dmInputDDF::GamepadMaps::m_DDFDescriptor, (void**)&gamepad_maps);
        (void)result;
        assert(dmDDF::RESULT_OK == result);
        dmInput::RegisterGamepads(m_Context, gamepad_maps);
        dmDDF::FreeMessage(gamepad_maps);
        dmDDF::LoadMessageFromFile("build/default/src/test/test.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_TestDDF);
        dmDDF::LoadMessageFromFile("build/default/src/test/test2.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_Test2DDF);
        dmDDF::LoadMessageFromFile("build/default/src/test/combinations.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_ComboDDF);
        m_DT = 1.0f / 60.0f;
    }

    virtual void TearDown()
    {
        dmDDF::FreeMessage(m_TestDDF);
        dmDDF::FreeMessage(m_Test2DDF);
        dmDDF::FreeMessage(m_ComboDDF);
        dmInput::DeleteContext(m_Context);
        dmHID::Finalize();
    }

    dmInput::HContext m_Context;
    dmInputDDF::InputBinding* m_TestDDF;
    dmInputDDF::InputBinding* m_Test2DDF;
    dmInputDDF::InputBinding* m_ComboDDF;
    float m_DT;
};

TEST_F(InputTest, CreateContext)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);
    ASSERT_NE(dmInput::INVALID_BINDING, binding);
    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Keyboard)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();

    dmhash_t key_0_id = dmHashString64("KEY_0");

    float v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(0.0f, v);

    dmInput::UpdateBinding(binding, m_DT);

    v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(1.0f, v);
    ASSERT_TRUE(dmInput::Pressed(binding, key_0_id));
    ASSERT_FALSE(dmInput::Released(binding, key_0_id));

    dmHID::SetKey(dmHID::KEY_0, false);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, key_0_id));
    ASSERT_TRUE(dmInput::Released(binding, key_0_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, key_0_id));
    ASSERT_FALSE(dmInput::Released(binding, key_0_id));

    dmInput::SetBinding(binding, m_Test2DDF);

    dmHID::SetKey(dmHID::KEY_0, true);
    dmHID::Update();

    dmInput::UpdateBinding(binding, m_DT);
    v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(0.0f, v);

    dmHID::SetKey(dmHID::KEY_1, true);
    dmHID::Update();

    dmInput::UpdateBinding(binding, m_DT);
    v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(1.0f, v);

    dmInput::DeleteBinding(binding);
}

void MouseCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
{
    dmHashTable64<dmInput::Action*>* actions = (dmHashTable64<dmInput::Action*>*)user_data;
    actions->Put(action_id, action);
}

TEST_F(InputTest, Mouse)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::SetMouseButton(dmHID::MOUSE_BUTTON_LEFT, true);

    dmHID::Update();

    dmhash_t mouse_click_id = dmHashString64("MOUSE_CLICK");

    float v = dmInput::GetValue(binding, mouse_click_id);
    ASSERT_EQ(0.0f, v);

    dmInput::UpdateBinding(binding, m_DT);

    v = dmInput::GetValue(binding, mouse_click_id);
    ASSERT_EQ(1.0f, v);
    ASSERT_TRUE(dmInput::Pressed(binding, mouse_click_id));
    ASSERT_FALSE(dmInput::Released(binding, mouse_click_id));

    dmHID::SetMouseButton(dmHID::MOUSE_BUTTON_LEFT, false);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, mouse_click_id));
    ASSERT_TRUE(dmInput::Released(binding, mouse_click_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, mouse_click_id));
    ASSERT_FALSE(dmInput::Released(binding, mouse_click_id));

    // Action with mouse movement

    const dmInput::Action* click_action = dmInput::GetAction(binding, mouse_click_id);

    ASSERT_EQ(0, click_action->m_X);
    ASSERT_EQ(0, click_action->m_Y);
    ASSERT_EQ(0, click_action->m_DX);
    ASSERT_EQ(0, click_action->m_DY);
    ASSERT_EQ(1, click_action->m_PositionSet);

    dmHID::SetMousePosition(0, 1);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0, click_action->m_X);
    ASSERT_EQ(1, click_action->m_Y);
    ASSERT_EQ(0, click_action->m_DX);
    ASSERT_EQ(1, click_action->m_DY);

    dmHID::SetMousePosition(0, -1);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0, click_action->m_X);
    ASSERT_EQ(-1, click_action->m_Y);
    ASSERT_EQ(0, click_action->m_DX);
    ASSERT_EQ(-2, click_action->m_DY);

    // Mouse movement

    dmHashTable64<dmInput::Action*> actions;
    actions.SetCapacity(32, 64);

    dmHID::SetMouseButton(dmHID::MOUSE_BUTTON_LEFT, false);
    dmHID::SetMousePosition(0, 0);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);
    // make mouse movement come to a rest => no mouse movement
    dmInput::UpdateBinding(binding, m_DT);
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);

    dmInput::Action** move_action = actions.Get(0);
    ASSERT_EQ((void*)0, (void*)move_action);

    dmHID::SetMousePosition(1, 0);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);
    actions.Clear();
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);

    move_action = actions.Get(0);
    ASSERT_NE((void*)0, (void*)move_action);

    ASSERT_EQ(1, (*move_action)->m_X);
    ASSERT_EQ(0, (*move_action)->m_Y);
    ASSERT_EQ(1, (*move_action)->m_DX);
    ASSERT_EQ(0, (*move_action)->m_DY);
    ASSERT_EQ(1, (*move_action)->m_PositionSet);

    dmHID::SetMousePosition(0, 0);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);
    actions.Clear();
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);
    move_action = actions.Get(0);

    ASSERT_EQ(0, (*move_action)->m_X);
    ASSERT_EQ(0, (*move_action)->m_Y);
    ASSERT_EQ(-1, (*move_action)->m_DX);
    ASSERT_EQ(0, (*move_action)->m_DY);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);
    actions.Clear();
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);
    move_action = actions.Get(0);

    ASSERT_EQ((void*)0, (void*)move_action);

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Gamepad)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmhash_t action_id = dmHashString64("GAMEPAD_LSTICK_UP");
    dmInputDDF::Gamepad input = dmInputDDF::GAMEPAD_LSTICK_UP;

    dmInput::GamepadConfig* map = m_Context->m_GamepadMaps.Get(dmHashString32("null_device"));
    ASSERT_NE((void*)0x0, (void*)map);

    uint32_t index = map->m_Inputs[input].m_Index;

    dmHID::Update();

    ASSERT_GT(dmHID::GetGamepadAxisCount(binding->m_GamepadBinding->m_Gamepad), index);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action_id));
    ASSERT_TRUE(dmInput::Pressed(binding, action_id));
    ASSERT_FALSE(dmInput::Released(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 0.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, action_id));
    ASSERT_TRUE(dmInput::Released(binding, action_id));

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, action_id));
    ASSERT_FALSE(dmInput::Released(binding, action_id));

    // Test modifiers

    dmhash_t up_id = dmHashString64("GAMEPAD_RSTICK_UP");
    dmhash_t down_id = dmHashString64("GAMEPAD_RSTICK_DOWN");

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, 2, 1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, up_id));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, down_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, 2, -1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, up_id));
    ASSERT_EQ(0.0f, dmInput::GetValue(binding, down_id));

    dmInput::SetBinding(binding, m_Test2DDF);

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 1.0f);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    input = dmInputDDF::GAMEPAD_LSTICK_DOWN;
    index = map->m_Inputs[input].m_Index;
    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, -1.0f);
    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action_id));

    dmInput::DeleteBinding(binding);
}

void ActionCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
{
    float* value = (float*)user_data;
    *value = action->m_Value;
}

TEST_F(InputTest, ForEachActive)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    float value = 0.0f;

    dmInput::ForEachActive(binding, ActionCallback, &value);

    ASSERT_EQ(0.0f, value);

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    dmInput::ForEachActive(binding, ActionCallback, &value);
    ASSERT_EQ(1.0f, value);

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Combinations)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_ComboDDF);

    dmhash_t action0 = dmHashString64("Action0");
    dmhash_t action1 = dmHashString64("Action1");

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action0));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action1));

    dmHID::SetKey(dmHID::KEY_0, false);
    dmHID::SetKey(dmHID::KEY_1, true);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action0));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action1));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, DeadZone)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmhash_t action_id = dmHashString64("GAMEPAD_LSTICK_UP");
    dmInputDDF::Gamepad input = dmInputDDF::GAMEPAD_LSTICK_UP;

    dmInput::GamepadConfig* config = m_Context->m_GamepadMaps.Get(dmHashString32("null_device"));
    ASSERT_NE((void*)0x0, (void*)config);

    uint32_t index = config->m_Inputs[input].m_Index;

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 0.05f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 0.1f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding, action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBinding->m_Gamepad, index, 1.0f);

    dmHID::Update();
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action_id));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, TestRepeat)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::SetKey(dmHID::KEY_0, true);

    dmHID::Update();

    dmhash_t key_0_id = dmHashString64("KEY_0");

    float v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(0.0f, v);

    ASSERT_FALSE(dmInput::Repeated(binding, key_0_id));

    dmInput::UpdateBinding(binding, m_DT);
    ASSERT_TRUE(dmInput::Repeated(binding, key_0_id));

    for (int i = 0; i < 29; ++i)
    {
        dmInput::UpdateBinding(binding, m_DT);
        ASSERT_FALSE(dmInput::Repeated(binding, key_0_id));
    }

    dmInput::UpdateBinding(binding, m_DT);
    ASSERT_TRUE(dmInput::Repeated(binding, key_0_id));

    for (int i = 0; i < 11; ++i)
    {
        dmInput::UpdateBinding(binding, m_DT);
        ASSERT_FALSE(dmInput::Repeated(binding, key_0_id));
    }

    dmInput::UpdateBinding(binding, m_DT);
    ASSERT_TRUE(dmInput::Repeated(binding, key_0_id));

    dmInput::UpdateBinding(binding, m_DT);
    ASSERT_FALSE(dmInput::Repeated(binding, key_0_id));

    dmInput::DeleteBinding(binding);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
