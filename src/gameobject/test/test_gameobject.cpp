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
#include "gameobject/test/test_gameobject_ddf.h"
#include "../proto/gameobject_ddf.h"

using namespace Vectormath::Aos;

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();
        dmGameObject::RegisterDDFType(TestGameObject::Spawn::m_DDFDescriptor);

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test");
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);

        // Register dummy physical resource type
        dmResource::FactoryResult e;
        e = dmResource::RegisterType(m_Factory, "pc", this, PhysCreate, PhysDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "a", this, ACreate, ADestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "b", this, BCreate, BDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "c", this, CCreate, CDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "deleteself", this, DeleteSelfCreate, DeleteSelfDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

        uint32_t resource_type;

        e = dmResource::GetTypeFromExtension(m_Factory, "pc", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType pc_type;
        pc_type.m_Name = "pc";
        pc_type.m_ResourceType = resource_type;
        pc_type.m_Context = this;
        pc_type.m_CreateFunction = PhysComponentCreate;
        pc_type.m_InitFunction = PhysComponentInit;
        pc_type.m_DestroyFunction = PhysComponentDestroy;
        pc_type.m_UpdateFunction = PhysComponentsUpdate;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, pc_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "a", &resource_type);
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
        result = dmGameObject::RegisterComponentType(m_Register, a_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // B has *not* component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "b", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType b_type;
        b_type.m_Name = "b";
        b_type.m_ResourceType = resource_type;
        b_type.m_Context = this;
        b_type.m_CreateFunction = BComponentCreate;
        b_type.m_InitFunction = BComponentInit;
        b_type.m_DestroyFunction = BComponentDestroy;
        b_type.m_UpdateFunction = BComponentsUpdate;
        result = dmGameObject::RegisterComponentType(m_Register, b_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // C has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "c", &resource_type);
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
        result = dmGameObject::RegisterComponentType(m_Register, c_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        e = dmResource::GetTypeFromExtension(m_Factory, "deleteself", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType ds_type;
        ds_type.m_Name = "deleteself";
        ds_type.m_ResourceType = resource_type;
        ds_type.m_Context = this;
        ds_type.m_CreateFunction = DeleteSelfComponentCreate;
        ds_type.m_InitFunction = DeleteSelfComponentInit;
        ds_type.m_DestroyFunction = DeleteSelfComponentDestroy;
        ds_type.m_UpdateFunction = DeleteSelfComponentsUpdate;
        result = dmGameObject::RegisterComponentType(m_Register, ds_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_MaxComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash] = 1000000;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
    }

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

    static dmResource::FResourceCreate    DeleteSelfCreate;
    static dmResource::FResourceDestroy   DeleteSelfDestroy;
    static dmGameObject::ComponentCreate  DeleteSelfComponentCreate;
    static dmGameObject::ComponentInit    DeleteSelfComponentInit;
    static dmGameObject::ComponentDestroy DeleteSelfComponentDestroy;
    static dmGameObject::UpdateResult     DeleteSelfComponentsUpdate(dmGameObject::HCollection collection,
                                           const dmGameObject::UpdateContext* update_context,
                                           void* world,
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

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
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
                                                         void* world,
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
                                    void* world,
                                    void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
    return dmGameObject::UPDATE_RESULT_OK;
}


template <typename T>
static dmGameObject::CreateResult GenericComponentDestroy(dmGameObject::HCollection collection,
                                                          dmGameObject::HInstance instance,
                                                          void* world,
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

dmResource::FResourceCreate GameObjectTest::PhysCreate              = GenericDDFCreate<TestGameObject::PhysComponent>;
dmResource::FResourceDestroy GameObjectTest::PhysDestroy            = GenericDDFDestory<TestGameObject::PhysComponent>;
dmGameObject::ComponentCreate GameObjectTest::PhysComponentCreate   = GenericComponentCreate<TestGameObject::PhysComponent,-1>;
dmGameObject::ComponentInit GameObjectTest::PhysComponentInit       = GenericComponentInit<TestGameObject::PhysComponent>;
dmGameObject::ComponentDestroy GameObjectTest::PhysComponentDestroy = GenericComponentDestroy<TestGameObject::PhysComponent>;
dmGameObject::ComponentsUpdate GameObjectTest::PhysComponentsUpdate = GenericComponentsUpdate<TestGameObject::PhysComponent>;

dmResource::FResourceCreate GameObjectTest::ACreate              = GenericDDFCreate<TestGameObject::AResource>;
dmResource::FResourceDestroy GameObjectTest::ADestroy            = GenericDDFDestory<TestGameObject::AResource>;
dmGameObject::ComponentCreate GameObjectTest::AComponentCreate   = GenericComponentCreate<TestGameObject::AResource, 1>;
dmGameObject::ComponentInit GameObjectTest::AComponentInit       = GenericComponentInit<TestGameObject::AResource>;
dmGameObject::ComponentDestroy GameObjectTest::AComponentDestroy = GenericComponentDestroy<TestGameObject::AResource>;
dmGameObject::ComponentsUpdate GameObjectTest::AComponentsUpdate = GenericComponentsUpdate<TestGameObject::AResource>;

dmResource::FResourceCreate GameObjectTest::BCreate              = GenericDDFCreate<TestGameObject::BResource>;
dmResource::FResourceDestroy GameObjectTest::BDestroy            = GenericDDFDestory<TestGameObject::BResource>;
dmGameObject::ComponentCreate GameObjectTest::BComponentCreate   = GenericComponentCreate<TestGameObject::BResource, -1>;
dmGameObject::ComponentInit GameObjectTest::BComponentInit       = GenericComponentInit<TestGameObject::BResource>;
dmGameObject::ComponentDestroy GameObjectTest::BComponentDestroy = GenericComponentDestroy<TestGameObject::BResource>;
dmGameObject::ComponentsUpdate GameObjectTest::BComponentsUpdate = GenericComponentsUpdate<TestGameObject::BResource>;

dmResource::FResourceCreate GameObjectTest::CCreate              = GenericDDFCreate<TestGameObject::CResource>;
dmResource::FResourceDestroy GameObjectTest::CDestroy            = GenericDDFDestory<TestGameObject::CResource>;
dmGameObject::ComponentCreate GameObjectTest::CComponentCreate   = GenericComponentCreate<TestGameObject::CResource, 10>;
dmGameObject::ComponentInit GameObjectTest::CComponentInit       = GenericComponentInit<TestGameObject::CResource>;
dmGameObject::ComponentDestroy GameObjectTest::CComponentDestroy = GenericComponentDestroy<TestGameObject::CResource>;
dmGameObject::ComponentsUpdate GameObjectTest::CComponentsUpdate = GenericComponentsUpdate<TestGameObject::CResource>;

dmResource::FResourceCreate GameObjectTest::DeleteSelfCreate              = GenericDDFCreate<TestGameObject::DeleteSelfResource>;
dmResource::FResourceDestroy GameObjectTest::DeleteSelfDestroy            = GenericDDFDestory<TestGameObject::DeleteSelfResource>;
dmGameObject::ComponentCreate GameObjectTest::DeleteSelfComponentCreate   = GenericComponentCreate<TestGameObject::DeleteSelfResource, -1>;
dmGameObject::ComponentInit GameObjectTest::DeleteSelfComponentInit       = GenericComponentInit<TestGameObject::DeleteSelfResource>;
dmGameObject::ComponentDestroy GameObjectTest::DeleteSelfComponentDestroy = GenericComponentDestroy<TestGameObject::DeleteSelfResource>;

TEST_F(GameObjectTest, Test01)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto01.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    dmGameObject::Delete(m_Collection, go);

    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestIdentifier)
{
    dmGameObject::HInstance go1 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance go2 = dmGameObject::New(m_Collection, "goproto01.goc");
    ASSERT_NE((void*) 0, (void*) go1);
    ASSERT_NE((void*) 0, (void*) go2);

    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    dmGameObject::Result r;
    r = dmGameObject::SetIdentifier(m_Collection, go1, "go1");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));

    r = dmGameObject::SetIdentifier(m_Collection, go1, "go1");
    ASSERT_NE(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go1));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go1");
    ASSERT_EQ(dmGameObject::RESULT_IDENTIFIER_IN_USE, r);
    ASSERT_EQ(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go2");
    ASSERT_EQ(dmGameObject::RESULT_OK, r);
    ASSERT_NE(dmGameObject::UNNAMED_IDENTIFIER, dmGameObject::GetIdentifier(go2));

    r = dmGameObject::SetIdentifier(m_Collection, go2, "go2");
    ASSERT_NE(dmGameObject::RESULT_OK, r);

    dmGameObject::Delete(m_Collection, go1);
    dmGameObject::Delete(m_Collection, go2);

    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto02.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 1, m_ComponentUpdateCountMap[TestGameObject::PhysComponent::m_DDFHash]);

    dmGameObject::Delete(m_Collection, go);
    ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPostDeleteUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto02.goc");
    ASSERT_NE((void*) 0, (void*) go);

    uint32_t message_id = dmHashString32("test");

    dmGameObject::InstanceMessageData data;
    data.m_MessageId = message_id;
    data.m_Component = 0xff;
    data.m_Instance = go;
    data.m_DDFDescriptor = 0x0;
    dmMessage::Post(dmGameObject::GetReplyMessageSocketId(m_Register), message_id, (void*)&data, sizeof(dmGameObject::InstanceMessageData));

    dmGameObject::Delete(m_Collection, go);

    bool ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
}

TEST_F(GameObjectTest, TestNonexistingComponent)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto03.goc");
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);

    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialNonexistingComponent1)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto04.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    // First one exists
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    // Even though the first physcomponent exits the prototype creation should fail before creating components
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash] = 1;
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto05.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);

    // One component should get created
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestComponentUserdata)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto06.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Delete(m_Collection, go);
    bool ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    // Two a:s
    ASSERT_EQ(2, m_ComponentUserDataAcc[TestGameObject::AResource::m_DDFHash]);
    // Zero c:s
    ASSERT_EQ(0, m_ComponentUserDataAcc[TestGameObject::BResource::m_DDFHash]);
    // Three c:s
    ASSERT_EQ(30, m_ComponentUserDataAcc[TestGameObject::CResource::m_DDFHash]);
}

