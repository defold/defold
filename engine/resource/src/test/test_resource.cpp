// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <dlib/log.h>

#include <dlib/atomic.h>
#include <dlib/condition_variable.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/mutex.h>
#include <dlib/socket.h>
#include <dlib/sys.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/testutil.h>
#include <ddf/ddf.h>

#if defined(DM_PLATFORM_VENDOR)
    #define TMP_DIR ""
    #define MOUNT_DIR "file:"
#else
    #define TMP_DIR "."
    #define MOUNT_DIR "."
#endif

#include <resource/resource_ddf.h>
#include "../resource.h"
#include "../resource_archive.h"
#include "../resource_archive_private.h"
#include "../resource_manifest.h"
#include "../resource_manifest_private.h"
#include "../resource_mounts.h"
#include "../resource_private.h"
#include "../resource_util.h"
#include "../resource_verify.h"
#include "../providers/provider_private.h"
#include "test/test_resource_ddf.h"

#if defined(DM_TEST_HTTP_SUPPORTED)
#include <dlib/http/http_client.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/uri.h>

static int g_HttpPort = -1;
char g_HttpAddress[128] = "localhost";

#endif


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static dmResource::Result TypedResourcePreload(const dmResource::ResourcePreloadParams*)
{
    return dmResource::RESULT_OK;
}

// Resource callbacks must retain the C API type when declared through the C++ SDK.
TEST(Resource, CallbackTypeCompatibility)
{
    FResourcePreload preload = TypedResourcePreload;
    ASSERT_EQ(RESOURCE_RESULT_OK, preload(0));
}


#include <vector>

extern unsigned char RESOURCES_ARCI[];
extern uint32_t RESOURCES_ARCI_SIZE;
extern unsigned char RESOURCES_ARCD[];
extern uint32_t RESOURCES_ARCD_SIZE;
extern unsigned char RESOURCES_DMANIFEST[];
extern uint32_t RESOURCES_DMANIFEST_SIZE;

#define EXT_CONSTANTS(prefix, ext)\
    static const dmhash_t prefix##_EXT_HASH = dmHashString64(ext);\

    EXT_CONSTANTS(CONT, "cont")
    EXT_CONSTANTS(FOO, "foo")

#undef EXT_CONSTANTS

class ResourceTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        JobSystemCreateParams job_thread_create_param = {0};
        job_thread_create_param.m_ThreadCount    = 1;
        m_JobContext = JobSystemCreate(&job_thread_create_param);

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        params.m_JobThreadContext = m_JobContext;

        factory = dmResource::NewFactory(&params, MOUNT_DIR);
        ASSERT_NE((void*) 0, factory);
    }

    void TearDown() override
    {
        if (factory != NULL)
        {
            dmResource::DeleteFactory(factory);
        }
        JobSystemDestroy(m_JobContext);
    }

    dmResource::HFactory factory;
    HJobContext m_JobContext;
};

class DynamicResourceTest : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        JobSystemCreateParams job_thread_create_param = {0};
        job_thread_create_param.m_ThreadCount    = 1;
        m_JobContext = JobSystemCreate(&job_thread_create_param);

        const char* test_dir = "build/src/test";
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        params.m_JobThreadContext = m_JobContext;

        factory = dmResource::NewFactory(&params, test_dir);
        ASSERT_NE((void*) 0, factory);
    }

    void TearDown() override
    {
        if (factory != NULL)
        {
            dmResource::DeleteFactory(factory);
        }
        JobSystemDestroy(m_JobContext);
    }

    dmResource::HFactory factory;
    HJobContext m_JobContext;
};


dmResource::Result DummyCreate(const dmResource::ResourceCreateParams* params)
{
    return dmResource::RESULT_OK;
}

dmResource::Result DummyDestroy(const dmResource::ResourceDestroyParams* params)
{
    return dmResource::RESULT_OK;
}

TEST_F(ResourceTest, RegisterType)
{
    dmResource::Result e;

    // Test create/destroy function == 0
    e = dmResource::RegisterType(factory, "foo", 0, 0, 0, 0, 0, 0);
    ASSERT_EQ(dmResource::RESULT_INVAL, e);

    // Test dot in extension
    e = dmResource::RegisterType(factory, ".foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0);
    ASSERT_EQ(dmResource::RESULT_INVAL, e);

    // Test "ok"
    e = dmResource::RegisterType(factory, "foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    // Test already registred
    e = dmResource::RegisterType(factory, "foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0);
    ASSERT_EQ(dmResource::RESULT_ALREADY_REGISTERED, e);

    // Test get type/extension from type/extension
    HResourceType type;
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
    e = dmResource::RegisterType(factory, "foo", 0, 0, &DummyCreate, 0, &DummyDestroy, 0);
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
    e = dmResource::Get(factory, "/build/src/test/test.testresourcecont", &resource);
    ASSERT_EQ(dmResource::RESULT_UNKNOWN_RESOURCE_TYPE, e);
    ASSERT_EQ((void*) 0, resource);
}

// Loaded version (in-game) of ResourceContainerDesc
struct TestResourceContainer
{
    uint64_t                                m_NameHash;
    std::vector<TestResource::ResourceFoo*> m_Resources;
};

dmResource::Result ResourceContainerPreload(const dmResource::ResourcePreloadParams* params);

dmResource::Result ResourceContainerCreate(const dmResource::ResourceCreateParams* params);

dmResource::Result ResourceContainerDestroy(const dmResource::ResourceDestroyParams* params);

dmResource::Result FooResourceCreate(const dmResource::ResourceCreateParams* params);

dmResource::Result FooResourcePostCreate(const dmResource::ResourcePostCreateParams* params);

dmResource::Result FooResourceDestroy(const dmResource::ResourceDestroyParams* params);

class GetResourceTest : public jc_test_params_class<const char*>
{
protected:
    void SetUp() override
    {
        m_ResourceContainerCreateCallCount = 0;
        m_ResourceContainerDestroyCallCount = 0;
        m_FooResourceCreateCallCount = 0;
        m_FooResourcePostCreateCallCount = 0;
        m_FooResourceDestroyCallCount = 0;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;

        const char* original_mount_path = GetParam();
#if defined(DM_TEST_HTTP_SUPPORTED)
        char mountpath[512];
        if (strstr(original_mount_path, "http") == original_mount_path)
        {
            dmSnPrintf(mountpath, sizeof(mountpath), original_mount_path, g_HttpAddress, g_HttpPort);
            original_mount_path = mountpath;
        }
#endif

        m_Factory = dmResource::NewFactory(&params, original_mount_path);

        ASSERT_NE((void*) 0, m_Factory);
        m_ResourceName = "/test.cont";

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "cont", this, &ResourceContainerPreload, &ResourceContainerCreate, 0, &ResourceContainerDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        e = dmResource::RegisterType(m_Factory, "foo", this, 0, &FooResourceCreate, &FooResourcePostCreate, &FooResourceDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);
    }

    void TearDown() override
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

dmResource::Result ResourceContainerPreload(const dmResource::ResourcePreloadParams* params)
{
    TestResource::ResourceContainerDesc* resource_container_desc;
    dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &TestResource_ResourceContainerDesc_DESCRIPTOR, (void**) &resource_container_desc);
    if (e != dmDDF::RESULT_OK)
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }

    for (uint32_t i = 0; i < resource_container_desc->m_Resources.m_Count; ++i)
    {
        dmResource::PreloadHint(params->m_HintInfo, resource_container_desc->m_Resources[i]);
    }

    *params->m_PreloadData = resource_container_desc;
    return dmResource::RESULT_OK;
}

dmResource::Result ResourceContainerCreate(const dmResource::ResourceCreateParams* params)
{
    HResourceType type = params->m_Type;
    GetResourceTest* self = (GetResourceTest*) ResourceTypeGetContext(type);
    self->m_ResourceContainerCreateCallCount++;

    TestResource::ResourceContainerDesc* resource_container_desc = (TestResource::ResourceContainerDesc*) params->m_PreloadData;

    TestResourceContainer* resource_cont = new TestResourceContainer();
    resource_cont->m_NameHash = dmHashBuffer64(resource_container_desc->m_Name, strlen(resource_container_desc->m_Name));

    ResourceDescriptorSetResource(params->m_Resource, (void*) resource_cont);

    bool error = false;
    dmResource::Result factory_e = dmResource::RESULT_OK;
    for (uint32_t i = 0; i < resource_container_desc->m_Resources.m_Count; ++i)
    {
        TestResource::ResourceFoo* sub_resource;
        factory_e = dmResource::Get(params->m_Factory, resource_container_desc->m_Resources[i], (void**)&sub_resource);
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
            dmResource::Release(params->m_Factory, resource_cont->m_Resources[i]);
        }
        delete resource_cont;
        ResourceDescriptorSetResource(params->m_Resource, 0);
        return factory_e;
    }
    else
    {
        return dmResource::RESULT_OK;
    }
}

dmResource::Result ResourceContainerDestroy(const dmResource::ResourceDestroyParams* params)
{
    HResourceType type = params->m_Type;
    GetResourceTest* self = (GetResourceTest*) ResourceTypeGetContext(type);
    self->m_ResourceContainerDestroyCallCount++;

    TestResourceContainer* resource_cont = (TestResourceContainer*) ResourceDescriptorGetResource(params->m_Resource);

    std::vector<TestResource::ResourceFoo*>::iterator i;
    for (i = resource_cont->m_Resources.begin(); i != resource_cont->m_Resources.end(); ++i)
    {
        dmResource::Release(params->m_Factory, *i);
    }
    delete resource_cont;
    return dmResource::RESULT_OK;
}

dmResource::Result FooResourceCreate(const dmResource::ResourceCreateParams* params)
{
    HResourceType type = params->m_Type;
    GetResourceTest* self = (GetResourceTest*) ResourceTypeGetContext(type);
    self->m_FooResourceCreateCallCount++;

    TestResource::ResourceFoo* resource_foo;

    dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer,
                                         params->m_BufferSize,
                                         &TestResource_ResourceFoo_DESCRIPTOR, (void**) &resource_foo);
    if (e == dmDDF::RESULT_OK)
    {
        ResourceDescriptorSetResource(params->m_Resource, resource_foo);
        return dmResource::RESULT_OK;
    }
    else
    {
        return dmResource::RESULT_FORMAT_ERROR;
    }
}

