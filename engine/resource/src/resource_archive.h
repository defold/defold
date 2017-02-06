#ifndef RESOURCE_ARCHIVE_H
#define RESOURCE_ARCHIVE_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dlib/align.h>

#define C_TO_JAVA ntohl
#define JAVA_TO_C htonl

// Maximum hash length convention. This size should large enough.
// If this length changes the VERSION needs to be bumped.
#define DMRESOURCE_MAX_HASH (64) // Equivalent to 512 bits

namespace dmResourceArchive
{
    /**
     * This specifies the version of the resource archive and allows the engine
     * to check a manifest to ensure that it's compatible with the engine's
     * version of the archive format.
     */
    const static uint32_t VERSION = 4;

    typedef struct ArchiveIndexContainer* HArchiveIndexContainer;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_NOT_FOUND = 1,
        RESULT_VERSION_MISMATCH = -1,
        RESULT_IO_ERROR = -2,
        RESULT_MEM_ERROR = -3,
        RESULT_OUTBUFFER_TOO_SMALL = -4,
        RESULT_ALREADY_STORED = -5,
        RESULT_UNKNOWN = -1000,
    };

    struct DM_ALIGNED(16) EntryData
    {
        EntryData() :
            m_ResourceDataOffset(0),
            m_ResourceSize(0),
            m_ResourceCompressedSize(0),
            m_Flags(0) {}

        uint32_t m_ResourceDataOffset;
        uint32_t m_ResourceSize;
        uint32_t m_ResourceCompressedSize; // 0xFFFFFFFF if uncompressed
        uint32_t m_Flags;
    };

    struct DM_ALIGNED(16) LiveUpdateResourceHeader {
        uint32_t m_Size;
        uint8_t m_Flags;
        uint8_t m_Padding[11];
    };

    struct LiveUpdateResource {
        LiveUpdateResource(const uint8_t* buf, size_t buflen) {
            m_Count = buflen - sizeof(LiveUpdateResourceHeader);
            m_Data = (uint8_t*)((uintptr_t)buf + sizeof(LiveUpdateResourceHeader));
            m_Header = (LiveUpdateResourceHeader*)((uintptr_t)buf);
        }

        const uint8_t* m_Data;
        size_t m_Count;
        LiveUpdateResourceHeader* m_Header;
    };

    struct LiveUpdateEntries {
        LiveUpdateEntries(const uint8_t* hashes, uint32_t hash_len, EntryData* entry_datas, uint32_t num_entries) {
            m_Hashes = hashes;
            m_HashLen = hash_len;
            m_Entries = entry_datas;
            m_Count = num_entries;
        }

        LiveUpdateEntries() {
            memset(this, 0, sizeof(LiveUpdateEntries));
        }

        const uint8_t* m_Hashes;
        uint32_t m_HashLen;
        EntryData* m_Entries;
        uint32_t m_Count;
    };

    /**
     * Wrap an archive index and data file already loaded in memory. Calling Delete() on wrapped
     * archive is not needed.
     * @param index_buffer archive index memory to wrap
     * @param index_buffer_size archive index size
     * @param resource_data resource data
     * @param lu_resource_data resource data acquired through liveupdate
     * @param archive archive index container handle
     * @return RESULT_OK on success
     */
    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, const void* resource_data, const char* lu_resource_filename, const void* lu_resource_data, FILE* f_lu_resource_data, HArchiveIndexContainer* archive);

    /**
     * Load archive from filename. Only the index data is loaded into memory.
     * Resources are loaded on-demand using Read2() function.
     * @param file_name archive index to load
     * @param archive archive index container handle
     * @return RESULT_OK on success
     */
    Result LoadArchive(const char* index_file_name, const char* data_file_name, const char* lu_data_file_path, HArchiveIndexContainer* archive);

    /**
     * Find resource entry within archive
     * @param archive archive index handle
     * @param hash resource hash digest to find
     * @param entry entry data
     * @return RESULT_OK on success
     */
    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, EntryData* entry);

    /**
     * Read resource
     * @param archive archive index handle
     * @param entry_data entry data
     * @param buffer buffer to load to
     * @return RESULT_OK on success
     */
    Result Read(HArchiveIndexContainer archive, EntryData* entry_data, void* buffer);

    /**
     * Delete archive index. Only required for archives created with LoadArchive function
     * @param archive archive index handle
     */
    void Delete(HArchiveIndexContainer archive);

    /**
     * Get total entries, i.e. files/resources in archive
     * @param archive archive index handle
     * @return entry count
     */
    uint32_t GetEntryCount(HArchiveIndexContainer archive);

    Result CalcInsertionIndex(HArchiveIndexContainer archive, const uint8_t* hash_digest, int& index);

    Result InsertResource(HArchiveIndexContainer archive, const uint8_t* hash_digest, uint32_t hash_digest_len, const dmResourceArchive::LiveUpdateResource* resource, const char* proj_id);

    Result ReloadBundledArchiveIndex(const char* bundled_index_path, const char* bundled_resourc_path, const char* lu_index_path, const char* lu_resource_path, ArchiveIndexContainer*& lu_index_container, void*& index_mount_info);

    int CmpArchiveIdentifier(const HArchiveIndexContainer archive_container, uint8_t* archive_id, uint32_t len);

}  // namespace dmResourceArchive

#endif
