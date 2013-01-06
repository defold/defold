#include <gtest/gtest.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../gameobject_script.h"
#include "../proto/gameobject_ddf.h"

using namespace Vectormath::Aos;

dmResource::Result ResCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource, const char* filename)
{
    // The resource is not relevant for this test
    resource->m_Resource = (void*)new uint8_t[4];
    return dmResource::RESULT_OK;
}
dmResource::Result ResDestroy(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource)
{
    // The resource is not relevant for this test
    delete [] (uint8_t*)resource->m_Resource;
    return dmResource::RESULT_OK;
}

dmGameObject::CreateResult CompNoUserDataCreate(const dmGameObject::ComponentCreateParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult CompNoUserDataDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

void CompNoUserDataSetProperties(const dmGameObject::ComponentSetPropertiesParams& params)
{
    // The test is that this function should never be reached
    dmGameObject::HProperties properties = (dmGameObject::HProperties)*params.m_UserData;
    SetPropertyData(properties, dmGameObject::PROPERTY_LAYER_INSTANCE, params.m_PropertyData);
}

class PropsTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Path = "build/default/src/gameobject/test/props";
        m_Factory = dmResource::NewFactory(&params, m_Path);
        m_ScriptContext = dmScript::NewContext(0);
        dmGameObject::Initialize(m_ScriptContext, m_Factory);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);

        // Register dummy physical resource type
        dmResource::Result e = dmResource::RegisterType(m_Factory, "no_user_datac", this, ResCreate, ResDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        uint32_t resource_type;
        dmGameObject::Result result;

        e = dmResource::GetTypeFromExtension(m_Factory, "no_user_datac", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType no_user_data_type;
        no_user_data_type.m_Name = "no_user_datac";
        no_user_data_type.m_ResourceType = resource_type;
        no_user_data_type.m_CreateFunction = CompNoUserDataCreate;
        no_user_data_type.m_DestroyFunction = CompNoUserDataDestroy;
        no_user_data_type.m_SetPropertiesFunction = CompNoUserDataSetProperties;
        no_user_data_type.m_InstanceHasUserData = false;
        result = dmGameObject::RegisterComponentType(m_Register, no_user_data_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::Finalize(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmScript::DeleteContext(m_ScriptContext);
    }

public:

    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmScript::HContext m_ScriptContext;
    const char* m_Path;
};

TEST_F(PropsTest, PropsDefault)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_default.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    // Twice since we had crash here
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(PropsTest, PropsGO)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_go.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(PropsTest, PropsCollection)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_coll.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_coll.scriptc", 0x0));
    dmResource::Release(m_Factory, collection);
}

TEST_F(PropsTest, PropsSubCollection)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_sub.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    dmResource::Release(m_Factory, collection);
}

TEST_F(PropsTest, PropsMultiScript)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_multi_script.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    dmResource::Release(m_Factory, collection);
}

TEST_F(PropsTest, PropsSpawn)
{
    lua_State* L = dmGameObject::GetLuaState();
    int top = lua_gettop(L);
    lua_newtable(L);
    lua_pushliteral(L, "number");
    lua_pushnumber(L, 200);
    lua_rawset(L, -3);
    lua_pushliteral(L, "hash");
    dmScript::PushHash(L, dmHashString64("hash"));
    lua_rawset(L, -3);
    dmMessage::URL url;
    dmMessage::ResetURL(url);
    url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    url.m_Path = dmHashString64("/path");
    lua_pushliteral(L, "url");
    dmScript::PushURL(L, url);
    lua_rawset(L, -3);
    lua_pushliteral(L, "vector3");
    dmScript::PushVector3(L, Vector3(1, 2, 3));
    lua_rawset(L, -3);
    lua_pushliteral(L, "vector4");
    dmScript::PushVector4(L, Vector4(1, 2, 3, 4));
    lua_rawset(L, -3);
    lua_pushliteral(L, "quat");
    dmScript::PushQuat(L, Quat(1, 2, 3, 4));
    lua_rawset(L, -3);
    uint8_t buffer[1024];
    uint32_t buffer_size = 1024;
    uint32_t size_used = dmScript::CheckTable(L, (char*)buffer, buffer_size, -1);
    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));
    ASSERT_LT(0u, size_used);
    ASSERT_LT(size_used, buffer_size);
    dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/props_spawn.goc", dmHashString64("test_id"), buffer, buffer_size, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), 1.0f);
    // Script init is run in spawn which verifies the properties
    ASSERT_NE((void*)0u, instance);
}

TEST_F(PropsTest, PropsSpawnNoProperties)
{
    dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/props_go.goc", dmHashString64("test_id"), 0x0, 0, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), 1.0f);
    // Script init is run in spawn which verifies the properties
    ASSERT_NE((void*)0u, instance);
}

TEST_F(PropsTest, PropsRelativeURL)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_rel_url.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    dmResource::Release(m_Factory, collection);
}

//TEST_F(PropsTest, PropsFailOverflowDefs)
//{
//    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_overflow_defs.goc");
//    ASSERT_EQ((void*) 0, (void*) go);
//}

//TEST_F(PropsTest, PropsFailOverflowGo)
//{
//    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_overflow_go.goc");
//    ASSERT_EQ((void*) 0, (void*) go);
//}

//TEST_F(PropsTest, PropsFailOverflowColl)
//{
//    dmGameObject::HCollection collection;
//    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_overflow_coll.collectionc", (void**)&collection);
//    ASSERT_NE(dmResource::RESULT_OK, res);
//}

TEST_F(PropsTest, PropsFailDefInInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_def_in_init.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_FALSE(result);
    dmGameObject::Delete(m_Collection, go);
}

//TEST_F(PropsTest, PropsFailLuaTableOverflow)
//{
//    lua_State* L = luaL_newstate();
//    const uint32_t original_count = 16;
//    lua_newtable(L);
//    for (uint32_t i = 0; i < original_count; ++i)
//    {
//        char key[3];
//        DM_SNPRINTF(key, 3, "%d", i);
//        lua_pushstring(L, key);
//        lua_pushnumber(L, i);
//        lua_settable(L, 1);
//    }
//    const uint32_t buffer_size = original_count*2;
//    uint8_t buffer[buffer_size];
//    uint32_t actual_size = dmGameObject::LuaTableToProperties(L, 1, buffer, buffer_size);
//    ASSERT_GT(actual_size, buffer_size);
//    lua_close(L);
//}

TEST_F(PropsTest, PropsFailNoUserData)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_no_user_data.collectionc", (void**)&collection);
    ASSERT_NE(dmResource::RESULT_OK, res);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
