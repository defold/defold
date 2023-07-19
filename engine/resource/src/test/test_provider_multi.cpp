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

#include <stdint.h>

#include <dlib/array.h>
#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>
#include <dlib/sys.h>

#include "../resource_mounts.h"
#include "../providers/provider.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

// ****************************************************************************************************************

TEST(ArchiveProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_NE((ArchiveLoader*)0, loader);
}

// // ****************************************************************************************************************

class ArchiveProvidersMulti : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        dmResourceProvider::ArchiveLoader* loader_file = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
        dmResourceProvider::ArchiveLoader* loader_zip = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
        dmResourceProvider::ArchiveLoader* loader_archive = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));

        ASSERT_NE((ArchiveLoader*)0, loader_file);
        ASSERT_NE((ArchiveLoader*)0, loader_zip);
        ASSERT_NE((ArchiveLoader*)0, loader_archive);

        {
            dmResourceProvider::Result result;

            result = Mount(loader_file, "build/src/test/overrides", &m_Archives[0]);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

            result = Mount(loader_archive, "dmanif:build/src/test/resources", &m_Archives[1]);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

            result = Mount(loader_zip, "zip:build/src/test/luresources_compressed.zip", &m_Archives[2]);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

            m_Mounts = dmResourceMounts::Create(0);
            ASSERT_NE((dmResourceMounts::HContext)0, m_Mounts);
        }

        // Mount them in order:
        //  archive - 10
        //  zip     - 20
        //  file    - 30
        {
            dmResource::Result result;
            result = dmResourceMounts::AddMount(m_Mounts, "a", m_Archives[0], 30, false); // file
            ASSERT_EQ(dmResource::RESULT_OK, result);

            result = dmResourceMounts::AddMount(m_Mounts, "b", m_Archives[1], 10, false); // archive
            ASSERT_EQ(dmResource::RESULT_OK, result);

            result = dmResourceMounts::AddMount(m_Mounts, "c", m_Archives[2], 20, false); // zip
            ASSERT_EQ(dmResource::RESULT_OK, result);
        }
    }

    virtual void TearDown()
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(m_Archives); ++i)
        {
            dmResourceMounts::RemoveMount(m_Mounts, m_Archives[i]);

            dmResourceProvider::Result result = dmResourceProvider::Unmount(m_Archives[i]);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        }

        dmResourceMounts::Destroy(m_Mounts);
    }

    dmResourceProvider::Result Mount(dmResourceProvider::ArchiveLoader* loader, const char* path, dmResourceProvider::HArchive* archive)
    {
        dmURI::Parts uri;
        dmURI::Parse(path, &uri);
        return dmResourceProvider::CreateMount(loader, &uri, 0, archive);
    }

    dmResourceMounts::HContext   m_Mounts;
    dmResourceProvider::HArchive m_Archives[3];
};

TEST_F(ArchiveProvidersMulti, GetMounts)
{
    // Note: The mounts are sorted on priority: Highest number comes first in the list
    dmResourceMounts::SGetMountResult result;

    // By index
    ASSERT_EQ(3, dmResourceMounts::GetNumMounts(m_Mounts));

    ASSERT_EQ(dmResource::RESULT_INVAL, dmResourceMounts::GetMountByIndex(m_Mounts, 4, &result));

    ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByIndex(m_Mounts, 0, &result));
    ASSERT_STREQ("a", result.m_Name);
    ASSERT_EQ(30, result.m_Priority);

    ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByIndex(m_Mounts, 1, &result));
    ASSERT_STREQ("c", result.m_Name);
    ASSERT_EQ(20, result.m_Priority);

    ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByIndex(m_Mounts, 2, &result));
    ASSERT_STREQ("b", result.m_Name);
    ASSERT_EQ(10, result.m_Priority);

    // By name
    ASSERT_EQ(dmResource::RESULT_INVAL, dmResourceMounts::GetMountByName(m_Mounts, "not_exist", &result));

    ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByName(m_Mounts, "c", &result));
    ASSERT_STREQ("c", result.m_Name);
    ASSERT_EQ(20, result.m_Priority);

    ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByName(m_Mounts, "a", &result));
    ASSERT_STREQ("a", result.m_Name);
    ASSERT_EQ(30, result.m_Priority);

    ASSERT_EQ(dmResource::RESULT_OK, dmResourceMounts::GetMountByName(m_Mounts, "b", &result));
    ASSERT_STREQ("b", result.m_Name);
    ASSERT_EQ(10, result.m_Priority);
}

static uint8_t* GetRawFile(const char* path, uint32_t* size, bool override)
{
    char path_buffer[256];
    const char* override_path = override ? "/overrides" : "";
    const char* host_path = dmTestUtil::MakeHostPathf(path_buffer, sizeof(path_buffer), "build/src/test%s%s", override_path, path);
    if (!dmSys::Exists(host_path))
        host_path = dmTestUtil::MakeHostPathf(path_buffer, sizeof(path_buffer), "src/test%s%s", override_path, path);
    return dmTestUtil::ReadHostFile(host_path, size);
}


TEST_F(ArchiveProvidersMulti, ReadFile)
{
    const char* file_paths[] = {
        "/archive_data/file4.adc", // archive
        "/archive_data/liveupdate.file6.scriptc", // zip
        "/somedata.adc", // file
        "/archive_data/file2.adc", // exists in archive, but overridden in file mount
        "/archive_data/liveupdate.file7.adc", // exists in zip archive, but overridden in file mout
    };

    uint32_t file_override_sizes[] = {
        0,
        0,
        13,
        20,
        25,
    };


    for (uint32_t i = 0; i < DM_ARRAY_SIZE(file_paths); ++i)
    {
        dmhash_t path_hash = dmHashString64(file_paths[i]);
        uint32_t raw_file_size = 0;
        uint8_t* raw_file = GetRawFile(file_paths[i], &raw_file_size, file_override_sizes[i] != 0);
        ASSERT_NE(0U, raw_file_size);
        ASSERT_NE((uint8_t*)0, raw_file);

        uint32_t resource_size;
        dmResource::Result result;

        result = dmResourceMounts::GetResourceSize(m_Mounts, path_hash, file_paths[i], &resource_size);
        ASSERT_EQ(dmResource::RESULT_OK, result);

        ASSERT_EQ(raw_file_size, resource_size);

        uint8_t* resource = new uint8_t[resource_size];
        result = dmResourceMounts::ReadResource(m_Mounts, path_hash, file_paths[i], resource, resource_size);

        ASSERT_ARRAY_EQ_LEN(raw_file, resource, resource_size);

        dmMemory::AlignedFree(raw_file);
        delete[] resource;
    }
}

int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
