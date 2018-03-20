#ifndef RESOURCE_ARCHIVE_PRIVATE_H
#define RESOURCE_ARCHIVE_PRIVATE_H

#include <stdio.h>
#include <stdint.h>

#include "resource_archive.h"
#include <dlib/path.h>

#define MD5_SIZE (16)

namespace dmResourceArchive
{
    enum EntryFlag
    {
        ENTRY_FLAG_ENCRYPTED        = 1 << 0,
        ENTRY_FLAG_COMPRESSED       = 1 << 1,
        ENTRY_FLAG_LIVEUPDATE_DATA  = 1 << 2,
    };

    struct DM_ALIGNED(16) ArchiveIndex
    {
        ArchiveIndex()
        {
            memset(this, 0, sizeof(ArchiveIndex));
        }

        uint32_t m_Version;
        uint32_t m_Pad;
        uint64_t m_Userdata;
        uint32_t m_EntryDataCount;
        uint32_t m_EntryDataOffset;
        uint32_t m_HashOffset;
        uint32_t m_HashLength;
        uint8_t  m_ArchiveIndexMD5[MD5_SIZE];
    };

    struct ArchiveIndexContainer
    {
        ArchiveIndexContainer()
        {
            memset(this, 0, sizeof(ArchiveIndexContainer));
        }

        ArchiveIndex* m_ArchiveIndex; // this could be mem-mapped or loaded into memory from file
        bool m_IsMemMapped;
        bool m_ResourcesMemMapped;
        bool m_LiveUpdateResourcesMemMapped;

        /// Used if the archive is loaded from file (bundled archive)
        uint8_t* m_Hashes;
        EntryData* m_Entries;
        uint8_t* m_ResourceData;
        FILE* m_FileResourceData; // game.arcd file handle

        /// Resources acquired with LiveUpdate
        char m_LiveUpdateResourcePath[DMPATH_MAX_PATH];
        uint8_t* m_LiveUpdateResourceData; // mem-mapped liveupdate.arcd
        uint32_t m_LiveUpdateResourceSize;
        FILE* m_LiveUpdateFileResourceData; // liveupdate.arcd file handle
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
	
	Result ShiftAndInsert(ArchiveIndexContainer* archive_container, ArchiveIndex* archive, const uint8_t* hash_digest, uint32_t hash_digest_len, int insertion_index, const dmResourceArchive::LiveUpdateResource* resource, const EntryData* entry);

	Result WriteResourceToArchive(ArchiveIndexContainer*& archive, const uint8_t* buf, uint32_t buf_len, uint32_t& bytes_written, uint32_t& offset);

	void NewArchiveIndexFromCopy(ArchiveIndex*& dst, ArchiveIndexContainer* src, uint32_t extra_entries_alloc);

    Result GetInsertionIndex(HArchiveIndexContainer archive, const uint8_t* hash_digest, int* index);
	
    Result GetInsertionIndex(ArchiveIndex* archive, const uint8_t* hash_digest, const uint8_t* hashes, int* index);

    void CacheLiveUpdateEntries(const ArchiveIndexContainer* archive_container, const ArchiveIndexContainer* bundled_archive_container, LiveUpdateEntries* lu_hashes_entries);

    uint32_t GetEntryDataOffset(ArchiveIndexContainer* archive_container);

    uint32_t GetEntryDataOffset(ArchiveIndex* archive);

    void Delete(ArchiveIndex* archive);

}
#endif // RESOURCE_ARCHIVE_PRIVATE_H