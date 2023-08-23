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

#include "provider.h"
#include "provider_private.h"
#include "provider_archive.h"
#include "provider_archive_private.h"

#include "../resource_manifest.h"
#include "../resource_manifest_private.h"
#include "../resource_archive.h"
#include "../resource_util.h"
#include "../resource_private.h"

#include <dlib/crypt.h>
#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/lz4.h>
#include <dlib/math.h>
#include <dlib/memory.h>
#include <dlib/sys.h>
#include <dlib/uri.h>
#include <ddf/ddf.h>

#include <string.h> // strrchr

namespace dmResourceProviderArchiveMutable
{
    struct EntryInfo
    {
        dmLiveUpdateDDF::ResourceEntry* m_ManifestEntry;
        dmResourceArchive::EntryData*   m_ArchiveInfo;
    };

    struct GameArchiveFile
    {
        dmResource::HManifest                       m_Manifest;
        dmResource::HManifest                       m_BaseManifest;
        dmResourceArchive::HArchiveIndexContainer   m_ArchiveContainer;
        dmHashTable64<EntryInfo>                    m_EntryMap; // url hash -> entry in the manifest
        dmURI::Parts                                m_Uri;

        GameArchiveFile()
        : m_Manifest(0)
        , m_BaseManifest(0)
        , m_ArchiveContainer(0)
        {
        }
    };

    static bool MatchesUri(const dmURI::Parts* uri)
    {
        return strcmp(uri->m_Scheme, "mutable") == 0 ||
               strcmp(uri->m_Scheme, "dmanif") == 0;
    }

