// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <jc_test/jc_test.h>

#include <dlib/dstrings.h>
#include <dlib/align.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../gameobject_script.h"
#include "../proto/gameobject/gameobject_ddf.h"
#include "../gameobject_props.h"
#include "../gameobject_props_lua.h"

#include <dmsdk/resource/resource.h>

#include <dmsdk/resource/resource.hpp>

using namespace dmVMath;

dmResource::Result ResCreate(const dmResource::ResourceCreateParams* params)
{
    // The resource is not relevant for this test
    ResourceDescriptorSetResource(params->m_Resource, new uint8_t[4]);
    return dmResource::RESULT_OK;
}
dmResource::Result ResDestroy(const dmResource::ResourceDestroyParams* params)
{
    // The resource is not relevant for this test
    delete [] (uint8_t*)ResourceDescriptorGetResource(params->m_Resource);
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

class PropsTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Path = "build/src/gameobject/test/props";
        m_Factory = dmResource::NewFactory(&params, m_Path);
        dmScript::ContextParams script_context_params = {};
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);

        m_Contexts.SetCapacity(7,16);
        m_Contexts.Put(dmHashString64("goc"), m_Register);
        m_Contexts.Put(dmHashString64("collectionc"), m_Register);
        m_Contexts.Put(dmHashString64("scriptc"), m_ScriptContext);
        m_Contexts.Put(dmHashString64("luac"), &m_ModuleContext);
        dmResource::RegisterTypes(m_Factory, &m_Contexts);

        dmGameObject::ComponentTypeCreateCtx component_create_ctx = {};
        component_create_ctx.m_Script = m_ScriptContext;
        component_create_ctx.m_Register = m_Register;
        component_create_ctx.m_Factory = m_Factory;
        dmGameObject::CreateRegisteredComponentTypes(&component_create_ctx);
        dmGameObject::SortComponentTypes(m_Register);

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);

        // Register dummy physical resource type
        dmResource::Result e = dmResource::RegisterType(m_Factory, "no_user_datac", this, 0, ResCreate, 0, ResDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        HResourceType resource_type;
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

    void TearDown() override
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
    dmHashTable64<void*> m_Contexts;
};

static void SetProperties(dmGameObject::HInstance instance)
{
    dmGameObject::Prototype::Component* components = instance->m_Prototype->m_Components;
    uint32_t count = instance->m_Prototype->m_ComponentCount;
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

static dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id, dmGameObject::HPropertyContainer properties, const Point3& position, const Quat& rotation, const Vector3& scale)
{
    dmGameObject::HPrototype prototype = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&prototype) == dmResource::RESULT_OK) {
        dmGameObject::HInstance result = dmGameObject::Spawn(collection, prototype, prototype_name, id, properties, position, rotation, scale);
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
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    // Twice since we had crash here
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/props_default.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(PropsTest, PropsGO)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    ASSERT_NE((void*) 0, (void*) go);
    SetProperties(go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/props_go.scriptc", 0x0));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(PropsTest, PropsCollection)
{
    dmGameObject::HCollection collection;
    dmResource::Result res = dmResource::Get(m_Factory, "/props_coll.collectionc", (void**)&collection);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    bool result = dmGameObject::Init(collection);
    ASSERT_TRUE(result);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/props_coll.scriptc", 0x0));
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

    lua_pushnumber(L, 200);
    lua_setfield(L, -2, "number");

    dmScript::PushHash(L, dmHashString64("hash"));
    lua_setfield(L, -2, "hash");

    dmMessage::URL url;
    dmMessage::ResetURL(&url);
    url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    url.m_Path = dmHashString64("/path");
    dmScript::PushURL(L, url);
    lua_setfield(L, -2, "url");

    dmScript::PushVector3(L, Vector3(1, 2, 3));
    lua_setfield(L, -2, "vector3");

    dmScript::PushVector4(L, Vector4(1, 2, 3, 4));
    lua_setfield(L, -2, "vector4");

    dmScript::PushQuat(L, Quat(1, 2, 3, 4));
    lua_setfield(L, -2, "quat");

    dmGameObject::HPropertyContainer properties = dmGameObject::PropertyContainerCreateFromLua(L, -1);
    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
    ASSERT_NE((dmGameObject::HPropertyContainer)0, properties);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/props_spawn.goc", dmHashString64("test_id"), properties, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    // Script init is run in spawn which verifies the properties
    ASSERT_NE((void*)0u, instance);

    dmGameObject::PropertyContainerDestroy(properties);
}

TEST_F(PropsTest, PropsSpawnNoProperties)
{
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/props_go.goc", dmHashString64("test_id"), 0, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
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

TEST_F(PropsTest, PropsNopDefInInit)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_nop_def_in_init.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool result = dmGameObject::Init(m_Collection);
    ASSERT_TRUE(result);
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
        dmGameObject::PropertyOptions opt;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v0, desc.m_Variant.m_Number, epsilon);\
    }\

#define ASSERT_SET_PROP_NUM(go, prop, v0, epsilon)\
    {\
        dmGameObject::PropertyOptions opt;\
        dmGameObject::PropertyVar var(v0);\
        dmGameObject::SetProperty(go, 0, hash(prop), opt, var);\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_NEAR(v0, desc.m_Variant.m_Number, epsilon);\
    }\

#define ASSERT_GET_PROP_V1(go, prop, v0, epsilon)\
    {\
        dmGameObject::SetScale(go, v0);\
        dmGameObject::PropertyDesc desc;\
        dmGameObject::PropertyOptions opt;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v0, *desc.m_ValuePtr, epsilon);\
    }\

