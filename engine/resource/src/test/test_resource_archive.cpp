#include <stdint.h>
#include <gtest/gtest.h>
#include "../resource.h"
#include "../resource_private.h"
#include "../resource_archive.h"
#include "../resource_archive_private.h"

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

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
static const char* path_name[]          = { "/archive_data/file4.adc", "/archive_data/liveupdate.file6.scriptc", "/archive_data/file5.scriptc", "/archive_data/file1.adc", "/archive_data/file3.adc",  "/archive_data/file2.adc", "/archive_data/liveupdate.file7.adc" };
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
    {  95U, 158U,  27U, 108U, 112U,  93U, 159U, 220U, 188U,  65U, 128U,  98U, 243U, 234U,  63U, 106U,  51U, 100U,   9U,  20U },
    { 110U, 207U, 167U,  68U,  57U, 224U,  20U,  24U, 135U, 248U, 166U, 192U, 197U, 173U,  48U, 150U,   3U,  64U, 180U,  88U },
    {   3U,  86U, 172U, 159U, 110U, 187U, 139U, 211U, 219U,   5U, 203U, 115U, 150U,  43U, 182U, 252U, 136U, 228U, 122U, 181U },
    {  16U, 184U, 254U, 147U, 172U,  48U,  89U, 214U,  29U,  90U, 128U, 156U,  37U,  60U, 100U,  69U, 246U, 252U, 122U,  99U },
    {  90U,  15U,  50U,  67U, 184U,   5U, 147U, 194U, 160U, 203U,  45U, 150U,  20U, 194U,  55U, 123U, 189U, 218U, 105U, 103U }
};

static const uint32_t ENTRY_SIZE = sizeof(dmResourceArchive::EntryData) + DMRESOURCE_MAX_HASH;

void PopulateLiveUpdateResource(dmResourceArchive::LiveUpdateResource*& resource)
{
    uint32_t count = strlen(content[0]);
    resource->m_Data = (uint8_t*)content[0];
    resource->m_Count = count;
    resource->m_Header->m_Flags = 0;
    resource->m_Header->m_Size = 0;
}

void FreeLiveUpdateEntries(dmResourceArchive::LiveUpdateEntries*& liveupdate_entries)
{
    free(liveupdate_entries->m_Entries);
    free((void*)liveupdate_entries->m_Hashes);
    delete liveupdate_entries;
}

// Call to this should be free'd with FreeMutableIndexData(...)
void GetMutableIndexData(void*& arci_data, uint32_t num_entries_to_be_added)
{
    uint32_t index_alloc_size = RESOURCES_ARCI_SIZE + ENTRY_SIZE * num_entries_to_be_added;
    arci_data = malloc(index_alloc_size);
    memcpy(arci_data, RESOURCES_ARCI, RESOURCES_ARCI_SIZE);
}

// Call to this should be free'd with FreeMutableIndexData(...)
void GetMutableBundledIndexData(void*& arci_data, uint32_t& arci_size, uint32_t num_entries_to_keep)
{
    uint32_t num_lu_entries = 2 - num_entries_to_keep; // 2 LiveUpdate resources in archive in total
    ASSERT_EQ(true, num_lu_entries >= 0);
    arci_data = malloc(RESOURCES_ARCI_SIZE - ENTRY_SIZE * num_lu_entries);
    // Init archive container including LU resources
    dmResourceArchive::ArchiveIndexContainer* archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer(RESOURCES_ARCI, RESOURCES_ARCD, 0x0, 0x0, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

    uint32_t entry_count = htonl(archive->m_ArchiveIndex->m_EntryDataCount);
    uint32_t hash_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_HashOffset);
    uint32_t entries_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataOffset);
    dmResourceArchive::EntryData* entries = (dmResourceArchive::EntryData*)((uintptr_t)archive->m_ArchiveIndex + entries_offset);

    // Construct "bundled" archive
    uint8_t* cursor = (uint8_t*)arci_data;
    uint8_t* cursor_hash = (uint8_t*)((uintptr_t)arci_data + hash_offset);
    uint8_t* cursor_entry = (uint8_t*)((uintptr_t)arci_data + entries_offset - num_lu_entries * DMRESOURCE_MAX_HASH);
    memcpy(cursor, RESOURCES_ARCI, sizeof(dmResourceArchive::ArchiveIndex)); // Copy header
    int lu_entries_to_copy = num_entries_to_keep;
    for (int i = 0; i < entry_count; ++i)
    {
        dmResourceArchive::EntryData& e = entries[i];
        bool is_lu_entry = JAVA_TO_C(e.m_Flags) & dmResourceArchive::ENTRY_FLAG_LIVEUPDATE_DATA;
        if (!is_lu_entry || lu_entries_to_copy > 0)
        {
            if (is_lu_entry)
            {
                --lu_entries_to_copy;
            }

            memcpy(cursor_hash, (void*)((uintptr_t)RESOURCES_ARCI + hash_offset + DMRESOURCE_MAX_HASH * i), DMRESOURCE_MAX_HASH);
            memcpy(cursor_entry, &e, sizeof(dmResourceArchive::EntryData));

            cursor_hash = (uint8_t*)((uintptr_t)cursor_hash + DMRESOURCE_MAX_HASH);
            cursor_entry = (uint8_t*)((uintptr_t)cursor_entry + sizeof(dmResourceArchive::EntryData));
            
        }
    }
    dmResourceArchive::ArchiveIndex* ai = (dmResourceArchive::ArchiveIndex*)arci_data;
    ai->m_EntryDataOffset = C_TO_JAVA(entries_offset - num_lu_entries * DMRESOURCE_MAX_HASH);
    ai->m_EntryDataCount = C_TO_JAVA(entry_count - num_lu_entries);

    arci_size = sizeof(dmResourceArchive::ArchiveIndex) + JAVA_TO_C(ai->m_EntryDataCount) * (ENTRY_SIZE);

    dmResourceArchive::Delete(archive);
}

