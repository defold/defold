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


#include "../resource_util.h"
#include "../resource_manifest.h"
#include "../providers/provider.h"
#include "../providers/provider_private.h"
#include "../providers/provider_archive.h"
#include <resource/liveupdate_ddf.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

typedef dmResourceProvider::ArchiveLoader ArchiveLoader;

extern unsigned char LURESOURCES_DMANIFEST[];
extern uint32_t LURESOURCES_DMANIFEST_SIZE;
extern unsigned char LURESOURCES_ARCI[];
extern uint32_t LURESOURCES_ARCI_SIZE;
extern unsigned char LURESOURCES_ARCD[];
extern uint32_t LURESOURCES_ARCD_SIZE;

// extern unsigned char RESOURCES_COMPRESSED_DMANIFEST[];
// extern uint32_t RESOURCES_COMPRESSED_DMANIFEST_SIZE;
// extern unsigned char RESOURCES_COMPRESSED_ARCI[];
// extern uint32_t RESOURCES_COMPRESSED_ARCI_SIZE;
// extern unsigned char RESOURCES_COMPRESSED_ARCD[];
// extern uint32_t RESOURCES_COMPRESSED_ARCD_SIZE;

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

const char* MISSING_FILE_PATHS[] = {
    "/archive_data/liveupdate.file6.scriptc",
    "/archive_data/liveupdate.file7.adc",
};

const uint8_t MISSING_FILE_FLAGS[] = {
    dmResourceArchive::ENTRY_FLAG_ENCRYPTED,
    0, // not encrypted
};

// ****************************************************************************************************************

TEST(ArchiveProviderBasic, Registered)
{
    dmResourceProvider::ArchiveLoader* loader;

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("mutable"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
    ASSERT_NE((ArchiveLoader*)0, loader);

    loader = dmResourceProvider::FindLoaderByName(dmHashString64("file"));
    ASSERT_EQ((ArchiveLoader*)0, loader);
}

TEST(ArchiveProviderBasic, CanMount)
{
    dmResourceProvider::ArchiveLoader* loader = dmResourceProvider::FindLoaderByName(dmHashString64("mutable"));
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
const char* s_MutableArchivePaths[] = {
        "build/src/test/luresources_dl.dmanifest",
        "build/src/test/luresources_dl.arci",
        "build/src/test/luresources_dl.arcd"
    };

class ArchiveProviderMutable : public jc_test_base_class
{
public:

    static void SetUpTestCase()
    {
        Cleanup();
        // Make sure the archive files don't exist yet
        for (int i = 0; i < DM_ARRAY_SIZE(s_MutableArchivePaths); ++i)
        {
            ASSERT_FALSE(dmSys::Exists(s_MutableArchivePaths[i]));
        }
    }

    static void TearDownTestCase()
    {
    }

protected:
    static void Cleanup()
    {
        for (int i = 0; i < DM_ARRAY_SIZE(s_MutableArchivePaths); ++i)
        {
            if (dmSys::Exists(s_MutableArchivePaths[i]))
            {
                dmLogInfo("Cleanup of %s", s_MutableArchivePaths[i]);
                dmSys::Unlink(s_MutableArchivePaths[i]);
            }
        }
    }

    virtual void SetUp()
    {
        m_BaseLoader = dmResourceProvider::FindLoaderByName(dmHashString64("archive"));
        ASSERT_NE((ArchiveLoader*)0, m_BaseLoader);
        m_Loader = dmResourceProvider::FindLoaderByName(dmHashString64("mutable"));
        ASSERT_NE((ArchiveLoader*)0, m_Loader);

        {
            dmURI::Parts uri;
            dmURI::Parse("dmanif:build/src/test/luresources", &uri);
            dmResourceProvider::Result result = dmResourceProvider::CreateMount(m_BaseLoader, &uri, 0, &m_BaseArchive);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        }
        {
            dmURI::Parts uri;
            dmURI::Parse("dmanif:build/src/test/luresources_dl", &uri);
            dmResourceProvider::Result result = dmResourceProvider::CreateMount(m_Loader, &uri, m_BaseArchive, &m_Archive);
            ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        }
    }

    virtual void TearDown()
    {
        dmResourceProvider::Result result;

        result = dmResourceProvider::Unmount(m_Archive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);

        result = dmResourceProvider::Unmount(m_BaseArchive);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
    }

    dmResourceProvider::HArchive       m_BaseArchive;
    dmResourceProvider::HArchive       m_Archive;
    dmResourceProvider::HArchiveLoader m_Loader;
    dmResourceProvider::HArchiveLoader m_BaseLoader;
};

TEST_F(ArchiveProviderMutable, GetSize)
{
    dmResourceProvider::Result result;
    uint32_t file_size;
    const char* path;

    path = "/archive_data/file1.adc";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);

    path = "src/test/files/not_exist";
    result = dmResourceProvider::GetFileSize(m_Archive, dmHashString64(path), path, &file_size);
    ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
}

// * Test that the files exist
// * Test that the content is the same as on disc
TEST_F(ArchiveProviderMutable, AfterSetup)
{
    uint8_t short_buffer[4] = {0};

    for (uint32_t i = 0; i < DM_ARRAY_SIZE(FILE_PATHS); ++i)
    {
        const char* path = FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);

        dmResourceProvider::Result result;
        uint32_t file_size;

        // The regular files shouldn't be found in the live update archive,
        // that's the job of the regular base archive
        result = dmResourceProvider::GetFileSize(m_Archive, path_hash, path, &file_size);
        ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);

        result = dmResourceProvider::ReadFile(m_Archive, path_hash, path, short_buffer, sizeof(short_buffer));
        ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
    }

    // We haven't added any
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(MISSING_FILE_PATHS); ++i)
    {
        const char* path = MISSING_FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);

        uint32_t file_size;
        dmResourceProvider::Result result = dmResourceProvider::GetFileSize(m_Archive, path_hash, path, &file_size);
        ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);

        result = dmResourceProvider::ReadFile(m_Archive, path_hash, path, 0, 0);
        ASSERT_EQ(dmResourceProvider::RESULT_NOT_FOUND, result);
    }
}

