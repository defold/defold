#include <gtest/gtest.h>

#include <assert.h>
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
    static dmGameObject::CreateResult CompMessageTargetCreate(dmGameObject::HCollection collection,
                                                             dmGameObject::HInstance instance,
                                                             void* resource,
                                                             void* world,
                                                             void* context,
                                                             uintptr_t* user_data);
    static dmGameObject::CreateResult CompMessageTargetDestroy(dmGameObject::HCollection collection,
                                                              dmGameObject::HInstance instance,
                                                              void* world,
                                                              void* context,
                                                              uintptr_t* user_data);
    static dmGameObject::UpdateResult CompMessageTargetOnMessage(dmGameObject::HInstance instance,
                                   const dmGameObject::InstanceMessageData* message_data,
                                   void* context,
                                   uintptr_t* user_data);

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;

    std::map<uint32_t, uint32_t> m_MessageMap;

    uint32_t m_MessageTargetCounter;
};

const static dmhash_t POST_NAMED_ID = dmHashString64("post_named");
const static dmhash_t POST_DDF_ID = dmHashString64(TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_ScriptName);
const static dmhash_t POST_NAMED_TO_INST_ID = dmHashString64("post_named_to_instance");
const static dmhash_t POST_DDF_TO_INST_ID = dmHashString64(TestGameObjectDDF::TestMessage::m_DDFDescriptor->m_ScriptName);

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

dmGameObject::CreateResult MessageTest::CompMessageTargetCreate(dmGameObject::HCollection collection,
                                                         dmGameObject::HInstance instance,
                                                         void* resource,
                                                         void* world,
                                                         void* context,
                                                         uintptr_t* user_data)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult MessageTest::CompMessageTargetDestroy(dmGameObject::HCollection collection,
                                                          dmGameObject::HInstance instance,
                                                          void* world,
                                                          void* context,
                                                          uintptr_t* user_data)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::UpdateResult MessageTest::CompMessageTargetOnMessage(dmGameObject::HInstance instance,
                                        const dmGameObject::InstanceMessageData* message_data,
                                        void* context,
                                        uintptr_t* user_data)
{
    MessageTest* self = (MessageTest*) context;

    if (message_data->m_MessageId == dmHashString64("inc"))
    {
        self->m_MessageTargetCounter++;
        if (self->m_MessageTargetCounter == 2)
        {
            dmGameObject::PostNamedMessageTo(instance, "component_message.scriptc", dmHashString64("test_message"), 0x0, 0);
        }
    }
    else if (message_data->m_MessageId == dmHashString64("dec"))
    {
        self->m_MessageTargetCounter--;
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

TEST_F(MessageTest, TestPostNamed)
{
    dmGameObject::PostNamedMessage(m_Register, POST_NAMED_ID);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_EQ(1U, m_MessageMap[POST_NAMED_ID]);
}

TEST_F(MessageTest, TestPostDDF)
{
    TestGameObjectDDF::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    dmGameObject::PostDDFMessage(m_Register, TestGameObjectDDF::TestMessage::m_DDFDescriptor, (char*)&ddf);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_EQ(2U, m_MessageMap[POST_DDF_ID]);
}

TEST_F(MessageTest, TestPostNamedTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    dmGameObject::PostNamedMessageTo(instance, 0, POST_NAMED_TO_INST_ID, 0x0, 0);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
}

TEST_F(MessageTest, TestPostDDFTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    TestGameObjectDDF::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    dmGameObject::PostDDFMessageTo(instance, 0, TestGameObjectDDF::TestMessage::m_DDFDescriptor, (char*)&ddf);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
}

TEST_F(MessageTest, TestTable)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_table.goc");
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, instance, "test_table_instance"));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    dmGameObject::Delete(m_Collection, instance);
}

TEST_F(MessageTest, TestComponentMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_message.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Result r;

    ASSERT_EQ(0U, m_MessageTargetCounter);

    r = dmGameObject::PostNamedMessageTo(go, "does_not_exists", dmHashString64("inc"), 0x0, 0);
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, r);

    r = dmGameObject::PostNamedMessageTo(go, "mt", dmHashString64("inc"), 0x0, 0);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));
    ASSERT_EQ(1U, m_MessageTargetCounter);

    r = dmGameObject::PostNamedMessageTo(go, "mt", dmHashString64("inc"), 0x0, 0);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));
    ASSERT_EQ(2U, m_MessageTargetCounter);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(MessageTest, TestBroadcastMessage)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "component_broadcast_message.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Result r;

    ASSERT_EQ(0U, m_MessageTargetCounter);

    r = dmGameObject::PostNamedMessageTo(go, 0, dmHashString64("inc"), 0x0, 0);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString64("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    dmGameObject::UpdateResult update_result = dmGameObject::DispatchInput(&m_Collection, 1, &action, 1);

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, update_result);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
