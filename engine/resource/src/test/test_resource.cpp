#include <gtest/gtest.h>

#include <dlib/buffer.h>
#include <dlib/socket.h>
#include <dlib/http_client.h>
#include <dlib/hash.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/message.h>
#include <dlib/thread.h>
#include <ddf/ddf.h>
#include "resource_ddf.h"
#include "../resource.h"
#include "../resource_private.h"
#include "test/test_resource_ddf.h"

extern unsigned char RESOURCES_ARCI[];
extern uint32_t RESOURCES_ARCI_SIZE;
extern unsigned char RESOURCES_ARCD[];
extern uint32_t RESOURCES_ARCD_SIZE;
extern unsigned char RESOURCES_DMANIFEST[];
extern uint32_t RESOURCES_DMANIFEST_SIZE;

#define EXT_CONSTANTS(prefix, ext)\
    static const char* prefix##_EXT = ext;\
    static const dmhash_t prefix##_EXT_HASH = dmHashString64(ext);\

    EXT_CONSTANTS(CONT, "cont")
    EXT_CONSTANTS(FOO, "foo")

#undef EXT_CONSTANTS

class ResourceTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        factory = dmResource::NewFactory(&params, ".");
        ASSERT_NE((void*) 0, factory);
    }

    virtual void TearDown()
    {
        if (factory != NULL)
        {
            dmResource::DeleteFactory(factory);
        }
    }

    dmResource::HFactory factory;
};

class DynamicResourceTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        const char* test_dir = "build/default/src/test";

        dmBuffer::NewContext();
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        factory = dmResource::NewFactory(&params, test_dir);
        ASSERT_NE((void*) 0, factory);
    }

    virtual void TearDown()
    {
        if (factory != NULL)
        {
            dmResource::DeleteFactory(factory);
        }
        dmBuffer::DeleteContext();
    }

    dmResource::HFactory factory;
};


dmResource::Result DummyCreate(const dmResource::ResourceCreateParams& params)
{
    return dmResource::RESULT_OK;
}

dmResource::Result DummyDestroy(const dmResource::ResourceDestroyParams& params)
{
    return dmResource::RESULT_OK;
}

TEST_F(ResourceTest, RegisterType)
{
    dmResource::Result e;

    // Test create/destroy function == 0
    e = dmResource::RegisterType(factory, "foo", 0, 0, 0, 0, 0, 0, 0);
    ASSERT_EQ(dmResource::RESULT_INVAL, e);

    // Test dot in extension
    e = dmResource::RegisterType(factory, ".foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0, 0);
    ASSERT_EQ(dmResource::RESULT_INVAL, e);

    // Test "ok"
    e = dmResource::RegisterType(factory, "foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    // Test already registred
    e = dmResource::RegisterType(factory, "foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0, 0);
    ASSERT_EQ(dmResource::RESULT_ALREADY_REGISTERED, e);

    // Test get type/extension from type/extension
    dmResource::ResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* ext;
    e = dmResource::GetExtensionFromType(factory, type, &ext);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_STREQ("foo", ext);

    e = dmResource::GetTypeFromExtension(factory, "noext", &type);
    ASSERT_EQ(dmResource::RESULT_UNKNOWN_RESOURCE_TYPE, e);
}

TEST_F(ResourceTest, NotFound)
{
    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    void* resource = (void*) 0xdeadbeef;
    e = dmResource::Get(factory, "/DOES_NOT_EXISTS.foo", &resource);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, e);
    ASSERT_EQ((void*) 0, resource);

    // Test empty string
    resource = (void*) 0xdeadbeef;
    e = dmResource::Get(factory, "", &resource);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, e);
    ASSERT_EQ((void*) 0, resource);
}

TEST_F(ResourceTest, UnknownResourceType)
{
    dmResource::Result e;

    void* resource = (void*) 0;
    e = dmResource::Get(factory, "/build/default/src/test/test.testresourcecont", &resource);
    ASSERT_EQ(dmResource::RESULT_UNKNOWN_RESOURCE_TYPE, e);
    ASSERT_EQ((void*) 0, resource);
}

// Loaded version (in-game) of ResourceContainerDesc
struct TestResourceContainer
{
    uint64_t                                m_NameHash;
    std::vector<TestResource::ResourceFoo*> m_Resources;
};

dmResource::Result ResourceContainerPreload(const dmResource::ResourcePreloadParams& params);

dmResource::Result ResourceContainerCreate(const dmResource::ResourceCreateParams& params);

dmResource::Result ResourceContainerDestroy(const dmResource::ResourceDestroyParams& params);

dmResource::Result FooResourceCreate(const dmResource::ResourceCreateParams& params);

dmResource::Result FooResourcePostCreate(const dmResource::ResourcePostCreateParams& params);

dmResource::Result FooResourceDestroy(const dmResource::ResourceDestroyParams& params);

class GetResourceTest : public ::testing::TestWithParam<const char*>
{
protected:
    virtual void SetUp()
    {
        m_ResourceContainerCreateCallCount = 0;
        m_ResourceContainerDestroyCallCount = 0;
        m_FooResourceCreateCallCount = 0;
        m_FooResourcePostCreateCallCount = 0;
        m_FooResourceDestroyCallCount = 0;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        m_Factory = dmResource::NewFactory(&params, GetParam());
        ASSERT_NE((void*) 0, m_Factory);
        m_ResourceName = "/test.cont";

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "cont", this, &ResourceContainerPreload, &ResourceContainerCreate, 0, &ResourceContainerDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        e = dmResource::RegisterType(m_Factory, "foo", this, 0, &FooResourceCreate, &FooResourcePostCreate, &FooResourceDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);
    }

    virtual void TearDown()
    {
        if (m_Factory != NULL)
        {
            dmResource::DeleteFactory(m_Factory);
        }
    }

    // dmResource::Get API but with preloader instead
    dmResource::Result PreloaderGet(dmResource::HFactory factory, const char *ref, void** resource)
    {
        // Unfortunately the assert macros won't work inside a function.
        dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, ref);
        dmResource::Result r;
        for (uint32_t i=0;i<33;i++)
        {
            r = dmResource::UpdatePreloader(pr, 0, 0, 30*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(30000);
        }

        if (r == dmResource::RESULT_OK)
        {
            r = dmResource::Get(factory, ref, resource);
        }
        else
        {
            *resource = 0x0;
        }

        dmResource::DeletePreloader(pr);
        return r;
    }

public:
    uint32_t           m_ResourceContainerCreateCallCount;
    uint32_t           m_ResourceContainerDestroyCallCount;
    uint32_t           m_FooResourceCreateCallCount;
    uint32_t           m_FooResourcePostCreateCallCount;
    uint32_t           m_FooResourceDestroyCallCount;

    dmResource::HFactory m_Factory;
    const char*        m_ResourceName;
};

dmResource::Result ResourceContainerPreload(const dmResource::ResourcePreloadParams& params)
{
    TestResource::ResourceContainerDesc* resource_container_desc;
    dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &TestResource_ResourceContainerDesc_DESCRIPTOR, (void**) &resource_container_desc);
    if (e != dmDDF::RESULT_OK)
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }

    for (uint32_t i = 0; i < resource_container_desc->m_Resources.m_Count; ++i)
    {
        dmResource::PreloadHint(params.m_HintInfo, resource_container_desc->m_Resources[i]);
    }

    *params.m_PreloadData = resource_container_desc;
    return dmResource::RESULT_OK;
}

