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
#include "../resource.h"
#include "../resource_manifest.h"
#include "../resource_manifest_private.h"
#include "../resource_archive_private.h"
#include "../resource_util.h"
#include "../resource_verify.h"
#include "../providers/provider_archive_private.h"
#include <dlib/dstrings.h>
#include <dlib/endian.hpp>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <testmain/testmain.h>

#include "../resource_archive.h"
#include "../resource_private.h"


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>


template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmResource::Result r) {
    return buffer + JC_TEST_SNPRINTF(buffer, buffer_len, "%s", dmResource::ResultToString(r));
}

// new file format, generated test data
extern unsigned char RESOURCES_ARCI[];
extern uint32_t RESOURCES_ARCI_SIZE;
extern unsigned char RESOURCES_ARCD[];
extern uint32_t RESOURCES_ARCD_SIZE;
extern unsigned char RESOURCES_DMANIFEST[];
extern uint32_t RESOURCES_DMANIFEST_SIZE;
extern unsigned char RESOURCES_PUBLIC[];
extern uint32_t RESOURCES_PUBLIC_SIZE;
extern unsigned char RESOURCES_MANIFEST_HASH[];
extern uint32_t RESOURCES_MANIFEST_HASH_SIZE;

extern unsigned char RESOURCES_COMPRESSED_ARCI[];
extern uint32_t RESOURCES_COMPRESSED_ARCI_SIZE;
extern unsigned char RESOURCES_COMPRESSED_ARCD[];
extern uint32_t RESOURCES_COMPRESSED_ARCD_SIZE;
extern unsigned char RESOURCES_COMPRESSED_DMANIFEST[];
extern uint32_t RESOURCES_COMPRESSED_DMANIFEST_SIZE;

// An archive without any live update files
extern unsigned char RESOURCES_NO_LU_ARCI[];
extern uint32_t RESOURCES_NO_LU_ARCI_SIZE;
extern unsigned char RESOURCES_NO_LU_ARCD[];
extern uint32_t RESOURCES_NO_LU_ARCD_SIZE;
extern unsigned char RESOURCES_NO_LU_DMANIFEST[];
extern uint32_t RESOURCES_NO_LU_DMANIFEST_SIZE;

static const uint64_t path_hash[]       = { 0x1db7f0530911b1ce, 0x68b7e06402ee965c, 0x731d3cc48697dfe4, 0x8417331f14a42e4b,  0xb4870d43513879ba,  0xe1f97b41134ff4a6, 0xe7b921ca4d761083 };
static const char* path_name[]          = { "/archive_data/file4.adc",
                                            "/archive_data/liveupdate.file6.scriptc",
                                            "/archive_data/file5.scriptc",
                                            "/archive_data/file1.adc",
                                            "/archive_data/file3.adc",
                                            "/archive_data/file2.adc",
                                            "/archive_data/liveupdate.file7.adc" };

static const char* content[]            = {
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
    "this script was loaded sometime in runtime with liveupdate",
    "stuff to test encryption",
    "file1_datafile1_datafile1_data",
    "file3_data",
    "file2_datafile2_datafile2_data",
    "liveupdatefile1_datafile1_datafile1_data"
};

static const uint64_t liveupdate_path_hash[2] = { 0x68b7e06402ee965c, 0xe7b921ca4d761083 };