TEST_F(GameObjectTest, AutoDelete)
{
    for (int i = 0; i < 512; ++i)
    {
        dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto01.goc");
        ASSERT_NE((void*) 0, (void*) go);
    }
}

dmGameObject::UpdateResult GameObjectTest::DeleteSelfComponentsUpdate(dmGameObject::HCollection m_Collection,
                                                const dmGameObject::UpdateContext* update_context,
                                                void* world,
                                                void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;

    for (uint32_t i = 0; i < game_object_test->m_SelfInstancesToDelete.size(); ++i)
    {
        dmGameObject::Delete(game_object_test->m_Collection, game_object_test->m_SelfInstancesToDelete[i]);
        // Test "double delete"
        dmGameObject::Delete(game_object_test->m_Collection, game_object_test->m_SelfInstancesToDelete[i]);
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
     * See New(..., goproto01.goc") below.
     */
    for (int iter = 0; iter < 4; ++iter)
    {
        m_DeleteSelfInstances.clear();
        m_DeleteSelfIndexToInstance.clear();

        for (int i = 0; i < 512; ++i)
        {
            dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto01.goc");
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
            bool ret = dmGameObject::Update(&m_Collection, 0, 1);
            ASSERT_TRUE(ret);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
            for (int i = 0; i < 16; ++i)
            {
                m_DeleteSelfIndices.pop_back();
            }

            m_SelfInstancesToDelete.clear();
        }
    }
}

TEST_F(GameObjectTest, Null)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "null.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::PostNamedMessageTo(go, 0, dmHashString32("test"), 0x0, 0));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString32("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(&m_Collection, 1, &action, 1));

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(GameObjectTest, TestScriptProperty)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "script_property.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::SetScriptIntProperty(go, "my_int_prop", 1010);
    dmGameObject::SetScriptFloatProperty(go, "my_float_prop", 1.0);
    dmGameObject::SetScriptStringProperty(go, "my_string_prop", "a string prop");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

struct TestScript01Context
{
    dmGameObject::HRegister m_Register;
    bool m_Result;
};

void TestScript01Dispatch(dmMessage::Message *message_object, void* user_ptr)
{
    dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message_object->m_Data;
    TestGameObject::Spawn* s = (TestGameObject::Spawn*) instance_message_data->m_Buffer;
    // NOTE: We relocate the string here (from offset to pointer)
    s->m_Prototype = (const char*) ((uintptr_t) s->m_Prototype + (uintptr_t) s);
    TestScript01Context* context = (TestScript01Context*)user_ptr;
    bool* dispatch_result = &context->m_Result;

    TestGameObject::SpawnResult result;
    result.m_Status = 1010;
    dmGameObject::PostDDFMessageTo(instance_message_data->m_Instance, 0x0, TestGameObject::SpawnResult::m_DDFDescriptor, (char*)&result);

    *dispatch_result = s->m_Pos.getX() == 1.0 && s->m_Pos.getY() == 2.0 && s->m_Pos.getZ() == 3.0 && strcmp("test", s->m_Prototype) == 0;
}

void TestScript01DispatchReply(dmMessage::Message *message_object, void* user_ptr)
{

}

TEST_F(GameObjectTest, TestScript01)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "testscriptproto01.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetIdentifier(m_Collection, go, "my_object01"));

    TestGameObject::GlobalData global_data;
    global_data.m_UIntValue = 12345;
    global_data.m_IntValue = -123;
    global_data.m_StringValue = "string_value";
    global_data.m_VecValue.setX(1.0f);
    global_data.m_VecValue.setY(2.0f);
    global_data.m_VecValue.setZ(3.0f);

    dmGameObject::Init(m_Collection);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));

    uint32_t socket = dmGameObject::GetMessageSocketId(m_Register);
    uint32_t reply_socket = dmGameObject::GetReplyMessageSocketId(m_Register);
    TestScript01Context context;
    context.m_Register = m_Register;
    context.m_Result = false;
    dmMessage::Dispatch(socket, TestScript01Dispatch, &context);

    ASSERT_TRUE(context.m_Result);

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    // Final dispatch to deallocate message data
    dmMessage::Dispatch(socket, TestScript01Dispatch, &context);
    dmMessage::Dispatch(reply_socket, TestScript01DispatchReply, &context);

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString32("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(&m_Collection, 1, &action, 1));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(GameObjectTest, TestFailingScript02)
{
    // Test init failure

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    dmGameObject::New(m_Collection, "testscriptproto02.goc");
    bool result = dmGameObject::Init(m_Collection);
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    ASSERT_FALSE(result);
}

TEST_F(GameObjectTest, TestFailingScript03)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "testscriptproto03.goc");
    ASSERT_NE((void*) 0, (void*) go);

    // Avoid logging expected errors. Better solution?
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    ASSERT_FALSE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
    dmGameObject::Delete(m_Collection, go);
}