dmResource::Result ResourceContainerCreate(const dmResource::ResourceCreateParams& params)
{
    GetResourceTest* self = (GetResourceTest*) params.m_Context;
    self->m_ResourceContainerCreateCallCount++;

    TestResource::ResourceContainerDesc* resource_container_desc = (TestResource::ResourceContainerDesc*) params.m_PreloadData;

    TestResourceContainer* resource_cont = new TestResourceContainer();
    resource_cont->m_NameHash = dmHashBuffer64(resource_container_desc->m_Name, strlen(resource_container_desc->m_Name));
    params.m_Resource->m_Resource = (void*) resource_cont;

    bool error = false;
    dmResource::Result factory_e = dmResource::RESULT_OK;
    for (uint32_t i = 0; i < resource_container_desc->m_Resources.m_Count; ++i)
    {
        TestResource::ResourceFoo* sub_resource;
        factory_e = dmResource::Get(params.m_Factory, resource_container_desc->m_Resources[i], (void**)&sub_resource);
        if (factory_e != dmResource::RESULT_OK)
        {
            error = true;
            break;
        }
        resource_cont->m_Resources.push_back(sub_resource);
    }

    dmDDF::FreeMessage(resource_container_desc);
    if (error)
    {
        for (uint32_t i = 0; i < resource_cont->m_Resources.size(); ++i)
        {
            dmResource::Release(params.m_Factory, resource_cont->m_Resources[i]);
        }
        delete resource_cont;
        params.m_Resource->m_Resource = 0;
        return factory_e;
    }
    else
    {
        return dmResource::RESULT_OK;
    }
}

dmResource::Result ResourceContainerDestroy(const dmResource::ResourceDestroyParams& params)
{
    GetResourceTest* self = (GetResourceTest*) params.m_Context;
    self->m_ResourceContainerDestroyCallCount++;

    TestResourceContainer* resource_cont = (TestResourceContainer*) params.m_Resource->m_Resource;

    std::vector<TestResource::ResourceFoo*>::iterator i;
    for (i = resource_cont->m_Resources.begin(); i != resource_cont->m_Resources.end(); ++i)
    {
        dmResource::Release(params.m_Factory, *i);
    }
    delete resource_cont;
    return dmResource::RESULT_OK;
}

