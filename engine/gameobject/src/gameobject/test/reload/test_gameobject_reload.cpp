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

#include <dmsdk/dlib/vmath.h>

#include <dlib/hash.h>
#include <dlib/path.h>
#include <dlib/testutil.h>

#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../gameobject_props.h"
#include "../component.h"

#include "gameobject/test/reload/test_gameobject_reload_ddf.h"

#include <dmsdk/resource/resource.h>

using namespace dmVMath;

struct ReloadTargetComponent
{
    char m_Byte;
};

static const uint32_t MAX_COMP_COUNT = 2;

struct ReloadTargetWorld
{
    ReloadTargetWorld() {
        memset(this, 0, sizeof(*this));
    }
    ReloadTargetComponent* m_Components[MAX_COMP_COUNT];
    int m_CreateCount;
    int m_DestroyCount;
    int m_InitCount;
    int m_FinalCount;
    int m_AddToUpdateCount;
    int m_ReloadCount;
    bool m_ChangeReloadTransform;
    bool m_ReloadQuaternionOnly;
    float m_FinalPositionX;
    Quat m_FinalRotation;
    Quat m_FinalWrittenRotation;
    uint16_t m_InitPrototypeIndex;
    uint16_t m_FinalPrototypeIndex;
};

static void ResetWorldCounters(ReloadTargetWorld* world) {
    world->m_CreateCount = 0;
    world->m_DestroyCount = 0;
    world->m_InitCount = 0;
    world->m_FinalCount = 0;
    world->m_AddToUpdateCount = 0;
    world->m_ReloadCount = 0;
}

class ReloadTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_NewResource = 0x0;

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        char path[DMPATH_MAX_PATH];
        m_Factory = dmResource::NewFactory(&params, dmTestUtil::MakeHostPath(path, sizeof(path), "build/src/gameobject/test/reload"));

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

        dmResource::Result e = dmResource::RegisterType(m_Factory, "rt", this, 0, ResReloadTargetCreate, 0, ResReloadTargetDestroy, ResReloadTargetRecreate);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        HResourceType resource_type;
        e = dmResource::GetTypeFromExtension(m_Factory, "rt", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType rt_type;
        rt_type.m_Name = "rt";
        rt_type.m_ResourceType = resource_type;
        rt_type.m_Context = this;
        rt_type.m_NewWorldFunction = CompReloadTargetNewWorld;
        rt_type.m_DeleteWorldFunction = CompReloadTargetDeleteWorld;
        rt_type.m_CreateFunction = CompReloadTargetCreate;
        rt_type.m_DestroyFunction = CompReloadTargetDestroy;
        rt_type.m_InitFunction = CompReloadTargetInit;
        rt_type.m_FinalFunction = CompReloadTargetFinal;
        rt_type.m_AddToUpdateFunction = CompReloadTargetAddToUpdate;
        rt_type.m_OnReloadFunction = CompReloadTargetOnReload;
        rt_type.m_InstanceHasUserData = true;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, rt_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_World = 0x0;

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0x0);
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

    static dmResource::Result ResReloadTargetCreate(const dmResource::ResourceCreateParams* params);
    static dmResource::Result ResReloadTargetDestroy(const dmResource::ResourceDestroyParams* params);
    static dmResource::Result ResReloadTargetRecreate(const dmResource::ResourceRecreateParams* params);

    static dmGameObject::CreateResult CompReloadTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    static dmGameObject::CreateResult CompReloadTargetDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    static dmGameObject::CreateResult CompReloadTargetCreate(const dmGameObject::ComponentCreateParams& params);
    static dmGameObject::CreateResult CompReloadTargetDestroy(const dmGameObject::ComponentDestroyParams& params);
    static dmGameObject::CreateResult CompReloadTargetInit(const dmGameObject::ComponentInitParams& params);
    static dmGameObject::CreateResult CompReloadTargetFinal(const dmGameObject::ComponentFinalParams& params);
    static dmGameObject::CreateResult CompReloadTargetAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    static void CompReloadTargetOnReload(const dmGameObject::ComponentOnReloadParams& params);

