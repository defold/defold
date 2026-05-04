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

#include <algorithm>
#include <map>
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/log.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "../script.h"
#include "gameobject/test/spawn_delete/test_gameobject_spawn_delete_ddf.h"
#include "../proto/gameobject/gameobject_ddf.h"
#include "../proto/gameobject/lua_ddf.h"
#include <dmsdk/resource/resource.h>

using namespace dmVMath;

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

static dmGameObject::HInstance FactorySpawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t* out_id)
{
    uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
    if (index == dmGameObject::INVALID_INSTANCE_POOL_INDEX)
        return 0x0;

    dmhash_t id = dmGameObject::CreateInstanceId();
    dmGameObject::HInstance instance = Spawn(factory, collection, prototype_name, id, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    if (instance)
    {
        dmGameObject::AssignInstanceIndex(index, instance);
        *out_id = id;
    }
    else
    {
        dmGameObject::ReleaseInstanceIndex(index, collection);
    }

    return instance;
}

static void PostSetParent(dmGameObject::HCollection collection, dmGameObject::HInstance child, dmGameObject::HInstance parent)
{
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(collection);
    receiver.m_Path = dmGameObject::GetIdentifier(child);
    receiver.m_Fragment = 0;

    dmGameObjectDDF::SetParent ddf;
    ddf.m_ParentId = parent ? dmGameObject::GetIdentifier(parent) : 0;
    ddf.m_KeepWorldTransform = 0;

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, dmGameObjectDDF::SetParent::m_DDFDescriptor->m_NameHash,
        (uintptr_t) child, (uintptr_t) dmGameObjectDDF::SetParent::m_DDFDescriptor, &ddf, sizeof(dmGameObjectDDF::SetParent), 0));
}

static int Lua_Spawn(lua_State* L) {
    const char* prototype = luaL_checkstring(L, 1);
    dmGameObject::HInstance instance = dmGameObject::GetInstanceFromLua(L);
    dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
    uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
    dmhash_t id = dmGameObject::CreateInstanceId();
    dmResource::HFactory factory = dmGameObject::GetFactory(collection);
    dmGameObject::HInstance spawned = Spawn(factory, collection, prototype, id, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    if (spawned == 0x0) {
        luaL_error(L, "failed to spawn");
        return 1;
    }
    dmScript::PushHash(L, id);
    return 1;
}

class SpawnDeleteTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Factory = dmResource::NewFactory(&params, "build/src/gameobject/test/spawn_delete");
        dmScript::ContextParams script_context_params = {};
        script_context_params.m_Factory = m_Factory;
        m_ScriptContext = dmScript::NewContext(script_context_params);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_ModuleContext.m_ScriptContexts.SetCapacity(1);
        m_ModuleContext.m_ScriptContexts.Push(m_ScriptContext);

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

        dmMessage::Result result = dmMessage::NewSocket("@system", &m_Socket);
        assert(result == dmMessage::RESULT_OK);

        lua_State* L = dmScript::GetLuaState(m_ScriptContext);
        lua_pushcfunction(L, Lua_Spawn);
        lua_setglobal(L, "spawn");

        // Register dummy physical resource type
        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        HResourceType resource_type;
        dmGameObject::Result go_result;

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "a", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType a_type;
        a_type.m_Name = "a";
        a_type.m_ResourceType = resource_type;
        a_type.m_Context = this;
        a_type.m_CreateFunction = AComponentCreate;
        a_type.m_InitFunction = AComponentInit;
        a_type.m_AddToUpdateFunction = AComponentAddToUpdate;
        a_type.m_FinalFunction = AComponentFinal;
        a_type.m_DestroyFunction = AComponentDestroy;
        a_type.m_UpdateFunction = AComponentsUpdate;
        a_type.m_InstanceHasUserData = true;
        go_result = dmGameObject::RegisterComponentType(m_Register, a_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 300);

        dmGameObject::SortComponentTypes(m_Register);

        ASSERT_EQ(dmGameObject::RESULT_OK, go_result);

        m_Collection = NewCollection("collection", m_Factory, m_Register, 10u, 0x0);
    }

    void TearDown() override
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmMessage::DeleteSocket(m_Socket);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

    static dmResource::FResourceCreate          ACreate;
    static dmResource::FResourceDestroy         ADestroy;
    static dmGameObject::ComponentCreate        AComponentCreate;
    static dmGameObject::ComponentInit          AComponentInit;
    static dmGameObject::ComponentFinal         AComponentFinal;
    static dmGameObject::ComponentDestroy       AComponentDestroy;
    static dmGameObject::ComponentAddToUpdate   AComponentAddToUpdate;
    static dmGameObject::ComponentsUpdate       AComponentsUpdate;

