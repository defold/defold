#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/log.h>
#include "../resource.h"
#include "../gameobject.h"
#include "gamesys/test/test_resource_ddf.h"
#include "../proto/gameobject_ddf.h"

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();
        dmGameObject::RegisterDDFType(TestResource::Spawn::m_DDFDescriptor);

        m_EventTargetCounter = 0;

        update_context.m_DT = 1.0f / 60.0f;
        update_context.m_GlobalData = 0;
        update_context.m_DDFGlobalDataDescriptor = 0;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        factory = dmResource::NewFactory(&params, "build/default/src/gamesys/test");
        collection = dmGameObject::NewCollection(factory, 1024);
        dmGameObject::RegisterResourceTypes(factory);
        script_context = dmGameObject::CreateScriptContext(0x0);
        dmGameObject::RegisterComponentTypes(factory, collection, script_context);

        // Register dummy physical resource type
        dmResource::FactoryResult e;
        e = dmResource::RegisterType(factory, "pc", this, PhysCreate, PhysDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "a", this, ACreate, ADestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "b", this, BCreate, BDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "c", this, CCreate, CDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "et", this, EventTargetCreate, EventTargetDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(factory, "deleteself", this, DeleteSelfCreate, DeleteSelfDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

        uint32_t resource_type;

        e = dmResource::GetTypeFromExtension(factory, "pc", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType pc_type;
        pc_type.m_Name = "pc";
        pc_type.m_ResourceType = resource_type;
        pc_type.m_Context = this;
        pc_type.m_CreateFunction = PhysComponentCreate;
        pc_type.m_InitFunction = PhysComponentInit;
        pc_type.m_DestroyFunction = PhysComponentDestroy;
        pc_type.m_UpdateFunction = PhysComponentsUpdate;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(collection, pc_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(factory, "a", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType a_type;
        a_type.m_Name = "a";
        a_type.m_ResourceType = resource_type;
        a_type.m_Context = this;
        a_type.m_CreateFunction = AComponentCreate;
        a_type.m_InitFunction = AComponentInit;
        a_type.m_DestroyFunction = AComponentDestroy;
        a_type.m_UpdateFunction = AComponentsUpdate;
        a_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(collection, a_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // B has *not* component_user_data
        e = dmResource::GetTypeFromExtension(factory, "b", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType b_type;
        b_type.m_Name = "b";
        b_type.m_ResourceType = resource_type;
        b_type.m_Context = this;
        b_type.m_CreateFunction = BComponentCreate;
        b_type.m_InitFunction = BComponentInit;
        b_type.m_DestroyFunction = BComponentDestroy;
        b_type.m_UpdateFunction = BComponentsUpdate;
        result = dmGameObject::RegisterComponentType(collection, b_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // C has component_user_data
        e = dmResource::GetTypeFromExtension(factory, "c", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType c_type;
        c_type.m_Name = "c";
        c_type.m_ResourceType = resource_type;
        c_type.m_Context = this;
        c_type.m_CreateFunction = CComponentCreate;
        c_type.m_InitFunction = CComponentInit;
        c_type.m_DestroyFunction = CComponentDestroy;
        c_type.m_UpdateFunction = CComponentsUpdate;
        c_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(collection, c_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // EventTargetComponent
        e = dmResource::GetTypeFromExtension(factory, "et", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType et_type;
        et_type.m_Name = "et";
        et_type.m_ResourceType = resource_type;
        et_type.m_Context = this;
        et_type.m_CreateFunction = EventTargetComponentCreate;
        et_type.m_InitFunction = EventTargetComponentInit;
        et_type.m_DestroyFunction = EventTargetComponentDestroy;
        et_type.m_UpdateFunction = EventTargetComponentsUpdate;
        et_type.m_OnEventFunction = &EventTargetOnEvent;
        et_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(collection, et_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        e = dmResource::GetTypeFromExtension(factory, "deleteself", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType ds_type;
        ds_type.m_Name = "deleteself";
        ds_type.m_ResourceType = resource_type;
        ds_type.m_Context = this;
        ds_type.m_CreateFunction = DeleteSelfComponentCreate;
        ds_type.m_InitFunction = DeleteSelfComponentInit;
        ds_type.m_DestroyFunction = DeleteSelfComponentDestroy;
        ds_type.m_UpdateFunction = DeleteSelfComponentsUpdate;
        result = dmGameObject::RegisterComponentType(collection, ds_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_MaxComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash] = 1000000;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(collection);
        dmGameObject::DestroyScriptContext(script_context);
        dmResource::DeleteFactory(factory);
        dmGameObject::Finalize();
    }

    uint32_t m_EventTargetCounter;

    static dmGameObject::UpdateResult EventTargetOnEvent(dmGameObject::HCollection collection,
                                   dmGameObject::HInstance instance,
                                   const dmGameObject::ScriptEventData* event_data,
                                   void* context,
                                   uintptr_t* user_data);

    static dmResource::FResourceCreate    PhysCreate;
    static dmResource::FResourceDestroy   PhysDestroy;
    static dmGameObject::ComponentCreate  PhysComponentCreate;
    static dmGameObject::ComponentInit    PhysComponentInit;
    static dmGameObject::ComponentDestroy PhysComponentDestroy;
    static dmGameObject::ComponentsUpdate PhysComponentsUpdate;

    static dmResource::FResourceCreate    ACreate;
    static dmResource::FResourceDestroy   ADestroy;
    static dmGameObject::ComponentCreate  AComponentCreate;
    static dmGameObject::ComponentInit    AComponentInit;
    static dmGameObject::ComponentDestroy AComponentDestroy;
    static dmGameObject::ComponentsUpdate AComponentsUpdate;

    static dmResource::FResourceCreate    BCreate;
    static dmResource::FResourceDestroy   BDestroy;
    static dmGameObject::ComponentCreate  BComponentCreate;
    static dmGameObject::ComponentInit    BComponentInit;
    static dmGameObject::ComponentDestroy BComponentDestroy;
    static dmGameObject::ComponentsUpdate BComponentsUpdate;

    static dmResource::FResourceCreate    CCreate;
    static dmResource::FResourceDestroy   CDestroy;
    static dmGameObject::ComponentCreate  CComponentCreate;
    static dmGameObject::ComponentInit    CComponentInit;
    static dmGameObject::ComponentDestroy CComponentDestroy;
    static dmGameObject::ComponentsUpdate CComponentsUpdate;

    static dmResource::FResourceCreate    EventTargetCreate;
    static dmResource::FResourceDestroy   EventTargetDestroy;
    static dmGameObject::ComponentCreate  EventTargetComponentCreate;
    static dmGameObject::ComponentInit    EventTargetComponentInit;
    static dmGameObject::ComponentDestroy EventTargetComponentDestroy;
    static dmGameObject::ComponentsUpdate EventTargetComponentsUpdate;

    static dmResource::FResourceCreate    DeleteSelfCreate;
    static dmResource::FResourceDestroy   DeleteSelfDestroy;
    static dmGameObject::ComponentCreate  DeleteSelfComponentCreate;
    static dmGameObject::ComponentInit    DeleteSelfComponentInit;
    static dmGameObject::ComponentDestroy DeleteSelfComponentDestroy;
    static dmGameObject::UpdateResult     DeleteSelfComponentsUpdate(dmGameObject::HCollection collection,
                                           const dmGameObject::UpdateContext* update_context,
                                           void* context);

public:

    std::map<uint64_t, uint32_t> m_CreateCountMap;
    std::map<uint64_t, uint32_t> m_DestroyCountMap;

    std::map<uint64_t, uint32_t> m_ComponentCreateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentInitCountMap;
    std::map<uint64_t, uint32_t> m_ComponentDestroyCountMap;
    std::map<uint64_t, uint32_t> m_ComponentUpdateCountMap;
    std::map<uint64_t, uint32_t> m_MaxComponentCreateCountMap;

    std::map<uint64_t, int>      m_ComponentUserDataAcc;

    // Data DeleteSelf test
    std::vector<dmGameObject::HInstance> m_SelfInstancesToDelete;
    std::vector<dmGameObject::HInstance> m_DeleteSelfInstances;
    std::vector<int> m_DeleteSelfIndices;
    std::map<int, dmGameObject::HInstance> m_DeleteSelfIndexToInstance;

    dmGameObject::UpdateContext update_context;
    dmGameObject::HCollection collection;
    dmResource::HFactory factory;
    dmGameObject::HScriptContext script_context;
};

template <typename T>
dmResource::CreateResult GenericDDFCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource, const char* filename)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_CreateCountMap[T::m_DDFHash]++;

    T* obj;
    dmDDF::Result e = dmDDF::LoadMessage<T>(buffer, buffer_size, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) obj;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

template <typename T>
dmResource::CreateResult GenericDDFDestory(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_DestroyCountMap[T::m_DDFHash]++;

    dmDDF::FreeMessage((void*) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
}

template <typename T, int add_to_user_data>
static dmGameObject::CreateResult GenericComponentCreate(dmGameObject::HCollection collection,
                                                         dmGameObject::HInstance instance,
                                                         void* resource,
                                                         void* context,
                                                         uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;

    if (user_data && add_to_user_data != -1)
    {
        *user_data += add_to_user_data;
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
static dmGameObject::CreateResult GenericComponentInit(dmGameObject::HCollection collection,
                                                        dmGameObject::HInstance instance,
                                                        void* context,
                                                        uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentInitCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::UpdateResult GenericComponentsUpdate(dmGameObject::HCollection collection,
                                    const dmGameObject::UpdateContext* update_context,
                                    void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
    return dmGameObject::UPDATE_RESULT_OK;
}


template <typename T>
static dmGameObject::CreateResult GenericComponentDestroy(dmGameObject::HCollection collection,
                                                          dmGameObject::HInstance instance,
                                                          void* context,
                                                          uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    if (user_data)
    {
        game_object_test->m_ComponentUserDataAcc[T::m_DDFHash] += *user_data;
    }

    game_object_test->m_ComponentDestroyCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate GameObjectTest::PhysCreate              = GenericDDFCreate<TestResource::PhysComponent>;
dmResource::FResourceDestroy GameObjectTest::PhysDestroy            = GenericDDFDestory<TestResource::PhysComponent>;
dmGameObject::ComponentCreate GameObjectTest::PhysComponentCreate   = GenericComponentCreate<TestResource::PhysComponent,-1>;
dmGameObject::ComponentInit GameObjectTest::PhysComponentInit       = GenericComponentInit<TestResource::PhysComponent>;
dmGameObject::ComponentDestroy GameObjectTest::PhysComponentDestroy = GenericComponentDestroy<TestResource::PhysComponent>;
dmGameObject::ComponentsUpdate GameObjectTest::PhysComponentsUpdate = GenericComponentsUpdate<TestResource::PhysComponent>;

dmResource::FResourceCreate GameObjectTest::ACreate              = GenericDDFCreate<TestResource::AResource>;
dmResource::FResourceDestroy GameObjectTest::ADestroy            = GenericDDFDestory<TestResource::AResource>;
dmGameObject::ComponentCreate GameObjectTest::AComponentCreate   = GenericComponentCreate<TestResource::AResource, 1>;
dmGameObject::ComponentInit GameObjectTest::AComponentInit       = GenericComponentInit<TestResource::AResource>;
dmGameObject::ComponentDestroy GameObjectTest::AComponentDestroy = GenericComponentDestroy<TestResource::AResource>;
dmGameObject::ComponentsUpdate GameObjectTest::AComponentsUpdate = GenericComponentsUpdate<TestResource::AResource>;

dmResource::FResourceCreate GameObjectTest::BCreate              = GenericDDFCreate<TestResource::BResource>;
dmResource::FResourceDestroy GameObjectTest::BDestroy            = GenericDDFDestory<TestResource::BResource>;
dmGameObject::ComponentCreate GameObjectTest::BComponentCreate   = GenericComponentCreate<TestResource::BResource, -1>;
dmGameObject::ComponentInit GameObjectTest::BComponentInit       = GenericComponentInit<TestResource::BResource>;
dmGameObject::ComponentDestroy GameObjectTest::BComponentDestroy = GenericComponentDestroy<TestResource::BResource>;
dmGameObject::ComponentsUpdate GameObjectTest::BComponentsUpdate = GenericComponentsUpdate<TestResource::BResource>;

dmResource::FResourceCreate GameObjectTest::CCreate              = GenericDDFCreate<TestResource::CResource>;
dmResource::FResourceDestroy GameObjectTest::CDestroy            = GenericDDFDestory<TestResource::CResource>;
dmGameObject::ComponentCreate GameObjectTest::CComponentCreate   = GenericComponentCreate<TestResource::CResource, 10>;
dmGameObject::ComponentInit GameObjectTest::CComponentInit       = GenericComponentInit<TestResource::CResource>;
dmGameObject::ComponentDestroy GameObjectTest::CComponentDestroy = GenericComponentDestroy<TestResource::CResource>;
dmGameObject::ComponentsUpdate GameObjectTest::CComponentsUpdate = GenericComponentsUpdate<TestResource::CResource>;

dmResource::FResourceCreate GameObjectTest::EventTargetCreate              = GenericDDFCreate<TestResource::EventTarget>;
dmResource::FResourceDestroy GameObjectTest::EventTargetDestroy            = GenericDDFDestory<TestResource::EventTarget>;
dmGameObject::ComponentCreate GameObjectTest::EventTargetComponentCreate   = GenericComponentCreate<TestResource::EventTarget, -1>;
dmGameObject::ComponentInit GameObjectTest::EventTargetComponentInit       = GenericComponentInit<TestResource::EventTarget>;
dmGameObject::ComponentDestroy GameObjectTest::EventTargetComponentDestroy = GenericComponentDestroy<TestResource::EventTarget>;
dmGameObject::ComponentsUpdate GameObjectTest::EventTargetComponentsUpdate = GenericComponentsUpdate<TestResource::EventTarget>;

dmResource::FResourceCreate GameObjectTest::DeleteSelfCreate              = GenericDDFCreate<TestResource::DeleteSelfResource>;
dmResource::FResourceDestroy GameObjectTest::DeleteSelfDestroy            = GenericDDFDestory<TestResource::DeleteSelfResource>;
dmGameObject::ComponentCreate GameObjectTest::DeleteSelfComponentCreate   = GenericComponentCreate<TestResource::DeleteSelfResource, -1>;
dmGameObject::ComponentInit GameObjectTest::DeleteSelfComponentInit       = GenericComponentInit<TestResource::DeleteSelfResource>;
dmGameObject::ComponentDestroy GameObjectTest::DeleteSelfComponentDestroy = GenericComponentDestroy<TestResource::DeleteSelfResource>;

TEST_F(GameObjectTest, Test01)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "goproto01.go");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = dmGameObject::Update(collection, &update_context);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(collection, &update_context);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(collection, &update_context);
    ASSERT_TRUE(ret);
    dmGameObject::Delete(collection, go);

    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestIdentifier)
{
    dmGameObject::HInstance go1 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance go2 = dmGameObject::New(collection, "goproto01.go");
    ASSERT_NE((void*) 0, (void*) go1);
    ASSERT_NE((void*) 0, (void*) go2);

    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    dmGameObject::Result r;
    r = dmGameObject::SetIdentifier(collection, go1, "go1");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));

    r = dmGameObject::SetIdentifier(collection, go1, "go1");
    ASSERT_NE(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));

    r = dmGameObject::SetIdentifier(collection, go2, "go1");
    ASSERT_EQ(dmGameObject::RESULT_IDENTIFIER_IN_USE, r);
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    r = dmGameObject::SetIdentifier(collection, go2, "go2");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    r = dmGameObject::SetIdentifier(collection, go2, "go2");
    ASSERT_NE(dmGameObject::RESULT_OK, r);

    dmGameObject::Delete(collection, go1);
    dmGameObject::Delete(collection, go2);

    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "goproto02.go");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret = dmGameObject::Update(collection, &update_context);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 1, m_ComponentUpdateCountMap[TestResource::PhysComponent::m_DDFHash]);

    dmGameObject::Delete(collection, go);
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestNonexistingComponent)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "goproto03.go");
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);

    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialNonexistingComponent1)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "goproto04.go");
    ASSERT_EQ((void*) 0, (void*) go);

    // First one exists
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    // Even though the first physcomponent exits the prototype creation should fail before creating components
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash] = 1;
    dmGameObject::HInstance go = dmGameObject::New(collection, "goproto05.go");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);

    // One component should get created
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestComponentUserdata)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "goproto06.go");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Delete(collection, go);
    // Two a:s
    ASSERT_EQ(2, m_ComponentUserDataAcc[TestResource::AResource::m_DDFHash]);
    // Zero c:s
    ASSERT_EQ(0, m_ComponentUserDataAcc[TestResource::BResource::m_DDFHash]);
    // Three c:s
    ASSERT_EQ(30, m_ComponentUserDataAcc[TestResource::CResource::m_DDFHash]);
}

