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
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/testutil.h>
#include <dlib/uri.h>

#include "../providers/provider.h"
#include "../providers/provider_private.h"
#include "../providers/provider_archive.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

extern unsigned char RESOURCES_DMANIFEST[];
extern uint32_t RESOURCES_DMANIFEST_SIZE;
extern unsigned char RESOURCES_ARCI[];
extern uint32_t RESOURCES_ARCI_SIZE;
extern unsigned char RESOURCES_ARCD[];
extern uint32_t RESOURCES_ARCD_SIZE;

extern unsigned char RESOURCES_COMPRESSED_DMANIFEST[];
extern uint32_t RESOURCES_COMPRESSED_DMANIFEST_SIZE;
extern unsigned char RESOURCES_COMPRESSED_ARCI[];
extern uint32_t RESOURCES_COMPRESSED_ARCI_SIZE;
extern unsigned char RESOURCES_COMPRESSED_ARCD[];
extern uint32_t RESOURCES_COMPRESSED_ARCD_SIZE;

// build/src/test/resources.dmanifest et al
// Manifest:
// entry: hash: 7F9000257A4918D7072655EA468540CDCBD42E0C url: 1db7f0530911b1ce  /archive_data/file4.adc
// entry: hash: AAE1F8CC01C23BA6067F7ED81DF5A187A047AA7F url: 68b7e06402ee965c  /archive_data/liveupdate.file6.scriptc
// entry: hash: 5F9E1B6C705D9FDCBC418062F3EA3F6A33640914 url: 731d3cc48697dfe4  /archive_data/file5.scriptc
// entry: hash: E1FBF98316E2B2D8F8B5DEA877F70B35B00E2BAA url: 8417331f14a42e4b  /archive_data/file1.adc
// entry: hash: 0356AC9F6EBB8BD3DB05CB73962BB6FC88E47AB5 url: b4870d43513879ba  /archive_data/file3.adc
// entry: hash: 451A0FEF8A6EA778D62690C813663F30AD291542 url: e1f97b41134ff4a6  /archive_data/file2.adc
// entry: hash: 5A0F3243B80593C2A0CB2D9614C2377BBDDA6967 url: e7b921ca4d761083  /archive_data/liveupdate.file7.adc
// Archive:
// entry: off:  224  sz:   10  csz: 4294967295 fgs:  0 hash: 0356AC9F6EBB8BD3DB05CB73962BB6FC88E47AB5
// entry: off:  236  sz:   30  csz: 4294967295 fgs:  0 hash: 451A0FEF8A6EA778D62690C813663F30AD291542
// entry: off:    0  sz:   40  csz: 4294967295 fgs:  4 hash: 5A0F3243B80593C2A0CB2D9614C2377BBDDA6967
// entry: off:  100  sz:   24  csz: 4294967295 fgs:  1 hash: 5F9E1B6C705D9FDCBC418062F3EA3F6A33640914
// entry: off:  124  sz:  100  csz: 4294967295 fgs:  0 hash: 7F9000257A4918D7072655EA468540CDCBD42E0C
// entry: off:   40  sz:   58  csz: 4294967295 fgs:  5 hash: AAE1F8CC01C23BA6067F7ED81DF5A187A047AA7F
// entry: off:  268  sz:   30  csz: 4294967295 fgs:  0 hash: E1FBF98316E2B2D8F8B5DEA877F70B35B00E2BAA


const char* FILE_PATHS[] = {
    "/archive_data/file1.adc",
    "/archive_data/file2.adc",
    "/archive_data/file3.adc",
    "/archive_data/file4.adc",
    "/archive_data/file5.scriptc",
};

// ****************************************************************************************************************

TEST(ArchiveProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_EQ((ArchiveLoader*)0, loader);
}

TEST(ArchiveProviderBasic, CanMount)
{
    dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    dmURI::Parts uri;
    dmURI::Parse("dmanif:build/src/test", &uri);
    ASSERT_TRUE(loader->m_CanMount(&uri));

    dmURI::Parse(".", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));

    dmURI::Parse("http://domain.com/path", &uri);
    ASSERT_FALSE(loader->m_CanMount(&uri));
}