public:

    dmGameObject::HCollection m_Collection;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmResource::HFactory m_Factory;
    dmMessage::HSocket m_Socket;
    dmScript::HContext m_ScriptContext;
    dmGameObject::ModuleContext m_ModuleContext;
    dmHashTable64<void*> m_Contexts;

    std::map<uint64_t, uint32_t> m_ComponentInitCountMap;
    std::map<uint64_t, uint32_t> m_ComponentFinalCountMap;
    std::map<uint64_t, uint32_t> m_ComponentUpdateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentAddToUpdateCountMap;

    uint32_t Count(std::map<uint64_t, uint32_t>& map) {
        return map[TestGameObjectDDF::AResource::m_DDFHash];
    }

    void NotNull(dmGameObject::HInstance instance) {
        ASSERT_NE((void*)0, (void*)instance);
    }

    dmGameObject::HInstance New(const char* prototype) {
        dmGameObject::HInstance go = dmGameObject::New(m_Collection, prototype);
        NotNull(go);
        return go;
    }

    void Init() {
        ASSERT_TRUE(dmGameObject::Init(m_Collection));
    }

    void Update() {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }

    void PostUpdate() {
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    }

    void Delete(dmGameObject::HInstance instance) {
        dmGameObject::Delete(m_Collection, instance, false);
    }
};

