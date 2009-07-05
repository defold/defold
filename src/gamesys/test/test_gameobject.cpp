#include <gtest/gtest.h>

#include <map>
#include <dlib/hash.h>
#include "../resource.h"
#include "../gameobject.h"
#include "gamesys/test/test_resource_ddf.h"

/*
 * TODO:
 * - Add scripting tests!
 */

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        factory = Resource::NewFactory(16, "build/default/src/gamesys/test");
        collection = GameObject::NewCollection();
        GameObject::RegisterResourceTypes(factory);

        // Register dummy physical resource type
        Resource::FactoryError e;
        e = Resource::RegisterType(factory, "pc", this, PhysCreate, PhysDestroy);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        e = Resource::RegisterType(factory, "a", this, ACreate, ADestroy);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        e = Resource::RegisterType(factory, "b", this, BCreate, BDestroy);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        e = Resource::RegisterType(factory, "c", this, CCreate, CDestroy);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);

        uint32_t resource_type;

        e = Resource::GetTypeFromExtension(factory, "pc", &resource_type);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        GameObject::Result result = GameObject::RegisterComponentType(collection, resource_type, this, PhysComponentCreate, PhysComponentDestroy, PhysComponentsUpdate, false);
        ASSERT_EQ(GameObject::RESULT_OK, result);

        // A has component_user_data
        e = Resource::GetTypeFromExtension(factory, "a", &resource_type);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        result = GameObject::RegisterComponentType(collection, resource_type, this, AComponentCreate, AComponentDestroy, AComponentsUpdate, true);
        ASSERT_EQ(GameObject::RESULT_OK, result);

        // B has *not* component_user_data
        e = Resource::GetTypeFromExtension(factory, "b", &resource_type);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        result = GameObject::RegisterComponentType(collection, resource_type, this, BComponentCreate, BComponentDestroy, BComponentsUpdate, false);
        ASSERT_EQ(GameObject::RESULT_OK, result);

        // C has component_user_data
        e = Resource::GetTypeFromExtension(factory, "c", &resource_type);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
        result = GameObject::RegisterComponentType(collection, resource_type, this, CComponentCreate, CComponentDestroy, CComponentsUpdate, true);
        ASSERT_EQ(GameObject::RESULT_OK, result);

        m_MaxComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash] = 1000000;
    }

    virtual void TearDown()
    {
        GameObject::DeleteCollection(collection, factory);
        Resource::DeleteFactory(factory);
    }

    static Resource::FResourceCreate    PhysCreate;
    static Resource::FResourceDestroy   PhysDestroy;
    static GameObject::ComponentCreate  PhysComponentCreate;
    static GameObject::ComponentDestroy PhysComponentDestroy;
    static GameObject::ComponentsUpdate PhysComponentsUpdate;

    static Resource::FResourceCreate    ACreate;
    static Resource::FResourceDestroy   ADestroy;
    static GameObject::ComponentCreate  AComponentCreate;
    static GameObject::ComponentDestroy AComponentDestroy;
    static GameObject::ComponentsUpdate AComponentsUpdate;

    static Resource::FResourceCreate    BCreate;
    static Resource::FResourceDestroy   BDestroy;
    static GameObject::ComponentCreate  BComponentCreate;
    static GameObject::ComponentDestroy BComponentDestroy;
    static GameObject::ComponentsUpdate BComponentsUpdate;

    static Resource::FResourceCreate    CCreate;
    static Resource::FResourceDestroy   CDestroy;
    static GameObject::ComponentCreate  CComponentCreate;
    static GameObject::ComponentDestroy CComponentDestroy;
    static GameObject::ComponentsUpdate CComponentsUpdate;

public:

    std::map<uint64_t, uint32_t> m_CreateCountMap;
    std::map<uint64_t, uint32_t> m_DestroyCountMap;

    std::map<uint64_t, uint32_t> m_ComponentCreateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentDestroyCountMap;
    std::map<uint64_t, uint32_t> m_ComponentUpdateCountMap;
    std::map<uint64_t, uint32_t> m_MaxComponentCreateCountMap;

    std::map<uint64_t, int>      m_ComponentUserDataAcc;

    GameObject::HCollection collection;
    Resource::HFactory factory;
};

template <typename T>
Resource::CreateError GenericDDFCreate(Resource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, Resource::SResourceDescriptor* resource)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_CreateCountMap[T::m_DDFHash]++;

    T* obj;
    DDFError e = DDFLoadMessage<T>(buffer, buffer_size, &obj);
    if (e == DDF_ERROR_OK)
    {
        resource->m_Resource = (void*) obj;
        return Resource::CREATE_RESULT_OK;
    }
    else
    {
        return Resource::CREATE_RESULT_UNKNOWN;
    }
}

template <typename T>
Resource::CreateError GenericDDFDestory(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_DestroyCountMap[T::m_DDFHash]++;

    DDFFreeMessage((void*) resource->m_Resource);
    return Resource::CREATE_RESULT_OK;
}

template <typename T, int add_to_user_data>
static GameObject::CreateResult GenericComponentCreate(GameObject::HCollection collection,
                                                       GameObject::HInstance instance,
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
            return GameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    game_object_test->m_ComponentCreateCountMap[T::m_DDFHash]++;
    return GameObject::CREATE_RESULT_OK;
}

template <typename T>
static void GenericComponentsUpdate(GameObject::HCollection collection,
                                    const GameObject::UpdateContext* update_context,
                                    void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
}


