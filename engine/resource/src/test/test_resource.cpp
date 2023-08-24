// Copyright 2020-2023 The Defold Foundation
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
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/socket.h>
#include <dlib/sys.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dlib/testutil.h>
#include <ddf/ddf.h>

#if defined(DM_PLATFORM_VENDOR)
    #define TMP_DIR ""
    #define MOUNT_DIR DM_HOSTFS
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
#include "../resource_private.h"
#include "../resource_util.h"
#include "../resource_verify.h"
#include "test/test_resource_ddf.h"

#if defined(DM_TEST_HTTP_SUPPORTED)
#include <dlib/http_client.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/uri.h>

static int g_HttpPort = -1;
char g_HttpAddress[128] = "localhost";

#endif


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

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
    virtual void SetUp()
    {
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;

        factory = dmResource::NewFactory(&params, MOUNT_DIR);
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

class DynamicResourceTest : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        const char* test_dir = "build/src/test";
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

dmResource::Result ResourceContainerPreload(const dmResource::ResourcePreloadParams& params);

dmResource::Result ResourceContainerCreate(const dmResource::ResourceCreateParams& params);

dmResource::Result ResourceContainerDestroy(const dmResource::ResourceDestroyParams& params);

dmResource::Result FooResourceCreate(const dmResource::ResourceCreateParams& params);

dmResource::Result FooResourcePostCreate(const dmResource::ResourcePostCreateParams& params);

dmResource::Result FooResourceDestroy(const dmResource::ResourceDestroyParams& params);

class GetResourceTest : public jc_test_params_class<const char*>
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

        const char* original_mount_path = GetParam();
        char mountpath[512];
        if (strstr(original_mount_path, "http") == original_mount_path)
        {
            dmSnPrintf(mountpath, sizeof(mountpath), original_mount_path, g_HttpAddress, g_HttpPort);
            original_mount_path = mountpath;
        }

        m_Factory = dmResource::NewFactory(&params, original_mount_path);

        ASSERT_NE((void*) 0, m_Factory);
        m_ResourceName = "/test.cont";

        dmResource::Result e;
        e = dmResource::RegisterType(m_Factory, "cont", this, &ResourceContainerPreload, &ResourceContainerCreate, 0, &ResourceContainerDestroy, 0);
        ASSERT_EQ(dmResource::RESULT_OK, e);

        e = dmResource::RegisterType(m_Factory, "foo", this, 0, &FooResourceCreate, &FooResourcePostCreate, &FooResourceDestroy, 0);
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

const char* params_resource_paths[] = {
    "build/src/test/",
#if defined(DM_TEST_HTTP_SUPPORTED)
    "http://%s:%d",
#endif
    "dmanif:build/src/test/resources_pb.dmanifest"
};
INSTANTIATE_TEST_CASE_P(GetResourceTestURI, GetResourceTest, jc_test_values_in(params_resource_paths));

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
    int* old_resource = new int();
    *old_resource = *recreate_resource;
    params.m_Resource->m_PrevResource = (void*)old_resource;

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

static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params) {
    ReloadData* data = (ReloadData*)params.m_UserData;
    data->m_Old = *((int*)params.m_Resource->m_PrevResource);
    data->m_New = *((int*)params.m_Resource->m_Resource);
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

    dmResource::ResourceType type;
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

static void ResourceReloadedHttpCallback(const dmResource::ResourceReloadedParams& params)
{
    ReloadedContext* ctx = (ReloadedContext*) params.m_UserData;

    if (dmResource::GetResource(params.m_Resource) == ctx->m_Resource)
    {
        dmAtomicStore32(ctx->m_Reloaded, 1);
    }
}

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

    dmResource::ResourceType type;
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

    dmThread::Thread send_thread = dmThread::New(&SendReloadThread, 0x8000, 0, "reload");

    uint64_t t_start = dmTime::GetTime();
    do
    {
        dmTime::Sleep(1000 * 10);
        dmResource::UpdateFactory(factory);

        uint64_t t = dmTime::GetTime();
        if ((t - t_start) >= 2 * 1000000)
        {
            ASSERT_TRUE(false && "Test timed out");
            break;
        }
    } while (!dmAtomicGet32(&send_reload_done));

    dmThread::Join(send_thread);

    ASSERT_EQ(456, *resource);

    dmResource::UnregisterResourceReloadedCallback(factory, ResourceReloadedHttpCallback, resource);

    dmSys::Unlink(host_name);

    send_reload_done = 0;
    send_thread = dmThread::New(&SendReloadThread, 0x8000, (void*)&state, "reload");

    do
    {
        dmTime::Sleep(1000 * 10);
        dmResource::UpdateFactory(factory);
    } while (!dmAtomicGet32(&send_reload_done));

    dmThread::Join(send_thread);

    dmResource::Result rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, rr);

    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

