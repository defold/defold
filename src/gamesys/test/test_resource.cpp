#include <gtest/gtest.h>

#include <dlib/hash.h>
#include "gamesys/resource.h"
#include "gamesys/test/test_resource_ddf.h"

class ResourceTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        factory = Resource::NewFactory(16, ".");
    }

    virtual void TearDown()
    {
        Resource::DeleteFactory(factory);
    }

    Resource::HFactory factory;
};

Resource::CreateError DummyCreate(Resource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, Resource::SResourceDescriptor* resource)
{
    return Resource::CREATE_RESULT_OK;
}

Resource::CreateError DummyDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    return Resource::CREATE_RESULT_OK;
}

TEST_F(ResourceTest, RegisterType)
{
    Resource::FactoryError e;

    // Test create/destroy function == 0
    e = Resource::RegisterType(factory, "foo", 0, 0, 0);
    ASSERT_EQ(Resource::FACTORY_RESULT_INVAL, e);

    // Test dot in extension
    e = Resource::RegisterType(factory, ".foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_RESULT_INVAL, e);

    // Test "ok"
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);

    // Test already registred
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_RESULT_ALREADY_REGISTERED, e);
}

TEST_F(ResourceTest, NotFound)
{
    Resource::FactoryError e;
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);

    void* resource = (void*) 0xdeadbeef;
    e = Resource::Get(factory, "DOES_NOT_EXISTS.foo", &resource);
    ASSERT_EQ(Resource::FACTORY_RESULT_RESOURCE_NOT_FOUND, e);
    ASSERT_EQ((void*) 0, resource);
}

TEST_F(ResourceTest, UnknwonResourceType)
{
    Resource::FactoryError e;

    void* resource = (void*) 0;
    e = Resource::Get(factory, "build/default/src/gamesys/test/test.testresourcecont", &resource);
    ASSERT_EQ(Resource::FACTORY_RESULT_UNKNOWN_RESOURCE_TYPE, e);
    ASSERT_EQ((void*) 0, resource);
}

// Loaded version (in-game) of ResourceContainerDesc
struct TestResourceContainer
{
    uint64_t                                m_NameHash;
    std::vector<TestResource::ResourceFoo*> m_Resources;
};

Resource::CreateError ResourceContainerCreate(Resource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              Resource::SResourceDescriptor* resource);

Resource::CreateError ResourceContainerDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource);

Resource::CreateError FooResourceCreate(Resource::HFactory factory,
                                        void* context,
                                        const void* buffer, uint32_t buffer_size,
                                        Resource::SResourceDescriptor* resource);

Resource::CreateError FooResourceDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource);

class GetResourceTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_ResourceContainerCreateCallCount = 0;
        m_ResourceContainerDestroyCallCount = 0;
        m_FooResourceCreateCallCount = 0;
        m_FooResourceDestroyCallCount = 0;

        m_Factory = Resource::NewFactory(16, "build/default/src/gamesys/test/");
        m_ResourceName = "test.cont";

        Resource::FactoryError e;
        e = Resource::RegisterType(m_Factory, "cont", this, &ResourceContainerCreate, &ResourceContainerDestroy);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);

        e = Resource::RegisterType(m_Factory, "foo", this, &FooResourceCreate, &FooResourceDestroy);
        ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    }

    virtual void TearDown()
    {
        Resource::DeleteFactory(m_Factory);
    }

public:
    uint32_t           m_ResourceContainerCreateCallCount;
    uint32_t           m_ResourceContainerDestroyCallCount;
    uint32_t           m_FooResourceCreateCallCount;
    uint32_t           m_FooResourceDestroyCallCount;

    Resource::HFactory m_Factory;
    const char*        m_ResourceName;
};

Resource::CreateError ResourceContainerCreate(Resource::HFactory factory,
                                         void* context,
                                         const void* buffer, uint32_t buffer_size,
                                         Resource::SResourceDescriptor* resource)
{
    GetResourceTest* self = (GetResourceTest*) context;
    self->m_ResourceContainerCreateCallCount++;

    TestResource::ResourceContainerDesc* resource_container_desc;
    dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &TestResource_ResourceContainerDesc_DESCRIPTOR, (void**) &resource_container_desc);
    if (e == dmDDF::RESULT_OK)
    {
        TestResourceContainer* resource_cont = new TestResourceContainer();
        resource_cont->m_NameHash = dmHashBuffer64(resource_container_desc->m_Name, strlen(resource_container_desc->m_Name));
        resource->m_Resource = (void*) resource_cont;

        for (uint32_t i = 0; i < resource_container_desc->m_Resources.m_Count; ++i)
        {
            TestResource::ResourceFoo* sub_resource;
            Resource::FactoryError factoy_e = Resource::Get(factory, resource_container_desc->m_Resources[i], (void**)&sub_resource);
            assert( factoy_e == Resource::FACTORY_RESULT_OK );
            resource_cont->m_Resources.push_back(sub_resource);
        }
        dmDDF::FreeMessage(resource_container_desc);
        return Resource::CREATE_RESULT_OK;
    }
    else
    {
        return Resource::CREATE_RESULT_UNKNOWN;
    }
}

