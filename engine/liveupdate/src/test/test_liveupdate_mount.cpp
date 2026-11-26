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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <stdint.h>
#include <string.h>

#include <dlib/configfile.h>
#include <dlib/hash.h>
#include <dlib/time.h>
#include <extension/extension.hpp>
#include <resource/resource.h>
#include <resource/resource_mounts.h>
#include <resource/providers/provider.h>
#include <providers/provider_private.h>

#include "liveupdate.h"

extern "C" void dmExportedSymbols();

// Archives created by the "lutest" loader that have not been unmounted yet
static int32_t                           g_LiveArchives = 0;
static int32_t                           g_MountCalls = 0;
// When set, the next mount takes this name in the mounts context before the archive is returned
static dmhash_t                          g_TakeNameOnMount = 0;
static dmResourceMounts::HContext        g_Mounts = 0;
static dmResourceProvider::ArchiveLoader g_TestLoader;

static bool TestCanMount(const dmURI::Parts*)
{
    return true;
}

static dmResourceProvider::Result TestMount(const dmURI::Parts*, dmResourceProvider::HArchive, dmResourceProvider::HArchiveInternal* out_archive)
{
    ++g_MountCalls;
    ++g_LiveArchives;
    *out_archive = (dmResourceProvider::HArchiveInternal)&g_TestLoader;

    if (g_TakeNameOnMount)
    {
        dmResourceProvider::HArchive placeholder = 0;
        dmResourceProvider::CreateMount(&g_TestLoader, (void*)&g_TestLoader, &placeholder);
        ++g_LiveArchives;
        dmResourceMounts::AddMount(g_Mounts, g_TakeNameOnMount, placeholder, 1);
        g_TakeNameOnMount = 0;
    }

    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result TestUnmount(dmResourceProvider::HArchiveInternal)
{
    --g_LiveArchives;
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result TestGetFileSize(dmResourceProvider::HArchiveInternal, dmhash_t, const char*, uint32_t*)
{
    return dmResourceProvider::RESULT_NOT_FOUND;
}

static dmResourceProvider::Result TestReadFile(dmResourceProvider::HArchiveInternal, dmhash_t, const char*, uint8_t*, uint32_t)
{
    return dmResourceProvider::RESULT_NOT_FOUND;
}

static dmResourceProvider::Result TestReadFilePartial(dmResourceProvider::HArchiveInternal, dmhash_t, const char*, uint32_t, uint32_t, uint8_t*, uint32_t*)
{
    return dmResourceProvider::RESULT_NOT_FOUND;
}

static void RegisterTestLoader()
{
    memset(&g_TestLoader, 0, sizeof(g_TestLoader));
    g_TestLoader.m_NameHash        = dmHashString64("lutest");
    g_TestLoader.m_CanMount        = TestCanMount;
    g_TestLoader.m_Mount           = TestMount;
    g_TestLoader.m_Unmount         = TestUnmount;
    g_TestLoader.m_GetFileSize     = TestGetFileSize;
    g_TestLoader.m_ReadFile        = TestReadFile;
    g_TestLoader.m_ReadFilePartial = TestReadFilePartial;
    dmResourceProvider::RegisterArchiveLoader(&g_TestLoader);
}

struct AddMountCallbackResult
{
    bool m_Done;
    int  m_Result;
};

static dmConfigFile::HConfig  g_Config = 0;
static dmResource::HFactory   g_Factory = 0;
static ExtensionAppParams     g_AppParams;
static ExtensionParams        g_Params;
static AddMountCallbackResult g_AddMountResult;

static void OnAddMount(dmhash_t, const char*, int result, void* ctx)
{
    AddMountCallbackResult* r = (AddMountCallbackResult*)ctx;
    r->m_Result = result;
    r->m_Done = true;
}

// A failed ASSERT skips TearDown, so the factory and the extension live for the whole test case
class LiveUpdateMountTest : public jc_test_base_class
{
    public:
    static void SetUpTestCase()
    {
        dmConfigFile::LoadFromBuffer(0, 0, 0, 0, &g_Config);

        dmResource::NewFactoryParams factory_params;
        g_Factory = dmResource::NewFactory(&factory_params, ".");
        if (!g_Factory)
        {
            return;
        }

        g_Mounts = dmResource::GetMountsContext(g_Factory);

        ExtensionAppParamsInitialize(&g_AppParams);
        g_AppParams.m_ConfigFile = g_Config;
        dmExtension::AppInitialize(&g_AppParams);

        ExtensionParamsInitialize(&g_Params);
        g_Params.m_ConfigFile = g_Config;
        g_Params.m_ResourceFactory = g_Factory;
        dmExtension::Initialize(&g_Params);
    }

    static void TearDownTestCase()
    {
        if (g_Factory)
        {
            dmExtension::Finalize(&g_Params);
            dmExtension::AppFinalize(&g_AppParams);
            ExtensionParamsFinalize(&g_Params);
            ExtensionAppParamsFinalize(&g_AppParams);
            dmResource::DeleteFactory(g_Factory);
        }

        dmConfigFile::Delete(g_Config);
        g_Factory = 0;
        g_Mounts = 0;
    }

    protected:
    void SetUp() override
    {
        ASSERT_NE((dmResource::HFactory)0, g_Factory);
        g_TakeNameOnMount = 0;
    }

    int AddMountAndWait(const char* name, const char* uri, int priority)
    {
        g_AddMountResult.m_Done = false;
        g_AddMountResult.m_Result = 0;
        EXPECT_EQ(dmLiveUpdate::RESULT_OK, dmLiveUpdate::AddMountAsync(dmHashString64(name), uri, priority, OnAddMount, &g_AddMountResult));

        uint64_t start = dmTime::GetMonotonicTime();
        while (!g_AddMountResult.m_Done && dmTime::GetMonotonicTime() - start < 10000000)
        {
            dmExtension::Update(&g_Params, 0.0f);
            if (!g_AddMountResult.m_Done)
            {
                dmTime::Sleep(1000);
            }
        }

        EXPECT_TRUE(g_AddMountResult.m_Done);
        return g_AddMountResult.m_Result;
    }

    int GetMountPriority(const char* name)
    {
        dmResourceMounts::SGetMountResult info;
        info.m_Priority = 0;
        EXPECT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByNameHash(g_Mounts, dmHashString64(name), &info));
        return info.m_Priority;
    }

};

// Verifies that a second mount with a name already in use reports LIVEUPDATE_INVAL to the callback,
// keeps the first mount and never creates the second archive.
// Guards against the callback receiving LIVEUPDATE_OK for a mount that was not added.
TEST_F(LiveUpdateMountTest, DuplicateNameFails)
{
    uint32_t num_mounts = dmResourceMounts::GetNumMounts(g_Mounts);
    int32_t live_archives = g_LiveArchives;
    int32_t mount_calls = g_MountCalls;

    ASSERT_EQ(dmLiveUpdate::RESULT_OK, AddMountAndWait("dup", "lutest:first", 10));
    ASSERT_EQ(num_mounts + 1, dmResourceMounts::GetNumMounts(g_Mounts));
    ASSERT_EQ(live_archives + 1, g_LiveArchives);

    ASSERT_EQ(dmLiveUpdate::RESULT_INVAL, AddMountAndWait("dup", "lutest:second", 20));
    ASSERT_EQ(num_mounts + 1, dmResourceMounts::GetNumMounts(g_Mounts));
    ASSERT_EQ(10, GetMountPriority("dup"));
    ASSERT_EQ(live_archives + 1, g_LiveArchives);
    ASSERT_EQ(mount_calls + 1, g_MountCalls);

    ASSERT_EQ(dmLiveUpdate::RESULT_OK, dmLiveUpdate::RemoveMountSync(dmHashString64("dup")));
    ASSERT_EQ(num_mounts, dmResourceMounts::GetNumMounts(g_Mounts));
    ASSERT_EQ(live_archives, g_LiveArchives);
}

// Verifies that when the name is taken while the archive is being created, the job unmounts
// the new archive and reports LIVEUPDATE_INVAL.
// Guards against leaking the archive, and the file it keeps open, when dmResourceMounts::AddMount fails.
TEST_F(LiveUpdateMountTest, NameTakenDuringMountReleasesArchive)
{
    uint32_t num_mounts = dmResourceMounts::GetNumMounts(g_Mounts);
    int32_t live_archives = g_LiveArchives;

    g_TakeNameOnMount = dmHashString64("race");
    ASSERT_EQ(dmLiveUpdate::RESULT_INVAL, AddMountAndWait("race", "lutest:race", 20));
    ASSERT_EQ(num_mounts + 1, dmResourceMounts::GetNumMounts(g_Mounts));
    ASSERT_EQ(1, GetMountPriority("race"));
    ASSERT_EQ(live_archives + 1, g_LiveArchives);

    ASSERT_EQ(dmLiveUpdate::RESULT_OK, dmLiveUpdate::RemoveMountSync(dmHashString64("race")));
    ASSERT_EQ(num_mounts, dmResourceMounts::GetNumMounts(g_Mounts));
    ASSERT_EQ(live_archives, g_LiveArchives);
}

int main(int argc, char** argv)
{
    dmExportedSymbols();
    RegisterTestLoader();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