TEST_F(GameObjectTest, AutoDelete)
{
    for (int i = 0; i < 512; ++i)
    {
        dmGameObject::HInstance go = dmGameObject::New(collection, "goproto01.go");
        ASSERT_NE((void*) 0, (void*) go);
    }
}

dmGameObject::UpdateResult GameObjectTest::DeleteSelfComponentsUpdate(dmGameObject::HCollection collection,
                                                const dmGameObject::UpdateContext* update_context,
                                                void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;

    for (uint32_t i = 0; i < game_object_test->m_SelfInstancesToDelete.size(); ++i)
    {
        dmGameObject::Delete(game_object_test->collection, game_object_test->m_SelfInstancesToDelete[i]);
        // Test "double delete"
        dmGameObject::Delete(game_object_test->collection, game_object_test->m_SelfInstancesToDelete[i]);
    }

    for (uint32_t i = 0; i < game_object_test->m_DeleteSelfIndices.size(); ++i)
    {
        int index = game_object_test->m_DeleteSelfIndices[i];

        dmGameObject::HInstance go = game_object_test->m_DeleteSelfIndexToInstance[index];
        if (index != (int)dmGameObject::GetPosition(go).getX())
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }

    return dmGameObject::UPDATE_RESULT_OK;
}

TEST_F(GameObjectTest, DeleteSelf)
{
    /*
     * NOTE: We do not have any .deleteself resources on disk even though we register the type
     * Component instances of type 'A' is used here. We need a specific ComponentUpdate though. (DeleteSelfComponentsUpdate)
     * See New(..., goproto01.go") below.
     */
    for (int iter = 0; iter < 4; ++iter)
    {
        m_DeleteSelfInstances.clear();
        m_DeleteSelfIndexToInstance.clear();

        for (int i = 0; i < 512; ++i)
        {
            dmGameObject::HInstance go = dmGameObject::New(collection, "goproto01.go");
            dmGameObject::SetPosition(go, Point3(i,i,i));
            ASSERT_NE((void*) 0, (void*) go);
            m_DeleteSelfInstances.push_back(go);
            m_DeleteSelfIndexToInstance[i] = go;
            m_DeleteSelfIndices.push_back(i);
        }

        std::random_shuffle(m_DeleteSelfIndices.begin(), m_DeleteSelfIndices.end());

        while (m_DeleteSelfIndices.size() > 0)
        {
            for (int i = 0; i < 16; ++i)
            {
                int index = *(m_DeleteSelfIndices.end() - i - 1);
                m_SelfInstancesToDelete.push_back(m_DeleteSelfIndexToInstance[index]);
            }
            bool ret = dmGameObject::Update(collection, 0);
            ASSERT_TRUE(ret);
            for (int i = 0; i < 16; ++i)
            {
                m_DeleteSelfIndices.pop_back();
            }

            m_SelfInstancesToDelete.clear();
        }
    }
}

