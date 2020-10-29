// Copyright 2020 The Defold Foundation
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

#ifndef _WIN32
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#endif

#include <sys/stat.h>

#include "resource.h"
#include "resource_archive_private.h"
#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/lz4.h>
#include <dlib/memory.h>
#include <dlib/path.h>
#include <dlib/sys.h>


namespace dmResourceArchive
{
    const static uint64_t FILE_LOADED_INDICATOR = 1337;
    const char* KEY = "aQj8CScgNP4VsfXK";

    int             g_NumArchiveLoaders = 0;
    ArchiveLoader   g_ArchiveLoader[4];

    ArchiveIndex::ArchiveIndex()
    {
        memset(this, 0, sizeof(ArchiveIndex));
        m_EntryDataOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
        m_HashOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
    }

    void ClearArchiveLoaders()
    {
        g_NumArchiveLoaders = 0;
    }

    void RegisterArchiveLoader(ArchiveLoader loader)
    {
        assert(g_NumArchiveLoaders < sizeof(g_ArchiveLoader)/sizeof(ArchiveLoader));
        g_ArchiveLoader[g_NumArchiveLoaders++] = loader;
    }

    Result LoadArchives(const char* archive_name, const char* app_path, const char* app_support_path, dmResource::Manifest** out_manifest, HArchiveIndexContainer* out_archive)
    {
        dmResource::Manifest* head_manifest = 0;
        HArchiveIndexContainer head_archive = 0;
        for (int i = 0; i < g_NumArchiveLoaders; ++i)
        {
            ArchiveLoader* loader = &g_ArchiveLoader[i];

            dmResource::Manifest* manifest = 0;
            Result result = loader->m_LoadManifest(archive_name, app_path, app_support_path, head_manifest, &manifest);
            if (RESULT_NOT_FOUND == result || RESULT_VERSION_MISMATCH == result)
                continue;
            if (RESULT_OK != result) {
                return result;
            }

            // The loader could opt for not loading an archive
            if (!manifest)
                continue;

            HArchiveIndexContainer archive = 0;
            result = loader->m_Load(manifest, archive_name, app_path, app_support_path, head_archive, &archive);
            if (RESULT_NOT_FOUND == result || RESULT_VERSION_MISMATCH == result)
            {
                dmResource::DeleteManifest(manifest);
                continue;
            }
            if (RESULT_OK != result)
            {
                dmResource::DeleteManifest(manifest);
                return result;
            }

            if (archive)
            {
                if (head_manifest)
                    dmResource::DeleteManifest(head_manifest);
                head_manifest = manifest;

                archive->m_Loader = *loader;
                if (head_archive != archive)
                {
                    archive->m_Next = head_archive;
                    head_archive = archive;
                }
            } else {
                // The loader could opt for not loading an archive
                dmResource::DeleteManifest(manifest);
            }
        }

        if (!head_manifest) {
            dmLogError("No manifest found");
            return RESULT_NOT_FOUND;
        }

        if (!head_archive) {
            dmLogError("No archives found");
            return RESULT_NOT_FOUND;
        }

        dmLogWarning("dmResourceArchive::LoadArchives:");
        dmResourceArchive::DebugArchiveIndex(head_archive);
        dmLogWarning("");

        *out_manifest = head_manifest;
        *out_archive = head_archive;
        return RESULT_OK;
    }

