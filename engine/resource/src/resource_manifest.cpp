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

#include "resource.h"
#include "resource_manifest.h"
#include "resource_util.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/path.h> // DMPATH_MAX_PATH
#include <dlib/sys.h>
#include <dlib/uri.h>
#include <resource/liveupdate_ddf.h>


namespace dmResource
{

const char* GetProjectId(dmResource::Manifest* manifest, char* buffer, uint32_t buffer_size)
{
    uint32_t hash_len = HashLength(dmLiveUpdateDDF::HASH_SHA1);
    if (buffer_size < (hash_len*2+1) )
        return 0;
    dmResource::BytesToHexString(manifest->m_DDFData->m_Header.m_ProjectIdentifier.m_Data.m_Data, hash_len, buffer, buffer_size);
    return buffer;
}

const char* GetManifestPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size)
{
    dmSnPrintf(buffer, buffer_size, "%s%s.dmanifest", uri->m_Location, uri->m_Path);
    return buffer;
}

uint32_t GetEntryHashLength(dmResource::Manifest* manifest)
{
    dmLiveUpdateDDF::HashAlgorithm algorithm = manifest->m_DDFData->m_Header.m_ResourceHashAlgorithm;
    return dmResource::HashLength(algorithm);
}

void DeleteManifest(dmResource::Manifest* manifest)
{
    if (!manifest)
        return;
    if (manifest->m_DDF)
        dmDDF::FreeMessage(manifest->m_DDF);
    if (manifest->m_DDFData)
        dmDDF::FreeMessage(manifest->m_DDFData);
    delete manifest;
}

static dmResource::Result ManifestLoadMessage(const uint8_t* manifest_msg_buf, uint32_t size, dmResource::Manifest*& out_manifest)
{
    // Read from manifest resource
    dmDDF::Result result = dmDDF::LoadMessage(manifest_msg_buf, size, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, (void**) &out_manifest->m_DDF);
    if (result != dmDDF::RESULT_OK)
    {
        dmLogError("Failed to parse Manifest (%i)", result);
        return dmResource::RESULT_DDF_ERROR;
    }

    if (out_manifest->m_DDF->m_Version != MANIFEST_VERSION)
    {
        dmLogError("Manifest file version mismatch (expected '%i', actual '%i')", MANIFEST_VERSION, out_manifest->m_DDF->m_Version);
        dmDDF::FreeMessage(out_manifest->m_DDF);
        out_manifest->m_DDF = 0x0;
        return dmResource::RESULT_VERSION_MISMATCH;
    }

    // Read data blob from ManifestFile into ManifestData message
    result = dmDDF::LoadMessage(out_manifest->m_DDF->m_Data.m_Data, out_manifest->m_DDF->m_Data.m_Count, dmLiveUpdateDDF::ManifestData::m_DDFDescriptor, (void**) &out_manifest->m_DDFData);
    if (result != dmDDF::RESULT_OK)
    {
        dmLogError("Failed to parse Manifest data (%i)", result);
        dmDDF::FreeMessage(out_manifest->m_DDF);
        out_manifest->m_DDF = 0x0;
        return dmResource::RESULT_DDF_ERROR;
    }

    return dmResource::RESULT_OK;
}

dmResource::Result LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out)
{
    dmResource::Manifest* manifest = new dmResource::Manifest();
    dmResource::Result result = ManifestLoadMessage(buffer, buffer_len, manifest);
    if (dmResource::RESULT_OK == result)
    {
        *out = manifest;
    }
    else
    {
        DeleteManifest(manifest);
    }
    return result;
}

dmResource::Result LoadManifest(const char* path, dmResource::Manifest** out)
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
        if (sys_result == dmSys::RESULT_NOENT)
        {
            dmLogError("LoadManifest: No such file %s (%i)", path, sys_result);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        dmLogError("LoadManifest: Failed to read manifest %s (%i)", path, sys_result);
        dmMemory::AlignedFree(manifest_buffer);
        return dmResource::RESULT_INVALID_DATA;
    }

    dmResource::Result result = LoadManifestFromBuffer(manifest_buffer, manifest_length, out);
    dmMemory::AlignedFree(manifest_buffer);
    return result;
}