TEST_F(GameObjectTest, TestFailingScript04)
{
    // Test update failure
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "testscriptproto04.goc");
    ASSERT_EQ((void*) 0, (void*) go);
}

TEST_F(GameObjectTest, Collection)
{
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not m_Collection in GameObjectTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "level1.collectionc", (void**) &coll);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        uint32_t go01ident = dmHashString32("go01");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);

        uint32_t go02ident = dmHashString32("go02");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);

        dmGameObject::Init(coll);
        dmGameObject::Update(&coll, 0, 1);

        ASSERT_NE(go01, go02);

        dmResource::Release(m_Factory, (void*) coll);
    }
}

TEST_F(GameObjectTest, PostCollection)
{
    for (int i = 0; i < 10; ++i)
    {
        dmResource::FactoryResult r;
        dmGameObject::HCollection coll1;
        r = dmResource::Get(m_Factory, "postcollection1.collectionc", (void**) &coll1);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll1);

        dmGameObject::HCollection coll2;
        r = dmResource::Get(m_Factory, "postcollection2.collectionc", (void**) &coll2);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll2);


/*        uint32_t go01ident = dmHashString32("go01");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);

        uint32_t go02ident = dmHashString32("go02");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);*/

        bool ret;
        dmGameObject::Init(coll1);
        dmGameObject::Init(coll2);

        dmGameObject::HCollection collections[2] = {coll1, coll2};
        ret = dmGameObject::Update(collections, 0, 2);
        ASSERT_TRUE(ret);

        //ASSERT_NE(go01, go02);

        dmResource::Release(m_Factory, (void*) coll1);
        dmResource::Release(m_Factory, (void*) coll2);
    }
}

