// Copyright 2020-2024 The Defold Foundation
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

#include <jc_test/jc_test.h>

#include <dmsdk/dlib/vmath.h>

#include <dlib/hash.h>
#include <dlib/dstrings.h>

#include <resource/resource.h>

#include "../gameobject.h"
#include "../component.h"

#include "gameobject/test/input/test_gameobject_input_ddf.h"

using namespace dmVMath;

class InputTest : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        m_InputCounter = 0;

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/src/gameobject/test/input");
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);

        m_Contexts.SetCapacity(7,16);
        m_Contexts.Put(dmHashString64("goc"), m_Register);
        m_Contexts.Put(dmHashString64("collectionc"), m_Register);
        m_Contexts.Put(dmHashString64("scriptc"), m_ScriptContext);
        m_Contexts.Put(dmHashString64("luac"), &m_ModuleContext);
        dmResource::RegisterTypes(m_Factory, &m_Contexts);

        dmGameObject::ComponentTypeCreateCtx component_create_ctx = {};
        component_create_ctx.m_Script = m_ScriptContext;
        component_create_ctx.m_Register = m_Register;
        component_create_ctx.m_Factory = m_Factory;
        dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);
        dmGameObject::SortComponentTypes(m_Register);

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);

        dmResource::Result e = dmResource::RegisterType(m_Factory, "it", this, 0, ResInputTargetCreate, 0, ResInputTargetDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
        e = dmResource::GetTypeFromExtension(m_Factory, "it", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType it_type;
        it_type.m_Name = "it";
        it_type.m_ResourceType = resource_type;
        it_type.m_Context = this;
        it_type.m_CreateFunction = CompInputTargetCreate;
        it_type.m_DestroyFunction = CompInputTargetDestroy;
        it_type.m_OnInputFunction = CompInputTargetOnInput;
        it_type.m_InstanceHasUserData = true;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, it_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

    static dmResource::Result ResInputTargetCreate(const dmResource::ResourceCreateParams& params);
    static dmResource::Result ResInputTargetDestroy(const dmResource::ResourceDestroyParams& params);

    static dmGameObject::CreateResult CompInputTargetCreate(const dmGameObject::ComponentCreateParams& params);
    static dmGameObject::CreateResult CompInputTargetDestroy(const dmGameObject::ComponentDestroyParams& params);
    static dmGameObject::InputResult CompInputTargetOnInput(const dmGameObject::ComponentOnInputParams& params);

public:

    uint32_t m_InputCounter;

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

dmResource::Result InputTest::ResInputTargetCreate(const dmResource::ResourceCreateParams& params)
{
    TestGameObjectDDF::InputTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::InputTarget>(params.m_Buffer, params.m_BufferSize, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        params.m_Resource->m_Resource = (void*) obj;
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmResource::Result InputTest::ResInputTargetDestroy(const dmResource::ResourceDestroyParams& params)
{
    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

dmGameObject::CreateResult InputTest::CompInputTargetCreate(const dmGameObject::ComponentCreateParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult InputTest::CompInputTargetDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::InputResult InputTest::CompInputTargetOnInput(const dmGameObject::ComponentOnInputParams& params)
{
    InputTest* self = (InputTest*) params.m_Context;

    if (params.m_InputAction->m_ActionId == dmHashString64("test_action")
            || params.m_InputAction->m_ActionId == 0)
    {
        self->m_InputCounter++;
        return dmGameObject::INPUT_RESULT_CONSUMED;
    }
    else
    {
        assert(0);
        return dmGameObject::INPUT_RESULT_UNKNOWN_ERROR;
    }
}

TEST_F(InputTest, TestComponentInput)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_input.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::UpdateResult r;

    ASSERT_EQ(0U, m_InputCounter);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    r = dmGameObject::DispatchInput(m_Collection, &action, 1);

    ASSERT_EQ(1U, m_InputCounter);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);

    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 0.0f;
    action.m_Pressed = 0;
    action.m_Released = 1;
    action.m_Repeated = 0;

    r = dmGameObject::DispatchInput(m_Collection, &action, 1);

    ASSERT_EQ(2U, m_InputCounter);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);
}

TEST_F(InputTest, TestComponentInput2)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_input2.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Repeated = 1;

    dmGameObject::UpdateResult r = dmGameObject::DispatchInput(m_Collection, &action, 1);

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);
}

TEST_F(InputTest, TestComponentInput3)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_input3.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    dmGameObject::UpdateResult r = dmGameObject::DispatchInput(m_Collection, &action, 1);

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR, r);
}

TEST_F(InputTest, TestComponentInput4)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_input4.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");

    dmGameObject::UpdateResult r = dmGameObject::DispatchInput(m_Collection, &action, 1);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);

    action.m_ActionId = 0;
    action.m_PositionSet = true;
    action.m_X = 1.0f;
    action.m_Y = 2.0f;
    action.m_DX = 3.0f;
    action.m_DY = 4.0f;

    r = dmGameObject::DispatchInput(m_Collection, &action, 1);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);
}

TEST_F(InputTest, TextComponentTextInput)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_text_input.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    char text_str[] = "testओ";

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_HasText = 0;
    action.m_TextCount = dmStrlCpy(action.m_Text, text_str, sizeof(action.m_Text));

    // Test normal text input action
    dmGameObject::UpdateResult r = dmGameObject::DispatchInput(m_Collection, &action, 1);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);

    // Test marked text input action
    action.m_ActionId = dmHashString64("test_action");
    action.m_HasText = 1;
    action.m_TextCount = dmStrlCpy(action.m_Text, text_str, sizeof(action.m_Text));

    r = dmGameObject::DispatchInput(m_Collection, &action, 1);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);
}

TEST_F(InputTest, TestDeleteFocusInstance)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_input.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::Delete(m_Collection, go, false);
    dmGameObject::PostUpdate(m_Collection);

    dmGameObject::UpdateResult r;

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    r = dmGameObject::DispatchInput(m_Collection, &action, 1);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, r);
}
