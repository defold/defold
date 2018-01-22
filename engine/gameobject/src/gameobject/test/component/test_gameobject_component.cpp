#include <gtest/gtest.h>

#include <map>

#include <dlib/dstrings.h>
#include <dlib/hash.h>

#include <resource/resource.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

#include "gameobject/test/component/test_gameobject_component_ddf.h"

class ComponentTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateCount = 0;
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/component");
        m_ScriptContext = dmScript::NewContext(0, 0, true);
        dmScript::Initialize(m_ScriptContext);
        dmGameObject::Initialize(m_Register, m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024, 0);

        // Register dummy physical resource type
        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "a", this, 0, ACreate, 0, ADestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "b", this, 0, BCreate, 0, BDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "c", this, 0, CCreate, 0, CDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::ResourceType resource_type;
        dmGameObject::Result result;

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
        result = dmGameObject::RegisterComponentType(m_Register, a_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 2);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // B has *not* component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "b", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType b_type;
        b_type.m_Name = "b";
        b_type.m_ResourceType = resource_type;
        b_type.m_Context = this;
        b_type.m_CreateFunction = BComponentCreate;
        b_type.m_InitFunction = BComponentInit;
        b_type.m_AddToUpdateFunction = BComponentAddToUpdate;
        b_type.m_FinalFunction = BComponentFinal;
        b_type.m_DestroyFunction = BComponentDestroy;
        b_type.m_UpdateFunction = BComponentsUpdate;
        result = dmGameObject::RegisterComponentType(m_Register, b_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 1);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // C has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "c", &resource_type);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        dmGameObject::ComponentType c_type;
        c_type.m_Name = "c";
        c_type.m_ResourceType = resource_type;
        c_type.m_Context = this;
        c_type.m_CreateFunction = CComponentCreate;
        c_type.m_InitFunction = CComponentInit;
        c_type.m_AddToUpdateFunction = CComponentAddToUpdate;
        c_type.m_FinalFunction = CComponentFinal;
        c_type.m_DestroyFunction = CComponentDestroy;
        c_type.m_UpdateFunction = CComponentsUpdate;
        c_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(m_Register, c_type);
        dmGameObject::SetUpdateOrderPrio(m_Register, resource_type, 0);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        dmGameObject::SortComponentTypes(m_Register);

        m_MaxComponentCreateCountMap[TestGameObjectDDF::AResource::m_DDFHash] = 1000000;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmGameObject::DeleteRegister(m_Register);
        dmResource::DeleteFactory(m_Factory);
    }

    static dmResource::FResourceCreate          ACreate;
    static dmResource::FResourceDestroy         ADestroy;
    static dmGameObject::ComponentCreate        AComponentCreate;
    static dmGameObject::ComponentInit          AComponentInit;
    static dmGameObject::ComponentFinal         AComponentFinal;
    static dmGameObject::ComponentDestroy       AComponentDestroy;
    static dmGameObject::ComponentAddToUpdate   AComponentAddToUpdate;
    static dmGameObject::ComponentsUpdate       AComponentsUpdate;

    static dmResource::FResourceCreate          BCreate;
    static dmResource::FResourceDestroy         BDestroy;
    static dmGameObject::ComponentCreate        BComponentCreate;
    static dmGameObject::ComponentInit          BComponentInit;
    static dmGameObject::ComponentFinal         BComponentFinal;
    static dmGameObject::ComponentDestroy       BComponentDestroy;
    static dmGameObject::ComponentAddToUpdate   BComponentAddToUpdate;
    static dmGameObject::ComponentsUpdate       BComponentsUpdate;

    static dmResource::FResourceCreate          CCreate;
    static dmResource::FResourceDestroy         CDestroy;
    static dmGameObject::ComponentCreate        CComponentCreate;
    static dmGameObject::ComponentInit          CComponentInit;
    static dmGameObject::ComponentFinal         CComponentFinal;
    static dmGameObject::ComponentDestroy       CComponentDestroy;
    static dmGameObject::ComponentAddToUpdate   CComponentAddToUpdate;
    static dmGameObject::ComponentsUpdate       CComponentsUpdate;

