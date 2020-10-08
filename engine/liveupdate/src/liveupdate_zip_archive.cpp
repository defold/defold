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

#include "liveupdate.h"
#include "liveupdate_private.h"

#include <dlib/dstrings.h>
#include <dlib/path.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/zip.h>

namespace dmLiveUpdate
{
    static Result VerifyZipArchive(const char* path, char* application_support_path, uint32_t application_support_path_len)
    {
        dmLogInfo("Verifying archive '%s'", path);

        dmZip::HZip zip;
        dmZip::Result zr = dmZip::Open(path, &zip);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not open zip file '%s'", path);
            return RESULT_INVALID_RESOURCE;
        }

        uint32_t manifest_len = 0;
        const char* manifest_entryname = "liveupdate.game.dmanifest";
        zr = dmZip::OpenEntry(zip, manifest_entryname);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not find entry name '%s'", manifest_entryname);
            return RESULT_INVALID_RESOURCE;
        }

        zr = dmZip::GetEntrySize(zip, &manifest_len);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not get entry size '%s'", manifest_entryname);
            dmZip::Close(zip);
            return RESULT_INVALID_RESOURCE;
        }

        uint8_t* manifest_data = (uint8_t*)malloc(manifest_len);
        zr = dmZip::GetEntryData(zip, manifest_data, manifest_len);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not read entry '%s'", manifest_entryname);
            dmZip::Close(zip);
            return RESULT_INVALID_RESOURCE;
        }
        dmZip::CloseEntry(zip);

        dmResource::Manifest* manifest = new dmResource::Manifest();

        // Create
        Result result = dmLiveUpdate::ParseManifestBin((uint8_t*) manifest_data, manifest_len, manifest);
        if (RESULT_OK == result)
        {
            // store the live update folder for later
            dmResource::GetApplicationSupportPath(manifest, application_support_path, application_support_path_len);

            // Verify
            result = dmLiveUpdate::VerifyManifest(manifest);
            if (RESULT_SCHEME_MISMATCH == result)
            {
                dmLogWarning("Scheme mismatch, manifest storage is only supported for bundled package. Manifest was not stored.");
            }
            else if (RESULT_OK != result)
            {
                dmLogError("Manifest verification failed. Manifest was not stored.");
            }


            // Verify the resources in the zip file
            uint32_t num_entries = dmZip::GetNumEntries(zip);
            uint8_t* entry_data = 0;
            uint32_t entry_data_capacity = 0;
            for( uint32_t i = 0; i < num_entries && RESULT_OK == result; ++i)
            {
                zr = dmZip::OpenEntry(zip, i);

                const char* entry_name = dmZip::GetEntryName(zip);
                printf("  ENTRY: '%s'\n", entry_name);
                if (!dmZip::IsEntryDir(zip) && !(strcmp(manifest_entryname, entry_name) == 0))
                {
                    // verify resource
                    uint32_t entry_size;
                    zr = dmZip::GetEntrySize(zip, &entry_size);
                    if (dmZip::RESULT_OK != zr)
                    {
                        dmLogError("Could not get entry size '%s'", entry_name);
                        dmZip::Close(zip);
                        return RESULT_INVALID_RESOURCE;
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
                        result = dmLiveUpdate::VerifyResource(manifest, entry_name, strlen(entry_name), (const char*)resource.m_Data, resource.m_Count);

                        if (RESULT_OK != result)
                        {
                            dmLogError("Failed to verify resource '%s' in archive %s", entry_name, path);
                        }
                    }
                    else {
                        dmLogError("Skipping resource %s from archive %s", entry_name, path);
                    }

                }

                dmZip::CloseEntry(zip);
            }

            free(entry_data);
            dmDDF::FreeMessage(manifest->m_DDFData);
            dmDDF::FreeMessage(manifest->m_DDF);
        }
        // else { resources are already free'd here }

        free(manifest_data);
        delete manifest;

        dmZip::Close(zip);

        dmLogInfo("Archive verification %s", RESULT_OK == result ? "OK":"FAILED");

        return result;
    }

    Result StoreZipArchive(const char* path)
    {
        char application_support_path[DMPATH_MAX_PATH];

        Result result = VerifyZipArchive(path, application_support_path, sizeof(application_support_path));
        if (RESULT_OK != result)
        {
            return result;
        }

        // Move zip file to "<support path>/liveupdate.zip.tmp"

        char archive_path[DMPATH_MAX_PATH];
        dmSnPrintf(archive_path, sizeof(archive_path), "%s/%s", application_support_path, dmResource::LIVEUPDATE_ARCHIVE_TMP_FILENAME);

        dmSys::Result sys_result = dmSys::RenameFile(archive_path, path);
        if (dmSys::RESULT_OK != sys_result)
        {
            dmLogError("Failed to rename '%s' to '%s': %d", path, archive_path, sys_result);
            return RESULT_IO_ERROR;
        }

        dmLogInfo("Renamed '%s' to '%s'", path, archive_path);

        dmLogInfo("Archive Stored OK");

        return RESULT_OK;
    }
}