template <typename T>
dmResource::Result GenericDDFCreate(const dmResource::ResourceCreateParams* params)
{
    T* obj;
    dmDDF::Result e = dmDDF::LoadMessage<T>(params->m_Buffer, params->m_BufferSize, &obj);
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

template <typename T>
dmResource::Result GenericDDFDestory(const dmResource::ResourceDestroyParams* params)
{
    dmDDF::FreeMessage((void*)ResourceDescriptorGetResource(params->m_Resource));
    return dmResource::RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentCreate(const dmGameObject::ComponentCreateParams& params)
{
    *params.m_UserData = (uintptr_t)1;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentInit(const dmGameObject::ComponentInitParams& params)
{
    SpawnDeleteTest* game_object_test = (SpawnDeleteTest*) params.m_Context;
    game_object_test->m_ComponentInitCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentFinal(const dmGameObject::ComponentFinalParams& params)
{
    SpawnDeleteTest* game_object_test = (SpawnDeleteTest*) params.m_Context;
    game_object_test->m_ComponentFinalCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
{
    SpawnDeleteTest* game_object_test = (SpawnDeleteTest*) params.m_Context;
    game_object_test->m_ComponentAddToUpdateCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::UpdateResult GenericComponentsUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
{
    SpawnDeleteTest* game_object_test = (SpawnDeleteTest*) params.m_Context;
    if (game_object_test->m_ComponentAddToUpdateCountMap[T::m_DDFHash] > game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]) {
        game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
    }
    return dmGameObject::UPDATE_RESULT_OK;
}


template <typename T>
static dmGameObject::CreateResult GenericComponentDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate SpawnDeleteTest::ACreate                      = GenericDDFCreate<TestGameObjectDDF::AResource>;
dmResource::FResourceDestroy SpawnDeleteTest::ADestroy                    = GenericDDFDestory<TestGameObjectDDF::AResource>;
dmGameObject::ComponentCreate SpawnDeleteTest::AComponentCreate           = GenericComponentCreate<TestGameObjectDDF::AResource>;
dmGameObject::ComponentInit SpawnDeleteTest::AComponentInit               = GenericComponentInit<TestGameObjectDDF::AResource>;
dmGameObject::ComponentFinal SpawnDeleteTest::AComponentFinal             = GenericComponentFinal<TestGameObjectDDF::AResource>;
dmGameObject::ComponentDestroy SpawnDeleteTest::AComponentDestroy         = GenericComponentDestroy<TestGameObjectDDF::AResource>;
dmGameObject::ComponentAddToUpdate SpawnDeleteTest::AComponentAddToUpdate = GenericComponentAddToUpdate<TestGameObjectDDF::AResource>;
dmGameObject::ComponentsUpdate SpawnDeleteTest::AComponentsUpdate         = GenericComponentsUpdate<TestGameObjectDDF::AResource>;

#define ASSERT_INIT(count)\
    ASSERT_EQ(count, Count(m_ComponentInitCountMap));
#define ASSERT_ADD_TO_UPDATE(count)\
    ASSERT_EQ(count, Count(m_ComponentAddToUpdateCountMap));
#define ASSERT_UPDATE(count)\
    ASSERT_EQ(count, Count(m_ComponentUpdateCountMap));
#define ASSERT_FINAL(count)\
    ASSERT_EQ(count, Count(m_ComponentFinalCountMap));

TEST_F(SpawnDeleteTest, CollectionInit_ScriptInit_Spawn)
{
    dmGameObject::HInstance go = New("/init_spawn.goc");

    Init();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(1u);

    Delete(go);
}

TEST_F(SpawnDeleteTest, CollectionUpdate_ScriptInit_Spawn)
{
    dmGameObject::HInstance go = New("/update_init_spawn.goc");

    Init();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(1u);

    Delete(go);
}

TEST_F(SpawnDeleteTest, CollectionUpdate_ScriptUpdate_Spawn)
{
    dmGameObject::HInstance go = New("/update_spawn.goc");

    Init();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(1u);

    Delete(go);
}

TEST_F(SpawnDeleteTest, CollectionUpdate_ScriptFinal_Spawn)
{
    dmGameObject::HInstance go1 = New("/final_spawn.goc");
    dmGameObject::SetIdentifier(m_Collection, go1, "/target");
    dmGameObject::HInstance go2 = New("/update_delete_target.goc");

    Init();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    PostUpdate();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(1u);

    Delete(go2);
}

TEST_F(SpawnDeleteTest, CollectionDelete_ScriptFinal_Spawn)
{
    // Temp swap collections to delete at end
    dmGameObject::HCollection old_collection = m_Collection;
    m_Collection = dmGameObject::NewCollection("collection2", m_Factory, m_Register, 10u, 0x0);

    New("/final_spawn.goc");

    Init();

    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    m_Collection = old_collection;
}

TEST_F(SpawnDeleteTest, CollectionInit_ScriptInit_Delete)
{
    dmGameObject::HInstance go1 = New("/a.goc");
    dmGameObject::SetIdentifier(m_Collection, go1, "/target");
    dmGameObject::HInstance go2 = New("/init_delete_target.goc");

    Init();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    PostUpdate();

    ASSERT_FINAL(1u);

    Delete(go2);
}

TEST_F(SpawnDeleteTest, CollectionUpdate_ScriptInit_Delete)
{
    dmGameObject::HInstance go1 = New("/a.goc");
    dmGameObject::SetIdentifier(m_Collection, go1, "/target");
    dmGameObject::HInstance go2 = New("/update_spawn_delete_target.goc");

    Init();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    Update();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(1u);
    ASSERT_FINAL(0u);

    PostUpdate();

    ASSERT_FINAL(1u);

    Delete(go2);
}

TEST_F(SpawnDeleteTest, CollectionDelete_ScriptFinal_Delete)
{
    // Temp swap collections to delete at end
    dmGameObject::HCollection old_collection = m_Collection;
    m_Collection = dmGameObject::NewCollection("collection2", m_Factory, m_Register, 10u, 0x0);

    New("/final_delete.goc");
    dmGameObject::HInstance go2 = New("/a.goc");
    dmGameObject::SetIdentifier(m_Collection, go2, "/target");

    Init();

    dmGameObject::DeleteCollection(m_Collection);

    dmGameObject::PostUpdate(m_Register);

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(1u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(1u);

    m_Collection = old_collection;
}

TEST_F(SpawnDeleteTest, CollectionInit_ScriptInit_SpawnDelete)
{
    dmGameObject::HInstance go = New("/init_spawndelete.goc");

    Init();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    Update();
    PostUpdate();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(1u);

    Delete(go);
}

TEST_F(SpawnDeleteTest, CollectionUpdate_ScriptUpdate_SpawnDelete)
{
    dmGameObject::HInstance go = New("/update_spawndelete.goc");

    Init();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    Update();
    PostUpdate();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(1u);

    // Extra update to ensure consistency
    // The instance will be flagged to add-to-update, but should now have been removed from the linked list
    Update();

    Delete(go);
}

TEST_F(SpawnDeleteTest, CollectionUpdate_ScriptFinal_SpawnDelete)
{
    dmGameObject::HInstance go = New("/final_spawndelete.goc");

    Init();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    Delete(go);

    Update();
    PostUpdate();

    ASSERT_INIT(1u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(1u);

    // Extra update to ensure consistency
    // The instance will be flagged to add-to-update, but should now have been removed from the linked list
    Update();
}

TEST_F(SpawnDeleteTest, CollectionDelete_ScriptFinal_SpawnDelete)
{
    // Temp swap collections to delete at end
    dmGameObject::HCollection old_collection = m_Collection;
    m_Collection = dmGameObject::NewCollection("collection2", m_Factory, m_Register, 10u, 0x0);

    New("/final_spawndelete.goc");

    Init();

    dmGameObject::DeleteCollection(m_Collection);

    dmGameObject::PostUpdate(m_Register);

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    m_Collection = old_collection;
}

TEST_F(SpawnDeleteTest, CollectionUpdate_SpawnDeleteMulti)
{
    dmGameObject::HInstance go = New("/spawndelete_multi.goc");

    Init();

    ASSERT_INIT(0u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(0u);

    Update();
    PostUpdate();

    ASSERT_INIT(2u);
    ASSERT_ADD_TO_UPDATE(0u);
    ASSERT_UPDATE(0u);
    ASSERT_FINAL(2u);

    // Extra update to ensure consistency
    // The instances will be flagged to add-to-update, but should now have been removed from the linked list
    Update();
    Delete(go);
}


// DEF-3218: Removing the objects out of order caused an assert
TEST_F(SpawnDeleteTest, CollectionUpdate_SpawnDeleteMulti2)
{
    Init();

    dmGameObject::HInstance go2 = Spawn(m_Factory, m_Collection, "/a.goc", 2, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));

    Update();

    ASSERT_EQ(0u, dmGameObject::GetAddToUpdateCount(m_Collection));
    ASSERT_EQ(0u, dmGameObject::GetRemoveFromUpdateCount(m_Collection));

    dmGameObject::HInstance go9 = Spawn(m_Factory, m_Collection, "/a.goc", 9, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    dmGameObject::HInstance go3 = Spawn(m_Factory, m_Collection, "/a.goc", 3, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    dmGameObject::HInstance go4 = Spawn(m_Factory, m_Collection, "/a.goc", 4, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    dmGameObject::HInstance go6 = Spawn(m_Factory, m_Collection, "/a.goc", 6, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));

    Delete(go4);
    Delete(go2);
    Delete(go6);
    Delete(go3);
    Delete(go9);

    ASSERT_EQ(4u, dmGameObject::GetAddToUpdateCount(m_Collection));
    ASSERT_EQ(5u, dmGameObject::GetRemoveFromUpdateCount(m_Collection));

    PostUpdate();
    // The linked list should now have been corrupted
    // and the next spawned object won't get added to the AddToUpdate list
    dmGameObject::HInstance go10 = Spawn(m_Factory, m_Collection, "/a.goc", 10, 0, dmVMath::Point3(0.0f, 0.0f, 0.0f), dmVMath::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));

    ASSERT_EQ(1u, dmGameObject::GetAddToUpdateCount(m_Collection));
    ASSERT_EQ(0u, dmGameObject::GetRemoveFromUpdateCount(m_Collection));

    Delete(go10);
    Update();
    PostUpdate();
}

// https://github.com/defold/defold/issues/12247
TEST_F(SpawnDeleteTest, SetParentDeleteStress)
{
    dmGameObject::DeleteCollection(m_Collection);
    dmGameObject::PostUpdate(m_Register);
    m_Collection = NewCollection("collection", m_Factory, m_Register, 1024u, 0x0);

    Init();

    const uint32_t iteration_count = 1798;
    dmArray<dmhash_t> ids;
    ids.SetCapacity(iteration_count);
    uint32_t state = 0x12247;

    for (uint32_t i = 0; i < iteration_count; ++i)
    {
        Update();

        state = state * 1664525u + 1013904223u;
        uint32_t op = ids.Empty() ? 0 : state % 3;

        if (op == 0)
        {
            dmhash_t id = 0;
            dmGameObject::HInstance instance = FactorySpawn(m_Factory, m_Collection, "/a.goc", &id);
            if (instance)
            {
                ids.Push(id);
            }
        }
        else if (op == 1)
        {
            state = state * 1664525u + 1013904223u;
            dmhash_t id = ids[state % ids.Size()];
            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(m_Collection, id);
            if (instance)
            {
                Delete(instance);
            }
        }
        else
        {
            state = state * 1664525u + 1013904223u;
            dmhash_t child_id = ids[state % ids.Size()];
            state = state * 1664525u + 1013904223u;
            dmhash_t parent_id = ids[state % ids.Size()];
            dmGameObject::HInstance child = dmGameObject::GetInstanceFromIdentifier(m_Collection, child_id);
            dmGameObject::HInstance parent = dmGameObject::GetInstanceFromIdentifier(m_Collection, parent_id);
            // Steer the deterministic random sequence into the same failing deletion path as issue #12247.
            if (i == 1797 && parent)
            {
                Delete(parent);
            }
            else if (child && parent && child != parent)
            {
                PostSetParent(m_Collection, child, parent);
            }
        }

        PostUpdate();
    }
}


#undef ASSERT_INIT
#undef ASSERT_ADD_TO_UPDATE
#undef ASSERT_UPDATE
#undef ASSERT_FINAL