#define ASSERT_SET_PROP_V1(go, prop, v0, epsilon)\
    {\
        dmGameObject::PropertyOptions opt;\
        dmGameObject::PropertyVar var(v0);\
        dmGameObject::SetProperty(go, 0, hash(prop), opt, var);\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_NEAR(v0, *desc.m_ValuePtr, epsilon);\
    }\

#define ASSERT_GET_PROP_V3(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyDesc desc;\
        dmGameObject::PropertyOptions opt;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);\
        ASSERT_EQ(hash(prop ".x"), desc.m_ElementIds[0]);\
        ASSERT_EQ(hash(prop ".y"), desc.m_ElementIds[1]);\
        ASSERT_EQ(hash(prop ".z"), desc.m_ElementIds[2]);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".x"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".y"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".z"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[0], epsilon);\
    }

#define ASSERT_SET_PROP_V3(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyVar var(v);\
        dmGameObject::PropertyOptions opt;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, hash(prop), opt, var));\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
\
        float v0 = v.getX() + 1.0f;\
        dmhash_t id = hash(prop ".x");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getY() + 1.0f;\
        id = hash(prop ".y");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getZ() + 1.0f;\
        id = hash(prop ".z");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
    }

#define ASSERT_GET_PROP_V4(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyDesc desc;\
        dmGameObject::PropertyOptions opt;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_TRUE(dmGameObject::PROPERTY_TYPE_VECTOR4 == desc.m_Variant.m_Type || dmGameObject::PROPERTY_TYPE_QUAT == desc.m_Variant.m_Type);\
        ASSERT_EQ(hash(prop ".x"), desc.m_ElementIds[0]);\
        ASSERT_EQ(hash(prop ".y"), desc.m_ElementIds[1]);\
        ASSERT_EQ(hash(prop ".z"), desc.m_ElementIds[2]);\
        ASSERT_EQ(hash(prop ".w"), desc.m_ElementIds[3]);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
        ASSERT_NEAR(v.getW(), desc.m_ValuePtr[3], epsilon);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".x"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".y"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".z"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[0], epsilon);\
\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop ".w"), opt, desc));\
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, desc.m_Variant.m_Type);\
        ASSERT_NEAR(v.getW(), desc.m_ValuePtr[0], epsilon);\
}