TEST_F(GameObjectTest, CollectionFail)
{
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not collection in GameObjectTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "failing_sub.collectionc", (void**) &coll);
        ASSERT_NE(dmResource::FACTORY_RESULT_OK, r);
    }
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
}

TEST_F(GameObjectTest, CollectionInCollection)
{
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not collection in GameObjectTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "root.collectionc", (void**) &coll);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, r);
        ASSERT_NE((void*) 0, coll);

        uint32_t go01ident = dmHashString32("go01");
        dmGameObject::HInstance go01 = dmGameObject::GetInstanceFromIdentifier(coll, go01ident);
        ASSERT_NE((void*) 0, go01);
        ASSERT_NEAR(dmGameObject::GetPosition(go01).getX(), 123.0f, 0.0000f);

        uint32_t go02ident = dmHashString32("go02");
        dmGameObject::HInstance go02 = dmGameObject::GetInstanceFromIdentifier(coll, go02ident);
        ASSERT_NE((void*) 0, go02);
        ASSERT_NEAR(dmGameObject::GetPosition(go02).getX(), 456.0f, 0.0000f);

        ASSERT_NE(go01, go02);

        uint32_t go01_sub1_ident = dmHashString32("sub1.go01");
        dmGameObject::HInstance go01_sub1 = dmGameObject::GetInstanceFromIdentifier(coll, go01_sub1_ident);
        ASSERT_NE((void*) 0, go01_sub1);
        ASSERT_NEAR(dmGameObject::GetPosition(go01_sub1).getY(), 1000.0f + 10.0f, 0.0000f);

        uint32_t go02_sub1_ident = dmHashString32("sub1.go02");
        dmGameObject::HInstance go02_sub1 = dmGameObject::GetInstanceFromIdentifier(coll, go02_sub1_ident);
        ASSERT_NE((void*) 0, go02_sub1);
        ASSERT_NEAR(dmGameObject::GetPosition(go02_sub1).getY(), 1000.0f + 20.0f, 0.0000f);

        uint32_t go01_sub2_ident = dmHashString32("sub2.go01");
        dmGameObject::HInstance go01_sub2 = dmGameObject::GetInstanceFromIdentifier(coll, go01_sub2_ident);
        ASSERT_NE((void*) 0, go01_sub2);
        ASSERT_NEAR(dmGameObject::GetPosition(go01_sub2).getY(), 1000.0f + 10.0f, 0.0000f);

        uint32_t go02_sub2_ident = dmHashString32("sub2.go02");
        dmGameObject::HInstance go02_sub2 = dmGameObject::GetInstanceFromIdentifier(coll, go02_sub2_ident);
        ASSERT_NE((void*) 0, go02_sub2);
        ASSERT_NEAR(dmGameObject::GetPosition(go02_sub2).getY(), 1000.0f + 20.0f, 0.0000f);

        // Relative identifiers
        ASSERT_EQ(dmHashString32("a"), dmGameObject::GetAbsoluteIdentifier(go01, "a"));
        ASSERT_EQ(dmHashString32("a"), dmGameObject::GetAbsoluteIdentifier(go02, "a"));
        ASSERT_EQ(dmHashString32("sub1.a"), dmGameObject::GetAbsoluteIdentifier(go01_sub1, "a"));
        ASSERT_EQ(dmHashString32("sub2.a"), dmGameObject::GetAbsoluteIdentifier(go01_sub2, "a"));

        bool ret = dmGameObject::Update(&coll, 0, 1);
        ASSERT_TRUE(ret);

        dmResource::Release(m_Factory, (void*) coll);
    }
}

