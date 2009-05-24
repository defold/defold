#include <gtest/gtest.h>

#include <dlib/hash.h>
#include "../resource.h"
#include "../gameobject.h"
#include "gamesys/test/test_resource_ddf.h"

Resource::CreateError PhysCreate(Resource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, Resource::SResourceDescriptor* resource)
{
    TestResource::PhysComponent* phys_comp;
    DDFError e = DDFLoadMessage(buffer, buffer_size, &TestResource_PhysComponent_DESCRIPTOR, (void**) &phys_comp);
    if (e == DDF_ERROR_OK)
    {
        resource->m_Resource = (void*) phys_comp;
        return Resource::CREATE_ERROR_OK;
    }
    else
    {
        return Resource::CREATE_ERROR_UNKNOWN;
    }
}

Resource::CreateError PhysDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    DDFFreeMessage((void*) resource->m_Resource);
    return Resource::CREATE_ERROR_OK;
}

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
        e = Resource::RegisterType(factory, "pc", 0, &PhysCreate, &PhysDestroy);
        ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
        uint32_t resource_type;
        e = Resource::GetTypeFromExtension(factory, "pc", &resource_type);
        ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
        GameObject::Result result = GameObject::RegisterComponentType(collection, resource_type, this, &PhysComponentCreate, &PhysComponentDestroy);
        ASSERT_EQ(GameObject::RESULT_OK, result);

        m_PhysComponentCreateCallCount = 0;
        m_PhysComponentDestroyCallCount = 0;
        m_MaxPhysComponentCreateCallCount = 1000000;
    }

    virtual void TearDown()
    {
        GameObject::DeleteCollection(collection, factory);
        Resource::DeleteFactory(factory);
    }

    static GameObject::CreateResult PhysComponentCreate(GameObject::HCollection collection,
                                                        uint32_t instance_id,
                                                        void* resource,
                                                        void* context)
    {
        GameObjectTest* game_object_test = (GameObjectTest*) context;
        if (game_object_test->m_PhysComponentCreateCallCount >= game_object_test->m_MaxPhysComponentCreateCallCount)
        {
            return GameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        else
        {
            game_object_test->m_PhysComponentCreateCallCount++;
            return GameObject::CREATE_RESULT_OK;
        }
    }

    static GameObject::CreateResult PhysComponentDestroy(GameObject::HCollection collection,
                                                         uint32_t instance_id,
                                                         void* context)
    {
        GameObjectTest* game_object_test = (GameObjectTest*) context;
        game_object_test->m_PhysComponentDestroyCallCount++;
        return GameObject::CREATE_RESULT_OK;
    }


    uint32_t m_MaxPhysComponentCreateCallCount;
    uint32_t m_PhysComponentCreateCallCount;
    uint32_t m_PhysComponentDestroyCallCount;
    GameObject::HCollection collection;
    Resource::HFactory factory;
};

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

    ASSERT_EQ(0, m_PhysComponentCreateCallCount);
    ASSERT_EQ(0, m_PhysComponentDestroyCallCount);
}

TEST_F(GameObjectTest, Test02)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto02.go");
    ASSERT_NE((void*) 0, (void*) go);
    GameObject::Delete(collection, factory, go);
    ASSERT_EQ(1, m_PhysComponentCreateCallCount);
    ASSERT_EQ(1, m_PhysComponentDestroyCallCount);
}

TEST_F(GameObjectTest, TestNonexistingComponent)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto03.go");
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ(0, m_PhysComponentCreateCallCount);
    ASSERT_EQ(0, m_PhysComponentDestroyCallCount);
}

TEST_F(GameObjectTest, TestPartialNonexistingComponent1)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto04.go");
    ASSERT_EQ((void*) 0, (void*) go);

    // Even though the first physcomponent exits the prototype creation should fail before creating components
    ASSERT_EQ(0, m_PhysComponentCreateCallCount);
    ASSERT_EQ(0, m_PhysComponentDestroyCallCount);
}

TEST_F(GameObjectTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxPhysComponentCreateCallCount = 1;
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto05.go");
    ASSERT_EQ((void*) 0, (void*) go);

    // One component should get created
    ASSERT_EQ(1, m_PhysComponentCreateCallCount);
    ASSERT_EQ(1, m_PhysComponentDestroyCallCount);
}

TEST_F(GameObjectTest, TestPhysComp)
{
    GameObject::HInstance go = GameObject::New(collection, factory, "goproto02.go");
    ASSERT_NE((void*) 0, (void*) go);
    GameObject::Delete(collection, factory, go);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    GameObject::Initialize();
    int ret = RUN_ALL_TESTS();
    GameObject::Finalize();
}
