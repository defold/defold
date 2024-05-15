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

#include <assert.h>
#include <stdint.h>
#include <map>

#include <dlib/hash.h>
#include <dlib/message.h>

#include <dlib/log.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "gameobject/test/message/test_gameobject_message_ddf.h"

#include "../../../proto/gameobject/gameobject_ddf.h"

void DispatchCallback(dmMessage::Message *message, void* user_ptr);

class MessageTest : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/src/gameobject/test/message");
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

        assert(dmMessage::NewSocket("@system", &m_Socket) == dmMessage::RESULT_OK);

        m_MessageTargetCounter = 0;

        dmResource::Result e = dmResource::RegisterType(m_Factory, "mt", this, 0, ResMessageTargetCreate, 0, ResMessageTargetDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        // MessageTargetComponent
        dmResource::ResourceType resource_type;
        e = dmResource::GetTypeFromExtension(m_Factory, "mt", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType mt_type;
        mt_type.m_Name = "mt";
        mt_type.m_ResourceType = resource_type;
        mt_type.m_Context = this;
        mt_type.m_NewWorldFunction = CompMessageTargetNewWorld;
        mt_type.m_DeleteWorldFunction = CompMessageTargetDeleteWorld;
        mt_type.m_CreateFunction = CompMessageTargetCreate;
        mt_type.m_DestroyFunction = CompMessageTargetDestroy;
        mt_type.m_OnMessageFunction = CompMessageTargetOnMessage;
        mt_type.m_InstanceHasUserData = true;

        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, mt_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);
    }


    virtual void TearDown()
    {
        dmMessage::DeleteSocket(m_Socket);
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

    static dmResource::Result ResMessageTargetCreate(const dmResource::ResourceCreateParams& params);
    static dmResource::Result ResMessageTargetDestroy(const dmResource::ResourceDestroyParams& params);
    static dmGameObject::CreateResult CompMessageTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    static dmGameObject::CreateResult CompMessageTargetDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    static dmGameObject::CreateResult CompMessageTargetCreate(const dmGameObject::ComponentCreateParams& params);
    static dmGameObject::CreateResult CompMessageTargetDestroy(const dmGameObject::ComponentDestroyParams& params);
    static dmGameObject::UpdateResult CompMessageTargetOnMessage(const dmGameObject::ComponentOnMessageParams& params);

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmScript::HContext m_ScriptContext;
    dmMessage::HSocket m_Socket;

    std::map<uint32_t, uint32_t> m_MessageMap;

    uint32_t m_MessageTargetCounter;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

const static dmhash_t POST_NAMED_ID = dmHashString64("post_named");
const static dmhash_t POST_DDF_ID = TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_NameHash;
const static dmhash_t POST_NAMED_TO_INST_ID = dmHashString64("post_named_to_instance");

dmResource::Result MessageTest::ResMessageTargetCreate(const dmResource::ResourceCreateParams& params)
{
    TestGameObjectDDF::MessageTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::MessageTarget>(params.m_Buffer, params.m_BufferSize, &obj);
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

dmResource::Result MessageTest::ResMessageTargetDestroy(const dmResource::ResourceDestroyParams& params)
{
    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

dmGameObject::CreateResult MessageTest::CompMessageTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params)
{
    *params.m_World = params.m_Context;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult MessageTest::CompMessageTargetDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult MessageTest::CompMessageTargetCreate(const dmGameObject::ComponentCreateParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult MessageTest::CompMessageTargetDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::UpdateResult MessageTest::CompMessageTargetOnMessage(const dmGameObject::ComponentOnMessageParams& params)
{
    MessageTest* self = (MessageTest*) params.m_Context;
    assert(params.m_Context == params.m_World);

    if (params.m_Message->m_Id == dmHashString64("inc"))
    {
        self->m_MessageTargetCounter++;
        if (self->m_MessageTargetCounter == 2)
        {
            dmhash_t message_id = dmHashString64("test_message");
            assert(dmMessage::RESULT_OK == dmMessage::Post(&params.m_Message->m_Receiver, &params.m_Message->m_Sender, message_id, 0, 0, 0x0, 0, 0));
        }
    }
    else if (params.m_Message->m_Id == dmHashString64("dec"))
    {
        self->m_MessageTargetCounter--;
    }
    else if (params.m_Message->m_Id == TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_NameHash)
    {
        self->m_MessageTargetCounter++;
    }
    else
    {
        assert(0);
        return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }
    return dmGameObject::UPDATE_RESULT_OK;
}

void DispatchCallback(dmMessage::Message *message, void* user_ptr)
{
    MessageTest* test = (MessageTest*)user_ptr;
    if (message->m_Id == POST_NAMED_ID)
    {
        test->m_MessageMap[POST_NAMED_ID] = 1;
    }
    else if (message->m_Id == POST_DDF_ID)
    {
        TestGameObjectDDF::TestMessage* ddf = (TestGameObjectDDF::TestMessage*)message->m_Data;
        test->m_MessageMap[POST_DDF_ID] = ddf->m_TestUint32;
    }
}

TEST_F(MessageTest, TestPostNamedTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/test_onmessage.goc");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_instance"));
    dmhash_t message_id = POST_NAMED_TO_INST_ID;
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(instance);
    receiver.m_Fragment = dmHashString64("script");
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, 0, 0, 0x0, 0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(MessageTest, TestPostDDFTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/test_onmessage.goc");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_instance"));
    TestGameObjectDDF::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(instance);
    receiver.m_Fragment = dmHashString64("script");
    uintptr_t descriptor = (uintptr_t)TestGameObjectDDF::TestMessage::m_DDFDescriptor;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_NameHash, 0, descriptor, &ddf, sizeof(TestGameObjectDDF::TestMessage), 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(MessageTest, TestTable)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/test_table.goc");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_table_instance"));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmGameObject::Delete(m_Collection, instance, false);
}

