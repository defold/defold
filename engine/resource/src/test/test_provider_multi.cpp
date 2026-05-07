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

static uint8_t* GetRawFile(const char* path, uint32_t* size, bool override)
{
    const char* override_path = override ? "/overrides" : "";
    const char* alternatives[2] = {
        "build/src/test%s%s",
        "src/test%s%s"
    };
    for (int i = 0; i < DM_ARRAY_SIZE(alternatives); ++i)
    {
        char path_buffer[256];
        dmSnPrintf(path_buffer, sizeof(path_buffer), alternatives[i], override_path, path);

        char host_buffer[256];
        dmTestUtil::MakeHostPath(host_buffer, sizeof(host_buffer), path_buffer);
        if (!dmSys::Exists(host_buffer))
            continue;
        return dmTestUtil::ReadHostFile(path_buffer, size);
    }
    return 0;
}

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

// ****************************************************************************************************************

class ArchiveProvidersMulti : public jc_test_base_class
{
protected:
    void SetUp() override
    {
        dmResourceProvider::ArchiveLoader* loader_file = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
        dmResourceProvider::ArchiveLoader* loader_zip = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
        dmResourceProvider::ArchiveLoader* loader_archive = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));

        ASSERT_NE((ArchiveLoader*)0, loader_file);
        ASSERT_NE((ArchiveLoader*)0, loader_zip);
        ASSERT_NE((ArchiveLoader*)0, loader_archive);

        {
            dmResourceProvider::Result result;

#define FSPREFIX ""
#if defined(__EMSCRIPTEN__)
            // Trigger the vsf init for emscripten (hidden in MakeHostPath)
            char path[1024];
            dmTestUtil::MakeHostPath(path, sizeof(path), "src");
            #undef FSPREFIX
            #define FSPREFIX DM_HOSTFS
#endif

            result = Mount(loader_file, FSPREFIX "build/src/test/overrides", &m_Archives[0]);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

            result = Mount(loader_archive, "dmanif:" FSPREFIX "build/src/test/resources", &m_Archives[1]);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

            result = Mount(loader_zip, "zip:" FSPREFIX "build/src/test/luresources_compressed.zip", &m_Archives[2]);
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

    void TearDown() override
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

TEST_F(ArchiveProvidersMulti, ReadFile)
{
    const char* file_paths[] = {
        "/archive_data/file4.adc", // archive
        "/archive_data/liveupdate.file6.scriptc", // zip
        "/somedata.adc", // file
        "/archive_data/file2.adc", // exists in archive, but overridden in file mount
        "/archive_data/liveupdate.file7.adc", // exists in zip archive, but overridden in file mout
    };

    bool file_overrides[] = {
        false,
        false,
        true,
        true,
        true,
    };


    for (uint32_t i = 0; i < DM_ARRAY_SIZE(file_paths); ++i)
    {
        dmhash_t path_hash = dmHashString64(file_paths[i]);
        uint32_t raw_file_size = 0;
        uint8_t* raw_file = GetRawFile(file_paths[i], &raw_file_size, file_overrides[i]);
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

TEST_F(ArchiveProvidersMulti, ReadCustomFile)
{
    uint8_t     file0_data[] = {0,1,2,3,4,5,6,7,8,9};
    uint32_t    file0_size = DM_ARRAY_SIZE(file0_data);
    const char* file0_path = "/custom/test.adc";
    dmhash_t    file0_hash = dmHashString64(file0_path);

    uint32_t resource_size = 0;
    dmResource::Result result = dmResource::RESULT_UNKNOWN_ERROR;

    // Test without having added the file
    result = dmResourceMounts::GetResourceSize(m_Mounts, file0_hash, file0_path, &resource_size);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, result);
    ASSERT_EQ(0u, resource_size); // test that the value hasn't been touched

    // Test with adding the file
    result = dmResourceMounts::AddFile(m_Mounts, file0_hash, file0_size, file0_data);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    result = dmResourceMounts::AddFile(m_Mounts, file0_hash, file0_size, file0_data);
    ASSERT_EQ(dmResource::RESULT_ALREADY_REGISTERED, result);

    result = dmResourceMounts::GetResourceSize(m_Mounts, file0_hash, file0_path, &resource_size);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(file0_size, resource_size);

    uint8_t* resource = new uint8_t[resource_size];
    result = dmResourceMounts::ReadResource(m_Mounts, file0_hash, file0_path, resource, resource_size);

    ASSERT_ARRAY_EQ_LEN(file0_data, resource, resource_size);

    delete[] resource;

    // Test removing the file
    result = dmResourceMounts::RemoveFile(m_Mounts, file0_hash);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    result = dmResourceMounts::RemoveFile(m_Mounts, file0_hash);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, result);

    resource_size = 0;
    result = dmResourceMounts::GetResourceSize(m_Mounts, file0_hash, file0_path, &resource_size);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, result);
    ASSERT_EQ(0u, resource_size);
}