Resource::CreateError ResourceContainerDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    GetResourceTest* self = (GetResourceTest*) context;
    self->m_ResourceContainerDestroyCallCount++;

    TestResourceContainer* resource_cont = (TestResourceContainer*) resource->m_Resource;

    std::vector<TestResource::ResourceFoo*>::iterator i;
    for (i = resource_cont->m_Resources.begin(); i != resource_cont->m_Resources.end(); ++i)
    {
        Resource::Release(factory, *i);
    }
    delete resource_cont;
    return Resource::CREATE_RESULT_OK;
}

Resource::CreateError FooResourceCreate(Resource::HFactory factory,
                                        void* context,
                                        const void* buffer, uint32_t buffer_size,
                                        Resource::SResourceDescriptor* resource)
{
    GetResourceTest* self = (GetResourceTest*) context;
    self->m_FooResourceCreateCallCount++;

    TestResource::ResourceFoo* resource_foo;

    dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &TestResource_ResourceFoo_DESCRIPTOR, (void**) &resource_foo);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) resource_foo;
        resource->m_ResourceKind = Resource::KIND_DDF_DATA;
        return Resource::CREATE_RESULT_OK;
    }
    else
    {
        return Resource::CREATE_RESULT_UNKNOWN;
    }
}

Resource::CreateError FooResourceDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    GetResourceTest* self = (GetResourceTest*) context;
    self->m_FooResourceDestroyCallCount++;

    dmDDF::FreeMessage(resource->m_Resource);
    return Resource::CREATE_RESULT_OK;
}

TEST_F(GetResourceTest, GetTestResource)
{
    Resource::FactoryError e;

    TestResourceContainer* test_resource_cont = 0;
    e = Resource::Get(m_Factory, m_ResourceName, (void**) &test_resource_cont);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_NE((void*) 0, test_resource_cont);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(test_resource_cont->m_Resources.size(), m_FooResourceCreateCallCount);
    ASSERT_EQ(0, m_FooResourceDestroyCallCount);
    ASSERT_EQ(123, test_resource_cont->m_Resources[0]->m_x);
    ASSERT_EQ(456, test_resource_cont->m_Resources[1]->m_x);

    ASSERT_EQ(dmHashBuffer64("Testing", strlen("Testing")), test_resource_cont->m_NameHash);
    Resource::Release(m_Factory, test_resource_cont);
}

TEST_F(GetResourceTest, GetReference1)
{
    Resource::FactoryError e;

    Resource::SResourceDescriptor descriptor;
    e = Resource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(Resource::FACTORY_RESULT_NOT_LOADED, e);
}

TEST_F(GetResourceTest, GetReference2)
{
    Resource::FactoryError e;

    void* resource = (void*) 0;
    e = Resource::Get(m_Factory, m_ResourceName, &resource);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_NE((void*) 0, resource);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(0, m_ResourceContainerDestroyCallCount);

    Resource::SResourceDescriptor descriptor;
    e = Resource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(0, m_ResourceContainerDestroyCallCount);

    ASSERT_EQ(1, descriptor.m_ReferenceCount);
    Resource::Release(m_Factory, resource);
}

TEST_F(GetResourceTest, ReferenceCountSimple)
{
    Resource::FactoryError e;

    TestResourceContainer* resource1 = 0;
    e = Resource::Get(m_Factory, m_ResourceName, (void**) &resource1);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    const uint32_t sub_resource_count = resource1->m_Resources.size();
    ASSERT_EQ(2, sub_resource_count); //NOTE: Hard coded for two resources in test.cont
    ASSERT_NE((void*) 0, resource1);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(0, m_FooResourceDestroyCallCount);

    Resource::SResourceDescriptor descriptor1;
    e = Resource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_EQ(1, descriptor1.m_ReferenceCount);

    TestResourceContainer* resource2 = 0;
    e = Resource::Get(m_Factory, m_ResourceName, (void**) &resource2);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_NE((void*) 0, resource2);
    ASSERT_EQ(resource1, resource2);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(0, m_FooResourceDestroyCallCount);

    Resource::SResourceDescriptor descriptor2;
    e = Resource::GetDescriptor(m_Factory, m_ResourceName, &descriptor2);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_EQ(2, descriptor2.m_ReferenceCount);

    // Release
    Resource::Release(m_Factory, resource1);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(0, m_FooResourceDestroyCallCount);

    // Check reference count equal to 1
    e = Resource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(Resource::FACTORY_RESULT_OK, e);
    ASSERT_EQ(1, descriptor1.m_ReferenceCount);

    // Release again
    Resource::Release(m_Factory, resource2);
    ASSERT_EQ(1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ(1, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceDestroyCallCount);

    // Make sure resource gets unloaded
    e = Resource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(Resource::FACTORY_RESULT_NOT_LOADED, e);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