// ****************************************************************************************************************

class ArchiveProviderArchive : public jc_test_base_class
{
protected:
    virtual void SetUp()
    {
        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
        ASSERT_NE((ArchiveLoader*)0, m_Loader);

        dmURI::Parts uri;
        dmURI::Parse("dmanif:build/src/test/resources", &uri);


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

TEST_F(ArchiveProviderArchive, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;
    const char* path;

    path = "/archive_data/file1.adc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(30U, file_size);

    path = "/archive_data/file4.adc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(100U, file_size);

    path = "src/test/files/not_exist";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

// * Test that the files exist
// * Test that the content is the same as on disc
TEST_F(ArchiveProviderArchive, ReadFile)
{
    uint8_t short_buffer[4] = {0};

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(FILE_PATHS); ++i)
    {
        const char* path = FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);

        uint32_t expected_file_size;

        char path_buffer1[256];
        dmSnPrintf(path_buffer1, sizeof(path_buffer1), "build/src/test%s", path);
        const uint8_t* expected_file = dmTestUtil::ReadHostFile(path_buffer1, &expected_file_size);
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
}

struct InMemoryParams
{
    uint8_t* m_ManifestData;
    uint32_t m_ManifestDataSize;
    uint8_t* m_ArciData;
    uint32_t m_ArciDataSize;
    uint8_t* m_ArcdData;
    uint32_t m_ArcdDataSize;
};

class ArchiveProviderArchiveInMemory : public jc_test_params_class<InMemoryParams>
{
protected:
    virtual void SetUp()
    {
        const InMemoryParams& params = GetParam();

        dmResourceProvider::Result result;

        dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
        ASSERT_NE((ArchiveLoader*)0, loader);

        dmResourceProvider::HArchiveInternal internal;
        result = dmResourceProviderArchive::CreateArchive(  params.m_ManifestData, params.m_ManifestDataSize,
                                                            params.m_ArciData, params.m_ArciDataSize,
                                                            params.m_ArcdData, params.m_ArcdDataSize,
                                                            &internal);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

        result = dmResourceProvider::CreateMount(loader, internal, &m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    virtual void TearDown()
    {
        dmResourceProvider::Result result = dmResourceProvider::Unmount(m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    dmResourceProvider::HArchive m_Archive;
};

TEST_P(ArchiveProviderArchiveInMemory, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;
    const char* path;

    path = "/archive_data/file1.adc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(30U, file_size);

    path = "/archive_data/file4.adc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    ASSERT_EQ(100U, file_size);

    path = "src/test/files/not_exist";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

TEST_P(ArchiveProviderArchiveInMemory, ReadFile)
{
    uint8_t short_buffer[4] = {0};

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(FILE_PATHS); ++i)
    {
        const char* path = FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);
        uint32_t expected_file_size;

        char path_buffer1[256];
        char path_buffer2[256];
        dmSnPrintf(path_buffer2, sizeof(path_buffer2), "build/src/test%s", path);
        const char* file_path = dmTestUtil::MakeHostPath(path_buffer1, sizeof(path_buffer1), path_buffer2);
        const uint8_t* expected_file = dmTestUtil::ReadFile(file_path, &expected_file_size);
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
}
InMemoryParams params_in_memory_archives[] = {
    {RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, RESOURCES_ARCI, RESOURCES_ARCI_SIZE, RESOURCES_ARCD, RESOURCES_ARCD_SIZE},
    {RESOURCES_COMPRESSED_DMANIFEST, RESOURCES_COMPRESSED_DMANIFEST_SIZE, RESOURCES_COMPRESSED_ARCI, RESOURCES_COMPRESSED_ARCI_SIZE, RESOURCES_COMPRESSED_ARCD, RESOURCES_COMPRESSED_ARCD_SIZE},
};

INSTANTIATE_TEST_CASE_P(ArchiveProviderInMemory, ArchiveProviderArchiveInMemory, jc_test_values_in(params_in_memory_archives));


int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