    Result UnloadArchives(HArchiveIndexContainer archive)
    {
        while (archive)
        {
            HArchiveIndexContainer next = archive->m_Next;
            Result result = archive->m_Loader.m_Unload(archive);
            if (RESULT_OK != result) {
                return result;
            }

            archive = next;
        }
        return RESULT_OK;
    }

    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, HArchiveIndexContainer* out_archive, EntryData* entry)
    {
        assert(archive != 0);
        while (archive)
        {
            ArchiveLoader* loader = &archive->m_Loader;
            Result result = loader->m_FindEntry(archive, hash, hash_len, entry);
            if (RESULT_OK == result) {
                if (out_archive)
                    *out_archive = archive;
                return result;
            }

            archive = archive->m_Next;
        }

        return RESULT_NOT_FOUND;
    }

    Result Read(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, EntryData* entry_data, void* buffer)
    {
        return archive->m_Loader.m_Read(archive, hash, hash_len, entry_data, buffer);
    }

    dmResourceArchive::Result LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out)
    {
        dmResource::Manifest* manifest = new dmResource::Manifest();
        dmResource::Result result = ManifestLoadMessage(buffer, buffer_len, manifest);

        if (dmResource::RESULT_OK == result)
            *out = manifest;
        else
        {
            dmResource::DeleteManifest(manifest);
        }

        return dmResource::RESULT_OK == result ? dmResourceArchive::RESULT_OK : dmResourceArchive::RESULT_IO_ERROR;
    }

    dmResourceArchive::Result LoadManifest(const char* path, dmResource::Manifest** out)
    {
        uint32_t manifest_length = 0;
        uint8_t* manifest_buffer = 0x0;

        uint32_t dummy_file_size = 0;
        dmSys::ResourceSize(path, &manifest_length);
        dmMemory::AlignedMalloc((void**)&manifest_buffer, 16, manifest_length);
        assert(manifest_buffer);
        dmSys::Result sys_result = dmSys::LoadResource(path, manifest_buffer, manifest_length, &dummy_file_size);

        if (sys_result != dmSys::RESULT_OK)
        {
            dmLogError("Failed to read manifest %s (%i)", path, sys_result);
            dmMemory::AlignedFree(manifest_buffer);
            return RESULT_IO_ERROR;
        }

        dmResourceArchive::Result result = LoadManifestFromBuffer(manifest_buffer, manifest_length, out);
        dmMemory::AlignedFree(manifest_buffer);
        return result;
    }

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

    static Result ResourceArchiveDefaultLoadManifest(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** out)
    {
        // This function should be called first, so there should be no prior manifest
        assert(previous == 0);

        char manifest_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_path, archive_name, manifest_path, sizeof(manifest_path));
        dmStrlCat(manifest_path, ".dmanifest", sizeof(manifest_path));

        return LoadManifest(manifest_path, out);
    }

    static Result ResourceArchiveDefaultLoad(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path, HArchiveIndexContainer previous, HArchiveIndexContainer* out)
    {
        char archive_index_path[DMPATH_MAX_PATH];
        char archive_data_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_path, archive_name, archive_index_path, sizeof(archive_index_path));
        dmPath::Concat(app_path, archive_name, archive_data_path, sizeof(archive_index_path));
        dmStrlCat(archive_index_path, ".arci", sizeof(archive_index_path));
        dmStrlCat(archive_data_path, ".arcd", sizeof(archive_data_path));

        void* mount_info = 0;
        dmResource::Result result = dmResource::MountArchiveInternal(archive_index_path, archive_data_path, 0x0, out, &mount_info);
        if (dmResource::RESULT_OK == result && mount_info != 0 && *out != 0)
        {
            (*out)->m_UserData = mount_info;
        }
        return RESULT_OK;
    }

    static Result ResourceArchiveDefaultUnload(HArchiveIndexContainer archive)
    {
        dmResource::UnmountArchiveInternal(archive, archive->m_UserData);
        return RESULT_OK;
    }

    Result FindEntryInArchive(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, EntryData* entry)
    {
        uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
        uint8_t* hashes = 0;
        EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            entries = archive->m_ArchiveFileIndex->m_Entries;
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
            uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * mid);

            int cmp = memcmp(hash, h, hash_len);
            if (cmp == 0)
            {
                if (entry != 0)
                {
                    EntryData* e = &entries[mid];
                    entry->m_ResourceDataOffset = dmEndian::ToNetwork(e->m_ResourceDataOffset);
                    entry->m_ResourceSize = dmEndian::ToNetwork(e->m_ResourceSize);
                    entry->m_ResourceCompressedSize = dmEndian::ToNetwork(e->m_ResourceCompressedSize);
                    entry->m_Flags = dmEndian::ToNetwork(e->m_Flags);
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

    Result DecryptBuffer(void* buffer, uint32_t buffer_len)
    {
        dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) buffer, buffer_len, (const uint8_t*) KEY, strlen(KEY));
        if (cr != dmCrypt::RESULT_OK)
        {
            return RESULT_UNKNOWN;
        }
        return RESULT_OK;
    }

    Result DecompressBuffer(const void* compressed_buf, uint32_t compressed_size, void* buffer, uint32_t buffer_len)
    {
        assert(compressed_buf != buffer);
        dmLZ4::Result r = dmLZ4::DecompressBufferFast(compressed_buf, compressed_size, buffer, buffer_len);
        if (dmLZ4::RESULT_OK != r)
        {
            return RESULT_OUTBUFFER_TOO_SMALL;
        }
        return RESULT_OK;
    }

    Result ReadEntryFromArchive(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const EntryData* entry, void* buffer)
    {
        (void)hash;
        (void)hash_len;
        uint32_t size = entry->m_ResourceSize;
        uint32_t compressed_size = entry->m_ResourceCompressedSize;

        bool encrypted = (entry->m_Flags & ENTRY_FLAG_ENCRYPTED);
        bool compressed = compressed_size != 0xFFFFFFFF;

        const ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;
        bool resource_memmapped = afi->m_IsMemMapped;

        // We have multiple combinations for regular archives:
        // memory mapped (yes/no) * compressed (yes/no) * encrypted (yes/no)
        // Decryption can be done into a buffer of the same size, although not into a read only array
        // We do this is to avoid unnecessary memory allocations

        //  compressed +  encrypted +  memmapped = need malloc (encrypt cannot modify memmapped file, decompress cannot use same src/dst)
        //  compressed +  encrypted + !memmapped = need malloc (decompress cannot use same src/dst)
        //  compressed + !encrypted +  memmapped = doesn't need malloc (can decompress because src != dst)
        //  compressed + !encrypted + !memmapped = need malloc (decompress cannot use same src/dst)
        // !compressed +  encrypted +  memmapped = doesn't need malloc
        // !compressed +  encrypted + !memmapped = doesn't need malloc
        // !compressed + !encrypted +  memmapped = doesn't need malloc
        // !compressed + !encrypted + !memmapped = doesn't need malloc

        void* compressed_buf = 0;
        bool temp_buffer = false;

        if (compressed && !( !encrypted && resource_memmapped))
        {
            compressed_buf = malloc(compressed_size);
            temp_buffer = true;
        } else {
            // For uncompressed data, use the destination buffer
            compressed_buf = buffer;
            memset(buffer, 0, size);
            compressed_size = compressed ? compressed_size : size;
        }

        assert(compressed_buf);

        bool loaded_with_liveupdate = (entry->m_Flags & ENTRY_FLAG_LIVEUPDATE_DATA);
dmLogWarning("Read Entry: csz: %6u sz: %10u   lz4: %d   encr: %d   mmap: %d  tmp: %d  LU: %d  archive: %p", compressed_size, size, compressed?1:0, encrypted?1:0, resource_memmapped?1:0, temp_buffer?1:0, loaded_with_liveupdate?1:0, archive);

        if (!resource_memmapped)
        {
            FILE* resource_file = afi->m_FileResourceData;

            assert(temp_buffer || compressed_buf == buffer);

            fseek(resource_file, entry->m_ResourceDataOffset, SEEK_SET);
            if (fread(compressed_buf, 1, compressed_size, resource_file) != compressed_size)
            {
                if (temp_buffer)
                    free(compressed_buf);
                return RESULT_IO_ERROR;
            }
        } else {
            void* r = (void*) (((uintptr_t)afi->m_ResourceData + entry->m_ResourceDataOffset));

            if (compressed && !encrypted)
            {
                compressed_buf = r;
            } else {
                memcpy(compressed_buf, r, compressed_size);
            }
        }

        // Design wish: If we switch the order of operations (and thus the file format)
        // we can simplify the code and save a temporary allocation
        //
        // Currently, we need to decrypt into a new buffer (since the input is read only):
        // input:[Encrypted + LZ4] ->   temp:[  LZ4      ] -> output:[   data   ]
        //
        // We could instead have:
        // input:[Encrypted + LZ4] -> output:[ Encrypted ] -> output:[   data   ]

        if(encrypted)
        {
            assert(temp_buffer || compressed_buf == buffer);
            dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) compressed_buf, compressed_size, (const uint8_t*) KEY, strlen(KEY));
            if (cr != dmCrypt::RESULT_OK)
            {
                if (temp_buffer)
                    free(compressed_buf);
                return RESULT_UNKNOWN;
            }
        }

        if (compressed)
        {
            assert(compressed_buf != buffer);
            dmLZ4::Result r = dmLZ4::DecompressBufferFast(compressed_buf, compressed_size, buffer, size);
            if (dmLZ4::RESULT_OK != r)
            {
                if (temp_buffer)
                    free(compressed_buf);
                return RESULT_OUTBUFFER_TOO_SMALL;
            }
        } else {
            if (buffer != compressed_buf)
                memcpy(buffer, compressed_buf, size);
        }

        if (temp_buffer)
            free(compressed_buf);

        return RESULT_OK;
    }

    void RegisterDefaultArchiveLoader()
    {
        dmResourceArchive::ArchiveLoader loader;
        loader.m_LoadManifest = ResourceArchiveDefaultLoadManifest;
        loader.m_Load = ResourceArchiveDefaultLoad;
        loader.m_Unload = ResourceArchiveDefaultUnload;
        loader.m_FindEntry = FindEntryInArchive;
        loader.m_Read = ReadEntryFromArchive;
        dmResourceArchive::RegisterArchiveLoader(loader);
    }

    void SetDefaultReader(HArchiveIndexContainer archive)
    {
        memset(&archive->m_Loader, 0, sizeof(archive->m_Loader));
        archive->m_Loader.m_FindEntry = FindEntryInArchive;
        archive->m_Loader.m_Read = ReadEntryFromArchive;
    }

    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, bool mem_mapped_index,
                             const void* resource_data, uint32_t resource_data_size, bool mem_mapped_data,
                             const char* lu_resource_filename,
                             const void* lu_resource_data, uint32_t lu_resource_data_size,
                             FILE* f_lu_resource_data, HArchiveIndexContainer* archive)
    {
        *archive = new ArchiveIndexContainer;
        (*archive)->m_IsMemMapped = mem_mapped_index;
        ArchiveIndex* a = (ArchiveIndex*) index_buffer;
        uint32_t version = dmEndian::ToNetwork(a->m_Version);
        if (version != VERSION)
        {
            return RESULT_VERSION_MISMATCH;
        }

        (*archive)->m_ArchiveFileIndex = new ArchiveFileIndex;
        (*archive)->m_ArchiveFileIndex->m_ResourceData = (uint8_t*)resource_data;
        (*archive)->m_ArchiveFileIndex->m_ResourceSize = resource_data_size;
        (*archive)->m_ArchiveFileIndex->m_IsMemMapped = mem_mapped_data;

        assert(lu_resource_data == 0);
        assert(lu_resource_data_size == 0);

        if (lu_resource_filename)
        {
            dmStrlCpy((*archive)->m_ArchiveFileIndex->m_Path, lu_resource_filename != 0 ? lu_resource_filename : "", DMPATH_MAX_PATH);
            dmLogInfo("Live Update archive: '%s'", (*archive)->m_ArchiveFileIndex->m_Path);
        }

        (*archive)->m_ArchiveFileIndex->m_FileResourceData = f_lu_resource_data;
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
            }

            if (afi->m_IsMemMapped)
            {
                void* tmp_ptr = (void*)afi->m_ResourceData;
                dmResource::UnmapFile(tmp_ptr, afi->m_ResourceSize);
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
        else
        {
            void* tmp_ptr = (void*)archive->m_ArchiveIndex;
            dmResource::UnmapFile(tmp_ptr, archive->m_ArchiveIndexSize);
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
        const uint8_t* end = hashes + count * hash_length;
        const uint8_t* insert = LowerBound(hashes, (size_t)count, hash_digest, hash_length);
        if (insert == end)
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
        dmLogInfo("HArchiveIndexContainer: %p  %s", archive, archive->m_ArchiveFileIndex?archive->m_ArchiveFileIndex->m_Path:"no path");

        uint8_t* hashes = 0;
        EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            if (archive->m_ArchiveFileIndex)
            {
                hashes = archive->m_ArchiveFileIndex->m_Hashes;
                entries = archive->m_ArchiveFileIndex->m_Entries;
            }
        }
        else
        {
            if (archive->m_ArchiveIndex)
            {
                uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
                uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
                hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
                entries = (EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
            }
        }

        uint32_t hash_len = 0;
        uint32_t entry_count = 0;
        if (archive->m_ArchiveIndex)
        {
            hash_len = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashLength);
            entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
        }

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            const EntryData* entry = &entries[i];
            const uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);

            char hash_buffer[dmResourceArchive::MAX_HASH*2+1];
            dmResource::BytesToHexString(h, hash_len, hash_buffer, sizeof(hash_buffer));
            uint32_t flags = dmEndian::ToNetwork(entry->m_Flags);

            uint32_t compressed_size = dmEndian::ToNetwork(entry->m_ResourceCompressedSize);
            bool compressed = compressed_size != 0xFFFFFFFF;
            bool encrypted = flags & ENTRY_FLAG_ENCRYPTED;
            bool liveupdate = flags & ENTRY_FLAG_LIVEUPDATE_DATA;

            dmLogInfo("Entry: %3d: '%s'  csz: %6u sz: %8u  offs: %8u  encr: %d lz4: %d lu: %d", i, hash_buffer,
                                    compressed ? compressed_size : 0,
                                    dmEndian::ToNetwork(entry->m_ResourceSize),
                                    dmEndian::ToNetwork(entry->m_ResourceDataOffset),
                                    encrypted, compressed, liveupdate);
        }

        if (archive->m_Next)
        {
            dmLogInfo("\n->m_Next\n");
            DebugArchiveIndex(archive->m_Next);
        }
    }

    int VerifyArchiveIndex(HArchiveIndexContainer archive)
    {
        dmLogInfo("VerifyArchiveIndex: %p", archive);

        uint8_t* hashes = 0;
        EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            entries = archive->m_ArchiveFileIndex->m_Entries;
        }
        else
        {
            uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
            uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            entries = (EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
        }

        uint32_t hash_len = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashLength);
        uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);

        const uint8_t* prevh = hashes;

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            const uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);

            int cmp = memcmp(prevh, h, dmResourceArchive::MAX_HASH);
            if (cmp > 0)
            {
                char hash_buffer1[dmResourceArchive::MAX_HASH*2+1];
                dmResource::BytesToHexString(prevh, hash_len, hash_buffer1, sizeof(hash_buffer1));
                char hash_buffer2[dmResourceArchive::MAX_HASH*2+1];
                dmResource::BytesToHexString(h, hash_len, hash_buffer2, sizeof(hash_buffer2));
                dmLogError("Entry %3d: '%s' is not smaller than", i == 0 ? -1 : i-1, hash_buffer1);
                dmLogError("Entry %3d: '%s'", i, hash_buffer2);
                return cmp;
            }

            prevh = h;
        }
        return 0;
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

    Result WriteResourceToArchive(HArchiveIndexContainer& archive, const uint8_t* buf, size_t buf_len, uint32_t& bytes_written, uint32_t& offset)
    {
        ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;
        FILE* res_file = afi->m_FileResourceData;

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
                dmLogError("Failed to map liveupdate respource file, result = %i", res);
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
                dmLogError("All bytes not written for resource, bytes written: %u, resource size: %zu", bytes_written, resource->m_Count);
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

    Result NewArchiveIndexWithResource(HArchiveIndexContainer archive_container, const char* tmp_index_path, const uint8_t* hash_digest, uint32_t hash_digest_len, const dmResourceArchive::LiveUpdateResource* resource, const char* app_support_path, HArchiveIndex& out_new_index)
    {
        out_new_index = 0x0;

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
        Result insert_result = ShiftAndInsert(archive_container, ai_temp, hash_digest, hash_digest_len, idx, resource, 0x0);

        if (insert_result != RESULT_OK)
        {
            delete ai_temp;
            dmLogError("Failed to insert resource, result = %i", insert_result);
            return insert_result;
        }

        // Write to temporary index file, filename liveupdate.arci.tmp
        FILE* f_lu_index = fopen(tmp_index_path, "wb");
        if (!f_lu_index)
        {
            dmLogError("Failed to create liveupdate index file: %s", tmp_index_path);
            return RESULT_IO_ERROR;
        }
        uint32_t entry_count = dmEndian::ToNetwork(ai_temp->m_EntryDataCount);
        uint32_t total_size = sizeof(ArchiveIndex) + entry_count * dmResourceArchive::MAX_HASH + entry_count * sizeof(EntryData);
        if (fwrite((void*)ai_temp, 1, total_size, f_lu_index) != total_size)
        {
            fclose(f_lu_index);
            dmLogError("Failed to write %u bytes to liveupdate index file: %s", (uint32_t)total_size, tmp_index_path);
            return RESULT_IO_ERROR;
        }
        fflush(f_lu_index);
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

    uint32_t GetEntryCount(HArchiveIndexContainer archive)
    {
        return dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
    }

    uint32_t GetEntryDataOffset(ArchiveIndexContainer* archive_container)
    {
        return dmEndian::ToNetwork(archive_container->m_ArchiveIndex->m_EntryDataOffset);
    }

    uint32_t GetEntryDataOffset(ArchiveIndex* archive)
    {
        return dmEndian::ToNetwork(archive->m_EntryDataOffset);
    }
}  // namespace dmResourceArchive