dmResource::Result FooResourcePostCreate(const dmResource::ResourcePostCreateParams* params)
{
    HResourceType type = params->m_Type;
    GetResourceTest* self = (GetResourceTest*) ResourceTypeGetContext(type);
    self->m_FooResourcePostCreateCallCount++;
    return dmResource::RESULT_OK;
}

dmResource::Result FooResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    HResourceType type = params->m_Type;
    GetResourceTest* self = (GetResourceTest*) ResourceTypeGetContext(type);
    self->m_FooResourceDestroyCallCount++;

    dmDDF::FreeMessage(ResourceDescriptorGetResource(params->m_Resource));
    return dmResource::RESULT_OK;
}

TEST_P(GetResourceTest, GetTestResource)
{
    dmResource::Result e;

    TestResourceContainer* test_resource_cont = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &test_resource_cont);
    if (e != dmResource::RESULT_OK)
    {
        printf("Failed to load resource: %s\n", m_ResourceName);
    }
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
    ASSERT_EQ(dmResource::RESULT_OK, e);
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

    HResourceDescriptor descriptor;
    // No exts means any ext
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, 0, 0, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, ResourceDescriptorGetNameHash(descriptor));

    // One ext
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, &CONT_EXT_HASH, 1, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, ResourceDescriptorGetNameHash(descriptor));

    // Two exts
    dmhash_t two_exts[] = {CONT_EXT_HASH, FOO_EXT_HASH};
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, two_exts, 2, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, ResourceDescriptorGetNameHash(descriptor));

    // Two exts, reversed order
    dmhash_t two_exts_rev[] = {FOO_EXT_HASH, CONT_EXT_HASH};
    e = dmResource::GetDescriptorWithExt(m_Factory, name_hash, two_exts_rev, 2, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ(name_hash, ResourceDescriptorGetNameHash(descriptor));

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

TEST_P(GetResourceTest, GetWithExt)
{
    void* resource = 0;
    dmResource::Result e = dmResource::GetWithExt(m_Factory, m_ResourceName, "cont", &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE((void*)0, resource);

    void* sentinel = (void*)0xdeadbeef;
    e = dmResource::GetWithExt(m_Factory, m_ResourceName, "foo", &sentinel);
    ASSERT_EQ(dmResource::RESULT_INVALID_FILE_EXTENSION, e);
    ASSERT_EQ((void*)0, sentinel);

    void* hashed_resource = 0;
    dmhash_t name_hash = dmHashString64(m_ResourceName);
    e = dmResource::GetWithExt(m_Factory, name_hash, CONT_EXT_HASH, &hashed_resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_NE((void*)0, hashed_resource);

    void* hashed_sentinel = (void*)0xdeadbeef;
    e = dmResource::GetWithExt(m_Factory, name_hash, FOO_EXT_HASH, &hashed_sentinel);
    ASSERT_EQ(dmResource::RESULT_INVALID_FILE_EXTENSION, e);
    ASSERT_EQ((void*)0xdeadbeef, hashed_sentinel);

    dmResource::Release(m_Factory, hashed_resource);
    dmResource::Release(m_Factory, resource);
}

const char* params_resource_paths[] = {
    "build/src/test",
#if defined(DM_TEST_HTTP_SUPPORTED)
    "http://%s:%d",
#endif
    "dmanif:build/src/test/resources_pb.dmanifest"
};
INSTANTIATE_TEST_CASE_P(GetResourceTestURI, GetResourceTest, jc_test_values_in(params_resource_paths));

TEST_P(GetResourceTest, GetReference1)
{
    dmResource::Result e;

    HResourceDescriptor descriptor;
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

    HResourceDescriptor descriptor;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, m_ResourceContainerCreateCallCount);
    ASSERT_EQ((uint32_t) 0, m_ResourceContainerDestroyCallCount);

    ASSERT_EQ((uint32_t) 1, descriptor->m_ReferenceCount);
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

    HResourceDescriptor descriptor1;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor1);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor1->m_ReferenceCount);

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

    HResourceDescriptor descriptor2;
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor2);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor2->m_ReferenceCount);

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
    ASSERT_EQ((uint32_t) 1, descriptor1->m_ReferenceCount);

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
    HResourceDescriptor descriptor;
    dmResource::Result e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor->m_ReferenceCount);

    TestResourceContainer* resource = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor->m_ReferenceCount);

    dmResource::DeletePreloader(pr);

    // only one after release
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor->m_ReferenceCount);

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
    HResourceDescriptor descriptor;
    dmResource::Result e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor->m_ReferenceCount);
    // Ensure preloader holds two references to subresources referenced by two parents
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor->m_ReferenceCount);

    TestResourceContainer* resource = 0;
    e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor->m_ReferenceCount);

    TestResourceContainer* subresource = 0;
    e = dmResource::Get(m_Factory, subresource_name, (void**) &subresource);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 3, descriptor->m_ReferenceCount);

    dmResource::DeletePreloader(pr);

    // only two after preloader release
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor->m_ReferenceCount);

    // only one after preloader release
    e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 1, descriptor->m_ReferenceCount);

    // only two after parent resource release
    e = dmResource::GetDescriptor(m_Factory, subresource_name, &descriptor);
    ASSERT_EQ(dmResource::RESULT_OK, e);
    ASSERT_EQ((uint32_t) 2, descriptor->m_ReferenceCount);

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

        uint32_t timeout = 4000;
        bool done;
        for (uint32_t j=0;j<100;j++)
        {
            done = true;
            for (uint32_t k=0;k<n;k++)
            {
                dmResource::Result r = dmResource::UpdatePreloader(pr[k], 0, 0, timeout);
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
        ASSERT_TRUE(done);

        TestResourceContainer* resource = 0;
        dmResource::Result e = dmResource::Get(m_Factory, m_ResourceName, (void**) &resource);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        HResourceDescriptor descriptor;
        e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        ASSERT_EQ((uint32_t) (n+1), descriptor->m_ReferenceCount);

        for (uint32_t j=0;j<n;j++)
        {
            dmResource::DeletePreloader(pr[j]);
        }

        // only one after release
        e = dmResource::GetDescriptor(m_Factory, m_ResourceName, &descriptor);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        ASSERT_EQ((uint32_t) 1, descriptor->m_ReferenceCount);

        dmResource::Release(m_Factory, resource);
    }
}

TEST_P(GetResourceTest, PreloadGetManyRefs)
{
    // this has more references than the preloader can fit into its tree
    dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, "/many_refs.cont");

    uint32_t timeout = 100*1000;
    dmResource::Result r;
    for (uint32_t i=0;i<1000;i++)
    {
        r = dmResource::UpdatePreloader(pr, 0, 0, timeout);
        if (r == dmResource::RESULT_PENDING)
            dmTime::Sleep(timeout);
        else
            break;
    }

    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, r);
    dmResource::DeletePreloader(pr);
}

TEST_P(GetResourceTest, PreloadPathCacheFull)
{
    // Normal paths exhaust the name cache. Noncanonical paths use two entries
    // and exhaust it while inserting the canonical name instead. Both early
    // returns must release the scoped lock exactly once: Darwin's os_unfair_lock
    // aborts on the double unlock that the old spinlock implementation tolerated.
    const char* prefixes[] = { "/", "/./" };
    for (uint32_t p = 0; p < 2; ++p)
    {
        dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, m_ResourceName);
        ResourcePreloadHintInfo info;
        info.m_Preloader = pr;
        info.m_Parent = 0;
        bool full = false;
        for (uint32_t i = 0; i < 4096; ++i)
        {
            char path[64];
            dmSnPrintf(path, sizeof(path), "%smissing_%u.foo", prefixes[p], i);
            if (!dmResource::PreloadHint(&info, path))
            {
                full = true;
                break;
            }
        }
        ASSERT_TRUE(full);
        // Re-enter the same lock after failure and confirm existing cache entries
        // remain usable when no new entries can be added.
        ASSERT_TRUE(dmResource::PreloadHint(&info, m_ResourceName));
        dmResource::DeletePreloader(pr);
    }
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


