// Copyright 2020-2022 The Defold Foundation
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

#include <sys/stat.h>

#include "liveupdate.h"
#include "liveupdate_private.h"
#include <resource/resource.h>
#include <resource/resource_archive.h>

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/path.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/zip.h>
#include <dlib/memory.h>

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
        dmMemory::AlignedMalloc((void**)&data, 16, data_len); // for loading aligned struct later on
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
        uint8_t* manifest_data = GetZipResource(zip, LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME, &manifest_len);
        if (!manifest_data)
        {
            dmLogError("Could not read entry name '%s' from archive", LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME);
            dmZip::Close(zip);
            return RESULT_INVALID_RESOURCE;
        }

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

            result = dmLiveUpdate::VerifyManifestReferences(manifest);
            if (RESULT_OK != result)
            {
                dmLogError("Manifest references non existing resources. Manifest was not stored.");
            }

            // Verify the resources in the zip file
            if (RESULT_OK == result)
            {
                uint32_t num_entries = dmZip::GetNumEntries(zip);
                uint8_t* entry_data = 0;
                uint32_t entry_data_capacity = 0;
                for( uint32_t i = 0; i < num_entries && RESULT_OK == result; ++i)
                {
                    zr = dmZip::OpenEntry(zip, i);

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
            }
            dmDDF::FreeMessage(manifest->m_DDFData);
            dmDDF::FreeMessage(manifest->m_DDF);
        }
        // else { resources are already free'd here }

        dmMemory::AlignedFree(manifest_data);
        delete manifest;

        dmZip::Close(zip);

        dmLogInfo("Archive verification %s", RESULT_OK == result ? "OK":"FAILED");

        return result;
    }

    // Store the path to the file into the liveupdate.ref.tmp
    Result StoreZipArchive(const char* path, const bool validate_archive)
    {
        char application_support_path[DMPATH_MAX_PATH];

        if (validate_archive)
        {
            Result result = VerifyZipArchive(path, application_support_path, sizeof(application_support_path));
            if (RESULT_OK != result)
            {
                return result;
            }
        }

        // Store zip file ref in "<support path>/liveupdate.ref.tmp"
        char archive_path[DMPATH_MAX_PATH];
        dmSnPrintf(archive_path, sizeof(archive_path), "%s/%s", application_support_path, LIVEUPDATE_ARCHIVE_TMP_FILENAME);

        FILE* f = fopen(archive_path, "wb");
        if (!f) {
            dmLogError("Couldn't open %s for writing", archive_path);
            return RESULT_IO_ERROR;
        }

        size_t len = (size_t)strlen(path);
        size_t nwritten = fwrite(path, 1, len, f);
        fclose(f);

        if (nwritten != len) {
            dmLogError("Couldn't write liveupdate path reference to %s", archive_path);
            return RESULT_IO_ERROR;
        }

        dmLogInfo("Stored reference to '%s' in '%s'", path, archive_path);

        return RESULT_OK;
    }

    // rename liveupdate.ref.tmp -> liveupdate.ref
    static dmResourceArchive::Result RenameTempZip(const char* app_support_path)
    {
        char archive_tmp_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_ARCHIVE_TMP_FILENAME, archive_tmp_path, DMPATH_MAX_PATH);

        if (!FileExists(archive_tmp_path))
            return dmResourceArchive::RESULT_OK;

        char archive_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_ARCHIVE_FILENAME, archive_path, DMPATH_MAX_PATH);

        dmSys::Result sys_result = dmSys::RenameFile(archive_path, archive_tmp_path);
        if (sys_result != dmSys::RESULT_OK)
        {
            // The recently added resources will not be available if we proceed after this point
            dmLogError("Failed to rename '%s' to '%s' (%i).", archive_tmp_path, archive_path, sys_result);
            return dmResourceArchive::RESULT_IO_ERROR;
        }
        dmSys::Unlink(archive_tmp_path);

        dmLogInfo("Archive renamed from '%s' to '%s'", archive_tmp_path, archive_path);
        return dmResourceArchive::RESULT_OK;
    }

    dmResourceArchive::Result LULoadManifest_Zip(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** out)
    {
        RenameTempZip(app_support_path);

        char archive_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_ARCHIVE_FILENAME, archive_path, DMPATH_MAX_PATH);

        if (!FileExists(archive_path))
        {
            return dmResourceArchive::RESULT_NOT_FOUND;
        }

        // Get the file path to the actual zip archive
        char zip_path[DMPATH_MAX_PATH] = {0};
        FILE* f = fopen(archive_path, "rb");
        fread(zip_path, 1, sizeof(zip_path), f);
        fclose(f);

        zip_path[sizeof(zip_path)-1] = 0;

        if (!FileExists(zip_path))
        {
            dmLogError("Could not find live update archive '%s'", zip_path);
            return dmResourceArchive::RESULT_NOT_FOUND;
        }

        dmLogInfo("Found zip archive reference to %s", zip_path);

        dmZip::HZip zip;
        dmZip::Result zr = dmZip::Open(zip_path, &zip);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not open zip file '%s'", zip_path);
            return dmResourceArchive::RESULT_NOT_FOUND;
        }

        uint32_t manifest_len = 0;
        uint8_t* manifest_data = GetZipResource(zip, LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME, &manifest_len);
        dmZip::Close(zip);
        if (!manifest_data)
        {
            dmLogError("Could not read entry name '%s' from archive", LIVEUPDATE_ARCHIVE_MANIFEST_FILENAME);
            return dmResourceArchive::RESULT_NOT_FOUND;
        }

        dmResourceArchive::Result result = dmResourceArchive::LoadManifestFromBuffer(manifest_data, manifest_len, out);
        dmMemory::AlignedFree(manifest_data);
        if (dmResourceArchive::RESULT_OK != result)
        {
            dmLogError("Could not load manifest: %d", result);
            return result;
        }

        // Verify
        dmLiveUpdate::Result lu_result = dmLiveUpdate::VerifyManifest(*out);
        if (dmLiveUpdate::RESULT_OK != lu_result)
        {
            dmLogError("Manifest verification failed. Manifest was not loaded. Result: %d", lu_result);
            delete *out;
            *out = 0;
            result = dmResourceArchive::RESULT_VERSION_MISMATCH;
        }

        return result;
    }

    dmResourceArchive::Result LULoadArchive_Zip(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path,
                                                dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out)
    {
        // At this point, the base archive has been loaded, and we can verify out manifest's references
        dmResource::Result result = dmResource::VerifyResourcesBundled(previous, manifest);
        if (dmResource::RESULT_OK != result)
        {
            dmLogError("Manifest references non existing resources.");
            return dmResourceArchive::RESULT_VERSION_MISMATCH;
        }

        char archive_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_ARCHIVE_FILENAME, archive_path, DMPATH_MAX_PATH);

        // Get the file path to the actual zip archive
        char zip_path[DMPATH_MAX_PATH] = {0};
        FILE* f = fopen(archive_path, "rb");
        fread(zip_path, 1, sizeof(zip_path), f);
        fclose(f);

        zip_path[sizeof(zip_path)-1] = 0;

        dmZip::HZip zip;
        dmZip::Result zr = dmZip::Open(zip_path, &zip);
        if (dmZip::RESULT_OK != zr)
        {
            dmLogError("Could not open zip file '%s'", zip_path);
            return dmResourceArchive::RESULT_IO_ERROR;
        }

        dmResourceArchive::HArchiveIndexContainer archive = new dmResourceArchive::ArchiveIndexContainer;
        archive->m_ArchiveFileIndex = new dmResourceArchive::ArchiveFileIndex;
        dmStrlCpy(archive->m_ArchiveFileIndex->m_Path, zip_path, DMPATH_MAX_PATH);

        archive->m_UserData = (void*)zip;

        *out = archive;

        return dmResourceArchive::RESULT_OK;
    }

    dmResourceArchive::Result LUUnloadArchive_Zip(dmResourceArchive::HArchiveIndexContainer archive)
    {
        dmZip::HZip zip = (dmZip::HZip)archive->m_UserData;
        if (zip)
            dmZip::Close(zip);
        return dmResourceArchive::RESULT_OK;
    }

    dmResourceArchive::Result LUFindEntryInArchive_Zip(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry)
    {
        dmZip::HZip zip = (dmZip::HZip)archive->m_UserData;

        char hash_buffer[dmResourceArchive::MAX_HASH*2+1];
        dmResource::BytesToHexString(hash, hash_len, hash_buffer, sizeof(hash_buffer));

        dmZip::Result zr = dmZip::OpenEntry(zip, hash_buffer);
        if (dmZip::RESULT_OK != zr)
            return dmResourceArchive::RESULT_NOT_FOUND;

        dmZip::CloseEntry(zip);

        if (entry != 0)
        {
            // TODO: Read from a cached index!

            uint32_t raw_resource_size;
            uint8_t* raw_resource = GetZipResource(zip, hash_buffer, &raw_resource_size);
            if (raw_resource == 0)
                return dmResourceArchive::RESULT_NOT_FOUND;

            dmResourceArchive::LiveUpdateResource resource(raw_resource, raw_resource_size);

            bool is_compressed = (resource.m_Header->m_Flags & dmResourceArchive::ENTRY_FLAG_COMPRESSED);

            entry->m_ResourceDataOffset = 0;
            entry->m_ResourceSize = is_compressed ? dmEndian::ToNetwork(resource.m_Header->m_Size) : resource.m_Count;
            entry->m_ResourceCompressedSize = is_compressed ? resource.m_Count : 0xFFFFFFFF;
            entry->m_Flags = resource.m_Header->m_Flags | dmResourceArchive::ENTRY_FLAG_LIVEUPDATE_DATA;

            dmMemory::AlignedFree(raw_resource);

        }

        return dmResourceArchive::RESULT_OK;
    }

    dmResourceArchive::Result LUReadEntryFromArchive_Zip(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const dmResourceArchive::EntryData* entry, void* buffer)
    {
        dmZip::HZip zip = (dmZip::HZip)archive->m_UserData;

        char hash_buffer[dmResourceArchive::MAX_HASH*2+1];
        dmResource::BytesToHexString(hash, hash_len, hash_buffer, sizeof(hash_buffer));

        uint32_t raw_resource_size;
        uint8_t* raw_resource = GetZipResource(zip, hash_buffer, &raw_resource_size);
        if (raw_resource == 0)
            return dmResourceArchive::RESULT_NOT_FOUND;

        dmResourceArchive::LiveUpdateResource resource(raw_resource, raw_resource_size);

        uint32_t flags = resource.m_Header->m_Flags;
        bool encrypted = flags & dmResourceArchive::ENTRY_FLAG_ENCRYPTED;
        bool compressed = flags & dmResourceArchive::ENTRY_FLAG_COMPRESSED;
        uint32_t compressed_size = entry->m_ResourceCompressedSize;
        uint32_t resource_size = entry->m_ResourceSize;

        dmResourceArchive::Result result = dmResourceArchive::RESULT_OK;
        if (encrypted)
        {
            result = dmResourceArchive::DecryptBuffer((uint8_t*)resource.m_Data, resource.m_Count);
            if (dmResourceArchive::RESULT_OK != result)
            {
                dmLogError("Failed to decrypt resource: '%s", hash_buffer);
                goto bail;
            }
        }

        if (compressed)
        {
            result = dmResourceArchive::DecompressBuffer((const uint8_t*)resource.m_Data, compressed_size, (uint8_t*)buffer, resource_size);
            if (dmResourceArchive::RESULT_OK != result)
            {
                dmLogError("Failed to decompress resource: '%s", hash_buffer);
                goto bail;
            }
        }
        else
        {
            memcpy(buffer, resource.m_Data, resource.m_Count);
        }

bail:
        dmMemory::AlignedFree(raw_resource);
        return result;
    }

    dmResourceArchive::Result LUCleanup_Zip(const char* archive_name, const char* app_path, const char* app_support_path)
    {
        const char* names[] = {LIVEUPDATE_ARCHIVE_FILENAME,LIVEUPDATE_ARCHIVE_TMP_FILENAME};
        for (int i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        {
            char path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, names[i], path, DMPATH_MAX_PATH);
            if (FileExists(path))
            {
                dmLogDebug("Cleaning up %s", path);
                dmSys::Unlink(path);
            }
        }
        return dmResourceArchive::RESULT_OK;
    }
}
