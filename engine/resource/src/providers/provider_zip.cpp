// Copyright 2020-2025 The Defold Foundation
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


#include "provider.h"
#include "provider_private.h"
#include "../resource_archive.h"
#include "../resource_util.h"
#include "../resource_manifest.h"
#include "../resource_manifest_private.h"
#include "../resource_private.h"

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/lz4.h>
#include <dlib/math.h>
#include <dlib/memory.h>
#include <dlib/sys.h>
#include <dlib/zip.h>

namespace dmResourceProviderZip
{

const char* LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME = "liveupdate.game.dmanifest";

const char* ANDROID_ASSET_PATH  = "/android_asset/";
const uint32_t ANDROID_ASSET_PATH_LENGTH = sizeof(ANDROID_ASSET_PATH);

struct EntryInfo
{
    dmLiveUpdateDDF::ResourceEntry* m_ManifestEntry; // If it's a resource provided by the manifest
    uint32_t                        m_Size;          // Used when there is no resource entry
    uint32_t                        m_EntryIndex;
};

struct ZipProviderContext
{
    dmURI::Parts                m_BaseUri;
    
    // handle to the zip archive
    dmZip::HZip                 m_Zip;

    // pointer to the asset associated with the mounted zip archive
    // will only be set when the archive was mapped from an asset and not from a file
    void*                       m_ZipAsset;
    // length of the mapped zip asset
    uint32_t                    m_ZipAssetLength;

    dmResource::HManifest       m_Manifest;
    dmHashTable64<EntryInfo>    m_EntryMap; // url hash -> entry in the manifest
};


static bool MatchesUri(const dmURI::Parts* uri)
{
    if (strcmp(uri->m_Scheme, "zip") == 0)
        return true;

    const char* ext = strrchr(uri->m_Path, '.');
    if (!ext)
        return false;
    return strcmp(ext, ".zip") == 0;
}

static void DeleteZipArchiveInternal(dmResourceProvider::HArchiveInternal _archive)
{
    ZipProviderContext* archive = (ZipProviderContext*)_archive;
    if (archive->m_Manifest)
        dmResource::DeleteManifest(archive->m_Manifest);
    if (archive->m_Zip)
        dmZip::Close(archive->m_Zip);
    if (archive->m_ZipAsset)
        dmResource::UnmapAsset(archive->m_ZipAsset, archive->m_ZipAssetLength);
    delete archive;
}

static void CreateEntryMap(ZipProviderContext* archive)
{
    dmZip::HZip zip = archive->m_Zip;
    uint32_t archive_entry_count = dmZip::GetNumEntries(zip);

    dmHashTable64<EntryInfo> temp_archive_map;
    temp_archive_map.SetCapacity(dmMath::Max(1U, (archive_entry_count * 2) / 3), archive_entry_count);

    // Iterate over archive once and collect all the info about files
    for (uint32_t i = 0; i < archive_entry_count; ++i)
    {
        dmZip::Result zr = dmZip::OpenEntry(zip, i);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Failed to list entry in zip file %s%s", archive->m_BaseUri.m_Location, archive->m_BaseUri.m_Path);
            continue;
        }

        const char* name = dmZip::GetEntryName(zip);

        char archive_path[dmResource::RESOURCE_PATH_MAX];
        dmSnPrintf(archive_path, sizeof(archive_path), "%s%s", name[0] != '/' ? "/" : "", name);
        dmhash_t archive_path_hash = dmHashBufferNoReverse64(archive_path, strlen(archive_path));

        EntryInfo info;
        info.m_ManifestEntry = 0;
        dmZip::GetEntrySize(zip, &info.m_Size);
        dmZip::GetEntryIndex(zip, &info.m_EntryIndex);

        dmZip::CloseEntry(zip);

        temp_archive_map.Put(archive_path_hash, info);
    }