dmResource::Result RecreateResourceCreate(const dmResource::ResourceCreateParams* params)
{
    const int TMP_BUFFER_SIZE = 64;
    char tmp[TMP_BUFFER_SIZE];
    uint32_t data_size = params->m_BufferSize;
    if (data_size < TMP_BUFFER_SIZE) {
        memcpy(tmp, params->m_Buffer, data_size);
        tmp[data_size] = '\0';
        int* recreate_resource = new int(atoi(tmp));
        ResourceDescriptorSetResource(params->m_Resource, recreate_resource);
        return dmResource::RESULT_OK;
    } else {
        return dmResource::RESULT_OUT_OF_MEMORY;
    }
}

dmResource::Result RecreateResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    int* recreate_resource = (int*) ResourceDescriptorGetResource(params->m_Resource);
    delete recreate_resource;
    return dmResource::RESULT_OK;
}

dmResource::Result RecreateResourceRecreate(const dmResource::ResourceRecreateParams* params)
{
    int* recreate_resource = (int*) ResourceDescriptorGetResource(params->m_Resource);
    assert(recreate_resource);
    int* old_resource = new int();
    *old_resource = *recreate_resource;

    ResourceDescriptorSetPrevResource(params->m_Resource, old_resource);

    if (params->m_Message)
    {
        *recreate_resource = *(const int*)params->m_Message;
        return dmResource::RESULT_OK;
    }

    const int TMP_BUFFER_SIZE = 64;
    char tmp[TMP_BUFFER_SIZE];
    uint32_t data_size = params->m_BufferSize;
    if (data_size < TMP_BUFFER_SIZE) {
        memcpy(tmp, params->m_Buffer, data_size);
        tmp[data_size] = '\0';
        *recreate_resource = atoi(tmp);
        return dmResource::RESULT_OK;
    } else {
        return dmResource::RESULT_OUT_OF_MEMORY;
    }
}

#if defined(DM_TEST_HTTP_SUPPORTED)
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
#endif

dmResource::Result AdResourceCreate(const dmResource::ResourceCreateParams* params)
{
    uint32_t data_size = params->m_BufferSize;
    const void* data = params->m_Buffer;
    char* duplicate = (char*)malloc((data_size + 1) * sizeof(char));
    memcpy(duplicate, data, data_size);
    duplicate[data_size] = '\0';
    ResourceDescriptorSetResource(params->m_Resource, duplicate);
    return dmResource::RESULT_OK;
}

dmResource::Result AdResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    free(ResourceDescriptorGetResource(params->m_Resource));
    return dmResource::RESULT_OK;
}

// Verifies that creation stores distinct resource versions, guarding against
// assigning the version after copying the descriptor into the resource map.
TEST_F(ResourceTest, CreatedResourcesHaveDistinctVersions)
{
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(factory, "adc", 0, 0, AdResourceCreate, 0, AdResourceDestroy, 0));

    char data[] = "test";
    void* first_resource = 0;
    void* second_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, "/version_first.adc", data, sizeof(data), &first_resource));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, "/version_second.adc", data, sizeof(data), &second_resource));

    uint16_t first_version = dmResource::GetVersion(factory, first_resource);
    uint16_t second_version = dmResource::GetVersion(factory, second_resource);
    dmResource::Release(factory, first_resource);
    dmResource::Release(factory, second_resource);

    ASSERT_NE(dmResource::RESOURCE_VERSION_INVALID, first_version);
    ASSERT_NE(dmResource::RESOURCE_VERSION_INVALID, second_version);
    ASSERT_NE(first_version, second_version);
}

static dmResource::Result VersionResourceCreate(const dmResource::ResourceCreateParams* params)
{
    ResourceDescriptorSetResource(params->m_Resource, params->m_Context);
    return dmResource::RESULT_OK;
}

// Verifies that reusing a resource's path and address changes its version,
// preventing stale handles from matching a later resource's identity.
TEST_F(ResourceTest, RecreatedResourceAtSameAddressHasNewVersion)
{
    int resource_storage = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(factory, "version", &resource_storage, 0, VersionResourceCreate, 0, DummyDestroy, 0));

    const char* path = "/recreated.version";
    char data[] = "test";
    void* first_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, path, data, sizeof(data), &first_resource));
    uint16_t first_version = dmResource::GetVersion(factory, first_resource);
    dmResource::Release(factory, first_resource);

    void* recreated_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, path, data, sizeof(data), &recreated_resource));
    uint16_t recreated_version = dmResource::GetVersion(factory, recreated_resource);
    dmResource::Release(factory, recreated_resource);

    ASSERT_EQ(first_resource, recreated_resource);
    ASSERT_NE(dmResource::RESOURCE_VERSION_INVALID, recreated_version);
    ASSERT_NE(first_version, recreated_version);
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

    dmResource::RegisterType(factory, "adc", 0, 0, AdResourceCreate, 0, AdResourceDestroy, 0);

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
        ASSERT_STREQ(content[i], (const char*) resource);

        dmResource::Release(factory, resource);
    }

    dmResource::DeleteFactory(factory);
}

struct ReloadData {
    ReloadData(): m_Old(0), m_New(0) {}
    int m_Old;
    int m_New;
};

static void ResourceReloadedCallback(const ResourceReloadedParams* params) {
    ReloadData* data = (ReloadData*)params->m_UserData;
    data->m_Old = *((int*)ResourceDescriptorGetPrevResource(params->m_Resource));
    data->m_New = *((int*)ResourceDescriptorGetResource(params->m_Resource));
}

static dmResource::Result CountRecreatedResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    ++*(uint32_t*)params->m_Context;
    return RecreateResourceDestroy(params);
}

// Both SetResource overloads must retain old data during callbacks and destroy it afterwards.
TEST(RecreateTest, SetResourceDestroysPreviousResourceAfterCallbacks)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*)0, factory);

    uint32_t destroy_count = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(factory, "foo", &destroy_count, 0,
        RecreateResourceCreate, 0, CountRecreatedResourceDestroy, RecreateResourceRecreate));
    ReloadData reload_data;
    dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedCallback, &reload_data);

    const char* resource_name = "/set_resource.foo";
    char initial_data[] = "123";
    int* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, resource_name, initial_data, sizeof(initial_data), (void**)&resource));
    HResourceDescriptor descriptor;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetDescriptor(factory, resource_name, &descriptor));

    char new_data[] = "456";
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(factory, dmHashString64(resource_name), new_data, sizeof(new_data)));
    ASSERT_EQ(123, reload_data.m_Old);
    ASSERT_EQ(456, reload_data.m_New);
    ASSERT_EQ(1u, destroy_count);
    ASSERT_EQ((void*)0, ResourceDescriptorGetPrevResource(descriptor));

    int message = 789;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(factory, dmHashString64(resource_name), &message));
    ASSERT_EQ(456, reload_data.m_Old);
    ASSERT_EQ(789, reload_data.m_New);
    ASSERT_EQ(2u, destroy_count);
    ASSERT_EQ((void*)0, ResourceDescriptorGetPrevResource(descriptor));

    dmResource::Release(factory, resource);
    ASSERT_EQ(3u, destroy_count);
    dmResource::DeleteFactory(factory);
}

TEST(RecreateTest, RecreateTest)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*) 0, factory);

    ReloadData reload_data;
    dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedCallback, &reload_data);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::HResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreate__.foo";
    char host_name[512];
    const char* path = dmTestUtil::MakeHostPathf(host_name, sizeof(host_name), "%s/%s", TMP_DIR, resource_name);

    FILE* f;

    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);
    ASSERT_EQ(123, *resource);

    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);
    ASSERT_EQ(456, *resource);

    ASSERT_EQ(123, reload_data.m_Old);
    ASSERT_EQ(456, reload_data.m_New);

    dmSys::Unlink(path);
    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::UnregisterResourceReloadedCallback(factory, ResourceReloadedCallback, &reload_data);
    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

struct ReloadedContext
{
    void*           m_Resource;
    int32_atomic_t* m_Reloaded;
};

static void SendReloadThread(void* _ctx)
{
    ReloadedContext* ctx = (ReloadedContext*)_ctx;

    uint32_t msg_size = sizeof(dmResourceDDF::Reload) + sizeof(uintptr_t) + (strlen("__testrecreate__.foo") + 1);
    dmResourceDDF::Reload* reload_resources = (dmResourceDDF::Reload*) malloc(msg_size);
    memset(reload_resources, 0x0, msg_size);
    reload_resources->m_Resources.m_Count = 1;
    uintptr_t str_ofs_offset = 2 * sizeof(uintptr_t); //
    uintptr_t str_offset = str_ofs_offset + reload_resources->m_Resources.m_Count * sizeof(uintptr_t);//0x18;
    memcpy((uint8_t*)reload_resources, &str_ofs_offset, sizeof(uintptr_t)); // offset to path string offsets
    memcpy((uint8_t*)reload_resources + str_ofs_offset, &str_offset, sizeof(uintptr_t)); // offset to start of resource path string
    memcpy((uint8_t*)(reload_resources) + str_offset, "__testrecreate__.foo", strlen("__testrecreate__.foo") + 1); // the actual resource path

    dmMessage::URL url;
    url.m_Fragment = 0;
    url.m_Path = 0;
    dmMessage::Result result;
    result = dmMessage::GetSocket("@resource", &url.m_Socket);
    assert(result == dmMessage::RESULT_OK);
    result = dmMessage::Post(0, &url, dmResourceDDF::Reload::m_DDFHash, 0, (uintptr_t) dmResourceDDF::Reload::m_DDFDescriptor, reload_resources, msg_size, 0);
    assert(result == dmMessage::RESULT_OK);

    if (ctx)
    {
        dmAtomicStore32(ctx->m_Reloaded, 1);
    }
    free(reload_resources);
}

