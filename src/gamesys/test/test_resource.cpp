#include <gtest/gtest.h>

#include <dlib/hash.h>
#include "gamesys/resource.h"
#include "gamesys/test/test_resource_ddf.h"

Resource::CreateError DummyCreate(Resource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, Resource::SResourceDescriptor* resource)
{
    return Resource::CREATE_ERROR_OK;
}

Resource::CreateError DummyDestroy(Resource::HFactory factory, void* context, Resource::SResourceDescriptor* resource)
{
    return Resource::CREATE_ERROR_OK;
}

TEST(Resource, RegisterType)
{
    Resource::HFactory factory = Resource::NewFactory(16, ".");

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

    Resource::DeleteFactory(factory);
}

TEST(Resource, NotFound)
{
    Resource::HFactory factory = Resource::NewFactory(16, ".");

    Resource::FactoryError e;
    e = Resource::RegisterType(factory, "foo", 0, &DummyCreate, &DummyDestroy);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);

    void* resource = (void*) 0xdeadbeef;
    e = Resource::Get(factory, "DOES_NOT_EXISTS.foo", &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_RESOURCE_NOT_FOUND, e);
    ASSERT_EQ((void*) 0, resource);

    Resource::DeleteFactory(factory);
}

TEST(Resource, UnknwonResourceType)
{
    Resource::HFactory factory = Resource::NewFactory(16, ".");

    Resource::FactoryError e;

    void* resource = (void*) 0;
    e = Resource::Get(factory, "build/default/src/gamesys/test/test.testresourcecont", &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_UNKNOWN_RESOURCE_TYPE, e);
    ASSERT_EQ((void*) 0, resource);

    Resource::DeleteFactory(factory);
}

// Loaded version (in-game) of ResourceContainerDesc
struct TestResourceContainer
{
    uint64_t                         m_NameHash;
    //std::vector<Resource::SResource> m_Resources;
};

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
    return Resource::CREATE_ERROR_OK;
}

TEST(Resource, GetTestResource)
{
    Resource::HFactory factory = Resource::NewFactory(16, ".");
    Resource::FactoryError e;

    e = Resource::RegisterType(factory, "testresourcecont", 0, &TestResourceCreate, &TestResourceDestroy);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);

    void* resource = (void*) 0;
    e = Resource::Get(factory, "build/default/src/gamesys/test/test.testresourcecont", &resource);
    ASSERT_EQ(Resource::FACTORY_ERROR_OK, e);
    ASSERT_NE((void*) 0, resource);

    TestResourceContainer* test_resource_cont = (TestResourceContainer*) resource;
    ASSERT_EQ(HashBuffer64("Testing", strlen("Testing")), test_resource_cont->m_NameHash);

    Resource::DeleteFactory(factory);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

