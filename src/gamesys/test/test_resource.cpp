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
    return Resource::CREATE_ERROR_OK;
}

Resource::CreateError DummyDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    return Resource::CREATE_ERROR_OK;
}

TEST_F(ResourceTest, RegisterType)
{
    Resource::FactoryError e;

    // Test create/destroy function == 0
    e = Resource::RegisterType(factory, "foo", 0, 0, 0);
    ASSERT_EQ(Resource::FACTORY_ERROR_INVAL, e);

    // Test dot in extension
    e = Resource::RegisterType(factory, ".foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_ERROR_INVAL, e);

    // Test "ok"
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);

    // Test already registred
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_ERROR_ALREADY_REGISTRED, e);
}

TEST_F(ResourceTest, NotFound)
{
    Resource::FactoryError e;
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);

    void* resource = (void*) 0xdeadbeef;
    e = Resource::Get(factory, "DOES_NOT_EXISTS.foo", &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_RESOURCE_NOT_FOUND, e);
    ASSERT_EQ((void*) 0, resource);
}

TEST_F(ResourceTest, UnknwonResourceType)
{
    Resource::FactoryError e;

    void* resource = (void*) 0;
    e = Resource::Get(factory, "build/default/src/gamesys/test/test.testresourcecont", &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_UNKNOWN_RESOURCE_TYPE, e);
    ASSERT_EQ((void*) 0, resource);
}

// Loaded version (in-game) of ResourceContainerDesc
struct TestResourceContainer
{
    uint64_t                         m_NameHash;
    //std::vector<Resource::SResource> m_Resources;
};

// TODO: Add Create/Destroy call count and check...
Resource::CreateError TestResourceCreate(Resource::HFactory factory,
                                         void* context,
                                         const void* buffer, uint32_t buffer_size,
                                         Resource::SResourceDescriptor* resource)
{
    TestResource::ResourceContainerDesc* resource_container_desc;
    DDFError e = DDFLoadMessage(buffer, buffer_size, &TestResource_ResourceContainerDesc_DESCRIPTOR, (void**) &resource_container_desc);
    if (e == DDF_ERROR_OK)
    {
        TestResourceContainer* resource_cont = new TestResourceContainer();
        resource_cont->m_NameHash = HashBuffer64(resource_container_desc->m_Name, strlen(resource_container_desc->m_Name));
        resource->m_Resource = (void*) resource_cont;
        return Resource::CREATE_ERROR_OK;
    }
    else
    {
        return Resource::CREATE_ERROR_UNKNOWN;
    }
}

Resource::CreateError TestResourceDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    DDFFreeMessage(resource->m_Resource);
    return Resource::CREATE_ERROR_OK;
}

class GetResourceTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        factory = Resource::NewFactory(16, ".");
        resource_name = "build/default/src/gamesys/test/test.testresourcecont";

        Resource::FactoryError e;
        e = Resource::RegisterType(factory, "testresourcecont", 0, &TestResourceCreate, &TestResourceDestroy);
        ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    }

    virtual void TearDown()
    {
        Resource::DeleteFactory(factory);
    }

    Resource::HFactory factory;
    const char* resource_name;
};


TEST_F(GetResourceTest, GetTestResource)
{
    Resource::FactoryError e;

    void* resource = (void*) 0;
    e = Resource::Get(factory, resource_name, &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_NE((void*) 0, resource);

    TestResourceContainer* test_resource_cont = (TestResourceContainer*) resource;
    ASSERT_EQ(HashBuffer64("Testing", strlen("Testing")), test_resource_cont->m_NameHash);
}

TEST_F(GetResourceTest, GetReference1)
{
    Resource::FactoryError e;

    Resource::SResourceDescriptor descriptor;
    e = Resource::GetDescriptor(factory, resource_name, &descriptor);
    ASSERT_EQ(Resource::FACTORY_ERROR_NOT_LOADED, e);
}

TEST_F(GetResourceTest, GetReference2)
{
    Resource::FactoryError e;

    void* resource = (void*) 0;
    e = Resource::Get(factory, resource_name, &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_NE((void*) 0, resource);

    Resource::SResourceDescriptor descriptor;
    e = Resource::GetDescriptor(factory, resource_name, &descriptor);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);

    ASSERT_EQ(1, descriptor.m_ReferenceCount);
}

TEST_F(GetResourceTest, ReferenceCountSimple)
{
    Resource::FactoryError e;

    void* resource1 = (void*) 0;
    e = Resource::Get(factory, resource_name, &resource1);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_NE((void*) 0, resource1);

    Resource::SResourceDescriptor descriptor1;
    e = Resource::GetDescriptor(factory, resource_name, &descriptor1);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_EQ(1, descriptor1.m_ReferenceCount);

    void* resource2 = (void*) 0;
    e = Resource::Get(factory, resource_name, &resource2);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_NE((void*) 0, resource2);
    ASSERT_EQ(resource1, resource2);

    Resource::SResourceDescriptor descriptor2;
    e = Resource::GetDescriptor(factory, resource_name, &descriptor2);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_EQ(2, descriptor2.m_ReferenceCount);

    // Release
    Resource::Release(factory, resource1);

    // Check reference count equal to 1
    e = Resource::GetDescriptor(factory, resource_name, &descriptor1);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_EQ(1, descriptor1.m_ReferenceCount);

    // Release again
    Resource::Release(factory, resource2);

    // Make sure resource gets unloaded
    e = Resource::GetDescriptor(factory, resource_name, &descriptor1);
    ASSERT_EQ(Resource::FACTORY_ERROR_NOT_LOADED, e);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