dmResource::Result FooResourceCreate(const dmResource::ResourceCreateParams& params)
{
    GetResourceTest* self = (GetResourceTest*) params.m_Context;
    self->m_FooResourceCreateCallCount++;

    TestResource::ResourceFoo* resource_foo;

    dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &TestResource_ResourceFoo_DESCRIPTOR, (void**) &resource_foo);
    if (e == dmDDF::RESULT_OK)
    {
        params.m_Resource->m_Resource = (void*) resource_foo;
        params.m_Resource->m_ResourceKind = dmResource::KIND_DDF_DATA;
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmResource::Result FooResourcePostCreate(const dmResource::ResourcePostCreateParams& params)
{
    GetResourceTest* self = (GetResourceTest*) params.m_Context;
    self->m_FooResourcePostCreateCallCount++;
    return dmResource::RESULT_OK;
}

dmResource::Result FooResourceDestroy(const dmResource::ResourceDestroyParams& params)
{
    GetResourceTest* self = (GetResourceTest*) params.m_Context;
    self->m_FooResourceDestroyCallCount++;

    dmDDF::FreeMessage(params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

TEST_P(GetResourceTest, GetTestResource)
{
    dmResource::Result e;

    TestResourceContainer* test_resource_cont = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE((void*) 0, test_resource_cont);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(test_resource_cont->m_Resources.size(), m_FooResourceCreateCallCount);
    ASSERT_EQ(m_FooResourceCreateCallCount, m_FooResourcePostCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_FooResourceDestroyCallCount);
    ASSERT_EQ((uint32_t) 123, test_resource_cont->m_Resources[0]->m_X);
    ASSERT_EQ((uint32_t) 456, test_resource_cont->m_Resources[1]->m_X);

    ASSERT_EQ(dmHashBuffer64("Testing", strlen("Testing")), test_resource_cont->m_NameHash);
    dmResource::Release(m_Factory, test_resource_cont);

    // Add test for RESULT_RESOURCE_NOT_FOUND (for http test)
    e = dmResource::Get(m_Factory, "does_not_exists.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, e);
}

TEST_P(GetResourceTest, GetRaw)
{
    dmResource::Result e;

    void* resource = 0;
    uint32_t resource_size = 0;
    e = dmResource::GetRaw(m_Factory, "/test01.foo", (void**) &resource, &resource_size);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    // NOTE: Not pretty to hard-code the size here
    ASSERT_EQ(2U, resource_size);
    free(resource);

    e = dmResource::GetRaw(m_Factory, "does_not_exists", (void**) &resource, &resource_size);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, e);
}

TEST_P(GetResourceTest, IncRef)
{
    dmResource::Result e;

    TestResourceContainer* test_resource_cont = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &test_resource_cont);
    dmResource::IncRef(m_Factory, test_resource_cont);
    dmResource::Release(m_Factory, test_resource_cont);
    dmResource::Release(m_Factory, test_resource_cont);

    (void)e;
}

TEST_P(GetResourceTest, SelfReferring)
{
    dmResource::Result e;
    TestResourceContainer* test_resource_cont;

    test_resource_cont = 0;
    e = dmResource::Get(m_Factory, "/self_referring.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    test_resource_cont = 0;
    e = dmResource::Get(m_Factory, "/self_referring.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    test_resource_cont = 0;
    e = PreloaderGet(m_Factory, "/self_referring.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);
}

TEST_P(GetResourceTest, Loop)
{
    dmResource::Result e;
    TestResourceContainer* test_resource_cont;

    test_resource_cont = 0;
    e = dmResource::Get(m_Factory, "/root_loop.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    test_resource_cont = 0;
    e = dmResource::Get(m_Factory, "/root_loop.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);

    e = PreloaderGet(m_Factory, "/root_loop.cont", (void**) &test_resource_cont);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, e);
    ASSERT_EQ((void*) 0, test_resource_cont);
}

TEST_P(GetResourceTest, GetDescriptorWithExt)
{
    dmResource::Result e;

    void* resource = (void*) 0;
    e = dmResource::Get(m_Factory, m_ResourceName, &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE((void*) 0, resource);
    dmhash_t name_hash = dmHashString64(m_ResourceName);

    dmResource::SResourceDescriptor descriptor;
    // No exts means any ext
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, 0, 0, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, descriptor.m_NameHash);

    // One ext
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, &CONT_EXT_HASH, 1, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, descriptor.m_NameHash);

    // Two exts
    dmhash_t two_exts[] = {CONT_EXT_HASH, FOO_EXT_HASH};
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, two_exts, 2, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, descriptor.m_NameHash);

    // Two exts, reversed order
    dmhash_t two_exts_rev[] = {FOO_EXT_HASH, CONT_EXT_HASH};
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, two_exts_rev, 2, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, descriptor.m_NameHash);

    // No match
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, &FOO_EXT_HASH, 1, &descriptor);
    ASSERT_EQ(dmResource::RESULT_INVALID_FILE_EXTENSION, e);

    // No match, two exts
    dmhash_t two_exts_inv[] = {FOO_EXT_HASH, 0};
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, two_exts_inv, 2, &descriptor);
    ASSERT_EQ(dmResource::RESULT_INVALID_FILE_EXTENSION, e);

    dmResource::Release(m_Factory, resource);

    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, &CONT_EXT_HASH, 1, &descriptor);
    ASSERT_EQ(dmResource::RESULT_NOT_LOADED, e);
}

INSTANTIATE_TEST_CASE_P(GetResourceTestURI,
                        GetResourceTest,
                        ::testing::Values("build/default/src/test/", "http://127.0.0.1:6123", "dmanif:build/default/src/test/resources_pb.dmanifest"));

TEST_P(GetResourceTest, GetReference1)
{
    dmResource::Result e;

    dmResource::SResourceDescriptor descriptor;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_NOT_LOADED, e);
}