static const uint8_t content_hash[][20] = {
    { 127U, 144U,   0U,  37U, 122U,  73U,  24U, 215U,   7U,  38U,  85U, 234U,  70U, 133U,  64U, 205U, 203U, 212U,  46U,  12U },
    { 205U,  82U, 220U, 208U,  16U, 146U, 230U, 113U, 118U,  43U,   6U,  77U,  19U,  47U, 181U, 219U, 201U,  63U,  81U, 143U },
    {  95U, 158U,  27U, 108U, 112U,  93U, 159U, 220U, 188U,  65U, 128U,  98U, 243U, 234U,  63U, 106U,  51U, 100U,   9U,  20U },
    { 225U, 251U, 249U, 131U,  22U, 226U, 178U, 216U, 248U, 181U, 222U, 168U, 119U, 247U,  11U,  53U, 176U,  14U,  43U, 170U },
    {   3U,  86U, 172U, 159U, 110U, 187U, 139U, 211U, 219U,   5U, 203U, 115U, 150U,  43U, 182U, 252U, 136U, 228U, 122U, 181U },
    {  69U,  26U,  15U, 239U, 138U, 110U, 167U, 120U, 214U,  38U, 144U, 200U,  19U, 102U,  63U,  48U, 173U,  41U,  21U,  66U },
    {  90U,  15U,  50U,  67U, 184U,   5U, 147U, 194U, 160U, 203U,  45U, 150U,  20U, 194U,  55U, 123U, 189U, 218U, 105U, 103U }
};
static const uint8_t compressed_content_hash[][20] = {
    { 206U, 246U, 241U, 188U, 170U, 142U,  34U, 244U, 115U,  87U,  65U,  38U,  88U,  34U, 188U,  33U, 144U,  44U,  18U,  46U },
    { 205U,  82U, 220U, 208U,  16U, 146U, 230U, 113U, 118U,  43U,   6U,  77U,  19U,  47U, 181U, 219U, 201U,  63U,  81U, 143U },
    { 0x29, 0xB0, 0x62, 0xDB, 0x2F, 0xDD, 0x91, 0x9F, 0xB6, 0x81, 0xF8, 0x43, 0xB5, 0x64, 0xF2, 0x96, 0xA2, 0xD0, 0x0B, 0x50 },
    { 110U, 207U, 167U,  68U,  57U, 224U,  20U,  24U, 135U, 248U, 166U, 192U, 197U, 173U,  48U, 150U,   3U,  64U, 180U,  88U },
    { 0x42, 0x31, 0xB3, 0xD1, 0x76, 0x31, 0xC4, 0x64, 0xAE, 0x92, 0xAA, 0xC0, 0x7C, 0xA8, 0x05, 0xA7, 0xF4, 0x84, 0xB5, 0x7C },
    {  16U, 184U, 254U, 147U, 172U,  48U,  89U, 214U,  29U,  90U, 128U, 156U,  37U,  60U, 100U,  69U, 246U, 252U, 122U,  99U },
    {  90U,  15U,  50U,  67U, 184U,   5U, 147U, 194U, 160U, 203U,  45U, 150U,  20U, 194U,  55U, 123U, 189U, 218U, 105U, 103U }
};

bool IsLiveUpdateResource(dmhash_t lu_path_hash)
{
    for (uint32_t i = 0; i < (sizeof(liveupdate_path_hash) / sizeof(liveupdate_path_hash[0])); ++i)
    {
        if (lu_path_hash == liveupdate_path_hash[i])
        {
            return true;
        }
    }
    return false;
}

TEST(dmResourceArchive, ManifestHeader)
{
    dmResource::HManifest manifest = 0;
    dmLiveUpdateDDF::ManifestData* manifest_data;
    dmResource::Result result = dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(dmResource::MANIFEST_VERSION, manifest->m_DDF->m_Version);

    manifest_data = manifest->m_DDFData;

    ASSERT_EQ(dmLiveUpdateDDF::HASH_SHA1, manifest_data->m_Header.m_ResourceHashAlgorithm);
    ASSERT_EQ(dmLiveUpdateDDF::HASH_SHA256, manifest_data->m_Header.m_SignatureHashAlgorithm);

    ASSERT_EQ(dmLiveUpdateDDF::SIGN_RSA, manifest_data->m_Header.m_SignatureSignAlgorithm);

    dmResource::DeleteManifest(manifest);
}