public:

    void* m_NewResource;
    ReloadTargetWorld* m_World;

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

dmResource::Result ReloadTest::ResReloadTargetCreate(const dmResource::ResourceCreateParams* params)
{
    TestGameObjectDDF::ReloadTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::ReloadTarget>(params->m_Buffer, params->m_BufferSize, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        ResourceDescriptorSetResource(params->m_Resource, obj);
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmResource::Result ReloadTest::ResReloadTargetDestroy(const dmResource::ResourceDestroyParams* params)
{
    dmDDF::FreeMessage((void*) ResourceDescriptorGetResource(params->m_Resource));
    return dmResource::RESULT_OK;
}

dmResource::Result ReloadTest::ResReloadTargetRecreate(const dmResource::ResourceRecreateParams* params)
{
    dmDDF::FreeMessage((void*) ResourceDescriptorGetResource(params->m_Resource));
    TestGameObjectDDF::ReloadTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::ReloadTarget>(params->m_Buffer, params->m_BufferSize, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        ResourceDescriptorSetResource(params->m_Resource, obj);
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params)
{
    ReloadTest* test = (ReloadTest*)params.m_Context;
    test->m_World = new ReloadTargetWorld();
    *params.m_World = test->m_World;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    delete rt_world;
    ReloadTest* test = (ReloadTest*)params.m_Context;
    test->m_World = 0x0;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetCreate(const dmGameObject::ComponentCreateParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    rt_world->m_CreateCount++;
    ReloadTargetComponent* comp = 0;
    for (uint32_t i = 0; i < MAX_COMP_COUNT; ++i) {
        if (!rt_world->m_Components[i]) {
            comp = new ReloadTargetComponent();
            rt_world->m_Components[i] = comp;
            break;
        }
    }
    if (!comp) {
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    *params.m_UserData = (uintptr_t)comp;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    rt_world->m_DestroyCount++;
    ReloadTargetComponent* comp = (ReloadTargetComponent*)(*params.m_UserData);
    for (uint32_t i = 0; i < MAX_COMP_COUNT; ++i) {
        if (rt_world->m_Components[i] == comp) {
            rt_world->m_Components[i] = 0;
        }
    }
    delete comp;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetInit(const dmGameObject::ComponentInitParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    rt_world->m_InitCount++;
    rt_world->m_InitPrototypeIndex = dmGameObject::GetInstanceFromHandle(params.m_Instance)->m_PrototypeIndex;
    if (rt_world->m_ChangeReloadTransform)
    {
        dmGameObject::SetPosition(params.m_Instance, Point3(99, 0, 0));
        if (rt_world->m_ReloadQuaternionOnly)
        {
            dmGameObject::SetRotation(params.m_Instance, dmVMath::EulerToQuat(Vector3(20, 30, 120)));
        }
        else
        {
            dmGameObject::PropertyOptions options;
            dmGameObject::SetProperty(params.m_Instance, 0, dmHashString64("euler"), options,
                dmGameObject::PropertyVar(Vector3(20, 30, 120)));
        }
    }
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetFinal(const dmGameObject::ComponentFinalParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    rt_world->m_FinalCount++;
    rt_world->m_FinalPrototypeIndex = dmGameObject::GetInstanceFromHandle(params.m_Instance)->m_PrototypeIndex;
    if (rt_world->m_ChangeReloadTransform)
    {
        rt_world->m_FinalPositionX = dmGameObject::GetPosition(params.m_Instance).getX();
        rt_world->m_FinalRotation = dmGameObject::GetRotation(params.m_Instance);
        dmGameObject::SetPosition(params.m_Instance, Point3(13, 0, 0));
        dmGameObject::PropertyOptions options;
        dmGameObject::SetProperty(params.m_Instance, 0, dmHashString64("euler.z"), options,
            dmGameObject::PropertyVar(13.0f));
        rt_world->m_FinalWrittenRotation = dmGameObject::GetRotation(params.m_Instance);
    }
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    rt_world->m_AddToUpdateCount++;
    return dmGameObject::CREATE_RESULT_OK;
}

void ReloadTest::CompReloadTargetOnReload(const dmGameObject::ComponentOnReloadParams& params)
{
    ReloadTest* self = (ReloadTest*) params.m_Context;
    self->m_NewResource = params.m_Resource;
    self->m_World = (ReloadTargetWorld*)params.m_World;
    self->m_World->m_ReloadCount++;
}

TEST_F(ReloadTest, TestComponentReload)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_reload.goc");
    ASSERT_NE(0, go);

    ReloadTargetWorld* world = m_World;
    ReloadTargetComponent* component = m_World->m_Components[0];

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    bool r = dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_FALSE(r);

    ASSERT_EQ((void*)0, m_NewResource);

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/reload_target.rt", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_EQ(world, m_World);
    ASSERT_EQ(component, m_World->m_Components[0]);

    ASSERT_NE((void*)0, m_NewResource);
    TestGameObjectDDF::ReloadTarget* rt = (TestGameObjectDDF::ReloadTarget*)m_NewResource;
    ASSERT_EQ(123, rt->m_Value);

    rr = dmResource::ReloadResource(m_Factory, "component_reload.scriptc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    r = dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_TRUE(r);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ReloadTest, TestComponentReloadScriptFail)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_reload_fail.goc");
    ASSERT_NE(0, go);

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/component_reload_fail.scriptc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    dmGameObject::Delete(m_Collection, go, false);
}

// Reload callbacks must resolve old/new prototype slots independently and release the temporary old slot afterwards.
TEST_F(ReloadTest, ReloadKeepsSeparatePrototypeSlots)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/rt.goc");
    ASSERT_NE((dmGameObject::HInstance)0, go);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    uint16_t index = dmGameObject::GetInstanceFromHandle(go)->m_PrototypeIndex;
    uint16_t count = m_Register->m_PrototypeIndices.Size();
    dmGameObject::Prototype* prototype = m_Register->m_Prototypes[index];

    for (uint32_t reload = 0; reload < 3; ++reload)
    {
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/rt.goc", 0));
        ASSERT_EQ(index, dmGameObject::GetInstanceFromHandle(go)->m_PrototypeIndex);
        ASSERT_EQ(index, m_World->m_InitPrototypeIndex);
        ASSERT_NE(index, m_World->m_FinalPrototypeIndex);
        ASSERT_EQ((dmGameObject::Prototype*)0, m_Register->m_Prototypes[m_World->m_FinalPrototypeIndex]);
        ASSERT_EQ(prototype, m_Register->m_Prototypes[index]);
        ASSERT_EQ(count, m_Register->m_PrototypeIndices.Size());
    }
}

// Reload init/final callbacks must see independent new/old transforms and Euler axes
// even when their instances share the same collection slot and Euler pool record.
TEST_F(ReloadTest, ReloadCallbacksKeepSeparateTransforms)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/rt.goc");
    ASSERT_NE((dmGameObject::HInstance)0, go);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::SetPosition(go, Point3(7, 0, 0));
    dmGameObject::PropertyOptions options;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, dmHashString64("euler"), options,
        dmGameObject::PropertyVar(Vector3(5, 10, 110))));
    m_World->m_ChangeReloadTransform = true;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/rt.goc", 0));
    ASSERT_NEAR(7.0f, m_World->m_FinalPositionX, 0.000001f);
    ASSERT_NEAR(99.0f, dmGameObject::GetPosition(go).getX(), 0.000001f);
    Quat old_rotation = dmVMath::EulerToQuat(Vector3(5, 10, 110));
    for (uint32_t axis = 0; axis < 4; ++axis)
        ASSERT_NEAR(old_rotation.getElem(axis), m_World->m_FinalRotation.getElem(axis), 0.0001f);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, dmHashString64("euler.x"), options,
        dmGameObject::PropertyVar(40.0f)));
    Quat new_rotation = dmVMath::EulerToQuat(Vector3(40, 30, 120));
    for (uint32_t axis = 0; axis < 4; ++axis)
        ASSERT_NEAR(new_rotation.getElem(axis), dmGameObject::GetRotation(go).getElem(axis), 0.0001f);
    m_World->m_ChangeReloadTransform = false;
}

