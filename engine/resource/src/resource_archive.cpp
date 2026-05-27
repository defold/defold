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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "resource.h"
#include "resource_archive.h"
#include "resource_private.h"
#include "resource_util.h"
#include "resource_archive_private.h"
#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/endian.hpp>
#include <dlib/log.h>
#include <dlib/lz4.h>
#include <dlib/memory.h>
#include <dlib/path.h>
#include <dlib/sys.h>

#define DEBUG_LOG 1
#if defined(DEBUG_LOG)
    #define LOG(...) dmLogInfo(__VA_ARGS__)
#else
    #define LOG(...)
#endif


namespace dmResourceArchive
{
    const static uint64_t FILE_LOADED_INDICATOR = 1337;

    ArchiveIndex::ArchiveIndex()
    {
        memset(this, 0, sizeof(ArchiveIndex));
        m_EntryDataOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
        m_HashOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
    }

    // *********************************************************************************

    static void CleanupResources(FILE* index_file, FILE* data_file, ArchiveIndexContainer* archive)
    {
        if (index_file)
        {
            fclose(index_file);
        }

        if (data_file)
        {
            fclose(data_file);
        }

        if (archive)
        {
            if (archive->m_ArchiveIndex)
            {
                delete archive->m_ArchiveIndex;
            }

            delete archive;
        }
    }

