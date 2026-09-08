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

#include <dlib/hash.h>
#include <dlib/path.h>
#include <dlib/testutil.h>

#include "../gameobject.h"
#include "../component.h"
#include "../../gameobject_private.h"

#include "gameobject/test/reload/test_gameobject_reload_ddf.h"

#include <dmsdk/resource/resource.h>

struct ReloadTargetComponent
{
    dmGameObject::HCollection m_Collection;
    dmGameObject::HGameObject m_GameObject;
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
    int m_HandleMismatchCount;
    int m_MissingCollectionResourceCount;
};

struct ReloadTargetWorld
{
    ReloadTargetWorld() {
        memset(this, 0, sizeof(*this));
    }
    ReloadTargetComponent* m_Components[MAX_COMP_COUNT];
    dmGameObject::HCollection m_Collection;
};

static void ResetWorldCounters(Stats* stats) {
    stats->m_CreateCount = 0;
    stats->m_DestroyCount = 0;
    stats->m_InitCount = 0;
    stats->m_FinalCount = 0;
    stats->m_AddToUpdateCount = 0;
    stats->m_ReloadCount = 0;
    stats->m_HandleMismatchCount = 0;
    stats->m_MissingCollectionResourceCount = 0;
}


class ReloadCollectionTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_NewResource = 0x0;
        m_CreatesUntilFailure = -1;
        m_FailNextInit = false;

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

        m_NumWorlds = 0;
        m_Worlds[0] = 0x0;
        m_Worlds[1] = 0x0;

        m_Collection = 0;
    }

    void TearDown() override
    {
        if (m_Collection)
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

    Stats m_Stats;
    void* m_NewResource;
    ReloadTargetWorld* m_Worlds[3]; // Before and after reload
    int m_NumWorlds;
    int m_CreatesUntilFailure;
    bool m_FailNextInit;

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HContext m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;
};

dmResource::Result ReloadCollectionTest::ResReloadTargetCreate(const dmResource::ResourceCreateParams* params)
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

dmResource::Result ReloadCollectionTest::ResReloadTargetDestroy(const dmResource::ResourceDestroyParams* params)
{
    dmDDF::FreeMessage((void*) ResourceDescriptorGetResource(params->m_Resource));
    return dmResource::RESULT_OK;
}

dmResource::Result ReloadCollectionTest::ResReloadTargetRecreate(const dmResource::ResourceRecreateParams* params)
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

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetNewWorld(const dmGameObject::ComponentNewWorldParams& params)
{
    ReloadCollectionTest* test = (ReloadCollectionTest*)params.m_Context;
    ReloadTargetWorld* world = new ReloadTargetWorld();
    world->m_Collection = params.m_Collection;
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
    if (rt_world->m_Collection != params.m_Collection)
    {
        self->m_Stats.m_HandleMismatchCount++;
    }
    dmGameObject::Collection* collection = dmGameObject::GetCollectionFromHandle(params.m_Collection);
    if (!collection || !collection->m_CollectionResource)
    {
        self->m_Stats.m_MissingCollectionResourceCount++;
    }
    if (self->m_CreatesUntilFailure == 0)
    {
        self->m_CreatesUntilFailure = -1;
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
    if (self->m_CreatesUntilFailure > 0)
    {
        self->m_CreatesUntilFailure--;
    }
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
    comp->m_Collection = params.m_Collection;
    comp->m_GameObject = params.m_Instance;
    return dmGameObject::CREATE_RESULT_OK;
}

dmGameObject::CreateResult ReloadCollectionTest::CompReloadTargetDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    ReloadCollectionTest* self = (ReloadCollectionTest*) params.m_Context;
    ReloadTargetWorld* rt_world = (ReloadTargetWorld*)params.m_World;
    self->m_Stats.m_DestroyCount++;
    ReloadTargetComponent* comp = (ReloadTargetComponent*)(*params.m_UserData);
    if (rt_world->m_Collection != params.m_Collection ||
        comp->m_Collection != params.m_Collection ||
        comp->m_GameObject != params.m_Instance ||
        !dmGameObject::IsValid(params.m_Collection, params.m_Instance))
    {
        self->m_Stats.m_HandleMismatchCount++;
    }
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
    if (self->m_FailNextInit)
    {
        self->m_FailNextInit = false;
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }
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
    ResetWorldCounters(&m_Stats);
    dmGameObject::HCollectionResource collection_resource = 0;
    dmResource::Get(m_Factory, "/test.collectionc", (void**)&collection_resource);
    m_Collection = dmGameObject::GetCollectionFromResource(collection_resource);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, m_Collection);
    ASSERT_EQ(m_Collection, dmGameObject::GetCollectionFromResource(collection_resource));
    ASSERT_EQ(0, m_Stats.m_MissingCollectionResourceCount);

    ResetWorldCounters(&m_Stats);
    ASSERT_EQ(0, m_Stats.m_CreateCount);
    ASSERT_EQ(0, m_Stats.m_DestroyCount);
    ASSERT_EQ(0, m_Stats.m_InitCount);
    ASSERT_EQ(0, m_Stats.m_FinalCount);
    ASSERT_EQ(0, m_Stats.m_AddToUpdateCount);
    ASSERT_EQ(0, m_Stats.m_ReloadCount);
    ASSERT_EQ(0, m_Stats.m_HandleMismatchCount);
    ASSERT_EQ(0, m_Stats.m_MissingCollectionResourceCount);

    dmGameObject::HCollection old_collection = m_Collection;
    dmGameObject::HGameObject old_game_object = dmGameObject::GetGameObjectFromIdentifier(old_collection, dmHashString64("/go1"));
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, old_game_object);
    dmResource::Result rr = dmResource::ReloadResource(m_Factory, "/test.collectionc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);
    m_Collection = dmGameObject::GetCollectionFromResource(collection_resource);
    ASSERT_NE(old_collection, m_Collection);
    ASSERT_EQ((dmGameObject::HContext)0, dmGameObject::GetGameObjectContext(old_collection));
    ASSERT_FALSE(dmGameObject::IsValid(old_collection, old_game_object));
    ASSERT_FALSE(dmGameObject::IsValid(m_Collection, old_game_object));
    ASSERT_EQ(0, m_Stats.m_MissingCollectionResourceCount);

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
    ASSERT_EQ(0, m_Stats.m_HandleMismatchCount);

    dmGameObject::Init(m_Collection);

    ASSERT_EQ(2, m_Stats.m_InitCount);
    ASSERT_EQ(0, m_Stats.m_FinalCount);         // No final, since they weren't recreated
    ASSERT_EQ(2, m_Stats.m_AddToUpdateCount);
    ASSERT_EQ(0, m_Stats.m_HandleMismatchCount);

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

    old_collection = m_Collection;
    old_game_object = dmGameObject::GetGameObjectFromIdentifier(old_collection, dmHashString64("/go1"));
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, old_game_object);
    rr = dmResource::ReloadResource(m_Factory, "/test.collectionc", 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);
    m_Collection = dmGameObject::GetCollectionFromResource(collection_resource);
    ASSERT_NE(old_collection, m_Collection);
    ASSERT_EQ((dmGameObject::HContext)0, dmGameObject::GetGameObjectContext(old_collection));
    ASSERT_FALSE(dmGameObject::IsValid(old_collection, old_game_object));
    ASSERT_FALSE(dmGameObject::IsValid(m_Collection, old_game_object));
    ASSERT_EQ(0, m_Stats.m_MissingCollectionResourceCount);

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
    ASSERT_EQ(0, m_Stats.m_HandleMismatchCount);

    dmResource::Release(m_Factory, collection_resource);
    m_Collection = 0;
}