dmGameObject::UpdateResult GameObjectTest::EventTargetOnEvent(dmGameObject::HCollection collection,
                                        dmGameObject::HInstance instance,
                                        const dmGameObject::ScriptEventData* event_data,
                                        void* context,
                                        uintptr_t* user_data)
{
    GameObjectTest* self = (GameObjectTest*) context;

    if (event_data->m_EventHash == dmHashString32("inc"))
    {
        self->m_EventTargetCounter++;
        if (self->m_EventTargetCounter == 2)
        {
            dmGameObject::PostNamedEvent(instance, "component_event.scriptc", "test_event");
        }
    }
    else if (event_data->m_EventHash == dmHashString32("dec"))
    {
        self->m_EventTargetCounter--;
    }
    else
    {
        assert(0);
        return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
    }
    return dmGameObject::UPDATE_RESULT_OK;
}

TEST_F(GameObjectTest, TestComponentEvent)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "component_event.go");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Result r;

    ASSERT_EQ(0U, m_EventTargetCounter);

    r = dmGameObject::PostNamedEvent(go, "does_not_exists", "inc");
    ASSERT_EQ(dmGameObject::RESULT_COMPONENT_NOT_FOUND, r);

    r = dmGameObject::PostNamedEvent(go, "event_target.et", "inc");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    ASSERT_TRUE(dmGameObject::Update(collection, 0));
    ASSERT_EQ(1U, m_EventTargetCounter);

    r = dmGameObject::PostNamedEvent(go, "event_target.et", "inc");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_TRUE(dmGameObject::Update(collection, 0));
    ASSERT_EQ(2U, m_EventTargetCounter);

    ASSERT_TRUE(dmGameObject::Update(collection, 0));

    dmGameObject::Delete(collection, go);
}

