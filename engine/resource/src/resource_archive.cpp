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

#include "resource.h"
#include "resource_archive_private.h"
#include <dlib/dstrings.h>
#include <dlib/lz4.h>
#include <dlib/log.h>
#include <dlib/crypt.h>
#include <dlib/path.h>
#include <dlib/sys.h>

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

namespace dmResourceArchive
{
    const static uint64_t FILE_LOADED_INDICATOR = 1337;
    const char* KEY = "aQj8CScgNP4VsfXK";

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
        (*archive)->m_ResourcesMemMapped = true;
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

        return RESULT_OK;
    }

    int CmpArchiveIdentifier(const HArchiveIndexContainer archive_container, uint8_t* archive_id, uint32_t len)
    {
        return memcmp(archive_container->m_ArchiveIndex->m_ArchiveIndexMD5, archive_id, len);
    }

    uint32_t CountLiveUpdateEntries(const ArchiveIndexContainer* lu_archive_container, const ArchiveIndexContainer* bundled_archive_container)
    {
        uint32_t count = 0;
        uint32_t entry_count = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entries_offset = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_HashOffset);
        uint32_t bundled_hash_offset = JAVA_TO_C(bundled_archive_container->m_ArchiveIndex->m_HashOffset);

        uint8_t* hashes = (!lu_archive_container->m_IsMemMapped) ? lu_archive_container->m_Hashes : (uint8_t*)((uintptr_t)lu_archive_container->m_ArchiveIndex + hash_offset);
        EntryData* entries = (!lu_archive_container->m_IsMemMapped) ? lu_archive_container->m_Entries : (EntryData*)((uintptr_t)lu_archive_container->m_ArchiveIndex + entries_offset);

        uint8_t* bundled_hashes = (!bundled_archive_container->m_IsMemMapped) ? bundled_archive_container->m_Hashes : (uint8_t*)((uintptr_t)bundled_archive_container->m_ArchiveIndex + bundled_hash_offset);

        // Count number of liveupdate entries to cache
        for (int i = 0; i < entry_count; ++i)
        {
            EntryData& e = entries[i];
            if (JAVA_TO_C(e.m_Flags) & ENTRY_FLAG_LIVEUPDATE_DATA)
            {
                // Calc insertion index into bundled archive to see if entry now resides in bundled archive instead
                uint8_t* entry_hash = (uint8_t*)((uintptr_t)hashes + DMRESOURCE_MAX_HASH * i);
                int insert_index = -1;
                Result index_result = GetInsertionIndex(bundled_archive_container->m_ArchiveIndex, entry_hash, bundled_hashes, &insert_index);

                if (index_result == RESULT_OK)
                {
                    ++count;
                }
            }
        }

        return count;
    }

    // Looks for and caches liveupdate entries that are in the liveupdate archive index and NOT in the bundled archive
    void CacheLiveUpdateEntries(const ArchiveIndexContainer* lu_archive_container, const ArchiveIndexContainer* bundled_archive_container, LiveUpdateEntries* lu_hashes_entries)
    {
        uint32_t entry_count = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_EntryDataCount);
        uint32_t hash_length = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_HashLength);
        uint32_t hash_offset = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_HashOffset);
        uint32_t entries_offset = JAVA_TO_C(lu_archive_container->m_ArchiveIndex->m_EntryDataOffset);

        uint8_t* hashes = (!lu_archive_container->m_IsMemMapped) ? lu_archive_container->m_Hashes : (uint8_t*)((uintptr_t)lu_archive_container->m_ArchiveIndex + hash_offset);
        EntryData* entries = (!lu_archive_container->m_IsMemMapped) ? lu_archive_container->m_Entries : (EntryData*)((uintptr_t)lu_archive_container->m_ArchiveIndex + entries_offset);

        uint32_t bundled_hash_offset = JAVA_TO_C(bundled_archive_container->m_ArchiveIndex->m_HashOffset);
        uint8_t* bundled_hashes = (!bundled_archive_container->m_IsMemMapped) ? bundled_archive_container->m_Hashes : (uint8_t*)((uintptr_t)bundled_archive_container->m_ArchiveIndex + bundled_hash_offset);

        uint32_t lu_entry_count = CountLiveUpdateEntries(lu_archive_container, bundled_archive_container);

        // Cache liveupdate entries
        uint8_t* lu_hashes = (uint8_t*)malloc(hash_length * lu_entry_count);
        EntryData* lu_entries = (EntryData*)malloc(sizeof(EntryData) * lu_entry_count);
        uint32_t lu_counter = 0;
        for (int i = 0; i < entry_count; ++i)
        {
            EntryData& e = entries[i];
            if (JAVA_TO_C(e.m_Flags) & ENTRY_FLAG_LIVEUPDATE_DATA)
            {
                uint8_t* entry_hash = (uint8_t*)((uintptr_t)hashes + DMRESOURCE_MAX_HASH * i);
                int insert_index = -1;
                Result index_result = GetInsertionIndex(bundled_archive_container->m_ArchiveIndex, entry_hash, bundled_hashes, &insert_index);

                if (index_result == RESULT_OK)
                {
                    memcpy((void*)((uintptr_t)lu_hashes + hash_length * lu_counter), (void*)((uintptr_t)hashes + DMRESOURCE_MAX_HASH * i), hash_length);
                    memcpy((void*)((uintptr_t)lu_entries + sizeof(EntryData) * lu_counter), (void*)((uintptr_t)entries + sizeof(EntryData) * i), sizeof(EntryData));
                    ++lu_counter;
                }
            }
        }

        lu_hashes_entries->m_Hashes = lu_hashes;
        lu_hashes_entries->m_HashLen = hash_length;
        lu_hashes_entries->m_Entries = lu_entries;
        lu_hashes_entries->m_Count = lu_entry_count;
    }

    // * Cache all liveupdate entries that are NOT in the (new) bundled archive
    // * Make a copy of the bundled archive with space allocated for the cached liveupdate entries
    // * Insert liveupdate entries sorted on hash in the bundled archive copy
    // * We now have the union of bundled archive entries and liveupdate entries (correct)
    Result ReloadBundledArchiveIndex(const char* bundled_index_path, const char* bundled_resource_path, const char* lu_index_path, const char* lu_resource_path, ArchiveIndexContainer*& lu_index_container, void*& index_mount_info)
    {
        LiveUpdateEntries* lu_entries = new LiveUpdateEntries;
        void* bundled_index_mount_info = 0;

        // Mount bundled archive and make deep copy
        ArchiveIndexContainer* bundled_archive_container = 0x0;
        ArchiveIndex* reloaded_index = 0x0;
        dmResource::Result mount_result = dmResource::MountArchiveInternal(bundled_index_path, bundled_resource_path, lu_resource_path, &bundled_archive_container, &bundled_index_mount_info);
        if (mount_result != dmResource::RESULT_OK)
        {
            dmLogError("Failed to mount bundled archive index during reload, result = %i", mount_result);
            delete lu_entries;
            return RESULT_IO_ERROR;
        }

        // Cache liveupdate entries and unmount liveupdate index
        CacheLiveUpdateEntries(lu_index_container, bundled_archive_container, lu_entries);
        dmResource::UnmountArchiveInternal(lu_index_container, index_mount_info);
        index_mount_info = bundled_index_mount_info;

        // Construct reloaded index, which is union of bundled index and cached liveupdate entries
        NewArchiveIndexFromCopy(reloaded_index, bundled_archive_container, lu_entries->m_Count);
        uint32_t hash_len = lu_entries->m_HashLen;
        uint8_t* reloaded_hashes = (uint8_t*)(uintptr_t(reloaded_index) + JAVA_TO_C(reloaded_index->m_HashOffset));
        for (int i = 0; i < lu_entries->m_Count; ++i)
        {
            int insert_index = -1;
            const uint8_t* hash = (const uint8_t*)((uintptr_t)lu_entries->m_Hashes + hash_len * i);
            const EntryData* entry = (EntryData*)((uintptr_t)lu_entries->m_Entries + sizeof(EntryData) * i);
            GetInsertionIndex(reloaded_index, hash, (const uint8_t*)reloaded_hashes, &insert_index);

            // Insert each liveupdate entry WITHOUT writing any resource data!
            Result insert_result = ShiftAndInsert(bundled_archive_container, reloaded_index, hash, hash_len, insert_index, 0x0, entry);
            if (insert_result != RESULT_OK)
            {
                dmLogError("Failed to shift and insert during reload, result = %i", insert_result);
                free(lu_entries->m_Entries);
                free((void*)lu_entries->m_Hashes);
                delete lu_entries;
                return RESULT_IO_ERROR;
            }
        }

        if (!bundled_archive_container->m_IsMemMapped)
        {
            delete bundled_archive_container->m_ArchiveIndex;
        }

        bundled_archive_container->m_ArchiveIndex = reloaded_index;
        bundled_archive_container->m_IsMemMapped = true;

        // reloaded_index is now the union of bundled archive index and liveupdate entries
        // use it as runtime index, and write it to liveupdate.arci.tmp
        lu_index_container = bundled_archive_container;

        // Write to temporary index file, filename liveupdate.arci.tmp, to be used at next engine init
        char lu_temp_index_path[DMPATH_MAX_PATH];
        dmStrlCpy(lu_temp_index_path, lu_index_path, DMPATH_MAX_PATH);
        dmStrlCat(lu_temp_index_path, ".tmp", DMPATH_MAX_PATH);
        FILE* f_lu_index = fopen(lu_temp_index_path, "wb");
        if (!f_lu_index)
        {
            dmLogError("Failed to create liveupdate index file");
            free(lu_entries->m_Entries);
            free((void*)lu_entries->m_Hashes);
            delete lu_entries;
            return RESULT_IO_ERROR;
        }
        uint32_t entry_count = JAVA_TO_C(reloaded_index->m_EntryDataCount);
        uint32_t total_size = sizeof(ArchiveIndex) + entry_count * DMRESOURCE_MAX_HASH + entry_count * sizeof(EntryData);
        uint32_t written_bytes = fwrite((void*)reloaded_index, 1, total_size, f_lu_index);
        if (written_bytes != total_size)
        {
            dmLogError("Failed to write liveupdate index file, written bytes: %u, expected: %u", written_bytes, total_size);
            fclose(f_lu_index);
            free(lu_entries->m_Entries);
            free((void*)lu_entries->m_Hashes);
            delete lu_entries;
            return RESULT_IO_ERROR;
        }
        fclose(f_lu_index);

        free(lu_entries->m_Entries);
        free((void*)lu_entries->m_Hashes);
        delete lu_entries;

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
        aic->m_ResourcesMemMapped = false;

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

    void Delete(ArchiveIndex* archive)
    {
        if (archive != 0x0)
        {
            delete[] (uint8_t*)archive;
        }
    }

    Result GetInsertionIndex(ArchiveIndex* archive, const uint8_t* hash_digest, const uint8_t* hashes, int* out_index)
    {
        int first = 0;
        int last = JAVA_TO_C(archive->m_EntryDataCount);
        int mid = first + (last - first) / 2;
        while (first <= last && first !=mid)
        {
            mid = first + (last - first) / 2;
            const uint8_t* h = (hashes + DMRESOURCE_MAX_HASH * mid);

            int cmp = memcmp(hash_digest, h, JAVA_TO_C(archive->m_HashLength));
            if (cmp == 0)
            {
                // attemping to insert an already inserted resource
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

        *out_index = mid;
        return RESULT_OK;
    }

    Result GetInsertionIndex(HArchiveIndexContainer archive, const uint8_t* hash_digest, int* out_index)
    {
        uint8_t* hashes = 0;
        uint32_t hashes_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_HashOffset);

        hashes = (archive->m_IsMemMapped) ? (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hashes_offset) : archive->m_Hashes;

        return GetInsertionIndex(archive->m_ArchiveIndex, hash_digest, hashes, out_index);
    }

    void NewArchiveIndexFromCopy(ArchiveIndex*& dst, ArchiveIndexContainer* src, uint32_t extra_entries_alloc)
    {
        ArchiveIndex* ai = src->m_ArchiveIndex;
        uint32_t hash_digests_size = JAVA_TO_C(ai->m_EntryDataCount) * DMRESOURCE_MAX_HASH;
        uint32_t entry_datas_size = (JAVA_TO_C(ai->m_EntryDataCount) * sizeof(EntryData));
        uint32_t single_entry_size = DMRESOURCE_MAX_HASH + sizeof(EntryData);
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
            memcpy(cursor, src->m_Hashes, hash_digests_size);
            cursor = (uint8_t*)((uintptr_t)cursor + hash_digests_size); // step cursor to entry data array
            if (extra_entries_alloc > 0)
            {
                cursor = (uint8_t*)((uintptr_t)cursor + DMRESOURCE_MAX_HASH * extra_entries_alloc);
            }
            memcpy(cursor, src->m_Entries, entry_datas_size);
        }
        else
        {
            memcpy(dst, ai, sizeof(ArchiveIndex)); // copy header data
            uint8_t* cursor =  (uint8_t*)((uintptr_t)dst + sizeof(ArchiveIndex)); // step cursor to hash digests array
            memcpy(cursor, (void*)((uintptr_t)ai + JAVA_TO_C(ai->m_HashOffset)), hash_digests_size);
            cursor = (uint8_t*)((uintptr_t)cursor + hash_digests_size); // step cursor to entry data array
            if (extra_entries_alloc > 0)
            {
                cursor = (uint8_t*)((uintptr_t)cursor + DMRESOURCE_MAX_HASH * extra_entries_alloc);
            }
            memcpy(cursor, (void*)(((uintptr_t)ai + JAVA_TO_C(ai->m_EntryDataOffset))), entry_datas_size);
        }

        if (extra_entries_alloc > 0)
        {
            dst->m_EntryDataOffset = C_TO_JAVA(JAVA_TO_C(dst->m_EntryDataOffset) + DMRESOURCE_MAX_HASH * extra_entries_alloc);
        }
    }

    Result WriteResourceToArchive(HArchiveIndexContainer& archive, const uint8_t* buf, size_t buf_len, uint32_t& bytes_written, uint32_t& offset)
    {
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
        }

        return RESULT_OK;
    }

    Result ShiftAndInsert(ArchiveIndexContainer* archive_container, ArchiveIndex* ai, const uint8_t* hash_digest, uint32_t hash_digest_len, int insertion_index, const dmResourceArchive::LiveUpdateResource* resource, const EntryData* lu_entry_data)
    {
        assert(insertion_index >= 0);
        ArchiveIndex* archive = (ai == 0x0) ? archive_container->m_ArchiveIndex : ai;
        uint8_t* hashes = (uint8_t*)((uintptr_t)archive + JAVA_TO_C(archive->m_HashOffset));
        EntryData* entries = (EntryData*)((uintptr_t)archive + JAVA_TO_C(archive->m_EntryDataOffset));

        uint32_t entry_count = JAVA_TO_C(archive->m_EntryDataCount);
        // Shift hashes after insertion_index down
        uint8_t* hash_shift_src = (uint8_t*)((uintptr_t)hashes + DMRESOURCE_MAX_HASH * insertion_index);
        uint8_t* hash_shift_dst = (uint8_t*)((uintptr_t)hash_shift_src + DMRESOURCE_MAX_HASH);
        if (insertion_index < entry_count) // no need to memmove if inserting at tail
        {
            uint32_t size_to_shift = (entry_count - insertion_index) * DMRESOURCE_MAX_HASH;
            memmove((void*)hash_shift_dst, (void*)hash_shift_src, size_to_shift);
        }
        memcpy(hash_shift_src, hash_digest, hash_digest_len);

        // Shift entry datas
        uint8_t* entries_shift_src = (uint8_t*)((uintptr_t)entries + sizeof(EntryData) * insertion_index);
        uint8_t* entries_shift_dst = (uint8_t*)((uintptr_t)entries_shift_src + sizeof(EntryData));
        if (insertion_index < entry_count)
        {
            uint32_t size_to_shift = (entry_count - insertion_index) * sizeof(EntryData);
            memmove(entries_shift_dst, entries_shift_src, size_to_shift);
        }

        bool write_resources = resource != 0x0;

        EntryData entry;

        if (!write_resources)
        {
            memcpy(&entry, lu_entry_data, sizeof(EntryData));
        }
        else
        {
            /// --- WRITE RESOURCE START
            // Write buf to resource file before creating EntryData instance
            uint32_t bytes_written;
            uint32_t offs;
            Result write_res = WriteResourceToArchive(archive_container, (uint8_t*)resource->m_Data, resource->m_Count, bytes_written, offs);
            if (write_res != RESULT_OK)
            {
                dmLogError("All bytes not written for resource, bytes written: %u, resource size: %zu", bytes_written, resource->m_Count);
                delete archive;
                return RESULT_IO_ERROR;
            }

            // Create entrydata instance and insert into index
            bool is_compressed = (resource->m_Header->m_Flags & ENTRY_FLAG_COMPRESSED);
            //EntryData entry;
            entry.m_ResourceDataOffset = C_TO_JAVA(offs);
            entry.m_ResourceSize = is_compressed ? resource->m_Header->m_Size : C_TO_JAVA(resource->m_Count);
            entry.m_ResourceCompressedSize = is_compressed ? C_TO_JAVA(resource->m_Count) : (C_TO_JAVA(0xffffffff));
            entry.m_Flags = C_TO_JAVA(resource->m_Header->m_Flags | ENTRY_FLAG_LIVEUPDATE_DATA);
            /// --- WRITE RESOURCE END
        }

        memcpy((void*)entries_shift_src, (void*)&entry, sizeof(EntryData));
        archive->m_EntryDataCount = C_TO_JAVA(JAVA_TO_C(archive->m_EntryDataCount) + 1);
        return RESULT_OK;
    }

    void CreateFilesIfNotExists(ArchiveIndexContainer* archive_container, const char* lu_index_path)
    {
        struct stat file_stat;
        bool resource_exists = stat(lu_index_path, &file_stat) == 0;

        // If liveupdate.arci does not exists, create it and liveupdate.arcd
        if (!resource_exists)
        {
            fopen(lu_index_path, "wb");

            // Data file has same path and filename as index file, but extension .arcd instead of .arci.
            char lu_data_path[DMPATH_MAX_PATH];
            dmStrlCpy(lu_data_path, lu_index_path, DMPATH_MAX_PATH);
            lu_data_path[strlen(lu_index_path)-1] = 'd';

            FILE* f_lu_data = fopen(lu_data_path, "wb+");
            if (!f_lu_data)
            {
                dmLogError("Failed to create liveupdate resource file");
            }

            dmStrlCpy(archive_container->m_LiveUpdateResourcePath, lu_data_path, DMPATH_MAX_PATH);
            archive_container->m_LiveUpdateResourceData = 0x0;
            archive_container->m_LiveUpdateResourceSize = 0;
            archive_container->m_LiveUpdateFileResourceData = f_lu_data;
            archive_container->m_LiveUpdateResourcesMemMapped = false;
        }
    }

    Result NewArchiveIndexWithResource(HArchiveIndexContainer archive_container, const uint8_t* hash_digest, uint32_t hash_digest_len, const dmResourceArchive::LiveUpdateResource* resource, const char* proj_id, HArchiveIndex& out_new_index)
    {
        out_new_index = 0x0;

        int idx = -1;
        Result index_result = GetInsertionIndex(archive_container, hash_digest, &idx);
        if (index_result != RESULT_OK)
        {
            dmLogError("Could not calculate valid resource insertion index");
            return index_result;
        }

        char app_support_path[DMPATH_MAX_PATH];
        char lu_index_path[DMPATH_MAX_PATH];

        dmSys::GetApplicationSupportPath(proj_id, app_support_path, DMPATH_MAX_PATH);
        dmPath::Concat(app_support_path, "liveupdate.arci", lu_index_path, DMPATH_MAX_PATH);
        CreateFilesIfNotExists(archive_container, lu_index_path);

        // Make deep-copy. Operate on this and only overwrite when done inserting
        ArchiveIndex* ai_temp = 0x0;
        NewArchiveIndexFromCopy(ai_temp, archive_container, 1);

        // Shift buffers and insert index- and resource data
        Result insert_result = ShiftAndInsert(archive_container, ai_temp, hash_digest, hash_digest_len, idx, resource, 0x0);

        if (insert_result != RESULT_OK)
        {
            delete ai_temp;
            dmLogError("Failed to insert resource, result = %i", insert_result);
            return insert_result;
        }

        // Write to temporary index file, filename liveupdate.arci.tmp
        dmStrlCat(lu_index_path, ".tmp", DMPATH_MAX_PATH);
        FILE* f_lu_index = fopen(lu_index_path, "wb");
        if (!f_lu_index)
        {
            dmLogError("Failed to create liveupdate index file");
            return RESULT_IO_ERROR;
        }
        uint32_t entry_count = JAVA_TO_C(ai_temp->m_EntryDataCount);
        uint32_t total_size = sizeof(ArchiveIndex) + entry_count * DMRESOURCE_MAX_HASH + entry_count * sizeof(EntryData);
        if (fwrite((void*)ai_temp, 1, total_size, f_lu_index) != total_size)
        {
            fclose(f_lu_index);
            dmLogError("Failed to write liveupdate index file");
            return RESULT_IO_ERROR;
        }
        fclose(f_lu_index);

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
        // Use this runtime archive index for the remainder of this engine instance
        archive_container->m_ArchiveIndex = new_index;
        // Since we store data sequentially when doing the deep-copy we want to access it in that fashion
        archive_container->m_IsMemMapped = mem_mapped;
    }

    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, EntryData* entry)
    {
        uint32_t entry_count = JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = JAVA_TO_C(archive->m_ArchiveIndex->m_HashOffset);
        uint32_t hash_len = JAVA_TO_C(archive->m_ArchiveIndex->m_HashLength);
        uint8_t* hashes = 0;
        EntryData* entries = 0;

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

        bool loaded_with_liveupdate = (entry_data->m_Flags & ENTRY_FLAG_LIVEUPDATE_DATA);
        bool resource_memmapped = false;

        if (loaded_with_liveupdate)
            resource_memmapped = archive->m_LiveUpdateResourcesMemMapped;
        else
            resource_memmapped = archive->m_ResourcesMemMapped;

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
                r = (void*) (((uintptr_t)archive->m_LiveUpdateResourceData + entry_data->m_ResourceDataOffset));
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
                dmLZ4::Result result = dmLZ4::DecompressBufferFast(decrypted, compressed_size, buffer, size);
                if (result == dmLZ4::RESULT_OK)
                {
                    ret = RESULT_OK;
                }
                else
                {
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

    uint32_t GetEntryDataOffset(ArchiveIndexContainer* archive_container)
    {
        return JAVA_TO_C(archive_container->m_ArchiveIndex->m_EntryDataOffset);
    }

    uint32_t GetEntryDataOffset(ArchiveIndex* archive)
    {
        return JAVA_TO_C(archive->m_EntryDataOffset);
    }
}  // namespace dmResourceArchive
