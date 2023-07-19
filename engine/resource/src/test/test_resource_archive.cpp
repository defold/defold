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
#include "../resource.h"
#include "../resource_manifest.h"
#include "../resource_manifest_private.h"
#include "../resource_archive_private.h"
#include "../resource_util.h"
#include "../resource_verify.h"
#include "../providers/provider_archive_private.h"
#include <dlib/dstrings.h>
#include <dlib/endian.h>
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

static const uint8_t sorted_first_hash[20] =
    {  0U,   1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U };
static const uint8_t sorted_middle_hash[20] =
    {  70U,  250U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U };
static const uint8_t sorted_last_hash[20] =
    { 226U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U };

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
    { 0x5F, 0x9E, 0x1B, 0x6C, 0x70, 0x5D, 0x9F, 0xDC, 0xBC, 0x41, 0x80, 0x62, 0xF3, 0xEA, 0x3F, 0x6A, 0x33, 0x64, 0x09, 0x14 },
    { 110U, 207U, 167U,  68U,  57U, 224U,  20U,  24U, 135U, 248U, 166U, 192U, 197U, 173U,  48U, 150U,   3U,  64U, 180U,  88U },
    { 0x03, 0x56, 0xAC, 0x9F, 0x6E, 0xBB, 0x8B, 0xD3, 0xDB, 0x05, 0xCB, 0x73, 0x96, 0x2B, 0xB6, 0xFC, 0x88, 0xE4, 0x7A, 0xB5 },
    {  16U, 184U, 254U, 147U, 172U,  48U,  89U, 214U,  29U,  90U, 128U, 156U,  37U,  60U, 100U,  69U, 246U, 252U, 122U,  99U },
    {  90U,  15U,  50U,  67U, 184U,   5U, 147U, 194U, 160U, 203U,  45U, 150U,  20U, 194U,  55U, 123U, 189U, 218U, 105U, 103U }
};

static const uint32_t ENTRY_SIZE = sizeof(dmResourceArchive::EntryData) + dmResourceArchive::MAX_HASH;

static void PopulateLiveUpdateResource(dmResourceArchive::LiveUpdateResource*& resource)
{
    uint32_t count = strlen(content[0]);
    resource->m_Data = (uint8_t*)content[0];
    resource->m_Count = count;
    resource->m_Header->m_Flags = 0;
    resource->m_Header->m_Size = 0;
}

// Call to this should be free'd with FreeMutableIndexData(...)
static uint32_t GetMutableIndexData(void*& arci_data, uint32_t num_entries_to_be_added)
{
    uint32_t index_alloc_size = RESOURCES_ARCI_SIZE + ENTRY_SIZE * num_entries_to_be_added;
    arci_data = (void*)new uint8_t[index_alloc_size];
    memcpy(arci_data, RESOURCES_ARCI, RESOURCES_ARCI_SIZE);
    return index_alloc_size;
}

static void FreeMutableIndexData(void*& arci_data)
{
    delete[] (uint8_t*)arci_data; // it is new[] uint8_t from the resource_archive.cpp
}

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