static void ResourceReloadedHttpCallback(const dmResource::ResourceReloadedParams* params)
{
    ReloadedContext* ctx = (ReloadedContext*)params->m_UserData;
    if (ResourceDescriptorGetResource(params->m_Resource) == ctx->m_Resource)
    {
        dmAtomicStore32(ctx->m_Reloaded, 1);
    }
}

static dmThread::Thread SendReload(ReloadedContext* context)
{
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    SendReloadThread(context);
    return 0;
#else
    return dmThread::New(&SendReloadThread, 0x8000, context, "reload");
#endif
}

// Verify queued reloads and missing-resource callbacks, including on single-thread web builds.
TEST(RecreateTest, RecreateTestHttp)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::HResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreate__.foo";
    char host_name[512];
    const char* path = dmTestUtil::MakeHostPathf(host_name, sizeof(host_name), "%s/%s", TMP_DIR, resource_name);

    FILE* f;

    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);
    ASSERT_EQ(123, *resource);

    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    int32_atomic_t send_reload_done = 0 ;

    ReloadedContext state;
    state.m_Resource = resource;
    state.m_Reloaded = &send_reload_done;
    dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedHttpCallback, &state);

    dmThread::Thread send_thread = SendReload(0);

    uint64_t t_start = dmTime::GetMonotonicTime();
    do
    {
        dmTime::Sleep(1000 * 10);
        dmResource::UpdateFactory(factory);

        uint64_t t = dmTime::GetMonotonicTime();
        if ((t - t_start) >= 2 * 1000000)
        {
            ASSERT_TRUE(false && "Test timed out");
            break;
        }
    } while (!dmAtomicGet32(&send_reload_done));

    if (send_thread)
        dmThread::Join(send_thread);

    ASSERT_EQ(456, *resource);

    dmResource::UnregisterResourceReloadedCallback(factory, ResourceReloadedHttpCallback, resource);

    dmSys::Unlink(host_name);

    send_reload_done = 0;
    send_thread = SendReload(&state);

    do
    {
        dmTime::Sleep(1000 * 10);
        dmResource::UpdateFactory(factory);
    } while (!dmAtomicGet32(&send_reload_done));

    if (send_thread)
        dmThread::Join(send_thread);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

// Test the "filename" callback argument

char filename_resource_filename[ 128 ];

dmResource::Result FilenameResourceCreate(const dmResource::ResourceCreateParams* params)
{
    if (strcmp(filename_resource_filename, params->m_Filename) == 0)
        return dmResource::RESULT_OK;
    else
        return dmResource::RESULT_FORMAT_ERROR;
}

dmResource::Result FilenameResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    return dmResource::RESULT_OK;
}

dmResource::Result FilenameResourceRecreate(const dmResource::ResourceRecreateParams* params)
{
    if (strcmp(filename_resource_filename, params->m_Filename) == 0)
        return dmResource::RESULT_OK;
    else
        return dmResource::RESULT_FORMAT_ERROR;
}

TEST(FilenameTest, FilenameTest)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::HResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testfilename__.foo";
    const char* path = dmTestUtil::MakeHostPathf(filename_resource_filename, sizeof(filename_resource_filename), "%s/%s", TMP_DIR, resource_name);

    FILE* f;

    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
    ASSERT_EQ(dmResource::RESULT_OK, fr);
    ASSERT_EQ(123, *resource);

    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);
    ASSERT_EQ(456, *resource);

    dmSys::Unlink(path);
    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}



static ResourceResult RegisterResourceTypeCustom(HResourceTypeContext ctx, HResourceType type)
{
    int* context = new int;
    *context = 1;

    // we're not actually invoking them, they just need to be non null
    ResourceTypeSetCreateFn(type, (FResourceCreate)1);
    ResourceTypeSetDestroyFn(type, (FResourceDestroy)1);
    ResourceTypeSetContext(type, context);


    int* counter = (int*)ResourceTypeContextGetContextByHash(ctx, dmHashString64("counter"));
    assert(counter != 0);
    (*counter)++;

    return RESOURCE_RESULT_OK;
}

static ResourceResult DeregisterResourceTypeCustom(HResourceTypeContext ctx, HResourceType type)
{
    int* context = (int*)ResourceTypeGetContext(type);
    assert(context != 0);
    assert((*context) == 1);
    delete context;

    int* counter = (int*)ResourceTypeContextGetContextByHash(ctx, dmHashString64("counter"));
    assert(counter != 0);
    (*counter)++;

    return RESOURCE_RESULT_OK;
}

TEST(ResourceExtension, CustomType)
{
    uint8_t DM_ALIGNED(16) desc[dmResource::s_ResourceTypeCreatorDescBufferSize];

    ResourceRegisterTypeCreatorDesc((void*)desc, sizeof(desc), "custom", RegisterResourceTypeCustom, DeregisterResourceTypeCustom);

    dmResource::Result e;
    dmResource::NewFactoryParams params;
    dmResource::HFactory factory = dmResource::NewFactory(&params, ".");
    ASSERT_NE((void*)0, factory);

    dmResource::HResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "custom", &type);
    ASSERT_EQ(dmResource::RESULT_UNKNOWN_RESOURCE_TYPE, e);

    dmHashTable64<void*> contexts;
    contexts.SetCapacity(7, 14);

    int counter = 0;
    contexts.Put(dmHashString64("counter"), &counter);

    dmResource::RegisterTypes(factory, &contexts);
    ASSERT_EQ(1u, contexts.Size());
    ASSERT_EQ(1u, counter);

    dmResource::DeregisterTypes(factory, &contexts);
    ASSERT_EQ(1u, contexts.Size());
    ASSERT_EQ(2u, counter);

    dmResource::DeleteFactory(factory);
}

struct CallbackUserData
{
    CallbackUserData() : m_Descriptor(0x0), m_Name(0) {}
    ResourceDescriptor* m_Descriptor;
    dmhash_t            m_Name;
};

void ReloadCallback(const dmResource::ResourceReloadedParams* params)
{
    CallbackUserData* data = (CallbackUserData*)params->m_UserData;
    HResourceDescriptor rd = params->m_Resource;
    data->m_Descriptor = rd;
    data->m_Name = ResourceDescriptorGetNameHash(rd);
}

TEST(RecreateTest, ReloadCallbackTest)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreate__.foo";
    char host_name[512];
    const char* path = dmTestUtil::MakeHostPathf(host_name, sizeof(host_name), "%s/%s", TMP_DIR, resource_name);

    FILE* f;

    f = fopen(path, "wb");
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
    ASSERT_EQ(dmHashString64(resource_name), user_data.m_Name);

    user_data = CallbackUserData();
    dmResource::UnregisterResourceReloadedCallback(factory, ReloadCallback, &user_data);

    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_EQ((void*)0, user_data.m_Descriptor);
    ASSERT_EQ(0, user_data.m_Name);

    dmSys::Unlink(path);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

