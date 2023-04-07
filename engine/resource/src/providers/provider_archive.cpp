#include "provider.h"
#include "provider_private.h"

#include "../resource.h" // dmResource::Manifest
#include "../resource_archive.h"

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/lz4.h>
#include <dlib/memory.h>
#include <dlib/sys.h>

namespace dmResourceProviderArchive
{
    struct EntryInfo
    {
        dmLiveUpdateDDF::ResourceEntry* m_ManifestEntry;
        dmResourceArchive::EntryData*   m_ArchiveInfo;
    };

    struct GameArchiveFile
    {
        dmResource::Manifest*                       m_Manifest;
        dmResourceArchive::HArchiveIndexContainer   m_ArchiveIndex;
        dmHashTable64<EntryInfo>                    m_EntryMap; // url hash -> entry in the manifest
    };

    static dmResourceProvider::Result LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out)
    {
        dmResource::Manifest* manifest = new dmResource::Manifest();
        dmResource::Result result = dmResource::ManifestLoadMessage(buffer, buffer_len, manifest);

        if (dmResource::RESULT_OK == result)
        {
            *out = manifest;
        }
        else
        {
            dmResource::DeleteManifest(manifest);
        }

        return dmResource::RESULT_OK == result ? dmResourceProvider::RESULT_OK : dmResourceProvider::RESULT_IO_ERROR;
    }

    dmResourceProvider::Result LoadManifest(const char* path, dmResource::Manifest** out)
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
            return dmResourceProvider::RESULT_IO_ERROR;
        }

        dmResourceProvider::Result result = LoadManifestFromBuffer(manifest_buffer, manifest_length, out);
        dmMemory::AlignedFree(manifest_buffer);

        if (result == dmResourceProvider::RESULT_OK)
            printf("Loaded manifest %s\n", path);
        return result;
    }

    static dmResourceProvider::Result LoadManifest(const dmURI::Parts* uri, dmResource::Manifest** out)
    {
        char manifest_path[DMPATH_MAX_PATH];
        dmSnPrintf(manifest_path, sizeof(manifest_path), "%s%s.dmanifest", uri->m_Location, uri->m_Path);
        return LoadManifest(manifest_path, out);
    }

    static dmResourceProvider::Result MountArchive(const dmURI::Parts* uri, dmResourceArchive::HArchiveIndexContainer* out)
    {
        char archive_index_path[DMPATH_MAX_PATH];
        char archive_data_path[DMPATH_MAX_PATH];
        dmSnPrintf(archive_index_path, sizeof(archive_index_path), "%s%s.arci", uri->m_Location, uri->m_Path);
        dmSnPrintf(archive_data_path, sizeof(archive_data_path), "%s%s.arcd", uri->m_Location, uri->m_Path);

        void* mount_info = 0;
        dmResource::Result result = dmResource::MountArchiveInternal(archive_index_path, archive_data_path, out, &mount_info);
        if (dmResource::RESULT_OK == result && mount_info != 0 && *out != 0)
        {
            (*out)->m_UserData = mount_info;
            return dmResourceProvider::RESULT_OK;
        }
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    static void PrintHash(const uint8_t* hash, uint32_t len)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            printf("%02X", hash[i]);
        }
    }

    static void DebugPrintArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive)
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

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);
            dmResourceArchive::EntryData* e = &entries[i];

            printf("entry: off: %4u  sz: %4u  csz: %4u flags: %2u hash: ", dmEndian::ToNetwork(e->m_ResourceDataOffset), dmEndian::ToNetwork(e->m_ResourceSize), dmEndian::ToNetwork(e->m_ResourceCompressedSize), dmEndian::ToNetwork(e->m_Flags));
            PrintHash(h, 20);
            printf("\n");
        }
    }

    // required string             url = 1;
    // required uint64             url_hash = 2;
    // required HashDigest         hash = 3;
    // repeated uint32             dependants = 4;
    // required uint32             flags = 5 [default = 0]; // ResourceEntryFlag
    static void DebugPrintManifest(dmResource::Manifest* manifest)
    {
        for (uint32_t i = 0; i < manifest->m_DDFData->m_Resources.m_Count; ++i)
        {
            dmLiveUpdateDDF::ResourceEntry* entry = &manifest->m_DDFData->m_Resources.m_Data[i];
            printf("entry: hash: ");
            uint8_t* h = entry->m_Hash.m_Data.m_Data;
            PrintHash(h, entry->m_Hash.m_Data.m_Count);
            printf("  flags: %u url: %llx  %s\n", entry->m_Flags, entry->m_UrlHash, entry->m_Url);
        }
    }

    static dmResourceArchive::Result FindEntryInArchiveInternal(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData** entry)
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

    // Legacy api
    dmResourceArchive::Result FindEntryInArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry)
    {
        dmResourceArchive::EntryData* e;
        dmResourceArchive::Result result = FindEntryInArchiveInternal(archive, hash, hash_len, &e);
        if (result == dmResourceArchive::RESULT_OK)
        {
            entry->m_ResourceDataOffset = dmEndian::ToNetwork(e->m_ResourceDataOffset);
            entry->m_ResourceSize = dmEndian::ToNetwork(e->m_ResourceSize);
            entry->m_ResourceCompressedSize = dmEndian::ToNetwork(e->m_ResourceCompressedSize);
            entry->m_Flags = dmEndian::ToNetwork(e->m_Flags);
        }
        return result;
    }

    static dmResourceArchive::Result ReadEntryInternal(dmResourceArchive::HArchiveIndexContainer archive,
                                                        const dmResourceArchive::EntryData* entry, void* buffer)
    {
        const uint32_t flags            = dmEndian::ToNetwork(entry->m_Flags);
        const uint32_t size             = dmEndian::ToNetwork(entry->m_ResourceSize);
        const uint32_t resource_offset  = dmEndian::ToNetwork(entry->m_ResourceDataOffset);
              uint32_t compressed_size  = dmEndian::ToNetwork(entry->m_ResourceCompressedSize);

        printf("Read entry: sz: %u  %u\n", size, compressed_size);

        bool encrypted = (flags & dmResourceArchive::ENTRY_FLAG_ENCRYPTED);
        bool compressed = compressed_size != 0xFFFFFFFF;

        const dmResourceArchive::ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;
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

        if (!resource_memmapped)
        {
            FILE* resource_file = afi->m_FileResourceData;

            assert(temp_buffer || compressed_buf == buffer);

            fseek(resource_file, resource_offset, SEEK_SET);
            if (fread(compressed_buf, 1, compressed_size, resource_file) != compressed_size)
            {
                if (temp_buffer)
                    free(compressed_buf);
                return dmResourceArchive::RESULT_IO_ERROR;
            }
        } else {
            void* r = (void*) (((uintptr_t)afi->m_ResourceData + resource_offset));

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
        // input:[LZ4 then Encrypted] -> Decrypt ->   temp:[  LZ4      ] -> Deflate -> output:[   data   ]
        //
        // We could instead have:
        // input:[Encrypted then LZ4] -> Deflate -> output:[ Encrypted ] -> Decrypt -> output:[   data   ]

        if(encrypted)
        {
            assert(temp_buffer || compressed_buf == buffer);

            dmResourceArchive::Result r = dmResourceArchive::DecryptBuffer(compressed_buf, compressed_size);
            if (r != dmResourceArchive::RESULT_OK)
            {
                if (temp_buffer)
                    free(compressed_buf);
                return r;
            }
        }

        if (compressed)
        {
            assert(compressed_buf != buffer);
            int decompressed_size;
            dmLZ4::Result r = dmLZ4::DecompressBuffer(compressed_buf, compressed_size, buffer, size, &decompressed_size);
            if (dmLZ4::RESULT_OK != r)
            {
                if (temp_buffer)
                    free(compressed_buf);
                return dmResourceArchive::RESULT_OUTBUFFER_TOO_SMALL;
            }
        } else {
            if (buffer != compressed_buf)
                memcpy(buffer, compressed_buf, size);
        }

        if (temp_buffer)
            free(compressed_buf);

        return dmResourceArchive::RESULT_OK;
    }

    // Legacy api
    dmResourceArchive::Result ReadEntryFromArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len,
                                                    const dmResourceArchive::EntryData* entry, void* buffer)
    {
        (void)hash;
        (void)hash_len;

        dmResourceArchive::EntryData e;
        // We convert back here since the ReadEntryInternal expects the file data format
        e.m_ResourceDataOffset = dmEndian::ToNetwork(entry->m_ResourceDataOffset);
        e.m_ResourceSize = dmEndian::ToNetwork(entry->m_ResourceSize);
        e.m_ResourceCompressedSize = dmEndian::ToNetwork(entry->m_ResourceCompressedSize);
        e.m_Flags = dmEndian::ToNetwork(entry->m_Flags);

        return ReadEntryInternal(archive, &e, buffer);
    }

    static bool MatchesUri(const dmURI::Parts* uri)
    {
        return strcmp(uri->m_Scheme, "dmanif") == 0;
    }


    // static Result GetApplicationSupportPath(dmResourceProvider::HArchive archive, char* buffer, uint32_t buffer_len)
    // {
    //     char id_buf[MANIFEST_PROJ_ID_LEN]; // String repr. of project id SHA1 hash
    //     BytesToHexString(manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, HashLength(dmLiveUpdateDDF::HASH_SHA1), id_buf, MANIFEST_PROJ_ID_LEN);
    //     dmSys::Result result = dmSys::GetApplicationSupportPath(id_buf, buffer, buffer_len);
    //     if (result != dmSys::RESULT_OK)
    //     {
    //         dmLogError("Failed get application support path for \"%s\", result = %i", id_buf, result);
    //         return RESULT_IO_ERROR;
    //     }
    //     return RESULT_OK;
    // }

    static void DeleteArchive(GameArchiveFile* archive)
    {
        if (archive->m_Manifest)
            dmResource::DeleteManifest(archive->m_Manifest);

        if (archive->m_ArchiveIndex)
            dmResource::UnmountArchiveInternal(archive->m_ArchiveIndex, archive->m_ArchiveIndex->m_UserData);

        delete archive;
    }

    static void CreateEntryMap(GameArchiveFile* archive)
    {
        uint32_t count = archive->m_Manifest->m_DDFData->m_Resources.m_Count;
        archive->m_EntryMap.SetCapacity(dmMath::Max(1U, (count*2)/3), count);

        dmLiveUpdateDDF::HashAlgorithm algorithm = archive->m_Manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
        uint32_t hash_len = dmResource::HashLength(algorithm);

        for (uint32_t i = 0; i < count; ++i)
        {
            dmLiveUpdateDDF::ResourceEntry* entry = &archive->m_Manifest->m_DDFData->m_Resources.m_Data[i];

            EntryInfo info;
            info.m_ManifestEntry = entry;
            dmResourceArchive::Result result = FindEntryInArchiveInternal(archive->m_ArchiveIndex, entry->m_Hash.m_Data.m_Data, hash_len, &info.m_ArchiveInfo);
            if (result != dmResourceArchive::RESULT_OK)
            {
                dmLogError("Failed to find data entry for %s in archive", entry->m_Url);
                continue;
            }
            archive->m_EntryMap.Put(entry->m_UrlHash, info);
        }
    }

    static dmResourceProvider::Result LoadArchive(const dmURI::Parts* uri, dmResourceProvider::HArchiveInternal* out_archive)
    {
        dmResourceProvider::Result result;
        GameArchiveFile* archive = new GameArchiveFile;

        result = LoadManifest(uri, &archive->m_Manifest);
        if (dmResourceProvider::RESULT_OK != result)
        {
            DeleteArchive(archive);
            return result;
        }

        printf("Manifest:\n");
        DebugPrintManifest(archive->m_Manifest);

        result = MountArchive(uri, &archive->m_ArchiveIndex);
        if (dmResourceProvider::RESULT_OK != result)
        {
            DeleteArchive(archive);
            return result;
        }

        printf("Archive:\n");
        DebugPrintArchiveIndex(archive->m_ArchiveIndex);

        CreateEntryMap(archive);

        archive->m_Manifest->m_ArchiveIndex = archive->m_ArchiveIndex;

        *out_archive = (dmResourceProvider::HArchive)archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchiveInternal* out_archive)
    {
        if (!MatchesUri(uri))
            return dmResourceProvider::RESULT_NOT_SUPPORTED;

        printf("\nMount archive: '%s:%s%s'\n", uri->m_Scheme, uri->m_Location, uri->m_Path);

        return LoadArchive(uri, out_archive);
    }

    static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
    {
        delete (GameArchiveFile*)archive;
        return dmResourceProvider::RESULT_OK;
    }

    dmResourceProvider::Result CreateArchive(uint8_t* manifest_data, uint32_t manifest_data_len,
                                             uint8_t* index_data, uint32_t index_data_len,
                                             uint8_t* archive_data, uint32_t archive_data_len,
                                             dmResourceProvider::HArchiveInternal* out_archive)
    {
        dmResourceProvider::Result result;
        GameArchiveFile* archive = new GameArchiveFile;

        result = LoadManifestFromBuffer(manifest_data, manifest_data_len, &archive->m_Manifest);
        if (dmResourceProvider::RESULT_OK != result)
        {
            dmLogError("Failed to load manifest in-memory, result: %u", result);
            DeleteArchive(archive);
            return result;
        }

        dmResourceArchive::Result ar_result = dmResourceArchive::WrapArchiveBuffer( index_data, index_data_len, true,
                                                                                    archive_data, archive_data_len, true,
                                                                                    &archive->m_Manifest->m_ArchiveIndex);
        if (dmResourceArchive::RESULT_OK != ar_result)
        {
            return dmResourceProvider::RESULT_IO_ERROR;
        }

        archive->m_ArchiveIndex = archive->m_Manifest->m_ArchiveIndex;

        CreateEntryMap(archive);

        *out_archive = archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal internal, const char* path, uint32_t* file_size)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;