TEST_F(GameObjectTest, TestBroadcastEvent)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "component_broadcast_event.go");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Result r;

    ASSERT_EQ(0U, m_EventTargetCounter);

    r = dmGameObject::PostNamedEvent(go, 0, "inc");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    ASSERT_TRUE(dmGameObject::Update(collection, 0));
    ASSERT_EQ(2U, m_EventTargetCounter);

    dmGameObject::Delete(collection, go);
}

TEST_F(GameObjectTest, TestScriptProperty)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "script_property.go");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::SetScriptIntProperty(go, "MyIntProp", 1010);
    dmGameObject::SetScriptFloatProperty(go, "MyFloatProp", 1.0);
    dmGameObject::SetScriptStringProperty(go, "MyStringProp", "a string prop");

    ASSERT_TRUE(dmGameObject::Update(collection, 0));

    dmGameObject::Delete(collection, go);
}

void TestScript01Dispatch(dmMessage::Message *event_object, void* user_ptr)
{
    dmGameObject::ScriptEventData* script_event_data = (dmGameObject::ScriptEventData*) event_object->m_Data;
    TestResource::Spawn* s = (TestResource::Spawn*) script_event_data->m_DDFData;
    // NOTE: We relocate the string here (from offset to pointer)
    s->m_Prototype = (const char*) ((uintptr_t) s->m_Prototype + (uintptr_t) s);
    bool* dispatch_result = (bool*) user_ptr;

    uint32_t event_id = dmHashString32("spawn_result");

    uint8_t reply_buf[sizeof(dmGameObject::ScriptEventData) + sizeof(TestResource::SpawnResult)];

    TestResource::SpawnResult* result = (TestResource::SpawnResult*) (reply_buf + sizeof(dmGameObject::ScriptEventData));
    result->m_Status = 1010;

    dmGameObject::ScriptEventData* reply_script_event = (dmGameObject::ScriptEventData*) reply_buf;
    reply_script_event->m_EventHash = dmHashString32(TestResource::SpawnResult::m_DDFDescriptor->m_Name);
    reply_script_event->m_Component = 0xff;
    reply_script_event->m_Instance = script_event_data->m_Instance;
    reply_script_event->m_DDFDescriptor = TestResource::SpawnResult::m_DDFDescriptor;

    uint32_t reply_socket = dmHashString32(DMGAMEOBJECT_REPLY_EVENT_SOCKET_NAME);
    dmMessage::Post(reply_socket, event_id, reply_buf, 256);

    *dispatch_result = s->m_Pos.m_X == 1.0 && s->m_Pos.m_Y == 2.0 && s->m_Pos.m_Z == 3.0 && strcmp("test", s->m_Prototype) == 0;
}