TEST_P(GetResourceTest, GetReference2)
{
    dmResource::Result e;

    void* resource = (void*) 0;
    e = dmResource::Get(m_Factory, m_ResourceName, &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE((void*) 0, resource);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);

    dmResource::SResourceDescriptor descriptor;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);

    ASSERT_EQ((uint32_t) 1, descriptor.m_ReferenceCount);
    dmResource::Release(m_Factory, resource);
}

TEST_P(GetResourceTest, ReferenceCountSimple)
{
    dmResource::Result e;

    TestResourceContainer* resource1 = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource1);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    const uint32_t sub_resource_count = resource1->m_Resources.size();
    ASSERT_EQ((uint32_t) 2, sub_resource_count); //NOTE: Hard coded for two resources in test.cont
    ASSERT_NE((void*) 0, resource1);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourcePostCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_FooResourceDestroyCallCount);

    dmResource::SResourceDescriptor descriptor1;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor1.m_ReferenceCount);

    TestResourceContainer* resource2 = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource2);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE((void*) 0, resource2);
    ASSERT_EQ(resource1, resource2);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourcePostCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_FooResourceDestroyCallCount);

    dmResource::SResourceDescriptor descriptor2;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor2);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor2.m_ReferenceCount);

    // Release
    dmResource::Release(m_Factory, resource1);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourcePostCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_FooResourceDestroyCallCount);

    // Check reference count equal to 1
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor1.m_ReferenceCount);

    // Release again
    dmResource::Release(m_Factory, resource2);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerDestroyCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceCreateCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourcePostCreateCallCount);
    ASSERT_EQ(sub_resource_count, m_FooResourceDestroyCallCount);

    // Make sure resource gets unloaded
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(dmResource::RESULT_NOT_LOADED, e);
}


static bool PreloaderCompleteCallback(const dmResource::PreloaderCompleteCallbackParams* params)
{
    uint32_t* user_data = (uint32_t*) params->m_UserData;
    (*user_data)++;
    return true;
}


TEST_P(GetResourceTest, PreloadGet)
{
    dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, m_ResourceName);


    uint32_t pcc_result = 0;
    dmResource::PreloaderCompleteCallbackParams pcc_params;
    pcc_params.m_Factory = m_Factory;
    pcc_params.m_UserData = &pcc_result;

    dmResource::Result r;
    for (uint32_t i=0;i<33;i++)
    {
        r = dmResource::UpdatePreloader(pr, PreloaderCompleteCallback, &pcc_params, 30*1000);
        if (r == dmResource::RESULT_PENDING)
            dmTime::Sleep(30000);
        else
            break;
    }

    ASSERT_EQ(r, dmResource::RESULT_OK);
    ASSERT_EQ(pcc_result, 1);

    // Ensure preloader holds one reference now
    dmResource::SResourceDescriptor descriptor;
    dmResource::Result e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor.m_ReferenceCount);

    TestResourceContainer* resource = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor.m_ReferenceCount);

    dmResource::DeletePreloader(pr);

    // only one after release
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor.m_ReferenceCount);

    dmResource::Release(m_Factory, resource);
}

TEST_P(GetResourceTest, PreloadGetList)
{
    const char* resource_names_list[] = { m_ResourceName, "/test_ref.cont" };
    dmArray<const char*> resource_names(resource_names_list, 2, 3);
    dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, resource_names);
    const char* subresource_name = "/test01.foo";

    uint32_t pcc_result = 0;
    dmResource::PreloaderCompleteCallbackParams pcc_params;
    pcc_params.m_Factory = m_Factory;
    pcc_params.m_UserData = &pcc_result;

    dmResource::Result r;
    for (uint32_t i=0;i<33;i++)
    {
        r = dmResource::UpdatePreloader(pr, PreloaderCompleteCallback, &pcc_params, 30*1000);
        if (r == dmResource::RESULT_PENDING)
            dmTime::Sleep(30000);
        else
            break;
    }
    ASSERT_EQ(r, dmResource::RESULT_OK);
    ASSERT_EQ(pcc_result, 1);

    // Ensure preloader holds one reference now
    dmResource::SResourceDescriptor descriptor;
    dmResource::Result e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor.m_ReferenceCount);
    // Ensure preloader holds two references to subresources referenced by two parents
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor.m_ReferenceCount);

    TestResourceContainer* resource = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor.m_ReferenceCount);

    TestResourceContainer* subresource = 0;
    e = dmResource::Get(m_Factory, subresource_name, (void**) &subresource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 3, descriptor.m_ReferenceCount);

    dmResource::DeletePreloader(pr);

    // only two after preloader release
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor.m_ReferenceCount);

    // only one after preloader release
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor.m_ReferenceCount);

    // only two after parent resource release
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor.m_ReferenceCount);

    dmResource::Release(m_Factory, subresource);
    dmResource::Release(m_Factory, resource);
}