#define ASSERT_SET_PROP_V4(go, prop, v, epsilon)\
    {\
        dmGameObject::PropertyVar var(v);\
        dmGameObject::PropertyOptions opt;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, hash(prop), opt, var));\
        dmGameObject::PropertyDesc desc;\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, hash(prop), opt, desc));\
        ASSERT_NEAR(v.getX(), desc.m_ValuePtr[0], epsilon);\
        ASSERT_NEAR(v.getY(), desc.m_ValuePtr[1], epsilon);\
        ASSERT_NEAR(v.getZ(), desc.m_ValuePtr[2], epsilon);\
        ASSERT_NEAR(v.getW(), desc.m_ValuePtr[3], epsilon);\
\
        float v0 = v.getX() + 1.0f;\
        dmhash_t id = hash(prop ".x");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getY() + 1.0f;\
        id = hash(prop ".y");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getZ() + 1.0f;\
        id = hash(prop ".z");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
\
        v0 = v.getW() + 1.0f;\
        id = hash(prop ".w");\
        var = dmGameObject::PropertyVar(v0);\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, id, opt, var));\
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, 0, id, opt, desc));\
        ASSERT_NEAR(v0, desc.m_ValuePtr[0], epsilon);\
    }

TEST_F(PropsTest, PropsGetSet)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/props_go.goc");
    SetProperties(go);
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

float DiffVector3(dmVMath::Vector3 a, dmVMath::Vector3 b)
{
    return dmMath::Abs(dmVMath::Length(a) - dmVMath::Length(b));
}
float DiffVector4(dmVMath::Vector4 a, dmVMath::Vector4 b)
{
    return dmMath::Abs(dmVMath::Length(a) - dmVMath::Length(b));
}
float DiffQuat(dmVMath::Quat a, dmVMath::Quat b)
{
    return dmMath::Abs(dmVMath::Length(a) - dmVMath::Length(b));
}