// TODO: Pass in the hash directly here!
        dmhash_t path_hash = dmHashString64(path);
        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (entry)
        {
            *file_size = dmEndian::ToNetwork(entry->m_ArchiveInfo->m_ResourceSize);
            return dmResourceProvider::RESULT_OK;
        }

        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal internal, const char* path, uint8_t* buffer, uint32_t buffer_len)
    {
        (void)buffer_len;
        GameArchiveFile* archive = (GameArchiveFile*)internal;
// TODO: Pass in the hash directly here!
        printf("ReadFile: %s\n", path);

        dmhash_t path_hash = dmHashString64(path);
        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (entry)
        {
            if (buffer_len < dmEndian::ToNetwork(entry->m_ArchiveInfo->m_ResourceSize))
                return dmResourceProvider::RESULT_INVAL_ERROR;
            ReadEntryInternal(archive->m_ArchiveIndex, entry->m_ArchiveInfo, buffer);
            return dmResourceProvider::RESULT_OK;
        }

        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static dmResourceProvider::Result GetManifest(dmResourceProvider::HArchiveInternal internal, dmResource::Manifest** out_manifest)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;
        if (archive->m_Manifest)
        {
            *out_manifest = archive->m_Manifest;
            return dmResourceProvider::RESULT_OK;
        }
        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static void SetupArchiveLoader(dmResourceProvider::ArchiveLoader* loader)
    {
        loader->m_CanMount      = MatchesUri;
        loader->m_Mount         = Mount;
        loader->m_Unmount       = Unmount;
        loader->m_GetFileSize   = GetFileSize;
        loader->m_ReadFile      = ReadFile;
        loader->m_GetManifest   = GetManifest;
    }

    DM_DECLARE_ARCHIVE_LOADER(ResourceProviderArchive, "archive", SetupArchiveLoader);
}