static uint8_t* CreateLiveupdateResource(const uint8_t* src, uint32_t src_len, bool encrypted, bool compressed, uint32_t* out_len)
{
    uint8_t* out = (uint8_t*)malloc(sizeof(dmResourceArchive::LiveUpdateResourceHeader) + src_len);
    dmResourceArchive::LiveUpdateResourceHeader* header = (dmResourceArchive::LiveUpdateResourceHeader*)out;
    memset(header, 0xED, sizeof(dmResourceArchive::LiveUpdateResourceHeader)); // same as in archive builder
    header->m_Size = dmEndian::ToHost(src_len);

    header->m_Flags = dmResourceArchive::ENTRY_FLAG_LIVEUPDATE_DATA;
    if (compressed)
        header->m_Flags |= dmResourceArchive::ENTRY_FLAG_COMPRESSED;
    if (encrypted)
        header->m_Flags |= dmResourceArchive::ENTRY_FLAG_ENCRYPTED;

    memcpy(out + sizeof(dmResourceArchive::LiveUpdateResourceHeader), src, src_len);
    *out_len = sizeof(dmResourceArchive::LiveUpdateResourceHeader) + src_len;
    return out;
}

static void PrintfBuffer(const uint8_t* buf, uint32_t buf_len)
{
    for (uint32_t i = 0; i < buf_len; ++i)
    {
        printf("%02X", (uint32_t)buf[i]);
    }
    printf("\n");
}

static void EncryptBuffer(uint8_t* buffer, uint32_t buffer_len)
{
    // The current algorithm is inplace and reversible ()
    dmResource::DecryptBuffer(buffer, buffer_len);
}