TEST_F(ReloadCollectionTest, TestCollectionReloadCreateFailureRestoresStableHandle)
{
    dmGameObject::HCollectionResource collection_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test.collectionc", (void**)&collection_resource));
    m_Collection = dmGameObject::GetCollectionFromResource(collection_resource);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HGameObject old_game_object = dmGameObject::GetGameObjectFromIdentifier(m_Collection, dmHashString64("/go1"));
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, old_game_object);
    ASSERT_TRUE(dmGameObject::IsValid(m_Collection, old_game_object));

    ResetWorldCounters(&m_Stats);
    m_CreatesUntilFailure = 1;
    dmResource::Result result = dmResource::ReloadResource(m_Factory, "/test.collectionc", 0);
    ASSERT_NE(dmResource::RESULT_OK, result);

    ASSERT_EQ(m_Collection, dmGameObject::GetCollectionFromResource(collection_resource));
    ASSERT_TRUE(dmGameObject::IsValid(m_Collection, old_game_object));
    ASSERT_EQ(old_game_object, dmGameObject::GetGameObjectFromIdentifier(m_Collection, dmHashString64("/go1")));
    ASSERT_EQ(0, m_Stats.m_HandleMismatchCount);
    ASSERT_EQ(0, m_Stats.m_MissingCollectionResourceCount);
    ASSERT_EQ(1, m_Stats.m_DestroyCount);
    ASSERT_EQ(2, m_Stats.m_InitCount);
    ASSERT_EQ(2, m_Stats.m_AddToUpdateCount);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmResource::Release(m_Factory, collection_resource);
    m_Collection = 0;
}

TEST_F(ReloadCollectionTest, TestCollectionReloadInitFailureRestoresStableHandle)
{
    dmGameObject::HCollectionResource collection_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test.collectionc", (void**)&collection_resource));
    m_Collection = dmGameObject::GetCollectionFromResource(collection_resource);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HGameObject old_game_object = dmGameObject::GetGameObjectFromIdentifier(m_Collection, dmHashString64("/go1"));
    ASSERT_NE(dmGameObject::INVALID_GAME_OBJECT, old_game_object);
    ASSERT_TRUE(dmGameObject::IsValid(m_Collection, old_game_object));

    ResetWorldCounters(&m_Stats);
    m_FailNextInit = true;
    dmResource::Result result = dmResource::ReloadResource(m_Factory, "/test.collectionc", 0);
    ASSERT_NE(dmResource::RESULT_OK, result);

    ASSERT_EQ(m_Collection, dmGameObject::GetCollectionFromResource(collection_resource));
    ASSERT_TRUE(dmGameObject::IsValid(m_Collection, old_game_object));
    ASSERT_EQ(old_game_object, dmGameObject::GetGameObjectFromIdentifier(m_Collection, dmHashString64("/go1")));
    ASSERT_EQ(0, m_Stats.m_HandleMismatchCount);
    ASSERT_EQ(0, m_Stats.m_MissingCollectionResourceCount);
    ASSERT_EQ(2, m_Stats.m_DestroyCount);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmResource::Release(m_Factory, collection_resource);
    m_Collection = 0;
}
