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

#include <dmsdk/input/input.h>
#include "test_input_api.h"

typedef struct InputAPI
{
    HTestInputApiFixture m_Fixture;
    HInputContext        m_Context;
} InputAPI;

JC_TEST_FIXTURE_SETUP(InputAPI)
{
    jc_test_fixture->m_Fixture = TestInputApiCreate();
    ASSERT_NE((HTestInputApiFixture)0, jc_test_fixture->m_Fixture);
    jc_test_fixture->m_Context = TestInputApiGetContext(jc_test_fixture->m_Fixture);
}

JC_TEST_FIXTURE_TEARDOWN(InputAPI)
{
    TestInputApiDelete(jc_test_fixture->m_Fixture);
}

// A live context has no C actions before its first completed binding update.
TEST_F(InputAPI, EmptyIterator)
{
    InputActionIterator iterator = InputIterateActions(jc_test_fixture->m_Context);
    ASSERT_FALSE(InputActionIterateNext(&iterator));
    ASSERT_FALSE(InputActionIterateNext(&iterator));
}

#if !defined(DM_HAVE_KEYBOARD_SUPPORT) || DM_HAVE_KEYBOARD_SUPPORT
// Real keyboard updates exercise the C iterator ABI, scalar flags and independent cursors.
TEST_F(InputAPI, KeyboardActions)
{
    for (uint32_t frame = 0; frame < 3; ++frame)
    {
        TestInputApiSetKey(jc_test_fixture->m_Fixture, frame != 2);
        TestInputApiUpdate(jc_test_fixture->m_Fixture, 1.0f);

        InputActionIterator iterator = InputIterateActions(jc_test_fixture->m_Context);
        InputActionIterator second = InputIterateActions(jc_test_fixture->m_Context);
        ASSERT_TRUE(InputActionIterateNext(&iterator));
        ASSERT_TRUE(InputActionIterateNext(&second));
        ASSERT_EQ(dmHashString64("up"), InputActionGetId(&iterator));
        ASSERT_EQ(InputActionGetId(&iterator), InputActionGetId(&second));
        ASSERT_EQ(frame != 2 ? 1.0f : 0.0f, InputActionGetValue(&iterator));
        ASSERT_EQ(frame == 0, InputActionIsPressed(&iterator));
        ASSERT_EQ(frame == 2, InputActionIsReleased(&iterator));
        ASSERT_EQ(frame == 1, InputActionIsRepeated(&iterator));
        ASSERT_EQ(INPUT_ACTION_SOURCE_KEYBOARD, InputActionGetSource(&iterator));

        uint32_t index = 77;
        ASSERT_FALSE(InputActionGetGamepadIndex(&iterator, &index));
        ASSERT_EQ(77u, index);
        ASSERT_EQ((dmhash_t)0, InputActionGetGamepadNameHash(&iterator));
        ASSERT_FALSE(InputActionIsGamepadConnected(&iterator));
        ASSERT_FALSE(InputActionIsGamepadDisconnected(&iterator));
        ASSERT_FALSE(InputActionIterateNext(&iterator));
        ASSERT_FALSE(InputActionIterateNext(&iterator));
        ASSERT_FALSE(InputActionIterateNext(&second));
    }
}
#endif

// C gamepad metadata preserves missing/last-known names and the completed batch through a reload.
TEST_F(InputAPI, GamepadMetadata)
{
    const dmhash_t name_hash = dmHashString64("Controller A");
    InputActionIterator iterator;
    for (uint32_t frame = 0; frame < 5; ++frame)
    {
        if (frame == 0)
        {
            TestInputApiSetGamepadConnected(jc_test_fixture->m_Fixture, true);
        }
        else if (frame == 1)
        {
            TestInputApiSetGamepadNameHash(jc_test_fixture->m_Fixture, name_hash);
            TestInputApiSetGamepadAxis(jc_test_fixture->m_Fixture, 0.5f);
        }
        else if (frame == 2)
        {
            TestInputApiSetGamepadNameHash(jc_test_fixture->m_Fixture, 0);
        }
        else if (frame == 3)
        {
            TestInputApiSetGamepadNameHash(jc_test_fixture->m_Fixture, name_hash);
            TestInputApiSetGamepadAxis(jc_test_fixture->m_Fixture, 0.0f);
        }
        else
        {
            TestInputApiSetGamepadConnected(jc_test_fixture->m_Fixture, false);
        }
        TestInputApiUpdate(jc_test_fixture->m_Fixture, 1.0f);

        iterator = InputIterateActions(jc_test_fixture->m_Context);
        ASSERT_TRUE(InputActionIterateNext(&iterator));
        const char* id = frame == 0 ? "connected" : frame == 4 ? "disconnected" : "axis";
        dmhash_t expected_name = frame == 0 ? dmHashString64("null_device") : frame == 2 ? 0 : name_hash;
        ASSERT_EQ(dmHashString64(id), InputActionGetId(&iterator));
        ASSERT_EQ(INPUT_ACTION_SOURCE_GAMEPAD, InputActionGetSource(&iterator));
        ASSERT_EQ(expected_name, InputActionGetGamepadNameHash(&iterator));
        ASSERT_EQ(frame == 0, InputActionIsGamepadConnected(&iterator));
        ASSERT_EQ(frame == 4, InputActionIsGamepadDisconnected(&iterator));

        uint32_t index = 77;
        ASSERT_TRUE(InputActionGetGamepadIndex(&iterator, &index));
        ASSERT_EQ(0u, index);
        InputActionIterator exhausted = iterator;
        ASSERT_FALSE(InputActionIterateNext(&exhausted));
    }

    TestInputApiClearBinding(jc_test_fixture->m_Fixture);
    ASSERT_EQ(name_hash, InputActionGetGamepadNameHash(&iterator));
    ASSERT_TRUE(InputActionIsGamepadDisconnected(&iterator));

    iterator = InputIterateActions(jc_test_fixture->m_Context);
    ASSERT_TRUE(InputActionIterateNext(&iterator));
    ASSERT_EQ(name_hash, InputActionGetGamepadNameHash(&iterator));

    TestInputApiUpdate(jc_test_fixture->m_Fixture, 1.0f);
    iterator = InputIterateActions(jc_test_fixture->m_Context);
    ASSERT_FALSE(InputActionIterateNext(&iterator));
}

int TestInputApiRunTests(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
