#include <gtest/gtest.h>

#include <assert.h>
#include <stdint.h>
#include <map>

#include <dlib/hash.h>
#include <dlib/message.h>

#include "../gameobject.h"
#include "../gameobject_private.h"
#include "gameobject/test/message/test_gameobject_message_ddf.h"

void DispatchCallback(dmMessage::Message *message, void* user_ptr);

class MessageTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_ScriptContext = dmScript::NewContext();
        dmScript::RegisterDDFType(m_ScriptContext, TestGameObjectDDF::TestMessage::m_DDFDescriptor);
        dmGameObject::Initialize(m_ScriptContext);

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/message");
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
        assert(dmMessage::NewSocket("@system", &m_Socket) == dmMessage::RESULT_OK);

        m_MessageTargetCounter = 0;

        dmResource::FactoryResult e = dmResource::RegisterType(m_Factory, "mt", this, ResMessageTargetCreate, ResMessageTargetDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

        // MessageTargetComponent
        uint32_t resource_type;
        e = dmResource::GetTypeFromExtension(m_Factory, "mt", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType mt_type;
        mt_type.m_Name = "mt";
        mt_type.m_ResourceType = resource_type;
        mt_type.m_Context = this;
        mt_type.m_CreateFunction = CompMessageTargetCreate;
        mt_type.m_DestroyFunction = CompMessageTargetDestroy;
        mt_type.m_OnMessageFunction = CompMessageTargetOnMessage;
        mt_type.m_InstanceHasUserData = true;

        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, mt_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }


    virtual void TearDown()
    {
        dmMessage::DeleteSocket(m_Socket);
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
        dmScript::DeleteContext(m_ScriptContext);
    }

    static dmResource::CreateResult ResMessageTargetCreate(dmResource::HFactory factory,
                                           void* context,
                                           const void* buffer, uint32_t buffer_size,
                                           dmResource::SResourceDescriptor* resource,
                                           const char* filename);
    static dmResource::CreateResult ResMessageTargetDestroy(dmResource::HFactory factory,
                                            void* context,
                                            dmResource::SResourceDescriptor* resource);
    static dmGameObject::CreateResult CompMessageTargetCreate(const dmGameObject::ComponentCreateParams& params);
    static dmGameObject::CreateResult CompMessageTargetDestroy(const dmGameObject::ComponentDestroyParams& params);
    static dmGameObject::UpdateResult CompMessageTargetOnMessage(const dmGameObject::ComponentOnMessageParams& params);

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmMessage::HSocket m_Socket;
    dmScript::HContext m_ScriptContext;

    std::map<uint32_t, uint32_t> m_MessageMap;

    uint32_t m_MessageTargetCounter;
};

const static dmhash_t POST_NAMED_ID = dmHashString64("post_named");
const static dmhash_t POST_DDF_ID = dmHashString64(TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_Name);
const static dmhash_t POST_NAMED_TO_INST_ID = dmHashString64("post_named_to_instance");

dmResource::CreateResult MessageTest::ResMessageTargetCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource, const char* filename)
{
    TestGameObjectDDF::MessageTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::MessageTarget>(buffer, buffer_size, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) obj;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

dmResource::CreateResult MessageTest::ResMessageTargetDestroy(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource)
{
    dmDDF::FreeMessage((void*) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
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

    if (params.m_Message->m_Id == dmHashString64("inc"))
    {
        self->m_MessageTargetCounter++;
        if (self->m_MessageTargetCounter == 2)
        {
            dmhash_t message_id = dmHashString64("test_message");
            dmMessage::URL receiver = params.m_Message->m_Sender;
            receiver.m_Socket = self->m_Socket;
            assert(dmMessage::RESULT_OK == dmMessage::Post(0x0, &receiver, message_id, 0, 0, 0x0, 0));
        }
    }
    else if (params.m_Message->m_Id == dmHashString64("dec"))
    {
        self->m_MessageTargetCounter--;
    }
    else if (params.m_Message->m_Id == dmHashString64(TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_Name))
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
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_instance"));
    dmhash_t message_id = POST_NAMED_TO_INST_ID;
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(instance);
    receiver.m_Fragment = dmHashString64("script");
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, 0, 0, 0x0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(MessageTest, TestPostDDFTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    ASSERT_NE((void*)0, (void*)instance);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_instance"));
    TestGameObjectDDF::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(instance);
    receiver.m_Fragment = dmHashString64("script");
    uintptr_t descriptor = (uintptr_t)TestGameObjectDDF::TestMessage::m_DDFDescriptor;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, dmHashString64(TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_Name), 0, descriptor, &ddf, sizeof(TestGameObjectDDF::TestMessage)));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(MessageTest, TestTable)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_table.goc");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_table_instance"));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmGameObject::Delete(m_Collection, instance);
}

TEST_F(MessageTest, TestComponentMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_message.goc");
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

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, 0, 0x0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));
    ASSERT_EQ(1U, m_MessageTargetCounter);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, 0, 0x0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(MessageTest, TestComponentMessageFail)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_message.goc");
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

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&sender, &receiver, message_id, 0, 0, 0x0, 0));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, 0));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(MessageTest, TestBroadcastDDFMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_broadcast_message.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::SetIdentifier(m_Collection, go, "cbm");

    ASSERT_EQ(0U, m_MessageTargetCounter);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(MessageTest, TestBroadcastNamedMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_broadcast_message.goc");
    ASSERT_NE((void*) 0, (void*) go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "test_instance"));

    ASSERT_EQ(0U, m_MessageTargetCounter);

    dmhash_t message_id = dmHashString64("test_message");
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, 0, 0, 0x0, 0));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    dmGameObject::Delete(m_Collection, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