TEST_F(MessageTest, TestComponentMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_message.goc");
    ASSERT_NE((void*) 0, (void*) go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_instance"));

    ASSERT_EQ(0U, m_MessageTargetCounter);

    dmhash_t message_id = dmHashString64("inc");
    dmMessage::URL sender;
    sender.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    sender.m_Path = dmGameObject::GetIdentifier(go);
    sender.m_Fragment = dmHashString64("script");
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = dmHashString64("mt");

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, 0, 0x0, 0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(1U, m_MessageTargetCounter);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, 0, 0x0, 0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(MessageTest, TestComponentMessageFail)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_message.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmhash_t message_id = dmHashString64("inc");
    dmMessage::URL sender;
    sender.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    sender.m_Path = dmGameObject::GetIdentifier(go);
    sender.m_Fragment = dmHashString64("script");
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = dmHashString64("apa");

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, 0, 0x0, 0, 0));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(MessageTest, TestBroadcastDDFMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_broadcast_message.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::SetIdentifier(m_Collection, go, "cbm");

    ASSERT_EQ(0U, m_MessageTargetCounter);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(MessageTest, TestBroadcastNamedMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_broadcast_message.goc");
    ASSERT_NE((void*) 0, (void*) go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_instance"));

    ASSERT_EQ(0U, m_MessageTargetCounter);

    dmhash_t message_id = dmHashString64("test_message");
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = 0;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, 0, 0, 0x0, 0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(MessageTest, TestInputFocus)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/test_no_onmessage.goc");
    ASSERT_NE((void*) 0, (void*) go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_instance"));

    dmhash_t message_id = dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor->m_NameHash;
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = 0;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, (uintptr_t)go, (uintptr_t)dmGameObjectDDF::AcquireInputFocus::m_DDFDescriptor, 0x0, 0, 0));

    ASSERT_EQ(0u, m_Collection->m_Collection->m_InputFocusStack.Size());

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(1u, m_Collection->m_Collection->m_InputFocusStack.Size());

    message_id = dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor->m_NameHash;

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, (uintptr_t)go, (uintptr_t)dmGameObjectDDF::ReleaseInputFocus::m_DDFDescriptor, 0x0, 0, 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(0u, m_Collection->m_Collection->m_InputFocusStack.Size());

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(MessageTest, TestSetParent)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/test_no_onmessage.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "/test_no_onmessage.goc");
    ASSERT_NE((void*) 0, (void*) parent);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_instance"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, parent, "parent_test_instance"));

    const float sq_2_half = sqrtf(2.0f) * 0.5f;
    const float epsilon = 0.000001f;

    dmGameObject::SetPosition(parent, dmVMath::Point3(1.0f, 0.0f, 0.0f));
    dmGameObject::SetRotation(parent, dmVMath::Quat(sq_2_half, 0.0f, 0.0f, sq_2_half));

    dmGameObject::SetPosition(go, dmVMath::Point3(1.0f, 0.0f, 0.0f));
    dmGameObject::SetRotation(go, dmVMath::Quat(sq_2_half, 0.0f, 0.0f, sq_2_half));

    dmhash_t message_id = dmGameObjectDDF::SetParent::m_DDFDescriptor->m_NameHash;
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = 0;
    dmGameObjectDDF::SetParent ddf;
    dmhash_t parent_id = dmHashString64("parent_test_instance");
    ddf.m_ParentId = parent_id;
    ddf.m_KeepWorldTransform = 0;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, (uintptr_t)go, (uintptr_t)dmGameObjectDDF::SetParent::m_DDFDescriptor, &ddf, sizeof(dmGameObjectDDF::SetParent), 0));

    ASSERT_EQ((void*)0, (void*)dmGameObject::GetParent(go));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NE((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(2.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(1.0f, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    // twice to make sure UpdateTransform has run
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NE((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(2.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(1.0f, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    ddf.m_ParentId = 0;
    ddf.m_KeepWorldTransform = 1;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, (uintptr_t)go, (uintptr_t)dmGameObjectDDF::SetParent::m_DDFDescriptor, &ddf, sizeof(dmGameObjectDDF::SetParent), 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(2.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(1.0f, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    // twice to make sure UpdateTransform has run

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(2.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(1.0f, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    dmGameObject::SetPosition(go, dmVMath::Point3(1.0f, 0.0f, 0.0f));
    dmGameObject::SetRotation(go, dmVMath::Quat(sq_2_half, 0.0f, 0.0f, sq_2_half));

    ddf.m_ParentId = parent_id;
    ddf.m_KeepWorldTransform = 1;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, (uintptr_t)go, (uintptr_t)dmGameObjectDDF::SetParent::m_DDFDescriptor, &ddf, sizeof(dmGameObjectDDF::SetParent), 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NE((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(1.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(sq_2_half, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(sq_2_half, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    // twice to make sure UpdateTransform has run
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NE((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(1.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(sq_2_half, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(sq_2_half, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    ddf.m_ParentId = 0;
    ddf.m_KeepWorldTransform = 0;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, (uintptr_t)go, (uintptr_t)dmGameObjectDDF::SetParent::m_DDFDescriptor, &ddf, sizeof(dmGameObjectDDF::SetParent), 0));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(0.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(1.0f, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    // twice to make sure UpdateTransform has run
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ((void*)0, (void*)dmGameObject::GetParent(go));
    ASSERT_EQ(0.0f, dmGameObject::GetWorldPosition(go).getX());
    ASSERT_NEAR(0.0f, dmGameObject::GetWorldRotation(go).getX(), epsilon);
    ASSERT_NEAR(1.0f, dmGameObject::GetWorldRotation(go).getW(), epsilon);

    dmGameObject::Delete(m_Collection, go, false);
    dmGameObject::Delete(m_Collection, parent, false);
}

TEST_F(MessageTest, TestPingPong)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/test_ping_pong.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::SetIdentifier(m_Collection, go, "test_instance");
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::UpdateContext update_context;
    update_context.m_DT = 1.0f / 60.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &update_context));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(MessageTest, TestInfPingPong)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/test_inf_ping_pong.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::SetIdentifier(m_Collection, go, "test_instance");
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::UpdateContext update_context;
    update_context.m_DT = 1.0f / 60.0f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &update_context));
    dmGameObject::Delete(m_Collection, go, false);
}



uint32_t g_PostDistpatchCalled = 0;

void CustomMessageDestroyCallback(dmMessage::Message* message)
{
    TestGameObjectDDF::TestDataMessage* test = (TestGameObjectDDF::TestDataMessage*)message->m_Data;
    g_PostDistpatchCalled = test->m_Value;
}

TEST_F(MessageTest, MessagePostDispatch)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/test_onmessage.goc");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_instance"));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(instance);
    receiver.m_Fragment = dmHashString64("script");

    // Here we assume the code invoked a message, which later will complete (actual example: http_service.cpp which will post a response later on)
    dmGameObject::Delete(m_Collection, instance, false);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmGameObject::PostUpdate(m_Collection);

    //

    TestGameObjectDDF::TestDataMessage ddf;
    ddf.m_Value = 42;
    uintptr_t descriptor = (uintptr_t)TestGameObjectDDF::TestDataMessage::m_DDFDescriptor;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, TestGameObjectDDF::TestDataMessage::m_DDFDescriptor->m_NameHash, 0, descriptor, &ddf, sizeof(TestGameObjectDDF::TestDataMessage), CustomMessageDestroyCallback));


    ASSERT_EQ(0u, g_PostDistpatchCalled);

    dmLogInfo("Expected error ->");

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    dmLogInfo("<- Expected error end");

    ASSERT_EQ(42u, g_PostDistpatchCalled);
}