TEST(OverflowTest, OverflowTest)
{
    const char* test_dir = MOUNT_DIR "/build/src/test";

    dmResource::NewFactoryParams params;
    params.m_MaxResources = 1;
    dmResource::HFactory factory = dmResource::NewFactory(&params, test_dir);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
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
    const char* test_dir = "build/src/test";
    for (uint32_t max=0;max<5;max++)
    {
        // recreate with new settings
        dmResource::DeleteFactory(m_Factory);
        dmResource::NewFactoryParams params;
        params.m_MaxResources = max;
        m_Factory = dmResource::NewFactory(&params, test_dir);
        ASSERT_NE((void*) 0, m_Factory);

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "foo", this, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
        ASSERT_EQ(dmResource::RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "cont", this, &ResourceContainerPreload, &ResourceContainerCreate, 0, &ResourceContainerDestroy, 0);
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


TEST_F(ResourceTest, ManifestLoadDdfFail)
{
    const char* buf = "this is not a manifest buffer";
    dmResource::Manifest* manifest;
    dmResource::Result result = dmResource::LoadManifestFromBuffer((uint8_t*)buf, strlen(buf), &manifest);
    ASSERT_EQ(dmResource::RESULT_DDF_ERROR, result);
}

TEST_F(ResourceTest, ManifestBundledResourcesVerification)
{
    dmResource::Manifest* manifest;
    dmResource::Result result = dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    dmResourceArchive::ArchiveIndexContainer* archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer(RESOURCES_ARCI, RESOURCES_ARCI_SIZE, true, RESOURCES_ARCD, RESOURCES_ARCD_SIZE, true, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);

    dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
    uint32_t hash_len = dmResource::HashLength(algorithm);

    result = dmResource::VerifyResourcesBundled(manifest->m_DDFData->m_Resources.m_Data, manifest->m_DDFData->m_Resources.m_Count, hash_len, archive);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    dmResourceArchive::Delete(archive);
    dmResource::DeleteManifest(manifest);
}

TEST_F(ResourceTest, ManifestBundledResourcesVerificationFail)
{
    dmResource::Manifest* manifest;
    dmResource::Result result = dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    dmResourceArchive::ArchiveIndexContainer* archive = 0;
    dmResourceArchive::Result r = dmResourceArchive::WrapArchiveBuffer(RESOURCES_ARCI, RESOURCES_ARCI_SIZE, true, RESOURCES_ARCD, RESOURCES_ARCD_SIZE, true, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, r);

    // Deep-copy current manifest resource entries with space for an extra resource entry
    uint32_t entry_count = manifest->m_DDFData->m_Resources.m_Count;
    dmLiveUpdateDDF::ResourceEntry* entries = (dmLiveUpdateDDF::ResourceEntry*) malloc((entry_count + 1) * sizeof(dmLiveUpdateDDF::ResourceEntry));
    memcpy(entries, manifest->m_DDFData->m_Resources.m_Data, entry_count);
    for (uint32_t i = 0; i < entry_count; ++i)
    {
        dmLiveUpdateDDF::ResourceEntry* entry = &manifest->m_DDFData->m_Resources.m_Data[i];
        dmLiveUpdateDDF::ResourceEntry* new_entry = &entries[i];
        new_entry->m_Hash = dmLiveUpdateDDF::HashDigest();
        new_entry->m_Hash.m_Data.m_Data = (uint8_t*)malloc(entry->m_Hash.m_Data.m_Count);
        memcpy(new_entry->m_Hash.m_Data.m_Data, entry->m_Hash.m_Data.m_Data, entry->m_Hash.m_Data.m_Count);
        new_entry->m_Flags = entry->m_Flags;
    }

    // Fill in bogus resource entry, tagged as BUNDLED but will not be found in the archive
    entries[entry_count].m_Flags = dmLiveUpdateDDF::BUNDLED;
    entries[entry_count].m_Hash.m_Data.m_Data = (uint8_t*)malloc(manifest->m_DDFData->m_Resources.m_Data[0].m_Hash.m_Data.m_Count);
    memset(entries[entry_count].m_Hash.m_Data.m_Data, 0xFF, manifest->m_DDFData->m_Resources.m_Data[0].m_Hash.m_Data.m_Count);
    entries[entry_count].m_Url = "not_in_bundle";

    dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
    uint32_t hash_len = dmResource::HashLength(algorithm);

    result = dmResource::VerifyResourcesBundled(entries, manifest->m_DDFData->m_Resources.m_Count+1, hash_len, archive);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, result);

    // Clean up deep-copied resource entries
    for (uint32_t i = 0; i < entry_count + 1; ++i)
    {
        dmLiveUpdateDDF::ResourceEntry* e = &entries[i];
        free(e->m_Hash.m_Data.m_Data);
    }
    free(entries);

    dmResourceArchive::Delete(archive);
    dmResource::DeleteManifest(manifest);
}

TEST(ResourceUtil, HexDigestLength)
{
    uint32_t actual = 0;

    actual = dmResource::HashLength(dmLiveUpdateDDF::HASH_MD5);
    ASSERT_EQ((128U / 8U), actual);

    actual = dmResource::HashLength(dmLiveUpdateDDF::HASH_SHA1);
    ASSERT_EQ((160U / 8U), actual);

    actual = dmResource::HashLength(dmLiveUpdateDDF::HASH_SHA256);
    ASSERT_EQ((256U / 8U), actual);

    actual = dmResource::HashLength(dmLiveUpdateDDF::HASH_SHA512);
    ASSERT_EQ((512U / 8U), actual);
}

TEST(ResourceUtil, BytesToHexString)
{
    uint8_t instance[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};

    char buffer_short[6];
    dmResource::BytesToHexString(instance, dmResource::HashLength(dmLiveUpdateDDF::HASH_MD5), buffer_short, 6);
    ASSERT_STREQ("00010", buffer_short);

    char buffer_fitted[33];
    dmResource::BytesToHexString(instance, dmResource::HashLength(dmLiveUpdateDDF::HASH_MD5), buffer_fitted, 33);
    ASSERT_STREQ("000102030405060708090a0b0c0d0e0f", buffer_fitted);

    char buffer_long[513];
    dmResource::BytesToHexString(instance, dmResource::HashLength(dmLiveUpdateDDF::HASH_MD5), buffer_long, 513);
    ASSERT_STREQ("000102030405060708090a0b0c0d0e0f", buffer_long);
}

// *********************************************************************************************************
// Streaming support

struct StreamTestResource
{
    uint32_t m_BufferSize;
    uint32_t m_ResourceSize;
    bool     m_IsBufferPartial;
    uint8_t* m_Data;

    const char* m_Path;
    uint32_t    m_Offset;
    uint32_t    m_ChunkSize;
};

static int StreamingPreloadCallback(dmResource::HFactory factory, void* cbk_ctx, HResourceDescriptor rd, uint32_t offset, uint32_t nread, uint8_t* buffer)
{
    StreamTestResource* resource = (StreamTestResource*)cbk_ctx;
    uint32_t newsize = offset + nread;
    resource->m_Data = (uint8_t*)realloc(resource->m_Data, newsize);

    memcpy(resource->m_Data + offset, buffer, nread);
    resource->m_Offset = newsize;

    if (resource->m_Offset < resource->m_ResourceSize)
    {
        dmResource::PreloadData(factory, resource->m_Path, resource->m_Offset, resource->m_ChunkSize, StreamingPreloadCallback, (void*)resource);
    }
    return 1;
}

static dmResource::Result StreamResourceCreate(const dmResource::ResourceCreateParams* params)
{
    StreamTestResource* resource = new StreamTestResource;
    resource->m_BufferSize = params->m_BufferSize;
    resource->m_ResourceSize = params->m_FileSize;
    resource->m_IsBufferPartial = params->m_IsBufferPartial;
    resource->m_Data = (uint8_t*)malloc(resource->m_BufferSize);
    resource->m_Path = strdup(params->m_Filename);
    memcpy(resource->m_Data, params->m_Buffer, resource->m_BufferSize);

    dmResource::SetResource(params->m_Resource, resource);
    dmResource::SetResourceSize(params->m_Resource, params->m_FileSize);

    if (params->m_IsBufferPartial)
    {
        resource->m_Offset    = resource->m_BufferSize;
        resource->m_ChunkSize = resource->m_BufferSize;
        dmResource::PreloadData(params->m_Factory, resource->m_Path, resource->m_Offset, resource->m_ChunkSize, StreamingPreloadCallback, (void*)resource);
    }
    return dmResource::RESULT_OK;
}

static dmResource::Result StreamResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    StreamTestResource* resource = (StreamTestResource*)dmResource::GetResource(params->m_Resource);
    free((void*)resource->m_Data);
    free((void*)resource->m_Path);
    delete resource;
    return dmResource::RESULT_OK;
}


TEST_F(ResourceTest, PartialReadTest)
{
    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &StreamResourceCreate, 0, &StreamResourceDestroy, 0);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::HResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "foo", &type);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__teststreaming__.foo";
    const char* path = dmTestUtil::MakeHostPathf(filename_resource_filename, sizeof(filename_resource_filename), "%s/%s", TMP_DIR, resource_name);

    // Write test data
    uint8_t expected_data[256];
    uint32_t expected_data_len = sizeof(expected_data);
    for (uint32_t i = 0; i < expected_data_len; ++i)
    {
        expected_data[i] = uint8_t(i % 0xFF);
    }

    {
        FILE* f = fopen(path, "wb");
        ASSERT_NE((FILE*) 0, f);
        fwrite(expected_data, 1, expected_data_len, f);
        fclose(f);
    }

    // Currently no streaming enabled
    {
        StreamTestResource* resource;
        dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);
        ASSERT_EQ(dmResource::RESULT_OK, fr);
        ASSERT_EQ(expected_data_len, resource->m_ResourceSize);
        ASSERT_EQ(expected_data_len, resource->m_BufferSize);
        ASSERT_EQ(false, resource->m_IsBufferPartial);
        ASSERT_ARRAY_EQ_LEN(expected_data, resource->m_Data, expected_data_len);

        dmResource::Release(factory, resource);
    }

    // Enable streaming
    uint32_t preload_size = 14;
    ResourceTypeSetStreaming(type, preload_size);

    {
        // Get the first preload data chunk
        StreamTestResource* resource;
        dmResource::Result fr = dmResource::Get(factory, resource_name, (void**) &resource);

        ASSERT_EQ(dmResource::RESULT_OK, fr);
        ASSERT_EQ(expected_data_len, resource->m_ResourceSize);
        ASSERT_EQ(preload_size, resource->m_BufferSize);
        ASSERT_EQ(true, resource->m_IsBufferPartial);
        ASSERT_ARRAY_EQ_LEN(expected_data, resource->m_Data, preload_size);

        uint64_t tstart = dmTime::GetMonotonicTime();
        uint32_t total_size = preload_size;
        while (total_size < expected_data_len)
        {
            uint64_t tend = dmTime::GetMonotonicTime();
            if ((tend - tstart) > 2 * 1000000)
            {
                dmLogError("Timeout!");
                ASSERT_TRUE(false);
            }

            JobSystemUpdate(m_JobContext, 2000); // pump the results from the job thread to the main thread

            ASSERT_ARRAY_EQ_LEN(expected_data, resource->m_Data, resource->m_Offset);

            total_size = resource->m_Offset;
        }

        dmResource::Release(factory, resource);
    }

    dmSys::Unlink(path);
}

