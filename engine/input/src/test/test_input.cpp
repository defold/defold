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
        m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
        dmHID::Init(m_HidContext);
        dmInput::NewContextParams params;
        params.m_HidContext = m_HidContext;
        params.m_RepeatDelay = 0.5f;
        params.m_RepeatInterval = 0.2f;
        m_Context = dmInput::NewContext(params);
        dmInputDDF::GamepadMaps* gamepad_maps;
        dmDDF::Result result = dmDDF::LoadMessageFromFile("build/default/src/test/test.gamepadsc", dmInputDDF::GamepadMaps::m_DDFDescriptor, (void**)&gamepad_maps);
        (void)result;
        assert(dmDDF::RESULT_OK == result);
        dmInput::RegisterGamepads(m_Context, gamepad_maps);
        dmDDF::FreeMessage(gamepad_maps);
        dmDDF::LoadMessageFromFile("build/default/src/test/test.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_TestDDF);
        dmDDF::LoadMessageFromFile("build/default/src/test/test2.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_Test2DDF);
        dmDDF::LoadMessageFromFile("build/default/src/test/combinations.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_ComboDDF);
        dmDDF::LoadMessageFromFile("build/default/src/test/test_text.input_bindingc", dmInputDDF::InputBinding::m_DDFDescriptor, (void**)&m_TextDDF);
        m_DT = 1.0f / 60.0f;
    }

    virtual void TearDown()
    {
        dmDDF::FreeMessage(m_TestDDF);
        dmDDF::FreeMessage(m_Test2DDF);
        dmDDF::FreeMessage(m_ComboDDF);
        dmDDF::FreeMessage(m_TextDDF);
        dmInput::DeleteContext(m_Context);
        dmHID::Final(m_HidContext);
        dmHID::DeleteContext(m_HidContext);
    }

    dmHID::HContext m_HidContext;
    dmInput::HContext m_Context;
    dmInputDDF::InputBinding* m_TestDDF;
    dmInputDDF::InputBinding* m_Test2DDF;
    dmInputDDF::InputBinding* m_ComboDDF;
    dmInputDDF::InputBinding* m_TextDDF;
    float m_DT;
};

TEST_F(InputTest, CreateContext)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);
    ASSERT_NE(dmInput::INVALID_BINDING, binding);
    dmInput::DeleteBinding(binding);
}

void TextInputCallback(dmhash_t action_id, dmInput::Action* action, void* user_data)
{
    dmHashTable64<dmInput::Action*>* actions = (dmHashTable64<dmInput::Action*>*)user_data;
    ASSERT_TRUE(action->m_HasText || action->m_TextCount > 0);
    actions->Put(action_id, action);
}

