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

#include "liveupdate.h"
#include "liveupdate_private.h"
#include "script_liveupdate.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <ddf/ddf.h>

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/sys.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/extension/extension.h>

#include <resource/resource.h>
#include <resource/resource_archive.h>
#include <resource/resource_mounts.h>
#include <resource/resource_manifest.h> // project id len
#include <resource/providers/provider.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

namespace dmLiveUpdate
{
    const char* LIVEUPDATE_LEGACY_MOUNT_NAME        = "_liveupdate";
    const int   LIVEUPDATE_LEGACY_MOUNT_PRIORITY    = 10;

    const char* LIVEUPDATE_MANIFEST_FILENAME        = "liveupdate.dmanifest";
    const char* LIVEUPDATE_MANIFEST_TMP_FILENAME    = "liveupdate.dmanifest.tmp";
    const char* LIVEUPDATE_INDEX_FILENAME           = "liveupdate.arci";
    const char* LIVEUPDATE_INDEX_TMP_FILENAME       = "liveupdate.arci.tmp";
    const char* LIVEUPDATE_DATA_FILENAME            = "liveupdate.arcd";
    const char* LIVEUPDATE_DATA_TMP_FILENAME        = "liveupdate.arcd.tmp";
    const char* LIVEUPDATE_ARCHIVE_FILENAME         = "liveupdate.ref";
    const char* LIVEUPDATE_ARCHIVE_TMP_FILENAME     = "liveupdate.ref.tmp";
    const char* LIVEUPDATE_BUNDLE_VER_FILENAME      = "bundle.ver";

    Result ResourceResultToLiveupdateResult(dmResource::Result r)
    {
        switch (r)
        {
            case dmResource::RESULT_OK:                 return RESULT_OK;
            case dmResource::RESULT_IO_ERROR:           return RESULT_INVALID_RESOURCE;
            case dmResource::RESULT_FORMAT_ERROR:       return RESULT_INVALID_RESOURCE;
            case dmResource::RESULT_VERSION_MISMATCH:   return RESULT_VERSION_MISMATCH;
            case dmResource::RESULT_SIGNATURE_MISMATCH: return RESULT_SIGNATURE_MISMATCH;
            case dmResource::RESULT_NOT_SUPPORTED:      return RESULT_SCHEME_MISMATCH;
            case dmResource::RESULT_INVALID_DATA:       return RESULT_BUNDLED_RESOURCE_MISMATCH;
            case dmResource::RESULT_DDF_ERROR:          return RESULT_FORMAT_ERROR;
            default: break;
        }
        return RESULT_INVALID_RESOURCE;
    }

    struct LiveUpdate
    {
        LiveUpdate()
        {
            memset(this, 0x0, sizeof(*this));
        }

        char                            m_AppSupportPath[DMPATH_MAX_PATH]; // app support path using the project id as the folder name
        dmResource::HFactory            m_ResourceFactory;      // Resource system factory
        dmResourceMounts::HContext      m_ResourceMounts;       // The resource mounts
        dmResourceProvider::HArchive    m_ResourceBaseArchive;  // The "game.arcd" archive
        dmResourceProvider::HArchive    m_LiveupdateArchive;

        dmJobThread::HJobThread         m_JobThread;
    };

    LiveUpdate g_LiveUpdate;

    // ******************************************************************
    // ** LiveUpdate utility functions
    // ******************************************************************

    // ******************************************************************
    // ** LiveUpdate legacy functions
    // ******************************************************************

    // enum LegacyArchiveFormat
    // {
    //     LAF_ARCHIVE_MUTABLE = 0,
    //     LAF_ZIP             = 1,
    //     LAF_NONE            = -1,
    // };