#if defined(DM_HAS_THREADS)

static const uint8_t STREAM_LOCK_TEST_DATA[] = { 1, 2, 3, 4 };

struct StreamReadLockProbe
{
    dmMutex::HMutex                         m_Mutex;
    dmConditionVariable::HConditionVariable m_Condition;
    bool                                   m_Reading;
    bool                                   m_Resume;
    bool                                   m_Finished;
};

static dmResourceProvider::Result StreamLockGetFileSize(dmResourceProvider::HArchiveInternal, dmhash_t, const char*, uint32_t* file_size)
{
    *file_size = sizeof(STREAM_LOCK_TEST_DATA);
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result StreamLockReadPartial(dmResourceProvider::HArchiveInternal internal, dmhash_t, const char*, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
{
    StreamReadLockProbe* probe = (StreamReadLockProbe*)internal;
    {
        dmMutex::ScopedLock lock(probe->m_Mutex);
        probe->m_Reading = true;
        dmConditionVariable::Signal(probe->m_Condition);
        while (!probe->m_Resume)
        {
            dmConditionVariable::Wait(probe->m_Condition, probe->m_Mutex);
        }
    }
    memcpy(buffer, STREAM_LOCK_TEST_DATA + offset, size);
    *nread = size;
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result StreamLockUnmount(dmResourceProvider::HArchiveInternal)
{
    // The fixture owns the probe until after the factory and worker are destroyed.
    return dmResourceProvider::RESULT_OK;
}

static int StreamLockWorkerFinished(HJobContext, HJob, void* context, void*)
{
    StreamReadLockProbe* probe = (StreamReadLockProbe*)context;
    dmMutex::ScopedLock lock(probe->m_Mutex);
    probe->m_Finished = true;
    dmConditionVariable::Signal(probe->m_Condition);
    return 0;
}

class StreamLoadMutexTest : public ResourceTest
{
protected:
    void SetUp() override
    {
        ResourceTest::SetUp();
        memset(&m_Probe, 0, sizeof(m_Probe));
        m_Probe.m_Mutex = dmMutex::New();
        m_Probe.m_Condition = dmConditionVariable::New();

        memset(&m_Loader, 0, sizeof(m_Loader));
        m_Loader.m_GetFileSize = StreamLockGetFileSize;
        m_Loader.m_ReadFilePartial = StreamLockReadPartial;
        m_Loader.m_Unmount = StreamLockUnmount;

        dmResourceProvider::HArchive archive;
        ASSERT_EQ(dmResourceProvider::RESULT_OK, dmResourceProvider::CreateMount(&m_Loader, &m_Probe, &archive));
        ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::AddMount(dmResource::GetMountsContext(factory), dmHashString64("stream_lock_probe"), archive, 10));
    }

    void TearDown() override
    {
        ResourceTest::TearDown();
        dmConditionVariable::Delete(m_Probe.m_Condition);
        dmMutex::Delete(m_Probe.m_Mutex);
    }

    StreamReadLockProbe              m_Probe;
    dmResourceProvider::ArchiveLoader m_Loader;
};

// Verifies a streamed read leaves the factory load mutex available, preventing texture upload deadlocks.
TEST_F(StreamLoadMutexTest, ReadDoesNotHoldLoadMutex)
{
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(factory, "foo", 0, 0, StreamResourceCreate, 0, StreamResourceDestroy, 0));
    const char* path = "/stream_lock.foo";
    StreamTestResource* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(factory, path, (void*)STREAM_LOCK_TEST_DATA, sizeof(STREAM_LOCK_TEST_DATA), (void**)&resource));
    resource->m_Offset = 0;
    memset(resource->m_Data, 0, sizeof(STREAM_LOCK_TEST_DATA));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::PreloadData(factory, path, 0, sizeof(STREAM_LOCK_TEST_DATA), StreamingPreloadCallback, resource));

    Job finished = { 0 };
    finished.m_Process = StreamLockWorkerFinished;
    finished.m_Context = &m_Probe;
    JobSystemPushJob(m_JobContext, JobSystemCreateJob(m_JobContext, &finished));

    {
        dmMutex::ScopedLock lock(m_Probe.m_Mutex);
        while (!m_Probe.m_Reading)
        {
            dmConditionVariable::Wait(m_Probe.m_Condition, m_Probe.m_Mutex);
        }
    }

    // The provider is paused on the worker; the main thread must be able to acquire the load mutex.
    dmMutex::HMutex load_mutex = dmResource::GetLoadMutex(factory);
    bool load_mutex_available = dmMutex::TryLock(load_mutex);
    if (load_mutex_available)
    {
        dmMutex::Unlock(load_mutex);
    }

    {
        dmMutex::ScopedLock lock(m_Probe.m_Mutex);
        m_Probe.m_Resume = true;
        dmConditionVariable::Signal(m_Probe.m_Condition);
        while (!m_Probe.m_Finished)
        {
            dmConditionVariable::Wait(m_Probe.m_Condition, m_Probe.m_Mutex);
        }
    }

    // The worker marker guarantees the stream completion is ready for this update.
    JobSystemUpdate(m_JobContext, 0);
    ASSERT_EQ(sizeof(STREAM_LOCK_TEST_DATA), resource->m_Offset);
    ASSERT_ARRAY_EQ_LEN(STREAM_LOCK_TEST_DATA, resource->m_Data, sizeof(STREAM_LOCK_TEST_DATA));
    dmResource::Release(factory, resource);
    ASSERT_TRUE(load_mutex_available);
}

#endif

static dmArray<char> g_ResourceProfileOutput;

static void CaptureResourceProfile(LogSeverity severity, const char* domain, const char* text)
{
    if (strcmp(domain, "RESOURCE") != 0 || !strstr(text, " took ") || !strstr(text, " (pre: "))
        return;
    uint32_t length = (uint32_t)strlen(text);
    if (g_ResourceProfileOutput.Remaining() < length + 1)
        g_ResourceProfileOutput.OffsetCapacity(length + 1);
    g_ResourceProfileOutput.PushArray(text, length);
}

class ResourceProfileTest : public jc_test_base_class
{
    public:
    void SetUp() override
    {
        m_Factory = 0;
        m_Preloader = 0;
        m_AllowPostCreate = true;
        m_AddDependency = false;
        m_PostCreateCount = 0;
        m_PendingPostCreateCalls = 0;
        m_PostCreateHasFilename = true;
        m_PreloadResult = dmResource::RESULT_OK;
        m_CreateResult = dmResource::RESULT_OK;
        m_PostCreateResult = dmResource::RESULT_OK;
        JobSystemCreateParams job_params = { 0 };
        job_params.m_ThreadCount = 1;
        m_JobContext = JobSystemCreate(&job_params);
        ASSERT_TRUE(FlushLogs());
        m_LogLevel = dmLogGetLevel();
        dmLogSetLevel(LOG_SEVERITY_INFO);
        g_ResourceProfileOutput.SetSize(0);
        dmLogRegisterListener(CaptureResourceProfile);
    }

    void TearDown() override
    {
        m_AllowPostCreate = true;
        if (m_Preloader)
            dmResource::DeletePreloader(m_Preloader);
        if (m_Factory)
            dmResource::DeleteFactory(m_Factory);
        JobSystemDestroy(m_JobContext);
        ASSERT_TRUE(FlushLogs());
        dmLogUnregisterListener(CaptureResourceProfile);
        dmLogSetLevel(m_LogLevel);
    }