TEST(dmResourceArchive, ShiftInsertResource)
{
    const char* resource_filename = "test_resource_liveupdate.arcd";
    char host_name[512];
    const char* path = dmTestUtil::MakeHostPath(host_name, sizeof(host_name), resource_filename);

    FILE* resource_file = fopen(path, "wb");
    bool success = resource_file != 0x0;
    ASSERT_EQ(success, true);

    // Resource data to insert
    dmResourceArchive::LiveUpdateResource* resource = (dmResourceArchive::LiveUpdateResource*)malloc(sizeof(dmResourceArchive::LiveUpdateResource));
    resource->m_Header = (dmResourceArchive::LiveUpdateResourceHeader*)malloc(sizeof(dmResourceArchive::LiveUpdateResourceHeader));
    PopulateLiveUpdateResource(resource);

    // Use copy since we will shift/insert data
    dmResourceArchive::HArchiveIndex arci_copy;
    uint32_t arci_size = GetMutableIndexData((void*&)arci_copy, 1);

    // Init archive container
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) arci_copy, arci_size, true, RESOURCES_ARCD, RESOURCES_ARCD_SIZE, false, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    archive->m_ArchiveFileIndex->m_FileResourceData = resource_file;

    // dmResourceArchive::SetDefaultReader(archive);

    uint32_t entry_count_before = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(5U, entry_count_before);

    // Insertion
    int index = -1;
    dmResourceArchive::GetInsertionIndex(archive, sorted_middle_hash, &index);
    ASSERT_TRUE(index >= 0);
    dmResourceArchive::Result insert_result = dmResourceArchive::ShiftAndInsert(archive, arci_copy, sorted_middle_hash, 20, index, resource, 0x0);
    ASSERT_EQ(insert_result, dmResourceArchive::RESULT_OK);
    uint32_t entry_count_after = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(6U, entry_count_after);

    // Find inserted entry in archive after insertion
    dmResourceArchive::EntryData* entry;
    result = dmResourceArchive::FindEntry(archive, sorted_middle_hash, sizeof(sorted_middle_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(resource->m_Count, dmEndian::ToNetwork(entry->m_ResourceSize));

    int cmp = dmResource::VerifyArchiveIndex(archive);
    ASSERT_EQ(0, cmp);

    free(resource->m_Header);
    free(resource);
    dmResourceArchive::Delete(archive); // fclose on the FILE*
    FreeMutableIndexData((void*&)arci_copy);
    dmSys::Unlink(path);
}


TEST(dmResourceArchive, ShiftInsertResource_InsertIssue)
{
    const char* resource_filename = "test_resource_liveupdate.arcd";
    char host_name[512];
    const char* path = dmTestUtil::MakeHostPath(host_name, sizeof(host_name), resource_filename);

    FILE* resource_file = fopen(path, "wb");
    bool success = resource_file != 0x0;
    ASSERT_EQ(success, true);

    // Resource data to insert
    dmResourceArchive::LiveUpdateResource* resource = (dmResourceArchive::LiveUpdateResource*)malloc(sizeof(dmResourceArchive::LiveUpdateResource));
    resource->m_Header = (dmResourceArchive::LiveUpdateResourceHeader*)malloc(sizeof(dmResourceArchive::LiveUpdateResourceHeader));
    PopulateLiveUpdateResource(resource);

    dmResourceArchive::HArchiveIndexContainer archive = new dmResourceArchive::ArchiveIndexContainer;
    archive->m_ArchiveIndex = new dmResourceArchive::ArchiveIndex;
    archive->m_ArchiveIndex->m_HashLength = dmEndian::ToHost(20U);
    archive->m_IsMemMapped = true;
    archive->m_ArchiveFileIndex = new dmResourceArchive::ArchiveFileIndex;
    archive->m_ArchiveFileIndex->m_ResourceSize = RESOURCES_ARCD_SIZE;
    archive->m_ArchiveFileIndex->m_ResourceData = RESOURCES_ARCD;
    archive->m_ArchiveFileIndex->m_FileResourceData = resource_file;
    archive->m_ArchiveFileIndex->m_IsMemMapped = false;

    // dmResourceArchive::SetDefaultReader(archive);

    dmResourceArchive::ArchiveIndex* ai_temp = 0;
    dmResourceArchive::NewArchiveIndexFromCopy(ai_temp, archive, 3);

    delete archive->m_ArchiveIndex;
    archive->m_ArchiveIndex = ai_temp;

    uint32_t entry_count_before = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(0U, entry_count_before);

    // we got:
    // 0: '9170a12e266ec41f5575bb8f837d0f4bb6cab03f'
    // 1: '07f1284d530deb401da0ed142da50d5724cb04cd'
    // 2: '3be32cfd80b4016a9ebaf1c7249396ef608c95bd'
    const uint8_t hash1[20] = {0x07, 0xf1, 0x28, 0x4d, 0x53, 0x0d, 0xeb, 0x40, 0x1d, 0xa0, 0xed, 0x14, 0x2d, 0xa5, 0x0d, 0x57, 0x24, 0xcb, 0x04, 0xcd};
    const uint8_t hash2[20] = {0x91, 0x70, 0xa1, 0x2e, 0x26, 0x6e, 0xc4, 0x1f, 0x55, 0x75, 0xbb, 0x8f, 0x83, 0x7d, 0x0f, 0x4b, 0xb6, 0xca, 0xb0, 0x3f};
    const uint8_t hash3[20] = {0x3b, 0xe3, 0x2c, 0xfd, 0x80, 0xb4, 0x01, 0x6a, 0x9e, 0xba, 0xf1, 0xc7, 0x24, 0x93, 0x96, 0xef, 0x60, 0x8c, 0x95, 0xbd};
    // but we wanted:
    // 0: '07f1284d530deb401da0ed142da50d5724cb04cd'
    // 1: '3be32cfd80b4016a9ebaf1c7249396ef608c95bd'
    // 2: '9170a12e266ec41f5575bb8f837d0f4bb6cab03f'

    // Insertion
    int index = -1;
    dmResourceArchive::GetInsertionIndex(archive, hash1, &index);
    ASSERT_EQ(0, index);

    dmResourceArchive::Result insert_result = dmResourceArchive::ShiftAndInsert(archive, ai_temp, hash1, sizeof(hash1), index, resource, 0x0);
    ASSERT_EQ(insert_result, dmResourceArchive::RESULT_OK);

    uint32_t entry_count_after = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(1U, entry_count_after);

    dmResourceArchive::GetInsertionIndex(archive, hash2, &index);
    ASSERT_EQ(1, index);

    insert_result = dmResourceArchive::ShiftAndInsert(archive, ai_temp, hash2, sizeof(hash2), index, resource, 0x0);
    ASSERT_EQ(insert_result, dmResourceArchive::RESULT_OK);

    entry_count_after = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(2U, entry_count_after);

    dmResourceArchive::GetInsertionIndex(archive, hash3, &index);
    ASSERT_EQ(1, index);

    insert_result = dmResourceArchive::ShiftAndInsert(archive, ai_temp, hash3, sizeof(hash3), index, resource, 0x0);
    ASSERT_EQ(insert_result, dmResourceArchive::RESULT_OK);

    entry_count_after = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(3U, entry_count_after);

    int cmp = dmResource::VerifyArchiveIndex(archive);
    ASSERT_EQ(0, cmp);

    // Find inserted entry in archive after insertion
    dmResourceArchive::Result result;
    dmResourceArchive::EntryData* entry;
    result = dmResourceArchive::FindEntry(archive, hash1, sizeof(hash1), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(resource->m_Count, dmEndian::ToNetwork(entry->m_ResourceSize));
    result = dmResourceArchive::FindEntry(archive, hash2, sizeof(hash2), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(resource->m_Count, dmEndian::ToNetwork(entry->m_ResourceSize));
    result = dmResourceArchive::FindEntry(archive, hash3, sizeof(hash3), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(resource->m_Count, dmEndian::ToNetwork(entry->m_ResourceSize));

    free(resource->m_Header);
    free(resource);
    dmResourceArchive::Delete(archive); // fclose on the FILE*
    FreeMutableIndexData((void*&)ai_temp);
    dmSys::Unlink(path);
}

TEST(dmResourceArchive, NewArchiveIndexFromCopy)
{
    uint32_t single_entry_offset = dmResourceArchive::MAX_HASH;

    dmResourceArchive::HArchiveIndexContainer archive_container = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCI_SIZE, true, RESOURCES_ARCD, RESOURCES_ARCD_SIZE, true, &archive_container);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(368U, dmResourceArchive::GetEntryDataOffset(archive_container));

    // No extra allocation
    dmResourceArchive::HArchiveIndex dst_archive = 0;
    dmResourceArchive::NewArchiveIndexFromCopy(dst_archive, archive_container, 0);
    ASSERT_EQ(368U, dmResourceArchive::GetEntryDataOffset(dst_archive));
    dmResourceArchive::Delete(dst_archive);

    // Allocate space for 3 extra entries
    dst_archive = 0;
    dmResourceArchive::NewArchiveIndexFromCopy(dst_archive, archive_container, 3);
    ASSERT_EQ(368U + 3 * single_entry_offset, dmResourceArchive::GetEntryDataOffset(dst_archive));
    dmResourceArchive::Delete(dst_archive);

    dmResourceArchive::Delete(archive_container);
}

TEST(dmResourceArchive, GetInsertionIndex)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCI_SIZE, true, RESOURCES_ARCD, RESOURCES_ARCD_SIZE, true, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(5U, dmResourceArchive::GetEntryCount(archive));

    // Saving it here in for debugging purposes
    // printf("Archive:\n");
    // dmResourceArchive::DebugArchiveIndex(archive);
    // printf("sorted_first_hash: "); dmResource::PrintHash(sorted_first_hash, sizeof(sorted_first_hash)); printf("\n");
    // printf("sorted_middle_hash: "); dmResource::PrintHash(sorted_middle_hash, sizeof(sorted_middle_hash)); printf("\n");
    // printf("sorted_last_hash: "); dmResource::PrintHash(sorted_last_hash, sizeof(sorted_last_hash)); printf("\n");

    int index = -1;

    dmResourceArchive::GetInsertionIndex(archive, sorted_first_hash, &index);
    ASSERT_EQ(0, index);

    dmResourceArchive::GetInsertionIndex(archive, sorted_middle_hash, &index);
    ASSERT_EQ(2, index);

    dmResourceArchive::GetInsertionIndex(archive, sorted_last_hash, &index);
    ASSERT_EQ(5, index);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, ManifestHeader)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
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

static void PrintHash(const uint8_t* hash, uint32_t len)
{
    char* buf = new char[len*2+1];
    buf[len] = '\0';
    for (uint32_t i = 0; i < len; ++i)
    {
        dmSnPrintf(buf+i*2, 3, "%02X", hash[i]);
    }
    printf("HASH: %s\n", buf);
    delete[] buf;
}

static void PrintArray(uint8_t* a, uint32_t size)
{
    uint32_t left = size;
    uint32_t stride = 32;
    while (left > 0) {
        if (left > 8) {
            for (int i = 0; i < 8; ++i, ++a) {
                printf("%02X ", *a);
            }
            left -= 8;
            printf(" ");
        } else {
            for (int i = 0; i < left; ++i, ++a) {
                printf("%02X ", *a);
            }
            left  = 0;
        }
        stride -= 8;
        if (stride == 0) {
            printf("\n");
            stride = 32;
        }
    }
}

/*
This test is failing intermittenly on Linux. Typical output from a failed test:

2020-04-19T09:57:44.1778093Z ManifestSignatureVerification
2020-04-19T09:57:44.1779593Z PUBLIC KEY (sz: 162):
2020-04-19T09:57:44.1780587Z
2020-04-19T09:57:44.1782352Z 30 81 9F 30 0D 06 09 2A  86 48 86 F7 0D 01 01 01  05 00 03 81 8D 00 30 81  89 02 81 81 00 A4 6F BC
2020-04-19T09:57:44.1784180Z 37 EF BA F9 60 0D 98 02  AE 97 44 B2 52 C0 0F 0E  73 95 7B 97 B0 B7 08 6D  F2 D7 4E C3 37 92 7C BF
2020-04-19T09:57:44.1785946Z 55 DD 3A BD 8D 4D 73 94  3F 1B 69 70 86 23 DE 07  6D FF 46 86 17 6E 64 5B  8C 1F 36 AA 16 65 87 91
2020-04-19T09:57:44.1787741Z 53 F4 1C 9F AC C9 31 0C  93 BA E8 CB BB 5A AE F9  FD DC 2F C0 10 5B 6A 1B  0B DE 22 50 B3 E6 D9 AD
2020-04-19T09:57:44.1789537Z A7 E2 1C 92 D1 69 CA 93  25 AC 98 28 02 EF 1F F6  BF 67 7F 59 FB 1B 54 B0  29 DC 24 91 8D 02 03 01
2020-04-19T09:57:44.1790849Z 00 01
2020-04-19T09:57:44.1791703Z
2020-04-19T09:57:44.1792941Z MANIFEST SIGNATURE (sz: 128):
2020-04-19T09:57:44.1793804Z
2020-04-19T09:57:44.1795548Z 32 7B 7A DE B2 CE CB A1  A6 B1 34 1D EF F7 51 36  8D 7A A1 B1 90 49 5A 2E  A9 D7 FB 69 05 6A 5D B9
2020-04-19T09:57:44.1797361Z DC 94 91 EC E9 15 00 4E  51 49 C8 99 60 A6 F0 5F  DC 2B 8C 27 FB C8 70 5A  4C A5 40 9C D7 7E DD 95
2020-04-19T09:57:44.1799205Z 28 D1 8F 37 5E DA 60 16  52 84 12 BA E6 74 5D 09  ED 43 43 DD FA 47 91 D1  9A DC AA DB C7 CC DE B1
2020-04-19T09:57:44.1802608Z DF 30 C9 BA 2A 0A A8 A4  F1 4F 9D 8C AB 68 80 98  D2 0E F9 AB 00 80 B2 9C  21 09 D2 36 37 25 16 AF
2020-04-19T09:57:44.1805102Z
2020-04-19T09:57:44.1807577Z
2020-04-19T09:57:44.1810518Z ../src/test/test_resource_archive.cpp:392:
2020-04-19T09:57:44.1814584Z Expected: (dmResource::RESULT_OK) == (dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len)), actual: OK vs INVALID_DATA
*/
#if !defined(__linux__)
TEST(dmResourceArchive, ManifestSignatureVerification)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest));

    uint32_t expected_digest_len = dmResource::HashLength(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm);
    uint8_t* expected_digest = (uint8_t*)RESOURCES_MANIFEST_HASH;

    // We have an intermittent fail here, so let's output the info so we can start investigating it.
    // We always print these so that we can compare the failed build with a successful one
    {
        // Print out the source data, so that we can perhaps reproduce locally
        printf("\nRESOURCES_DMANIFEST (sz: %u):\n\n", RESOURCES_DMANIFEST_SIZE);
        PrintArray(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE);
        printf("\n");

        printf("\nRESOURCES_ARCD (sz: %u):\n\n", RESOURCES_ARCD_SIZE);
        PrintArray(RESOURCES_ARCD, RESOURCES_ARCD_SIZE);
        printf("\n");

        printf("\nRESOURCES_ARCI (sz: %u):\n\n", RESOURCES_ARCI_SIZE);
        PrintArray(RESOURCES_ARCI, RESOURCES_ARCI_SIZE);
        printf("\n");

        //
        printf("\nPUBLIC KEY (sz: %u):\n\n", RESOURCES_PUBLIC_SIZE);
        PrintArray(RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE);
        printf("\n");

        uint8_t* signature = manifest->m_DDF->m_Signature.m_Data;
        uint32_t signature_len = manifest->m_DDF->m_Signature.m_Count;
        printf("\nMANIFEST SIGNATURE (sz: %u):\n\n", signature_len);
        PrintArray(signature, signature_len);
        printf("\n");

        printf("Expected digest (%u bytes):\n", expected_digest_len);
        PrintHash((const uint8_t*)expected_digest, expected_digest_len);
    }
    uint8_t* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len));

    // debug prints to determine cause of intermittent test fail on both Linux/macOS
    printf("Actual digest (%u bytes):\n", hex_digest_len);
    PrintHash((const uint8_t*)hex_digest, hex_digest_len);
    // end debug

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::MemCompare((const uint8_t*) hex_digest, hex_digest_len, (const uint8_t*) expected_digest, expected_digest_len));

    free(hex_digest);
    dmResource::DeleteManifest(manifest);
}
#endif

