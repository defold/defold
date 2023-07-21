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
#include "provider_archive_private.h"

#include "../resource_util.h"
#include "../resource_private.h" // MountArchiveInternal
#include "../resource_manifest.h"
#include "../resource_manifest_private.h"
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
        dmURI::Parts                                m_BaseUri;
        dmResource::HManifest                       m_Manifest;
        dmResourceArchive::HArchiveIndexContainer   m_ArchiveIndex;
        dmHashTable64<EntryInfo>                    m_EntryMap; // url hash -> entry in the manifest

        GameArchiveFile()
        : m_Manifest(0)
        , m_ArchiveIndex(0)
        {}
    };

    static dmResourceProvider::Result MountArchive(const dmURI::Parts* uri, dmResourceArchive::HArchiveIndexContainer* out)
    {
        char archive_index_path[DMPATH_MAX_PATH];
        char archive_data_path[DMPATH_MAX_PATH];
        dmSnPrintf(archive_index_path, sizeof(archive_index_path), "%s%s.arci", uri->m_Location, uri->m_Path);
        dmSnPrintf(archive_data_path, sizeof(archive_data_path), "%s%s.arcd", uri->m_Location, uri->m_Path);

        void* mount_info = 0;
        dmResource::Result result = dmResource::MountArchiveInternal(archive_index_path, archive_data_path, out, &mount_info);
        if (dmResource::RESULT_OK == result && *out != 0)
        {
            (*out)->m_UserData = mount_info;
            return dmResourceProvider::RESULT_OK;
        }
        dmLogError("Failed to mount archive from '%s' and '%s': %s", archive_index_path, archive_data_path, dmResource::ResultToString(result));
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    static bool MatchesUri(const dmURI::Parts* uri)
    {
        return strcmp(uri->m_Scheme, "dmanif") == 0 ||
               strcmp(uri->m_Scheme, "archive") == 0;
    }

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
            dmResourceArchive::Result result = dmResourceArchive::FindEntry(archive->m_ArchiveIndex, entry->m_Hash.m_Data.m_Data, hash_len, &info.m_ArchiveInfo);
            if (result != dmResourceArchive::RESULT_OK)
            {
                continue;
            }
            archive->m_EntryMap.Put(entry->m_UrlHash, info);
        }
    }

    static dmResourceProvider::Result LoadArchive(const dmURI::Parts* uri, dmResourceProvider::HArchiveInternal* out_archive)
    {
        GameArchiveFile* archive = new GameArchiveFile;
        {
            memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));
            // Remove the suffix, as we'll reuse this later
            char* dot = strrchr(archive->m_BaseUri.m_Path, '.');
            if (dot != 0 && strcmp(dot, ".dmanifest") == 0)
                *dot = 0;
        }

        dmResource::Result m_result = dmResource::LoadManifest(&archive->m_BaseUri, &archive->m_Manifest);
        if (dmResource::RESULT_OK != m_result)
        {
            DeleteArchive(archive);
            return dmResourceProvider::RESULT_INVAL_ERROR;
        }

    // printf("Manifest:\n");
    // dmResource::DebugPrintManifest(archive->m_Manifest);

        dmResourceProvider::Result result = MountArchive(&archive->m_BaseUri, &archive->m_ArchiveIndex);
        if (dmResourceProvider::RESULT_OK != result)
        {
            DeleteArchive(archive);
            return result;
        }

    // printf("Archive:\n");
    // dmResourceProviderArchivePrivate::DebugPrintArchiveIndex(archive->m_ArchiveIndex);

        CreateEntryMap(archive);

        archive->m_Manifest->m_ArchiveIndex = archive->m_ArchiveIndex;

        *out_archive = (dmResourceProvider::HArchive)archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
    {
        if (!MatchesUri(uri))
            return dmResourceProvider::RESULT_NOT_SUPPORTED;
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
        GameArchiveFile* archive = new GameArchiveFile;
        memset(&archive->m_BaseUri, 0, sizeof(dmURI::Parts));

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

        archive->m_ArchiveIndex = archive->m_Manifest->m_ArchiveIndex;

        CreateEntryMap(archive);

        *out_archive = archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal internal, dmhash_t path_hash, const char* path, uint32_t* file_size)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;
        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (entry)
        {
            *file_size = dmEndian::ToNetwork(entry->m_ArchiveInfo->m_ResourceSize);
            return dmResourceProvider::RESULT_OK;
        }

        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal internal, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
    {
        GameArchiveFile* archive = (GameArchiveFile*)internal;
        EntryInfo* entry = archive->m_EntryMap.Get(path_hash);
        if (entry)
        {
            if (buffer_len < dmEndian::ToNetwork(entry->m_ArchiveInfo->m_ResourceSize))
                return dmResourceProvider::RESULT_INVAL_ERROR;
            dmResourceArchive::ReadEntry(archive->m_ArchiveIndex, entry->m_ArchiveInfo, buffer);
            return dmResourceProvider::RESULT_OK;
        }

        return dmResourceProvider::RESULT_NOT_FOUND;
    }

    static dmResourceProvider::Result GetManifest(dmResourceProvider::HArchiveInternal internal, dmResource::HManifest* out_manifest)
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
        loader->m_GetManifest   = GetManifest;
        loader->m_GetFileSize   = GetFileSize;
        loader->m_ReadFile      = ReadFile;
    }

    DM_DECLARE_ARCHIVE_LOADER(ResourceProviderArchive, "archive", SetupArchiveLoader);
}