void TestScript01DispatchReply(dmMessage::Message *event_object, void* user_ptr)
{

}

TEST_F(GameObjectTest, TestScript01)
{
    dmGameObject::HInstance go = dmGameObject::New(collection, "testscriptproto01.go");
    ASSERT_NE((void*) 0, (void*) go);

    TestResource::GlobalData global_data;
    global_data.m_UIntValue = 12345;
    global_data.m_IntValue = -123;
    global_data.m_StringValue = "string_value";
    global_data.m_VecValue.m_X = 1.0f;
    global_data.m_VecValue.m_Y = 2.0f;
    global_data.m_VecValue.m_Z = 3.0f;

    update_context.m_GlobalData = &global_data;
    update_context.m_DDFGlobalDataDescriptor = TestResource::GlobalData::m_DDFDescriptor;

    dmGameObject::Init(collection, &update_context);

    ASSERT_TRUE(dmGameObject::Update(collection, &update_context));

    uint32_t socket = dmHashString32(DMGAMEOBJECT_EVENT_SOCKET_NAME);
    uint32_t reply_socket = dmHashString32(DMGAMEOBJECT_REPLY_EVENT_SOCKET_NAME);
    bool dispatch_result = false;
    dmMessage::Dispatch(socket, TestScript01Dispatch, &dispatch_result);

    ASSERT_TRUE(dispatch_result);

    ASSERT_TRUE(dmGameObject::Update(collection, &update_context));
    // Final dispatch to deallocate event data
    dmMessage::Dispatch(socket, TestScript01Dispatch, &dispatch_result);
    dmMessage::Dispatch(reply_socket, TestScript01DispatchReply, &dispatch_result);

    dmGameObject::Delete(collection, go);
}

