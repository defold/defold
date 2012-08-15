#include <gtest/gtest.h>

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/log.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

using namespace Vectormath::Aos;

class FactoryTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/factory");
        dmGameObject::Initialize(0x0, m_Factory);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::Finalize(m_Factory);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
};

TEST_F(FactoryTest, Factory)
{
    const int count = 10;
    for (int i = 0; i < count; ++i)
    {
        dmhash_t id = dmGameObject::GenerateUniqueInstanceId(m_Collection);
        ASSERT_NE(0u, id);
        dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat());
        ASSERT_NE(0u, (uintptr_t)instance);
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