TEST_P(GetResourceTest, PreloadGetParallell)
{
    // Race preloaders against eachother with the same Factory
    for (uint32_t i=0;i<5;i++)
    {
        const uint32_t n = 16;
        dmResource::HPreloader pr[n];
        for (uint32_t j=0;j<n;j++)
        {
            pr[j] = dmResource::NewPreloader(m_Factory, m_ResourceName);
        }

        for (uint32_t j=0;j<30;j++)
        {
            bool done = true;
            for (uint32_t k=0;k<n;k++)
            {
                dmResource::Result r = dmResource::UpdatePreloader(pr[k], 0, 0, 1000);
                if (r == dmResource::RESULT_PENDING)
                {
                    done = false;
                    continue;
                }
                ASSERT_EQ(dmResource::RESULT_OK, r);
            }
            if (done)
            {
                break;
            }
        }

        TestResourceContainer* resource = 0;
        dmResource::Result e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        dmResource::SResourceDescriptor descriptor;
        e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        ASSERT_EQ((uint32_t) (n+1), descriptor.m_ReferenceCount);

        for (uint32_t j=0;j<n;j++)
        {
            dmResource::DeletePreloader(pr[j]);
        }

        // only one after release
        e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        ASSERT_EQ((uint32_t) 1, descriptor.m_ReferenceCount);

        dmResource::Release(m_Factory, resource);
    }
}

TEST_P(GetResourceTest, PreloadGetManyRefs)
{
    // this has more references than the preloader can fit into its tree
    dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, "/many_refs.cont");

    dmResource::Result r;
    for (uint32_t i=0;i<1000;i++)
    {
        r = dmResource::UpdatePreloader(pr, 0, 0, 30*1000);
        if (r == dmResource::RESULT_PENDING)
            dmTime::Sleep(30000);
        else
            break;
    }

    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, r);
    dmResource::DeletePreloader(pr);
}


TEST_P(GetResourceTest, PreloadGetAbort)
{
    // Must not leak or crash
    for (uint32_t i=0;i<20;i++)
    {
        dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, m_ResourceName);
        for (uint32_t j=0;j<i;j++)
            dmResource::UpdatePreloader(pr, 0, 0, 1);
        dmResource::DeletePreloader(pr);
    }
}


dmResource::Result RecreateResourceCreate(const dmResource::ResourceCreateParams& params)
{
    const int TMP_BUFFER_SIZE = 64;
    char tmp[TMP_BUFFER_SIZE];
    if (params.m_BufferSize < TMP_BUFFER_SIZE) {
        memcpy(tmp, params.m_Buffer, params.m_BufferSize);
        tmp[params.m_BufferSize] = '\0';
        int* recreate_resource = new int(atoi(tmp));
        params.m_Resource->m_Resource = (void*) recreate_resource;
        params.m_Resource->m_ResourceKind = dmResource::KIND_DDF_DATA;
        return dmResource::RESULT_OK;
    } else {
        return dmResource::RESULT_OUT_OF_MEMORY;
    }
}

dmResource::Result RecreateResourceDestroy(const dmResource::ResourceDestroyParams& params)
{
    int* recreate_resource = (int*) params.m_Resource->m_Resource;
    delete recreate_resource;
    return dmResource::RESULT_OK;
}

dmResource::Result RecreateResourceRecreate(const dmResource::ResourceRecreateParams& params)
{
    int* recreate_resource = (int*) params.m_Resource->m_Resource;
    assert(recreate_resource);

    const int TMP_BUFFER_SIZE = 64;
    char tmp[TMP_BUFFER_SIZE];
    if (params.m_BufferSize < TMP_BUFFER_SIZE) {
        memcpy(tmp, params.m_Buffer, params.m_BufferSize);
        tmp[params.m_BufferSize] = '\0';
        *recreate_resource = atoi(tmp);
        return dmResource::RESULT_OK;
    } else {
        return dmResource::RESULT_OUT_OF_MEMORY;
    }
}

TEST(dmResource, InvalidHost)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, "http://foo_host");
    ASSERT_EQ((void*) 0, factory);
}

TEST(dmResource, InvalidUri)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, "gopher://foo_host");
    ASSERT_EQ((void*) 0, factory);
}

dmResource::Result AdResourceCreate(const dmResource::ResourceCreateParams& params)
{
    char* duplicate = (char*)malloc((params.m_BufferSize + 1) * sizeof(char));
    memcpy(duplicate, params.m_Buffer, params.m_BufferSize);
    duplicate[params.m_BufferSize] = '\0';
    params.m_Resource->m_Resource = duplicate;
    return dmResource::RESULT_OK;
}

dmResource::Result AdResourceDestroy(const dmResource::ResourceDestroyParams& params)
{
    free(params.m_Resource->m_Resource);
    return dmResource::RESULT_OK;
}

TEST(dmResource, Builtins)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;

    params.m_ArchiveIndex.m_Data    = (const void*) RESOURCES_ARCI;
    params.m_ArchiveIndex.m_Size    = RESOURCES_ARCI_SIZE;

    params.m_ArchiveData.m_Data     = (const void*) RESOURCES_ARCD;
    params.m_ArchiveData.m_Size     = RESOURCES_ARCD_SIZE;

    params.m_ArchiveManifest.m_Data = (const void*) RESOURCES_DMANIFEST;
    params.m_ArchiveManifest.m_Size = RESOURCES_DMANIFEST_SIZE;

    dmResource::HFactory factory = dmResource::NewFactory(&params, ".");
    ASSERT_NE((void*) 0, factory);

    dmResource::RegisterType(factory, "adc", 0, 0, AdResourceCreate, 0, AdResourceDestroy, 0, 0);

    void* resource;
    const char* path_name[]     = { "/archive_data/file4.adc", "/archive_data/file1.adc", "/archive_data/file3.adc", "/archive_data/file2.adc" };
    const char* content[]       = {
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "file1_datafile1_datafile1_data",
        "file3_data",
        "file2_datafile2_datafile2_data"
    };

    for (uint32_t i = 0; i < (sizeof(path_name) / sizeof(path_name[0])); ++i)
    {
        dmResource::Result result = dmResource::Get(factory, path_name[i], &resource);
        ASSERT_EQ(dmResource::RESULT_OK, result);
        ASSERT_STRCASEEQ(content[i], (const char*) resource);

        dmResource::Release(factory, resource);
    }

    dmResource::DeleteFactory(factory);
}