// Test the "filename" callback argument

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
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*) 0, factory);

    dmResource::Result e;
    e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    dmResource::ResourceType type;
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



static dmResource::Result RegisterResourceTypeCustom(dmResource::ResourceTypeRegisterContext& ctx)
{
    int* context = new int;
    *context = 1;
    ctx.m_Contexts->Put(ctx.m_NameHash, context);
    ctx.m_Contexts->Put(dmHashString64("register_called"), 0);

    // we're not actually invoking them, they just need to be non null
    dmResource::FResourceCreate create_fn = (dmResource::FResourceCreate)1;
    dmResource::FResourceDestroy destroy_fn = (dmResource::FResourceDestroy)1;
    return dmResource::RegisterType(ctx.m_Factory,
                                    ctx.m_Name,
                                    context,
                                    0,
                                    create_fn,
                                    0,
                                    destroy_fn,
                                    0);
}

static dmResource::Result DeregisterResourceTypeCustom(dmResource::ResourceTypeRegisterContext& ctx)
{
    int** context = (int**)ctx.m_Contexts->Get(ctx.m_NameHash);
    delete *context;
    ctx.m_Contexts->Erase(ctx.m_NameHash);
    ctx.m_Contexts->Put(dmHashString64("deregister_called"), 0);
    return dmResource::RESULT_OK;
}

TEST(ResourceExtension, CustomType)
{
    uint8_t DM_ALIGNED(16) desc[dmResource::s_ResourceTypeCreatorDescBufferSize];

    dmResource::RegisterTypeCreatorDesc((struct dmResource::TypeCreatorDesc*)desc, sizeof(desc),
                                        "custom", RegisterResourceTypeCustom, DeregisterResourceTypeCustom);

    dmResource::Result e;
    dmResource::NewFactoryParams params;
    dmResource::HFactory factory = dmResource::NewFactory(&params, ".");
    ASSERT_NE((void*)0, factory);

    dmResource::ResourceType type;
    e = dmResource::GetTypeFromExtension(factory, "custom", &type);
    ASSERT_EQ(dmResource::RESULT_UNKNOWN_RESOURCE_TYPE, e);

    dmHashTable64<void*> contexts;
    contexts.SetCapacity(7, 14);

    dmResource::RegisterTypes(factory, &contexts);
    ASSERT_EQ(2u, contexts.Size());

    dmResource::DeregisterTypes(factory, &contexts);
    ASSERT_EQ(2u, contexts.Size());


    void** context = (void**)contexts.Get(dmHashString64("deregister_called"));
    ASSERT_NE(0u, (uintptr_t)context);

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
    ASSERT_EQ(0, strcmp(resource_name, user_data.m_Name));

    user_data = CallbackUserData();
    dmResource::UnregisterResourceReloadedCallback(factory, ReloadCallback, &user_data);

    rr = dmResource::ReloadResource(factory, resource_name, 0);
    ASSERT_EQ(dmResource::RESULT_OK, rr);

    ASSERT_EQ((void*)0, user_data.m_Descriptor);
    ASSERT_EQ((void*)0, user_data.m_Name);

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


int main(int argc, char **argv)
{
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
    return ret;
}

