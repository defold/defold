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

dmResource::Result ResCreate(const dmResource::ResourceCreateParams& params)
{
    // The resource is not relevant for this test
    params.m_Resource->m_Resource = (void*)new uint8_t[4];
    return dmResource::RESULT_OK;
}
dmResource::Result ResDestroy(const dmResource::ResourceDestroyParams& params)
{
    // The resource is not relevant for this test
    delete [] (uint8_t*)params.m_Resource->m_Resource;
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

dmGameObject::PropertyResult CompNoUserDataSetProperties(const dmGameObject::ComponentSetPropertiesParams& params)
{
    // The test is that this function should never be reached
    dmGameObject::HProperties properties = (dmGameObject::HProperties)*params.m_UserData;
    SetPropertySet(properties, dmGameObject::PROPERTY_LAYER_INSTANCE, params.m_PropertySet);
    return dmGameObject::PROPERTY_RESULT_OK;
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
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        dmGameObject::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0);

        // Register dummy physical resource type
        dmResource::Result e = dmResource::RegisterType(m_Factory, "no_user_datac", this, 0, ResCreate, 0, ResDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
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
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmGameObject::DeleteRegister(m_Register);
    }

public:

    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmScript::HContext m_ScriptContext;
    const char* m_Path;
    dmGameObject::ModuleContext m_ModuleContext;
};

static void SetProperties(dmGameObject::HInstance instance)
{
    dmArray<dmGameObject::Prototype::Component>& components = instance->m_Prototype->m_Components;
    uint32_t count = components.Size();
    uint32_t component_instance_data_index = 0;
    dmGameObject::ComponentSetPropertiesParams params;
    params.m_Instance = instance;
    params.m_PropertySet.m_GetPropertyCallback = 0;
    params.m_PropertySet.m_FreeUserDataCallback = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::ComponentType* type = components[i].m_Type;
        if (type->m_SetPropertiesFunction != 0x0)
        {
            uintptr_t* component_instance_data = &instance->m_ComponentInstanceUserData[component_instance_data_index];
            params.m_UserData = component_instance_data;
            type->m_SetPropertiesFunction(params);
        }
        if (components[i].m_Type->m_InstanceHasUserData)
            ++component_instance_data_index;
    }
}

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

TEST_F(PropsTest, PropsDefault)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_default.goc");
    ASSERT_NE((void*) 0, (void*) go);
    SetProperties(go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    // Twice since we had crash here
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(PropsTest, PropsGO)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    ASSERT_NE((void*) 0, (void*) go);
    SetProperties(go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, ReloadResource(m_Factory, "/props_go.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go, false);
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
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
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
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/props_spawn.goc", dmHashString64("test_id"), buffer, buffer_size, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    // Script init is run in spawn which verifies the properties
    ASSERT_NE((void*)0u, instance);
}

TEST_F(PropsTest, PropsSpawnNoProperties)
{
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/props_go.goc", dmHashString64("test_id"), 0x0, 0, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
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
    dmGameObject::Delete(m_Collection, go, false);
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

#define ASSERT_GET_PROP_NUM(go, prop, v0, epsilon)\
    {\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v0, desc.m_Variant.m_Number, epsilon);\
    }\

#define ASSERT_SET_PROP_NUM(go, prop, v0, epsilon)\
    {\
        dmGameObject::PropertyVar var(v0);\
        dmGameObject::SetProperty(go, 0, hash(prop), var);\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_NEAR(v0, desc.m_Variant.m_Number, epsilon);\
    }\

#define ASSERT_GET_PROP_V1(go, prop, v0, epsilon)\
    {\
        dmGameObject::SetScale(go, v0);\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v0, *desc.m_ValuePtr, epsilon);\
    }\

#define ASSERT_SET_PROP_V1(go, prop, v0, epsilon)\
    {\
        dmGameObject::PropertyVar var(v0);\
        dmGameObject::SetProperty(go, 0, hash(prop), var);\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_NEAR(v0, *desc.m_ValuePtr, epsilon);\
    }\

#define ASSERT_GET_PROP_V3(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);\
        ASSERT_EQ(hash(prop ".x"), desc.m_ElementIds[0]);\
        ASSERT_EQ(hash(prop ".y"), desc.m_ElementIds[1]);\
        ASSERT_EQ(hash(prop ".z"), desc.m_ElementIds[2]);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".x"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".y"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".z"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[0], epsilon);\
    }

#define ASSERT_SET_PROP_V3(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyVar var(v);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, hash(prop), var));\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
\
        float v0 = v.getX() + 1.0f;\
        dmhash_t id = hash(prop ".x");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getY() + 1.0f;\
        id = hash(prop ".y");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getZ() + 1.0f;\
        id = hash(prop ".z");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
    }

#define ASSERT_GET_PROP_V4(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_TRUE(dmGameObject::PROPERTY_TYPE_VECTOR4 == desc.m_Variant.m_Type || dmGameObject::PROPERTY_TYPE_QUAT == desc.m_Variant.m_Type);\
        ASSERT_EQ(hash(prop ".x"), desc.m_ElementIds[0]);\
        ASSERT_EQ(hash(prop ".y"), desc.m_ElementIds[1]);\
        ASSERT_EQ(hash(prop ".z"), desc.m_ElementIds[2]);\
        ASSERT_EQ(hash(prop ".w"), desc.m_ElementIds[3]);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
        ASSERT_NEAR(v.getW(), desc.m_ValuePtr[3], epsilon);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".x"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".y"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".z"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".w"), desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getW(), desc.m_ValuePtr[0], epsilon);\
}