TEST_F(PropsTest, PropsGetSetAs)
{
    dmGameObject::HInstance instance = dmGameObject::New(m_Collection, "/props_go.goc");
    SetProperties(instance);
    dmGameObject::Init(m_Collection);

    dmGameObject::PropertyResult r;

    dmhash_t geth;
    dmhash_t seth = hash("foobar");
    // check initial value
    r = dmGameObject::GetPropertyAsHash(instance, hash("script"), hash("hash"), &geth);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(hash("hash"), geth);
    // set new value
    r = dmGameObject::SetPropertyFromHash(instance, hash("script"), hash("hash"), seth);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    // check that it was set
    r = dmGameObject::GetPropertyAsHash(instance, hash("script"), hash("hash"), &geth);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(seth, geth);
    // check that it verifies property type
    r = dmGameObject::GetPropertyAsHash(instance, 0, hash("rotation"), &geth);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromHash(instance, 0, hash("rotation"), seth);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    float getf;
    float setf = -123;
    r = dmGameObject::GetPropertyAsFloat(instance, hash("script"), hash("number"), &getf);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(200, getf);
    r = dmGameObject::SetPropertyFromFloat(instance, hash("script"), hash("number"), setf);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsFloat(instance, hash("script"), hash("number"), &getf);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(setf, getf);
    r = dmGameObject::GetPropertyAsFloat(instance, 0, hash("rotation"), &getf);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromFloat(instance, 0, hash("rotation"), setf);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    dmVMath::Vector3 getvec3;
    dmVMath::Vector3 setvec3 = dmVMath::Vector3(-10, -1, 9);
    r = dmGameObject::GetPropertyAsVector3(instance, hash("script"), hash("vec3"), &getvec3);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_LT(DiffVector3(dmVMath::Vector3(1, 2, 3), getvec3), 0.0001f);
    r = dmGameObject::SetPropertyFromVector3(instance, hash("script"), hash("vec3"), setvec3);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsVector3(instance, hash("script"), hash("vec3"), &getvec3);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_LT(DiffVector3(setvec3, getvec3), 0.0001f);
    r = dmGameObject::GetPropertyAsVector3(instance, 0, hash("rotation"), &getvec3);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromVector3(instance, 0, hash("rotation"), setvec3);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    dmVMath::Vector4 getvec4;
    dmVMath::Vector4 setvec4 = dmVMath::Vector4(-10, -1, 9, 500);
    r = dmGameObject::GetPropertyAsVector4(instance, hash("script"), hash("vec4"), &getvec4);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_LT(DiffVector4(dmVMath::Vector4(4, 5, 6, 7), getvec4), 0.0001f);
    r = dmGameObject::SetPropertyFromVector4(instance, hash("script"), hash("vec4"), setvec4);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsVector4(instance, hash("script"), hash("vec4"), &getvec4);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_LT(DiffVector4(setvec4, getvec4), 0.0001f);
    r = dmGameObject::GetPropertyAsVector4(instance, 0, hash("rotation"), &getvec4);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromVector4(instance, 0, hash("rotation"), setvec4);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    dmVMath::Quat getquat;
    dmVMath::Quat setquat = dmVMath::Quat(-10, -1, 9, 500);
    r = dmGameObject::GetPropertyAsQuat(instance, hash("script"), hash("quat"), &getquat);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_LT(DiffQuat(dmVMath::Quat(8, 9, 10, 11), getquat), 0.0001f);
    r = dmGameObject::SetPropertyFromQuat(instance, hash("script"), hash("quat"), setquat);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsQuat(instance, hash("script"), hash("quat"), &getquat);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_LT(DiffQuat(setquat, getquat), 0.0001f);
    r = dmGameObject::GetPropertyAsQuat(instance, 0, hash("position"), &getquat);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromQuat(instance, 0, hash("position"), setquat);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    bool getb;
    bool setb = 1;
    r = dmGameObject::GetPropertyAsBool(instance, hash("script"), hash("bool"), &getb);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(0, getb);
    r = dmGameObject::SetPropertyFromBool(instance, hash("script"), hash("bool"), setb);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsBool(instance, hash("script"), hash("bool"), &getb);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(setb, getb);
    r = dmGameObject::GetPropertyAsBool(instance, 0, hash("rotation"), &getb);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromBool(instance, 0, hash("position"), setb);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    dmMessage::URL geturl;
    dmMessage::URL seturl;
    seturl.m_Socket = hash("foo");
    seturl.m_Path = hash("bar");
    seturl.m_Fragment = hash("baz");
    r = dmGameObject::GetPropertyAsURL(instance, hash("script"), hash("url"), &geturl);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(hash("/path"), geturl.m_Path);
    r = dmGameObject::SetPropertyFromURL(instance, hash("script"), hash("url"), seturl);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsURL(instance, hash("script"), hash("url"), &geturl);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    ASSERT_EQ(seturl.m_Socket, geturl.m_Socket);
    ASSERT_EQ(seturl.m_Path, geturl.m_Path);
    ASSERT_EQ(seturl.m_Fragment, geturl.m_Fragment);
    r = dmGameObject::GetPropertyAsURL(instance, hash("script"), hash("vec3"), &geturl);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::GetPropertyAsURL(instance, 0, hash("rotation"), &geturl);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);
    r = dmGameObject::SetPropertyFromURL(instance, 0, hash("position"), seturl);
    ASSERT_NE(dmGameObject::PROPERTY_RESULT_OK, r);

    dmGameObject::Delete(m_Collection, instance, false);
}

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
    dmGameObject::HInstance i = Spawn(m_Factory, m_Collection, path, dmHashString64("id"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));\
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