void FreeMutableIndexData(void*& arci_data)
{
    free(arci_data);
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

void CreateBundledArchive(dmResourceArchive::HArchiveIndexContainer& bundled_archive_container, dmResourceArchive::HArchiveIndex& bundled_archive_index, uint32_t num_entries_to_keep)
{
    bundled_archive_container = 0;
    bundled_archive_index = 0;
    uint32_t bundled_archive_size = 0;
    GetMutableBundledIndexData((void*&)bundled_archive_index, bundled_archive_size, num_entries_to_keep);
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*&) bundled_archive_index, RESOURCES_ARCD, 0x0, 0x0, 0x0, &bundled_archive_container);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(5U + num_entries_to_keep, dmResourceArchive::GetEntryCount(bundled_archive_container));
}

void FreeBundledArchive(dmResourceArchive::HArchiveIndexContainer& bundled_archive_container, dmResourceArchive::HArchiveIndex& bundled_archive_index)
{
    dmResourceArchive::Delete(bundled_archive_container);
    FreeMutableIndexData((void*&)bundled_archive_index);
}

TEST(dmResourceArchive, ShiftInsertResource)
{
    const char* resource_filename = "test_resource_liveupdate.arcd";
    FILE* resource_file = fopen(resource_filename, "wb");
    bool success = resource_file != 0x0;
    ASSERT_EQ(success, true);

    // Resource data to insert
    dmResourceArchive::LiveUpdateResource* resource = (dmResourceArchive::LiveUpdateResource*)malloc(sizeof(dmResourceArchive::LiveUpdateResource));
    resource->m_Header = (dmResourceArchive::LiveUpdateResourceHeader*)malloc(sizeof(dmResourceArchive::LiveUpdateResourceHeader));
    PopulateLiveUpdateResource(resource);

    // Use copy since we will shift/insert data
    uint8_t* arci_copy;
    GetMutableIndexData((void*&)arci_copy, 1);

    // Init archive container
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) arci_copy, RESOURCES_ARCD, resource_filename, 0x0, resource_file, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    uint32_t entry_count_before = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(7U, entry_count_before);

    // Insertion
    int index = -1;
    dmResourceArchive::GetInsertionIndex(archive, sorted_middle_hash, &index);
    ASSERT_TRUE(index >= 0);
    dmResourceArchive::Result insert_result = dmResourceArchive::ShiftAndInsert(archive, 0x0, sorted_middle_hash, 20, index, resource, 0x0);
    ASSERT_EQ(insert_result, dmResourceArchive::RESULT_OK);
    uint32_t entry_count_after = dmResourceArchive::GetEntryCount(archive);
    ASSERT_EQ(8U, entry_count_after);

    // Find inserted entry in archive after insertion
    dmResourceArchive::EntryData entry;
    result = dmResourceArchive::FindEntry(archive, sorted_middle_hash, &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(entry.m_ResourceSize, resource->m_Count);
    
    free(resource->m_Header);
    free(resource);
    dmResourceArchive::Delete(archive);
    FreeMutableIndexData((void*&)arci_copy);
}

