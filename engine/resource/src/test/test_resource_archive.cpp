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

#if defined(__linux__) && !defined(__ANDROID__) && !defined(_LARGEFILE64_SOURCE)
#define _LARGEFILE64_SOURCE 1
#endif

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#if defined(_WIN32)
#include <io.h>
#include <dlib/safe_windows.h>
#ifndef _MSC_VER
#include <windows.h>
#endif
#include <winioctl.h>
#elif defined(__ANDROID__)
#include <fcntl.h>
#include <unistd.h>
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#endif
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

static FILE* TestOpenLargeFileForWrite(const char* path)
{
#if defined(__ANDROID__)
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE, 0666);
    if (fd < 0)
    {
        return 0;
    }
    FILE* file = fdopen(fd, "wb");
    if (!file)
    {
        close(fd);
        return 0;
    }
    setvbuf(file, 0, _IONBF, 0);
    return file;
#elif defined(__linux__)
    return fopen64(path, "wb");
#else
    return fopen(path, "wb");
#endif
}

static int TestSeekFile64(FILE* file, uint64_t offset)
{
#if defined(_WIN32)
    return _fseeki64(file, (int64_t)offset, SEEK_SET);
#elif defined(__ANDROID__)
    return lseek64(fileno(file), (off64_t)offset, SEEK_SET) < 0 ? -1 : 0;
#elif defined(__linux__)
    return fseeko64(file, (off64_t)offset, SEEK_SET);
#else
    return fseeko(file, (off_t)offset, SEEK_SET);
#endif
}

