#include "provider.h"
#include "provider_private.h"
#include "../resource_archive.h"
#include "../resource_util.h"
#include "../resource_manifest.h"

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/lz4.h>
#include <dlib/math.h>
#include <dlib/zip.h>
#include <dlib/memory.h>

namespace dmResourceProviderZip
{

const char* LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME = "liveupdate.game.dmanifest";

struct EntryInfo
{
    dmLiveUpdateDDF::ResourceEntry* m_ManifestEntry; // If it's a resource provided by the manifest
    uint32_t                        m_Size;          // Used when there is no resource entry
    uint32_t                        m_EntryIndex;
};

struct ZipProviderContext
{
    dmURI::Parts                m_BaseUri;
    dmZip::HZip                 m_Zip;
    dmResource::Manifest*       m_Manifest;
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
        dmResourceManifest::DeleteManifest(archive->m_Manifest);
    if (archive->m_Zip)
        dmZip::Close(archive->m_Zip);
    delete archive;
}

static void CreateEntryMap(ZipProviderContext* archive)
{
    uint32_t count = archive->m_Manifest->m_DDFData->m_Resources.m_Count;
    archive->m_EntryMap.SetCapacity(dmMath::Max(1U, (count*2)/3), count);

    dmLiveUpdateDDF::HashAlgorithm algorithm = archive->m_Manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
    uint32_t hash_len = dmResource::HashLength(algorithm);

    dmZip::HZip zip = archive->m_Zip;
    for (uint32_t i = 0; i < count; ++i)
    {
        dmLiveUpdateDDF::ResourceEntry* entry = &archive->m_Manifest->m_DDFData->m_Resources.m_Data[i];

        char hash_buffer[dmResourceArchive::MAX_HASH*2+1];
        dmResource::BytesToHexString(entry->m_Hash.m_Data.m_Data, hash_len, hash_buffer, sizeof(hash_buffer));
        hash_buffer[dmResourceArchive::MAX_HASH*2] = 0;

        dmZip::Result zr = dmZip::OpenEntry(archive->m_Zip, hash_buffer);
        if (dmZip::RESULT_OK != zr)
        {
            continue;
        }

        EntryInfo info;
        info.m_ManifestEntry = entry;
        info.m_Size = entry->m_Size;
        dmZip::GetEntryIndex(zip, &info.m_EntryIndex);

        // For live update resources, we account for the header size
        dmZip::CloseEntry(archive->m_Zip);

dmLogWarning("Added '%s' for '%s' to file list. %u -> %u", entry->m_Url, hash_buffer, entry->m_Size, info.m_Size);

        archive->m_EntryMap.Put(entry->m_UrlHash, info);
    }

    // Also add any other files that the developer might have added to the zip archive
    uint32_t entry_count = dmZip::GetNumEntries(zip);
    for (uint32_t i = 0; i < entry_count; ++i)
    {
        dmZip::Result zr = dmZip::OpenEntry(zip, i);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Failed to list entry in zip file %s%s", archive->m_BaseUri.m_Location, archive->m_BaseUri.m_Path);
            continue;
        }

        const char* name  = dmZip::GetEntryName(zip);
        dmhash_t name_hash = dmHashString64(name);

dmLogWarning("Added extra '%s' to file list", name?name:"null");

        EntryInfo info;
        info.m_ManifestEntry = 0;
        dmZip::GetEntrySize(zip, &info.m_Size);
        dmZip::GetEntryIndex(zip, &info.m_EntryIndex);
        dmZip::CloseEntry(zip);

        archive->m_EntryMap.Put(name_hash, info);
    }
}

static dmResourceProvider::Result LoadManifest(dmZip::HZip zip, const char* path, dmResource::Manifest** manifest)
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
    dmResource::Result r = dmResourceManifest::LoadManifestFromBuffer(manifest_data, manifest_len, manifest);
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
    dmSnPrintf(path, sizeof(path), "%s", uri->m_Path);
    dmPath::Normalize(path, path, sizeof(path));

    dmZip::Result zr = dmZip::Open(path, &archive->m_Zip);
    if (dmZip::RESULT_OK != zr)
    {
        dmLogError("Could not open zip file '%s'", path);
        DeleteZipArchiveInternal(archive);
        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    LoadManifest(archive->m_Zip, LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME, &archive->m_Manifest);

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
        *file_size = entry->m_Size;
        return dmResourceProvider::RESULT_OK;
    }

    return dmResourceProvider::RESULT_NOT_FOUND;
}

static dmResourceProvider::Result UnpackData(const char* path, dmLiveUpdateDDF::ResourceEntry* entry, const uint8_t* raw_resource, uint32_t raw_resource_size, uint8_t* out_buffer)
{
    dmResourceArchive::LiveUpdateResource resource(raw_resource, raw_resource_size);

    uint32_t flags = entry->m_Flags;
    bool encrypted = flags & dmLiveUpdateDDF::ENCRYPTED;
    bool compressed = flags & dmLiveUpdateDDF::COMPRESSED;
    uint32_t compressed_size = compressed ? entry->m_CompressedSize : entry->m_Size;
    uint32_t resource_size = entry->m_Size;

    if (encrypted)
    {
        dmResource::Result r = dmResource::DecryptBuffer((uint8_t*)resource.m_Data, resource.m_Count);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Failed to decrypt resource: '%s", path);
            return dmResourceProvider::RESULT_IO_ERROR;
        }
    }

    if (compressed)
    {
        int decompressed_size;
        dmLZ4::Result r = dmLZ4::DecompressBuffer((const uint8_t*)resource.m_Data, compressed_size, (uint8_t*)out_buffer, resource_size, &decompressed_size);
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
        // Uncompressed, regular files
        dmZip::GetEntryData(archive->m_Zip, (void*)buffer, buffer_len);
    }

    dmZip::CloseEntry(archive->m_Zip);

    return result;
}

static void SetupArchiveLoaderHttpZip(dmResourceProvider::ArchiveLoader* loader)
{
    loader->m_CanMount      = MatchesUri;
    loader->m_Mount         = Mount;
    loader->m_Unmount       = Unmount;
    loader->m_GetFileSize   = GetFileSize;
    loader->m_ReadFile      = ReadFile;
}

DM_DECLARE_ARCHIVE_LOADER(ResourceProviderZip, "zip", SetupArchiveLoaderHttpZip);
}