TEST_F(GameObjectTest, TestFailingScript02)
{
    // Test init failure

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    dmGameObject::New(collection, "testscriptproto02.go");
    bool result = dmGameObject::Init(collection, &update_context);
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    ASSERT_FALSE(result);
}

TEST_F(GameObjectTest, TestFailingScript03)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(collection, "testscriptproto03.go");
    ASSERT_NE((void*) 0, (void*) go);

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    ASSERT_FALSE(dmGameObject::Update(collection, &update_context));
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    dmGameObject::Delete(collection, go);
}

static void CreateFile(const char* file_name, const char* contents)
{
    FILE* f;
    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "%s", contents);
    fclose(f);
}

TEST(ScriptTest, TestReloadScript)
{
    dmGameObject::Initialize();
    dmGameObject::RegisterDDFType(TestResource::Spawn::m_DDFDescriptor);

    const char* tmp_dir = 0;
#if defined(_MSC_VER)
    tmp_dir = ".";
#else
    tmp_dir = "/tmp";
#endif

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, tmp_dir);
    dmGameObject::HCollection collection = dmGameObject::NewCollection(factory, 1024);
    dmGameObject::RegisterResourceTypes(factory);
    dmGameObject::HScriptContext script_context = dmGameObject::CreateScriptContext(0x0);
    dmGameObject::RegisterComponentTypes(factory, collection, script_context);

    uint32_t type;
    dmResource::FactoryResult e = dmResource::GetTypeFromExtension(factory, "scriptc", &type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

    const char* script_file_name = "__test__.scriptc";
    char script_path[512];
    DM_SNPRINTF(script_path, sizeof(script_path), "%s/%s", tmp_dir, script_file_name);

    char go_file_name[512];
    DM_SNPRINTF(go_file_name, sizeof(go_file_name), "%s/%s", tmp_dir, "__go__.go");

    dmGameObject::PrototypeDesc prototype;
    //memset(&prototype, 0, sizeof(prototype));
    prototype.m_Name = "foo";
    prototype.m_Components.m_Count = 1;
    dmGameObject::ComponentDesc component_desc;
    memset(&component_desc, 0, sizeof(component_desc));
    component_desc.m_Name = script_file_name;
    component_desc.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    component_desc.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    prototype.m_Components.m_Data = &component_desc;

    dmDDF::Result ddf_r = dmDDF::SaveMessageToFile(&prototype, dmGameObject::PrototypeDesc::m_DDFDescriptor, go_file_name);
    ASSERT_EQ(dmDDF::RESULT_OK, ddf_r);

    CreateFile(script_path,
               "function Init(self)\n"
               "end\n"
               "function Update(self)\n"
                   "self.Position = {1,2,3}\n"
               "end\n"
               "functions = { Init = Init, Update = Update }\n");

    dmGameObject::HInstance go;
    go = dmGameObject::New(collection, "__go__.go");
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    dmGameObject::Update(collection, 0);
    Point3 p1 = dmGameObject::GetPosition(go);
    ASSERT_EQ(1, p1.getX());
    ASSERT_EQ(2, p1.getY());
    ASSERT_EQ(3, p1.getZ());

    dmSleep(1000000); // TODO: Currently seconds time resolution in modification time

    CreateFile(script_path,
               "function Init(self)\n"
               "end\n"
               "function Update(self)\n"
                   "self.Position = {10,20,30}\n"
               "end\n"
               "functions = { Init = Init, Update = Update }\n");


    dmResource::FactoryResult fr = dmResource::ReloadType(factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, fr);

    dmGameObject::Update(collection, 0);
    Point3 p2 = dmGameObject::GetPosition(go);
    ASSERT_EQ(10, p2.getX());
    ASSERT_EQ(20, p2.getY());
    ASSERT_EQ(30, p2.getZ());

    unlink(script_path);
    fr = dmResource::ReloadType(factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_RESOURCE_NOT_FOUND, fr);

    unlink(go_file_name);

    dmGameObject::Delete(collection, go);
    dmGameObject::DeleteCollection(collection);
    dmGameObject::DestroyScriptContext(script_context);
    dmResource::DeleteFactory(factory);
    dmGameObject::Finalize();
}