// Creating Euler records in reload callbacks must preserve previously uncached old/new quaternion axes.
TEST_F(ReloadTest, ReloadCallbacksInitializeUncachedEulerAxes)
{
    for (uint32_t quaternion_only = 0; quaternion_only < 2; ++quaternion_only)
    {
        dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/rt.goc");
        ASSERT_NE((dmGameObject::HInstance)0, go);
        ASSERT_TRUE(dmGameObject::Init(m_Collection));
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        Quat old_rotation = dmVMath::EulerToQuat(Vector3(5, 10, 110));
        dmGameObject::SetRotation(go, old_rotation);
        m_World->m_ChangeReloadTransform = true;
        m_World->m_ReloadQuaternionOnly = quaternion_only != 0;

        ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/rt.goc", 0));
        Vector3 old_euler = dmVMath::QuatToEuler(old_rotation.getX(), old_rotation.getY(), old_rotation.getZ(), old_rotation.getW());
        old_euler.setZ(13.0f);
        Quat expected = dmVMath::EulerToQuat(old_euler);
        for (uint32_t axis = 0; axis < 4; ++axis)
            ASSERT_NEAR(expected.getElem(axis), m_World->m_FinalWrittenRotation.getElem(axis), 0.0001f);

        Vector3 new_euler(20, 30, 120);
        if (quaternion_only)
        {
            Quat new_rotation = dmVMath::EulerToQuat(new_euler);
            new_euler = dmVMath::QuatToEuler(new_rotation.getX(), new_rotation.getY(), new_rotation.getZ(), new_rotation.getW());
        }
        new_euler.setX(40.0f);
        dmGameObject::PropertyOptions options;
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, 0, dmHashString64("euler.x"), options,
            dmGameObject::PropertyVar(40.0f)));
        expected = dmVMath::EulerToQuat(new_euler);
        Quat actual = dmGameObject::GetRotation(go);
        for (uint32_t axis = 0; axis < 4; ++axis)
            ASSERT_NEAR(expected.getElem(axis), actual.getElem(axis), 0.0001f);
        m_World->m_ChangeReloadTransform = false;
        dmGameObject::Delete(m_Collection, go, false);
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    }
}

