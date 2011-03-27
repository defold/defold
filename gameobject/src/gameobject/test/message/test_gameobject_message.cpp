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
        dmGameObject::Initialize();
        dmGameObject::RegisterDDFType(TestGameObjectDDF::TestMessage::m_DDFDescriptor);

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/message");
        m_Register = dmGameObject::NewRegister(DispatchCallback, this);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);

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
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
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

    if (params.m_MessageData->m_MessageId == dmHashString64("inc"))
    {
        self->m_MessageTargetCounter++;
        if (self->m_MessageTargetCounter == 2)
        {
            dmGameObject::InstanceMessageParams message_params;
            message_params.m_MessageId = dmHashString64("test_message");
            message_params.m_ReceiverInstance = params.m_MessageData->m_SenderInstance;
            message_params.m_ReceiverComponent = params.m_MessageData->m_SenderComponent;
            dmGameObject::Result result = dmGameObject::PostInstanceMessage(message_params);
            assert(result == dmGameObject::RESULT_OK);
        }
    }
    else if (params.m_MessageData->m_MessageId == dmHashString64("dec"))
    {
        self->m_MessageTargetCounter--;
    }
    else if (params.m_MessageData->m_MessageId == dmHashString64(TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_Name))
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
    assert(test->m_Register->m_MessageId == message->m_ID);
    dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*)message->m_Data;
    if (instance_message_data->m_MessageId == POST_NAMED_ID)
    {
        test->m_MessageMap[POST_NAMED_ID] = 1;
    }
    else if (instance_message_data->m_MessageId == POST_DDF_ID)
    {
        TestGameObjectDDF::TestMessage* ddf = (TestGameObjectDDF::TestMessage*)instance_message_data->m_Buffer;
        test->m_MessageMap[POST_DDF_ID] = ddf->m_TestUint32;
    }
}

TEST_F(MessageTest, TestPostNamedTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    ASSERT_NE((void*)0, (void*)instance);
    uint8_t component_index;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(instance, dmHashString64("script"), &component_index));
    dmGameObject::InstanceMessageParams params;
    params.m_MessageId = POST_NAMED_TO_INST_ID;
    params.m_ReceiverInstance = instance;
    params.m_ReceiverComponent = component_index;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::PostInstanceMessage(params));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(MessageTest, TestPostDDFTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    TestGameObjectDDF::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    uint8_t component_index;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(instance, dmHashString64("script"), &component_index));
    dmGameObject::InstanceMessageParams params;
    params.m_ReceiverInstance = instance;
    params.m_ReceiverComponent = component_index;
    params.m_DDFDescriptor = TestGameObjectDDF::TestMessage::m_DDFDescriptor;
    params.m_Buffer = &ddf;
    params.m_BufferSize = sizeof(TestGameObjectDDF::TestMessage);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::PostInstanceMessage(params));
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

    dmGameObject::Result r;

    ASSERT_EQ(0U, m_MessageTargetCounter);

    dmGameObject::InstanceMessageParams params;
    params.m_MessageId = dmHashString64("inc");
    params.m_SenderInstance = go;
    params.m_ReceiverInstance = go;

    r = dmGameObject::GetComponentIndex(go, dmHashString64("script"), &params.m_SenderComponent);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    r = dmGameObject::GetComponentIndex(go, dmHashString64("mt"), &params.m_ReceiverComponent);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    r = dmGameObject::PostInstanceMessage(params);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));
    ASSERT_EQ(1U, m_MessageTargetCounter);

    r = dmGameObject::PostInstanceMessage(params);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, 0));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(MessageTest, TestComponentMessageFail)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_message.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Result r;

    dmGameObject::InstanceMessageParams params;
    params.m_MessageId = dmHashString64("inc");
    params.m_SenderInstance = go;
    params.m_ReceiverInstance = go;
    r = dmGameObject::PostInstanceMessage(params);
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, r);

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

    dmGameObject::Result r;

    ASSERT_EQ(0U, m_MessageTargetCounter);

    dmGameObject::InstanceMessageParams params;
    params.m_MessageId = dmHashString64("test_message");
    params.m_ReceiverInstance = go;
    r = dmGameObject::BroadcastInstanceMessage(params);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
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