TEST_F(GameObjectTest, CollectionInCollectionChildFail)
{
    dmLogSetlevel(DM_LOG_SEVERITY_FATAL);
    for (int i = 0; i < 10; ++i)
    {
        // NOTE: Coll is local and not collection in GameObjectTest
        dmGameObject::HCollection coll;
        dmResource::FactoryResult r = dmResource::Get(m_Factory, "root2.collectionc", (void**) &coll);
        ASSERT_NE(dmResource::FACTORY_RESULT_OK, r);
    }
    dmLogSetlevel(DM_LOG_SEVERITY_WARNING);
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
    dmGameObject::RegisterDDFType(TestGameObject::Spawn::m_DDFDescriptor);

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
    dmGameObject::HRegister m_Register = dmGameObject::NewRegister(0, 0);
    dmGameObject::RegisterResourceTypes(factory, m_Register);
    dmGameObject::RegisterComponentTypes(factory, m_Register);
    dmGameObject::HCollection collection = dmGameObject::NewCollection(factory, m_Register, 1024);

    uint32_t type;
    dmResource::FactoryResult e = dmResource::GetTypeFromExtension(factory, "scriptc", &type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

    const char* script_file_name = "__test__.scriptc";
    char script_path[512];
    DM_SNPRINTF(script_path, sizeof(script_path), "%s/%s", tmp_dir, script_file_name);

    char go_file_name[512];
    DM_SNPRINTF(go_file_name, sizeof(go_file_name), "%s/%s", tmp_dir, "__go__.goc");

    dmGameObjectDDF::PrototypeDesc prototype;
    //memset(&prototype, 0, sizeof(prototype));
    prototype.m_Components.m_Count = 1;
    dmGameObjectDDF::ComponentDesc component_desc;
    memset(&component_desc, 0, sizeof(component_desc));
    component_desc.m_Resource = script_file_name;
    component_desc.m_Position = Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f);
    component_desc.m_Rotation = Vectormath::Aos::Quat(0.0f, 0.0f, 0.0f, 1.0f);
    prototype.m_Components.m_Data = &component_desc;

    dmDDF::Result ddf_r = dmDDF::SaveMessageToFile(&prototype, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, go_file_name);
    ASSERT_EQ(dmDDF::RESULT_OK, ddf_r);

    CreateFile(script_path,
               "function update(self)\n"
               "    go.set_position(self, vmath.vector3(1,2,3))\n"
               "end\n");

    dmGameObject::HInstance go;
    go = dmGameObject::New(collection, "__go__.goc");
    ASSERT_NE((dmGameObject::HInstance) 0, go);

    dmGameObject::Update(&collection, 0, 1);
    Point3 p1 = dmGameObject::GetPosition(go);
    ASSERT_EQ(1, p1.getX());
    ASSERT_EQ(2, p1.getY());
    ASSERT_EQ(3, p1.getZ());

    dmTime::Sleep(1000000); // TODO: Currently seconds time resolution in modification time

    CreateFile(script_path,
               "function update(self)\n"
               "    go.set_position(self, vmath.vector3(10,20,30))\n"
               "end\n");


    dmResource::FactoryResult fr = dmResource::ReloadType(factory, type);
    ASSERT_EQ(dmResource::FACTORY_RESULT_OK, fr);

    dmGameObject::Update(&collection, 0, 1);
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
    dmResource::DeleteFactory(factory);
    dmGameObject::DeleteRegister(m_Register);
    dmGameObject::Finalize();
}