TEST(GameObjectProps, TestPropertyContainerCreate)
{
    const float numberFirst = 1.f;
    const float numberSecond = 2.f;
    dmhash_t hashFirst = dmHashString64("hash_first");
    const char urlStringFirst[] = "url_string_first";
    const char urlStringSecond[] = "url_string_second";
    const char urlFirst[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
                                1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
                                1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
                                1, 2};
    float vector3First[3] = {1, 2, 3};
    float vector3Second[3] = {-1, -2, -3};
    float vector4First[4] = {1, 2, 3};
    float quatFirst[4] = {1, 2, 3};
    bool boolFirst = true;
    bool boolSecond = false;

    dmGameObject::PropertyContainerBuilderParams params;
    params.m_NumberCount = 2;
    params.m_HashCount = 1;
    params.m_URLStringCount = 2;
    params.m_URLCount = 1;
    params.m_Vector3Count = 2;
    params.m_Vector4Count = 1;
    params.m_QuatCount = 1;
    params.m_BoolCount = 2;
    params.m_URLStringSize += strlen("url_string_first") + 1;
    params.m_URLStringSize += strlen("url_string_second") + 1;
    dmGameObject::HPropertyContainerBuilder builder = dmGameObject::PropertyContainerCreateBuilder(params);
    ASSERT_NE(builder, (dmGameObject::HPropertyContainerBuilder)0x0);
    dmGameObject::PropertyContainerPushFloat(builder, dmHashString64("NumberFirst"), numberFirst);
    dmGameObject::PropertyContainerPushFloat(builder, dmHashString64("NumberSecond"), numberSecond);
    dmGameObject::PropertyContainerPushHash(builder, dmHashString64("HashFirst"), hashFirst);
    dmGameObject::PropertyContainerPushURLString(builder, dmHashString64("URLStringFirst"), urlStringFirst);
    dmGameObject::PropertyContainerPushURLString(builder, dmHashString64("URLStringSecond"), urlStringSecond);
    dmGameObject::PropertyContainerPushURL(builder, dmHashString64("URLFirst"), urlFirst);
    dmGameObject::PropertyContainerPushVector3(builder, dmHashString64("Vector3First"), vector3First);
    dmGameObject::PropertyContainerPushVector3(builder, dmHashString64("Vector3Second"), vector3Second);
    dmGameObject::PropertyContainerPushVector4(builder, dmHashString64("Vector4First"), vector4First);
    dmGameObject::PropertyContainerPushQuat(builder, dmHashString64("QuatFirst"), quatFirst);
    dmGameObject::PropertyContainerPushBool(builder, dmHashString64("BoolFirst"), boolFirst);
    dmGameObject::PropertyContainerPushBool(builder, dmHashString64("BoolSecond"), boolSecond);
    dmGameObject::HPropertyContainer c = dmGameObject::PropertyContainerCreate(builder);
    ASSERT_NE(c, (dmGameObject::HPropertyContainer)0x0);
    dmGameObject::PropertyVar var;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_NOT_FOUND, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("NonExistant"), var));

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("NumberFirst"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, var.m_Type);
    ASSERT_EQ(numberFirst, var.m_Number);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("NumberSecond"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, var.m_Type);
    ASSERT_EQ(numberSecond, var.m_Number);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("HashFirst"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_HASH, var.m_Type);
    ASSERT_EQ(hashFirst, var.m_Hash);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("URLFirst"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_URL, var.m_Type);
    ASSERT_EQ(0, memcmp(urlFirst, var.m_URL, 32));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("Vector3First"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, var.m_Type);
    ASSERT_EQ(vector3First[0], var.m_V4[0]);
    ASSERT_EQ(vector3First[1], var.m_V4[1]);
    ASSERT_EQ(vector3First[2], var.m_V4[2]);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("Vector3Second"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, var.m_Type);
    ASSERT_EQ(vector3Second[0], var.m_V4[0]);
    ASSERT_EQ(vector3Second[1], var.m_V4[1]);
    ASSERT_EQ(vector3Second[2], var.m_V4[2]);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("Vector4First"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR4, var.m_Type);
    ASSERT_EQ(vector4First[0], var.m_V4[0]);
    ASSERT_EQ(vector4First[1], var.m_V4[1]);
    ASSERT_EQ(vector4First[2], var.m_V4[2]);
    ASSERT_EQ(vector4First[3], var.m_V4[3]);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("QuatFirst"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_QUAT, var.m_Type);
    ASSERT_EQ(vector4First[0], var.m_V4[0]);
    ASSERT_EQ(vector4First[1], var.m_V4[1]);
    ASSERT_EQ(vector4First[2], var.m_V4[2]);
    ASSERT_EQ(vector4First[3], var.m_V4[3]);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("BoolFirst"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_BOOLEAN, var.m_Type);
    ASSERT_EQ(boolFirst, var.m_Bool);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)c, dmHashString64("BoolSecond"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_BOOLEAN, var.m_Type);
    ASSERT_EQ(boolSecond, var.m_Bool);
    dmGameObject::PropertyContainerDestroy(c);
}