TEST_F(GameObjectTest, TestHierarchy1)
{
    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
        dmGameObject::HInstance child = dmGameObject::New(collection, "goproto01.go");

        const float parent_rot = 3.14159265f / 4.0f;

        Point3 parent_pos(1, 2, 0);
        Point3 child_pos(4, 5, 0);

        Matrix4 parent_m = Matrix4::rotationZ(parent_rot);
        parent_m.setCol3(Vector4(parent_pos));

        Matrix4 child_m = Matrix4::identity();
        child_m.setCol3(Vector4(child_pos));

        dmGameObject::SetPosition(parent, parent_pos);
        dmGameObject::SetRotation(parent, Quat::rotationZ(parent_rot));
        dmGameObject::SetPosition(child, child_pos);

        ASSERT_EQ(0U, dmGameObject::GetDepth(child));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        dmGameObject::SetParent(child, parent);

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));

        ASSERT_EQ(1U, dmGameObject::GetDepth(child));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        bool ret;
        ret = dmGameObject::Update(collection, 0);
        ASSERT_TRUE(ret);

        Point3 expected_child_pos = Point3((parent_m * child_pos).getXYZ());

        ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(parent) - parent_pos), 0.001f);
        ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child) - expected_child_pos), 0.001f);

        if (i % 2 == 0)
        {
            dmGameObject::Delete(collection, parent);
            ASSERT_EQ(0U, dmGameObject::GetDepth(child));
            ASSERT_EQ(0U, dmGameObject::GetChildCount(child));
            dmGameObject::Delete(collection, child);
        }
        else
        {
            dmGameObject::Delete(collection, child);
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(0U, dmGameObject::GetChildCount(parent));
            dmGameObject::Delete(collection, parent);
        }
    }
}

TEST_F(GameObjectTest, TestHierarchy2)
{
    // Test transform
    dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child_child = dmGameObject::New(collection, "goproto01.go");

    const float parent_rot = 3.14159265f / 4.0f;
    const float child_rot = 3.14159265f / 5.0f;

    Point3 parent_pos(1, 1, 0);
    Point3 child_pos(0, 1, 0);
    Point3 child_child_pos(7, 2, 0);

    Matrix4 parent_m = Matrix4::rotationZ(parent_rot);
    parent_m.setCol3(Vector4(parent_pos));

    Matrix4 child_m = Matrix4::rotationZ(child_rot);
    child_m.setCol3(Vector4(child_pos));

    Matrix4 child_child_m = Matrix4::identity();
    child_child_m.setCol3(Vector4(child_child_pos));

    dmGameObject::SetPosition(parent, parent_pos);
    dmGameObject::SetRotation(parent, Quat::rotationZ(parent_rot));
    dmGameObject::SetPosition(child, child_pos);
    dmGameObject::SetRotation(child, Quat::rotationZ(child_rot));
    dmGameObject::SetPosition(child_child, child_child_pos);

    dmGameObject::SetParent(child, parent);
    dmGameObject::SetParent(child_child, child);

    ASSERT_EQ(1U, dmGameObject::GetChildCount(child));
    ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));

    ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
    ASSERT_EQ(1U, dmGameObject::GetDepth(child));
    ASSERT_EQ(2U, dmGameObject::GetDepth(child_child));

    bool ret;
    ret = dmGameObject::Update(collection, 0);
    ASSERT_TRUE(ret);

    Point3 expected_child_pos = Point3((parent_m * child_pos).getXYZ());
    Point3 expected_child_child_pos = Point3(((parent_m * child_m) * child_child_pos).getXYZ());
    Point3 expected_child_child_pos2 = Point3(((parent_m * child_m) * child_child_m).getCol3().getXYZ());

    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(parent) - parent_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child) - expected_child_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child_child) - expected_child_child_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child_child) - expected_child_child_pos2), 0.001f);

    dmGameObject::Delete(collection, parent);
    dmGameObject::Delete(collection, child);
    dmGameObject::Delete(collection, child_child);
}

TEST_F(GameObjectTest, TestHierarchy3)
{
    // Test with siblings
    for (int i = 0; i < 3; ++i)
    {
        dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
        dmGameObject::HInstance child1 = dmGameObject::New(collection, "goproto01.go");
        dmGameObject::HInstance child2 = dmGameObject::New(collection, "goproto01.go");

        ASSERT_EQ(0U, dmGameObject::GetDepth(child1));
        ASSERT_EQ(0U, dmGameObject::GetDepth(child2));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child1));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(child2));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(parent));

        dmGameObject::SetParent(child1, parent);

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child1));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(child2));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));

        dmGameObject::SetParent(child2, parent);

        ASSERT_EQ(0U, dmGameObject::GetChildCount(child1));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(child2));
        ASSERT_EQ(2U, dmGameObject::GetChildCount(parent));

        ASSERT_EQ(1U, dmGameObject::GetDepth(child1));
        ASSERT_EQ(1U, dmGameObject::GetDepth(child2));
        ASSERT_EQ(0U, dmGameObject::GetDepth(parent));

        bool ret;
        ret = dmGameObject::Update(collection, 0);
        ASSERT_TRUE(ret);

        // Test all possible delete orders in this configuration
        if (i == 0)
        {
            dmGameObject::Delete(collection, parent);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child1));
            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(collection, child1);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(collection, child2);
        }
        else if (i == 1)
        {
            dmGameObject::Delete(collection, child1);

            ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(1U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(collection, parent);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(collection, child2);
        }
        else if (i == 2)
        {
            dmGameObject::Delete(collection, child2);

            ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(1U, dmGameObject::GetDepth(child1));

            dmGameObject::Delete(collection, parent);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child1));

            dmGameObject::Delete(collection, child1);
        }
        else
        {
            assert(false);
        }
    }
}