TEST_F(GameObjectTest, TestHierarchy1)
{
    for (int i = 0; i < 2; ++i)
    {
        dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
        dmGameObject::HInstance child = dmGameObject::New(m_Collection, "goproto01.goc");

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
        ret = dmGameObject::Update(&m_Collection, 0, 1);
        ASSERT_TRUE(ret);
        ret = dmGameObject::PostUpdate(&m_Collection, 1);
        ASSERT_TRUE(ret);

        Point3 expected_child_pos = Point3((parent_m * child_pos).getXYZ());

        ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(parent) - parent_pos), 0.001f);
        ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child) - expected_child_pos), 0.001f);

        if (i % 2 == 0)
        {
            dmGameObject::Delete(m_Collection, parent);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
            ASSERT_EQ(0U, dmGameObject::GetDepth(child));
            ASSERT_EQ(0U, dmGameObject::GetChildCount(child));
            dmGameObject::Delete(m_Collection, child);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
        }
        else
        {
            dmGameObject::Delete(m_Collection, child);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(0U, dmGameObject::GetChildCount(parent));
            dmGameObject::Delete(m_Collection, parent);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
        }
    }
}

TEST_F(GameObjectTest, TestHierarchy2)
{
    // Test transform
    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child_child = dmGameObject::New(m_Collection, "goproto01.goc");

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
    ret = dmGameObject::Update(&m_Collection, 0, 1);
    ASSERT_TRUE(ret);

    Point3 expected_child_pos = Point3((parent_m * child_pos).getXYZ());
    Point3 expected_child_child_pos = Point3(((parent_m * child_m) * child_child_pos).getXYZ());
    Point3 expected_child_child_pos2 = Point3(((parent_m * child_m) * child_child_m).getCol3().getXYZ());

    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(parent) - parent_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child) - expected_child_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child_child) - expected_child_child_pos), 0.001f);
    ASSERT_NEAR(0.0f, length(dmGameObject::GetWorldPosition(child_child) - expected_child_child_pos2), 0.001f);

    dmGameObject::Delete(m_Collection, parent);
    dmGameObject::Delete(m_Collection, child);
    dmGameObject::Delete(m_Collection, child_child);
}

