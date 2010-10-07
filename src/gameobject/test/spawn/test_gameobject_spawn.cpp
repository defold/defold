#include <gtest/gtest.h>

#include <assert.h>
#include <map>

#include <dlib/hash.h>
#include <dlib/message.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

class TestGameObjectSpawn : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();

        m_UpdateContext.m_DT = 1.0f / 60.0f;
        m_UpdateContext.m_ViewProj = Vectormath::Aos::Matrix4::identity();

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/spawn");
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
    }

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
};

TEST_F(TestGameObjectSpawn, TestSpawn)
{
    (void)dmGameObject::New(m_Collection, "test_spawn.goc");
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_EQ(1, m_Collection->m_InstanceIndices.Size());
    dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_EQ(2, m_Collection->m_InstanceIndices.Size());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