public:
    uint32_t                     m_UpdateCount;
    std::map<uint64_t, uint32_t> m_CreateCountMap;
    std::map<uint64_t, uint32_t> m_DestroyCountMap;

    std::map<uint64_t, uint32_t> m_ComponentCreateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentInitCountMap;
    std::map<uint64_t, uint32_t> m_ComponentFinalCountMap;
    std::map<uint64_t, uint32_t> m_ComponentDestroyCountMap;
    std::map<uint64_t, uint32_t> m_ComponentUpdateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentAddToUpdateCountMap;
    std::map<uint64_t, uint32_t> m_MaxComponentCreateCountMap;

    std::map<uint64_t, uint32_t> m_ComponentUpdateOrderMap;

    std::map<uint64_t, int>      m_ComponentUserDataAcc;

    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
};

template <typename T>
dmResource::Result GenericDDFCreate(const dmResource::ResourceCreateParams& params)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    game_object_test->m_CreateCountMap[T::m_DDFHash]++;

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
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    game_object_test->m_DestroyCountMap[T::m_DDFHash]++;

    dmDDF::FreeMessage((void*) params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

template <typename T, int add_to_user_data>
static dmGameObject::CreateResult GenericComponentCreate(const dmGameObject::ComponentCreateParams& params)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;

    if (params.m_UserData && add_to_user_data != -1)
    {
        *params.m_UserData += add_to_user_data;
    }

    if (game_object_test->m_MaxComponentCreateCountMap.find(T::m_DDFHash) != game_object_test->m_MaxComponentCreateCountMap.end())
    {
        if (game_object_test->m_ComponentCreateCountMap[T::m_DDFHash] >= game_object_test->m_MaxComponentCreateCountMap[T::m_DDFHash])
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    game_object_test->m_ComponentCreateCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentInit(const dmGameObject::ComponentInitParams& params)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    game_object_test->m_ComponentInitCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentFinal(const dmGameObject::ComponentFinalParams& params)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    game_object_test->m_ComponentFinalCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    game_object_test->m_ComponentAddToUpdateCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::UpdateResult GenericComponentsUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
    game_object_test->m_ComponentUpdateOrderMap[T::m_DDFHash] = game_object_test->m_UpdateCount++;
    return dmGameObject::UPDATE_RESULT_OK;
}


template <typename T>
static dmGameObject::CreateResult GenericComponentDestroy(const dmGameObject::ComponentDestroyParams& params)
{
    ComponentTest* game_object_test = (ComponentTest*) params.m_Context;
    if (params.m_UserData)
    {
        game_object_test->m_ComponentUserDataAcc[T::m_DDFHash] += *params.m_UserData;
    }

    game_object_test->m_ComponentDestroyCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate ComponentTest::ACreate                      = GenericDDFCreate<TestGameObjectDDF::AResource>;
dmResource::FResourceDestroy ComponentTest::ADestroy                    = GenericDDFDestory<TestGameObjectDDF::AResource>;
dmGameObject::ComponentCreate ComponentTest::AComponentCreate           = GenericComponentCreate<TestGameObjectDDF::AResource, 1>;
dmGameObject::ComponentInit ComponentTest::AComponentInit               = GenericComponentInit<TestGameObjectDDF::AResource>;
dmGameObject::ComponentFinal ComponentTest::AComponentFinal             = GenericComponentFinal<TestGameObjectDDF::AResource>;
dmGameObject::ComponentDestroy ComponentTest::AComponentDestroy         = GenericComponentDestroy<TestGameObjectDDF::AResource>;
dmGameObject::ComponentAddToUpdate ComponentTest::AComponentAddToUpdate = GenericComponentAddToUpdate<TestGameObjectDDF::AResource>;
dmGameObject::ComponentsUpdate ComponentTest::AComponentsUpdate         = GenericComponentsUpdate<TestGameObjectDDF::AResource>;

dmResource::FResourceCreate ComponentTest::BCreate                      = GenericDDFCreate<TestGameObjectDDF::BResource>;
dmResource::FResourceDestroy ComponentTest::BDestroy                    = GenericDDFDestory<TestGameObjectDDF::BResource>;
dmGameObject::ComponentCreate ComponentTest::BComponentCreate           = GenericComponentCreate<TestGameObjectDDF::BResource, -1>;
dmGameObject::ComponentInit ComponentTest::BComponentInit               = GenericComponentInit<TestGameObjectDDF::BResource>;
dmGameObject::ComponentFinal ComponentTest::BComponentFinal             = GenericComponentFinal<TestGameObjectDDF::BResource>;
dmGameObject::ComponentDestroy ComponentTest::BComponentDestroy         = GenericComponentDestroy<TestGameObjectDDF::BResource>;
dmGameObject::ComponentAddToUpdate ComponentTest::BComponentAddToUpdate = GenericComponentAddToUpdate<TestGameObjectDDF::BResource>;
dmGameObject::ComponentsUpdate ComponentTest::BComponentsUpdate         = GenericComponentsUpdate<TestGameObjectDDF::BResource>;

dmResource::FResourceCreate ComponentTest::CCreate                      = GenericDDFCreate<TestGameObjectDDF::CResource>;
dmResource::FResourceDestroy ComponentTest::CDestroy                    = GenericDDFDestory<TestGameObjectDDF::CResource>;
dmGameObject::ComponentCreate ComponentTest::CComponentCreate           = GenericComponentCreate<TestGameObjectDDF::CResource, 10>;
dmGameObject::ComponentInit ComponentTest::CComponentInit               = GenericComponentInit<TestGameObjectDDF::CResource>;
dmGameObject::ComponentFinal ComponentTest::CComponentFinal             = GenericComponentFinal<TestGameObjectDDF::CResource>;
dmGameObject::ComponentDestroy ComponentTest::CComponentDestroy         = GenericComponentDestroy<TestGameObjectDDF::CResource>;
dmGameObject::ComponentAddToUpdate ComponentTest::CComponentAddToUpdate = GenericComponentAddToUpdate<TestGameObjectDDF::CResource>;
dmGameObject::ComponentsUpdate ComponentTest::CComponentsUpdate         = GenericComponentsUpdate<TestGameObjectDDF::CResource>;

TEST_F(ComponentTest, TestUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_NE((void*) 0, (void*) go);
    dmGameObject::Init(m_Collection);
    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);
    ret = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(ret);

    dmGameObject::Delete(m_Collection, go, false);
    ret = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentInitCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentAddToUpdateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentUpdateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentFinalCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
}

TEST_F(ComponentTest, TestPostDeleteUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_NE((void*) 0, (void*) go);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "go1"));

    dmhash_t message_id = dmHashString64("test");
    dmMessage::URL receiver;
    receiver.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = dmHashString64("script");
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(0x0, &receiver, message_id, 0, 0, 0x0, 0, 0));

    dmGameObject::Delete(m_Collection, go, false);

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);

    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
}

