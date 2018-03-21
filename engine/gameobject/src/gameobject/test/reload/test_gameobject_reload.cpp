#include <gtest/gtest.h>

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/hash.h>

#include <resource/resource.h>

#include "../gameobject.h"

#include "gameobject/test/reload/test_gameobject_reload_ddf.h"

using namespace Vectormath::Aos;

struct ReloadTargetComponent
{
    char m_Byte;
};

struct ReloadTargetWorld
{
    ReloadTargetComponent* m_Component;
};

class ReloadTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_NewResource = 0x0;

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/reload");
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);

        dmResource::Result e = dmResource::RegisterType(m_Factory, "rt", this, 0, ResReloadTargetCreate, 0, ResReloadTargetDestroy, ResReloadTargetRecreate, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
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
        rt_type.m_OnReloadFunction = CompReloadTargetOnReload;
        rt_type.m_InstanceHasUserData = true;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, rt_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_World = 0x0;

        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
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

    static dmResource::Result ResReloadTargetCreate(const dmResource::ResourceCreateParams& params);
    static dmResource::Result ResReloadTargetDestroy(const dmResource::ResourceDestroyParams& params);
    static dmResource::Result ResReloadTargetRecreate(const dmResource::ResourceRecreateParams& params);

    static dmGameObject::CreateResult CompReloadTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    static dmGameObject::CreateResult CompReloadTargetDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    static dmGameObject::CreateResult CompReloadTargetCreate(const dmGameObject::ComponentCreateParams& params);
    static dmGameObject::CreateResult CompReloadTargetDestroy(const dmGameObject::ComponentDestroyParams& params);
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
};

dmResource::Result ReloadTest::ResReloadTargetCreate(const dmResource::ResourceCreateParams& params)
{
    TestGameObjectDDF::ReloadTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::ReloadTarget>(params.m_Buffer, params.m_BufferSize, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        params.m_Resource->m_Resource = (void*) obj;
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmResource::Result ReloadTest::ResReloadTargetDestroy(const dmResource::ResourceDestroyParams& params)
{
    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

dmResource::Result ReloadTest::ResReloadTargetRecreate(const dmResource::ResourceRecreateParams& params)
{
    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    TestGameObjectDDF::ReloadTarget* obj;
    dmDDF::Result e = dmDDF::LoadMessage<TestGameObjectDDF::ReloadTarget>(params.m_Buffer, params.m_BufferSize, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        params.m_Resource->m_Resource = (void*) obj;
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
    test->m_World->m_Component = 0x0;
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
    rt_world->m_Component = new ReloadTargetComponent();
    *params.m_UserData = (uintptr_t)rt_world->m_Component;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadTest::CompReloadTargetDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    delete rt_world->m_Component;
    rt_world->m_Component = 0x0;
    return dmGameObject::CREATE_RESULT_OK;
}

void ReloadTest::CompReloadTargetOnReload(const dmGameObject::ComponentOnReloadParams& params)
{
    ReloadTest* self = (ReloadTest*) params.m_Context;
    self->m_NewResource = params.m_Resource;
    self->m_World = (ReloadTargetWorld*)params.m_World;
    self->m_World->m_Component = (ReloadTargetComponent*)*params.m_UserData;
}

TEST_F(ReloadTest, TestComponentReload)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/component_reload.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ReloadTargetWorld* world = m_World;
    ReloadTargetComponent* component = m_World->m_Component;

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    bool r = dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_FALSE(r);

    ASSERT_EQ((void*)0, m_NewResource);

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/reload_target.rt", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_EQ(world, m_World);
    ASSERT_EQ(component, m_World->m_Component);

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
    ASSERT_NE((void*) 0, (void*) go);

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/component_reload_fail.scriptc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    dmGameObject::Delete(m_Collection, go, false);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
