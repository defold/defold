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

#include "liveupdate_verify.h"
#include "liveupdate_private.h"
#include <resource/resource_manifest.h>
#include <resource/resource_verify.h>

#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/zip.h>

namespace dmLiveUpdate
{
    const char* LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME = "liveupdate.game.dmanifest";

    static uint8_t* GetZipResource(dmZip::HZip zip, const char* path, uint32_t* size)
    {
        uint32_t data_len = 0;
        dmZip::Result zr = dmZip::OpenEntry(zip, path);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not find entry name '%s'", path);
            return 0;
        }

        zr = dmZip::GetEntrySize(zip, &data_len);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not get entry size for '%s'", path);
            dmZip::CloseEntry(zip);
            return 0;
        }

        uint8_t* data = 0;
        dmMemory::AlignedMalloc((void**)&data, 16, data_len); // for loading aligned structs later on
        zr = dmZip::GetEntryData(zip, data, data_len);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not read entry '%s'", path);
            dmZip::CloseEntry(zip);
            dmMemory::AlignedFree(data);
            return 0;
        }
        dmZip::CloseEntry(zip);

        *size = data_len;
        return data;
    }

    static dmResource::Result VerifyZipEntries(dmResource::Manifest* manifest, dmZip::HZip zip)
    {
        dmResource::Result result = dmResource::RESULT_OK;
        uint32_t num_entries = dmZip::GetNumEntries(zip);
        uint32_t entry_data_capacity = 0;
        uint8_t* entry_data = 0;
        for( uint32_t i = 0; i < num_entries && dmResource::RESULT_OK == result; ++i)
        {
            dmZip::Result zr = dmZip::OpenEntry(zip, i);

            const char* entry_name = dmZip::GetEntryName(zip);
            if (!dmZip::IsEntryDir(zip) && !(strcmp(LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME, entry_name) == 0))
            {
                // verify resource
                uint32_t entry_size;
                zr = dmZip::GetEntrySize(zip, &entry_size);
                if (dmZip::RESULT_OK != zr)
                {
                    dmLogError("Could not get entry size '%s'", entry_name);
                    dmZip::Close(zip);
                    free((void*)entry_data);
                    return dmResource::RESULT_INVALID_DATA;
                }

                if (entry_data_capacity < entry_size)
                {
                    entry_data = (uint8_t*)realloc(entry_data, entry_size);
                    entry_data_capacity = entry_size;
                }

                zr = dmZip::GetEntryData(zip, entry_data, entry_size);

                dmResourceArchive::LiveUpdateResource resource(entry_data, entry_size);
                if (entry_size >= sizeof(dmResourceArchive::LiveUpdateResourceHeader))
                {
                    // NOTE: The entry "name" is the actual checksum of the contents of that file. It is not a url.
                    // NOTE: We probably need to handle custom files existing in the .zip file that _aren't_ part of the manifest
                    result = dmResource::VerifyResource(manifest, (const uint8_t*)entry_name, strlen(entry_name), resource.m_Data, resource.m_Count);

                    if (dmResource::RESULT_OK != result)
                    {
                        dmLogError("Failed to verify resource '%s' in archive", entry_name);
                    }
                }
                else {
                    dmLogError("Skipping resource %s from archive", entry_name);
                }
            }

            dmZip::CloseEntry(zip);
        }
        free((void*)entry_data);
        return dmResource::RESULT_OK;
    }

    dmResource::Result VerifyZipArchive(const char* path, const char* public_key_path)
    {
        dmLogInfo("Verifying archive '%s'", path);

        dmZip::HZip zip;
        dmZip::Result zr = dmZip::Open(path, &zip);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not open zip file '%s'", path);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        uint32_t manifest_len = 0;
        uint8_t* manifest_data = GetZipResource(zip, LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME, &manifest_len);
        if (!manifest_data)
        {
            dmLogError("Could not find or read entry name '%s' from archive", LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME);
            dmZip::Close(zip);
            return dmResource::RESULT_IO_ERROR;
        }

        dmResource::Manifest* manifest = 0;
        dmResource::Result result = dmResource::LoadManifestFromBuffer(manifest_data, manifest_len, &manifest);
        dmMemory::AlignedFree(manifest_data);
        if (dmResource::RESULT_OK != result)
        {
            dmLogError("Failed to load manifest from buffer");
            return dmResource::RESULT_IO_ERROR;
        }

        result = dmResource::VerifyManifest(manifest, public_key_path);
        if (dmResource::RESULT_OK != result)
        {
            dmLogError("Manifest verification failed: %s", dmResource::ResultToString(result));
            goto cleanup;
        }

        // TODO: What to do here. It is now ok for a liveupdate manifest/archive to not contain all the resources
        // result = dmLiveUpdate::VerifyManifestReferences(manifest);
        // if (RESULT_OK != result)
        // {
        //     dmLogError("Manifest references non existing resources: %s", dmResource::ResultToString(result));
        //     goto cleanup;
        // }

        // TODO: What to do here. It is now ok for a liveupdate manifest/archive to not contain all the resources
        //      * We can require the manifest to only contain entries for the files in the archive
        result = VerifyZipEntries(manifest, zip);
        if (dmResource::RESULT_OK != result)
        {
            dmLogError("Manifest references non existing resources");
            goto cleanup;
        }

cleanup:

        if (manifest)
        {
            dmResource::DeleteManifest(manifest);
        }

        dmZip::Close(zip);

        dmLogInfo("Archive load and verify: %s", dmResource::ResultToString(result));

        return result;
    }

} // namespace