static bool TestMarkFileSparse(FILE* file)
{
#if defined(_WIN32)
    HANDLE handle = (HANDLE)_get_osfhandle(_fileno(file));
    if (handle == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD bytes_returned = 0;
    return DeviceIoControl(handle, FSCTL_SET_SPARSE, 0, 0, 0, 0, &bytes_returned, 0) != 0;
#else
    return true;
#endif
}

static bool WriteArchiveHeaderWithVersion(const char* archive_path, uint32_t version)
{
    FILE* file = fopen(archive_path, "wb");
    if (!file)
    {
        return false;
    }

    dmResourceArchive::ArchiveIndex header;
    memset(&header, 0, sizeof(header));
    header.m_Version = dmEndian::ToNetwork(version);

    bool ok = fwrite(&header, 1, sizeof(header), file) == sizeof(header);
    fclose(file);
    return ok;
}

static bool WriteArchiveIndexV6(const char* archive_path, const uint8_t* hash, uint32_t hash_len, uint64_t resource_offset, uint32_t resource_size, uint32_t flags)
{
    FILE* file = fopen(archive_path, "wb");
    if (!file)
    {
        return false;
    }

    dmResourceArchive::ArchiveIndex header;
    memset(&header, 0, sizeof(header));
    header.m_Version = dmEndian::ToNetwork(dmResourceArchive::VERSION_6);
    header.m_EntryDataCount = dmEndian::ToNetwork(1U);
    header.m_EntryDataOffset = dmEndian::ToNetwork((uint32_t)(sizeof(dmResourceArchive::ArchiveIndex) + dmResourceArchive::MAX_HASH));
    header.m_HashOffset = dmEndian::ToNetwork((uint32_t)sizeof(dmResourceArchive::ArchiveIndex));
    header.m_HashLength = dmEndian::ToNetwork(hash_len);

    uint8_t hash_buffer[dmResourceArchive::MAX_HASH] = { 0 };
    memcpy(hash_buffer, hash, hash_len);

    bool ok = fwrite(&header, 1, sizeof(header), file) == sizeof(header);
    ok = ok && fwrite(hash_buffer, 1, sizeof(hash_buffer), file) == sizeof(hash_buffer);

    dmResourceArchive::EntryData entry;
    entry.m_ResourceDataOffsetAndFlags = dmEndian::ToNetwork(dmResourceArchive::PackEntryDataOffsetAndFlags(resource_offset, flags));
    entry.m_ResourceSize = dmEndian::ToNetwork(resource_size);
    entry.m_ResourceCompressedSize = dmEndian::ToNetwork(0xFFFFFFFFU);
    ok = ok && fwrite(&entry, 1, sizeof(entry), file) == sizeof(entry);

    fclose(file);
    return ok;
}

static bool WriteLargeOffsetArchiveData(const char* resource_path, uint64_t resource_offset, const uint8_t* payload, uint32_t payload_size)
{
    FILE* file = TestOpenLargeFileForWrite(resource_path);
    if (!file)
    {
        return false;
    }

    bool ok = TestMarkFileSparse(file);
    ok = ok && TestSeekFile64(file, resource_offset) == 0;
    ok = ok && fwrite(payload, 1, payload_size, file) == payload_size;
    fclose(file);
    return ok;
}

static void WriteLargeOffsetArchiveV6(const char* archive_path, const char* resource_path, const uint8_t* hash, uint32_t hash_len, uint64_t resource_offset, const uint8_t* payload, uint32_t payload_size, uint32_t flags)
{
    remove(archive_path);
    remove(resource_path);

    ASSERT_TRUE(WriteArchiveIndexV6(archive_path, hash, hash_len, resource_offset, payload_size, flags));
    ASSERT_TRUE(WriteLargeOffsetArchiveData(resource_path, resource_offset, payload, payload_size));
}

static void AssertArchiveEntryReadable(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, uint64_t resource_offset, uint32_t flags, const uint8_t* payload, uint32_t payload_size)
{
    dmResourceArchive::EntryData* entry = 0;
    dmResourceArchive::Result result = dmResourceArchive::FindEntry(archive, hash, hash_len, &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(resource_offset, dmResourceArchive::GetEntryResourceDataOffset(entry));
    ASSERT_EQ(flags, dmResourceArchive::GetEntryFlags(entry));

    uint8_t buffer[16] = { 0 };
    ASSERT_LE(payload_size, (uint32_t)sizeof(buffer));
    result = dmResourceArchive::ReadEntry(archive, entry, buffer);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(payload, buffer, payload_size);

    uint8_t partial_buffer[4] = { 0 };
    uint32_t nread = 0;
    result = dmResourceArchive::ReadEntryPartial(archive, entry, 2, sizeof(partial_buffer), partial_buffer, &nread);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ((uint32_t)sizeof(partial_buffer), nread);
    ASSERT_ARRAY_EQ_LEN(payload + 2, partial_buffer, sizeof(partial_buffer));
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

// New builds no longer need v5 archive compatibility. This minimal v5 index
// verifies the engine rejects the archive version before attempting to load
// hashes or entries using the old layout.
TEST(dmResourceArchive, LoadFromDisk_RejectsVersion5Archive)
{
    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/version5_unsupported.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/version5_unsupported.arcd");

    remove(archive_path);
    remove(resource_path);

    ASSERT_TRUE(WriteArchiveHeaderWithVersion(archive_path, 5));

    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::LoadArchiveFromFile(archive_path, resource_path, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_VERSION_MISMATCH, result);
    ASSERT_EQ((dmResourceArchive::HArchiveIndexContainer)0, archive);

    remove(archive_path);
    remove(resource_path);
}

// V6 exists to let .arcd data grow past the uint32 offset limit while keeping resource
// sizes uint32. The entry packs the currently unused fourth flag bit with an offset
// above 4 GiB, writes a tiny payload there in a sparse .arcd, and verifies the decoded
// flags plus full and partial file-backed reads.
TEST(dmResourceArchive, LoadFromDisk_ResourceOffsetAbove4GiB)
{
    const uint64_t resource_offset = 0x100000010ULL;
    const uint32_t flags = 1 << 3;
    const uint8_t hash[20] = {
        0x43, 0xe9, 0xa1, 0x2b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90, 0xab,
        0xbc, 0xcd, 0xde, 0xef, 0x10, 0x21, 0x32, 0x43, 0x54, 0x65
    };
    const uint8_t payload[] = { 0x41, 0x52, 0x43, 0x36, 0xde, 0xad, 0xbe, 0xef };

    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/large_offset_v6_sparse.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/large_offset_v6_sparse.arcd");

    WriteLargeOffsetArchiveV6(archive_path, resource_path, hash, sizeof(hash), resource_offset, payload, sizeof(payload), flags);

    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::LoadArchiveFromFile(archive_path, resource_path, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    AssertArchiveEntryReadable(archive, hash, sizeof(hash), resource_offset, flags, payload, sizeof(payload));

    dmResourceArchive::Delete(archive);
    remove(archive_path);
    remove(resource_path);
}

// Platform mount code may choose mmap instead of the file-backed archive reader. This
// verifies that mounting a local v6 archive with data above 4 GiB still allows full and
// partial reads through the mounted archive handle.
TEST(dmResourceArchive, MountArchiveInternal_ResourceOffsetAbove4GiB)
{
    const uint64_t resource_offset = 0x100000010ULL;
    const uint32_t flags = 1 << 3;
    const uint8_t hash[20] = {
        0x63, 0xd9, 0xa1, 0x2b, 0x5c, 0x6d, 0x7e, 0x8f, 0x90, 0xab,
        0xbc, 0xcd, 0xde, 0xef, 0x10, 0x21, 0x32, 0x43, 0x54, 0x66
    };
    const uint8_t payload[] = { 0x4d, 0x4e, 0x54, 0x36, 0xde, 0xad, 0xbe, 0xef };

    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/mount_large_offset_v6_sparse.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/mount_large_offset_v6_sparse.arcd");

    WriteLargeOffsetArchiveV6(archive_path, resource_path, hash, sizeof(hash), resource_offset, payload, sizeof(payload), flags);

    dmResourceArchive::HArchiveIndexContainer archive = 0;
    void* mount_info = 0;
    dmResource::Result mount_result = dmResource::MountArchiveInternal(archive_path, resource_path, &archive, &mount_info);
    ASSERT_EQ(dmResource::RESULT_OK, mount_result);

    AssertArchiveEntryReadable(archive, hash, sizeof(hash), resource_offset, flags, payload, sizeof(payload));

    dmResource::UnmountArchiveInternal(archive, mount_info);
    remove(archive_path);
    remove(resource_path);
}

// File-backed archives keep hash/entry arrays beside the loaded index. Replacing the
// active index must discard those arrays, otherwise lookups continue using the old index.
TEST(dmResourceArchive, SetNewArchiveIndex_ClearsFileBackedLookupArrays)
{
    const uint8_t old_hash[20] = {
        0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x9a,
        0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0, 0x10, 0x20, 0x30, 0x40
    };
    const uint8_t new_hash[20] = {
        0x02, 0x13, 0x24, 0x35, 0x46, 0x57, 0x68, 0x79, 0x8a, 0x9b,
        0xac, 0xbd, 0xce, 0xdf, 0xe0, 0xf1, 0x11, 0x21, 0x31, 0x41
    };
    const uint8_t payload[] = { 'r', 'e', 'p', 'l', 'a', 'c', 'e', 0 };

    char archive_path[512];
    char resource_path[512];
    dmTestUtil::MakeHostPath(archive_path, sizeof(archive_path), "build/src/test/replace_index_v6.arci");
    dmTestUtil::MakeHostPath(resource_path, sizeof(resource_path), "build/src/test/replace_index_v6.arcd");

    remove(archive_path);
    remove(resource_path);

    ASSERT_TRUE(WriteArchiveIndexV6(archive_path, old_hash, sizeof(old_hash), 0, sizeof(payload), 0));
    ASSERT_TRUE(WriteLargeOffsetArchiveData(resource_path, 0, payload, sizeof(payload)));

    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::LoadArchiveFromFile(archive_path, resource_path, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    struct ArchiveIndexV6Buffer
    {
        dmResourceArchive::ArchiveIndex m_Header;
        uint8_t m_Hash[dmResourceArchive::MAX_HASH];
        dmResourceArchive::EntryData m_Entry;
    };

    ArchiveIndexV6Buffer replacement;
    memset(&replacement, 0, sizeof(replacement));
    replacement.m_Header.m_Version = dmEndian::ToNetwork(dmResourceArchive::VERSION_6);
    replacement.m_Header.m_EntryDataCount = dmEndian::ToNetwork(1U);
    replacement.m_Header.m_EntryDataOffset = dmEndian::ToNetwork((uint32_t)((uintptr_t)&replacement.m_Entry - (uintptr_t)&replacement));
    replacement.m_Header.m_HashOffset = dmEndian::ToNetwork((uint32_t)((uintptr_t)&replacement.m_Hash - (uintptr_t)&replacement));
    replacement.m_Header.m_HashLength = dmEndian::ToNetwork((uint32_t)sizeof(new_hash));
    memcpy(replacement.m_Hash, new_hash, sizeof(new_hash));
    replacement.m_Entry.m_ResourceDataOffsetAndFlags = dmEndian::ToNetwork(dmResourceArchive::PackEntryDataOffsetAndFlags(0, 0));
    replacement.m_Entry.m_ResourceSize = dmEndian::ToNetwork((uint32_t)sizeof(payload));
    replacement.m_Entry.m_ResourceCompressedSize = dmEndian::ToNetwork(0xFFFFFFFFU);

    dmResourceArchive::SetNewArchiveIndex(archive, &replacement.m_Header, true);

    dmResourceArchive::EntryData* entry = 0;
    result = dmResourceArchive::FindEntry(archive, old_hash, sizeof(old_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    result = dmResourceArchive::FindEntry(archive, new_hash, sizeof(new_hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    uint8_t buffer[sizeof(payload)] = { 0 };
    result = dmResourceArchive::ReadEntry(archive, entry, buffer);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(payload, buffer, sizeof(payload));

    dmResourceArchive::Delete(archive);
    remove(archive_path);
    remove(resource_path);
}

// Bundled and mapped archives do not go through the file-load normalization path, so v6
// lookup must work directly from the 16-byte packed entry layout. This in-memory v6 index
// contains one mapped entry with the currently unused fourth flag bit and verifies
// FindEntry plus ReadEntry against mapped payload bytes.
TEST(dmResourceArchive, Wrap_Version6)
{
    const uint32_t flags = 1 << 3;
    const uint8_t hash[20] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
        0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x10, 0x20, 0x30, 0x40
    };
    const uint8_t payload[] = { 'v', '6', '_', 'w', 'r', 'a', 'p', 0 };

    struct ArchiveIndexV6Buffer
    {
        dmResourceArchive::ArchiveIndex m_Header;
        uint8_t m_Hash[dmResourceArchive::MAX_HASH];
        dmResourceArchive::EntryData m_Entry;
    };

    ArchiveIndexV6Buffer index_buffer;
    memset(&index_buffer, 0, sizeof(index_buffer));
    index_buffer.m_Header.m_Version = dmEndian::ToNetwork(dmResourceArchive::VERSION_6);
    index_buffer.m_Header.m_EntryDataCount = dmEndian::ToNetwork(1U);
    index_buffer.m_Header.m_EntryDataOffset = dmEndian::ToNetwork((uint32_t)((uintptr_t)&index_buffer.m_Entry - (uintptr_t)&index_buffer));
    index_buffer.m_Header.m_HashOffset = dmEndian::ToNetwork((uint32_t)((uintptr_t)&index_buffer.m_Hash - (uintptr_t)&index_buffer));
    index_buffer.m_Header.m_HashLength = dmEndian::ToNetwork((uint32_t)sizeof(hash));
    memcpy(index_buffer.m_Hash, hash, sizeof(hash));
    index_buffer.m_Entry.m_ResourceDataOffsetAndFlags = dmEndian::ToNetwork(dmResourceArchive::PackEntryDataOffsetAndFlags(0, flags));
    index_buffer.m_Entry.m_ResourceSize = dmEndian::ToNetwork((uint32_t)sizeof(payload));
    index_buffer.m_Entry.m_ResourceCompressedSize = dmEndian::ToNetwork(0xFFFFFFFFU);

    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer(&index_buffer, sizeof(index_buffer), true, payload, sizeof(payload), true, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    dmResourceArchive::EntryData* entry = 0;
    result = dmResourceArchive::FindEntry(archive, hash, sizeof(hash), &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(flags, dmResourceArchive::GetEntryFlags(entry));

    uint8_t buffer[sizeof(payload)] = { 0 };
    result = dmResourceArchive::ReadEntry(archive, entry, buffer);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_ARRAY_EQ_LEN(payload, buffer, sizeof(payload));

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