    void CreateFactory(bool enabled, uint32_t max_resources = 1024, bool post_create = true)
    {
        dmResource::NewFactoryParams params;
        params.m_Flags = enabled ? RESOURCE_FACTORY_FLAGS_PROFILE : 0;
        params.m_MaxResources = max_resources;
        params.m_JobThreadContext = m_JobContext;
        m_Factory = dmResource::NewFactory(&params, "build/src/test");
        ASSERT_NE((void*)0, m_Factory);
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(m_Factory, "foo", this, Preload, Create, post_create ? PostCreate : 0, Destroy, 0));
    }

    static dmResource::Result Preload(const dmResource::ResourcePreloadParams* params)
    {
        ResourceProfileTest* self = (ResourceProfileTest*)params->m_Context;
        if (self->m_AddDependency && strcmp(params->m_Filename, "/test02.foo") == 0)
            dmResource::PreloadHint(params->m_HintInfo, "/test01.foo");
        return self->m_PreloadResult;
    }

    static dmResource::Result Create(const dmResource::ResourceCreateParams* params)
    {
        ResourceProfileTest* self = (ResourceProfileTest*)params->m_Context;
        if (self->m_CreateResult != dmResource::RESULT_OK)
            return self->m_CreateResult;
        ResourceDescriptorSetResource(params->m_Resource, new uint32_t(42));
        return dmResource::RESULT_OK;
    }

    static dmResource::Result PostCreate(const dmResource::ResourcePostCreateParams* params)
    {
        ResourceProfileTest* self = (ResourceProfileTest*)params->m_Context;
        ++self->m_PostCreateCount;
        self->m_PostCreateHasFilename = self->m_PostCreateHasFilename && params->m_Filename && strcmp(params->m_Filename, "/test01.foo") == 0;
        return self->m_AllowPostCreate && self->m_PostCreateCount > self->m_PendingPostCreateCalls ? self->m_PostCreateResult : dmResource::RESULT_PENDING;
    }

    static dmResource::Result Destroy(const dmResource::ResourceDestroyParams* params)
    {
        delete (uint32_t*)ResourceDescriptorGetResource(params->m_Resource);
        return dmResource::RESULT_OK;
    }

    static bool FlushLogs()
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (dmLog::GetPendingLogCount() && dmTime::GetMonotonicTime() < deadline)
        {
        }
        return dmLog::GetPendingLogCount() == 0;
    }

    const char* Output()
    {
        if (g_ResourceProfileOutput.Full())
            g_ResourceProfileOutput.OffsetCapacity(1);
        g_ResourceProfileOutput.Begin()[g_ResourceProfileOutput.Size()] = 0;
        return g_ResourceProfileOutput.Begin();
    }

    dmResource::Result CompleteLoad()
    {
        uint64_t           deadline = dmTime::GetMonotonicTime() + 5000000;
        dmResource::Result result;
        do
        {
            result = dmResource::UpdatePreloader(m_Preloader, 0, 0, 1000);
        } while (result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
        return result;
    }

    static bool CaptureTimings(const dmResource::IteratorResource& resource, void* context)
    {
        dmResource::IteratorResource* result = (dmResource::IteratorResource*)context;
        if (resource.m_Id != result->m_Id)
            return true;
        *result = resource;
        return false;
    }

    dmResource::LoadTimings GetTimings(const char* path)
    {
        dmResource::IteratorResource resource = {};
        resource.m_Id = dmHashString64(path);
        dmResource::IterateResources(m_Factory, CaptureTimings, &resource);
        EXPECT_GT(resource.m_RefCount, 0u);
        return resource.m_LoadTimings;
    }

    void AssertLoggedTimings(const char* path)
    {
        dmResource::LoadTimings timings = GetTimings(path);
        ASSERT_TRUE(timings.m_Recorded);
        ASSERT_EQ(timings.m_Total, timings.m_Preload + timings.m_Read + timings.m_Create + timings.m_PostCreate + timings.m_Queue + timings.m_Wait);
        ASSERT_TRUE(FlushLogs());
        const char* record = strstr(Output(), path);
        ASSERT_NE((const char*)0, record);
        double   total, preload, read, create, post_create, queue, wait;
        uint32_t count;
        ASSERT_EQ(8, sscanf(record + strlen(path), " took %lfms (pre: %lfms, read: %lfms, create: %lfms, post: %lfms [%u calls], queue: %lfms, wait: %lfms", &total, &preload, &read, &create, &post_create, &count, &queue, &wait));
        ASSERT_EQ(timings.m_Total, (uint64_t)(total * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_Preload, (uint64_t)(preload * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_Read, (uint64_t)(read * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_Create, (uint64_t)(create * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_PostCreate, (uint64_t)(post_create * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_Queue, (uint64_t)(queue * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_Wait, (uint64_t)(wait * 1000.0 + 0.5));
        ASSERT_EQ(timings.m_PostCreateCount, count);
    }

    dmResource::HFactory   m_Factory;
    dmResource::HPreloader m_Preloader;
    HJobContext            m_JobContext;
    bool                   m_AllowPostCreate;
    bool                   m_AddDependency;
    bool                   m_PostCreateHasFilename;
    uint32_t               m_PostCreateCount;
    uint32_t               m_PendingPostCreateCalls;
    LogSeverity            m_LogLevel;
    dmResource::Result     m_PreloadResult;
    dmResource::Result     m_CreateResult;
    dmResource::Result     m_PostCreateResult;
};

// Loading without resource profiling must not emit timing records on either loading path.
TEST_F(ResourceProfileTest, Disabled)
{
    CreateFactory(false);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    ASSERT_FALSE(GetTimings("/test01.foo").m_Recorded);
    dmResource::Release(m_Factory, resource);
    m_Preloader = dmResource::NewPreloader(m_Factory, "/test01.foo");
    ASSERT_EQ(dmResource::RESULT_OK, CompleteLoad());
    ASSERT_FALSE(GetTimings("/test01.foo").m_Recorded);
    ASSERT_TRUE(FlushLogs());
    ASSERT_STREQ("", Output());
}

// A synchronous load reports all stages once, and subsequent cache hits do not report another load.
TEST_F(ResourceProfileTest, Synchronous)
{
    CreateFactory(true);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    AssertLoggedTimings("/test01.foo");
    void* cached = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &cached));
    ASSERT_EQ(resource, cached);
    AssertLoggedTimings("/test01.foo");
    dmResource::Release(m_Factory, cached);
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    const char* output = Output();
    const char* record = strstr(output, "/test01.foo took ");
    ASSERT_NE((void*)0, record);
    ASSERT_NE((void*)0, strstr(output, "INFO:RESOURCE: /test01.foo took "));
    ASSERT_EQ((void*)0, strstr(record + 1, "INFO:RESOURCE: "));
    ASSERT_NE((void*)0, strstr(output, "queue: "));
    ASSERT_NE((void*)0, strstr(output, "read: "));
    ASSERT_NE((void*)0, strstr(output, "pre: "));
    ASSERT_NE((void*)0, strstr(output, "wait: "));
    ASSERT_NE((void*)0, strstr(output, "create: "));
    ASSERT_NE((void*)0, strstr(output, "post: "));
    ASSERT_NE((void*)0, strstr(output, "[1 calls]"));
    ASSERT_NE((void*)0, strstr(output, "result: OK"));
}

// Profile logs follow the normal log-level filter even when resource profiling is enabled.
TEST_F(ResourceProfileTest, LogLevel)
{
    CreateFactory(true);
    dmLogSetLevel(LOG_SEVERITY_WARNING);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    ASSERT_TRUE(GetTimings("/test01.foo").m_Recorded);
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    ASSERT_STREQ("", Output());
}

// Creating from supplied bytes reports the callbacks with no queue or file-read time.
TEST_F(ResourceProfileTest, SuppliedBytes)
{
    CreateFactory(true);
    uint32_t data = 42;
    void*    resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_Factory, "/test01.foo", &data, sizeof(data), &resource));
    AssertLoggedTimings("/test01.foo");
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    ASSERT_NE((void*)0, strstr(Output(), "/test01.foo took "));
    ASSERT_NE((void*)0, strstr(Output(), "queue: 0.000ms"));
    ASSERT_NE((void*)0, strstr(Output(), "read: 0.000ms"));
    ASSERT_NE((void*)0, strstr(Output(), "[1 calls]"));
    ASSERT_NE((void*)0, strstr(Output(), "result: OK"));
}

// Synchronous PostCreate retries accumulate into one terminal record.
TEST_F(ResourceProfileTest, SynchronousPendingPostCreate)
{
    CreateFactory(true);
    m_PendingPostCreateCalls = 2;
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    AssertLoggedTimings("/test01.foo");
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    ASSERT_EQ(3u, m_PostCreateCount);
    const char* record = strstr(Output(), "/test01.foo took ");
    ASSERT_NE((void*)0, record);
    ASSERT_EQ((void*)0, strstr(record + 1, "INFO:RESOURCE: "));
    ASSERT_NE((void*)0, strstr(record, "[3 calls]"));
    ASSERT_NE((void*)0, strstr(record, "result: OK"));
}

// Preloading an already resident resource must not emit a new loading record.
TEST_F(ResourceProfileTest, AsynchronousCacheHit)
{
    CreateFactory(true);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    dmResource::LoadTimings original = GetTimings("/test01.foo");
    ASSERT_TRUE(FlushLogs());
    g_ResourceProfileOutput.SetSize(0);
    m_Preloader = dmResource::NewPreloader(m_Factory, "/test01.foo");
    ASSERT_EQ(dmResource::RESULT_OK, CompleteLoad());
    dmResource::LoadTimings cached = GetTimings("/test01.foo");
    ASSERT_EQ(original.m_Total, cached.m_Total);
    ASSERT_EQ(original.m_Preload, cached.m_Preload);
    ASSERT_EQ(original.m_Read, cached.m_Read);
    ASSERT_EQ(original.m_Create, cached.m_Create);
    ASSERT_EQ(original.m_PostCreate, cached.m_PostCreate);
    ASSERT_EQ(original.m_Queue, cached.m_Queue);
    ASSERT_EQ(original.m_Wait, cached.m_Wait);
    ASSERT_EQ(original.m_PostCreateCount, cached.m_PostCreateCount);
    ASSERT_EQ(original.m_Recorded, cached.m_Recorded);
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    ASSERT_STREQ("", Output());
}

// Pending PostCreate callbacks retain the request timing and defer the single record until completion.
TEST_F(ResourceProfileTest, PendingPostCreate)
{
    CreateFactory(true);
    m_AllowPostCreate = false;
    m_Preloader = dmResource::NewPreloader(m_Factory, "/test01.foo");
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_PostCreateCount && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(m_Preloader, 0, 0, 1000));
    ASSERT_GT(m_PostCreateCount, 0u);
    ASSERT_FALSE(GetTimings("/test01.foo").m_Recorded);
    ASSERT_TRUE(FlushLogs());
    ASSERT_STREQ("", Output());

    m_AllowPostCreate = true;
    ASSERT_EQ(dmResource::RESULT_OK, CompleteLoad());
    AssertLoggedTimings("/test01.foo");
    ASSERT_TRUE(FlushLogs());
    ASSERT_TRUE(m_PostCreateHasFilename);
    ASSERT_GT(m_PostCreateCount, 1u);
    char count[64];
    dmSnPrintf(count, sizeof(count), "[%u calls]", m_PostCreateCount);
    const char* output = Output();
    const char* record = strstr(output, "/test01.foo took ");
    ASSERT_NE((void*)0, record);
    ASSERT_EQ((void*)0, strstr(record + 1, "INFO:RESOURCE: "));
    ASSERT_NE((void*)0, strstr(output, count));
    ASSERT_NE((void*)0, strstr(output, "result: OK"));
}

// Dependency requests and the parent retain independent timing records even when request slots are pruned.
TEST_F(ResourceProfileTest, Dependencies)
{
    CreateFactory(true);
    m_AddDependency = true;
    m_Preloader = dmResource::NewPreloader(m_Factory, "/test02.foo");
    ASSERT_EQ(dmResource::RESULT_OK, CompleteLoad());
    AssertLoggedTimings("/test02.foo");
    ASSERT_TRUE(FlushLogs());
    const char* output = Output();
    ASSERT_NE((void*)0, strstr(output, "/test01.foo took "));
    ASSERT_NE((void*)0, strstr(output, "/test02.foo took "));
    const char* first = strstr(output, "INFO:RESOURCE: ");
    const char* second = strstr(first + 1, "INFO:RESOURCE: ");
    ASSERT_EQ((void*)0, strstr(second + 1, "INFO:RESOURCE: "));
}

// Loads without PostCreate retain their timings after synchronous and asynchronous completion.
TEST_F(ResourceProfileTest, TimingsWithoutPostCreate)
{
    for (uint32_t async = 0; async < 2; ++async)
    {
        CreateFactory(true, 1024, false);
        if (async)
        {
            m_Preloader = dmResource::NewPreloader(m_Factory, "/test01.foo");
            ASSERT_EQ(dmResource::RESULT_OK, CompleteLoad());
        }
        void* resource = 0;
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
        AssertLoggedTimings("/test01.foo");
        ASSERT_EQ(0u, GetTimings("/test01.foo").m_PostCreateCount);
        if (m_Preloader)
        {
            dmResource::DeletePreloader(m_Preloader);
            m_Preloader = 0;
            AssertLoggedTimings("/test01.foo");
        }
        dmResource::Release(m_Factory, resource);
        dmResource::DeleteFactory(m_Factory);
        m_Factory = 0;
        ASSERT_TRUE(FlushLogs());
        g_ResourceProfileOutput.SetSize(0);
    }
}

// Unloading discards the snapshot; a later load retains only its own completed callbacks.
TEST_F(ResourceProfileTest, TimingsAfterUnload)
{
    CreateFactory(true);
    m_PendingPostCreateCalls = 2;
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    ASSERT_EQ(3u, GetTimings("/test01.foo").m_PostCreateCount);
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    g_ResourceProfileOutput.SetSize(0);
    m_PostCreateCount = 0;
    m_PendingPostCreateCalls = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test01.foo", &resource));
    AssertLoggedTimings("/test01.foo");
    ASSERT_EQ(1u, GetTimings("/test01.foo").m_PostCreateCount);
    dmResource::Release(m_Factory, resource);
}

// A failed synchronous file read still emits its terminal loading record.
TEST_F(ResourceProfileTest, SynchronousReadFailure)
{
    CreateFactory(true);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, dmResource::Get(m_Factory, "/missing.foo", &resource));
    ASSERT_TRUE(FlushLogs());
    ASSERT_NE((void*)0, strstr(Output(), "/missing.foo took "));
    ASSERT_NE((void*)0, strstr(Output(), "result: RESOURCE_NOT_FOUND"));
}