    // // .ref archives take precedence over .arci/.arcd
    // // .tmp takes precedence over non .tmp
    // //  0: .arci/.arci format
    // //  1: .ref format
    // // -1: No LU archive file format found
    // static LegacyArchiveFormat LegacyDetermineLUType(const char* app_support_path)
    // {
    //     const char* names[4] = {LIVEUPDATE_ARCHIVE_TMP_FILENAME, LIVEUPDATE_INDEX_TMP_FILENAME, LIVEUPDATE_ARCHIVE_FILENAME, LIVEUPDATE_INDEX_FILENAME};
    //     LegacyArchiveFormat types[4] = {LAF_ZIP, LAF_ARCHIVE_MUTABLE, LAF_ZIP, LAF_ARCHIVE_MUTABLE};
    //     for (int i = 0; i < 4; ++i)
    //     {
    //         char path[DMPATH_MAX_PATH];
    //         dmPath::Concat(app_support_path, names[i], path, DMPATH_MAX_PATH);
    //         if (FileExists(path))
    //         {
    //             dmLogInfo("Found %s", path);
    //             return types[i];
    //         }
    //     }
    //     return LAF_NONE;
    // }

    // Result ParseManifestBin(uint8_t* manifest_data, uint32_t manifest_len, dmResource::Manifest* manifest)
    // {
    //     return ResourceResultToLiveupdateResult(dmResource::ManifestLoadMessage(manifest_data, manifest_len, manifest));
    // }

//     static Result StoreManifestInternal(const char* app_support_path, dmResource::Manifest* manifest)
//     {
// Replace with HArchive, and call SetManifest()

//         char manifest_file_path[DMPATH_MAX_PATH];
//         char manifest_tmp_file_path[DMPATH_MAX_PATH];

//         dmPath::Concat(app_support_path, LIVEUPDATE_MANIFEST_FILENAME, manifest_file_path, DMPATH_MAX_PATH);
//         dmPath::Concat(app_support_path, LIVEUPDATE_MANIFEST_TMP_FILENAME, manifest_tmp_file_path, DMPATH_MAX_PATH);

//         // write to tempfile, if successful move/rename and then delete tmpfile
//         dmDDF::Result ddf_result = dmDDF::SaveMessageToFile(manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor, manifest_tmp_file_path);
//         if (ddf_result != dmDDF::RESULT_OK)
//         {
//             dmLogError("Failed storing manifest to file '%s', result: %i", manifest_tmp_file_path, ddf_result);
//             return RESULT_IO_ERROR;
//         }
//         dmSys::Result sys_result = dmSys::Rename(manifest_file_path, manifest_tmp_file_path);
//         if (sys_result != dmSys::RESULT_OK)
//         {
//             return RESULT_IO_ERROR;
//         }
//         dmLogInfo("Stored manifest: '%s'", manifest_file_path);
//         return RESULT_OK;
//     }