TEST_F(GameObjectTest, TestHierarchy4)
{
    // Test RESULT_MAXIMUM_HIEARCHICAL_DEPTH

    dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child1 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child2 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child3 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child4 = dmGameObject::New(collection, "goproto01.go");

    dmGameObject::Result r;

    r = dmGameObject::SetParent(child1, parent);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    r = dmGameObject::SetParent(child2, child1);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    r = dmGameObject::SetParent(child3, child2);
    ASSERT_EQ(dmGameObject::RESULT_OK, r);

    r = dmGameObject::SetParent(child4, child3);
    ASSERT_EQ(dmGameObject::RESULT_MAXIMUM_HIEARCHICAL_DEPTH, r);

    ASSERT_EQ(0U, dmGameObject::GetChildCount(child3));

    dmGameObject::Delete(collection, parent);
    dmGameObject::Delete(collection, child1);
    dmGameObject::Delete(collection, child2);
    dmGameObject::Delete(collection, child3);
    dmGameObject::Delete(collection, child4);
}

TEST_F(GameObjectTest, TestHierarchy5)
{
    // Test parent subtree

    dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child1 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child2 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child3 = dmGameObject::New(collection, "goproto01.go");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child3, child2);

    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));
    ASSERT_EQ(child2, dmGameObject::GetParent(child3));

    dmGameObject::Delete(collection, parent);
    dmGameObject::Delete(collection, child1);
    dmGameObject::Delete(collection, child2);
    dmGameObject::Delete(collection, child3);
}

TEST_F(GameObjectTest, TestHierarchy6)
{
    // Test invalid reparent.
    // Test that the child node is not present in the upward trace from parent

    dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child1 = dmGameObject::New(collection, "goproto01.go");

    // parent -> child1
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(child1, parent));

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));

    // child1 -> parent
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameObject::SetParent(parent, child1));

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));

    dmGameObject::Delete(collection, parent);
    dmGameObject::Delete(collection, child1);
}

TEST_F(GameObjectTest, TestHierarchy7)
{
    // Test remove interior node

    dmGameObject::HInstance parent = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child1 = dmGameObject::New(collection, "goproto01.go");
    dmGameObject::HInstance child2 = dmGameObject::New(collection, "goproto01.go");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));

    dmGameObject::Delete(collection, child1);
    ASSERT_EQ(parent, dmGameObject::GetParent(child2));
    ASSERT_TRUE(dmGameObject::IsChildOf(child2, parent));

    dmGameObject::Delete(collection, parent);
    dmGameObject::Delete(collection, child2);
}

TEST_F(GameObjectTest, TestHierarchy8)
{
    /*
        A1
      B2  C2
     D3

     Rearrange tree to:

        A1
          C2
            B2
              D3
     */

    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HInstance a1 = dmGameObject::New(collection, "goproto01.go");
        dmGameObject::HInstance b2 = dmGameObject::New(collection, "goproto01.go");
        dmGameObject::HInstance c2 = dmGameObject::New(collection, "goproto01.go");
        dmGameObject::HInstance d3 = dmGameObject::New(collection, "goproto01.go");

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(d3, b2));
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(b2, a1));
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(c2, a1));

        ASSERT_EQ(a1, dmGameObject::GetParent(b2));
        ASSERT_EQ(a1, dmGameObject::GetParent(c2));
        ASSERT_EQ(b2, dmGameObject::GetParent(d3));

        bool ret;
        ret = dmGameObject::Update(collection, 0);
        ASSERT_TRUE(ret);

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(b2, c2));

        ASSERT_EQ(a1, dmGameObject::GetParent(c2));
        ASSERT_EQ(c2, dmGameObject::GetParent(b2));
        ASSERT_EQ(b2, dmGameObject::GetParent(d3));

        ASSERT_EQ(1U, dmGameObject::GetChildCount(a1));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(c2));
        ASSERT_EQ(1U, dmGameObject::GetChildCount(b2));
        ASSERT_EQ(0U, dmGameObject::GetChildCount(d3));

        ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
        ASSERT_EQ(1U, dmGameObject::GetDepth(c2));
        ASSERT_EQ(2U, dmGameObject::GetDepth(b2));
        ASSERT_EQ(3U, dmGameObject::GetDepth(d3));

        ASSERT_TRUE(dmGameObject::IsChildOf(c2, a1));
        ASSERT_TRUE(dmGameObject::IsChildOf(b2, c2));
        ASSERT_TRUE(dmGameObject::IsChildOf(d3, b2));

        if (i == 0)
        {
            ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
            dmGameObject::Delete(collection, a1);

            ASSERT_EQ(0U, dmGameObject::GetDepth(c2));
            dmGameObject::Delete(collection, c2);

            ASSERT_EQ(0U, dmGameObject::GetDepth(b2));
            dmGameObject::Delete(collection, b2);

            ASSERT_EQ(0U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(collection, d3);
        }
        else
        {
            ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
            ASSERT_EQ(3U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(collection, a1);

            ASSERT_EQ(1U, dmGameObject::GetDepth(b2));
            ASSERT_EQ(2U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(collection, b2);
            ASSERT_EQ(c2, dmGameObject::GetParent(d3));
            ASSERT_TRUE(dmGameObject::IsChildOf(d3, c2));

            ASSERT_EQ(0U, dmGameObject::GetDepth(c2));
            ASSERT_EQ(1U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(collection, c2);

            ASSERT_EQ(0U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(collection, d3);
        }
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