/*
This test is failing intermittenly on Linux. Typical output from a failed test:

2020-04-24T11:09:51.7615960Z ManifestSignatureVerificationLengthFail
2020-04-24T11:09:51.7616210Z ../src/test/test_resource_archive.cpp:445:
2020-04-24T11:09:51.7616493Z Expected: (dmResource::RESULT_OK) == (dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len)), actual: OK vs INVALID_DATA
2020-04-24T11:09:51.7616663Z
*/
#if !defined(__linux__)
TEST(dmResourceArchive, ManifestSignatureVerificationLengthFail)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest));

    uint32_t expected_digest_len = dmResource::HashLength(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm);
    uint8_t* expected_digest = (uint8_t*)RESOURCES_MANIFEST_HASH;

    uint8_t* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len));
    hex_digest_len *= 0.5f; // make the supplied hash shorter than expected
    ASSERT_EQ(dmResource::RESULT_FORMAT_ERROR, dmResource::MemCompare(hex_digest, hex_digest_len, expected_digest, expected_digest_len));

    free(hex_digest);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}
#endif

/*
This test is failing intermittenly on Linux. Typical output from a failed test:

2020-04-28T05:00:04.1089407Z ManifestSignatureVerificationHashFail
2020-04-28T05:00:04.1089610Z ../src/test/test_resource_archive.cpp:475:
2020-04-28T05:00:04.1089868Z Expected: (dmResource::RESULT_OK) == (dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len)), actual: OK vs INVALID_DATA
*/
#if !defined(__linux__)
TEST(dmResourceArchive, ManifestSignatureVerificationHashFail)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest));

    uint32_t expected_digest_len = dmResource::HashLength(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm);
    uint8_t* expected_digest = (uint8_t*)RESOURCES_MANIFEST_HASH;

    uint8_t* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len));
    memset(hex_digest, 0x0, hex_digest_len / 2); // NULL out the first half of hash
    ASSERT_EQ(dmResource::RESULT_SIGNATURE_MISMATCH, dmResource::MemCompare(hex_digest, hex_digest_len, expected_digest, expected_digest_len));

    free(hex_digest);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}
#endif

TEST(dmResourceArchive, ManifestSignatureVerificationWrongKey)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::LoadManifestFromBuffer(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, &manifest));

    unsigned char* resources_public_wrong = (unsigned char*)malloc(RESOURCES_PUBLIC_SIZE);
    memcpy(resources_public_wrong, &RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE);
    resources_public_wrong[0] = RESOURCES_PUBLIC[0] + 1; // make the key invalid
    uint8_t* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, dmResource::DecryptSignatureHash(manifest, resources_public_wrong, RESOURCES_PUBLIC_SIZE, &hex_digest, &hex_digest_len));

    free(hex_digest);
    free(resources_public_wrong);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ResourceEntries)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
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

        for (uint32_t n = 0; n < manifest_data->m_Resources.m_Data[i].m_Hash.m_Data.m_Count; ++n) {
            uint8_t current_byte = manifest_data->m_Resources.m_Data[i].m_Hash.m_Data.m_Data[n];
            ASSERT_EQ(content_hash[i][n], current_byte);
        }
    }

    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ResourceEntries_Compressed)
{
    dmResource::HManifest manifest = new dmResource::Manifest();
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
