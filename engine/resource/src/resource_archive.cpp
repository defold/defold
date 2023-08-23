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
#include <dlib/endian.h>
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

    static const uint8_t* LowerBound(const uint8_t* first, size_t size, const uint8_t* hash_digest, uint32_t hash_length)
    {
        while (size > 0) {
            size_t half = size >> 1;
            const uint8_t* middle = first + half * dmResourceArchive::MAX_HASH;

            int cmp = memcmp(hash_digest, middle, hash_length);
            if (cmp >= 0)
            {
                first = middle + dmResourceArchive::MAX_HASH;
                size = size - half - 1;
            }
            else
            {
                size = half;
            }
        }
        return first;
    }

    Result GetInsertionIndex(ArchiveIndex* archive, const uint8_t* hash_digest, const uint8_t* hashes, int* out_index)
    {
        int count = dmEndian::ToNetwork(archive->m_EntryDataCount);
        size_t hash_length = dmEndian::ToNetwork(archive->m_HashLength);
        const uint8_t* end = hashes + count * dmResourceArchive::MAX_HASH;
        const uint8_t* insert = LowerBound(hashes, (size_t)count, hash_digest, hash_length);
        if (insert >= end)
        {
            *out_index = count;
            return RESULT_OK;
        }
        if (memcmp(insert, hash_digest, hash_length) == 0)
        {
            return RESULT_ALREADY_STORED;
        }

        *out_index = (insert - hashes) / dmResourceArchive::MAX_HASH;
        return RESULT_OK;
    }

    Result GetInsertionIndex(HArchiveIndexContainer archive, const uint8_t* hash_digest, int* out_index)
    {
        uint8_t* hashes = 0;
        uint32_t hashes_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);

        hashes = (archive->m_IsMemMapped) ? (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hashes_offset) : archive->m_ArchiveFileIndex->m_Hashes;

        return GetInsertionIndex(archive->m_ArchiveIndex, hash_digest, hashes, out_index);
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

    void NewArchiveIndexFromCopy(ArchiveIndex*& dst, HArchiveIndexContainer src, uint32_t extra_entries_alloc)
    {
        ArchiveIndex* ai = src->m_ArchiveIndex;
        uint32_t hash_digests_size = dmEndian::ToNetwork(ai->m_EntryDataCount) * dmResourceArchive::MAX_HASH;
        uint32_t entry_datas_size = (dmEndian::ToNetwork(ai->m_EntryDataCount) * sizeof(EntryData));
        uint32_t single_entry_size = dmResourceArchive::MAX_HASH + sizeof(EntryData);
        uint32_t size_to_alloc = sizeof(ArchiveIndex) + hash_digests_size + entry_datas_size;

        if (extra_entries_alloc > 0)
        {
            size_to_alloc += single_entry_size * extra_entries_alloc;
        }

        dst = (ArchiveIndex*)new uint8_t[size_to_alloc];

        if (!src->m_IsMemMapped)
        {
            memcpy((void*)dst, (void*)ai, sizeof(ArchiveIndex)); // copy header data
            uint8_t* cursor =  (uint8_t*)((uintptr_t)dst + sizeof(ArchiveIndex)); // step cursor to hash digests array
            memcpy(cursor, src->m_ArchiveFileIndex->m_Hashes, hash_digests_size);
            cursor = (uint8_t*)((uintptr_t)cursor + hash_digests_size); // step cursor to entry data array
            if (extra_entries_alloc > 0)
            {
                cursor = (uint8_t*)((uintptr_t)cursor + dmResourceArchive::MAX_HASH * extra_entries_alloc);
            }
            memcpy(cursor, src->m_ArchiveFileIndex->m_Entries, entry_datas_size);
        }
        else
        {
            memcpy(dst, ai, sizeof(ArchiveIndex)); // copy header data
            uint8_t* cursor =  (uint8_t*)((uintptr_t)dst + sizeof(ArchiveIndex)); // step cursor to hash digests array
            memcpy(cursor, (void*)((uintptr_t)ai + dmEndian::ToNetwork(ai->m_HashOffset)), hash_digests_size);
            cursor = (uint8_t*)((uintptr_t)cursor + hash_digests_size); // step cursor to entry data array
            if (extra_entries_alloc > 0)
            {
                cursor = (uint8_t*)((uintptr_t)cursor + dmResourceArchive::MAX_HASH * extra_entries_alloc);
            }
            memcpy(cursor, (void*)(((uintptr_t)ai + dmEndian::ToNetwork(ai->m_EntryDataOffset))), entry_datas_size);
        }

        if (extra_entries_alloc > 0)
        {
            dst->m_EntryDataOffset = dmEndian::ToHost(dmEndian::ToNetwork(dst->m_EntryDataOffset) + dmResourceArchive::MAX_HASH * extra_entries_alloc);
        }
    }

    Result WriteResourceToArchive(HArchiveIndexContainer& archive, const uint8_t* buf, uint32_t buf_len, uint32_t& bytes_written, uint32_t& offset)
    {
        ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;
        FILE* res_file = afi->m_FileResourceData;
        assert(afi->m_FileResourceData != 0);

        fseek(res_file, 0, SEEK_END);
        uint32_t offs = (uint32_t)ftell(res_file);
        size_t bytes = fwrite(buf, 1, buf_len, res_file);
        if(bytes != buf_len)
        {
            return RESULT_IO_ERROR;
        }
        bytes_written = bytes;
        offset = offs;

        fflush(res_file); // make sure all writes flushed before mem-mapping below

        // We have written to the resource file, need to update mapping
        if (afi->m_IsMemMapped)
        {
            void* temp_map = (void*)afi->m_ResourceData;
            assert(afi->m_ResourceSize == offset); // I want to use the m_ResourceSize
            dmResource::UnmapFile(temp_map, offset);

            temp_map = 0x0;
            uint32_t map_size = 0;
            dmResource::Result res = dmResource::MapFile(afi->m_Path, temp_map, map_size);
            if (res != dmResource::RESULT_OK)
            {
                dmLogError("Failed to map liveupdate resource file, result = %i", res);
                return RESULT_IO_ERROR;
            }
            afi->m_ResourceData = (uint8_t*)temp_map;
            afi->m_ResourceSize = offset + bytes_written;
            assert((offset + bytes_written) == map_size); // I want to use the map_size
        }

        return RESULT_OK;
    }

    // only used for live update archives
    Result ShiftAndInsert(ArchiveIndexContainer* archive_container, ArchiveIndex* ai, const uint8_t* hash_digest, uint32_t hash_digest_len, int insertion_index,
                            const dmResourceArchive::LiveUpdateResource* resource, const EntryData* entry_data)
    {
        assert(insertion_index >= 0);
        ArchiveIndex* archive = (ai == 0x0) ? archive_container->m_ArchiveIndex : ai;
        uint8_t* hashes = (uint8_t*)((uintptr_t)archive + dmEndian::ToNetwork(archive->m_HashOffset));
        EntryData* entries = (EntryData*)((uintptr_t)archive + dmEndian::ToNetwork(archive->m_EntryDataOffset));

        uint32_t entry_count = dmEndian::ToNetwork(archive->m_EntryDataCount);
        // Shift hashes after insertion_index down
        uint8_t* hash_shift_src = (uint8_t*)((uintptr_t)hashes + dmResourceArchive::MAX_HASH * insertion_index);
        uint8_t* hash_shift_dst = (uint8_t*)((uintptr_t)hash_shift_src + dmResourceArchive::MAX_HASH);
        if ((uint32_t)insertion_index < entry_count) // no need to memmove if inserting at tail
        {
            uint32_t size_to_shift = (entry_count - insertion_index) * dmResourceArchive::MAX_HASH;
            memmove((void*)hash_shift_dst, (void*)hash_shift_src, size_to_shift);
        }
        memcpy(hash_shift_src, hash_digest, hash_digest_len);

        // Shift entry datas
        uint8_t* entries_shift_src = (uint8_t*)((uintptr_t)entries + sizeof(EntryData) * insertion_index);
        uint8_t* entries_shift_dst = (uint8_t*)((uintptr_t)entries_shift_src + sizeof(EntryData));
        if ((uint32_t)insertion_index < entry_count)
        {
            uint32_t size_to_shift = (entry_count - insertion_index) * sizeof(EntryData);
            memmove(entries_shift_dst, entries_shift_src, size_to_shift);
        }

        bool write_resources = resource != 0x0;

        EntryData entry;

        if (!write_resources)
        {
            memcpy(&entry, entry_data, sizeof(EntryData));
        }
        else
        {
            /// --- WRITE RESOURCE START
            // Write buf to resource file before creating EntryData instance
            uint32_t bytes_written = 0;
            uint32_t offs = 0;
            Result write_res = WriteResourceToArchive(archive_container, (uint8_t*)resource->m_Data, resource->m_Count, bytes_written, offs);
            if (write_res != RESULT_OK)
            {
                dmLogError("All bytes not written for resource, bytes written: %u, resource size: %u", bytes_written, resource->m_Count);
                delete archive;
                return RESULT_IO_ERROR;
            }

            // Create entrydata instance and insert into index
            bool is_compressed = (resource->m_Header->m_Flags & ENTRY_FLAG_COMPRESSED);
            //EntryData entry;
            entry.m_ResourceDataOffset = dmEndian::ToHost(offs);
            entry.m_ResourceSize = is_compressed ? resource->m_Header->m_Size : dmEndian::ToHost((uint32_t)resource->m_Count);
            entry.m_ResourceCompressedSize = is_compressed ? dmEndian::ToHost((uint32_t)resource->m_Count) : (dmEndian::ToHost(0xffffffff));
            entry.m_Flags = dmEndian::ToHost((uint32_t)(resource->m_Header->m_Flags | ENTRY_FLAG_LIVEUPDATE_DATA));
            /// --- WRITE RESOURCE END
        }

        memcpy((void*)entries_shift_src, (void*)&entry, sizeof(EntryData));
        archive->m_EntryDataCount = dmEndian::ToHost(dmEndian::ToNetwork(archive->m_EntryDataCount) + 1);
        return RESULT_OK;
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
            if (!compressed)
            {
                // we can read directly to the output buffer
                if (fread(buffer, 1, size, resource_file) != size)
                {
                    result = dmResourceArchive::RESULT_IO_ERROR;
                }
                source_data = (uint8_t*)buffer;
                source_data_size = size;
            }
            else
            {
                // We need a temp buffer to read to, since we can't decompress to the same folder
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
            source_data = (uint8_t*) (((uintptr_t)afi->m_ResourceData + resource_offset));
            source_data_size = compressed ? compressed_size : size;

            if (!compressed)
            {
                // we can copy it directly to the output buffer
                memcpy(buffer, source_data, source_data_size);
            }
        }

        // At this point the source_data is the file "stored on disc"
        // and will be treated as the input

        if (compressed)
        {
            int decompressed_size;
            dmLZ4::Result r = dmLZ4::DecompressBuffer(source_data, source_data_size, buffer, size, &decompressed_size);
            if (dmLZ4::RESULT_OK != r)
            {
                delete[] temp_data;
                return dmResourceArchive::RESULT_OUTBUFFER_TOO_SMALL;
            }
        }

        // Encryption can be done in-place
        if(encrypted)
        {
            dmResource::Result r = dmResource::DecryptBuffer((uint8_t*)buffer, size);
            if (dmResource::RESULT_OK != r)
            {
                delete[] temp_data;
                return dmResourceArchive::RESULT_UNKNOWN;
            }
        }


        delete[] temp_data;
        return dmResourceArchive::RESULT_OK;
    }

    Result WriteArchiveIndex(const char* path, ArchiveIndex* ai)
    {
        // Write to temporary index file, filename liveupdate.arci.tmp
        FILE* f = fopen(path, "wb");
        if (!f)
        {
            dmLogError("Failed to create liveupdate index file: %s", path);
            return RESULT_IO_ERROR;
        }
        uint32_t entry_count = dmEndian::ToNetwork(ai->m_EntryDataCount);
        uint32_t total_size = sizeof(ArchiveIndex) + entry_count * dmResourceArchive::MAX_HASH + entry_count * sizeof(EntryData);
        if (fwrite((void*)ai, 1, total_size, f) != total_size)
        {
            fclose(f);
            dmLogError("Failed to write %u bytes to liveupdate index file: %s", (uint32_t)total_size, path);
            return RESULT_IO_ERROR;
        }
        fflush(f);
        fclose(f);

        return RESULT_OK;
    }

    Result NewArchiveIndexWithResource(HArchiveIndexContainer archive_container, const char* path,
                                        const uint8_t* hash_digest, uint32_t hash_digest_len,
                                        const dmResourceArchive::LiveUpdateResource* resource, HArchiveIndex& out_new_index)
    {
        int idx = -1;
        Result index_result = GetInsertionIndex(archive_container, hash_digest, &idx);
        if (index_result != RESULT_OK)
        {
            dmLogError("Could not calculate valid resource insertion index, resource probably already stored in index. Result: %d", index_result);
            return index_result;
        }

        // Make deep-copy. Operate on this and only overwrite when done inserting
        ArchiveIndex* ai_temp = 0x0;
        NewArchiveIndexFromCopy(ai_temp, archive_container, 1);

        // Shift buffers and insert index- and resource data
        Result result = ShiftAndInsert(archive_container, ai_temp, hash_digest, hash_digest_len, idx, resource, 0x0);

        if (result != RESULT_OK)
        {
            delete ai_temp;
            dmLogError("Failed to insert resource, result = %i", result);
            return result;
        }

        result = WriteArchiveIndex(path, ai_temp);
        if (RESULT_OK != result)
        {
            Delete(ai_temp);
            return result;
        }

        // set result
        out_new_index = ai_temp;
        return RESULT_OK;
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
