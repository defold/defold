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

#include "test_input_api.h"

#include <ddf/ddf.h>
#include <dlib/testutil.h>
#include <hid_private.h>
#include <input/input_ddf.h>

#include "../input.h"
#include "../input_private.h"

struct TestInputApiFixture
{
    dmHID::HContext   m_HidContext;
    dmInput::HContext m_Context;
    dmInput::HBinding m_Binding;
    uint32_t          m_GamepadAxis;
};

extern "C" HTestInputApiFixture TestInputApiCreate(void)
{
    HTestInputApiFixture fixture = new TestInputApiFixture();
    fixture->m_HidContext = dmHID::NewContext(dmHID::NewContextParams());
    dmHID::Init(fixture->m_HidContext);

    dmInput::NewContextParams params;
    params.m_HidContext = fixture->m_HidContext;
    params.m_RepeatDelay = 0.0f;
    params.m_RepeatInterval = 1.0f;
    params.m_GamepadDeadZone = 0.2f;
    fixture->m_Context = dmInput::NewContext(params);

    char path[256];
    dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/test/build/test.gamepadsc");
    dmInputDDF::GamepadMapsRuntime* maps = 0;
    if (dmDDF::LoadMessageFromFile(path, dmInputDDF::GamepadMapsRuntime::m_DDFDescriptor, (void**)&maps) != dmDDF::RESULT_OK)
    {
        TestInputApiDelete(fixture);
        return 0;
    }
    dmInput::RegisterGamepads(fixture->m_Context, maps);
    dmDDF::FreeMessage(maps);

    uint32_t* config_index = fixture->m_Context->m_GamepadMaps.Get(dmHashString32("null_device"));
    if (!config_index)
    {
        TestInputApiDelete(fixture);
        return 0;
    }
    fixture->m_GamepadAxis = fixture->m_Context->m_GamepadConfigs[*config_index].m_Inputs[dmInputDDF::GAMEPAD_LSTICK_UP].m_Index;

    dmInputDDF::KeyTrigger key = { dmInputDDF::KEY_UP, "up" };
    dmInputDDF::GamepadTrigger gamepad[] = {
        { dmInputDDF::GAMEPAD_CONNECTED, "connected" },
        { dmInputDDF::GAMEPAD_DISCONNECTED, "disconnected" },
        { dmInputDDF::GAMEPAD_LSTICK_UP, "axis" },
    };
    dmInputDDF::InputBinding binding = {};
    binding.m_KeyTrigger.m_Data = &key;
    binding.m_KeyTrigger.m_Count = 1;
    binding.m_GamepadTrigger.m_Data = gamepad;
    binding.m_GamepadTrigger.m_Count = DM_ARRAY_SIZE(gamepad);
    fixture->m_Binding = dmInput::NewBinding(fixture->m_Context);
    dmInput::SetBinding(fixture->m_Binding, &binding);
    return fixture;
}

extern "C" void TestInputApiDelete(HTestInputApiFixture fixture)
{
    if (!fixture)
    {
        return;
    }
    if (fixture->m_Binding)
    {
        dmInput::DeleteBinding(fixture->m_Binding);
    }
    dmInput::DeleteContext(fixture->m_Context);
    dmHID::Final(fixture->m_HidContext);
    dmHID::DeleteContext(fixture->m_HidContext);
    delete fixture;
}

extern "C" HInputContext TestInputApiGetContext(HTestInputApiFixture fixture)
{
    return fixture->m_Context;
}

extern "C" void TestInputApiSetKey(HTestInputApiFixture fixture, bool down)
{
    dmHID::SetKey(dmHID::GetKeyboard(fixture->m_HidContext, 0), dmHID::KEY_UP, down);
}

extern "C" void TestInputApiSetGamepadConnected(HTestInputApiFixture fixture, bool connected)
{
    dmHID::SetGamepadConnectivity(fixture->m_HidContext, 0, connected);
}

extern "C" void TestInputApiSetGamepadAxis(HTestInputApiFixture fixture, float value)
{
    dmHID::SetGamepadAxis(dmHID::GetGamepad(fixture->m_HidContext, 0), fixture->m_GamepadAxis, value);
}

extern "C" void TestInputApiSetGamepadNameHash(HTestInputApiFixture fixture, dmhash_t name_hash)
{
    fixture->m_Binding->m_GamepadBindings[0]->m_DeviceNameHash = name_hash;
}

extern "C" void TestInputApiUpdate(HTestInputApiFixture fixture, float dt)
{
    dmHID::Update(fixture->m_HidContext);
    dmInput::UpdateBinding(fixture->m_Binding, dt);
}

extern "C" void TestInputApiClearBinding(HTestInputApiFixture fixture)
{
    dmInputDDF::InputBinding empty = {};
    dmInput::SetBinding(fixture->m_Binding, &empty);
}

int main(int argc, char** argv)
{
    return TestInputApiRunTests(argc, argv);
}
