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

#ifndef RESOURCE_ARCHIVE_PRIVATE_H
#define RESOURCE_ARCHIVE_PRIVATE_H

#include <stdio.h>
#include <stdint.h>

#include "resource_archive.h"

namespace dmResourceArchive
{
    Result ShiftAndInsert(HArchiveIndexContainer archive_container, ArchiveIndex* archive, const uint8_t* hash_digest, uint32_t hash_digest_len, int insertion_index, const dmResourceArchive::LiveUpdateResource* resource, const EntryData* entry);

    Result WriteResourceToArchive(HArchiveIndexContainer& archive, const uint8_t* buf, uint32_t buf_len, uint32_t& bytes_written, uint32_t& offset);

    void NewArchiveIndexFromCopy(ArchiveIndex*& dst, HArchiveIndexContainer src, uint32_t extra_entries_alloc);

    Result GetInsertionIndex(HArchiveIndexContainer archive, const uint8_t* hash_digest, int* index);

    Result GetInsertionIndex(ArchiveIndex* archive, const uint8_t* hash_digest, const uint8_t* hashes, int* index);

    // Unit test helpers
    /**
     * Get total entries, i.e. files/resources in archive
     * @param archive archive index handle
     * @return entry count
     */
    uint32_t GetEntryCount(HArchiveIndexContainer archive);

    uint32_t GetEntryDataOffset(HArchiveIndexContainer archive_container);

    uint32_t GetEntryDataOffset(ArchiveIndex* archive);

    void Delete(ArchiveIndex* archive);
}
#endif // RESOURCE_ARCHIVE_PRIVATE_H
