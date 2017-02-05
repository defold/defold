#ifndef RESOURCE_ARCHIVE_PRIVATE_H
#define RESOURCE_ARCHIVE_PRIVATE_H

#include <stdio.h>
#include <stdint.h>

#include "resource_archive.h"
#include <dlib/path.h>

namespace dmResourceArchive
{
	struct ArchiveIndex;
	
	Result ShiftAndInsert(ArchiveIndexContainer* archive_container, ArchiveIndex* archive, const uint8_t* hash_digest, uint32_t hash_digest_len, uint32_t insertion_index, const dmResourceArchive::LiveUpdateResource* resource, const EntryData* entry);

	Result WriteResourceToArchive(ArchiveIndexContainer*& archive, const uint8_t* buf, uint32_t buf_len, uint32_t& bytes_written, uint32_t& offset);

	void DeepCopyArchiveIndex(ArchiveIndex*& dst, ArchiveIndexContainer* src, uint32_t extra_entries_alloc);
	
    Result CalcInsertionIndex(ArchiveIndex* archive, const uint8_t* hash_digest, const uint8_t* hashes, int& index);

}
#endif // RESOURCE_ARCHIVE_PRIVATE_H