TEST(RecreateTest, RecreateTest)
{
    const char* tmp_dir = 0;
#if defined(_MSC_VER)
    tmp_dir = ".";
#else
    tmp_dir = ".";
#endif

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, tmp_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::ResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreate__.foo";
    char file_name[512];
    DM_SNPRINTF(file_name, sizeof(file_name), "%s/%s", tmp_dir, resource_name);

    FILE* f;

    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);
    ASSERT_EQ(123, *resource);

    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);
    ASSERT_EQ(456, *resource);

    unlink(file_name);
    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

volatile bool SendReloadDone = false;
void SendReloadThread(void*)
{
    char buf[256];
    dmResourceDDF::Reload* reload_resource = (dmResourceDDF::Reload*) buf;
    reload_resource->m_Resource = (const char*) sizeof(dmResourceDDF::Reload);
    memcpy(buf + sizeof(reload_resource), "__testrecreate__.foo", strlen("__testrecreate__.foo") + 1);

    dmMessage::URL url;
    url.m_Fragment = 0;
    url.m_Path = 0;
    dmMessage::GetSocket("@resource", &url.m_Socket);
    dmMessage::Post(0, &url, dmResourceDDF::Reload::m_DDFHash, 0, (uintptr_t) dmResourceDDF::Reload::m_DDFDescriptor, buf, sizeof(buf), 0);

    SendReloadDone = true;
}

TEST(RecreateTest, RecreateTestHttp)
{
    const char* tmp_dir = 0;
#if defined(_MSC_VER)
    tmp_dir = ".";
#else
    tmp_dir = ".";
#endif

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, tmp_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::ResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreate__.foo";
    char file_name[512];
    DM_SNPRINTF(file_name, sizeof(file_name), "%s/%s", tmp_dir, resource_name);

    FILE* f;

    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);
    ASSERT_EQ(123, *resource);

    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    SendReloadDone = false;
    dmThread::Thread send_thread = dmThread::New(&SendReloadThread, 0x8000, 0, "reload");

    do
    {
        dmTime::Sleep(1000 * 10);
        dmResource::UpdateFactory(factory);
    } while (!SendReloadDone);

    dmThread::Join(send_thread);

    ASSERT_EQ(456, *resource);

    unlink(file_name);

    SendReloadDone = false;
    send_thread = dmThread::New(&SendReloadThread, 0x8000, 0, "reload");

    do
    {
        dmTime::Sleep(1000 * 10);
        dmResource::UpdateFactory(factory);
    } while (!SendReloadDone);
    dmThread::Join(send_thread);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

// Test the "filename" callback argument (overkill, but chmu is a TDD-nazi!)

char filename_resource_filename[ 128 ];

dmResource::Result FilenameResourceCreate(const dmResource::ResourceCreateParams& params)
{
    if (strcmp(filename_resource_filename, params.m_Filename) == 0)
        return dmResource::RESULT_OK;
    else
        return dmResource::RESULT_FORMAT_ERROR;
}

dmResource::Result FilenameResourceDestroy(const dmResource::ResourceDestroyParams& params)
{
    return dmResource::RESULT_OK;
}

dmResource::Result FilenameResourceRecreate(const dmResource::ResourceRecreateParams& params)
{
    if (strcmp(filename_resource_filename, params.m_Filename) == 0)
        return dmResource::RESULT_OK;
    else
        return dmResource::RESULT_FORMAT_ERROR;
}

