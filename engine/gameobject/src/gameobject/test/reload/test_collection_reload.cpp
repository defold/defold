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

static const uint32_t MAX_COMP_COUNT = 2;

struct Stats
{
    int m_CreateCount;
    int m_DestroyCount;
    int m_InitCount;
    int m_FinalCount;
    int m_AddToUpdateCount;
    int m_ReloadCount;
};

struct ReloadTargetWorld
{
    ReloadTargetWorld() {
        memset(this, 0, sizeof(*this));
    }
    ReloadTargetComponent* m_Components[MAX_COMP_COUNT];
};

static void ResetWorldCounters(Stats* stats) {
    stats->m_CreateCount = 0;
    stats->m_DestroyCount = 0;
    stats->m_InitCount = 0;
    stats->m_FinalCount = 0;
    stats->m_AddToUpdateCount = 0;
    stats->m_ReloadCount = 0;
}


class ReloadCollectionTest : public ::testing::Test
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
        rt_type.m_InitFunction = CompReloadTargetInit;
        rt_type.m_FinalFunction = CompReloadTargetFinal;
        rt_type.m_AddToUpdateFunction = CompReloadTargetAddToUpdate;
        rt_type.m_OnReloadFunction = CompReloadTargetOnReload;
        rt_type.m_InstanceHasUserData = true;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, rt_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_NumWorlds = 0;
        m_Worlds[0] = 0x0;
        m_Worlds[1] = 0x0;

        m_Collection = 0;
    }

    virtual void TearDown()
    {
        if (m_Collection)
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
    static dmGameObject::CreateResult CompReloadTargetInit(const dmGameObject::ComponentInitParams& params);
    static dmGameObject::CreateResult CompReloadTargetFinal(const dmGameObject::ComponentFinalParams& params);
    static dmGameObject::CreateResult CompReloadTargetAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    static void CompReloadTargetOnReload(const dmGameObject::ComponentOnReloadParams& params);

public:

    Stats m_Stats;
    void* m_NewResource;
    ReloadTargetWorld* m_Worlds[2]; // Before and after reload
    int m_NumWorlds;

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
};

dmResource::Result ReloadCollectionTest::ResReloadTargetCreate(const dmResource::ResourceCreateParams& params)
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

dmResource::Result ReloadCollectionTest::ResReloadTargetDestroy(const dmResource::ResourceDestroyParams& params)
{
    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

dmResource::Result ReloadCollectionTest::ResReloadTargetRecreate(const dmResource::ResourceRecreateParams& params)
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

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params)
{
    ReloadCollectionTest* test = (ReloadCollectionTest*)params.m_Context;
    ReloadTargetWorld* world = new ReloadTargetWorld();
    test->m_Worlds[test->m_NumWorlds++] = world;
    *params.m_World = world;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
{
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    ReloadCollectionTest* test = (ReloadCollectionTest*)params.m_Context;

    for( int i = 0; i < test->m_NumWorlds; ++i)
    {
        if (test->m_Worlds[i] == rt_world) {
            test->m_Worlds[i] = 0x0;
            break;
        }
    }
    delete rt_world;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetCreate(const dmGameObject::ComponentCreateParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    self->m_Stats.m_CreateCount++;
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

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    self->m_Stats.m_DestroyCount++;
    ReloadTargetComponent* comp = (ReloadTargetComponent*)(*params.m_UserData);
    for (uint32_t i = 0; i < MAX_COMP_COUNT; ++i) {
        if (rt_world->m_Components[i] == comp) {
            rt_world->m_Components[i] = 0;
        }
    }
    delete comp;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetInit(const dmGameObject::ComponentInitParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    self->m_Stats.m_InitCount++;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetFinal(const dmGameObject::ComponentFinalParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    self->m_Stats.m_FinalCount++;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    self->m_Stats.m_AddToUpdateCount++;
    return dmGameObject::CREATE_RESULT_OK;
}

void ReloadCollectionTest::CompReloadTargetOnReload(const dmGameObject::ComponentOnReloadParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    self->m_NewResource = params.m_Resource;
    self->m_Stats.m_ReloadCount++;
}

TEST_F(ReloadCollectionTest, TestCollectionReload)
{
    m_Collection = 0;
    dmResource::Get(m_Factory, "/test.collectionc", (void**)&m_Collection);
    ASSERT_NE((void*)0, m_Collection);

    ResetWorldCounters(&m_Stats);
    ASSERT_EQ(0, m_Stats.m_CreateCount);
    ASSERT_EQ(0, m_Stats.m_DestroyCount);
    ASSERT_EQ(0, m_Stats.m_InitCount);
    ASSERT_EQ(0, m_Stats.m_FinalCount);
    ASSERT_EQ(0, m_Stats.m_AddToUpdateCount);
    ASSERT_EQ(0, m_Stats.m_ReloadCount);

    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/test.collectionc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    bool r;
    r = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(r);
    r = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(r);

    // expects 2 component instances to be created
    ASSERT_EQ(2, m_Stats.m_CreateCount);
    ASSERT_EQ(2, m_Stats.m_DestroyCount);
    ASSERT_EQ(0, m_Stats.m_InitCount);          // No init yet
    ASSERT_EQ(0, m_Stats.m_FinalCount);         // No final, since they weren't initialized
    ASSERT_EQ(0, m_Stats.m_AddToUpdateCount);   // as above
    ASSERT_EQ(0, m_Stats.m_ReloadCount);

    dmGameObject::Init(m_Collection);

    ASSERT_EQ(2, m_Stats.m_InitCount);
    ASSERT_EQ(0, m_Stats.m_FinalCount);         // No final, since they weren't recreated
    ASSERT_EQ(2, m_Stats.m_AddToUpdateCount);

    r = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(r);
    r = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(r);

    // Reload it once again, now when the collection is already initialized

    ResetWorldCounters(&m_Stats);
    ASSERT_EQ(0, m_Stats.m_CreateCount);
    ASSERT_EQ(0, m_Stats.m_DestroyCount);
    ASSERT_EQ(0, m_Stats.m_InitCount);
    ASSERT_EQ(0, m_Stats.m_FinalCount);
    ASSERT_EQ(0, m_Stats.m_AddToUpdateCount);
    ASSERT_EQ(0, m_Stats.m_ReloadCount);

    rr = dmResource::ReloadResource(m_Factory, "/test.collectionc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    r = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(r);
    r = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(r);

    // expects 2 component instances to be recreated
    ASSERT_EQ(2, m_Stats.m_CreateCount);
    ASSERT_EQ(2, m_Stats.m_DestroyCount);
    ASSERT_EQ(2, m_Stats.m_InitCount);
    ASSERT_EQ(2, m_Stats.m_FinalCount);
    ASSERT_EQ(2, m_Stats.m_AddToUpdateCount);
    ASSERT_EQ(0, m_Stats.m_ReloadCount);

    dmResource::Release(m_Factory, m_Collection);
    m_Collection = 0;
}