    dmLiveUpdateDDF::HashAlgorithm algorithm = archive->m_Manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
    uint32_t hash_len = dmResource::HashLength(algorithm);

    uint32_t manifest_entry_count = archive->m_Manifest->m_DDFData->m_Resources.m_Count;

    dmHashTable64<EntryInfo>* entry_map = &archive->m_EntryMap;
    uint32_t map_size = archive_entry_count + manifest_entry_count;
    entry_map->SetCapacity(dmMath::Max(1U, (map_size*2)/3), map_size);

    // Iterate over manifest entries. Different entries may refer to the same file in archive.
    for (uint32_t i = 0; i < manifest_entry_count; ++i)
    {
        dmLiveUpdateDDF::ResourceEntry* entry = &archive->m_Manifest->m_DDFData->m_Resources.m_Data[i];
        char name_buffer[dmResourceArchive::MAX_HASH*2+1];
        dmResource::BytesToHexString(entry->m_Hash.m_Data.m_Data, hash_len, name_buffer, sizeof(name_buffer));
        char archive_path_buffer[dmResourceArchive::MAX_HASH*2+1];
        dmSnPrintf(archive_path_buffer, dmResourceArchive::MAX_HASH*2, "%s%s", name_buffer[0] != '/' ? "/" : "", name_buffer);
        archive_path_buffer[dmResourceArchive::MAX_HASH*2] = 0;
        dmhash_t archive_path_hash = dmHashBufferNoReverse64(archive_path_buffer, strlen(archive_path_buffer));
        EntryInfo* info = temp_archive_map.Get(archive_path_hash);
        if (!info)
        {
            // There is no such file in this archive
            continue;
        }
        info->m_ManifestEntry = entry;
        EntryInfo manifest_info;
        manifest_info.m_ManifestEntry = entry;
        // If we have file in manifest, get file size from there
        manifest_info.m_Size = entry->m_Size;
        manifest_info.m_EntryIndex = info->m_EntryIndex;
        entry_map->Put(entry->m_UrlHash, manifest_info);
        DM_RESOURCE_DBG_LOG(3, "Added entry: %s %llx (%u bytes)\n", archive_path_buffer, archive_path_hash, manifest_info.m_Size);
    }

    // Also add any other files that the developer might have added to the zip archive
    dmHashTable64<EntryInfo>::Iterator archive_entries_iter = temp_archive_map.GetIterator();
    while(archive_entries_iter.Next())
    {
        const EntryInfo info = archive_entries_iter.GetValue();
        if (info.m_ManifestEntry)
        {
            // This file already in the map as manifest entry
            continue;
        }
        dmhash_t hash_key = archive_entries_iter.GetKey();
        entry_map->Put(hash_key, info);
        DM_RESOURCE_DBG_LOG(3, "Added extra entry: %llx (%u bytes)\n", hash_key, info.m_Size);
    }

}

static dmResourceProvider::Result LoadManifest(dmZip::HZip zip, const char* path, dmResource::HManifest* manifest)
{
    dmZip::Result zr = dmZip::OpenEntry(zip, path);
    if (dmZip::RESULT_OK != zr)
    {
        dmLogError("Failed to find entry '%s'", path);
        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    uint32_t manifest_len;
    dmZip::GetEntrySize(zip, &manifest_len);
    uint8_t* manifest_data = new uint8_t[manifest_len];
    dmZip::GetEntryData(zip, (void*)manifest_data, manifest_len);
    dmZip::CloseEntry(zip);

    dmResourceProvider::Result result = dmResourceProvider::RESULT_OK;
    dmResource::Result r = dmResource::LoadManifestFromBuffer(manifest_data, manifest_len, manifest);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Could not read manifest '%s' from archive", path);
        result = dmResourceProvider::RESULT_INVAL_ERROR;
    }

    delete[] manifest_data;
    return result;
}

