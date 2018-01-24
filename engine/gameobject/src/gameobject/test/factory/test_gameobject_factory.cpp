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
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
        dmGameObject::Result result;

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "a", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType a_type;
        a_type.m_Name = "a";
        a_type.m_ResourceType = resource_type;
        a_type.m_Context = this;
        a_type.m_CreateFunction = AComponentCreate;
        a_type.m_DestroyFunction = AComponentDestroy;
        result = dmGameObject::RegisterComponentType(m_Register, a_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 2);
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

    static dmResource::FResourceCreate    ACreate;
    static dmResource::FResourceDestroy   ADestroy;
    static dmGameObject::ComponentCreate  AComponentCreate;
    static dmGameObject::ComponentDestroy AComponentDestroy;

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
};

static dmResource::Result NullResourceCreate(const dmResource::ResourceCreateParams& params)
{
    params.m_Resource->m_Resource = (void*)1; // asserted for != 0 in dmResource
    return dmResource::RESULT_OK;
}

static dmResource::Result NullResourceDestroy(const dmResource::ResourceDestroyParams& params)
{
    return dmResource::RESULT_OK;
}

static dmGameObject::CreateResult TestComponentCreate(const dmGameObject::ComponentCreateParams& params)
{
    // Hard coded for the specific case "CreateCallback" below
    dmGameObject::HInstance instance = params.m_Instance;
    if (dmGameObject::GetIdentifier(instance) != dmHashString64("/instance0")) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    if (dmGameObject::GetWorldPosition(instance).getX() != 2.0f) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    return dmGameObject::CREATE_RESULT_OK;
}

static dmGameObject::CreateResult TestComponentDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate FactoryTest::ACreate              = NullResourceCreate;
dmResource::FResourceDestroy FactoryTest::ADestroy            = NullResourceDestroy;
dmGameObject::ComponentCreate FactoryTest::AComponentCreate   = TestComponentCreate;
dmGameObject::ComponentDestroy FactoryTest::AComponentDestroy = TestComponentDestroy;

static dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id, uint8_t* property_buffer, uint32_t property_buffer_size, const Point3& position, const Quat& rotation, const Vector3& scale)
{
    dmGameObject::HPrototype prototype = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&prototype) == dmResource::RESULT_OK) {
        dmGameObject::HInstance result = dmGameObject::Spawn(collection, prototype, prototype_name, id, property_buffer, property_buffer_size, position, rotation, scale);
        dmResource::Release(factory, prototype);
        return result;
    }
    return 0x0;
}

TEST_F(FactoryTest, Factory)
{
    const int count = 10;
    for (int i = 0; i < count; ++i)
    {
        uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
        dmhash_t id = dmGameObject::ConstructInstanceId(index);

        ASSERT_NE(0u, id);
        dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), Vector3(1, 1, 1));
        ASSERT_NE(0u, (uintptr_t)instance);
    }
}

TEST_F(FactoryTest, FactoryScale)
{
    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);
    ASSERT_NE(0u, id);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_EQ(2.0f, dmGameObject::GetUniformScale(instance));
}

TEST_F(FactoryTest, FactoryScaleAlongZ)
{
    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);

    m_Collection->m_ScaleAlongZ = 1;
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_TRUE(dmGameObject::ScaleAlongZ(instance));

    index = dmGameObject::AcquireInstanceIndex(m_Collection);
    id = dmGameObject::ConstructInstanceId(index);
    m_Collection->m_ScaleAlongZ = 0;
    instance = Spawn(m_Factory, m_Collection, "/test.goc", id, 0x0, 0, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_FALSE(dmGameObject::ScaleAlongZ(instance));
}

TEST_F(FactoryTest, FactoryProperties)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
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

    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, (unsigned char*)buffer, buffer_size, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_NE((void*)0, instance);

    index = dmGameObject::AcquireInstanceIndex(m_Collection);
    id = dmGameObject::ConstructInstanceId(index);
    instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, (unsigned char*)buffer, buffer_size, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_NE((void*)0, instance);
}

TEST_F(FactoryTest, FactoryPropertiesFailUnsupportedType)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_newtable(L);
    lua_pushliteral(L, "number");
    lua_pushliteral(L, "fail");
    lua_rawset(L, -3);
    char buffer[256];
    uint32_t buffer_size = dmScript::CheckTable(L, buffer, 256, -1);
    lua_pop(L, 1);

    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, (unsigned char*)buffer, buffer_size, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_EQ((void*)0, instance);
}

TEST_F(FactoryTest, FactoryPropertiesFailTypeMismatch)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_newtable(L);
    lua_pushliteral(L, "number");
    dmScript::PushHash(L, (dmhash_t)0);
    lua_rawset(L, -3);
    char buffer[256];
    uint32_t buffer_size = dmScript::CheckTable(L, buffer, 256, -1);
    lua_pop(L, 1);

    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, (unsigned char*)buffer, buffer_size, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_EQ((void*)0, instance);
}

TEST_F(FactoryTest, FactoryCreateCallback)
{
    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_create.goc", id, 0x0, 0, Point3(2.0f, 0.0f, 0.0f), Quat(), Vector3(2, 2, 2));
    ASSERT_NE((void*)0, instance);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
