#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#include <sys/stat.h>

#include "resource_archive.h"
#include "resource.h"
#include <dlib/dstrings.h>
#include <dlib/lz4.h>
#include <dlib/log.h>
#include <dlib/crypt.h>
#include <dlib/path.h>
#include <dlib/sys.h>

// Maximum hash length convention. This size should large enough.
// If this length changes the VERSION needs to be bumped.
#define DMRESOURCE_MAX_HASH (64) // Equivalent to 512 bits

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

#define C_TO_JAVA ntohl
#define JAVA_TO_C htonl

namespace dmResourceArchive
{
    const static uint64_t FILE_LOADED_INDICATOR = 1337;
    const char* KEY = "aQj8CScgNP4VsfXK";

    enum EntryFlag
    {
        ENTRY_FLAG_ENCRYPTED        = 0b00000001,
        ENTRY_FLAG_COMPRESSED       = 0b00000010,
        ENTRY_FLAG_LIVEUPDATE_DATA  = 0b00000100,
    };

    struct ArchiveIndex
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
    };

    struct ArchiveIndexContainer
    {
        ArchiveIndexContainer()
        {
            memset(this, 0, sizeof(ArchiveIndexContainer));
        }

        ArchiveIndex* m_ArchiveIndex; // this could be mem-mapped or loaded into memory from file
        bool m_IsMemMapped;
        bool m_LiveUpdateResourcesMemMapped;

        /// Used if the archive is loaded from file
        uint8_t* m_Hashes;
        EntryData* m_Entries;
        uint8_t* m_ResourceData;
        FILE* m_FileResourceData;

        /// Resources acquired with LiveUpdate
        char m_LiveUpdateResourcePath[DMPATH_MAX_PATH];
        uint8_t* m_LiveUpdateResourceData;
        uint32_t m_LiveUpdateResourceSize;
        FILE* m_LiveUpdateFileResourceData;
    };

    // DEBUG
    void PrintHash(const uint8_t* hash, uint32_t len)
    {
        char* slask = new char[len*2+1];
        slask[len] = '\0';
        for (int i = 0; i < len; ++i)
        {
            sprintf(slask+i*2, "%02X", hash[i]);
        }
        dmLogInfo("HASH PRINTED: %s", slask);
        delete[] slask;
    }
    // END DEBUG

    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, const void* resource_data, const char* lu_resource_filename, const void* lu_resource_data, FILE* f_lu_resource_data, HArchiveIndexContainer* archive)
    {
        *archive = new ArchiveIndexContainer;
        (*archive)->m_IsMemMapped = true;
        ArchiveIndex* a = (ArchiveIndex*) index_buffer;
        uint32_t version = JAVA_TO_C(a->m_Version);
        if (version != VERSION)
        {
            return RESULT_VERSION_MISMATCH;
        }
        (*archive)->m_ResourceData = (uint8_t*)resource_data;
        (*archive)->m_LiveUpdateResourceData = (uint8_t*)lu_resource_data;
        (*archive)->m_LiveUpdateFileResourceData = f_lu_resource_data;

        if (lu_resource_data)
        {
            (*archive)->m_LiveUpdateResourcesMemMapped = true;
        }
        if (lu_resource_filename != 0x0)
        {
            dmStrlCpy((*archive)->m_LiveUpdateResourcePath, lu_resource_filename, DMPATH_MAX_PATH);
        }

        (*archive)->m_ArchiveIndex = a;

        if (lu_resource_filename != 0x0)
        {
            dmLogInfo("LU resource path: %s", (*archive)->m_LiveUpdateResourcePath);
        }

        // DEBUG
        // dmLogInfo("-----------WRAPPING! entry_count: %u, hash_len: %u", JAVA_TO_C(a->m_EntryDataCount), JAVA_TO_C(a->m_HashLength));
        // uint8_t* hashes = (uint8_t*)(uintptr_t(a) + JAVA_TO_C(a->m_HashOffset));
        // for (int i = 0; i < JAVA_TO_C(a->m_EntryDataCount); ++i)
        // {
        //     uint8_t* hash = (uint8_t*)(uintptr_t(hashes) + DMRESOURCE_MAX_HASH*i);
        //     PrintHash(hash, JAVA_TO_C(a->m_HashLength));
        // }
        // END DEBUG

        return RESULT_OK;
    }

    void CleanupResources(FILE* index_file, FILE* data_file, FILE* lu_data_file, ArchiveIndexContainer* archive)
    {
        if (index_file)
        {
            fclose(index_file);
        }

        if (data_file)
        {
            fclose(data_file);
        }

        if (lu_data_file)
        {
            fclose(lu_data_file);
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

    Result LoadArchive(const char* index_file_path, const char* data_file_path, const char* lu_data_file_path, HArchiveIndexContainer* archive)
    {
        uint32_t filename_count = 0;
        while (true)
        {
            if (index_file_path[filename_count] == '\0')
                break;
            if (filename_count >= DMPATH_MAX_PATH)
            {
                return RESULT_IO_ERROR;
            }

            ++filename_count;
        }

        uint32_t entry_count = 0, entry_offset = 0, hash_len = 0, hash_offset = 0, hash_total_size = 0, entries_total_size = 0;
        FILE* f_index = fopen(index_file_path, "rb");
        FILE* f_data = 0;
        FILE* f_lu_data = 0;

        ArchiveIndexContainer* aic = 0;
        ArchiveIndex* ai = 0;
        *archive = 0;

        Result r = RESULT_OK;
        
        if (!f_index)
        {
            CleanupResources(f_index, f_data, f_lu_data, aic);
            return RESULT_IO_ERROR;
        }

        aic = new ArchiveIndexContainer;
        aic->m_IsMemMapped = false;

        ai = new ArchiveIndex;
        if (fread(ai, 1, sizeof(ArchiveIndex), f_index) != sizeof(ArchiveIndex))
        {
            CleanupResources(f_index, f_data, f_lu_data, aic);
            return RESULT_IO_ERROR;
        }

        if(JAVA_TO_C(ai->m_Version) != VERSION)
        {
            CleanupResources(f_index, f_data, f_lu_data, aic);
            return RESULT_VERSION_MISMATCH;
        }

        entry_count = JAVA_TO_C(ai->m_EntryDataCount);
        entry_offset = JAVA_TO_C(ai->m_EntryDataOffset);
        hash_len = JAVA_TO_C(ai->m_HashLength);
        hash_offset = JAVA_TO_C(ai->m_HashOffset);

        fseek(f_index, hash_offset, SEEK_SET);
        aic->m_Hashes = new uint8_t[entry_count * DMRESOURCE_MAX_HASH];
        hash_total_size = entry_count * DMRESOURCE_MAX_HASH;
        if (fread(aic->m_Hashes, 1, hash_total_size, f_index) != hash_total_size)
        {
            CleanupResources(f_index, f_data, f_lu_data, aic);
            return RESULT_IO_ERROR;
        }

        fseek(f_index, entry_offset, SEEK_SET);
        aic->m_Entries = new EntryData[entry_count];
        entries_total_size = entry_count * sizeof(EntryData);
        if (fread(aic->m_Entries, 1, entries_total_size, f_index) != entries_total_size)
        {
            CleanupResources(f_index, f_data, f_lu_data, aic);
            return RESULT_IO_ERROR;
        }

        // Mark that this archive was loaded from file, and not memory-mapped
        ai->m_Userdata = FILE_LOADED_INDICATOR;

        // Open file for resource acquired through liveupdate
        // Assumes file already exists if a path to it is supplied
        if (lu_data_file_path != 0x0)
        {
            f_lu_data = fopen(lu_data_file_path, "rb+");

            if (!f_lu_data)
            {
                CleanupResources(f_index, f_data, f_lu_data, aic);
                return RESULT_IO_ERROR;
            }
            dmStrlCpy(aic->m_LiveUpdateResourcePath, lu_data_file_path, DMPATH_MAX_PATH);
            aic->m_LiveUpdateResourcesMemMapped = false;
        }

        f_data = fopen(data_file_path, "rb");

        if (!f_data)
        {
            CleanupResources(f_index, f_data, f_lu_data, aic);
            return RESULT_IO_ERROR;
        }

        aic->m_FileResourceData = f_data;
        aic->m_LiveUpdateFileResourceData = f_lu_data;
        aic->m_LiveUpdateResourceData = 0x0;
        aic->m_LiveUpdateResourcesMemMapped = false;
        aic->m_ArchiveIndex = ai;
        *archive = aic;

        return r;
    }

    void Delete(HArchiveIndexContainer archive)
    {
        if (archive->m_Entries)
        {
            delete[] archive->m_Entries;
        }

        if (archive->m_Hashes)
        {
            delete[] archive->m_Hashes;
        }

        if (archive->m_FileResourceData)
        {
            fclose(archive->m_FileResourceData);
        }

        if (archive->m_LiveUpdateFileResourceData)
        {
            fclose(archive->m_LiveUpdateFileResourceData);
        }

        if (archive->m_LiveUpdateResourceData)
        {
            void* tmp_ptr = (void*)archive->m_LiveUpdateResourceData;
            dmResource::UnmapFile(tmp_ptr, archive->m_LiveUpdateResourceSize);
        }

        if (!archive->m_IsMemMapped)
        {
            delete archive->m_ArchiveIndex;
        }

        delete archive;
    }

    Result CalcInsertionIndex(HArchiveIndexContainer archive, const uint8_t* hash_digest, int& index)
    {
        uint8_t* hashes = 0;
        uint32_t hashes_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_HashOffset);
        
        hashes = (archive->m_IsMemMapped) ? (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hashes_offset) : archive->m_Hashes;

        int first = 0;
        int last = JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataCount);
        int mid = first + (last - first) / 2;
        while (first <= last && first !=mid)
        {
            mid = first + (last - first) / 2;
            //dmLogInfo("first: %i, mid: %i, last: %i", first, mid, last);
            uint8_t* h = (hashes + DMRESOURCE_MAX_HASH * mid);

            int cmp = memcmp(hash_digest, h, JAVA_TO_C(archive->m_ArchiveIndex->m_HashLength));
            if (cmp == 0)
            {
                // attemping to insert an already inserted resource
                dmLogWarning("Resource already stored");
                return RESULT_ALREADY_STORED;
            }
            else if (cmp > 0)
            {
                first = mid+1;
            }
            else if (cmp < 0)
            {
                last = mid; // last index should be included
            }
        }

        index = mid;
        return RESULT_OK;
    }

    void DeepCopyArchiveIndex(ArchiveIndex*& dst, ArchiveIndexContainer* src, bool alloc_extra_entry)
    {
        ArchiveIndex* ai = src->m_ArchiveIndex;
        uint32_t hash_digests_size = JAVA_TO_C(ai->m_EntryDataCount) * DMRESOURCE_MAX_HASH;
        uint32_t entry_datas_size = (JAVA_TO_C(ai->m_EntryDataCount) * sizeof(EntryData));
        uint32_t single_entry_size = DMRESOURCE_MAX_HASH + sizeof(EntryData);
        uint32_t size_to_alloc = sizeof(ArchiveIndex) + hash_digests_size + entry_datas_size;

        if (alloc_extra_entry)
        {
            size_to_alloc += single_entry_size;
        }

        dst = (ArchiveIndex*)new uint8_t[size_to_alloc];

        if (!src->m_IsMemMapped)
        {
            memcpy((void*)dst, (void*)ai, sizeof(ArchiveIndex)); // copy header data
            uint8_t* cursor =  (uint8_t*)((uintptr_t)dst + sizeof(ArchiveIndex)); // step cursor to hash digests array
            memcpy(cursor, src->m_Hashes, hash_digests_size);
            cursor = (uint8_t*)((uintptr_t)cursor + hash_digests_size); // step cursor to entry data array
            if (alloc_extra_entry)
            {
                cursor = (uint8_t*)((uintptr_t)cursor + DMRESOURCE_MAX_HASH);
            }
            memcpy(cursor, src->m_Entries, entry_datas_size);
        }
        else
        {
            memcpy(dst, ai, sizeof(ArchiveIndex)); // copy header data
            uint8_t* cursor =  (uint8_t*)((uintptr_t)dst + sizeof(ArchiveIndex)); // step cursor to hash digests array
            memcpy(cursor, (void*)((uintptr_t)ai + JAVA_TO_C(ai->m_HashOffset)), hash_digests_size);
            cursor = (uint8_t*)((uintptr_t)cursor + hash_digests_size); // step cursor to entry data array
            if (alloc_extra_entry)
            {
                cursor = (uint8_t*)((uintptr_t)cursor + DMRESOURCE_MAX_HASH);
            }
            memcpy(cursor, (void*)(((uintptr_t)ai + JAVA_TO_C(ai->m_EntryDataOffset))), entry_datas_size);
        }

        if (alloc_extra_entry)
        {
            dst->m_EntryDataOffset = C_TO_JAVA(JAVA_TO_C(dst->m_EntryDataOffset) + DMRESOURCE_MAX_HASH);
        }
    }

    Result WriteResourceToArchive(HArchiveIndexContainer &archive, const uint8_t* buf, uint32_t buf_len, uint32_t& bytes_written, uint32_t& offset)
    {
        dmLogInfo("Writing resource data...");
        fseek(archive->m_LiveUpdateFileResourceData, 0, SEEK_END);
        uint32_t offs = (uint32_t)ftell(archive->m_LiveUpdateFileResourceData);
        size_t bytes = fwrite(buf, 1, buf_len, archive->m_LiveUpdateFileResourceData);
        if(bytes != buf_len)
        {
            return RESULT_IO_ERROR;
        }
        bytes_written = bytes;
        offset = offs;

        fflush(archive->m_LiveUpdateFileResourceData); // make sure all writes flushed before mem-mapping below

        // We have written to the resource file, need to update mapping
        if (archive->m_LiveUpdateResourcesMemMapped)
        {
            dmLogInfo("Attempt to map to file at: %s", archive->m_LiveUpdateResourcePath);
            void* temp_map = (void*)archive->m_LiveUpdateResourceData;
            dmResource::UnmapFile(temp_map, offset);
            temp_map = 0x0;
            uint32_t map_size = 0;
            dmResource::Result res = dmResource::MapFile(archive->m_LiveUpdateResourcePath, temp_map, map_size);
            if (res != dmResource::RESULT_OK)
            {
                dmLogError("Failed to map liveupdate respource file, result = %i", res);
                return RESULT_IO_ERROR;
            }
            archive->m_LiveUpdateResourceData = (uint8_t*)temp_map;
            archive->m_LiveUpdateResourceSize += bytes_written;

            dmLogInfo("New map with size: %u", archive->m_LiveUpdateResourceSize);
        }
        
        return RESULT_OK;
    }

    Result InsertResource(HArchiveIndexContainer archive, const uint8_t* hash_digest, uint32_t hash_digest_len, const dmResourceArchive::LiveUpdateResource* resource, const char* proj_id)
    {
        Result result = RESULT_OK;

        char app_support_path[DMPATH_MAX_PATH];
        char lu_index_path[DMPATH_MAX_PATH];

        dmSys::GetApplicationSupportPath(proj_id, app_support_path, DMPATH_MAX_PATH);
        dmPath::Concat(app_support_path, "liveupdate.arci", lu_index_path, DMPATH_MAX_PATH);
        dmLogInfo("InsertResource, buf_len: %u", resource->m_Count);
        struct stat file_stat;
        bool resource_exists = stat(lu_index_path, &file_stat) == 0;

        uint8_t* hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + JAVA_TO_C(archive->m_ArchiveIndex->m_HashOffset));
        EntryData* entries = (EntryData*)((uintptr_t)archive->m_ArchiveIndex + JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataOffset));

        dmLogInfo("num entries before insertion: %u", JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataCount));

        int idx = 0;
        Result index_result = CalcInsertionIndex(archive, hash_digest, idx);

        if (index_result != RESULT_OK)
        {
            dmLogError("Could not calculate valid resource insertion index");
            return index_result;
        }

        dmLogInfo("Calculated insertion index: %i", idx);

		
        if (!resource_exists)
        {
            fopen(lu_index_path, "wb");

            //char lu_data_path[DMPATH_MAX_PATH];
            // Data file has same path and filename as index file, but extension .arcd instead of .arci.
            char lu_data_path[DMPATH_MAX_PATH];
            dmStrlCpy(lu_data_path, lu_index_path, DMPATH_MAX_PATH);
            lu_data_path[strlen(lu_index_path)-1] = 'd';

            FILE* f_lu_data = fopen(lu_data_path, "wb+");
            if (!f_lu_data)
            {
                dmLogError("Failed to create liveupdate resource file");
            }
            
            dmLogInfo("LU data path: %s", lu_data_path);
            dmStrlCpy(archive->m_LiveUpdateResourcePath, lu_data_path, DMPATH_MAX_PATH);
            archive->m_LiveUpdateResourceData = 0x0;
            archive->m_LiveUpdateResourceSize = 0;
            archive->m_LiveUpdateFileResourceData = f_lu_data;
            archive->m_LiveUpdateResourcesMemMapped = false;
        }

        // Make deep-copy. Operate on this and only overwrite when done inserting
        ArchiveIndex* ai_temp = 0x0;
        DeepCopyArchiveIndex(ai_temp, archive, true);

        // From now on we only work on ai_temp until done
        hashes = (uint8_t*)((uintptr_t)ai_temp + JAVA_TO_C(ai_temp->m_HashOffset));
        entries = (EntryData*)((uintptr_t)ai_temp + JAVA_TO_C(ai_temp->m_EntryDataOffset));

        uint32_t entry_count = JAVA_TO_C(ai_temp->m_EntryDataCount);
        // Shift hashes after index idx down
        uint8_t* hash_shift_src = (uint8_t*)((uintptr_t)hashes + DMRESOURCE_MAX_HASH * idx);
        uint8_t* hash_shift_dst = (uint8_t*)((uintptr_t)hash_shift_src + DMRESOURCE_MAX_HASH);
        if (idx < entry_count) // no need to memmove if inserting at tail
        {
            uint32_t size_to_shift = (entry_count - idx) * DMRESOURCE_MAX_HASH;
            memmove((void*)hash_shift_dst, (void*)hash_shift_src, size_to_shift);
        }
        memcpy(hash_shift_src, hash_digest, hash_digest_len);

        // Shift entry datas
        uint8_t* entries_shift_src = (uint8_t*)((uintptr_t)entries + sizeof(EntryData) * idx);
        uint8_t* entries_shift_dst = (uint8_t*)((uintptr_t)entries_shift_src + sizeof(EntryData));
        if (idx < entry_count)
        {
            uint32_t size_to_shift = (entry_count - idx) * sizeof(EntryData);
            memmove(entries_shift_dst, entries_shift_src, size_to_shift);
        }

        // Write buf to resource file before creating EntryData instance
        uint32_t bytes_written;
        uint32_t offs;
        Result write_res = WriteResourceToArchive(archive, (uint8_t*)resource->m_Data, resource->m_Count, bytes_written, offs);
        if (write_res != RESULT_OK)
        {
            dmLogError("All bytes not written for resource, bytes written: %u, resource size: %u", bytes_written, resource->m_Count);
            delete ai_temp;
            return RESULT_IO_ERROR;
        }

        // Create entrydata and copy it to temp index
        bool is_compressed = (resource->m_Header->m_Flags & ENTRY_FLAG_COMPRESSED);
        EntryData entry;
        entry.m_ResourceDataOffset = C_TO_JAVA(offs);
        entry.m_ResourceSize = is_compressed ? resource->m_Header->m_Size : C_TO_JAVA(resource->m_Count);
        entry.m_ResourceCompressedSize = is_compressed ? C_TO_JAVA(resource->m_Count) : (C_TO_JAVA(0xffffffff));
        entry.m_Flags = C_TO_JAVA(resource->m_Header->m_Flags | ENTRY_FLAG_LIVEUPDATE_DATA);
        memcpy((void*)entries_shift_src, (void*)&entry, sizeof(EntryData));

        if (!archive->m_IsMemMapped)
        {
            delete archive->m_ArchiveIndex;
        }

        ai_temp->m_EntryDataCount = C_TO_JAVA(JAVA_TO_C(ai_temp->m_EntryDataCount) + 1);

        // DEBUG print entry contents
        dmLogInfo("Hash to insert at idx: %i", idx);
        PrintHash(hash_digest, hash_digest_len);
        dmLogInfo("AFTER INSERTION, count: %u", JAVA_TO_C(ai_temp->m_EntryDataCount));
        for (int i = 0; i < JAVA_TO_C(ai_temp->m_EntryDataCount); ++i)
        {
            PrintHash((uint8_t*)((uintptr_t)hashes + DMRESOURCE_MAX_HASH * i), JAVA_TO_C(ai_temp->m_HashLength));
            EntryData& e = entries[i];
            dmLogInfo("offs: %u, size: %u, comp_size: %u, flags: %u", JAVA_TO_C(e.m_ResourceDataOffset), JAVA_TO_C(e.m_ResourceSize), JAVA_TO_C(e.m_ResourceCompressedSize), JAVA_TO_C(e.m_Flags));
        }
        // END DEBUG

        // Use this runtime archive index for the remainder of this engine instance
        archive->m_ArchiveIndex = ai_temp;

        // Since we store data sequentially when doing the deep-copy we want to access it in that fashion
        archive->m_IsMemMapped = true;

        // Write to temporary index file, should have filename liveupdate.arci-temp
        dmStrlCat(lu_index_path, "-temp", DMPATH_MAX_PATH);
        FILE* f_lu_index = fopen(lu_index_path, "wb");
        if (!f_lu_index)
        {
            dmLogError("Failed to create liveupdate index file");
        }
        entry_count = JAVA_TO_C(ai_temp->m_EntryDataCount);
        uint32_t total_size = sizeof(ArchiveIndex) + entry_count * DMRESOURCE_MAX_HASH + entry_count * sizeof(EntryData);
        if (fwrite((void*)ai_temp, 1, total_size, f_lu_index) != total_size)
        {
            fclose(f_lu_index);
            dmLogError("Failed to write liveupdate index file");
            return RESULT_IO_ERROR;
        }
        fclose(f_lu_index);

        return result;
    }

    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, EntryData* entry)
    {
        uint32_t entry_count = JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_HashOffset);
        uint32_t hash_len = JAVA_TO_C(archive->m_ArchiveIndex->m_HashLength);
        uint8_t* hashes = 0;
        EntryData* entries = 0;

        dmLogInfo("entry_count: %u, entry_offset: %u", entry_count, entry_offset);

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_Hashes;
            entries = archive->m_Entries;
        }
        else
        {
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            entries = (EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
        }

        // Search for hash with binary search (entries are sorted on hash)
        int first = 0;
        int last = (int)entry_count-1;
        while (first <= last)
        {
            int mid = first + (last - first) / 2;
            uint8_t* h = (hashes + DMRESOURCE_MAX_HASH * mid);

            int cmp = memcmp(hash, h, hash_len);
            if (cmp == 0)
            {
                if (entry != NULL)
                {
                    EntryData* e = &entries[mid];
                    entry->m_ResourceDataOffset = JAVA_TO_C(e->m_ResourceDataOffset);
                    entry->m_ResourceSize = JAVA_TO_C(e->m_ResourceSize);
                    entry->m_ResourceCompressedSize = JAVA_TO_C(e->m_ResourceCompressedSize);
                    entry->m_Flags = JAVA_TO_C(e->m_Flags);
                }

                return RESULT_OK;
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

        return RESULT_NOT_FOUND;
    }

    Result Read(HArchiveIndexContainer archive, EntryData* entry_data, void* buffer)
    {
        uint32_t size = entry_data->m_ResourceSize;
        uint32_t compressed_size = entry_data->m_ResourceCompressedSize;

        dmLogInfo("Reading resource, index memmapped?: %i, LU?: %i, comp: %i, encr: %i", archive->m_IsMemMapped, 
            (entry_data->m_Flags & ENTRY_FLAG_LIVEUPDATE_DATA) == ENTRY_FLAG_LIVEUPDATE_DATA,
            (entry_data->m_Flags & ENTRY_FLAG_COMPRESSED) == ENTRY_FLAG_COMPRESSED,
            (entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED) == ENTRY_FLAG_ENCRYPTED);

        bool loaded_with_liveupdate = (entry_data->m_Flags & ENTRY_FLAG_LIVEUPDATE_DATA);
        bool resource_memmapped = false;

        if (loaded_with_liveupdate)
          resource_memmapped = archive->m_LiveUpdateResourcesMemMapped;
        else
           resource_memmapped = archive->m_IsMemMapped;

        if (!resource_memmapped)
        {
            FILE* resource_file;

            if (loaded_with_liveupdate)
            {
                resource_file = archive->m_LiveUpdateFileResourceData;
            }
            else
            {
                resource_file = archive->m_FileResourceData;
            }

            fseek(resource_file, entry_data->m_ResourceDataOffset, SEEK_SET);
            if (compressed_size != 0xFFFFFFFF) // resource is compressed
            {
                char *compressed_buf = (char*)malloc(compressed_size);
                if (!compressed_buf)
                {
                    return RESULT_MEM_ERROR;
                }

                if (fread(compressed_buf, 1, compressed_size, resource_file) != compressed_size)
                {
                    free(compressed_buf);
                    return RESULT_IO_ERROR;
                }

                if(entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED)
                {
                    dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) compressed_buf, compressed_size, (const uint8_t*) KEY, strlen(KEY));
                    if (cr != dmCrypt::RESULT_OK)
                    {
                        free(compressed_buf);
                        return RESULT_UNKNOWN;
                    }
                }

                dmLZ4::Result r = dmLZ4::DecompressBufferFast(compressed_buf, compressed_size, buffer, size);
                free(compressed_buf);

                if (r == dmLZ4::RESULT_OK)
                {
                    return RESULT_OK;
                }
                else
                {
                    return RESULT_OUTBUFFER_TOO_SMALL;
                }
            }
            else
            {
                // Entry is uncompressed
                if (fread(buffer, 1, size, resource_file) == size)
                {
                    dmCrypt::Result cr = dmCrypt::RESULT_OK;
                    if (entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED)
                    {
                        cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) buffer, size, (const uint8_t*) KEY, strlen(KEY));
                    }
                    return (cr == dmCrypt::RESULT_OK) ? RESULT_OK : RESULT_UNKNOWN;
                }
                else
                {
                    return RESULT_OUTBUFFER_TOO_SMALL;
                }
            }
        }
        else
        {
            void* r = 0x0;
            if (loaded_with_liveupdate)
            {
                dmLogInfo("Reading LU resource data! compressed_size = %u", compressed_size);
                uint32_t bufsize = (compressed_size != 0xFFFFFFFF) ? compressed_size : size;
                r = (void*) (((uintptr_t)archive->m_LiveUpdateResourceData + entry_data->m_ResourceDataOffset));
                dmLogInfo("Reading LU resource with MEMMAP!");
            }
            else
            {
                r = (void*) (((uintptr_t)archive->m_ResourceData + entry_data->m_ResourceDataOffset));
            }

            Result ret;

            void* decrypted = r;

            if (entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED)
            {
                uint32_t bufsize = (compressed_size != 0xFFFFFFFF) ? compressed_size : size;
                decrypted = (uint8_t*) malloc(bufsize);
                memcpy(decrypted, r, bufsize);
                dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) decrypted, bufsize, (const uint8_t*) KEY, strlen(KEY));
                if (cr != dmCrypt::RESULT_OK)
                {
                    free(decrypted);
                    return RESULT_UNKNOWN;
                }
            }

            if (compressed_size != 0xFFFFFFFF)
            {
                // Entry is compressed
                dmLogInfo("Entry is compressed, size: %u, compressed_size: %u", size, compressed_size);
                dmLZ4::Result result = dmLZ4::DecompressBufferFast(decrypted, compressed_size, buffer, size);
                if (result == dmLZ4::RESULT_OK)
                {
                    ret = RESULT_OK;
                }
                else
                {
                    dmLogInfo("Decompression failed with result = %i", result);
                    ret = RESULT_OUTBUFFER_TOO_SMALL;
                }
            }
            else
            {
                // Entry is uncompressed
                memcpy(buffer, decrypted, size);
                ret = RESULT_OK;
            }

            // if needed aux buffer
            if (decrypted != r)
            {
                free(decrypted);
            }

            return ret;
        }
    }

    uint32_t GetEntryCount(HArchiveIndexContainer archive)
    {
        return JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataCount);
    }
}  // namespace dmResourceArchive