template <typename T>
static GameObject::CreateResult GenericComponentDestroy(GameObject::HCollection collection,
                                                       GameObject::HInstance instance,
                                                       void* context,
                                                       uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    if (user_data)
    {
        game_object_test->m_ComponentUserDataAcc[T::m_DDFHash] += *user_data;
    }

    game_object_test->m_ComponentDestroyCountMap[T::m_DDFHash]++;
    return GameObject::CREATE_RESULT_OK;
}

Resource::FResourceCreate GameObjectTest::PhysCreate              = GenericDDFCreate<TestResource::PhysComponent>;
Resource::FResourceDestroy GameObjectTest::PhysDestroy            = GenericDDFDestory<TestResource::PhysComponent>;
GameObject::ComponentCreate GameObjectTest::PhysComponentCreate   = GenericComponentCreate<TestResource::PhysComponent,-1>;
GameObject::ComponentDestroy GameObjectTest::PhysComponentDestroy = GenericComponentDestroy<TestResource::PhysComponent>;
GameObject::ComponentsUpdate GameObjectTest::PhysComponentsUpdate = GenericComponentsUpdate<TestResource::PhysComponent>;

Resource::FResourceCreate GameObjectTest::ACreate              = GenericDDFCreate<TestResource::AResource>;
Resource::FResourceDestroy GameObjectTest::ADestroy            = GenericDDFDestory<TestResource::AResource>;
GameObject::ComponentCreate GameObjectTest::AComponentCreate   = GenericComponentCreate<TestResource::AResource, 1>;
GameObject::ComponentDestroy GameObjectTest::AComponentDestroy = GenericComponentDestroy<TestResource::AResource>;
GameObject::ComponentsUpdate GameObjectTest::AComponentsUpdate = GenericComponentsUpdate<TestResource::AResource>;

Resource::FResourceCreate GameObjectTest::BCreate              = GenericDDFCreate<TestResource::BResource>;
Resource::FResourceDestroy GameObjectTest::BDestroy            = GenericDDFDestory<TestResource::BResource>;
GameObject::ComponentCreate GameObjectTest::BComponentCreate   = GenericComponentCreate<TestResource::BResource, -1>;
GameObject::ComponentDestroy GameObjectTest::BComponentDestroy = GenericComponentDestroy<TestResource::BResource>;
GameObject::ComponentsUpdate GameObjectTest::BComponentsUpdate = GenericComponentsUpdate<TestResource::BResource>;

Resource::FResourceCreate GameObjectTest::CCreate              = GenericDDFCreate<TestResource::CResource>;
Resource::FResourceDestroy GameObjectTest::CDestroy            = GenericDDFDestory<TestResource::CResource>;
GameObject::ComponentCreate GameObjectTest::CComponentCreate   = GenericComponentCreate<TestResource::CResource, 10>;
GameObject::ComponentDestroy GameObjectTest::CComponentDestroy = GenericComponentDestroy<TestResource::CResource>;
GameObject::ComponentsUpdate GameObjectTest::CComponentsUpdate = GenericComponentsUpdate<TestResource::CResource>;

TEST_F(GameObjectTest, Test01)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto01.go");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = GameObject::Update(collection, go);
    ASSERT_TRUE(ret);
    ret = GameObject::Update(collection, go);
    ASSERT_TRUE(ret);
    ret = GameObject::Update(collection, go);
    ASSERT_TRUE(ret);
    GameObject::Delete(collection, factory, go);

    ASSERT_EQ(0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestUpdate)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto02.go");
    ASSERT_NE((void*) 0, (void*) go);
    GameObject::UpdateContext update_context;
    update_context.m_DT = 1.0f / 60.0f;
    GameObject::Update(collection, &update_context);
    ASSERT_EQ(1, m_ComponentUpdateCountMap[TestResource::PhysComponent::m_DDFHash]);

    GameObject::Delete(collection, factory, go);
    ASSERT_EQ(1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(1, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(1, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestNonexistingComponent)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto03.go");
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ(0, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(0, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);

    ASSERT_EQ(0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialNonexistingComponent1)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto04.go");
    ASSERT_EQ((void*) 0, (void*) go);

    // First one exists
    ASSERT_EQ(1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
    // Even though the first physcomponent exits the prototype creation should fail before creating components
    ASSERT_EQ(0, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(0, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash] = 1;
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto05.go");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_EQ(1, m_CreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(1, m_DestroyCountMap[TestResource::PhysComponent::m_DDFHash]);

    // One component should get created
    ASSERT_EQ(1, m_ComponentCreateCountMap[TestResource::PhysComponent::m_DDFHash]);
    ASSERT_EQ(1, m_ComponentDestroyCountMap[TestResource::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestComponentUserdata)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto06.go");
    ASSERT_NE((void*) 0, (void*) go);

    GameObject::Delete(collection, factory, go);
    // Two a:s
    ASSERT_EQ(2, m_ComponentUserDataAcc[TestResource::AResource::m_DDFHash]);
    // Zero c:s
    ASSERT_EQ(0, m_ComponentUserDataAcc[TestResource::BResource::m_DDFHash]);
    // Three c:s
    ASSERT_EQ(30, m_ComponentUserDataAcc[TestResource::CResource::m_DDFHash]);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    GameObject::Initialize();
    int ret = RUN_ALL_TESTS();
    GameObject::Finalize();
}