TEST(dmResourceArchive, NewArchiveIndexFromCopy)
{
    uint32_t single_entry_offset = DMRESOURCE_MAX_HASH;

    dmResourceArchive::HArchiveIndexContainer archive_container = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCD, 0x0, 0x0, 0x0, &archive_container);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(496U, dmResourceArchive::GetEntryDataOffset(archive_container));

    // No extra allocation
    dmResourceArchive::HArchiveIndex dst_archive = 0;
    dmResourceArchive::NewArchiveIndexFromCopy(dst_archive, archive_container, 0);
    ASSERT_EQ(496U, dmResourceArchive::GetEntryDataOffset(dst_archive));
    dmResourceArchive::Delete(dst_archive);
    
    // Allocate space for 3 extra entries
    dst_archive = 0;
    dmResourceArchive::NewArchiveIndexFromCopy(dst_archive, archive_container, 3);
    ASSERT_EQ(496U + 3 * single_entry_offset, dmResourceArchive::GetEntryDataOffset(dst_archive));
    dmResourceArchive::Delete(dst_archive);

    dmResourceArchive::Delete(archive_container);
}

TEST(dmResourceArchive, CacheLiveUpdateEntries)
{
    dmResourceArchive::HArchiveIndexContainer bundled_archive_container;
    dmResourceArchive::ArchiveIndex* bundled_archive_index;

    dmResourceArchive::HArchiveIndexContainer archive_container = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCD, 0x0, 0x0, 0x0, &archive_container);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(7U, dmResourceArchive::GetEntryCount(archive_container));

    // Create "bundled" archive container that does not contain any LiveUpdate resources
    CreateBundledArchive(bundled_archive_container, bundled_archive_index, 0);
    dmResourceArchive::LiveUpdateEntries* liveupdate_entries = new dmResourceArchive::LiveUpdateEntries;
    dmResourceArchive::CacheLiveUpdateEntries(archive_container, bundled_archive_container, liveupdate_entries);
    ASSERT_EQ(2U, liveupdate_entries->m_Count);
    FreeLiveUpdateEntries(liveupdate_entries);
    FreeBundledArchive(bundled_archive_container, bundled_archive_index);

    // Create "bundled" archive container that has 1 LiveUpdate entry now in bundle instead
    CreateBundledArchive(bundled_archive_container, bundled_archive_index, 1);
    liveupdate_entries = new dmResourceArchive::LiveUpdateEntries;
    dmResourceArchive::CacheLiveUpdateEntries(archive_container, bundled_archive_container, liveupdate_entries);
    ASSERT_EQ(1U, liveupdate_entries->m_Count);
    FreeLiveUpdateEntries(liveupdate_entries);
    FreeBundledArchive(bundled_archive_container, bundled_archive_index);

    dmResourceArchive::Delete(archive_container);
}

