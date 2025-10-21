// Copyright 2020-2025 The Defold Foundation
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

#include <stdint.h>

#include <dlib/hash.h>
#include <dlib/align.h>
#include <dlib/log.h>

#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../gameobject_props_lua.h"

#include <dmsdk/resource/resource.h>

#include <dmsdk/resource/resource.hpp>

using namespace dmVMath;

class FactoryTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/src/gameobject/test/factory");
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

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        HResourceType resource_type;
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

    void TearDown() override
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
    dmHashTable64<void*> m_Contexts;
};

static dmResource::Result NullResourceCreate(const dmResource::ResourceCreateParams* params)
{
    ResourceDescriptorSetResource(params->m_Resource, (void*)1); // asserted for != 0 in dmResource
    return dmResource::RESULT_OK;
}

static dmResource::Result NullResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    return dmResource::RESULT_OK;
}

static dmGameObject::CreateResult TestComponentCreate(const dmGameObject::ComponentCreateParams& params)
{
    // Hard coded for the specific case "CreateCallback" below
    dmGameObject::HInstance instance = params.m_Instance;
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

TEST_F(FactoryTest, Factory)
{
    const int count = 10;
    for (int i = 0; i < count; ++i)
    {
        uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
        dmhash_t id = dmGameObject::CreateInstanceId();

        ASSERT_NE(0u, id);
        dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test.goc", id, 0, Point3(), Quat(), Vector3(1, 1, 1));
        ASSERT_NE(0u, (uintptr_t)instance);
    }
}

TEST_F(FactoryTest, FactoryScale)
{
    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::CreateInstanceId();
    ASSERT_NE(0u, id);
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test.goc", id, 0, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_EQ(2.0f, dmGameObject::GetUniformScale(instance));
}

TEST_F(FactoryTest, FactoryProperties)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_newtable(L);
    lua_pushnumber(L, 3);
    lua_setfield(L, -2, "number");

    dmScript::PushHash(L, dmHashString64("hash3"));
    lua_setfield(L, -2, "hash");

    dmMessage::URL url;
    url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    url.m_Path = dmHashString64("/url3");
    url.m_Fragment = 0;
    dmScript::PushURL(L, url);
    lua_setfield(L, -2, "url");

    dmScript::PushVector3(L, Vector3(11, 12, 13));
    lua_setfield(L, -2, "vec3");

    dmScript::PushVector4(L, Vector4(14, 15, 16, 17));
    lua_setfield(L, -2, "vec4");

    dmScript::PushQuat(L, Quat(18, 19, 20, 21));
    lua_setfield(L, -2, "quat");

    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "bool");

    dmGameObject::HPropertyContainer properties = dmGameObject::PropertyContainerCreateFromLua(L, -1);
    lua_pop(L, 1);

    dmGameObject::PropertyContainerPrint(properties);

    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::CreateInstanceId();
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, properties, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_NE((void*)0, instance);

    index = dmGameObject::AcquireInstanceIndex(m_Collection);
    id = dmGameObject::CreateInstanceId();
    instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, properties, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_NE((void*)0, instance);

    dmGameObject::PropertyContainerDestroy(properties);
}

TEST_F(FactoryTest, FactoryPropertiesFailUnsupportedType)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_newtable(L);
    lua_pushliteral(L, "number");
    lua_pushliteral(L, "fail");
    lua_rawset(L, -3);

    dmGameObject::HPropertyContainer properties = dmGameObject::PropertyContainerCreateFromLua(L, -1);
    lua_pop(L, 1);

    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::CreateInstanceId();
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, properties, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_EQ((void*)0, instance);

    dmGameObject::PropertyContainerDestroy(properties);
}

TEST_F(FactoryTest, FactoryPropertiesFailTypeMismatch)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_newtable(L);
    lua_pushliteral(L, "number");
    dmScript::PushHash(L, (dmhash_t)0);
    lua_rawset(L, -3);

    dmGameObject::HPropertyContainer properties = dmGameObject::PropertyContainerCreateFromLua(L, -1);
    lua_pop(L, 1);

    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::CreateInstanceId();
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_props.goc", id, properties, Point3(), Quat(), Vector3(2, 2, 2));
    ASSERT_EQ((void*)0, instance);

    dmGameObject::PropertyContainerDestroy(properties);
}

TEST_F(FactoryTest, FactoryCreateCallback)
{
    uint32_t index = dmGameObject::AcquireInstanceIndex(m_Collection);
    dmhash_t id = dmGameObject::CreateInstanceId();
    dmGameObject::HInstance instance = Spawn(m_Factory, m_Collection, "/test_create.goc", id, 0, Point3(2.0f, 0.0f, 0.0f), Quat(), Vector3(2, 2, 2));
    ASSERT_NE((void*)0, instance);
}