TEST_F(GameObjectTest, TestHierarchy3)
{
    // Test with siblings
    for (int i = 0; i < 3; ++i)
    {
        dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
        dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "goproto01.goc");
        dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "goproto01.goc");

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
        ret = dmGameObject::Update(&m_Collection, 0, 1);
        ASSERT_TRUE(ret);
        ret = dmGameObject::PostUpdate(&m_Collection, 1);
        ASSERT_TRUE(ret);

        // Test all possible delete orders in this configuration
        if (i == 0)
        {
            dmGameObject::Delete(m_Collection, parent);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child1));
            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, child1);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, child2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
        }
        else if (i == 1)
        {
            dmGameObject::Delete(m_Collection, child1);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(1U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, parent);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child2));

            dmGameObject::Delete(m_Collection, child2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
        }
        else if (i == 2)
        {
            dmGameObject::Delete(m_Collection, child2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(1U, dmGameObject::GetChildCount(parent));
            ASSERT_EQ(0U, dmGameObject::GetDepth(parent));
            ASSERT_EQ(1U, dmGameObject::GetDepth(child1));

            dmGameObject::Delete(m_Collection, parent);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(child1));

            dmGameObject::Delete(m_Collection, child1);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
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

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child3 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child4 = dmGameObject::New(m_Collection, "goproto01.goc");

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

    dmGameObject::Delete(m_Collection, parent);
    dmGameObject::Delete(m_Collection, child1);
    dmGameObject::Delete(m_Collection, child2);
    dmGameObject::Delete(m_Collection, child3);
    dmGameObject::Delete(m_Collection, child4);
}

TEST_F(GameObjectTest, TestHierarchy5)
{
    // Test parent subtree

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child3 = dmGameObject::New(m_Collection, "goproto01.goc");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child3, child2);

    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));
    ASSERT_EQ(child2, dmGameObject::GetParent(child3));

    dmGameObject::Delete(m_Collection, parent);
    dmGameObject::Delete(m_Collection, child1);
    dmGameObject::Delete(m_Collection, child2);
    dmGameObject::Delete(m_Collection, child3);
}