TEST_F(ArchiveProvidersMulti, ReadCustomFilePartial)
{
    uint8_t     file_data[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    uint32_t    file_size = DM_ARRAY_SIZE(file_data);
    const char* file_path = "/custom/test.adc";
    dmhash_t    file_hash = dmHashString64(file_path);

    dmResource::Result result = dmResource::RESULT_UNKNOWN_ERROR;

    // Test with adding the file
    result = dmResourceMounts::AddFile(m_Mounts, file_hash, file_size, file_data);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    uint32_t resource_size = 0;
    result = dmResourceMounts::GetResourceSize(m_Mounts, file_hash, file_path, &resource_size);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(file_size, resource_size);

    for (uint32_t chunk_size = 1; chunk_size < file_size+2; ++chunk_size)
    {
        uint8_t read_buffer[DM_ARRAY_SIZE(file_data)];
        memset(read_buffer, 0, sizeof(read_buffer));

        uint32_t offset = 0;
        while (offset < file_size)
        {
            uint32_t nread;
            result = dmResourceMounts::ReadResourcePartial(m_Mounts, file_hash, file_path, offset, chunk_size, read_buffer, &nread);
            ASSERT_EQ(dmResource::RESULT_OK, result);
            ASSERT_NE(0u, nread);
            ASSERT_GE(chunk_size, nread);

            ASSERT_ARRAY_EQ_LEN(&file_data[offset], read_buffer, nread);
            offset += nread;
        }
    }

    result = dmResourceMounts::RemoveFile(m_Mounts, file_hash);
    ASSERT_EQ(dmResource::RESULT_OK, result);
}

TEST_F(ArchiveProvidersMulti, ReadCustomEmptyFile)
{
    uint8_t     file_data[] = {0};
    uint32_t    file_size = 0;
    const char* file_path = "/custom/empty.adc";
    dmhash_t    file_hash = dmHashString64(file_path);

    uint32_t resource_size = 0;
    dmResource::Result result = dmResource::RESULT_UNKNOWN_ERROR;

    // Test with adding the file
    result = dmResourceMounts::AddFile(m_Mounts, file_hash, file_size, file_data);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    result = dmResourceMounts::GetResourceSize(m_Mounts, file_hash, file_path, &resource_size);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(file_size, resource_size);

    uint8_t resource[1];
    result = dmResourceMounts::ReadResource(m_Mounts, file_hash, file_path, resource, resource_size);
    ASSERT_EQ(file_size, resource_size);

    uint32_t nread;
    result = dmResourceMounts::ReadResourcePartial(m_Mounts, file_hash, file_path, 0, file_size, resource, &nread);
    ASSERT_EQ(file_size, nread);

    // Test removing the file
    result = dmResourceMounts::RemoveFile(m_Mounts, file_hash);
    ASSERT_EQ(dmResource::RESULT_OK, result);
}

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