#define ASSERT_SET_PROP_V4(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyVar var(v);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, hash(prop), var));\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), desc));\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
        ASSERT_NEAR(v.getW(), desc.m_ValuePtr[3], epsilon);\
\
        float v0 = v.getX() + 1.0f;\
        dmhash_t id = hash(prop ".x");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getY() + 1.0f;\
        id = hash(prop ".y");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getZ() + 1.0f;\
        id = hash(prop ".z");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getW() + 1.0f;\
        id = hash(prop ".w");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
    }

TEST_F(PropsTest, PropsGetSet)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    dmGameObject::Init(m_Collection);

    float epsilon = 0.000001f;

    Vector3 pos(1, 2, 3);
    dmGameObject::SetPosition(go, Point3(pos));
    ASSERT_GET_PROP_V3(go, "position", pos, epsilon);
    pos *= 2.0f;
    ASSERT_SET_PROP_V3(go, "position", pos, epsilon);

    // Uniform scale
    dmGameObject::SetScale(go, 2.0f);
    ASSERT_GET_PROP_V3(go, "scale", Vector3(2.0f), epsilon);
    ASSERT_SET_PROP_V3(go, "scale", Vector3(3.0f), epsilon);

    // Non-uniform scale
    dmGameObject::SetScale(go, 2.0f);
    ASSERT_SET_PROP_NUM(go, "scale.x", 3.0f, epsilon);
    ASSERT_GET_PROP_NUM(go, "scale.y", 2.0f, epsilon);
    ASSERT_GET_PROP_NUM(go, "scale.z", 2.0f, epsilon);

    dmGameObject::SetScale(go, 2.0f);
    ASSERT_SET_PROP_NUM(go, "scale.y", 3.0f, epsilon);
    ASSERT_GET_PROP_NUM(go, "scale.x", 2.0f, epsilon);
    ASSERT_GET_PROP_NUM(go, "scale.z", 2.0f, epsilon);

    dmGameObject::SetScale(go, 2.0f);
    ASSERT_SET_PROP_NUM(go, "scale.z", 3.0f, epsilon);
    ASSERT_GET_PROP_NUM(go, "scale.x", 2.0f, epsilon);
    ASSERT_GET_PROP_NUM(go, "scale.y", 2.0f, epsilon);

    Quat rot(1, 2, 3, 4);
    dmGameObject::SetRotation(go, rot);
    ASSERT_GET_PROP_V4(go, "rotation", rot, epsilon);
    rot *= 2.0f;
    ASSERT_SET_PROP_V4(go, "rotation", rot, epsilon);

    epsilon = 0.02f;

    rot = Quat(M_SQRT1_2, 0, 0, M_SQRT1_2);   // Based on conversion tool (YZX rotation sequence) on http://www.euclideanspace.com/maths/geometry/rotations/conversions/eulerToQuaternion/
    dmGameObject::SetRotation(go, rot);
    Vector3 euler(90.0f, 0.0f, 0.0f);               // bank, heading, attitude

    ASSERT_GET_PROP_V3(go, "euler", euler, epsilon);
    euler = Vector3(0.0f, 0.0f, 1.0);
    ASSERT_SET_PROP_V3(go, "euler", euler, epsilon);

    dmGameObject::Delete(m_Collection, go, false);
}

#undef ASSERT_GET_PROP_NUM
#undef ASSERT_SET_PROP_NUM
#undef ASSERT_GET_PROP_V1
#undef ASSERT_SET_PROP_V1
#undef ASSERT_GET_PROP_V3
#undef ASSERT_SET_PROP_V3
#undef ASSERT_GET_PROP_V4
#undef ASSERT_SET_PROP_V4

TEST_F(PropsTest, PropsGetSetScript)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_get_set.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_TRUE(dmGameObject::Init(collection));
    dmGameObject::UpdateContext context;
    context.m_DT = 1 / 60.0f;
    ASSERT_TRUE(dmGameObject::Update(collection, &context));
    dmResource::Release(m_Factory, collection);
}

#define ASSERT_SPAWN_FAILS(path)\
    dmGameObject::HInstance i = Spawn(m_Factory, m_Collection, path, dmHashString64("id"), (uint8_t*)0x0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));\
    ASSERT_EQ(0, i);

TEST_F(PropsTest, PropsGetBadURL)
{
    ASSERT_SPAWN_FAILS("/props_get_bad_url.goc");
}

TEST_F(PropsTest, PropsGetNotFound)
{
    ASSERT_SPAWN_FAILS("/props_get_not_found.goc");
}

TEST_F(PropsTest, PropsGetCompNotFound)
{
    ASSERT_SPAWN_FAILS("/props_get_comp_not_found.goc");
}

TEST_F(PropsTest, PropsSetBadURL)
{
    ASSERT_SPAWN_FAILS("/props_set_bad_url.goc");
}

TEST_F(PropsTest, PropsSetNoInst)
{
    ASSERT_SPAWN_FAILS("/props_set_no_inst.goc");
}

TEST_F(PropsTest, PropsSetNotFound)
{
    ASSERT_SPAWN_FAILS("/props_set_not_found.goc");
}

TEST_F(PropsTest, PropsSetInvType)
{
    ASSERT_SPAWN_FAILS("/props_set_inv_type.goc");
}

TEST_F(PropsTest, PropsSetCompNotFound)
{
    ASSERT_SPAWN_FAILS("/props_set_comp_not_found.goc");
}

#undef ASSERT_SPAWN_FAILS

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