TEST(dmResourceArchive, GetInsertionIndex)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCD, 0x0, 0x0, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(7U, dmResourceArchive::GetEntryCount(archive));

    int index = -1;

    dmResourceArchive::GetInsertionIndex(archive, sorted_first_hash, &index);
    ASSERT_EQ(0, index);

    dmResourceArchive::GetInsertionIndex(archive, sorted_middle_hash, &index);
    ASSERT_EQ(2, index);

    dmResourceArchive::GetInsertionIndex(archive, sorted_last_hash, &index);
    ASSERT_EQ(7, index);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, ManifestHeader)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    dmLiveUpdateDDF::ManifestData* manifest_data;
    dmResource::Result result = dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    manifest_data = manifest->m_DDFData;

    ASSERT_EQ(dmResource::MANIFEST_MAGIC_NUMBER, manifest_data->m_Header.m_MagicNumber);
    ASSERT_EQ(dmResource::MANIFEST_VERSION, manifest_data->m_Header.m_Version);

    ASSERT_EQ(dmLiveUpdateDDF::HASH_SHA1, manifest_data->m_Header.m_ResourceHashAlgorithm);
    ASSERT_EQ(dmLiveUpdateDDF::HASH_SHA256, manifest_data->m_Header.m_SignatureHashAlgorithm);

    ASSERT_EQ(dmLiveUpdateDDF::SIGN_RSA, manifest_data->m_Header.m_SignatureSignAlgorithm);

    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ManifestSignatureVerification)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest));

    uint32_t expected_digest_len = dmResource::HashLength(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm) * 2 + 1;
    char* expected_digest = (char*)malloc(expected_digest_len);
    dmResource::HashToString(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm, RESOURCES_MANIFEST_HASH, expected_digest, expected_digest_len);

    char* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, hex_digest, hex_digest_len));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::HashCompare((const uint8_t*) hex_digest, hex_digest_len, (const uint8_t*) expected_digest, expected_digest_len));

    free(expected_digest);
    free(hex_digest);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ManifestSignatureVerificationLengthFail)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest));

    uint32_t expected_digest_len = dmResource::HashLength(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm) * 2 + 1;
    char* expected_digest = (char*)malloc(expected_digest_len);
    dmResource::HashToString(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm, RESOURCES_MANIFEST_HASH, expected_digest, expected_digest_len);

    char* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, hex_digest, hex_digest_len));
    hex_digest_len *= 0.5f; // make the supplied hash shorter than expected
    ASSERT_EQ(dmResource::RESULT_FORMAT_ERROR, dmResource::HashCompare((const uint8_t*) hex_digest, hex_digest_len - 1, (const uint8_t*) expected_digest, strlen(expected_digest)));

    free(expected_digest);
    free(hex_digest);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ManifestSignatureVerificationHashFail)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest));

    uint32_t expected_digest_len = dmResource::HashLength(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm) * 2 + 1;
    char* expected_digest = (char*)malloc(expected_digest_len);
    dmResource::HashToString(manifest->m_DDFData->m_Header.m_SignatureHashAlgorithm, RESOURCES_MANIFEST_HASH, expected_digest, expected_digest_len);

    char* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::DecryptSignatureHash(manifest, RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE, hex_digest, hex_digest_len));
    memset(hex_digest, 0x0, hex_digest_len / 2); // NULL out the first half of hash
    ASSERT_EQ(dmResource::RESULT_FORMAT_ERROR, dmResource::HashCompare((const uint8_t*) hex_digest, hex_digest_len - 1, (const uint8_t*) expected_digest, strlen(expected_digest)));

    free(expected_digest);
    free(hex_digest);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, ManifestSignatureVerificationWrongKey)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest));

    unsigned char* resources_public_wrong = (unsigned char*)malloc(RESOURCES_PUBLIC_SIZE);
    memcpy(resources_public_wrong, &RESOURCES_PUBLIC, RESOURCES_PUBLIC_SIZE);
    resources_public_wrong[0] = RESOURCES_PUBLIC[0] + 1; // make the key invalid
    char* hex_digest = 0x0;
    uint32_t hex_digest_len;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, dmResource::DecryptSignatureHash(manifest, resources_public_wrong, RESOURCES_PUBLIC_SIZE, hex_digest, hex_digest_len));

    free(hex_digest);
    free(resources_public_wrong);
    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, BundleVersionValidSuccess)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest));

    dmResource::Result bundle_valid_result = dmResource::BundleVersionValid(manifest, "./resources.manifest_hash");
    ASSERT_EQ(dmResource::RESULT_OK, bundle_valid_result);

    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, BundleVersionValidVersionFail)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest));

    manifest->m_DDF->m_Signature.m_Data[0] += 1; // make the signature wrong
    dmResource::Result bundle_valid_result = dmResource::BundleVersionValid(manifest, "./resources.manifest_hash");
    ASSERT_EQ(dmResource::RESULT_VERSION_MISMATCH, bundle_valid_result);
}

