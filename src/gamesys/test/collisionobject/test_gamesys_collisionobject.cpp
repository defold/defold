#include <gtest/gtest.h>

#include <resource/resource.h>

#include <sound/sound.h>
#include <gameobject/gameobject.h>

#include "gamesys/gamesys.h"

class CollisionObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmSound::Initialize(0x0, 0x0);
        dmGameObject::Initialize();

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gamesys/test/collisionobject");
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);

        assert(dmResource::FACTORY_RESULT_OK == dmGameSystem::RegisterResourceTypes(m_Factory));

        memset(&m_RenderContext, 0, sizeof(m_RenderContext));
        memset(&m_PhysicsContext, 0, sizeof(m_PhysicsContext));
        memset(&m_EmitterContext, 0, sizeof(m_EmitterContext));
        memset(&m_RenderWorld, 0, sizeof(m_RenderWorld));

        assert(dmGameObject::RESULT_OK == dmGameSystem::RegisterComponentTypes(m_Factory, m_Register, &m_RenderContext, &m_PhysicsContext, &m_EmitterContext, m_RenderWorld));

        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
        dmSound::Finalize();
    }

public:
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;

    dmRender::RenderContext m_RenderContext;
    dmGameSystem::PhysicsContext m_PhysicsContext;
    dmGameSystem::EmitterContext m_EmitterContext;
    dmRender::HRenderWorld m_RenderWorld;
};

TEST_F(CollisionObjectTest, TestFailingInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "illegal_mass.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