TEST_F(InputTest, Text) {
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TextDDF);

    char marked_text[] = "marked text";
    dmHID::SetMarkedText(m_HidContext, marked_text);
    dmHID::AddKeyboardChar(m_HidContext, 't');
    dmHID::AddKeyboardChar(m_HidContext, 'e');
    dmHID::AddKeyboardChar(m_HidContext, 's');
    dmHID::AddKeyboardChar(m_HidContext, 't');
    dmHID::AddKeyboardChar(m_HidContext, 0x0913); // Unicode point for: ओ (UTF8: e0 a4 93)
    dmHID::Update(m_HidContext);

    dmInput::UpdateBinding(binding, m_DT);

    dmHashTable64<dmInput::Action*> actions;
    actions.SetCapacity(32, 64);
    dmInput::ForEachActive(binding, TextInputCallback, (void*)&actions);

    dmInput::Action** text_action = actions.Get(dmHashString64("text"));
    ASSERT_EQ(7, (*text_action)->m_TextCount);
    ASSERT_STREQ("testओ", (*text_action)->m_Text);

    dmInput::Action** marked_text_action = actions.Get(dmHashString64("marked_text"));
    ASSERT_EQ(11, (*marked_text_action)->m_TextCount);
    ASSERT_STREQ("marked text", (*marked_text_action)->m_Text);

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Keyboard)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, true);

    dmHID::Update(m_HidContext);

    dmhash_t key_0_id = dmHashString64("KEY_0");

    float v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(0.0f, v);

    dmInput::UpdateBinding(binding, m_DT);

    v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(1.0f, v);
    ASSERT_TRUE(dmInput::Pressed(binding, key_0_id));
    ASSERT_FALSE(dmInput::Released(binding, key_0_id));

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, false);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, key_0_id));
    ASSERT_TRUE(dmInput::Released(binding, key_0_id));

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, key_0_id));
    ASSERT_FALSE(dmInput::Released(binding, key_0_id));

    dmInput::SetBinding(binding, m_Test2DDF);

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, true);
    dmHID::Update(m_HidContext);

    dmInput::UpdateBinding(binding, m_DT);
    v = dmInput::GetValue(binding, key_0_id);
    ASSERT_EQ(0.0f, v);

    dmHID::SetKey(m_HidContext, dmHID::KEY_1, true);
    dmHID::Update(m_HidContext);

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

    dmHID::SetMouseButton(m_HidContext, dmHID::MOUSE_BUTTON_LEFT, true);

    dmHID::Update(m_HidContext);

    dmhash_t mouse_click_id = dmHashString64("MOUSE_CLICK");

    float v = dmInput::GetValue(binding, mouse_click_id);
    ASSERT_EQ(0.0f, v);

    dmInput::UpdateBinding(binding, m_DT);

    v = dmInput::GetValue(binding, mouse_click_id);
    ASSERT_EQ(1.0f, v);
    ASSERT_TRUE(dmInput::Pressed(binding, mouse_click_id));
    ASSERT_FALSE(dmInput::Released(binding, mouse_click_id));

    dmHID::SetMouseButton(m_HidContext, dmHID::MOUSE_BUTTON_LEFT, false);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, mouse_click_id));
    ASSERT_TRUE(dmInput::Released(binding, mouse_click_id));

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding, mouse_click_id));
    ASSERT_FALSE(dmInput::Released(binding, mouse_click_id));

    // Action with mouse movement

    const dmInput::Action* click_action = dmInput::GetAction(binding, mouse_click_id);

    ASSERT_EQ(0, click_action->m_X);
    ASSERT_EQ(0, click_action->m_Y);
    ASSERT_EQ(0, click_action->m_DX);
    ASSERT_EQ(0, click_action->m_DY);
    ASSERT_TRUE(click_action->m_PositionSet);

    dmHID::SetMousePosition(m_HidContext, 0, 1);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0, click_action->m_X);
    ASSERT_EQ(1, click_action->m_Y);
    ASSERT_EQ(0, click_action->m_DX);
    ASSERT_EQ(1, click_action->m_DY);

    dmHID::SetMousePosition(m_HidContext, 0, -1);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0, click_action->m_X);
    ASSERT_EQ(-1, click_action->m_Y);
    ASSERT_EQ(0, click_action->m_DX);
    ASSERT_EQ(-2, click_action->m_DY);

    // Mouse movement

    dmHashTable64<dmInput::Action*> actions;
    actions.SetCapacity(32, 64);

    dmHID::SetMouseButton(m_HidContext, dmHID::MOUSE_BUTTON_LEFT, false);
    dmHID::SetMousePosition(m_HidContext, 0, 0);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);
    // make mouse movement come to a rest => no mouse movement
    dmInput::UpdateBinding(binding, m_DT);
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);

    dmInput::Action** move_action = actions.Get(0);
    ASSERT_EQ((void*)0, (void*)move_action);

    dmHID::SetMousePosition(m_HidContext, 1, 0);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);
    actions.Clear();
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);

    move_action = actions.Get(0);
    ASSERT_NE((void*)0, (void*)move_action);

    ASSERT_EQ(1, (*move_action)->m_X);
    ASSERT_EQ(0, (*move_action)->m_Y);
    ASSERT_EQ(1, (*move_action)->m_DX);
    ASSERT_EQ(0, (*move_action)->m_DY);
    ASSERT_TRUE((*move_action)->m_PositionSet);

    dmHID::SetMousePosition(m_HidContext, 0, 0);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);
    actions.Clear();
    dmInput::ForEachActive(binding, MouseCallback, (void*)&actions);
    move_action = actions.Get(0);

    ASSERT_EQ(0, (*move_action)->m_X);
    ASSERT_EQ(0, (*move_action)->m_Y);
    ASSERT_EQ(-1, (*move_action)->m_DX);
    ASSERT_EQ(0, (*move_action)->m_DY);

    dmHID::Update(m_HidContext);
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

    dmHID::Update(m_HidContext);

    ASSERT_GT(dmHID::GetGamepadAxisCount(binding->m_GamepadBindings[0]->m_Gamepad), index);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, 1.0f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));
    ASSERT_TRUE(dmInput::Pressed(binding->m_GamepadBindings[0], action_id));
    ASSERT_FALSE(dmInput::Released(binding->m_GamepadBindings[0], action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, 0.0f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding->m_GamepadBindings[0], action_id));
    ASSERT_TRUE(dmInput::Released(binding->m_GamepadBindings[0], action_id));

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_FALSE(dmInput::Pressed(binding->m_GamepadBindings[0], action_id));
    ASSERT_FALSE(dmInput::Released(binding->m_GamepadBindings[0], action_id));

    // Test modifiers

    dmhash_t up_id = dmHashString64("GAMEPAD_RSTICK_UP");
    dmhash_t down_id = dmHashString64("GAMEPAD_RSTICK_DOWN");

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, 2, 1.0f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding->m_GamepadBindings[0], up_id));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding->m_GamepadBindings[0], down_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, 2, -1.0f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding->m_GamepadBindings[0], up_id));
    ASSERT_EQ(0.0f, dmInput::GetValue(binding->m_GamepadBindings[0], down_id));

    dmInput::SetBinding(binding, m_Test2DDF);

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, 1.0f);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));

    input = dmInputDDF::GAMEPAD_LSTICK_DOWN;
    index = map->m_Inputs[input].m_Index;
    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, -1.0f);
    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, Touch)
{
    // Create new contexts to avoid mouse interference
    dmHID::NewContextParams hid_params;
    hid_params.m_IgnoreMouse = true;
    dmHID::HContext hid_context = dmHID::NewContext(hid_params);
    ASSERT_TRUE(dmHID::Init(hid_context));
    dmInput::NewContextParams input_params;
    input_params.m_HidContext = hid_context;
    input_params.m_RepeatDelay = 0.5f;
    input_params.m_RepeatInterval = 0.2f;
    dmInput::HContext context = dmInput::NewContext(input_params);

    dmInput::HBinding binding = dmInput::NewBinding(context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    dmhash_t touch_1_id = dmHashString64("TOUCH_1");
    dmhash_t touch_2_id = dmHashString64("TOUCH_2");

    const dmInput::Action* action = dmInput::GetAction(binding, touch_1_id);
    ASSERT_NE((void*)0, (void*)action);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_FALSE(action->m_PositionSet);
    action = dmInput::GetAction(binding, touch_2_id);
    ASSERT_NE((void*)0, (void*)action);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_FALSE(action->m_PositionSet);

    dmHID::AddTouch(hid_context, 0, 1, 0, dmHID::PHASE_MOVED);
    dmHID::AddTouch(hid_context, 2, 3, 1, dmHID::PHASE_MOVED);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_1_id);
    ASSERT_EQ(1.0f, action->m_Value);
    ASSERT_TRUE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_TRUE(action->m_Repeated);
    ASSERT_TRUE(action->m_PositionSet);
    ASSERT_EQ(0, action->m_X);
    ASSERT_EQ(1, action->m_Y);
    ASSERT_EQ(0, action->m_DX);
    ASSERT_EQ(0, action->m_DY);
    ASSERT_EQ(0, action->m_Touch[0].m_X);
    ASSERT_EQ(1, action->m_Touch[0].m_Y);

    action = dmInput::GetAction(binding, touch_2_id);
    ASSERT_EQ(1.0f, action->m_Value);
    ASSERT_TRUE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_TRUE(action->m_Repeated);
    ASSERT_TRUE(action->m_PositionSet);
    ASSERT_EQ(2, action->m_Touch[1].m_X);
    ASSERT_EQ(3, action->m_Touch[1].m_Y);

    dmHID::ClearTouches(hid_context);
    dmHID::AddTouch(hid_context, 4, 5, 0, dmHID::PHASE_MOVED);
    dmHID::AddTouch(hid_context, 6, 7, 1, dmHID::PHASE_MOVED);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_1_id);
    ASSERT_EQ(1.0f, action->m_Value);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_TRUE(action->m_PositionSet);
    ASSERT_EQ(4, action->m_Touch[0].m_X);
    ASSERT_EQ(5, action->m_Touch[0].m_Y);

    action = dmInput::GetAction(binding, touch_2_id);
    ASSERT_EQ(1.0f, action->m_Value);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_TRUE(action->m_PositionSet);
    ASSERT_EQ(6, action->m_Touch[1].m_X);
    ASSERT_EQ(7, action->m_Touch[1].m_Y);

    dmHID::ClearTouches(hid_context);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_1_id);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_TRUE(action->m_Released);
    ASSERT_FALSE(action->m_PositionSet);

    action = dmInput::GetAction(binding, touch_2_id);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_TRUE(action->m_Released);
    ASSERT_FALSE(action->m_PositionSet);

    dmInput::DeleteBinding(binding);

    // Destroy contexts
    dmInput::DeleteContext(context);
    dmHID::Final(hid_context);
    dmHID::DeleteContext(hid_context);
}

