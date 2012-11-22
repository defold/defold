#include <gtest/gtest.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
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
    dmGameObject::SetProperties(properties, params.m_PropertyBuffer, params.m_PropertyBufferSize);
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

TEST_F(PropsTest, PropsNil)
{
    lua_State* L = luaL_newstate();
    lua_pushnil(L);
    uint32_t size = dmGameObject::LuaTableToProperties(L, 1, 0x0, 0);
    ASSERT_EQ(0u, size);
    lua_close(L);
}

TEST_F(PropsTest, PropsSpawnNoProperties)
{
    dmGameObject::HInstance instance = dmGameObject::Spawn(m_Collection, "/props_go.goc", dmHashString64("test_id"), 0x0, 0, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f));
    ASSERT_NE((void*)0u, instance);
    // Script init is run in spawn which verifies the properties
}

TEST_F(PropsTest, PropsFailDefaultURL)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_default_url.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(PropsTest, PropsFailRelURL)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_rel_url.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(PropsTest, PropsFailOverflowDefs)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_overflow_defs.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(PropsTest, PropsFailOverflowGo)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_overflow_go.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(PropsTest, PropsFailOverflowColl)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_overflow_coll.collectionc", (void**)&collection);
    ASSERT_NE(dmResource::RESULT_OK, res);
}

TEST_F(PropsTest, PropsFailUnsuppGo)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_unsupp_go.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(PropsTest, PropsFailUnsuppColl)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_unsupp_coll.collectionc", (void**)&collection);
    ASSERT_NE(dmResource::RESULT_OK, res);
}

TEST_F(PropsTest, PropsFailDefInInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_def_in_init.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_FALSE(result);
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(PropsTest, PropsFailLuaTableOverflow)
{
    lua_State* L = luaL_newstate();
    const uint32_t original_count = 16;
    lua_newtable(L);
    for (uint32_t i = 0; i < original_count; ++i)
    {
        char key[3];
        DM_SNPRINTF(key, 3, "%d", i);
        lua_pushstring(L, key);
        lua_pushnumber(L, i);
        lua_settable(L, 1);
    }
    const uint32_t buffer_size = original_count*2;
    uint8_t buffer[buffer_size];
    uint32_t actual_size = dmGameObject::LuaTableToProperties(L, 1, buffer, buffer_size);
    ASSERT_GT(actual_size, buffer_size);
    lua_close(L);
}

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