TEST(GameObjectProps, TestMergePropertyContainer)
{
    const float NUMBER = 1.f;
    const char URL[] = "/url";
    const char URL_OVERIDE[32] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
                                1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
                                1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
                                1, 2};
    float VECTOR3[3] = {1, 2, 3};
    float VECTOR3_OVERIDE[3] = {-1, -2, -3};
    float VECTOR3_2[3] = {10, -21, -30};

    dmGameObject::PropertyContainerBuilderParams params;
    params.m_NumberCount = 1;
    params.m_URLStringCount = 1;
    params.m_URLCount = 0;
    params.m_Vector3Count = 1;
    params.m_URLStringSize += strlen("/url") + 1;
    dmGameObject::HPropertyContainerBuilder builder = dmGameObject::PropertyContainerCreateBuilder(params);
    ASSERT_NE(builder, (dmGameObject::HPropertyContainerBuilder)0x0);
    dmGameObject::PropertyContainerPushFloat(builder, dmHashString64("number"), NUMBER);
    dmGameObject::PropertyContainerPushURLString(builder, dmHashString64("url"), URL);
    dmGameObject::PropertyContainerPushVector3(builder, dmHashString64("vector3"), VECTOR3);

    dmGameObject::HPropertyContainer c = dmGameObject::PropertyContainerCreate(builder);
    ASSERT_NE(c, (dmGameObject::HPropertyContainer)0x0);

    params = dmGameObject::PropertyContainerBuilderParams();
    params.m_URLCount = 1;
    params.m_Vector3Count = 2;
    builder = dmGameObject::PropertyContainerCreateBuilder(params);
    ASSERT_NE(builder, (dmGameObject::HPropertyContainerBuilder)0x0);
    dmGameObject::PropertyContainerPushURL(builder, dmHashString64("url"), URL_OVERIDE);
    dmGameObject::PropertyContainerPushVector3(builder, dmHashString64("vector3"), VECTOR3_OVERIDE);
    dmGameObject::PropertyContainerPushVector3(builder, dmHashString64("vector3-2"), VECTOR3_2);

    dmGameObject::HPropertyContainer o = dmGameObject::PropertyContainerCreate(builder);
    ASSERT_NE(o, (dmGameObject::HPropertyContainer)0x0);

    dmGameObject::HPropertyContainer m = dmGameObject::PropertyContainerMerge(c, o);
    ASSERT_NE(m, (dmGameObject::HPropertyContainer)0x0);
    dmGameObject::PropertyContainerDestroy(c);
    dmGameObject::PropertyContainerDestroy(o);

    dmGameObject::PropertyVar var;

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)m, dmHashString64("number"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_NUMBER, var.m_Type);
    ASSERT_EQ(NUMBER, var.m_Number);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)m, dmHashString64("url"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_URL, var.m_Type);
    ASSERT_EQ(0, memcmp(URL_OVERIDE, var.m_URL, 32));

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)m, dmHashString64("vector3"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, var.m_Type);
    ASSERT_EQ(VECTOR3_OVERIDE[0], var.m_V4[0]);
    ASSERT_EQ(VECTOR3_OVERIDE[1], var.m_V4[1]);
    ASSERT_EQ(VECTOR3_OVERIDE[2], var.m_V4[2]);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::PropertyContainerGetPropertyCallback(0x0, (uintptr_t)m, dmHashString64("vector3-2"), var));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, var.m_Type);
    ASSERT_EQ(VECTOR3_2[0], var.m_V4[0]);
    ASSERT_EQ(VECTOR3_2[1], var.m_V4[1]);
    ASSERT_EQ(VECTOR3_2[2], var.m_V4[2]);

    dmGameObject::PropertyContainerDestroy(m);
}