TEST(dmResourceArchive, ResourceEntries)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    dmLiveUpdateDDF::ManifestData* manifest_data;
    dmResource::Result result = dmResource::ParseManifestDDF(RESOURCES_DMANIFEST, RESOURCES_DMANIFEST_SIZE, manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    manifest_data = manifest->m_DDFData;

    ASSERT_EQ(7U, manifest_data->m_Resources.m_Count);
    for (uint32_t i = 0; i < manifest_data->m_Resources.m_Count; ++i) {
        const char* current_path = manifest_data->m_Resources.m_Data[i].m_Url;
        uint64_t current_hash = dmHashString64(current_path);

        if (IsLiveUpdateResource(current_hash)) continue;

        ASSERT_STRCASEEQ(path_name[i], current_path);
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
    dmResource::Manifest* manifest = new dmResource::Manifest();
    dmLiveUpdateDDF::ManifestData* manifest_data;
    dmResource::Result result = dmResource::ParseManifestDDF(RESOURCES_COMPRESSED_DMANIFEST, RESOURCES_COMPRESSED_DMANIFEST_SIZE, manifest);
    ASSERT_EQ(dmResource::RESULT_OK, result);

    manifest_data = manifest->m_DDFData;

    ASSERT_EQ(7U, manifest_data->m_Resources.m_Count);
    for (uint32_t i = 0; i < manifest_data->m_Resources.m_Count; ++i) {
        const char* current_path = manifest_data->m_Resources.m_Data[i].m_Url;
        uint64_t current_hash = dmHashString64(current_path);

        if (IsLiveUpdateResource(current_hash)) continue;

        ASSERT_STRCASEEQ(path_name[i], current_path);
        ASSERT_EQ(path_hash[i], current_hash);

        for (uint32_t n = 0; n < manifest_data->m_Resources.m_Data[i].m_Hash.m_Data.m_Count; ++n) {
            uint8_t current_byte = manifest_data->m_Resources.m_Data[i].m_Hash.m_Data.m_Data[n];

            ASSERT_EQ(compressed_content_hash[i][n], current_byte);
        }
    }

    dmDDF::FreeMessage(manifest->m_DDFData);
    dmDDF::FreeMessage(manifest->m_DDF);
    delete manifest;
}

TEST(dmResourceArchive, Wrap)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_ARCI, RESOURCES_ARCD, 0x0, 0x0, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(7U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData entry;
    for (uint32_t i = 0; i < (sizeof(path_hash) / sizeof(path_hash[0])); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, content_hash[i], &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::Read(archive, &entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STRCASEEQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, Wrap_Compressed)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    dmResourceArchive::Result result = dmResourceArchive::WrapArchiveBuffer((void*) RESOURCES_COMPRESSED_ARCI, (void*) RESOURCES_COMPRESSED_ARCD, 0x0, 0x0, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(7U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData entry;
    for (uint32_t i = 0; i < (sizeof(path_hash) / sizeof(path_hash[0])); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, compressed_content_hash[i], &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::Read(archive, &entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STRCASEEQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, LoadFromDisk)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    const char* archive_path = "build/default/src/test/resources.arci";
    const char* resource_path = "build/default/src/test/resources.arcd";
    dmResourceArchive::Result result = dmResourceArchive::LoadArchive(archive_path, resource_path, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(7U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData entry;
    for (uint32_t i = 0; i < sizeof(path_name)/sizeof(path_name[0]); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, content_hash[i], &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::Read(archive, &entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STRCASEEQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

TEST(dmResourceArchive, LoadFromDisk_MissingArchive)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    const char* archive_path = "build/default/src/test/missing-archive.arci";
    const char* resource_path = "build/default/src/test/resources.arcd";
    dmResourceArchive::Result result = dmResourceArchive::LoadArchive(archive_path, resource_path, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_IO_ERROR, result);
}

TEST(dmResourceArchive, LoadFromDisk_Compressed)
{
    dmResourceArchive::HArchiveIndexContainer archive = 0;
    const char* archive_path = "build/default/src/test/resources_compressed.arci";
    const char* resource_path = "build/default/src/test/resources_compressed.arcd";
    dmResourceArchive::Result result = dmResourceArchive::LoadArchive(archive_path, resource_path, 0x0, &archive);
    ASSERT_EQ(dmResourceArchive::RESULT_OK, result);
    ASSERT_EQ(7U, dmResourceArchive::GetEntryCount(archive));

    dmResourceArchive::EntryData entry;
    for (uint32_t i = 0; i < sizeof(path_name)/sizeof(path_name[0]); ++i)
    {
        if (IsLiveUpdateResource(path_hash[i])) continue;

        char buffer[1024] = { 0 };
        result = dmResourceArchive::FindEntry(archive, compressed_content_hash[i], &entry);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        result = dmResourceArchive::Read(archive, &entry, buffer);
        ASSERT_EQ(dmResourceArchive::RESULT_OK, result);

        ASSERT_EQ(strlen(content[i]), strlen(buffer));
        ASSERT_STRCASEEQ(content[i], buffer);
    }

    uint8_t invalid_hash[] = { 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U, 10U };
    result = dmResourceArchive::FindEntry(archive, invalid_hash, &entry);
    ASSERT_EQ(dmResourceArchive::RESULT_NOT_FOUND, result);

    dmResourceArchive::Delete(archive);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
