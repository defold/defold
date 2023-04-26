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
#include "resource_archive_private.h"
#include "providers/provider.h"
#include "providers/provider_private.h"
#include "providers/provider_archive.h"
#include "providers/provider_file.h"
#include "providers/provider_http.h"
#include "providers/provider_zip.h"
#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/endian.h>
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

    static Result DecryptWithXtea(void* buffer, uint32_t buffer_len);

    const static uint64_t FILE_LOADED_INDICATOR = 1337;
    const char* KEY = "aQj8CScgNP4VsfXK";

    FDecryptResource g_ResourceDecryption = DecryptWithXtea; // Currently global since we didn't use the resource factory as the context!

    struct ArchiveMount
    {
        dmResourceProvider::HArchive    m_Archive;
        int                             m_Priority;
    };

    struct ResourceArchiveContext
    {
        // The currently mounted archives, in sorted order
        dmArray<ArchiveMount>                       m_Mounts;
    } g_ResourceArchiveContext;

    HContext Create()
    {
        ResourceArchiveContext* ctx = new ResourceArchiveContext;
        ctx->m_Mounts.SetCapacity(2);
        return ctx;
    }

    void Destroy(HContext ctx)
    {
        UnmountArchives(ctx);
        delete ctx;
    }

    ArchiveIndex::ArchiveIndex()
    {
        memset(this, 0, sizeof(ArchiveIndex));
        m_EntryDataOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
        m_HashOffset = dmEndian::ToHost((uint32_t)sizeof(ArchiveIndex));
    }

    void RegisterResourceDecryption(FDecryptResource decrypt_resource)
    {
        g_ResourceDecryption = decrypt_resource;
    }

    void RegisterArchiveLoaders(HContext ctx)
    {
    }

    static Result ProviderResultToResult(dmResourceProvider::Result result)
    {
        switch(result)
        {
        case dmResourceProvider::RESULT_OK:         return RESULT_OK;
        case dmResourceProvider::RESULT_IO_ERROR:   return RESULT_IO_ERROR;
        case dmResourceProvider::RESULT_NOT_FOUND:  return RESULT_NOT_FOUND;
        default:                                    return RESULT_UNKNOWN;
        }
    }

    static dmResourceProvider::Result Mount(HContext ctx, dmResourceProvider::HArchive base_archive, const dmURI::Parts* base_uri, dmResourceProvider::HArchive* archive)
    {
        dmResourceProvider::Result result = dmResourceProvider::Mount(base_uri, base_archive, archive);
        if (result == dmResourceProvider::RESULT_OK)
        {
            LOG("Mounted archive with uri '%s:%s/%s'", base_uri->m_Scheme, base_uri->m_Location, base_uri->m_Path);
            return result;
        }
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    static void AddMountInternal(HContext ctx, const ArchiveMount& mount)
    {
        if (ctx->m_Mounts.Full())
            ctx->m_Mounts.OffsetCapacity(2);
        ctx->m_Mounts.Push(mount);
    }

    Result AddMount(HContext ctx, dmResourceProvider::HArchive archive, int priority)
    {
        ArchiveMount mount;
        mount.m_Priority = priority;
        mount.m_Archive = archive;
        AddMountInternal(ctx, mount);
        LOG("Mounted archive %p", archive);
        return RESULT_OK;
    }

    Result RemoveMount(HContext ctx, dmResourceProvider::HArchive archive)
    {
        uint32_t size = ctx->m_Mounts.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            ArchiveMount& mount = ctx->m_Mounts[i];
            if (mount.m_Archive == archive)
            {
                dmResourceProvider::Unmount(mount.m_Archive);

                // shift all remaining items left one step
                uint32_t num_to_move = size - i - 1;
                ArchiveMount* array = ctx->m_Mounts.Begin();
                memmove(array+i, array+i+1, sizeof(ArchiveMount) * num_to_move);
                ctx->m_Mounts.SetSize(size-1);
                return RESULT_OK;
            }
        }
        return RESULT_NOT_FOUND;
    }

    // TODO: Get mount info (iterator?)

    Result MountArchive(HContext ctx, const dmURI::Parts* uri, int priority)
    {
        dmResourceProvider::HArchive base_archive = 0;
        if (!ctx->m_Mounts.Empty())
        {
            base_archive = ctx->m_Mounts[0].m_Archive;
        }

        ArchiveMount mount;
        mount.m_Priority = priority;
        dmResourceProvider::Result result = Mount(ctx, base_archive, uri, &mount.m_Archive);
        if (result != dmResourceProvider::RESULT_OK)
        {
            dmLogError("Failed to mount base archive: '%s:%s/%s'", uri->m_Scheme, uri->m_Location, uri->m_Path);
            return RESULT_IO_ERROR;
        }
        assert(mount.m_Archive);
        AddMountInternal(ctx, mount);
        return RESULT_OK;
    }

    Result UnmountArchives(HContext ctx)
    {
        uint32_t size = ctx->m_Mounts.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            ArchiveMount& mount = ctx->m_Mounts[i];
            dmResourceProvider::Unmount(mount.m_Archive);
        }
        ctx->m_Mounts.SetSize(0);
        return RESULT_OK;
    }

    Result GetResourceSize(HContext ctx, dmhash_t path_hash, const char* path, uint32_t* resource_size)
    {
        uint32_t size = ctx->m_Mounts.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            ArchiveMount& mount = ctx->m_Mounts[i];
            dmResourceProvider::Result result = dmResourceProvider::GetFileSize(mount.m_Archive, path_hash, path, resource_size);
            if (result == dmResourceProvider::RESULT_OK)
                return RESULT_OK;
        }
        return RESULT_NOT_FOUND;
    }

    Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_size)
    {
        uint32_t size = ctx->m_Mounts.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            ArchiveMount& mount = ctx->m_Mounts[i];
            dmResourceProvider::Result result = dmResourceProvider::ReadFile(mount.m_Archive, path_hash, path, buffer, buffer_size);
            if (result == dmResourceProvider::RESULT_OK)
                return RESULT_OK;
        }
        return RESULT_NOT_FOUND;
    }

    Result ReadResource(HContext ctx, const char* path, dmhash_t path_hash, dmResource::LoadBufferType* buffer)
    {
        uint32_t resource_size;
        uint32_t size = ctx->m_Mounts.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            ArchiveMount& mount = ctx->m_Mounts[i];
            dmResourceProvider::Result result = dmResourceProvider::GetFileSize(mount.m_Archive, path_hash, path, &resource_size);
            if (result == dmResourceProvider::RESULT_OK)
            {
                if (buffer->Capacity() < resource_size)
                    buffer->SetCapacity(resource_size);
                buffer->SetSize(resource_size);

                result = dmResourceProvider::ReadFile(mount.m_Archive, path_hash, path, (uint8_t*)buffer->Begin(), resource_size);
                return ProviderResultToResult(result);
            }
        }
        return RESULT_NOT_FOUND;
    }

    // *********************************************************************************

    Result LoadArchives(const char* archive_name, const char* app_path, const char* app_support_path, dmResource::Manifest** out_manifest, HArchiveIndexContainer* out_archive)
    {
        dmLogError("NOT IMPLEMENTED ANYMORE! %s: %s", __FILE__, __FUNCTION__);
        return RESULT_VERSION_MISMATCH;
        // dmResource::Manifest* head_manifest = 0;
        // HArchiveIndexContainer head_archive = 0;
        // for (int i = 0; i < g_NumArchiveLoaders; ++i)
        // {
        //     ArchiveLoader* loader = &g_ArchiveLoader[i];

        //     dmResource::Manifest* manifest = 0;
        //     Result result = loader->m_LoadManifest(archive_name, app_path, app_support_path, head_manifest, &manifest);
        //     if (RESULT_NOT_FOUND == result || RESULT_VERSION_MISMATCH == result)
        //         continue;
        //     if (RESULT_OK != result) {
        //         return result;
        //     }

        //     // The loader could opt for not loading a manifest
        //     if (!manifest)
        //     {
        //         manifest = head_manifest;
        //     }

        //     HArchiveIndexContainer archive = 0;
        //     result = loader->m_Load(manifest, archive_name, app_path, app_support_path, head_archive, &archive);
        //     if (RESULT_NOT_FOUND == result || RESULT_VERSION_MISMATCH == result)
        //     {
        //         if (manifest != head_manifest)
        //             dmResource::DeleteManifest(manifest);
        //         continue;
        //     }
        //     if (RESULT_OK != result)
        //     {
        //         if (manifest != head_manifest)
        //             dmResource::DeleteManifest(manifest);
        //         return result;
        //     }

        //     if (archive)
        //     {
        //         if (manifest != head_manifest)
        //         {
        //             if (head_manifest)
        //                 dmResource::DeleteManifest(head_manifest);
        //             head_manifest = manifest;
        //         }

        //         archive->m_Loader = *loader;
        //         if (head_archive != archive)
        //         {
        //             archive->m_Next = head_archive;
        //             head_archive = archive;
        //         }
        //     } else {
        //         // The loader could opt for not loading an archive
        //         if (manifest != head_manifest)
        //             dmResource::DeleteManifest(manifest);
        //     }
        // }

        // if (!head_manifest) {
        //     dmLogError("No manifest found");
        //     return RESULT_NOT_FOUND;
        // }

        // if (!head_archive) {
        //     dmLogError("No archives found");
        //     return RESULT_NOT_FOUND;
        // }

        // *out_manifest = head_manifest;
        // *out_archive = head_archive;
        // return RESULT_OK;
    }

    Result UnloadArchives(HArchiveIndexContainer archive)
    {
        dmLogError("NOT IMPLEMENTED ANYMORE! %s: %s", __FILE__, __FUNCTION__);
        return RESULT_VERSION_MISMATCH;
        // while (archive)
        // {
        //     HArchiveIndexContainer next = archive->m_Next;
        //     Result result = archive->m_Loader.m_Unload(archive);
        //     if (RESULT_OK != result) {
        //         return result;
        //     }

        //     archive = next;
        // }
        // return RESULT_OK;
    }

    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, HArchiveIndexContainer* out_archive, EntryData* entry)
    {
        dmLogError("NOT IMPLEMENTED ANYMORE! %s: %s", __FILE__, __FUNCTION__);
        return RESULT_VERSION_MISMATCH;
        // assert(archive != 0);
        // while (archive)
        // {
        //     ArchiveLoader* loader = &archive->m_Loader;
        //     Result result = loader->m_FindEntry(archive, hash, hash_len, entry);
        //     if (RESULT_OK == result) {
        //         if (out_archive)
        //             *out_archive = archive;
        //         return result;
        //     }

        //     archive = archive->m_Next;
        // }

        // return RESULT_NOT_FOUND;
    }

    Result Read(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, EntryData* entry_data, void* buffer)
    {
        dmLogError("NOT IMPLEMENTED ANYMORE! %s: %s", __FILE__, __FUNCTION__);
        return RESULT_VERSION_MISMATCH;
        //return archive->m_Loader.m_Read(archive, hash, hash_len, entry_data, buffer);
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


    static Result DecryptWithXtea(void* buffer, uint32_t buffer_len)
    {
        dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) buffer, buffer_len, (const uint8_t*) KEY, strlen(KEY));
        if (cr != dmCrypt::RESULT_OK)
        {
            return RESULT_UNKNOWN;
        }
        return RESULT_OK;
    }

    Result DecryptBuffer(void* buffer, uint32_t buffer_len)
    {
        return g_ResourceDecryption(buffer, buffer_len);
    }

    Result DecompressBuffer(const void* compressed_buf, uint32_t compressed_size, void* buffer, uint32_t buffer_len)
    {
        assert(compressed_buf != buffer);
        int decompressed_size;
        dmLZ4::Result r = dmLZ4::DecompressBuffer(compressed_buf, compressed_size, buffer, buffer_len, &decompressed_size);
        if (dmLZ4::RESULT_OK != r)
        {
            return RESULT_OUTBUFFER_TOO_SMALL;
        }
        return RESULT_OK;
    }

    void SetDefaultReader(HArchiveIndexContainer archive)
    {
        dmLogError("NOT IMPLEMENTED ANYMORE! %s: %s", __FILE__, __FUNCTION__);
        // memset(&archive->m_Loader, 0, sizeof(archive->m_Loader));
        // archive->m_Loader.m_FindEntry = dmResourceProviderArchive::FindEntryInArchive;
        // archive->m_Loader.m_Read = dmResourceProviderArchive::ReadEntryFromArchive;
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

    dmResource::Result VerifyResourcesBundled(dmLiveUpdateDDF::ResourceEntry* entries, uint32_t num_entries, uint32_t hash_len, dmResourceArchive::HArchiveIndexContainer archive)
    {
        for(uint32_t i = 0; i < num_entries; ++i)
        {
            if (entries[i].m_Flags == dmLiveUpdateDDF::BUNDLED)
            {
                uint8_t* hash = entries[i].m_Hash.m_Data.m_Data;
                dmResourceArchive::Result res = dmResourceArchive::FindEntry(archive, hash, hash_len, 0x0, 0x0);
                if (res == dmResourceArchive::RESULT_NOT_FOUND)
                {
                    char hash_buffer[64*2+1]; // String repr. of project id SHA1 hash
                    dmResource::BytesToHexString(hash, hash_len, hash_buffer, sizeof(hash_buffer));

                    // Manifest expect the resource to be bundled, but it is not in the archive index.
                    dmLogError("Resource '%s' (%s) is expected to be in the bundle was not found.\nResource was modified between publishing the bundle and publishing the manifest?", entries[i].m_Url, hash_buffer);
                    return dmResource::RESULT_INVALID_DATA;
                }
            }
        }

        return dmResource::RESULT_OK;
    }

    dmResource::Result VerifyResourcesBundled(dmResourceArchive::HArchiveIndexContainer base_archive, const dmResource::Manifest* manifest)
    {
        uint32_t entry_count = manifest->m_DDFData->m_Resources.m_Count;
        dmLiveUpdateDDF::ResourceEntry* entries = manifest->m_DDFData->m_Resources.m_Data;

        dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t hash_len = dmResource::HashLength(algorithm);

        return VerifyResourcesBundled(entries, entry_count, hash_len, base_archive);
    }


    int VerifyArchiveIndex(HArchiveIndexContainer archive)
    {
        dmLogInfo("VerifyArchiveIndex: %p", archive);

        uint8_t* hashes = 0;
        //EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            //entries = archive->m_ArchiveFileIndex->m_Entries;
        }
        else
        {
            //uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
            uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            //entries = (EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
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

    uint32_t GetEntryDataOffset(ArchiveIndexContainer* archive_container)
    {
        return dmEndian::ToNetwork(archive_container->m_ArchiveIndex->m_EntryDataOffset);
    }

    uint32_t GetEntryDataOffset(ArchiveIndex* archive)
    {
        return dmEndian::ToNetwork(archive->m_EntryDataOffset);
    }
}  // namespace dmResourceArchive