TEST(FilenameTest, FilenameTest)
{
    const char* tmp_dir = 0;
#if defined(_MSC_VER)
    tmp_dir = ".";
#else
    tmp_dir = ".";
#endif

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, tmp_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::ResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testfilename__.foo";
    DM_SNPRINTF(filename_resource_filename, sizeof(filename_resource_filename), "%s/%s", tmp_dir, resource_name);

    FILE* f;

    f = fopen(filename_resource_filename, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);
    ASSERT_EQ(123, *resource);

    f = fopen(filename_resource_filename, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);
    ASSERT_EQ(456, *resource);

    unlink(filename_resource_filename);
    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

struct CallbackUserData
{
    CallbackUserData() : m_Descriptor(0x0), m_Name(0x0) {}
    dmResource::SResourceDescriptor* m_Descriptor;
    const char* m_Name;
};

void ReloadCallback(const dmResource::ResourceReloadedParams& params)
{
    CallbackUserData* data = (CallbackUserData*) params.m_UserData;
    data->m_Descriptor = params.m_Resource;
    data->m_Name = params.m_Name;
}

TEST(RecreateTest, ReloadCallbackTest)
{
    const char* tmp_dir = 0;
#if defined(_MSC_VER)
    tmp_dir = ".";
#else
    tmp_dir = ".";
#endif

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, tmp_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreate__.foo";
    char file_name[512];
    DM_SNPRINTF(file_name, sizeof(file_name), "%s/%s", tmp_dir, resource_name);

    FILE* f;

    f = fopen(file_name, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);

    CallbackUserData user_data;
    dmResource::RegisterResourceReloadedCallback(factory, ReloadCallback, &user_data);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_NE((void*)0, user_data.m_Descriptor);
    ASSERT_EQ(0, strcmp(resource_name, user_data.m_Name));

    user_data = CallbackUserData();
    dmResource::UnregisterResourceReloadedCallback(factory, ReloadCallback, &user_data);

    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_EQ((void*)0, user_data.m_Descriptor);
    ASSERT_EQ((void*)0, user_data.m_Name);

    unlink(file_name);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

TEST(OverflowTest, OverflowTest)
{
    const char* test_dir = "build/default/src/test";

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 1;
    dmResource::HFactory factory = dmResource::NewFactory(&params, test_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, "/test01.foo", (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);

    int* resource2;
    fr = dmResource::Get(factory, "/test02.foo", (void**) &resource2);
    ASSERT_NE(dmResource::RESULT_OK, fr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

TEST_P(GetResourceTest, OverflowTestRecursive)
{
    // Needs to be GetResourceTest or cannot use ResourceContainer resource here which is needed for the test.
    const char* test_dir = "build/default/src/test";
    for (uint32_t max=0;max<5;max++)
    {
        // recreate with new settings
        dmResource::DeleteFactory(m_Factory);
        dmResource::NewFactoryParams params;
        params.m_MaxResources = max;
        m_Factory = dmResource::NewFactory(&params, test_dir);
        ASSERT_NE((void*) 0, m_Factory);

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "cont", this, &ResourceContainerPreload, &ResourceContainerCreate, 0, &ResourceContainerDestroy, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        int* resource;
        dmResource::Result fr = dmResource::Get(m_Factory, "/test.cont", (void**) &resource);

        // test.cont contains 2 children so anything less than 3 means it must fail
        if (max < 3)
        {
            ASSERT_EQ(dmResource::RESULT_OUT_OF_RESOURCES, fr);
        }
        else
        {
            ASSERT_EQ(dmResource::RESULT_OK, fr);
            dmResource::Release(m_Factory, resource);
        }
    }
}



struct ResourceHolder
{
    int* m_Value;
};

dmResource::Result SharedResourceCreate(const dmResource::ResourceCreateParams& params)
{
    const int TMP_BUFFER_SIZE = 64;
    char tmp[TMP_BUFFER_SIZE];
    if (params.m_BufferSize < TMP_BUFFER_SIZE) {
        memcpy(tmp, params.m_Buffer, params.m_BufferSize);
        tmp[params.m_BufferSize] = '\0';
        ResourceHolder* resource = new ResourceHolder;
        resource->m_Value = new int(atoi(tmp));
        params.m_Resource->m_Resource = (void*) resource;
        params.m_Resource->m_ResourceKind = dmResource::KIND_DDF_DATA;

        return dmResource::RESULT_OK;
    } else {
        return dmResource::RESULT_OUT_OF_MEMORY;
    }
}

dmResource::Result SharedResourceDestroy(const dmResource::ResourceDestroyParams& params)
{

    ResourceHolder* resource = (ResourceHolder*) params.m_Resource->m_Resource;
    if( params.m_Resource->m_SharedState == dmResource::DATA_SHARE_STATE_NONE )
    {
        delete resource->m_Value;
    }

    delete resource;
    return dmResource::RESULT_OK;
}

dmResource::Result SharedResourceRecreate(const dmResource::ResourceRecreateParams& params)
{
    ResourceHolder* resource = (ResourceHolder*) params.m_Resource->m_Resource;
    assert(resource);

    if( params.m_Resource->m_SharedState == dmResource::DATA_SHARE_STATE_SHALLOW )
    {
        resource->m_Value = new int;
    }

    const int TMP_BUFFER_SIZE = 64;
    char tmp[TMP_BUFFER_SIZE];
    if (params.m_BufferSize < TMP_BUFFER_SIZE) {
        memcpy(tmp, params.m_Buffer, params.m_BufferSize);
        tmp[params.m_BufferSize] = '\0';

        *(resource->m_Value) = atoi(tmp);
        return dmResource::RESULT_OK;
    } else {
        return dmResource::RESULT_OUT_OF_MEMORY;
    }
}

dmResource::Result SharedResourceDuplicate(const dmResource::ResourceDuplicateParams& params)
{
    const ResourceHolder* oldresource = (const ResourceHolder*)params.m_OriginalResource->m_Resource;

    ResourceHolder* resource = new ResourceHolder;
    resource->m_Value = oldresource->m_Value;
    params.m_Resource->m_Resource = (void*)resource;
    return dmResource::RESULT_OK;
}


TEST_F(ResourceTest, IsShared)
{
    ASSERT_EQ(false, dmResource::IsPathTagged("/a/b/c.png") );
    ASSERT_EQ(false, dmResource::IsPathTagged("/a/b/c:d.png") ); // the separator is still a valid filename character

    // This resource is not shared
    ASSERT_EQ(true, dmResource::IsPathTagged("/a/b/c.png:") );
}

TEST(DynamicResources, GetPath)
{
    const char* test_dir = "build/default/src/test";

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 8;
    dmResource::HFactory factory = dmResource::NewFactory(&params, test_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &SharedResourceCreate, 0, &SharedResourceDestroy, &SharedResourceRecreate, &SharedResourceDuplicate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    ResourceHolder* resource1;
    e = dmResource::Get(factory, "/test01.foo", (void**) &resource1);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    ResourceHolder* resource2;
    e = dmResource::Get(factory, "/test02.foo", (void**) &resource2);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource2));

    ResourceHolder* resource3;
    e = dmResource::Get(factory, "/test02.foo:", (void**) &resource3);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource3));
    ASSERT_EQ(2U, dmResource::GetRefCount(factory, resource2));
    ASSERT_NE(resource2, resource3);
    ASSERT_EQ(resource2->m_Value, resource3->m_Value);


    char canonical_path[dmResource::RESOURCE_PATH_MAX];


    uint64_t hash = 0;
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, dmResource::GetPath(factory, 0, &hash) );

    hash = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetPath(factory, resource1, &hash) );
    dmResource::GetCanonicalPath("/test01.foo", canonical_path);
    ASSERT_EQ( dmHashString64(canonical_path), hash );

    hash = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetPath(factory, resource2, &hash) );
    dmResource::GetCanonicalPath("/test02.foo", canonical_path);
    ASSERT_EQ( dmHashString64(canonical_path), hash );

    hash = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetPath(factory, resource3, &hash) );
    dmResource::GetCanonicalPath("/test02.foo_0", canonical_path);
    ASSERT_EQ( dmHashString64(canonical_path), hash );


    dmResource::Release(factory, resource3);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource2));
    dmResource::Release(factory, resource2);
    dmResource::Release(factory, resource1);
    dmResource::DeleteFactory(factory);
}

