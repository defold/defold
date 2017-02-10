#ifndef RESOURCE_ARCHIVE_PRIVATE_H
#define RESOURCE_ARCHIVE_PRIVATE_H

#include <stdio.h>
#include <stdint.h>

#include "resource_archive.h"
#include <dlib/path.h>

namespace dmResourceArchive
{
	struct ArchiveIndex;

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
	
	Result ShiftAndInsert(ArchiveIndexContainer* archive_container, ArchiveIndex* archive, const uint8_t* hash_digest, uint32_t hash_digest_len, uint32_t insertion_index, const dmResourceArchive::LiveUpdateResource* resource, const EntryData* entry);

	Result WriteResourceToArchive(ArchiveIndexContainer*& archive, const uint8_t* buf, uint32_t buf_len, uint32_t& bytes_written, uint32_t& offset);

	void DeepCopyArchiveIndex(ArchiveIndex*& dst, ArchiveIndexContainer* src, uint32_t extra_entries_alloc);
	
    Result CalcInsertionIndex(ArchiveIndex* archive, const uint8_t* hash_digest, const uint8_t* hashes, int& index);

    void StoreLiveUpdateEntries(const ArchiveIndexContainer* archive_container, LiveUpdateEntries* lu_hashes_entries, uint32_t& num_lu_entries);

    uint32_t GetEntryDataOffset(ArchiveIndexContainer* archive_container);

    uint32_t GetEntryDataOffset(ArchiveIndex* archive);

    void Delete(ArchiveIndex* archive);

}
#endif // RESOURCE_ARCHIVE_PRIVATE_H