static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
{
    if (!MatchesUri(uri))
        return dmResourceProvider::RESULT_NOT_SUPPORTED;

    ZipProviderContext* archive = new ZipProviderContext;
    memset(archive, 0, sizeof(ZipProviderContext));
    memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));

    char path[1024];

    // starts with /android_asset/ ?
    if (strncmp(ANDROID_ASSET_PATH, uri->m_Path, ANDROID_ASSET_PATH_LENGTH) == 0)
    {
        dmSnPrintf(path, sizeof(path), "%s", uri->m_Path + ANDROID_ASSET_PATH_LENGTH);
        dmPath::Normalize(path, path, sizeof(path));

        void* zip_map = 0x0;
        dmResource::Result mr = dmResource::MapAsset((const char*)path, (void*&)archive->m_ZipAsset, archive->m_ZipAssetLength, zip_map);
        if (dmResource::RESULT_OK != mr)
        {
            dmLogError("Could not map asset '%s' (%d)", path, mr);
            DeleteZipArchiveInternal(archive);
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        dmZip::Result zr = dmZip::OpenStream((const char*)zip_map, archive->m_ZipAssetLength, &archive->m_Zip);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not open zip stream '%s' (%d)", path, zr);
            DeleteZipArchiveInternal(archive);
            return dmResourceProvider::RESULT_NOT_FOUND;
        }
    }
    else
    {
        dmSnPrintf(path, sizeof(path), "%s", uri->m_Path);
        dmPath::Normalize(path, path, sizeof(path));

        char mount_path[1024];

        dmSys::Result rr = dmSys::ResolveMountFileName(mount_path, sizeof(mount_path), path);
        if (dmSys::RESULT_OK != rr)
        {
            dmLogError("Could not resolve a mount path '%s' (%d)", path, rr);
            DeleteZipArchiveInternal(archive);
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        dmZip::Result zr = dmZip::Open(mount_path, &archive->m_Zip);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not open zip file '%s' (%d)", mount_path, zr);
            DeleteZipArchiveInternal(archive);
            return dmResourceProvider::RESULT_NOT_FOUND;
        }
    }

    dmResourceProvider::Result result = LoadManifest(archive->m_Zip, LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME, &archive->m_Manifest);
    if (dmResourceProvider::RESULT_OK != result)
    {
        dmLogInfo("Could not load manifest (%d)", result);
        return result;
    }

    CreateEntryMap(archive);

    *out_archive = (dmResourceProvider::HArchiveInternal)archive;
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
{
    DeleteZipArchiveInternal((ZipProviderContext*)archive);
    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
{
    ZipProviderContext* archive = (ZipProviderContext*)_archive;
    EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
    if (entry)
    {
        DM_RESOURCE_DBG_LOG(3, "ZIP: %s: File size: %s %llx -> %u\n", __FUNCTION__, path, path_hash, entry->m_Size);
        *file_size = entry->m_Size;
        return dmResourceProvider::RESULT_OK;
    }

    DM_RESOURCE_DBG_LOG(3, "ZIP: %s: Failed to find: %s %llx\n", __FUNCTION__, path, path_hash);
    return dmResourceProvider::RESULT_NOT_FOUND;
}

static dmResourceProvider::Result UnpackData(const char* path, dmLiveUpdateDDF::ResourceEntry* entry, uint8_t* raw_resource, uint32_t raw_resource_size, uint8_t* out_buffer)
{
    dmResourceArchive::LiveUpdateResource resource(raw_resource, raw_resource_size);

    uint32_t flags = entry->m_Flags;
    bool encrypted = flags & dmLiveUpdateDDF::ENCRYPTED;
    bool compressed = flags & dmLiveUpdateDDF::COMPRESSED;
    uint32_t compressed_size = compressed ? entry->m_CompressedSize : entry->m_Size;
    uint32_t resource_size = entry->m_Size;

    if (encrypted)
    {
        dmResource::Result r = dmResource::DecryptBuffer((void*)resource.m_Data, resource.m_Count);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Failed to decrypt resource: '%s", path);
            return dmResourceProvider::RESULT_IO_ERROR;
        }
    }

    if (compressed)
    {
        int decompressed_size;
        dmLZ4::Result r = dmLZ4::DecompressBuffer((const uint8_t*)resource.m_Data, compressed_size, out_buffer, resource_size, &decompressed_size);
        if (dmLZ4::RESULT_OK != r)
        {
            dmLogError("Failed to decompress resource: '%s", path);
            return dmResourceProvider::RESULT_IO_ERROR;
        }
    }
    else
    {
        memcpy(out_buffer, resource.m_Data, resource.m_Count);
    }

    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
{
    ZipProviderContext* archive = (ZipProviderContext*)_archive;
    EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
    if (!entry)
        return dmResourceProvider::RESULT_NOT_FOUND;

    if (buffer_len < entry->m_Size)
        return dmResourceProvider::RESULT_INVAL_ERROR;

    dmZip::Result zr = dmZip::OpenEntry(archive->m_Zip, entry->m_EntryIndex);
    if (dmZip::RESULT_OK != zr)
        return dmResourceProvider::RESULT_IO_ERROR;

    dmResourceProvider::Result result = dmResourceProvider::RESULT_OK;
    if (entry->m_ManifestEntry)
    {
        uint32_t raw_data_size;
        dmZip::GetEntrySize(archive->m_Zip, &raw_data_size);
        uint8_t* raw_data = new uint8_t[raw_data_size];
        dmZip::GetEntryData(archive->m_Zip, (void*)raw_data, raw_data_size);
        result = UnpackData(path, entry->m_ManifestEntry, raw_data, raw_data_size, buffer);
        delete[] raw_data;
    } else
    {
        // Uncompressed, regular files (i.e. no Liveupdate header)
        dmZip::GetEntryData(archive->m_Zip, (void*)buffer, buffer_len);
    }

    dmZip::CloseEntry(archive->m_Zip);

    return result;
}

static dmResourceProvider::Result ReadFilePartial(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
{
    ZipProviderContext* archive = (ZipProviderContext*)_archive;
    EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
    if (!entry)
        return dmResourceProvider::RESULT_NOT_FOUND;

    dmZip::Result zr = dmZip::OpenEntry(archive->m_Zip, entry->m_EntryIndex);
    if (dmZip::RESULT_OK != zr)
        return dmResourceProvider::RESULT_IO_ERROR;

    zr = dmZip::GetEntryDataOffset(archive->m_Zip, offset, size, (void*)buffer, nread);
    dmZip::CloseEntry(archive->m_Zip);

    if (dmZip::RESULT_OK != zr)
        return dmResourceProvider::RESULT_IO_ERROR;

    return dmResourceProvider::RESULT_OK;
}

static dmResourceProvider::Result GetManifest(dmResourceProvider::HArchiveInternal _archive, dmResource::HManifest* out_manifest)
{
    ZipProviderContext* archive = (ZipProviderContext*)_archive;
    if (archive->m_Manifest)
    {
        *out_manifest = archive->m_Manifest;
        return dmResourceProvider::RESULT_OK;
    }
    return dmResourceProvider::RESULT_NOT_FOUND;
}

static void SetupArchiveLoaderHttpZip(dmResourceProvider::ArchiveLoader* loader)
{
    loader->m_CanMount          = MatchesUri;
    loader->m_Mount             = Mount;
    loader->m_Unmount           = Unmount;
    loader->m_GetManifest       = GetManifest;
    loader->m_GetFileSize       = GetFileSize;
    loader->m_ReadFile          = ReadFile;
    loader->m_ReadFilePartial   = ReadFilePartial;
}

DM_DECLARE_ARCHIVE_LOADER(ResourceProviderZip, "zip", SetupArchiveLoaderHttpZip, 0, 0);
}
