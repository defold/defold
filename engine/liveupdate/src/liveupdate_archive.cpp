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

#include "liveupdate.h"
#include "liveupdate_private.h"
#include <resource/resource.h>
#include <resource/resource_archive.h>

#include <dlib/path.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

namespace dmLiveUpdate
{
    Result BundleVersionValid(const dmResource::Manifest* manifest, const char* bundle_ver_path)
    {
        Result result = RESULT_OK;
        struct stat file_stat;
        bool bundle_ver_exists = stat(bundle_ver_path, &file_stat) == 0;

        uint8_t* signature = manifest->m_DDF->m_Signature.m_Data;
        uint32_t signature_len = manifest->m_DDF->m_Signature.m_Count;
        if (bundle_ver_exists)
        {
            FILE* bundle_ver = fopen(bundle_ver_path, "rb");
            uint8_t* buf = (uint8_t*)alloca(signature_len);
            fread(buf, 1, signature_len, bundle_ver);
            fclose(bundle_ver);
            if (memcmp(buf, signature, signature_len) != 0)
            {
                // Bundle has changed, local liveupdate manifest no longer valid.
                result = RESULT_VERSION_MISMATCH;
            }
        }
        else
        {
            // Take bundled manifest signature and write to 'bundle_ver' file
            FILE* bundle_ver = fopen(bundle_ver_path, "wb");
            size_t bytes_written = fwrite(signature, 1, signature_len, bundle_ver);
            if (bytes_written != signature_len)
            {
                dmLogWarning("Failed to write bundle version to file, wrote %u bytes out of %u bytes.", (uint32_t)bytes_written, signature_len);
            }
            fclose(bundle_ver);
            result = RESULT_OK;
        }

        return result;
    }

    dmResourceArchive::Result LULoadManifest_Regular(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** out)
    {
        char manifest_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_MANIFEST_FILENAME, manifest_path, DMPATH_MAX_PATH);

        if (!FileExists(manifest_path))
        {
            char archive_index_path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, LIVEUPDATE_INDEX_FILENAME, archive_index_path, DMPATH_MAX_PATH);

