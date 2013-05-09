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
    SetPropertySet(properties, dmGameObject::PROPERTY_LAYER_INSTANCE, params.m_PropertySet);
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

TEST_F(PropsTest, PropsFailDefInInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_fail_def_in_init.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_FALSE(result);
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(PropsTest, PropsFailNoUserData)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_fail_no_user_data.collectionc", (void**)&collection);
    ASSERT_NE(dmResource::RESULT_OK, res);
}

static dmhash_t hash(const char* s)
{
    return dmHashString64(s);
}

#define ASSERT_GET_PROP_V1(prop, get_params, v0, epsilon)\
    {\
        dmGameObject::GetPropertyOutParams o;\
        get_params.m_PropertyId = hash(prop);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        float* v = (float*)o.m_Value;\
        ASSERT_NEAR(v0, v[0], epsilon);\
        ASSERT_EQ(dmGameObject::ELEMENT_TYPE_FLOAT, o.m_ElementType);\
        ASSERT_EQ(1u, o.m_ElementCount);\
        ASSERT_TRUE(o.m_ValueMutable);\
    }\

#define ASSERT_SET_PROP_V1(prop, get_params, set_params, v0, epsilon)\
    {\
        dmGameObject::GetPropertyOutParams o;\
        get_params.m_PropertyId = hash(prop);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        float vp[] = {v0};\
        set_params.m_Value = vp;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        float* v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[0], v[0], epsilon);\
    }\

#define ASSERT_GET_PROP_V3(prop, get_params, v0, v1, v2, epsilon)\
    {\
        dmGameObject::GetPropertyOutParams o;\
        get_params.m_PropertyId = hash(prop);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        float* v = (float*)o.m_Value;\
        ASSERT_NEAR(v0, v[0], epsilon);\
        ASSERT_NEAR(v1, v[1], epsilon);\
        ASSERT_NEAR(v2, v[2], epsilon);\
        ASSERT_EQ(dmGameObject::ELEMENT_TYPE_FLOAT, o.m_ElementType);\
        ASSERT_EQ(3u, o.m_ElementCount);\
        ASSERT_TRUE(o.m_ValueMutable);\
\
        get_params.m_PropertyId = hash(prop ".x");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v0, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
\
        get_params.m_PropertyId = hash(prop ".y");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v1, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
\
        get_params.m_PropertyId = hash(prop ".z");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v2, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
    }

#define ASSERT_SET_PROP_V3(prop, get_params, set_params, v0, v1, v2, epsilon)\
    {\
        dmGameObject::GetPropertyOutParams o;\
        get_params.m_PropertyId = hash(prop);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        float vp[] = {v0, v1, v2};\
        set_params.m_Value = vp;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        float* v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[0], v[0], epsilon);\
        ASSERT_NEAR(vp[1], v[1], epsilon);\
        ASSERT_NEAR(vp[2], v[2], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".x");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[0] += 1.0f;\
        set_params.m_Value = &vp[0];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[0], v[0], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".y");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[1] += 1.0f;\
        set_params.m_Value = &vp[1];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[1], v[0], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".z");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[2] += 1.0f;\
        set_params.m_Value = &vp[2];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[2], v[0], epsilon);\
    }

#define ASSERT_GET_PROP_V4(prop, params, v0, v1, v2, v3, epsilon)\
    {\
        dmGameObject::GetPropertyOutParams o;\
        params.m_PropertyId = hash(prop);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(params, o));\
        float* v = (float*)o.m_Value;\
        ASSERT_NEAR(v0, v[0], epsilon);\
        ASSERT_NEAR(v1, v[1], epsilon);\
        ASSERT_NEAR(v2, v[2], epsilon);\
        ASSERT_NEAR(v3, v[3], epsilon);\
        ASSERT_EQ(dmGameObject::ELEMENT_TYPE_FLOAT, o.m_ElementType);\
        ASSERT_EQ(4u, o.m_ElementCount);\
        ASSERT_TRUE(o.m_ValueMutable);\
        params.m_PropertyId = hash(prop ".x");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v0, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
        params.m_PropertyId = hash(prop ".y");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v1, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
        params.m_PropertyId = hash(prop ".z");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v2, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
        params.m_PropertyId = hash(prop ".w");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(v3, v[0], epsilon);\
        ASSERT_EQ(1u, o.m_ElementCount);\
    }

#define ASSERT_SET_PROP_V4(prop, get_params, set_params, v0, v1, v2, v3, epsilon)\
    {\
        dmGameObject::GetPropertyOutParams o;\
        get_params.m_PropertyId = hash(prop);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        float vp[] = {v0, v1, v2};\
        set_params.m_Value = vp;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        float* v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[0], v[0], epsilon);\
        ASSERT_NEAR(vp[1], v[1], epsilon);\
        ASSERT_NEAR(vp[2], v[2], epsilon);\
        ASSERT_NEAR(vp[3], v[3], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".x");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[0] += 1.0f;\
        set_params.m_Value = &vp[0];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[0], v[0], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".y");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[1] += 1.0f;\
        set_params.m_Value = &vp[1];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[1], v[0], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".z");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[2] += 1.0f;\
        set_params.m_Value = &vp[2];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[2], v[0], epsilon);\
\
        get_params.m_PropertyId = hash(prop ".z");\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        set_params.m_PropertyIndex = o.m_Index;\
        set_params.m_ElementCount = o.m_ElementCount;\
        vp[3] += 1.0f;\
        set_params.m_Value = &vp[3];\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(set_params));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(get_params, o));\
        v = (float*)o.m_Value;\
        ASSERT_NEAR(vp[3], v[0], epsilon);\
    }

TEST_F(PropsTest, PropsGetSet)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    dmGameObject::Init(m_Collection);

    dmGameObject::GetPropertyParams gp;
    gp.m_Instance = go;
    dmGameObject::SetPropertyParams sp;
    sp.m_Instance = go;

    float epsilon = 0.000001f;

    dmGameObject::SetPosition(go, Point3(1, 2, 3));
    ASSERT_GET_PROP_V3("position", gp, 1.0f, 2.0f, 3.0f, epsilon);
    ASSERT_SET_PROP_V3("position", gp, sp, 2.0f, 3.0f, 4.0f, epsilon);

    dmGameObject::SetRotation(go, Quat(1, 2, 3, 4));
    ASSERT_GET_PROP_V4("rotation", gp, 1.0f, 2.0f, 3.0f, 4.0f, epsilon);
    ASSERT_SET_PROP_V4("rotation", gp, sp, 2.0f, 3.0f, 4.0f, 5.0f, epsilon);

    dmGameObject::SetScale(go, 2.0f);
    ASSERT_GET_PROP_V1("scale", gp, 2.0f, epsilon);
    ASSERT_SET_PROP_V1("scale", gp, sp, 1.0f, epsilon);

    epsilon = 0.02f;

    dmGameObject::SetRotation(go, Quat(M_SQRT1_2, 0, 0, M_SQRT1_2));
    ASSERT_GET_PROP_V3("euler", gp, 90.0f, 90.0f, 0.0f, epsilon);
    ASSERT_SET_PROP_V3("euler", gp, sp, 0.0f, 0.0f, 1.0f, epsilon);

    dmGameObject::Delete(m_Collection, go);
}

#undef ASSERT_GET_PROP_V1
#undef ASSERT_SET_PROP_V1
#undef ASSERT_GET_PROP_V3
#undef ASSERT_SET_PROP_V3
#undef ASSERT_GET_PROP_V4
#undef ASSERT_SET_PROP_V4

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
