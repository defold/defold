#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/log.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "gameobject/test/spawn_delete/test_gameobject_spawn_delete_ddf.h"
#include "../proto/gameobject/gameobject_ddf.h"
#include "../proto/gameobject/lua_ddf.h"

using namespace Vectormath::Aos;

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

static int Lua_Spawn(lua_State* L) {
    const char* prototype = luaL_checkstring(L, 1);
    dmGameObject::HInstance instance = dmGameObject::GetInstanceFromLua(L);
    dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
    uint32_t index = dmGameObject::AcquireInstanceIndex(collection);
    dmhash_t id = dmGameObject::ConstructInstanceId(index);
    dmResource::HFactory factory = dmGameObject::GetFactory(collection);
    dmGameObject::HInstance spawned = Spawn(factory, collection, prototype, id, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    if (spawned == 0x0) {
        luaL_error(L, "failed to spawn");
        return 1;
    }
    dmScript::PushHash(L, id);
    return 1;
}

class SpawnDeleteTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/spawn_delete");
        m_ScriptContext = dmScript::NewContext(0, m_Factory, true);
        dmScript::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_ModuleContext.m_ScriptContexts.SetCapacity(1);
        m_ModuleContext.m_ScriptContexts.Push(m_ScriptContext);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        dmMessage::Result result = dmMessage::NewSocket("@system", &m_Socket);
        assert(result == dmMessage::RESULT_OK);

        lua_State* L = dmScript::GetLuaState(m_ScriptContext);
        lua_pushcfunction(L, Lua_Spawn);
        lua_setglobal(L, "spawn");

        // Register dummy physical resource type
        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::ResourceType resource_type;
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

        m_Collection = NewCollection("collection", m_Factory, m_Register, 10u);
    }

    virtual void TearDown()
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
dmResource::Result GenericDDFCreate(const dmResource::ResourceCreateParams& params)
{
    T* obj;
    dmDDF::Result e = dmDDF::LoadMessage<T>(params.m_Buffer, params.m_BufferSize, &obj);
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

template <typename T>
dmResource::Result GenericDDFDestory(const dmResource::ResourceDestroyParams& params)
{
    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
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
    m_Collection = dmGameObject::NewCollection("collection2", m_Factory, m_Register, 10u);

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
    m_Collection = dmGameObject::NewCollection("collection2", m_Factory, m_Register, 10u);

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
    m_Collection = dmGameObject::NewCollection("collection2", m_Factory, m_Register, 10u);

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

    dmGameObject::HInstance go2 = Spawn(m_Factory, m_Collection, "/a.goc", 2, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));

    Update();

    ASSERT_EQ(0u, dmGameObject::GetAddToUpdateCount(m_Collection));
    ASSERT_EQ(0u, dmGameObject::GetRemoveFromUpdateCount(m_Collection));

    dmGameObject::HInstance go9 = Spawn(m_Factory, m_Collection, "/a.goc", 9, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    dmGameObject::HInstance go3 = Spawn(m_Factory, m_Collection, "/a.goc", 3, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    dmGameObject::HInstance go4 = Spawn(m_Factory, m_Collection, "/a.goc", 4, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    dmGameObject::HInstance go6 = Spawn(m_Factory, m_Collection, "/a.goc", 6, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));

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
    dmGameObject::HInstance go10 = Spawn(m_Factory, m_Collection, "/a.goc", 10, 0x0, 0, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));

    ASSERT_EQ(1u, dmGameObject::GetAddToUpdateCount(m_Collection));
    ASSERT_EQ(0u, dmGameObject::GetRemoveFromUpdateCount(m_Collection));

    Delete(go10);
    Update();
    PostUpdate();
}


#undef ASSERT_INIT
#undef ASSERT_ADD_TO_UPDATE
#undef ASSERT_UPDATE
#undef ASSERT_FINAL

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