TEST(dmResourceArchive, HasLiveupdateContent_MatchesArchiveManifestFlag)
{
    dmResource::HManifest manifest = 0;
    dmResource::Result result = dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(dmResource::MANIFEST_VERSION, manifest->m_DDF->m_Version);

    ASSERT_EQ(manifest->m_DDFData->m_HasExcludedResources, dmResource::HasManifestExcludedEntries(manifest));

    dmResource::DeleteManifest(manifest);
}

TEST(dmResourceArchive, HasLiveupdateContent_False)
{
    dmResource::HManifest manifest = 0;
    dmResource::Result result = dmResource::LoadManifestFromBuffer(RESOURCES_NO_LU_DMANIFEST, RESOURCES_NO_LU_DMANIFEST_SIZE, &manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(dmResource::MANIFEST_VERSION, manifest->m_DDF->m_Version);

    ASSERT_FALSE(dmResource::HasManifestExcludedEntries(manifest));

    dmResource::DeleteManifest(manifest);
}

TEST(dmResourceArchive, HasLiveupdateContent_TrueFromManifestFlag)
{
    dmResource::Manifest manifest;
    dmLiveUpdateDDF::ManifestData manifest_data;
    manifest.m_DDFData = &manifest_data;
    manifest_data.m_HasExcludedResources = true;

    ASSERT_TRUE(dmResource::HasManifestExcludedEntries(&manifest));
}

TEST(dmResourceArchive, HasLiveupdateContent_FalseWithoutManifestFlag)
{
    dmResource::Manifest manifest;
    dmLiveUpdateDDF::ManifestData manifest_data;
    dmLiveUpdateDDF::ResourceEntry entries[1];
    manifest.m_DDFData = &manifest_data;
    manifest_data.m_HasExcludedResources = false;
    manifest_data.m_Resources.m_Count = 1;
    manifest_data.m_Resources.m_Data = entries;
    entries[0].m_Flags = dmLiveUpdateDDF::EXCLUDED;

    ASSERT_FALSE(dmResource::HasManifestExcludedEntries(&manifest));
}

TEST(dmResourceArchive, ResourceEntries)
{
    dmResource::HManifest manifest = 0;
    dmLiveUpdateDDF::ManifestData* manifest_data;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest));

    manifest_data = manifest->m_DDFData;

    ASSERT_EQ(7U, manifest_data->m_Resources.m_Count);
    for (uint32_t i = 0; i < manifest_data->m_Resources.m_Count; ++i) {
        const char* current_path = manifest_data->m_Resources.m_Data[i].m_Url;
        uint64_t current_hash = dmHashString64(current_path);

        if (IsLiveUpdateResource(current_hash)) continue;

        ASSERT_STREQ(path_name[i], current_path);
        ASSERT_EQ(path_hash[i], current_hash);

        ASSERT_ARRAY_EQ_LEN(content_hash[i], manifest_data->m_Resources.m_Data[i].m_Hash.m_Data.m_Data, manifest_data->m_Resources.m_Data[i].m_Hash.m_Data.m_Count);
    }

    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ResourceEntries_Compressed)
{
    dmResource::HManifest manifest = 0;
    dmLiveUpdateDDF::ManifestData* manifest_data;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::LoadManifestFromBuffer(RESOURCES_COMPRESSED_DMANIFEST, RESOURCES_COMPRESSED_DMANIFEST_SIZE, &manifest));

    manifest_data = manifest->m_DDFData;

    ASSERT_EQ(7U, manifest_data->m_Resources.m_Count);
    for (uint32_t i = 0; i < manifest_data->m_Resources.m_Count; ++i) {
        const char* current_path = manifest_data->m_Resources.m_Data[i].m_Url;
        uint64_t current_hash = dmHashString64(current_path);

        if (IsLiveUpdateResource(current_hash)) continue;

        ASSERT_STREQ(path_name[i], current_path);
        ASSERT_EQ(path_hash[i], current_hash);

        dmLiveUpdateDDF::HashDigest* digest = &manifest_data->m_Resources.m_Data[i].m_Hash;
        ASSERT_ARRAY_EQ_LEN(compressed_content_hash[i], digest->m_Data.m_Data, digest->m_Data.m_Count);
    }

    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, Wrap)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCI_SIZE, true, RESOURCES_ARCD, RESOURCES_ARCD_SIZE, true, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData* entry;
    for (uint32_t i = 0; i < (sizeof(path_hash) / sizeof(path_hash[0])); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };

        result = dmResourceArchive::FindEntry(archive, content_hash[i], sizeof(content_hash[i]), &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::ReadEntry(archive, entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STREQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, sizeof(invalid_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, Wrap_Compressed)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_COMPRESSED_ARCI, RESOURCES_COMPRESSED_ARCI_SIZE, true, (void*) RESOURCES_COMPRESSED_ARCD, RESOURCES_COMPRESSED_ARCD_SIZE, true, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData* entry;
    for (uint32_t i = 0; i < (sizeof(path_hash) / sizeof(path_hash[0])); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, compressed_content_hash[i], sizeof(compressed_content_hash[i]), &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::ReadEntry(archive, entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STREQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, sizeof(invalid_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, LoadFromDisk)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/resources.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/resources.arcd");

    dmResourceArchive::Result result = dmResourceArchive::LoadArchiveFromFile(archive_path, resource_path, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData* entry;
    for (uint32_t i = 0; i < sizeof(path_name)/sizeof(path_name[0]); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, content_hash[i], sizeof(content_hash[i]), &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::ReadEntry(archive, entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STREQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, sizeof(invalid_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, LoadFromDisk_MissingArchive)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/missing-archive.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/resources.arcd");
    dmResourceArchive::Result result = dmResourceArchive::LoadArchiveFromFile(archive_path, resource_path, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_IO_ERROR, result);
}

TEST(dmResourceArchive, LoadFromDisk_Compressed)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/resources_compressed.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/resources_compressed.arcd");
    dmResourceArchive::Result result = dmResourceArchive::LoadArchiveFromFile(archive_path, resource_path, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData* entry;
    for (uint32_t i = 0; i < sizeof(path_name)/sizeof(path_name[0]); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, compressed_content_hash[i], sizeof(compressed_content_hash[i]), &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::ReadEntry(archive, entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STREQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, sizeof(invalid_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}


static dmResource::Result TestDecryption(void* buffer, uint32_t buffer_len)
{
    uint8_t* b = (uint8_t*)buffer;
    for (int i=0; i<buffer_len; i++)
    {
        b[i] = i;
    }
    return dmResource::RESULT_OK;
}

TEST(dmResourceArchive, ResourceDecryption)
{
    uint8_t buffer[] = { 0x00, 0x00, 0x00 };
    uint32_t buffer_len = 3;

    // test the default decryption (using Xtea)
    dmResource::Result result = dmResource::DecryptBuffer(buffer, buffer_len);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    uint8_t expected_buffer_xtea[] = { 0xE7, 0xF0, 0x00 };
    ASSERT_ARRAY_EQ_LEN(expected_buffer_xtea, buffer, DM_ARRAY_SIZE(buffer));

    // set a custom decryption function and test that it works
    dmResource::RegisterResourceDecryptionFunction(TestDecryption);
    result = dmResource::DecryptBuffer(buffer, buffer_len);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    uint8_t expected_buffer_custom[] = { 0x00, 0x01, 0x02 };
    ASSERT_ARRAY_EQ_LEN(expected_buffer_custom, buffer, DM_ARRAY_SIZE(buffer));

    // reset the custom decryption function (to Xtea)
    memset(buffer, 0, sizeof(buffer));
    dmResource::RegisterResourceDecryptionFunction(0);
    result = dmResource::DecryptBuffer(buffer, buffer_len);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(expected_buffer_xtea, buffer, DM_ARRAY_SIZE(buffer));
}

int main(int argc, char **argv)
{
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    return ret;
}
