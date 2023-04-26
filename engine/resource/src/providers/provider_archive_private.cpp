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

#include "provider_private.h"
#include "../resource.h"

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/sys.h>

namespace dmResourceProviderArchivePrivate
{
    const char* GetManifestPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size)
    {
        dmSnPrintf(buffer, buffer_size, "%s%s.dmanifest", uri->m_Location, uri->m_Path);
        return buffer;
    }

    const char* GetArchiveIndexPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size)
    {
        dmSnPrintf(buffer, buffer_size, "%s%s.arci", uri->m_Location, uri->m_Path);
        return buffer;
    }

    const char* GetArchiveDataPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size)
    {
        dmSnPrintf(buffer, buffer_size, "%s%s.arcd", uri->m_Location, uri->m_Path);
        return buffer;
    }

    dmResourceProvider::Result LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out)
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

    dmResourceProvider::Result LoadManifest(const dmURI::Parts* uri, dmResource::Manifest** out)
    {
        char manifest_path[DMPATH_MAX_PATH];
        return LoadManifest(GetManifestPath(uri, manifest_path, sizeof(manifest_path)), out);
    }

    dmResourceProvider::Result WriteManifest(const char* path, dmResource::Manifest* manifest)
    {
        char manifest_tmp_file_path[DMPATH_MAX_PATH];
        dmSnPrintf(manifest_tmp_file_path, sizeof(manifest_tmp_file_path), "%s.tmp", path);

        // write to tempfile, if successful move/rename and then delete tmpfile
        dmDDF::Result ddf_result = dmDDF::SaveMessageToFile(manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, manifest_tmp_file_path);
        if (ddf_result != dmDDF::RESULT_OK)
        {
            dmLogError("Failed storing manifest to file '%s', result: %i", manifest_tmp_file_path, ddf_result);
            return dmResourceProvider::RESULT_IO_ERROR;
        }
        dmSys::Result sys_result = dmSys::RenameFile(path, manifest_tmp_file_path);
        if (sys_result != dmSys::RESULT_OK)
        {
            return dmResourceProvider::RESULT_IO_ERROR;
        }
        dmLogInfo("Stored manifest: '%s'", path);
        return dmResourceProvider::RESULT_OK;
    }

    static void PrintHash(const uint8_t* hash, uint32_t len)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            printf("%02X", hash[i]);
        }
    }

    void DebugPrintBuffer(const uint8_t* hash, uint32_t len)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            printf("%02X", hash[i]);
        }
    }

    void DebugPrintArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive)
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
    void DebugPrintManifest(dmResource::Manifest* manifest)
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
}
