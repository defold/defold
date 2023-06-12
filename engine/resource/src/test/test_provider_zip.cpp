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
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>

#include "../providers/provider.h"
#include "../providers/provider_private.h"
#include "../providers/provider_archive.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

// extern unsigned char RESOURCES_DMANIFEST[];
// extern uint32_t RESOURCES_DMANIFEST_SIZE;
// extern unsigned char RESOURCES_ARCI[];
// extern uint32_t RESOURCES_ARCI_SIZE;
// extern unsigned char RESOURCES_ARCD[];
// extern uint32_t RESOURCES_ARCD_SIZE;

// extern unsigned char RESOURCES_COMPRESSED_DMANIFEST[];
// extern uint32_t RESOURCES_COMPRESSED_DMANIFEST_SIZE;
// extern unsigned char RESOURCES_COMPRESSED_ARCI[];
// extern uint32_t RESOURCES_COMPRESSED_ARCI_SIZE;
// extern unsigned char RESOURCES_COMPRESSED_ARCD[];
// extern uint32_t RESOURCES_COMPRESSED_ARCD_SIZE;

// build/src/test/resources.dmanifest et al
// Manifest:
// entry: hash: CEF6F1BCAA8E22F4735741265822BC21902C122E  flags: 1 url: 1db7f0530911b1ce  /archive_data/file4.adc
// entry: hash: AAE1F8CC01C23BA6067F7ED81DF5A187A047AA7F  flags: 2 url: 68b7e06402ee965c  /archive_data/liveupdate.file6.scriptc
// entry: hash: 5F9E1B6C705D9FDCBC418062F3EA3F6A33640914  flags: 1 url: 731d3cc48697dfe4  /archive_data/file5.scriptc
// entry: hash: 6ECFA74439E0141887F8A6C0C5AD30960340B458  flags: 1 url: 8417331f14a42e4b  /archive_data/file1.adc
// entry: hash: 0356AC9F6EBB8BD3DB05CB73962BB6FC88E47AB5  flags: 1 url: b4870d43513879ba  /archive_data/file3.adc
// entry: hash: 10B8FE93AC3059D61D5A809C253C6445F6FC7A63  flags: 1 url: e1f97b41134ff4a6  /archive_data/file2.adc
// entry: hash: 2C199FD56AB941DB233A217E150A21FD2AE1460D  flags: 2 url: e7b921ca4d761083  /archive_data/liveupdate.file7.adc
// Zip Archive:
// Archive:  build/src/test/defold.resourcepack_generic.zip
//   Length      Date    Time    Name
// ---------  ---------- -----   ----
//       691  05-23-2023 16:10   liveupdate.game.dmanifest
//        74  05-23-2023 16:10   aae1f8cc01c23ba6067f7ed81df5a187a047aa7f
//        46  05-23-2023 16:10   2c199fd56ab941db233a217e150a21fd2ae1460d

const char* FILE_PATHS[] = {
    "/archive_data/liveupdate.file6.scriptc",
    "/archive_data/liveupdate.file7.adc"
};

// Custom files added to the zip file
const char* EXTRA_FILE_PATHS[] = {
    "/archive_data/file3.adc",
    "/uniquezipdata.adc"
};

const uint32_t EXTRA_FILE_SIZES[] = {
    37,
    29
};

// ****************************************************************************************************************

TEST(ArchiveProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_EQ((ArchiveLoader*)0, loader);
}

TEST(ArchiveProviderBasic, CanMount)
{
    dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    dmURI::Parts uri;
    dmURI::Parse("zip:some/test/path.zip", &uri);
    ASSERT_TRUE(loader->m_CanMount(&uri));

    dmURI::Parse(".", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("http://domain.com/path", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));
}

// ****************************************************************************************************************

struct ZipParams
{
    const char* m_Path;
    bool        m_ExtraFiles;
};