// Failures while reading, preloading, creating or post-creating each terminate and report the load once.
TEST_F(ResourceProfileTest, AsynchronousFailures)
{
    for (uint32_t stage = 0; stage < 4; ++stage)
    {
        CreateFactory(true);
        m_PreloadResult = stage == 1 ? dmResource::RESULT_FORMAT_ERROR : dmResource::RESULT_OK;
        m_CreateResult = stage == 2 ? dmResource::RESULT_FORMAT_ERROR : dmResource::RESULT_OK;
        m_PostCreateResult = stage == 3 ? dmResource::RESULT_FORMAT_ERROR : dmResource::RESULT_OK;
        const char* path = stage == 0 ? "/missing.foo" : "/test01.foo";
        m_Preloader = dmResource::NewPreloader(m_Factory, path);
        dmResource::Result expected = stage == 0 ? dmResource::RESULT_RESOURCE_NOT_FOUND : dmResource::RESULT_FORMAT_ERROR;
        ASSERT_EQ(expected, CompleteLoad());
        ASSERT_TRUE(FlushLogs());
        const char* output = Output();
        const char* record = strstr(output, "INFO:RESOURCE: ");
        ASSERT_NE((void*)0, record);
        ASSERT_NE((void*)0, strstr(record, path));
        ASSERT_NE((void*)0, strstr(record, dmResource::ResultToString(expected)));
        ASSERT_EQ((void*)0, strstr(record + 1, "INFO:RESOURCE: "));
        dmResource::DeletePreloader(m_Preloader);
        m_Preloader = 0;
        dmResource::DeleteFactory(m_Factory);
        m_Factory = 0;
        ASSERT_TRUE(FlushLogs());
        g_ResourceProfileOutput.SetSize(0);
    }
}

// A pending PostCreate must preserve an earlier insertion failure as the terminal log result.
TEST_F(ResourceProfileTest, InsertionFailureWithPendingPostCreate)
{
    CreateFactory(true, 1);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test02.foo", &resource));
    ASSERT_TRUE(FlushLogs());
    g_ResourceProfileOutput.SetSize(0);
    m_PostCreateCount = 0;
    m_AllowPostCreate = false;
    m_PostCreateResult = dmResource::RESULT_FORMAT_ERROR;
    m_Preloader = dmResource::NewPreloader(m_Factory, "/test01.foo");
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_PostCreateCount && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(m_Preloader, 0, 0, 1000));
    ASSERT_GT(m_PostCreateCount, 0u);
    ASSERT_TRUE(FlushLogs());
    ASSERT_STREQ("", Output());
    m_AllowPostCreate = true;
    ASSERT_EQ(dmResource::RESULT_OUT_OF_RESOURCES, CompleteLoad());
    dmResource::Release(m_Factory, resource);
    ASSERT_TRUE(FlushLogs());
    const char* record = strstr(Output(), "/test01.foo took ");
    ASSERT_NE((void*)0, record);
    ASSERT_EQ((void*)0, strstr(record + 1, "INFO:RESOURCE: "));
    ASSERT_NE((void*)0, strstr(record, "result: OUT_OF_RESOURCES"));
}

// *********************************************************************************************************

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

#if defined(DM_TEST_HTTP_SUPPORTED)
    dmSocket::Initialize();

    if(argc > 1)
    {
        char path[512];
        dmTestUtil::MakeHostPath(path, sizeof(path), argv[1]);

        dmConfigFile::HConfig config;
        if( dmConfigFile::Load(path, argc, (const char**)argv, &config) != dmConfigFile::RESULT_OK )
        {
            dmLogError("Could not read config file '%s'", argv[1]);
            return 1;
        }
        dmTestUtil::GetSocketsFromConfig(config, &g_HttpPort, 0, 0);
        if (!dmTestUtil::GetIpFromConfig(config, g_HttpAddress, sizeof(g_HttpAddress))) {
            dmLogError("Failed to get server ip!");
        } else {
            dmLogInfo("Server ip: %s:%d", g_HttpAddress, g_HttpPort);
        }

        dmConfigFile::Delete(config);
    }
    else
    {
        dmLogError("No config file specified!");
        return 1;
    }
#endif

    dmHashEnableReverseHash(true);

    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();

#if defined(DM_TEST_HTTP_SUPPORTED)
    dmSocket::Finalize();
#endif
    dmLog::LogFinalize();
    return ret;
}