    Result LoadArchiveFromFile(const char* index_file_path, const char* data_file_path, HArchiveIndexContainer* archive)
    {
        FILE* f_index = fopen(index_file_path, "rb");
        if (!f_index)
        {
            return RESULT_IO_ERROR;
        }

        FILE* f_data = 0;
        ArchiveIndexContainer* aic = new ArchiveIndexContainer;
        aic->m_IsMemMapped = false;

        ArchiveIndex* ai = new ArchiveIndex;
        aic->m_ArchiveIndex = ai;

        aic->m_ArchiveFileIndex = new ArchiveFileIndex;
        dmStrlCpy(aic->m_ArchiveFileIndex->m_Path, index_file_path, DMPATH_MAX_PATH);

        if (fread(ai, 1, sizeof(ArchiveIndex), f_index) != sizeof(ArchiveIndex))
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        if(dmEndian::ToNetwork(ai->m_Version) != VERSION)
        {
            dmLogError("Archive version differs. Expected %d, but it was %d", VERSION, dmEndian::ToNetwork(ai->m_Version));
            CleanupResources(f_index, f_data, aic);
            return RESULT_VERSION_MISMATCH;
        }

        uint32_t entry_count = dmEndian::ToNetwork(ai->m_EntryDataCount);
        uint32_t entry_offset = dmEndian::ToNetwork(ai->m_EntryDataOffset);
        uint32_t hash_offset = dmEndian::ToNetwork(ai->m_HashOffset);

        fseek(f_index, hash_offset, SEEK_SET);
        aic->m_ArchiveFileIndex->m_Hashes = new uint8_t[entry_count * dmResourceArchive::MAX_HASH];

        uint32_t hash_total_size = entry_count * dmResourceArchive::MAX_HASH;
        if (fread(aic->m_ArchiveFileIndex->m_Hashes, 1, hash_total_size, f_index) != hash_total_size)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        fseek(f_index, entry_offset, SEEK_SET);
        aic->m_ArchiveFileIndex->m_Entries = new EntryData[entry_count];
        uint32_t entries_total_size = entry_count * sizeof(EntryData);
        if (fread(aic->m_ArchiveFileIndex->m_Entries, 1, entries_total_size, f_index) != entries_total_size)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        // Mark that this archive was loaded from file, and not memory-mapped
        ai->m_Userdata = FILE_LOADED_INDICATOR;

        f_data = fopen(data_file_path, "rb");

        if (!f_data)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        aic->m_ArchiveFileIndex->m_FileResourceData = f_data; // game.arcd file handle
        *archive = aic;

        fclose(f_index);

        return RESULT_OK;
    }

    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, bool mem_mapped_index,
                             const void* resource_data, uint32_t resource_data_size, bool mem_mapped_data,
                             HArchiveIndexContainer* archive)
    {
        *archive = new ArchiveIndexContainer;
        (*archive)->m_IsMemMapped = mem_mapped_index;
        ArchiveIndex* a = (ArchiveIndex*) index_buffer;
        uint32_t version = dmEndian::ToNetwork(a->m_Version);
        if (version != VERSION)
        {
            dmLogError("Archive version differs. Expected %d, but it was %d", VERSION, version);
            return RESULT_VERSION_MISMATCH;
        }

        (*archive)->m_ArchiveFileIndex = new ArchiveFileIndex;
        (*archive)->m_ArchiveFileIndex->m_ResourceData = (uint8_t*)resource_data;
        (*archive)->m_ArchiveFileIndex->m_ResourceSize = resource_data_size;
        (*archive)->m_ArchiveFileIndex->m_IsMemMapped = mem_mapped_data;

        (*archive)->m_ArchiveIndex = a;
        (*archive)->m_ArchiveIndexSize = index_buffer_size;

        return RESULT_OK;
    }

    static void DeleteArchiveFileIndex(ArchiveFileIndex* afi)
    {
        if (afi != 0)
        {
            delete[] afi->m_Entries;
            delete[] afi->m_Hashes;

            if (afi->m_FileResourceData)
            {
                fclose(afi->m_FileResourceData);
                afi->m_FileResourceData = 0;
            }
        }

        delete afi;
    }

    void Delete(HArchiveIndexContainer &archive)
    {
        DeleteArchiveFileIndex(archive->m_ArchiveFileIndex);

        if (!archive->m_IsMemMapped)
        {
            delete archive->m_ArchiveIndex;
        }

        delete archive;
        archive = 0;
    }

    void Delete(ArchiveIndex* archive)
    {
        if (archive != 0x0)
        {
            delete[] (uint8_t*)archive;
        }
    }

    dmResourceArchive::Result FindEntry(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData** entry)
    {
        uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
        uint8_t* hashes = 0;
        dmResourceArchive::EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            entries = archive->m_ArchiveFileIndex->m_Entries;
        }
        else
        {
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            entries = (dmResourceArchive::EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
        }

        // Search for hash with binary search (entries are sorted on hash)
        int first = 0;
        int last = (int)entry_count-1;
        while (first <= last)
        {
            int mid = first + (last - first) / 2;
            uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * mid);

            int cmp = memcmp(hash, h, hash_len);
            if (cmp == 0)
            {
                if (entry != 0)
                {
                    *entry = &entries[mid];
                }
                return dmResourceArchive::RESULT_OK;
            }
            else if (cmp > 0)
            {
                first = mid+1;
            }
            else if (cmp < 0)
            {
                last = mid-1;
            }
        }

        return dmResourceArchive::RESULT_NOT_FOUND;
    }

    void DebugArchiveIndex(HArchiveIndexContainer archive)
    {
        uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
        uint8_t* hashes = 0;
        dmResourceArchive::EntryData* entries = 0;

        dmLogInfo("HArchiveIndexContainer: %p  %s", archive, archive->m_ArchiveFileIndex?archive->m_ArchiveFileIndex->m_Path:"no path");

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            entries = archive->m_ArchiveFileIndex->m_Entries;
        }
        else
        {
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            entries = (dmResourceArchive::EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
        }

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);
            dmResourceArchive::EntryData* e = &entries[i];
            uint32_t flags = dmEndian::ToNetwork(e->m_Flags);

            printf("entry: off: %4u  sz: %4u  csz: %4u flags: %2u encr: %d lu: %d hash: ", dmEndian::ToNetwork(e->m_ResourceDataOffset), dmEndian::ToNetwork(e->m_ResourceSize), dmEndian::ToNetwork(e->m_ResourceCompressedSize),
                                flags, flags & ENTRY_FLAG_ENCRYPTED, flags & ENTRY_FLAG_LIVEUPDATE_DATA);
            dmResource::PrintHash(h, 20);
            printf("\n");
        }
    }

    Result ReadEntry(HArchiveIndexContainer archive, const EntryData* entry, void* buffer)
    {
        // We always assume it's in Host format, since it may arrive from memory mapped data
        const uint32_t flags            = dmEndian::ToNetwork(entry->m_Flags);
        const uint32_t size             = dmEndian::ToNetwork(entry->m_ResourceSize);
        const uint32_t resource_offset  = dmEndian::ToNetwork(entry->m_ResourceDataOffset);
              uint32_t compressed_size  = dmEndian::ToNetwork(entry->m_ResourceCompressedSize);

        bool encrypted = (flags & dmResourceArchive::ENTRY_FLAG_ENCRYPTED);
        bool compressed = (flags & dmResourceArchive::ENTRY_FLAG_COMPRESSED);

        const ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;
        bool resource_memmapped = afi->m_IsMemMapped;

        uint8_t* temp_data = 0;
        uint8_t* source_data = 0;
        uint32_t source_data_size = 0;

        if (!resource_memmapped)
        {
            // we need to read from the file on disc
            FILE* resource_file = afi->m_FileResourceData;
            fseek(resource_file, resource_offset, SEEK_SET);

            Result result = dmResourceArchive::RESULT_OK;
            // Note, we don't need to check if it's encrypted here, as it's guaranteed to
            // have the same size after decryption
            // So, we only need a temp buffer if the file is compressed
            if (!compressed)
            {
                // we can read directly to the output buffer
                if (fread(buffer, 1, size, resource_file) != size)
                {
                    result = dmResourceArchive::RESULT_IO_ERROR;
                }
                source_data = (uint8_t*)buffer;
                source_data_size = (uint32_t)size;
            }
            else
            {
                // We need a temp buffer to read to, since we can't decompress to the same buffer
                temp_data = new uint8_t[compressed_size];
                if (fread(temp_data, 1, compressed_size, resource_file) != compressed_size)
                {
                    result = RESULT_IO_ERROR;
                }
                source_data = temp_data;
                source_data_size = compressed_size;
            }

            if (result != dmResourceArchive::RESULT_OK)
            {
                delete[] temp_data;
                return result;
            }
        }
        else
        {
            const uint8_t* archive_data = (uint8_t*) (((uintptr_t)afi->m_ResourceData + resource_offset));

            if (!compressed)
            {
                // we can copy it directly to the output buffer
                memcpy(buffer, archive_data, size);

                source_data = (uint8_t*)buffer;
                source_data_size = (uint32_t)size;
            }
            else if (!encrypted) // && compressed
            {
                // We don't need to decrypt (destructive process of the source data)
                // so we can use the archive data directly
                source_data = (uint8_t*)archive_data;
                source_data_size = compressed_size;
            }
            else
            {
                // We need a temp buffer to read to, since we can't decompress to the same buffer
                temp_data = new uint8_t[compressed_size];
                memcpy(temp_data, archive_data, compressed_size);

                source_data = temp_data;
                source_data_size = compressed_size;
            }
        }

        // At this point the source_data is the file "stored on disc"
        // and will be treated as the input

        // Encryption is done in-place
        if(encrypted)
        {
            dmResource::Result r = dmResource::DecryptBuffer((uint8_t*)source_data, source_data_size);
            if (dmResource::RESULT_OK != r)
            {
                delete[] temp_data;
                return dmResourceArchive::RESULT_UNKNOWN;
            }
        }

        if (compressed)
        {
            int decompressed_size;
            dmLZ4::Result r = dmLZ4::DecompressBuffer(source_data, source_data_size, buffer, size, &decompressed_size);
            if (dmLZ4::RESULT_OK != r)
            {
                dmLogError("LZ4 decompression failed: result=%d, expected size=%u, actual size=%d", r, size, decompressed_size);
                if (r == dmLZ4::RESULT_OUTPUT_SIZE_TOO_LARGE) {
                    dmLogError("Resource too large for LZ4 decompression: %u bytes exceeds maximum limit", size);
                }
                delete[] temp_data;
                return dmResourceArchive::RESULT_OUTBUFFER_TOO_SMALL;
            }
        }

        delete[] temp_data;
        return dmResourceArchive::RESULT_OK;
    }

    Result ReadEntryPartial(HArchiveIndexContainer archive, const EntryData* entry, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
    {
        // We always assume it's in Host format, since it may arrive from memory mapped data
        const uint32_t resource_size    = dmEndian::ToNetwork(entry->m_ResourceSize);
        const uint32_t resource_offset  = dmEndian::ToNetwork(entry->m_ResourceDataOffset);

        if (offset >= resource_size)
        {
            *nread = 0;
            return RESULT_OK;
        }

        if ((offset+size) > resource_size)
        {
            size = resource_size - offset;
        }

        Result result = dmResourceArchive::RESULT_OK;

        const ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;

        if (!afi->m_IsMemMapped)
        {
            // we need to read from the file on disc
            FILE* resource_file = afi->m_FileResourceData;
            fseek(resource_file, resource_offset+offset, SEEK_SET);

            // we can read directly to the output buffer
            size_t nmemb = fread(buffer, 1, size, resource_file);
            if (!ferror(resource_file))
            {
                *nread = (uint32_t)nmemb;
            }
            else {
                result = RESULT_IO_ERROR;
            }

            return result;
        }
        else
        {
            const uint8_t* archive_data = (uint8_t*) (((uintptr_t)afi->m_ResourceData + resource_offset));
            memcpy(buffer, archive_data + offset, size);
            *nread = size;
        }

        return dmResourceArchive::RESULT_OK;
    }

    void SetNewArchiveIndex(HArchiveIndexContainer archive_container, HArchiveIndex new_index, bool mem_mapped)
    {
        if (!archive_container->m_IsMemMapped)
        {
            delete archive_container->m_ArchiveIndex;
        }
        // Use this runtime archive index until the next reboot
        archive_container->m_ArchiveIndex = new_index;
        // Since we store data sequentially when doing the deep-copy we want to access it in that fashion
        archive_container->m_IsMemMapped = mem_mapped;
    }

    uint32_t GetEntryCount(HArchiveIndexContainer archive)
    {
        return dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
    }

    uint32_t GetEntryDataOffset(HArchiveIndex index)
    {
        return dmEndian::ToNetwork(index->m_EntryDataOffset);
    }

    uint32_t GetEntryDataOffset(HArchiveIndexContainer archive_container)
    {
        return GetEntryDataOffset(archive_container->m_ArchiveIndex);
    }

}  // namespace dmResourceArchive