TEST_F(InputTest, TouchPhases)
{
    // Create new contexts to avoid mouse interference
    dmHID::NewContextParams hid_params;
    hid_params.m_IgnoreMouse = true;
    dmHID::HContext hid_context = dmHID::NewContext(hid_params);
    ASSERT_TRUE(dmHID::Init(hid_context));
    dmInput::NewContextParams input_params;
    input_params.m_HidContext = hid_context;
    input_params.m_RepeatDelay = 0.5f;
    input_params.m_RepeatInterval = 0.2f;
    dmInput::HContext context = dmInput::NewContext(input_params);

    dmInput::HBinding binding = dmInput::NewBinding(context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    dmhash_t touch_action_id = dmHashString64("TOUCH_1");

    const dmInput::Action* action = dmInput::GetAction(binding, touch_action_id);
    ASSERT_NE((void*)0, (void*)action);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_EQ(0, action->m_TouchCount);
    ASSERT_FALSE(action->m_PositionSet);

    // Step 1: Both touches began
    dmHID::AddTouch(hid_context, 0, 1, 0, dmHID::PHASE_BEGAN);
    dmHID::AddTouch(hid_context, 2, 3, 1, dmHID::PHASE_BEGAN);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_action_id);
    ASSERT_EQ(1.0f, action->m_Value);
    ASSERT_EQ(2, action->m_TouchCount);
    ASSERT_TRUE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_TRUE(action->m_Repeated);
    ASSERT_TRUE(action->m_PositionSet);
    ASSERT_EQ(0, action->m_X);
    ASSERT_EQ(1, action->m_Y);
    ASSERT_EQ(0, action->m_DX);
    ASSERT_EQ(0, action->m_DY);

    // Touch id: 0
    {
        ASSERT_EQ(0, action->m_Touch[0].m_X);
        ASSERT_EQ(1, action->m_Touch[0].m_Y);
        ASSERT_EQ(0, action->m_Touch[0].m_Id);
        ASSERT_EQ(dmHID::PHASE_BEGAN, action->m_Touch[0].m_Phase);
    }

    // Touch id: 1
    {
        ASSERT_EQ(2, action->m_Touch[1].m_X);
        ASSERT_EQ(3, action->m_Touch[1].m_Y);
        ASSERT_EQ(1, action->m_Touch[1].m_Id);
        ASSERT_EQ(dmHID::PHASE_BEGAN, action->m_Touch[1].m_Phase);
    }

    dmHID::ClearTouches(hid_context);

    // Step 2: Touch 1 ended, while touch 0 moved
    dmHID::AddTouch(hid_context, 4, 5, 1, dmHID::PHASE_ENDED);
    dmHID::AddTouch(hid_context, 6, 7, 0, dmHID::PHASE_MOVED);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_action_id);
    ASSERT_EQ(1.0f, action->m_Value);
    ASSERT_EQ(2, action->m_TouchCount);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_TRUE(action->m_PositionSet);

    // Touch id: 1
    {
        ASSERT_EQ(4, action->m_Touch[0].m_X);
        ASSERT_EQ(5, action->m_Touch[0].m_Y);
        ASSERT_EQ(1, action->m_Touch[0].m_Id);
        ASSERT_EQ(dmHID::PHASE_ENDED, action->m_Touch[0].m_Phase);
    }

    // Touch id: 0
    {
        ASSERT_EQ(6, action->m_Touch[1].m_X);
        ASSERT_EQ(7, action->m_Touch[1].m_Y);
        ASSERT_EQ(0, action->m_Touch[1].m_Id);
        ASSERT_EQ(dmHID::PHASE_MOVED, action->m_Touch[1].m_Phase);
    }

    dmHID::ClearTouches(hid_context);

    // Step 3: Touch 0 ended
    dmHID::AddTouch(hid_context, 6, 7, 0, dmHID::PHASE_ENDED);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_action_id);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_EQ(1, action->m_TouchCount);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_TRUE(action->m_Released);
    ASSERT_TRUE(action->m_PositionSet);

    // Touch id: 0
    {
        ASSERT_EQ(6, action->m_Touch[0].m_X);
        ASSERT_EQ(7, action->m_Touch[0].m_Y);
        ASSERT_EQ(0, action->m_Touch[0].m_Id);
        ASSERT_EQ(dmHID::PHASE_ENDED, action->m_Touch[0].m_Phase);
    }

    dmHID::ClearTouches(hid_context);

    dmHID::Update(hid_context);
    dmInput::UpdateBinding(binding, m_DT);

    action = dmInput::GetAction(binding, touch_action_id);
    ASSERT_EQ(0.0f, action->m_Value);
    ASSERT_EQ(0, action->m_TouchCount);
    ASSERT_FALSE(action->m_Pressed);
    ASSERT_FALSE(action->m_Released);
    ASSERT_FALSE(action->m_PositionSet);

    dmInput::DeleteBinding(binding);

    // Destroy contexts
    dmInput::DeleteContext(context);
    dmHID::Final(hid_context);
    dmHID::DeleteContext(hid_context);
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

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    float value = 0.0f;

    dmInput::ForEachActive(binding, ActionCallback, &value);

    ASSERT_EQ(0.0f, value);

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, true);

    dmHID::Update(m_HidContext);
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

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, true);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action0));
    ASSERT_EQ(1.0f, dmInput::GetValue(binding, action1));

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, false);
    dmHID::SetKey(m_HidContext, dmHID::KEY_1, true);

    dmHID::Update(m_HidContext);
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

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, 0.05f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, 0.1f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(0.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));

    dmHID::SetGamepadAxis(binding->m_GamepadBindings[0]->m_Gamepad, index, 1.0f);

    dmHID::Update(m_HidContext);
    dmInput::UpdateBinding(binding, m_DT);

    ASSERT_EQ(1.0f, dmInput::GetValue(binding->m_GamepadBindings[0], action_id));

    dmInput::DeleteBinding(binding);
}

TEST_F(InputTest, TestRepeat)
{
    dmInput::HBinding binding = dmInput::NewBinding(m_Context);
    dmInput::SetBinding(binding, m_TestDDF);

    dmHID::SetKey(m_HidContext, dmHID::KEY_0, true);

    dmHID::Update(m_HidContext);

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