TEST_F(ReloadTest, TestGameObjectReload)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/rt.goc");
    ASSERT_NE(0, go);
    dmGameObject::SetIdentifier(m_Collection, go, 1);
    ASSERT_EQ(go, dmGameObject::GetInstanceFromIdentifier(m_Collection, 1));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    bool r = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(r);
    r = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(r);

    ResetWorldCounters(m_World);

    ASSERT_EQ(0, m_World->m_CreateCount);
    ASSERT_EQ(0, m_World->m_DestroyCount);
    ASSERT_EQ(0, m_World->m_InitCount);
    ASSERT_EQ(0, m_World->m_FinalCount);
    ASSERT_EQ(0, m_World->m_AddToUpdateCount);
    ASSERT_EQ(0, m_World->m_ReloadCount);

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/rt.goc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    r = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(r);
    r = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(r);
    ASSERT_EQ(go, dmGameObject::GetInstanceFromIdentifier(m_Collection, 1));

    ASSERT_EQ(1, m_World->m_CreateCount);
    ASSERT_EQ(1, m_World->m_DestroyCount);
    ASSERT_EQ(1, m_World->m_InitCount);
    ASSERT_EQ(1, m_World->m_FinalCount);
    ASSERT_EQ(1, m_World->m_AddToUpdateCount);
    ASSERT_EQ(0, m_World->m_ReloadCount);
}
