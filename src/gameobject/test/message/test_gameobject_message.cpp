#include <gtest/gtest.h>

#include <assert.h>
#include <map>

#include <dlib/hash.h>
#include <dlib/message.h>

#include "../gameobject.h"
#include "../gameobject_common.h"
#include "gameobject/test/message/test_gameobject_message_ddf.h"

class TestGameObjectMessage : public ::testing::Test
{
protected:
    virtual void SetUp();

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
    }

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;

    std::map<uint32_t, uint32_t> m_MessageMap;
};

const static uint32_t POST_NAMED_ID = dmHashString32("post_named");
const static uint32_t POST_DDF_ID = dmHashString32(dmTestGameObject::TestMessage::m_DDFDescriptor->m_ScriptName);
const static uint32_t POST_NAMED_TO_INST_ID = dmHashString32("post_named_to_instance");
const static uint32_t POST_DDF_TO_INST_ID = dmHashString32(dmTestGameObject::TestMessage::m_DDFDescriptor->m_ScriptName);

void DispatchCallback(dmMessage::Message *message, void* user_ptr)
{
    TestGameObjectMessage* test = (TestGameObjectMessage*)user_ptr;
    assert(test->m_Register->m_MessageId == message->m_ID);
    dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*)message->m_Data;
    if (instance_message_data->m_MessageId == POST_NAMED_ID)
    {
        test->m_MessageMap[POST_NAMED_ID] = 1;
    }
    else if (instance_message_data->m_MessageId == POST_DDF_ID)
    {
        dmTestGameObject::TestMessage* ddf = (dmTestGameObject::TestMessage*)instance_message_data->m_DDFData;
        test->m_MessageMap[POST_DDF_ID] = ddf->m_TestUint32;
    }
}

void TestGameObjectMessage::SetUp()
{
    dmGameObject::Initialize();
    dmGameObject::RegisterDDFType(dmTestGameObject::TestMessage::m_DDFDescriptor);

    m_UpdateContext.m_DT = 1.0f / 60.0f;

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
    m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/message");
    m_Register = dmGameObject::NewRegister(DispatchCallback, this);
    dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
    dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
    m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
}

TEST_F(TestGameObjectMessage, TestPostNamed)
{
    dmGameObject::PostNamedMessage(m_Register, POST_NAMED_ID);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_EQ(1U, m_MessageMap[POST_NAMED_ID]);
}

TEST_F(TestGameObjectMessage, TestPostDDF)
{
    dmTestGameObject::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    dmGameObject::PostDDFMessage(m_Register, dmTestGameObject::TestMessage::m_DDFDescriptor, (char*)&ddf);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_EQ(2U, m_MessageMap[POST_DDF_ID]);
}

TEST_F(TestGameObjectMessage, TestPostNamedTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    dmGameObject::PostNamedMessageTo(instance, 0, POST_NAMED_TO_INST_ID);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
}

TEST_F(TestGameObjectMessage, TestPostDDFTo)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "test_onmessage.goc");
    dmTestGameObject::TestMessage ddf;
    ddf.m_TestUint32 = 2;
    dmGameObject::PostDDFMessageTo(instance, 0, dmTestGameObject::TestMessage::m_DDFDescriptor, (char*)&ddf);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
