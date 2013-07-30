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
        m_ScriptContext = dmScript::NewContext(0, 0);
        dmGameObject::Initialize(m_ScriptContext, m_Factory);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmGameObject::Finalize(m_ScriptContext, m_Factory);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:
    dmScript::HContext m_ScriptContext;
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
        dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), 1.0f);
        ASSERT_NE(0u, (uintptr_t)instance);
    }
}

TEST_F(FactoryTest, FactoryScale)
{
    dmhash_t id = dmGameObject::GenerateUniqueInstanceId(m_Collection);
    ASSERT_NE(0u, id);
    dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), 2.0f);
    ASSERT_EQ(2.0f, dmGameObject::GetScale(instance));
}

TEST_F(FactoryTest, FactoryScaleAlongZ)
{
    dmhash_t id = dmGameObject::GenerateUniqueInstanceId(m_Collection);
    m_Collection->m_ScaleAlongZ = 1;
    dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), 2.0f);
    ASSERT_TRUE(dmGameObject::ScaleAlongZ(instance));
    id = dmGameObject::GenerateUniqueInstanceId(m_Collection);
    m_Collection->m_ScaleAlongZ = 0;
    instance = dmGameObject::Spawn(m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), 2.0f);
    ASSERT_FALSE(dmGameObject::ScaleAlongZ(instance));
}

TEST_F(FactoryTest, FactoryProperties)
{
    lua_State* L = dmGameObject::GetLuaState();
    lua_newtable(L);
    lua_pushliteral(L, "number");
    lua_pushnumber(L, 3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "hash");
    dmScript::PushHash(L, dmHashString64("hash3"));
    lua_rawset(L, -3);
    lua_pushliteral(L, "url");
    dmMessage::URL url;
    url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    url.m_Path = dmHashString64("/url3");
    url.m_Fragment = 0;
    dmScript::PushURL(L, url);
    lua_rawset(L, -3);
    lua_pushliteral(L, "vec3");
    dmScript::PushVector3(L, Vector3(11, 12, 13));
    lua_rawset(L, -3);
    lua_pushliteral(L, "vec4");
    dmScript::PushVector4(L, Vector4(14, 15, 16, 17));
    lua_rawset(L, -3);
    lua_pushliteral(L, "quat");
    dmScript::PushQuat(L, Quat(18, 19, 20, 21));
    lua_rawset(L, -3);
    char buffer[256];
    uint32_t buffer_size = dmScript::CheckTable(L, buffer, 256, -1);
    lua_pop(L, 1);
    dmhash_t id = dmGameObject::GenerateUniqueInstanceId(m_Collection);
    dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/test_props.goc", id, (unsigned char*)buffer, buffer_size, Point3(), Quat(), 2.0f);
    ASSERT_NE((void*)0, instance);
    id = dmGameObject::GenerateUniqueInstanceId(m_Collection);
    instance = dmGameObject::Spawn(m_Collection, "/test_props.goc", id, (unsigned char*)buffer, buffer_size, Point3(), Quat(), 2.0f);
    ASSERT_NE((void*)0, instance);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