    static dmResourceProvider::Result MountArchive(const dmURI::Parts* uri, dmResourceArchive::HArchiveIndexContainer* out)
    {
        char archive_index_path[DMPATH_MAX_PATH];
        char archive_data_path[DMPATH_MAX_PATH];
        dmResourceProviderArchivePrivate::GetArchiveIndexPath(uri, archive_index_path, sizeof(archive_index_path));
        dmResourceProviderArchivePrivate::GetArchiveDataPath(uri, archive_data_path, sizeof(archive_data_path));

        void* mount_info = 0;
        dmResource::Result result = dmResource::MountArchiveInternal(archive_index_path, archive_data_path, out, &mount_info);
        if (dmResource::RESULT_VERSION_MISMATCH == result)
            return dmResourceProvider::RESULT_SIGNATURE_MISMATCH;
        if (dmResource::RESULT_OK == result && *out != 0)
        {
            (*out)->m_UserData = mount_info;
            return dmResourceProvider::RESULT_OK;
        }
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    static void DeleteArchive(GameArchiveFile* archive)
    {
        if (archive->m_Manifest)
            dmResource::DeleteManifest(archive->m_Manifest);

        if (archive->m_ArchiveContainer)
            dmResource::UnmountArchiveInternal(archive->m_ArchiveContainer, archive->m_ArchiveContainer->m_UserData);

        delete archive;
    }

    static void UpdateEntryMap(GameArchiveFile* archive, dmhash_t url_hash, EntryInfo& info)
    {
        if (archive->m_EntryMap.Full())
        {
            uint32_t capacity = archive->m_EntryMap.Capacity() + 32;
            archive->m_EntryMap.SetCapacity((2*capacity)/3, capacity);
        }

        archive->m_EntryMap.Put(url_hash, info);


        #if defined(DM_RESOURCE_DBG_LOG_LEVEL)
        char hash_buffer[dmResourceArchive::MAX_HASH*2+1] = {0};
        if (info.m_ManifestEntry)
        {
            uint32_t hash_len = dmResource::HashLength(archive->m_Manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm);
            dmResource::BytesToHexString(info.m_ManifestEntry->m_Hash.m_Data.m_Data, hash_len, hash_buffer, sizeof(hash_buffer));
            hash_buffer[dmResourceArchive::MAX_HASH*2] = 0;
        }
        DM_RESOURCE_DBG_LOG(3, "Added entry: name: '%s' url_hash: %016llx\n", hash_buffer, url_hash);
        #endif // DM_RESOURCE_DBG_LOG_LEVEL
    }

    static void CreateEntryMap(GameArchiveFile* archive)
    {
        uint32_t count = archive->m_Manifest->m_DDFData->m_Resources.m_Count;
        archive->m_EntryMap.SetCapacity(dmMath::Max(1U, (count*2)/3), count);

        for (uint32_t i = 0; i < count; ++i)
        {
            dmLiveUpdateDDF::ResourceEntry* entry = &archive->m_Manifest->m_DDFData->m_Resources.m_Data[i];

            bool liveupdate = entry->m_Flags & dmLiveUpdateDDF::EXCLUDED;
            if (!liveupdate)
                continue;

            EntryInfo info;
            info.m_ManifestEntry = entry;
            info.m_ArchiveInfo = 0;;
            if (archive->m_ArchiveContainer)
            {
                dmResourceArchive::Result result = dmResourceArchive::FindEntry(archive->m_ArchiveContainer,
                                                                                entry->m_Hash.m_Data.m_Data, entry->m_Hash.m_Data.m_Count, &info.m_ArchiveInfo);
                if (result != dmResourceArchive::RESULT_OK)
                {
                    dmLogError("Failed to find data entry for %s in archive", entry->m_Url);
                    continue;
                }
            }

            UpdateEntryMap(archive, entry->m_UrlHash, info);
        }

    }

    static bool ArchiveFilesExist(const dmURI::Parts* uri)
    {
        char path[DMPATH_MAX_PATH];
        dmResourceProviderArchivePrivate::GetArchiveIndexPath(uri, path, sizeof(path));
        if (!dmSys::Exists(path))
            return false;
        dmResourceProviderArchivePrivate::GetArchiveDataPath(uri, path, sizeof(path));
        if (!dmSys::Exists(path))
            return false;
        return true;
    }

    static void CreateFilesIfNotExists(const dmURI::Parts* uri)
    {
        char archive_index_path[DMPATH_MAX_PATH];
        char archive_data_path[DMPATH_MAX_PATH];
        dmResourceProviderArchivePrivate::GetArchiveIndexPath(uri, archive_index_path, sizeof(archive_index_path));
        dmResourceProviderArchivePrivate::GetArchiveDataPath(uri, archive_data_path, sizeof(archive_data_path));

        if (!dmSys::Exists(archive_index_path))
        {
            FILE* f = fopen(archive_index_path, "ab+");
            if (!f) {
                dmLogError("Failed to create liveupdate resource file");
            } else {
                fclose(f);
            }
        }

        if (!dmSys::Exists(archive_data_path))
        {
            FILE* f = fopen(archive_data_path, "ab+");
            if (!f) {
                dmLogError("Failed to create liveupdate resource file");
            } else {
                fclose(f);
            }
        }
    }

    static dmResource::HManifest CreateManifestCopy(const dmResource::HManifest base_manifest)
    {
        // Create the actual copy
        dmResource::HManifest manifest = new dmResource::Manifest;
        dmDDF::CopyMessage(base_manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, (void**)&manifest->m_DDF);
        dmDDF::CopyMessage(base_manifest->m_DDFData, dmLiveUpdateDDF::ManifestData::m_DDFDescriptor, (void**)&manifest->m_DDFData);

        return manifest;
    }

    static void CreateDynamicManifestArchiveIndex(dmResource::HManifest manifest, const dmResource::HManifest base_manifest)
    {
        if (manifest->m_ArchiveIndex)
            return;

        manifest->m_ArchiveIndex = new dmResourceArchive::ArchiveIndexContainer;
        manifest->m_ArchiveIndex->m_ArchiveIndex = new dmResourceArchive::ArchiveIndex;
        manifest->m_ArchiveIndex->m_ArchiveFileIndex = new dmResourceArchive::ArchiveFileIndex;
        // Even though it isn't technically true, it will allow us to use the correct ArchiveIndex struct
        manifest->m_ArchiveIndex->m_IsMemMapped = 1;

        manifest->m_ArchiveIndex->m_ArchiveIndex->m_Version = base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_Version;
        manifest->m_ArchiveIndex->m_ArchiveIndex->m_HashLength = base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_HashLength;
        memcpy(manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5, base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5, sizeof(manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5));
    }

    static void OpenDynamicArchiveFile(dmURI::Parts* uri, dmResource::HManifest manifest)
    {
        dmResourceArchive::ArchiveFileIndex* afi = manifest->m_ArchiveIndex->m_ArchiveFileIndex;

        if (afi->m_FileResourceData)
            return;

        char path[DMPATH_MAX_PATH];
        dmResourceProviderArchivePrivate::GetArchiveDataPath(uri, path, sizeof(path));

        FILE* f = fopen(path, "ab+");
        if (!f)
        {
            dmLogError("Failed to create/load liveupdate resource file");
        }

        dmStrlCpy(afi->m_Path, path, DMPATH_MAX_PATH);
        dmLogInfo("Live Update archive: %s", afi->m_Path);

        afi->m_FileResourceData = f;
        afi->m_ResourceData = 0x0;
        afi->m_ResourceSize = 0;
        afi->m_IsMemMapped = false;
    }

    static dmResourceProvider::Result RenameTempFiles(const dmURI::Parts* uri)
    {
        char archive_index_path[DMPATH_MAX_PATH];
        char archive_index_tmp_path[DMPATH_MAX_PATH];
        dmResourceProviderArchivePrivate::GetArchiveIndexPath(uri, archive_index_path, sizeof(archive_index_path));
        dmResourceProviderArchivePrivate::GetArchiveIndexPath(uri, archive_index_tmp_path, sizeof(archive_index_tmp_path));
        dmStrlCat(archive_index_tmp_path, ".tmp", sizeof(archive_index_tmp_path));

        bool luTempIndexExists = dmSys::Exists(archive_index_tmp_path);
        if (luTempIndexExists)
        {
            dmSys::Result sys_result = dmSys::Rename(archive_index_path, archive_index_tmp_path);
            if (sys_result != dmSys::RESULT_OK)
            {
                // The recently added resources will not be available if we proceed after this point
                dmLogError("Failed to rename '%s' to '%s' (%i).", archive_index_tmp_path, archive_index_path, sys_result);
                return dmResourceProvider::RESULT_IO_ERROR;
            }

            dmLogInfo("Renamed '%s' to '%s'", archive_index_tmp_path, archive_index_path);
            if (dmSys::Exists(archive_index_tmp_path))
                dmSys::Unlink(archive_index_tmp_path);
        }

        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result LoadArchive(const dmURI::Parts* uri, dmResource::HManifest base_manifest, dmResourceProvider::HArchiveInternal* out_archive)
    {
        dmResourceProvider::Result result;

        // If we have pending temp files, then we rename them and we can start using them
        result = RenameTempFiles(uri);
        if (result != dmResourceProvider::RESULT_OK)
        {
            return result;
        }

        GameArchiveFile* archive = new GameArchiveFile;
        *out_archive = (dmResourceProvider::HArchive)archive;
        archive->m_Manifest = 0;
        memcpy(&archive->m_Uri, uri, sizeof(dmURI::Parts));

        char manifest_path[DMPATH_MAX_PATH];
        dmResource::GetManifestPath(&archive->m_Uri, manifest_path, sizeof(manifest_path));

        if (dmSys::Exists(manifest_path))
        {
            dmResource::Result mresult = dmResource::LoadManifest(uri, &archive->m_Manifest);
            if (dmResource::RESULT_OK != mresult)
            {
                dmLogError("Failed to load manifest '%s': %s", manifest_path, dmResource::ResultToString(mresult));
                dmLogError("Removing '%s'", manifest_path);
                dmSys::Unlink(manifest_path);
                archive->m_Manifest = 0;
            }
        }

        if (!archive->m_Manifest)
        {
            archive->m_Manifest = CreateManifestCopy(base_manifest);
        }

        archive->m_BaseManifest = base_manifest;
        CreateEntryMap(archive);

        if (!ArchiveFilesExist(uri))
        {
            return dmResourceProvider::RESULT_OK;
        }

        result = MountArchive(uri, &archive->m_ArchiveContainer);
        if (dmResourceProvider::RESULT_OK != result)
        {
            DeleteArchive(archive);
            return result;
        }

        uint32_t version = dmEndian::ToNetwork(archive->m_ArchiveContainer->m_ArchiveIndex->m_Version);
        if (version != dmResourceArchive::VERSION)
        {
            dmLogError("Archive version differs. Expected %d, but it was %d", dmResourceArchive::VERSION, version);
            DeleteArchive(archive);
            return dmResourceProvider::RESULT_SIGNATURE_MISMATCH;
        }

        CreateEntryMap(archive);

        dmResourceProviderArchivePrivate::DebugPrintArchiveIndex(archive->m_ArchiveContainer);

        archive->m_Manifest->m_ArchiveIndex = archive->m_ArchiveContainer;

        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result Mount(const dmURI::Parts* _uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
    {
        dmURI::Parts uri;
        memcpy(&uri, _uri, sizeof(uri));
        char* suffix = strrchr(uri.m_Path, '.');
        if (suffix != 0 && strcmp(suffix, ".tmp") == 0)
        {
            *suffix = 0;
            suffix = strrchr(uri.m_Path, '.');
        }
        if (suffix != 0 && (strcmp(suffix, ".arci") == 0 || strcmp(suffix, ".arcd") == 0))
        {
            *suffix = 0;
        }

        if (!MatchesUri(&uri))
            return dmResourceProvider::RESULT_NOT_SUPPORTED;

        dmResource::HManifest manifest = 0;
        if (dmResourceProvider::RESULT_OK != dmResourceProvider::GetManifest(base_archive, &manifest))
        {
            dmLogError("Failed to get manifest from base archive");
        }

        return LoadArchive(&uri, manifest, out_archive);
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
        GameArchiveFile* archive = new GameArchiveFile;

        dmResource::Result result = dmResource::LoadManifestFromBuffer(manifest_data, manifest_data_len, &archive->m_Manifest);
        if (dmResource::RESULT_OK != result)
        {
            dmLogError("Failed to load manifest in-memory, result: %u", result);
            DeleteArchive(archive);
            return dmResourceProvider::RESULT_INVAL_ERROR;
        }

        dmResourceArchive::Result ar_result = dmResourceArchive::WrapArchiveBuffer( index_data, index_data_len, true,
                                                                                    archive_data, archive_data_len, true,
                                                                                    &archive->m_Manifest->m_ArchiveIndex);
        if (dmResourceArchive::RESULT_OK != ar_result)
        {
            return dmResourceProvider::RESULT_IO_ERROR;
        }

        archive->m_ArchiveContainer = archive->m_Manifest->m_ArchiveIndex;

        CreateEntryMap(archive);

        *out_archive = archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal internal, dmhash_t path_hash, const char* path, uint32_t* file_size)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;
        if (!archive->m_ArchiveContainer)
            return dmResourceProvider::RESULT_NOT_FOUND;

        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (entry && entry->m_ArchiveInfo)
        {
            *file_size = dmEndian::ToNetwork(entry->m_ArchiveInfo->m_ResourceSize);
            return dmResourceProvider::RESULT_OK;
        }

        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal internal, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;
        if (!archive->m_ArchiveContainer)
            return dmResourceProvider::RESULT_NOT_FOUND;
        if (archive->m_EntryMap.Empty())
            return dmResourceProvider::RESULT_NOT_FOUND;

        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (!entry)
            return dmResourceProvider::RESULT_NOT_FOUND;

        if (buffer_len < dmEndian::ToNetwork(entry->m_ArchiveInfo->m_ResourceSize))
            return dmResourceProvider::RESULT_INVAL_ERROR;

        dmResourceArchive::Result result = dmResourceArchive::ReadEntry(archive->m_ArchiveContainer, entry->m_ArchiveInfo, buffer);
        if (dmResourceArchive::RESULT_OK != result)
        {
            return dmResourceProvider::RESULT_IO_ERROR;
        }
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result VerifyResource(const dmResource::HManifest manifest, const uint8_t* expected, uint32_t expected_length, const uint8_t* data, uint32_t data_length)
    {
        if (manifest == 0x0 || data == 0x0)
        {
            return dmResourceProvider::RESULT_INVAL_ERROR;
        }
        bool comp = dmResource::MemCompare(data, data_length, expected, expected_length) == dmResource::RESULT_OK;
        return comp ? dmResourceProvider::RESULT_OK : dmResourceProvider::RESULT_SIGNATURE_MISMATCH;
    }

    static dmResourceProvider::Result NewArchiveIndexWithResource(GameArchiveFile* archive,
                                                                    const uint8_t* digest, const uint32_t digest_length, const dmResourceArchive::LiveUpdateResource* resource,
                                                                    dmResourceArchive::HArchiveIndex& out_new_index)
    {
        char index_tmp_path[DMPATH_MAX_PATH];
        dmResourceProviderArchivePrivate::GetArchiveIndexPath(&archive->m_Uri, index_tmp_path, sizeof(index_tmp_path));
        dmStrlCat(index_tmp_path, ".tmp", sizeof(index_tmp_path));

        // Calls ShiftAndInsert, which in turn calls WriteResourceToArchive which writes to the file handle currently stored in m_FileResourceData
        // Calls WriteArchiveIndex (i.e. stores the index to index_tmp_path)
        dmResourceArchive::Result res = dmResourceArchive::NewArchiveIndexWithResource(archive->m_Manifest->m_ArchiveIndex, index_tmp_path, digest, digest_length, resource, out_new_index);

        return (res == dmResourceArchive::RESULT_OK) ? dmResourceProvider::RESULT_OK : dmResourceProvider::RESULT_IO_ERROR;
    }

    static dmResourceProvider::Result WriteFile(dmResourceProvider::HArchiveInternal internal, dmhash_t path_hash, const char* path, const uint8_t* buffer, uint32_t buffer_length)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;

        // If we don't have a manifest at this point, we might need to create one
        dmLiveUpdateDDF::HashAlgorithm algorithm = archive->m_Manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;

        uint32_t expected_digest_length;
        const uint8_t*    expected_digest;

        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (entry)
        {
            expected_digest_length  = entry->m_ManifestEntry->m_Hash.m_Data.m_Count;
            expected_digest         = (const uint8_t*)entry->m_ManifestEntry->m_Hash.m_Data.m_Data;
        }
        else
        {
            dmLogError("Couldn't find path '%s' in manifest!", path);
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        uint32_t algorithm_length = dmResource::HashLength(algorithm);

        uint8_t hex_expected_digest[512];
        dmResource::BytesToHexString(expected_digest, expected_digest_length, (char*)hex_expected_digest, expected_digest_length*2 + 1);

        dmResourceArchive::LiveUpdateResource resource(buffer, buffer_length);

        // Hash the incoming data...
        uint8_t digest[512]; // max length
        dmResource::CreateResourceHash(algorithm, resource.m_Data, resource.m_Count, digest);

        // ...and compare it to the signature in the manifest
        dmResourceProvider::Result result = VerifyResource(archive->m_Manifest, expected_digest, expected_digest_length,
                                                                                digest, algorithm_length);
        if(dmResourceProvider::RESULT_OK != result)
        {
            dmLogError("Verification failure for Liveupdate archive for resource: %s - %d", expected_digest, result);
            return result;
        }

        dmResourceArchive::HArchiveIndex new_archive_index;

        CreateFilesIfNotExists(&archive->m_Uri);
        CreateDynamicManifestArchiveIndex(archive->m_Manifest, archive->m_BaseManifest);
        OpenDynamicArchiveFile(&archive->m_Uri, archive->m_Manifest);

        result = NewArchiveIndexWithResource(archive, digest, algorithm_length*2, &resource, new_archive_index);
        if (dmResourceProvider::RESULT_OK == result)
        {
            dmResourceArchive::SetNewArchiveIndex(archive->m_Manifest->m_ArchiveIndex, new_archive_index, true);
            archive->m_ArchiveContainer = archive->m_Manifest->m_ArchiveIndex;
        }

        if (!entry->m_ArchiveInfo)
        {
            dmResourceArchive::Result result = dmResourceArchive::FindEntry(archive->m_ArchiveContainer,
                                                                            entry->m_ManifestEntry->m_Hash.m_Data.m_Data, entry->m_ManifestEntry->m_Hash.m_Data.m_Count, &entry->m_ArchiveInfo);
            if (result != dmResourceArchive::RESULT_OK)
            {
                dmLogError("Failed to find data entry for %s in archive", entry->m_ManifestEntry->m_Url);
            }
        }

        return result;
    }

    static dmResourceProvider::Result GetManifest(dmResourceProvider::HArchiveInternal _archive, dmResource::HManifest* out_manifest)
    {
        GameArchiveFile* archive = (GameArchiveFile*)_archive;
        if (archive->m_Manifest)
        {
            *out_manifest = archive->m_Manifest;
            return dmResourceProvider::RESULT_OK;
        }
        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static dmResourceProvider::Result SetManifest(dmResourceProvider::HArchiveInternal _archive, dmResource::HManifest manifest)
    {
        GameArchiveFile* archive = (GameArchiveFile*)_archive;
        if (archive->m_Manifest)
            dmResource::DeleteManifest(archive->m_Manifest);

        char manifest_path[DMPATH_MAX_PATH];
        dmResource::GetManifestPath(&archive->m_Uri, manifest_path, sizeof(manifest_path));

        if (dmSys::Exists(manifest_path))
        {
            dmSys::Unlink(manifest_path);
        }

        archive->m_Manifest = 0;
        if (manifest)
        {
            archive->m_Manifest = CreateManifestCopy(manifest);
            dmResource::WriteManifest(manifest_path, archive->m_Manifest);
            dmLogInfo("Wrote manifest to '%s'", manifest_path);
        }
        CreateEntryMap(archive);
        return dmResourceProvider::RESULT_OK;
    }

    static void SetupArchiveLoader(dmResourceProvider::ArchiveLoader* loader)
    {
        loader->m_CanMount      = MatchesUri;
        loader->m_Mount         = Mount;
        loader->m_Unmount       = Unmount;
        loader->m_GetManifest   = GetManifest;
        loader->m_SetManifest   = SetManifest;
        loader->m_GetFileSize   = GetFileSize;
        loader->m_ReadFile      = ReadFile;
        loader->m_WriteFile     = WriteFile;
    }

    DM_DECLARE_ARCHIVE_LOADER(ResourceProviderArchiveMutable, "mutable", SetupArchiveLoader);
}