TEST_F(DynamicResourceTest, Set)
{
    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &SharedResourceCreate, 0, &SharedResourceDestroy, &SharedResourceRecreate, &SharedResourceDuplicate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    ResourceHolder* resource1;
    e = dmResource::Get(factory, "/test01.foo", (void**) &resource1);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource1));

    ResourceHolder* resource2;
    e = dmResource::Get(factory, "/test01.foo:", (void**) &resource2);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource2));
    ASSERT_EQ(2U, dmResource::GetRefCount(factory, resource1));


    ASSERT_NE(resource1, resource2);
    ASSERT_EQ(resource1->m_Value, resource2->m_Value);

    // Create buffer
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("data"), dmBuffer::VALUE_TYPE_UINT8, 1}
    };

    uint32_t element_count = 32;
    dmBuffer::HBuffer buffer = 0;
    dmBuffer::Create(element_count, streams_decl, 1, &buffer);

    uint8_t* data = 0;
    uint32_t datasize = 0;
    dmBuffer::Result r = dmBuffer::GetBytes(buffer, (void**)&data, &datasize);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_NE(0U, (uintptr_t)data);
    ASSERT_EQ(32U, datasize);


    // Set buffer
    uint64_t hash = 0;

    // Change data on the shared resource
    strcpy((char*)data, "123");

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetPath(factory, resource1, &hash) );
    e = dmResource::SetResource(factory, hash, buffer);

    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(resource1->m_Value, resource2->m_Value); // The pointers are still the same
    ASSERT_EQ(*resource1->m_Value, *resource2->m_Value);
    ASSERT_EQ(123, *resource2->m_Value);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource2));
    ASSERT_EQ(2U, dmResource::GetRefCount(factory, resource1));

    // Set data on the tagged resource
    strcpy((char*)data, "42");

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetPath(factory, resource2, &hash) );
    e = dmResource::SetResource(factory, hash, buffer);

    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE(resource1->m_Value, resource2->m_Value);
    ASSERT_NE(*resource1->m_Value, *resource2->m_Value);
    ASSERT_EQ(123, *resource1->m_Value);
    ASSERT_EQ(42, *resource2->m_Value);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource2));
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource1));

    dmBuffer::Destroy(buffer);
    dmResource::Release(factory, resource2);
    dmResource::Release(factory, resource1);
}


TEST_F(DynamicResourceTest, RefCount)
{
    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", this, 0, &SharedResourceCreate, 0, &SharedResourceDestroy, &SharedResourceRecreate, &SharedResourceDuplicate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    ResourceHolder* resource1;
    e = dmResource::Get(factory, "/test01.foo:", (void**) &resource1);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, resource1));

    // Make sure the original resource exists, and has refcount 1
    char canonical_path[dmResource::RESOURCE_PATH_MAX];
    dmResource::GetCanonicalPath("/test01.foo", canonical_path);
    uint64_t hash = dmHashString64(canonical_path);
    ASSERT_EQ(1U, dmResource::GetRefCount(factory, hash));

    dmResource::Release(factory, resource1);
}


int main(int argc, char **argv)
{
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}