TEST_F(ComponentTest, TestNonexistingComponent)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go2.goc");
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);

    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
}

TEST_F(ComponentTest, TestPartialNonexistingComponent1)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go3.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    // First one exists
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    // Even though the first a-component exits the prototype creation should fail before creating components
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
}

TEST_F(ComponentTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxComponentCreateCountMap[TestGameObjectDDF::AResource::m_DDFHash] = 1;
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go4.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);

    // One component should get created
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestGameObjectDDF::AResource::m_DDFHash]);
}

TEST_F(ComponentTest, TestComponentUserdata)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go5.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Delete(m_Collection, go, false);
    bool ret = dmGameObject::PostUpdate(m_Collection);
    ASSERT_TRUE(ret);
    // Two a:s
    ASSERT_EQ(2, m_ComponentUserDataAcc[TestGameObjectDDF::AResource::m_DDFHash]);
    // Zero c:s
    ASSERT_EQ(0, m_ComponentUserDataAcc[TestGameObjectDDF::BResource::m_DDFHash]);
    // Three c:s
    ASSERT_EQ(30, m_ComponentUserDataAcc[TestGameObjectDDF::CResource::m_DDFHash]);
}

TEST_F(ComponentTest, TestUpdateOrder)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 2, m_ComponentUpdateOrderMap[TestGameObjectDDF::AResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentUpdateOrderMap[TestGameObjectDDF::BResource::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentUpdateOrderMap[TestGameObjectDDF::CResource::m_DDFHash]);
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ComponentTest, TestDuplicatedIds)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go6.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(ComponentTest, TestIndexId)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/go1.goc");

    uint16_t component_index;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(go, dmHashString64("script"), &component_index));
    ASSERT_EQ(0u, component_index);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(go, dmHashString64("a"), &component_index));
    ASSERT_EQ(1u, component_index);
    dmhash_t component_id;
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentId(go, 0, &component_id));
    ASSERT_EQ(dmHashString64("script"), component_id);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentId(go, 1, &component_id));
    ASSERT_EQ(dmHashString64("a"), component_id);
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, dmGameObject::GetComponentIndex(go, dmHashString64("does_not_exist"), &component_index));
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, dmGameObject::GetComponentId(go, 2, &component_id));
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(ComponentTest, TestManyComponents)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/many.goc");

    char name[64];
    uint16_t component_index;
    dmhash_t component_id;
    const uint32_t num_components = 300;
    for( uint32_t i = 0; i < num_components; ++i)
    {
        DM_SNPRINTF(name, sizeof(name), "script%d", i);
        dmhash_t id = dmHashString64(name);
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentIndex(go, id, &component_index));
        ASSERT_EQ(i, component_index);

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponentId(go, component_index, &component_id));
        ASSERT_EQ(id, component_id);
    }
    DM_SNPRINTF(name, sizeof(name), "script%d", num_components);
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, dmGameObject::GetComponentIndex(go, dmHashString64(name), &component_index));
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, dmGameObject::GetComponentId(go, num_components, &component_id));

    dmGameObject::Delete(m_Collection, go, false);
}