class ArchiveProviderZip : public jc_test_params_class<ZipParams>
{
protected:
    virtual void SetUp()
    {
        const ZipParams& params = GetParam();

        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
        ASSERT_NE((ArchiveLoader*)0, m_Loader);

        dmURI::Parts uri;
        dmURI::Parse(params.m_Path, &uri);

        dmResourceProvider::Result result = dmResourceProvider::CreateMount(m_Loader, &uri, 0, &m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmResourceProvider::Result result = dmResourceProvider::Unmount(m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::ArchiveLoader* m_Loader;
};

TEST_P(ArchiveProviderZip, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;
    const char* path;

    path = "/archive_data/liveupdate.file6.scriptc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(58U, file_size);

    path = "/archive_data/liveupdate.file7.adc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(40U, file_size);

    path = "src/test/files/not_exist";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

static uint8_t* GetRawFile(const char* path, uint32_t* size, bool override)
{
    char path_buffer[256];
    const char* override_path = override ? "/zipfiles" : "";
    const char* host_path = dmTestUtil::MakeHostPathf(path_buffer, sizeof(path_buffer), "build/src/test%s%s", override_path, path);
    if (!dmSys::Exists(host_path))
        host_path = dmTestUtil::MakeHostPathf(path_buffer, sizeof(path_buffer), "src/test%s%s", override_path, path);
    return dmTestUtil::ReadHostFile(host_path, size);
}

// * Test that the files exist
// * Test that the content is the same as on disc
TEST_P(ArchiveProviderZip, ReadFile)
{
    uint8_t short_buffer[4] = {0};

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(FILE_PATHS); ++i)
    {
        const char* path = FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);

        uint32_t expected_file_size;

        // char path_buffer1[256];
        // dmSnPrintf(path_buffer1, sizeof(path_buffer1), "build/src/test%s", path);
        // const uint8_t* expected_file = dmTestUtil::ReadHostFile(path_buffer1, &expected_file_size);
        const uint8_t* expected_file = GetRawFile(path, &expected_file_size, false);
        ASSERT_NE((uint8_t*)0, expected_file);

        dmResourceProvider::Result result;
        uint32_t file_size;

        result = dmResourceProvider::GetFileSize(m_Archive, path_hash, path, &file_size);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        ASSERT_EQ(expected_file_size, file_size);

        result = dmResourceProvider::ReadFile(m_Archive, path_hash, path, short_buffer, sizeof(short_buffer));
        ASSERT_EQ(dmResourceProvider::RESULT_INVAL_ERROR, result);

        uint8_t* buffer = new uint8_t[file_size];

        result = dmResourceProvider::ReadFile(m_Archive, path_hash, path, buffer, file_size);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

        ASSERT_ARRAY_EQ_LEN(expected_file, buffer, file_size);

        delete[] buffer;
        dmMemory::AlignedFree((void*)expected_file);
    }

    if (GetParam().m_ExtraFiles)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(EXTRA_FILE_PATHS); ++i)
        {
            const char* path = EXTRA_FILE_PATHS[i];
            dmhash_t path_hash = dmHashString64(path);

            uint32_t expected_file_size;
            const uint8_t* expected_file = GetRawFile(path, &expected_file_size, true);
            ASSERT_NE((uint8_t*)0, expected_file);

            dmResourceProvider::Result result;
            uint32_t file_size;

            result = dmResourceProvider::GetFileSize(m_Archive, path_hash, path, &file_size);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
            ASSERT_EQ(expected_file_size, file_size);

            dmMemory::AlignedFree((void*)expected_file);
        }
    }
}

ZipParams params_zip_archives[] = {
    {"zip:build/src/test/luresources.zip", true},
    {"zip:build/src/test/luresources_compressed.zip", false},
};

INSTANTIATE_TEST_CASE_P(ArchiveProviderZipTest, ArchiveProviderZip, jc_test_values_in(params_zip_archives));

int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
