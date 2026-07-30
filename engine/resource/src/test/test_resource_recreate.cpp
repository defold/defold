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

// Focused, self-contained coverage for the production-safe recreation path added for the
// graphics context-restore feature: dmResource::RecreateResource() addresses a loaded resource
// by its content hash, reuses the existing resource handle, and (unlike ReloadResource) does NOT
// invoke ResourceReloadedCallbacks. It also exercises dmResource::GetResourceTypeExtensionHash(),
// which render.cpp uses to bucket GPU-backed resources.
//
// Kept apart from test_resource.cpp because that suite depends on generated DDF/embedded archives
// and is not (yet) part of the CMake build; this test only needs the file provider and raw bytes.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../resource.h"
#include "../resource_private.h"

#if defined(DM_PLATFORM_VENDOR)
    #define TMP_DIR ""
    #define MOUNT_DIR "file:"
#else
    #define TMP_DIR "."
    #define MOUNT_DIR "."
#endif

// Resource type backed by a single int parsed from the file's ascii bytes. Copied from
// test_resource.cpp's recreate helpers so this test can build independently.
static dmResource::Result RecreateResourceCreate(const dmResource::ResourceCreateParams* params)
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

static dmResource::Result RecreateResourceDestroy(const dmResource::ResourceDestroyParams* params)
{
    int* recreate_resource = (int*) ResourceDescriptorGetResource(params->m_Resource);
    delete recreate_resource;
    return dmResource::RESULT_OK;
}

static dmResource::Result RecreateResourceRecreate(const dmResource::ResourceRecreateParams* params)
{
    int* recreate_resource = (int*) ResourceDescriptorGetResource(params->m_Resource);
    assert(recreate_resource);
    int* old_resource = new int();
    *old_resource = *recreate_resource;

    ResourceDescriptorSetPrevResource(params->m_Resource, old_resource);

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

struct ReloadData
{
    ReloadData(): m_Old(0), m_New(0) {}
    int m_Old;
    int m_New;
};

static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
{
    ReloadData* data = (ReloadData*) params->m_UserData;
    data->m_Old = *((int*) ResourceDescriptorGetPrevResource(params->m_Resource));
    data->m_New = *((int*) ResourceDescriptorGetResource(params->m_Resource));
}

static bool CaptureResourceHash(const dmResource::IteratorResource& resource, void* user_ctx)
{
    *(dmhash_t*) user_ctx = resource.m_Id; // only one resource is loaded in the test below
    return true;
}

TEST(RecreateByHashTest, RecreateByHash)
{
    dmResource::NewFactoryParams params;
    params.m_MaxResources = 16;
    params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT; // gives a reliable file lookup for this test
    dmResource::HFactory factory = dmResource::NewFactory(&params, MOUNT_DIR);
    ASSERT_NE((void*) 0, factory);

    // A reloaded callback that we expect RecreateResource to leave untouched.
    ReloadData reload_data;
    reload_data.m_Old = -1;
    reload_data.m_New = -1;
    dmResource::RegisterResourceReloadedCallback(factory, ResourceReloadedCallback, &reload_data);

    dmResource::Result e = dmResource::RegisterType(factory, "foo", 0, 0, &RecreateResourceCreate, 0, &RecreateResourceDestroy, &RecreateResourceRecreate);
    ASSERT_EQ(dmResource::RESULT_OK, e);

    const char* resource_name = "/__testrecreatebyhash__.foo";
    char host_name[512];
    const char* path = dmTestUtil::MakeHostPathf(host_name, sizeof(host_name), "%s/%s", TMP_DIR, resource_name);

    FILE* f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "123");
    fclose(f);

    int* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(factory, resource_name, (void**) &resource));
    ASSERT_EQ(123, *resource);

    // Obtain the resource's content hash via the same iteration the reload driver uses.
    dmhash_t name_hash = 0;
    dmResource::IterateResources(factory, CaptureResourceHash, &name_hash);
    ASSERT_NE((dmhash_t) 0, name_hash);

    // The type is reported by its extension hash (used to bucket GPU-backed resources).
    ASSERT_EQ(dmHashString64("foo"), dmResource::GetResourceTypeExtensionHash(factory, name_hash));

    // Change the bytes, then recreate by hash. The same resource handle (pointer) is reused.
    f = fopen(path, "wb");
    ASSERT_NE((FILE*) 0, f);
    fprintf(f, "456");
    fclose(f);

    int* resource_before = resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RecreateResource(factory, name_hash));
    ASSERT_EQ(456, *resource);
    ASSERT_EQ(resource_before, resource);

    // RecreateResource must NOT invoke ResourceReloadedCallbacks (they are absent in release builds).
    ASSERT_EQ(-1, reload_data.m_Old);
    ASSERT_EQ(-1, reload_data.m_New);

    // Unknown hash -> not found; type hash of an unknown resource -> 0.
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, dmResource::RecreateResource(factory, dmHashString64("does/not/exist")));
    ASSERT_EQ((dmhash_t) 0, dmResource::GetResourceTypeExtensionHash(factory, dmHashString64("does/not/exist")));

    dmSys::Unlink(path);
    dmResource::UnregisterResourceReloadedCallback(factory, ResourceReloadedCallback, &reload_data);
    dmResource::Release(factory, resource);
    dmResource::DeleteFactory(factory);
}

// Force-links and registers the resource providers referenced via exported symbols (the file
// provider here), mirroring the other resource provider tests.
extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();

    dmLog::LogFinalize();
    return result;
}
