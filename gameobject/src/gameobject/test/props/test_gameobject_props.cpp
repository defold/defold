#include <gtest/gtest.h>

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../proto/gameobject_ddf.h"

using namespace Vectormath::Aos;

class ScriptTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_ScriptContext = dmScript::NewContext(0);
        dmGameObject::Initialize(m_ScriptContext);

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Path = "build/default/src/gameobject/test/props";
        m_Factory = dmResource::NewFactory(&params, m_Path);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
        dmScript::DeleteContext(m_ScriptContext);
    }

public:

    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmScript::HContext m_ScriptContext;
    const char* m_Path;
};

TEST_F(ScriptTest, PropsDefault)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_default.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(ScriptTest, PropsGO)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_go.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(ScriptTest, PropsCollection)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_coll.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_coll.scriptc", 0x0));
    dmResource::Release(m_Factory, collection);
}

TEST_F(ScriptTest, PropsSubCollection)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_sub.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    dmResource::Release(m_Factory, collection);
}

TEST_F(ScriptTest, PropsFailDefaultURL)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_default_url.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ScriptTest, PropsFailRelURL)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_rel_url.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ScriptTest, PropsFailOverflowDefs)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_overflow_defs.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ScriptTest, PropsFailOverflowGo)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_overflow_go.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ScriptTest, PropsFailOverflowColl)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_overflow_coll.collectionc", (void**)&collection);
    ASSERT_NE(dmResource::RESULT_OK, res);
}

TEST_F(ScriptTest, PropsFailUnsuppGo)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_unsupp_go.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ScriptTest, PropsFailUnsuppColl)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_unsupp_coll.collectionc", (void**)&collection);
    ASSERT_NE(dmResource::RESULT_OK, res);
}

TEST_F(ScriptTest, PropsFailDefInInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_def_in_init.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_FALSE(result);
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(ScriptTest, PropsFailLuaTableOverflow)
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
    ASSERT_EQ(0u, actual_size);
    lua_close(L);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