TEST_F(ArchiveProviderMutable, DownloadFiles)
{
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(MISSING_FILE_PATHS); ++i)
    {
        const char* path = MISSING_FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);

        bool encrypted = MISSING_FILE_FLAGS[i] & dmResourceArchive::ENTRY_FLAG_ENCRYPTED;

        char path_buffer[256];
        dmSnPrintf(path_buffer, sizeof(path_buffer), "build/src/test%s", path);

        uint32_t expected_file_size;
        uint8_t* expected_file = dmTestUtil::ReadHostFile(path_buffer, &expected_file_size);
        ASSERT_NE((uint8_t*)0, expected_file);

        if (encrypted)
        {
            printf("Before encryption: sz: %u ", expected_file_size); PrintfBuffer(expected_file, expected_file_size);
            EncryptBuffer(expected_file, expected_file_size);
            printf("After encryption: sz: %u ", expected_file_size); PrintfBuffer(expected_file, expected_file_size);
        }

        uint32_t lu_file_size = 0;
        uint8_t* lu_resource = CreateLiveupdateResource(expected_file, expected_file_size, encrypted, false, &lu_file_size);
        ASSERT_EQ(expected_file_size+sizeof(dmResourceArchive::LiveUpdateResourceHeader), lu_file_size);

        dmResourceProvider::WriteFile(m_Archive, path_hash, path, lu_resource, lu_file_size);

        free((void*)lu_resource);
        dmMemory::AlignedFree((void*)expected_file);
    }

    // We cannot read these files until the "reboot", i.e. the archive has been unmounted/mounted again
}

TEST_F(ArchiveProviderMutable, ReadDownloadedFiles)
{
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(MISSING_FILE_PATHS); ++i)
    {
        const char* path = MISSING_FILE_PATHS[i];
        dmhash_t path_hash = dmHashString64(path);
        //bool encrypted = MISSING_FILE_FLAGS[i] & dmResourceArchive::ENTRY_FLAG_ENCRYPTED;

        char path_buffer[256];
        dmSnPrintf(path_buffer, sizeof(path_buffer), "build/src/test%s", path);

        uint32_t expected_file_size;
        uint8_t* expected_file = dmTestUtil::ReadHostFile(path_buffer, &expected_file_size);
        ASSERT_NE((uint8_t*)0, expected_file);

        // ****************************************************************************************************************

        uint32_t file_size;
        dmResourceProvider::Result

        result = dmResourceProvider::GetFileSize(m_Archive, path_hash, path, &file_size);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        ASSERT_EQ(expected_file_size, file_size);

        uint8_t* buffer = new uint8_t[file_size];
        result = dmResourceProvider::ReadFile(m_Archive, path_hash, path, buffer, file_size);
        ASSERT_EQ(dmResourceProvider::RESULT_OK, result);
        ASSERT_ARRAY_EQ_LEN(expected_file, buffer, file_size);

        delete[] buffer;
        dmMemory::AlignedFree((void*)expected_file);
    }
}

TEST_F(ArchiveProviderMutable, GetUrlHashFromHexDigest)
{
    const char* digest1 = "5f9e1b6c705d9fdcbc418062f3ea3f6a33640914"; // url: 731d3cc48697dfe4  /archive_data/file5.scriptc
    const char* digest2 = "aae1f8cc01c23ba6067f7ed81df5a187a047aa7f"; // url: 68b7e06402ee965c  /archive_data/liveupdate.file6.scriptc
    const uint32_t digest_len = strlen(digest1);

    dmhash_t digest_hash1 = dmHashBuffer64(digest1, digest_len);
    dmhash_t digest_hash2 = dmHashBuffer64(digest2, digest_len);

    dmhash_t url_hash1 = 0; // We only grab the live update resources from this manifest, so all other entries will return 0
    dmhash_t url_hash2 = dmHashString64("/archive_data/liveupdate.file6.scriptc");

    dmResource::HManifest manifest;
    dmResourceProvider::GetManifest(m_Archive, &manifest);
    ASSERT_NE(nullptr, manifest);

    ASSERT_EQ(url_hash1, dmResource::GetUrlHashFromHexDigest(manifest, digest_hash1));
    ASSERT_EQ(url_hash2, dmResource::GetUrlHashFromHexDigest(manifest, digest_hash2));
}


int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);
    dmLog::LogParams logparams;
    dmLog::LogInitialize(&logparams);

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