            // We might download a manifest later, so we shouldn't clean the currently downloaded archive.
            // This is the behavior from 1.2.174 and before.
            if (!FileExists(archive_index_path))
            {
                return dmResourceArchive::RESULT_NOT_FOUND; // we have neither manifest nor archive
            }
            *out = 0;
            return dmResourceArchive::RESULT_OK; // we at least have archive
        }

        // Check if bundle has changed (e.g. app upgraded)
        char bundle_ver_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_BUNDLE_VER_FILENAME, bundle_ver_path, DMPATH_MAX_PATH);
        Result bundle_ver_valid = BundleVersionValid(previous, bundle_ver_path);
        if (bundle_ver_valid != RESULT_OK)
        {
            // Bundle version file exists from previous run, but signature does not match currently loaded bundled manifest.
            // Unlink liveupdate.manifest and bundle_ver_path from filesystem and load bundled manifest instead.
            dmLogError("Found old '%s' (%d), removing old liveupdate data", bundle_ver_path, bundle_ver_valid);
            dmSys::Unlink(bundle_ver_path);
            dmSys::Unlink(manifest_path);

            return dmResourceArchive::RESULT_OK;
        }

        dmLogWarning("Loading LiveUpdate manifest: %s", manifest_path);
        dmResourceArchive::Result result = dmResourceArchive::LoadManifest(manifest_path, out);
        if (result != dmResourceArchive::RESULT_OK)
        {
            // we'll treat any of the errors as a version mismatch
            // this way, we may fallback to the base archive file in the game bundle
            return dmResourceArchive::RESULT_VERSION_MISMATCH;
        }

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

    dmResourceArchive::Result LULoadArchive_Regular(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path,
                                                    dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out)
    {
        char archive_index_tmp_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_INDEX_TMP_FILENAME, archive_index_tmp_path, DMPATH_MAX_PATH);

        char archive_index_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_INDEX_FILENAME, archive_index_path, DMPATH_MAX_PATH);

        struct stat file_stat;
        bool luTempIndexExists = stat(archive_index_tmp_path, &file_stat) == 0;
        if (luTempIndexExists)
        {
            dmSys::Result sys_result = dmSys::RenameFile(archive_index_path, archive_index_tmp_path);
            if (sys_result != dmSys::RESULT_OK)
            {
                // The recently added resources will not be available if we proceed after this point
                dmLogError("Failed to rename '%s' to '%s' (%i).", archive_index_tmp_path, archive_index_path, sys_result);
                return dmResourceArchive::RESULT_IO_ERROR;
            }
            dmLogInfo("Renamed '%s' to '%s'", archive_index_tmp_path, archive_index_path);
            dmSys::Unlink(archive_index_tmp_path);
        }

        bool file_exists = stat(archive_index_path, &file_stat) == 0;
        if (!file_exists)
            return dmResourceArchive::RESULT_OK;

        char archive_data_path[DMPATH_MAX_PATH];
        dmPath::Concat(app_support_path, LIVEUPDATE_DATA_FILENAME, archive_data_path, DMPATH_MAX_PATH);

        void* mount_info = 0;
        dmResource::Result result = dmResource::MountArchiveInternal(archive_index_path, archive_data_path, out, &mount_info);
        if (dmResource::RESULT_OK == result && mount_info != 0 && *out != 0)
        {
            (*out)->m_UserData = mount_info;
        }
        return dmResource::RESULT_OK == result ? dmResourceArchive::RESULT_OK : dmResourceArchive::RESULT_IO_ERROR;
    }


    dmResourceArchive::Result LUUnloadArchive_Regular(dmResourceArchive::HArchiveIndexContainer archive)
    {
        dmResource::UnmountArchiveInternal(archive, archive->m_UserData);
        return dmResourceArchive::RESULT_OK;
    }

    dmResourceArchive::Result LUFindEntryInArchive_Regular(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry)
    {
        dmResourceArchive::EntryData tmpentry;
        dmResourceArchive::Result result = dmResourceArchive::FindEntryInArchive(archive, hash, hash_len, &tmpentry);
        if (dmResourceArchive::RESULT_OK == result)
        {
            // Make sure it's from live update
            // If not, the offset data will not be correct (i.e.: it's an older format .arci where the entries were copied over from the original manifest)
            uint32_t flags = tmpentry.m_Flags;
            if (flags & dmResourceArchive::ENTRY_FLAG_LIVEUPDATE_DATA)
            {
                if (entry != 0)
                    *entry = tmpentry;
                return result;
            }
        }

        return dmResourceArchive::RESULT_NOT_FOUND;
    }

    dmResourceArchive::Result LUReadEntryFromArchive_Regular(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const dmResourceArchive::EntryData* entry, void* buffer)
    {
        return dmResourceArchive::ReadEntryFromArchive(archive, hash, hash_len, entry, buffer);
    }

    dmResourceArchive::Result LUCleanup_Regular(const char* archive_name, const char* app_path, const char* app_support_path)
    {
        const char* names[] = {LIVEUPDATE_MANIFEST_FILENAME,LIVEUPDATE_MANIFEST_TMP_FILENAME,LIVEUPDATE_INDEX_FILENAME,LIVEUPDATE_INDEX_TMP_FILENAME,LIVEUPDATE_DATA_FILENAME,LIVEUPDATE_DATA_TMP_FILENAME,LIVEUPDATE_BUNDLE_VER_FILENAME};
        for (int i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
        {
            char path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, names[i], path, DMPATH_MAX_PATH);
            if (FileExists(path))
            {
                dmLogInfo("Cleaning up %s", path);
                dmSys::Unlink(path);
            }
        }
        return dmResourceArchive::RESULT_OK;
    }
}