TEST_F(GameObjectTest, TestHierarchy6)
{
    // Test invalid reparent.
    // Test that the child node is not present in the upward trace from parent

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "goproto01.goc");

    // parent -> child1
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(child1, parent));

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));

    // child1 -> parent
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameObject::SetParent(parent, child1));

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));

    dmGameObject::Delete(m_Collection, parent);
    dmGameObject::Delete(m_Collection, child1);
}

TEST_F(GameObjectTest, TestHierarchy7)
{
    // Test remove interior node

    dmGameObject::HInstance parent = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child1 = dmGameObject::New(m_Collection, "goproto01.goc");
    dmGameObject::HInstance child2 = dmGameObject::New(m_Collection, "goproto01.goc");

    dmGameObject::SetParent(child1, parent);
    dmGameObject::SetParent(child2, child1);

    ASSERT_EQ(parent, dmGameObject::GetParent(child1));
    ASSERT_EQ(child1, dmGameObject::GetParent(child2));

    dmGameObject::Delete(m_Collection, child1);
    bool ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    ASSERT_EQ(parent, dmGameObject::GetParent(child2));
    ASSERT_TRUE(dmGameObject::IsChildOf(child2, parent));

    dmGameObject::Delete(m_Collection, parent);
    dmGameObject::Delete(m_Collection, child2);
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
        dmGameObject::HInstance a1 = dmGameObject::New(m_Collection, "goproto01.goc");
        dmGameObject::HInstance b2 = dmGameObject::New(m_Collection, "goproto01.goc");
        dmGameObject::HInstance c2 = dmGameObject::New(m_Collection, "goproto01.goc");
        dmGameObject::HInstance d3 = dmGameObject::New(m_Collection, "goproto01.goc");

        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(d3, b2));
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(b2, a1));
        ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(c2, a1));

        ASSERT_EQ(a1, dmGameObject::GetParent(b2));
        ASSERT_EQ(a1, dmGameObject::GetParent(c2));
        ASSERT_EQ(b2, dmGameObject::GetParent(d3));

        bool ret;
        ret = dmGameObject::Update(&m_Collection, 0, 1);
        ASSERT_TRUE(ret);
        ret = dmGameObject::PostUpdate(&m_Collection, 1);
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
            dmGameObject::Delete(m_Collection, a1);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(c2));
            dmGameObject::Delete(m_Collection, c2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(b2));
            dmGameObject::Delete(m_Collection, b2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, d3);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
        }
        else
        {
            ASSERT_EQ(0U, dmGameObject::GetDepth(a1));
            ASSERT_EQ(3U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, a1);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(1U, dmGameObject::GetDepth(b2));
            ASSERT_EQ(2U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, b2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
            ASSERT_EQ(c2, dmGameObject::GetParent(d3));
            ASSERT_TRUE(dmGameObject::IsChildOf(d3, c2));

            ASSERT_EQ(0U, dmGameObject::GetDepth(c2));
            ASSERT_EQ(1U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, c2);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);

            ASSERT_EQ(0U, dmGameObject::GetDepth(d3));
            dmGameObject::Delete(m_Collection, d3);
            ret = dmGameObject::PostUpdate(&m_Collection, 1);
            ASSERT_TRUE(ret);
        }
    }
}

TEST_F(GameObjectTest, TestIsVisible)
{
    dmGameObject::UpdateContext m_UpdateContext;
    m_UpdateContext.m_DT = 1.0f / 60.0f;
    m_UpdateContext.m_ViewProj = Vectormath::Aos::Matrix4::identity();
    dmGameObject::HInstance is_visible = dmGameObject::New(m_Collection, "is_visible.goc");
    ASSERT_NE((void*)0, (void*)is_visible);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));
}

TEST_F(GameObjectTest, TestScriptDelete)
{
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HInstance is_visible = dmGameObject::New(m_Collection, "delete.goc");
    ASSERT_NE((void*)0, (void*)is_visible);
    ASSERT_NE(0, m_Collection->m_InstanceIndices.Size());
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));
    ASSERT_EQ(0, m_Collection->m_InstanceIndices.Size());
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