dmResource::Result LoadManifest(const dmURI::Parts* uri, dmResource::Manifest** out)
{
    char manifest_path[DMPATH_MAX_PATH];
    return LoadManifest(GetManifestPath(uri, manifest_path, sizeof(manifest_path)), out);
}

dmResource::Result WriteManifest(const char* path, dmResource::Manifest* manifest)
{
    char manifest_tmp_file_path[DMPATH_MAX_PATH];
    dmSnPrintf(manifest_tmp_file_path, sizeof(manifest_tmp_file_path), "%s.tmp", path);

    // write to tempfile, if successful move/rename and then delete tmpfile
    dmDDF::Result ddf_result = dmDDF::SaveMessageToFile(manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, manifest_tmp_file_path);
    if (ddf_result != dmDDF::RESULT_OK)
    {
        dmLogError("Failed storing manifest to file '%s', result: %i", manifest_tmp_file_path, ddf_result);
        return dmResource::RESULT_IO_ERROR;
    }
    dmSys::Result sys_result = dmSys::Rename(path, manifest_tmp_file_path);
    if (sys_result != dmSys::RESULT_OK)
    {
        return dmResource::RESULT_IO_ERROR;
    }
    dmLogInfo("Stored manifest: '%s'", path);
    return dmResource::RESULT_OK;
}

dmLiveUpdateDDF::ResourceEntry* FindEntry(dmResource::Manifest* manifest, dmhash_t url_hash)
{
    dmLiveUpdateDDF::ResourceEntry* entries = manifest->m_DDFData->m_Resources.m_Data;

    int first = 0;
    int last = manifest->m_DDFData->m_Resources.m_Count - 1;
    while (first <= last)
    {
        int mid = first + (last - first) / 2;
        dmhash_t currentHash = entries[mid].m_UrlHash;
        if (currentHash == url_hash)
        {
            return &entries[mid];
        }
        else if (currentHash > url_hash)
        {
            last = mid - 1;
        }
        else if (currentHash < url_hash)
        {
            first = mid + 1;
        }
    }
    return 0;
}

dmResource::Result GetDependencies(dmResource::Manifest* manifest, const dmhash_t url_hash, dmArray<dmhash_t>& dependencies)
{
    dmLiveUpdateDDF::ResourceEntry* entry = FindEntry(manifest, url_hash);
    if (!entry)
        return dmResource::RESULT_RESOURCE_NOT_FOUND;

    uint32_t count = entry->m_Dependants.m_Count;
    if (dependencies.Capacity() < count)
        dependencies.SetCapacity(count);

    dependencies.PushArray(entry->m_Dependants.m_Data, count);
    return dmResource::RESULT_OK;
}

void DebugPrintManifest(dmResource::Manifest* manifest)
{
    for (uint32_t i = 0; i < manifest->m_DDFData->m_Resources.m_Count; ++i)
    {
        dmLiveUpdateDDF::ResourceEntry* entry = &manifest->m_DDFData->m_Resources.m_Data[i];
        printf("entry: hash: ");
        uint8_t* h = entry->m_Hash.m_Data.m_Data;
        dmResource::PrintHash(h, entry->m_Hash.m_Data.m_Count);
        printf("  b/l/e/c: %u%u%u%u url: %llx  %s  sz: %u  csz: %u\n",
                (entry->m_Flags & dmLiveUpdateDDF::BUNDLED) != 0,
                (entry->m_Flags & dmLiveUpdateDDF::EXCLUDED) != 0,
                (entry->m_Flags & dmLiveUpdateDDF::ENCRYPTED) != 0,
                (entry->m_Flags & dmLiveUpdateDDF::COMPRESSED) != 0,
                entry->m_UrlHash, entry->m_Url, entry->m_Size, entry->m_CompressedSize);
    }
}

} // namespace
