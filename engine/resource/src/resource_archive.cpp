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
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#if defined(__ANDROID__)
#include <fcntl.h>
#include <unistd.h>
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
#endif

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
#include <dlib/static_assert.h>
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

    DM_STATIC_ASSERT(sizeof(EntryData) == ENTRY_DATA_SIZE_V6, Invalid_Archive_EntryData_Size);

    static bool IsSupportedVersion(uint32_t version)
    {
        return version == VERSION_6;
    }

    uint64_t PackEntryDataOffsetAndFlags(uint64_t resource_offset, uint32_t flags)
    {
        return ((uint64_t)(flags & ENTRY_DATA_FLAGS_MASK) << ENTRY_DATA_FLAGS_SHIFT) | (resource_offset & ENTRY_DATA_OFFSET_MASK);
    }

    uint64_t GetEntryResourceDataOffset(const EntryData* entry)
    {
        uint64_t offset_and_flags = dmEndian::ToNetwork(entry->m_ResourceDataOffsetAndFlags);
        return offset_and_flags & ENTRY_DATA_OFFSET_MASK;
    }

    uint32_t GetEntryFlags(const EntryData* entry)
    {
        uint64_t offset_and_flags = dmEndian::ToNetwork(entry->m_ResourceDataOffsetAndFlags);
        return (uint32_t)(offset_and_flags >> ENTRY_DATA_FLAGS_SHIFT);
    }

    static Result ReadEntryData(FILE* file, uint32_t entry_count, EntryData* entries)
    {
        uint32_t entries_total_size = entry_count * ENTRY_DATA_SIZE_V6;
        if (fread(entries, 1, entries_total_size, file) != entries_total_size)
        {
            return RESULT_IO_ERROR;
        }
        return RESULT_OK;
    }

    static void ClearArchiveFileIndexLookup(ArchiveFileIndex* file_index)
    {
        if (!file_index)
        {
            return;
        }

        delete[] file_index->m_Hashes;
        delete[] file_index->m_Entries;
        file_index->m_Hashes = 0;
        file_index->m_Entries = 0;
    }

    static uint8_t* GetArchiveHashes(HArchiveIndexContainer archive)
    {
        if (archive->m_ArchiveFileIndex && archive->m_ArchiveFileIndex->m_Hashes)
        {
            return archive->m_ArchiveFileIndex->m_Hashes;
        }

        uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
        return (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
    }

    static EntryData* GetArchiveEntries(HArchiveIndexContainer archive)
    {
        if (archive->m_ArchiveFileIndex && archive->m_ArchiveFileIndex->m_Entries)
        {
            return archive->m_ArchiveFileIndex->m_Entries;
        }

        uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
        return (EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
    }

    static FILE* FileOpen64(const char* path)
    {
#if defined(__ANDROID__)
        int fd = open(path, O_RDONLY | O_LARGEFILE);
        if (fd < 0)
        {
            return 0;
        }
        FILE* file = fdopen(fd, "rb");
        if (!file)
        {
            close(fd);
            return 0;
        }
        setvbuf(file, 0, _IONBF, 0);
        return file;
#elif defined(__linux__)
        return fopen64(path, "rb");
#else
        return fopen(path, "rb");
#endif
    }

    static int FileSeek64(FILE* file, uint64_t offset)
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

    ArchiveIndex::ArchiveIndex()
    {
        memset(this, 0, sizeof(ArchiveIndex));
        m_EntryDataOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
        m_HashOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
    }

    // *********************************************************************************

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

    static void CleanupResources(FILE* index_file, FILE* data_file, ArchiveIndexContainer* archive)
    {
        if (index_file)
        {
            fclose(index_file);
        }

        if (archive)
        {
            bool archive_file_index_owns_data_file = data_file &&
                archive->m_ArchiveFileIndex &&
                archive->m_ArchiveFileIndex->m_FileResourceData == data_file;
            DeleteArchiveFileIndex(archive->m_ArchiveFileIndex);
            if (archive_file_index_owns_data_file)
            {
                data_file = 0;
            }

            if (archive->m_ArchiveIndex)
            {
                delete archive->m_ArchiveIndex;
            }

            delete archive;
        }

        if (data_file)
        {
            fclose(data_file);
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

        uint32_t version = dmEndian::ToNetwork(ai->m_Version);
        if(!IsSupportedVersion(version))
        {
            dmLogError("Archive version differs. Expected %d, but it was %d", VERSION_6, version);
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
        if (ReadEntryData(f_index, entry_count, aic->m_ArchiveFileIndex->m_Entries) != RESULT_OK)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        // Mark that this archive was loaded from file, and not memory-mapped
        ai->m_Userdata = FILE_LOADED_INDICATOR;

        f_data = FileOpen64(data_file_path);

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
                             const void* resource_data, uint64_t resource_data_size, bool mem_mapped_data,
                             HArchiveIndexContainer* archive)
    {
        *archive = new ArchiveIndexContainer;
        (*archive)->m_IsMemMapped = mem_mapped_index;
        ArchiveIndex* a = (ArchiveIndex*) index_buffer;
        uint32_t version = dmEndian::ToNetwork(a->m_Version);
        if (!IsSupportedVersion(version))
        {
            dmLogError("Archive version differs. Expected %d, but it was %d", VERSION_6, version);
            delete *archive;
            *archive = 0;
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
        uint8_t* hashes = GetArchiveHashes(archive);
        dmResourceArchive::EntryData* entries = GetArchiveEntries(archive);

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
        uint8_t* hashes = GetArchiveHashes(archive);
        dmResourceArchive::EntryData* entries = GetArchiveEntries(archive);

        dmLogInfo("HArchiveIndexContainer: %p  %s", archive, archive->m_ArchiveFileIndex?archive->m_ArchiveFileIndex->m_Path:"no path");

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);
            dmResourceArchive::EntryData* e = &entries[i];
            uint32_t flags = GetEntryFlags(e);
            uint64_t resource_offset = GetEntryResourceDataOffset(e);

            printf("entry: off: %4" PRIu64 "  sz: %4u  csz: %4u flags: %2u encr: %d lu: %d hash: ", resource_offset, dmEndian::ToNetwork(e->m_ResourceSize), dmEndian::ToNetwork(e->m_ResourceCompressedSize),
                                flags, flags & ENTRY_FLAG_ENCRYPTED, flags & ENTRY_FLAG_LIVEUPDATE_DATA);
            dmResource::PrintHash(h, 20);
            printf("\n");
        }
    }

    Result ReadEntry(HArchiveIndexContainer archive, const EntryData* entry, void* buffer)
    {
        // We always assume it's in Host format, since it may arrive from memory mapped data
        const uint32_t flags            = GetEntryFlags(entry);
        const uint32_t size             = dmEndian::ToNetwork(entry->m_ResourceSize);
        const uint64_t resource_offset  = GetEntryResourceDataOffset(entry);
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
            if (FileSeek64(resource_file, resource_offset) != 0)
            {
                return dmResourceArchive::RESULT_IO_ERROR;
            }

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
        const uint64_t resource_offset  = GetEntryResourceDataOffset(entry);

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
            if (FileSeek64(resource_file, resource_offset + offset) != 0)
            {
                return RESULT_IO_ERROR;
            }

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

        ClearArchiveFileIndexLookup(archive_container->m_ArchiveFileIndex);

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