    // Clean up old mount file formats that we don't need any more
    static void LegacyCleanOldMountFormats(const char* app_support_path)
    {
        const char* filenames[] = {LIVEUPDATE_ARCHIVE_TMP_FILENAME, LIVEUPDATE_ARCHIVE_FILENAME, LIVEUPDATE_BUNDLE_VER_FILENAME};
        for (int i = 0; i < DM_ARRAY_SIZE(filenames); ++i)
        {
            char path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, filenames[i], path, sizeof(path));
            if (dmSys::Exists(path))
            {
                dmLogError("Removed legacy file '%s'", path);
                dmSys::Unlink(path);
            }
        }
    }

    static dmResourceProvider::HArchiveLoader LegacyGetLoader(const char* name)
    {
        dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64(name));
        if (!loader)
        {
            dmLogError("Couldn't find archive loader of type '%s'", name);
            return 0;
        }
        return loader;
    }

    static dmResourceProvider::HArchive LegacyCreateMutableArchive(const char* app_support_path, dmResourceProvider::HArchive base_archive)
    {
        dmLogError("NOT IMPLEMENTED YET");
        return 0;
    }

    static dmResourceProvider::HArchive LegacyLoadMutableArchive(const char* app_support_path, dmResourceProvider::HArchive base_archive)
    {
        dmResourceProvider::HArchiveLoader loader = LegacyGetLoader("archive_mutable");
        if (!loader)
            return 0;

        const char* filenames[] = {LIVEUPDATE_INDEX_TMP_FILENAME, LIVEUPDATE_INDEX_FILENAME};
        for (int i = 0; i < DM_ARRAY_SIZE(filenames); ++i)
        {
            size_t scheme_len = strlen("dmanif:");
            char index_uri[DMPATH_MAX_PATH] = "dmanif:";
            char* index_path = index_uri + scheme_len;
            dmPath::Concat(app_support_path, filenames[i], index_path, sizeof(index_uri)-scheme_len);
            if (!dmSys::Exists(index_path))
                continue;

            dmLogInfo("Found index file at '%s'", index_path);

            dmURI::Parts uri;
            dmURI::Parse(index_uri, &uri);

            dmResourceProvider::HArchive archive;
            dmResourceProvider::Result result = dmResourceProvider::CreateMount(loader, &uri, base_archive, &archive);
            if (dmResourceProvider::RESULT_OK == result)
            {
                dmLogInfo("Created mutable archive mount from '%s'", index_uri);
                return archive;
            }
        }

        dmLogInfo("Found no liveupdate index paths");

        return 0;
    }

    static void TrimWhiteSpace(const char* src, char* dst, uint32_t dst_len)
    {
        const char* first = src;
        while (isspace(*first)) first++;

        const char* last = first;
        while (*last) last++; // find string end
        do {
            last--;
        } while (isspace(*last));

        uint32_t str_len = uint32_t(last - first + 1);
        if (str_len > (dst_len-1))
            str_len = (dst_len-1);

        memcpy(dst, first, str_len);
        dst[str_len] = 0;
    }

    // Load the .zip reference from a .ref / .ref.tmp file
    static void LegacyReadFileReference(const char* path, char* buffer, uint32_t buffer_len)
    {
        char zip_path[DMPATH_MAX_PATH] = {0};
        FILE* f = fopen(path, "rb");
        if (!f)
            return;

        if (buffer_len > 0)
        {
            fread(zip_path, 1, sizeof(zip_path), f);
            zip_path[sizeof(zip_path)-1] = 0;
            TrimWhiteSpace(zip_path, buffer, buffer_len);
        }

        fclose(f);
    }

    static dmResourceProvider::HArchive LegacyLoadZipArchive(const char* app_support_path, dmResourceProvider::HArchive base_archive)
    {
        dmResourceProvider::HArchiveLoader loader = LegacyGetLoader("zip");
        if (!loader)
            return 0;

        const char* filenames[] = {LIVEUPDATE_ARCHIVE_TMP_FILENAME, LIVEUPDATE_ARCHIVE_FILENAME};
        for (int i = 0; i < DM_ARRAY_SIZE(filenames); ++i)
        {
            char ref_path[DMPATH_MAX_PATH];
            dmPath::Concat(app_support_path, filenames[i], ref_path, sizeof(ref_path));
            if (!dmSys::Exists(ref_path))
                continue;

            dmLogInfo("Found zip reference file at '%s'", ref_path);

            size_t scheme_len = strlen("zip:");
            char zip_uri[DMPATH_MAX_PATH] = "zip:";
            char* zip_path = zip_uri + scheme_len;
            LegacyReadFileReference(ref_path, zip_path, sizeof(zip_uri)-scheme_len);

            dmLogInfo("Read zip reference '%s'", zip_path);

            if (!dmSys::Exists(zip_path))
            {
                dmLogInfo("Zip reference didn't exist: '%s'", zip_path);
                continue;
            }

            dmLogInfo("Found zip file at '%s'", zip_path);

            dmURI::Parts uri;
            dmURI::Parse(zip_uri, &uri);

            dmResourceProvider::HArchive archive;
            dmResourceProvider::Result result = dmResourceProvider::CreateMount(loader, &uri, base_archive, &archive);
            if (dmResourceProvider::RESULT_OK == result)
            {
                dmLogInfo("Created zip mount from '%s'", zip_uri);
                return archive;
            }
        }

        dmLogInfo("Found no liveupdate zip file references");
        return 0;
    }

    // The idea with this solution is to convert old formats into new mount format.
    // We do this by adding them to the mount system with a specific name.
    static dmResourceProvider::HArchive LegacyLoadArchive(dmResourceMounts::HContext mounts, dmResourceProvider::HArchive base_archive, const char* app_support_path)
    {
        dmResourceProvider::HArchive archive = LegacyLoadZipArchive(app_support_path, base_archive);
        if (!archive)
        {
            archive = LegacyLoadMutableArchive(app_support_path, base_archive);
        }

        // Don't remove these files until we can persist the mounts!
        //LegacyCleanOldMountFormats(app_support_path);

        if (archive)
        {
            dmResource::Result result = dmResourceMounts::AddMount(mounts, LIVEUPDATE_LEGACY_MOUNT_NAME, archive, LIVEUPDATE_LEGACY_MOUNT_PRIORITY, true);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Failed to mount legacy archive: %s", dmResource::ResultToString(result));
            }

            // Also flush the resource mounts to disc
            // dmResourceMounts::Save(mounts);
        }

        return archive;
    }

    // Legacy function
    Result StoreManifestToMutableArchive(dmResource::Manifest* manifest)
    {
        if (!g_LiveUpdate.m_LiveupdateArchive)
        {
            if (!manifest)
            {
                dmLogWarning("Trying to set a null manifest to a non existing liveupdate archive");
                return RESULT_OK;
            }

            // time to create a liveupdate mutable archive (legacy)
            g_LiveUpdate.m_LiveupdateArchive = LegacyCreateMutableArchive(g_LiveUpdate.m_AppSupportPath, g_LiveUpdate.m_ResourceBaseArchive);
        }

        dmResourceProvider::Result result = dmResourceProvider::SetManifest(g_LiveUpdate.m_LiveupdateArchive, manifest);
        if (dmResourceProvider::RESULT_OK != result)
        {
            dmURI::Parts uri;
            dmResourceProvider::GetUri(g_LiveUpdate.m_LiveupdateArchive, &uri);
            dmLogWarning("Failed to set manifest to mounted archive uri: %s:%s/%s\n", uri.m_Scheme, uri.m_Location, uri.m_Path);
            return RESULT_INVALID_RESOURCE;
        }

        return RESULT_OK;
        // char app_support_path[DMPATH_MAX_PATH];
        // if (dmResource::RESULT_OK != GetApplicationSupportPath(manifest, app_support_path, (uint32_t)sizeof(app_support_path)))
        // {
        //     return RESULT_IO_ERROR;
        // }

        // // Store the manifest file to disc
        // return StoreManifestInternal(g_LiveUpdate.m_AppSupportPath, manifest) == RESULT_OK ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    Result StoreResourceAsync(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data)
    {
        if (manifest == 0x0 || resource->m_Data == 0x0)
        {
            return RESULT_MEM_ERROR;
        }

        AsyncResourceRequest request;
        request.m_Manifest = manifest;
        request.m_ExpectedResourceDigestLength = expected_digest_length;
        request.m_ExpectedResourceDigest = expected_digest;
        request.m_Resource.Set(*resource);
        request.m_CallbackData = callback_data;
        request.m_Callback = callback;
        bool res = AddAsyncResourceRequest(request);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    Result StoreArchiveAsync(const char* path, void (*callback)(bool, void*), void* callback_data, bool verify_archive)
    {
        if (!dmSys::Exists(path)) {
            dmLogError("File does not exist: '%s'", path);
            return RESULT_INVALID_RESOURCE;
        }

        AsyncResourceRequest request;
        request.m_CallbackData = callback_data;
        request.m_Callback = callback;
        request.m_Path = path;
        request.m_IsArchive = 1;
        request.m_VerifyArchive = verify_archive;
        //request.m_Manifest = dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory);
        dmResourceProvider::GetManifest(g_LiveUpdate.m_ResourceBaseArchive, &request.m_Manifest);

        bool res = AddAsyncResourceRequest(request);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    // static void* CopyDDFMessage(void* message, const dmDDF::Descriptor* desc)
    // {
    //     if (!message)
    //         return 0;

    //     dmArray<uint8_t> buffer;
    //     dmDDF::Result ddf_result = dmDDF::SaveMessageToArray(message, desc, buffer);
    //     if (dmDDF::RESULT_OK != ddf_result)
    //     {
    //         return 0;
    //     }

    //     void* copy = 0;
    //     ddf_result = dmDDF::LoadMessage((void*)&buffer[0], buffer.Size(), desc, (void**)&copy);
    //     if (dmDDF::RESULT_OK != ddf_result)
    //     {
    //         return 0;
    //     }

    //     return copy;
    // }

    // static void CreateFilesIfNotExists(dmResourceArchive::HArchiveIndexContainer archive_container, const char* app_support_path, const char* arci, const char* arcd)
    // {
    //     char lu_index_path[DMPATH_MAX_PATH] = {};
    //     dmPath::Concat(app_support_path, arci, lu_index_path, DMPATH_MAX_PATH);

    //     // If liveupdate.arci does not exists, create it and liveupdate.arcd
    //     if (!dmSys::Exists(lu_index_path))
    //     {
    //         FILE* f_lu_index = fopen(lu_index_path, "wb");
    //         fclose(f_lu_index);
    //     }

    //     dmResourceArchive::ArchiveFileIndex* afi = archive_container->m_ArchiveFileIndex;
    //     if (afi->m_FileResourceData == 0)
    //     {
    //         // Data file has same path and filename as index file, but extension .arcd instead of .arci.
    //         char lu_data_path[DMPATH_MAX_PATH];
    //         dmPath::Concat(app_support_path, arcd, lu_data_path, DMPATH_MAX_PATH);

    //         FILE* f_lu_data = fopen(lu_data_path, "ab+");
    //         if (!f_lu_data)
    //         {
    //             dmLogError("Failed to create liveupdate resource file");
    //         }

    //         dmResourceArchive::ArchiveFileIndex* afi = archive_container->m_ArchiveFileIndex;

    //         dmStrlCpy(afi->m_Path, lu_data_path, DMPATH_MAX_PATH);
    //         dmLogInfo("Live Update archive: %s", afi->m_Path);
    //         afi->m_FileResourceData = f_lu_data;
    //         afi->m_ResourceData = 0x0;
    //         afi->m_ResourceSize = 0;
    //         afi->m_IsMemMapped = false;
    //     }
    // }

    // static dmResourceArchive::Result LULoadManifest(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* base_manifest, dmResource::Manifest** out)
    // {
    //     dmStrlCpy(g_LiveUpdate.m_AppPath, app_path, DMPATH_MAX_PATH); // MAWE : Only used to find the public key!

    //     g_LiveUpdate.m_ArchiveType = DetermineLUType(app_support_path);
    //     if (g_LiveUpdate.m_ArchiveType == -1)
    //         return dmResourceArchive::RESULT_NOT_FOUND;

    //     dmResourceArchive::Result result = dmResourceArchive::RESULT_OK;
    //     if (g_LiveUpdate.m_ArchiveType == 1)
    //     {
    //         result = LULoadManifest_Zip(archive_name, app_path, app_support_path, base_manifest, out);
    //         if (dmResourceArchive::RESULT_OK != result)
    //         {
    //             LUCleanup_Zip(archive_name, app_path, app_support_path);
    //             g_LiveUpdate.m_ArchiveType = 0;
    //         }
    //         else
    //         {
    //             LUCleanup_Regular(archive_name, app_path, app_support_path);
    //         }
    //     }

    //     if (g_LiveUpdate.m_ArchiveType == 0)
    //     {
    //         result = LULoadManifest_Regular(archive_name, app_path, app_support_path, base_manifest, out);
    //         if (dmResourceArchive::RESULT_OK != result)
    //         {
    //             LUCleanup_Regular(archive_name, app_path, app_support_path);
    //             g_LiveUpdate.m_ArchiveType = -1;
    //         }
    //         else
    //         {
    //             LUCleanup_Zip(archive_name, app_path, app_support_path);
    //         }
    //     }

    //     if (dmResourceArchive::RESULT_OK == result)
    //     {
    //         assert(g_LiveUpdate.m_LUManifest == 0);
    //         g_LiveUpdate.m_LUManifest = *out;
    //     }
    //     return result;
    // }

    // static dmResourceArchive::Result LUCleanupArchive(const char* archive_name, const char* app_path, const char* app_support_path)
    // {
    //     if (g_LiveUpdate.m_ArchiveType == -1)
    //         return dmResourceArchive::RESULT_OK;
    //     else if (g_LiveUpdate.m_ArchiveType == 1)
    //         return LUCleanup_Zip(archive_name, app_path, app_support_path);
    //     else
    //         return LUCleanup_Regular(archive_name, app_path, app_support_path);
    // }

    // static dmResourceArchive::Result LULoadArchive(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path, dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out)
    // {
    //     if (g_LiveUpdate.m_ArchiveType == -1)
    //         return dmResourceArchive::RESULT_NOT_FOUND;

    //     dmResourceArchive::Result result;
    //     if (g_LiveUpdate.m_ArchiveType == 1)
    //         result = LULoadArchive_Zip(manifest, archive_name, app_path, app_support_path, previous, out);
    //     else
    //         result = LULoadArchive_Regular(manifest, archive_name, app_path, app_support_path, previous, out);

    //     if (dmResourceArchive::RESULT_OK != result)
    //     {
    //         LUCleanupArchive(archive_name, app_path, app_support_path);
    //         g_LiveUpdate.m_ArchiveType = -1;
    //     }

    //     return result;
    // }

    // static dmResourceArchive::Result LUUnloadArchive(dmResourceArchive::HArchiveIndexContainer archive)
    // {
    //     assert(g_LiveUpdate.m_ArchiveType != -1);
    //     if (g_LiveUpdate.m_ArchiveType == 1)
    //         return LUUnloadArchive_Zip(archive);
    //     else
    //         return LUUnloadArchive_Regular(archive);
    // }

    // static dmResourceArchive::Result LUFindEntryInArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry)
    // {
    //     assert(g_LiveUpdate.m_ArchiveType != -1);
    //     if (g_LiveUpdate.m_ArchiveType == 1)
    //         return LUFindEntryInArchive_Zip(archive, hash, hash_len, entry);
    //     else
    //         return LUFindEntryInArchive_Regular(archive, hash, hash_len, entry);
    // }

    // static dmResourceArchive::Result LUReadEntryInArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, const dmResourceArchive::EntryData* entry, void* buffer)
    // {
    //     assert(g_LiveUpdate.m_ArchiveType != -1);
    //     if (g_LiveUpdate.m_ArchiveType == 1)
    //         return LUReadEntryFromArchive_Zip(archive, hash, hash_len, entry, buffer);
    //     else
    //         return LUReadEntryFromArchive_Regular(archive, hash, hash_len, entry, buffer);
    // }

    // static dmResource::Manifest* CreateLUManifest(const dmResource::Manifest* base_manifest)
    // {
    //     // Create the actual copy
    //     dmResource::Manifest* manifest = new dmResource::Manifest;
    //     manifest->m_DDF = (dmLiveUpdateDDF::ManifestFile*)CopyDDFMessage(base_manifest->m_DDF, dmLiveUpdateDDF::ManifestFile::m_DDFDescriptor);
    //     manifest->m_DDFData = (dmLiveUpdateDDF::ManifestData*)CopyDDFMessage(base_manifest->m_DDFData, dmLiveUpdateDDF::ManifestData::m_DDFDescriptor);

    //     manifest->m_ArchiveIndex = new dmResourceArchive::ArchiveIndexContainer;
    //     manifest->m_ArchiveIndex->m_ArchiveIndex = new dmResourceArchive::ArchiveIndex;
    //     manifest->m_ArchiveIndex->m_ArchiveFileIndex = new dmResourceArchive::ArchiveFileIndex;
    //     // Even though it isn't technically true, it will allow us to use the correct ArchiveIndex struct
    //     manifest->m_ArchiveIndex->m_IsMemMapped = 1;

    //     manifest->m_ArchiveIndex->m_ArchiveIndex->m_Version = base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_Version;
    //     manifest->m_ArchiveIndex->m_ArchiveIndex->m_HashLength = base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_HashLength;
    //     memcpy(manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5, base_manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5, sizeof(manifest->m_ArchiveIndex->m_ArchiveIndex->m_ArchiveIndexMD5));

    //     char app_support_path[DMPATH_MAX_PATH];
    //     if (dmResource::RESULT_OK != GetApplicationSupportPath(base_manifest, app_support_path, (uint32_t)sizeof(app_support_path)))
    //     {
    //         return 0;
    //     }

    //     // Data file has same path and filename as index file, but extension .arcd instead of .arci.
    //     char lu_data_path[DMPATH_MAX_PATH];
    //     dmPath::Concat(app_support_path, LIVEUPDATE_DATA_FILENAME, lu_data_path, DMPATH_MAX_PATH);

    //     FILE* f_lu_data = fopen(lu_data_path, "wb+");
    //     if (!f_lu_data)
    //     {
    //         dmLogError("Failed to create/load liveupdate resource file");
    //     }

    //     dmStrlCpy(manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_Path, lu_data_path, DMPATH_MAX_PATH);
    //     dmLogInfo("Live Update archive: %s", manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_Path);

    //     manifest->m_ArchiveIndex->m_ArchiveFileIndex->m_FileResourceData = f_lu_data;

    //     manifest->m_ArchiveIndex->m_Loader.m_Unload = LUUnloadArchive_Regular;
    //     manifest->m_ArchiveIndex->m_Loader.m_FindEntry = LUFindEntryInArchive_Regular;
    //     manifest->m_ArchiveIndex->m_Loader.m_Read = LUReadEntryFromArchive_Regular;

    //     return manifest;
    // }

    // void RegisterArchiveLoaders()
    // {
    //     dmResourceArchive::ArchiveLoader loader;
    //     loader.m_LoadManifest = LULoadManifest;
    //     loader.m_Load = LULoadArchive;
    //     loader.m_Unload = LUUnloadArchive;
    //     loader.m_FindEntry = LUFindEntryInArchive;
    //     loader.m_Read = LUReadEntryInArchive;
    //     dmResourceArchive::RegisterArchiveLoader(loader);
    // }


    // Result StoreZipArchive(const char* path, bool verify_archive)
    // {
    //     char application_support_path[DMPATH_MAX_PATH];
    //     Result result = LoadAndOptionallyVerifyZipArchive(path, application_support_path, sizeof(application_support_path), verify_archive);
    //     if (RESULT_OK != result)
    //     {
    //         return result;
    //     }

    //     // Store zip file ref in "<support path>/liveupdate.ref.tmp"
    //     char archive_path[DMPATH_MAX_PATH];
    //     dmSnPrintf(archive_path, sizeof(archive_path), "%s/%s", application_support_path, LIVEUPDATE_ARCHIVE_TMP_FILENAME);

    //     FILE* f = fopen(archive_path, "wb");
    //     if (!f) {
    //         dmLogError("Couldn't open %s for writing", archive_path);
    //         return RESULT_IO_ERROR;
    //     }

    //     size_t len = (size_t)strlen(path);
    //     size_t nwritten = fwrite(path, 1, len, f);
    //     fclose(f);

    //     if (nwritten != len) {
    //         dmLogError("Couldn't write liveupdate path reference to %s", archive_path);
    //         return RESULT_IO_ERROR;
    //     }

    //     dmLogInfo("Stored reference to '%s' in '%s'", path, archive_path);

    //     return RESULT_OK;
    // }

    // ******************************************************************
    // ** LiveUpdate async functions
    // ******************************************************************

    bool PushAsyncJob(dmJobThread::FJobItemProcess process, dmJobThread::FJobItemCallback callback, void* jobctx, void* jobdata)
    {
        return dmJobThread::PushJob(g_LiveUpdate.m_JobThread, process, callback, jobctx, jobdata);
    }

    // ******************************************************************
    // ** LiveUpdate utility functions
    // ******************************************************************

    static Result GetApplicationSupportPath(dmResource::Manifest* manifest, char* buffer, uint32_t buffer_len)
    {
        char id_buf[dmResource::MANIFEST_PROJ_ID_LEN]; // String repr. of project id SHA1 hash
        const char* project_id = dmResource::GetProjectId(manifest, id_buf, sizeof(id_buf));
        if (project_id == 0)
        {
            dmLogError("Failed get project id from manifest");
            return RESULT_IO_ERROR;
        }

        dmSys::Result s_result = dmSys::GetApplicationSupportPath(id_buf, buffer, buffer_len);
        if (dmSys::RESULT_OK != s_result)
        {
            dmLogError("Failed get application support path for \"%s\", result = %i", id_buf, dmSys::RESULT_OK);
            return RESULT_IO_ERROR;
        }
        return RESULT_OK;
    }

    static dmResourceProvider::HArchive FindLiveupdateArchiveMount(dmResourceMounts::HContext mounts, const char* name)
    {
        dmResourceMounts::SGetMountResult info;
        dmResource::Result result = dmResourceMounts::GetMountByName(mounts, name, &info);
        if (dmResource::RESULT_OK == result)
        {
            return info.m_Archive;
        }
        return 0;
    }

    // ******************************************************************
    // ** LiveUpdate scripting
    // ******************************************************************

    bool HasLiveUpdateMount()
    {
        return g_LiveUpdate.m_LiveupdateArchive != 0;
    }

    // ******************************************************************
    // ** LiveUpdate life cycle functions
    // ******************************************************************

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        dmResource::HFactory factory = params->m_ResourceFactory;
        g_LiveUpdate.m_ResourceFactory = factory;
        g_LiveUpdate.m_ResourceMounts = dmResource::GetMountsContext(factory);
        g_LiveUpdate.m_ResourceBaseArchive = GetBaseArchive(factory);

        dmResource::Manifest* manifest;
        dmResourceProvider::Result p_result = dmResourceProvider::GetManifest(g_LiveUpdate.m_ResourceBaseArchive, &manifest);
        if (dmResourceProvider::RESULT_OK != p_result)
        {
            dmLogError("Could not get base archive manifest project id. Liveupdate disabled");
            return dmExtension::RESULT_OK;
        }

        Result result = GetApplicationSupportPath(manifest, g_LiveUpdate.m_AppSupportPath, sizeof(g_LiveUpdate.m_AppSupportPath));
        if (RESULT_OK != result)
        {
            dmLogError("Could not determine liveupdate folder. Liveupdate disabled");
            return dmExtension::RESULT_OK;
        }

        dmLogInfo("Liveupdate folder located at: %s", g_LiveUpdate.m_AppSupportPath);

        g_LiveUpdate.m_LiveupdateArchive = FindLiveupdateArchiveMount(g_LiveUpdate.m_ResourceMounts, LIVEUPDATE_LEGACY_MOUNT_NAME);
        if (!g_LiveUpdate.m_LiveupdateArchive)
        {
            g_LiveUpdate.m_LiveupdateArchive = LegacyLoadArchive(g_LiveUpdate.m_ResourceMounts, g_LiveUpdate.m_ResourceBaseArchive, g_LiveUpdate.m_AppSupportPath);
        }

        if (!g_LiveUpdate.m_LiveupdateArchive)
        {
            dmLogDebug("No liveupdate mount or archive found at startup");
        }

        dmLiveUpdate::AsyncInitialize(factory);
        g_LiveUpdate.m_JobThread = dmJobThread::New("liveupdate_jobs");

        if (params->m_L) // TODO: until unit tests have been updated
            ScriptInit(params->m_L, factory);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params* params)
    {
        dmJobThread::Join(g_LiveUpdate.m_JobThread);
        // // if this is is not the base manifest
        // if (g_LiveUpdate.m_ResourceFactory && dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory) != g_LiveUpdate.m_LUManifest)
        //     dmResource::DeleteManifest(g_LiveUpdate.m_LUManifest);
        // g_LiveUpdate.m_LUManifest = 0;
        g_LiveUpdate.m_ResourceFactory = 0;
        dmLiveUpdate::AsyncFinalize();
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params* params)
    {
        DM_PROFILE("LiveUpdate");
        AsyncUpdate();
        dmJobThread::Update(g_LiveUpdate.m_JobThread);
        return dmExtension::RESULT_OK;
    }
};

DM_DECLARE_EXTENSION(LiveUpdateExt, "LiveUpdate", 0, 0, dmLiveUpdate::Initialize, dmLiveUpdate::Update, 0, dmLiveUpdate::Finalize)