static int LuaTestCompType(lua_State* L)
{
    int top = lua_gettop(L);

    dmGameObject::HInstance instance = dmGameObject::GetInstanceFromLua(L);
    uintptr_t user_data = 0;
    dmMessage::URL receiver;
    dmGameObject::GetComponentUserDataFromLua(L, 1, dmGameObject::GetCollection(instance), "a", &user_data, &receiver, 0);
    assert(user_data == 1);

    assert(top == lua_gettop(L));

    return 0;
}

TEST_F(ComponentTest, TestComponentType)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_pushcfunction(L, LuaTestCompType);
    lua_setglobal(L, "test_comp_type");

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/test_comp_type.goc");
    dmGameObject::SetIdentifier(m_Collection, go, "test_instance");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    bool ret = dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_FALSE(ret);

    dmGameObject::Delete(m_Collection, go, false);
}

// Deleting the first go should delete the second in its final callback
// The test is to verify that the final callback is called for a cascading deletion of objects.
TEST_F(ComponentTest, FinalCallsFinal)
{
    dmGameObject::HCollection collection = dmGameObject::NewCollection("test_final_collection", m_Factory, m_Register, 11, 0);

    dmGameObject::HInstance go_a = dmGameObject::New(collection, "/test_final_final.goc");
    dmGameObject::SetIdentifier(collection, go_a, "first");

    char buf[5];
    for (uint32_t i = 0; i < 10; ++i) {
        dmGameObject::HInstance go_b = dmGameObject::New(collection, "/test_final_final.goc");
        DM_SNPRINTF(buf, 5, "id%d", i);
        dmGameObject::SetIdentifier(collection, go_b, buf);
    }

    // 11 objects in total
    ASSERT_EQ(11u, collection->m_InstanceIndices.Size());

    dmGameObject::Init(collection); // Init is required for final
    dmGameObject::Delete(collection, go_a, false);
    dmGameObject::PostUpdate(collection);

    // One lingering due to the cap of passes in dmGameObject::PostUpdate, which is currently set to 10
    ASSERT_EQ(1u, collection->m_InstanceIndices.Size());
    ASSERT_EQ((uint32_t) 10, m_ComponentFinalCountMap[TestGameObjectDDF::AResource::m_DDFHash]);

    // One more pass needed to delete the last object in the chain
    dmGameObject::PostUpdate(collection);

    // All done
    ASSERT_EQ(0u, collection->m_InstanceIndices.Size());
    ASSERT_EQ((uint32_t) 11, m_ComponentFinalCountMap[TestGameObjectDDF::AResource::m_DDFHash]);

    dmGameObject::DeleteCollection(collection);
    dmGameObject::PostUpdate(m_Register);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
