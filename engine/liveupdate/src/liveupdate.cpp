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
#include "liveupdate_verify.h"
#include "script_liveupdate.h"
#include "job_thread.h"

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
#include <resource/resource_verify.h>   // VerifyManifest
#include <resource/resource_util.h>     // BytesToHexString for debug printing
#include <resource/providers/provider.h>

#if defined(_WIN32)
#include <malloc.h>
#define alloca(_SIZE) _alloca(_SIZE)
#endif

namespace dmLiveUpdate
{
    const char* LIVEUPDATE_LEGACY_MOUNT_NAME        = "liveupdate"; // By not prefixing it with '_', the user may then remove it
    const int   LIVEUPDATE_LEGACY_MOUNT_PRIORITY    = 10;

    const char* LIVEUPDATE_INDEX_FILENAME           = "liveupdate.arci";
    const char* LIVEUPDATE_INDEX_TMP_FILENAME       = "liveupdate.arci.tmp";
    const char* LIVEUPDATE_ARCHIVE_FILENAME         = "liveupdate.arcd";
    const char* LIVEUPDATE_ARCHIVE_TMP_FILENAME     = "liveupdate.arcd.tmp";
    const char* LIVEUPDATE_ZIP_ARCHIVE_FILENAME     = "liveupdate.ref";
    const char* LIVEUPDATE_ZIP_ARCHIVE_TMP_FILENAME = "liveupdate.ref.tmp";
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

    const char* ResultToString(Result result)
    {
        #define DM_LU_RESULT_TO_STR(x) case RESULT_##x: return #x
        switch (result)
        {
            DM_LU_RESULT_TO_STR(OK);
            DM_LU_RESULT_TO_STR(INVALID_HEADER);
            DM_LU_RESULT_TO_STR(MEM_ERROR);
            DM_LU_RESULT_TO_STR(INVALID_RESOURCE);
            DM_LU_RESULT_TO_STR(VERSION_MISMATCH);
            DM_LU_RESULT_TO_STR(ENGINE_VERSION_MISMATCH);
            DM_LU_RESULT_TO_STR(SIGNATURE_MISMATCH);
            DM_LU_RESULT_TO_STR(SCHEME_MISMATCH);
            DM_LU_RESULT_TO_STR(BUNDLED_RESOURCE_MISMATCH);
            DM_LU_RESULT_TO_STR(FORMAT_ERROR);
            DM_LU_RESULT_TO_STR(IO_ERROR);
            DM_LU_RESULT_TO_STR(INVAL);
            DM_LU_RESULT_TO_STR(UNKNOWN);
            default: break;
        }
        #undef DM_LU_RESULT_TO_STR
        return "RESULT_UNDEFINED";
    }

    struct LiveUpdateCtx
    {
        LiveUpdateCtx()
        {
            memset(this, 0x0, sizeof(*this));
        }

        char                            m_AppSupportPath[DMPATH_MAX_PATH]; // app support path using the project id as the folder name
        dmResourceMounts::HContext      m_ResourceMounts;       // The resource mounts
        dmResourceProvider::HArchive    m_ResourceBaseArchive;  // The "game.arcd" archive

        dmJobThread::HContext           m_JobThread;

        // Legacy functionality
        dmResource::HFactory            m_ResourceFactory;      // Resource system factory
        dmResourceProvider::HArchive    m_LiveupdateArchive;
        dmResource::HManifest           m_LiveupdateArchiveManifest;

    } g_LiveUpdate;

    // ******************************************************************
    // ** LiveUpdate legacy functions
    // ******************************************************************

    // Clean up old mount file formats that we don't need any more
    static void LegacyCleanOldZipMountFormats(const char* app_support_path)
    {
        const char* filenames[] = {LIVEUPDATE_ZIP_ARCHIVE_TMP_FILENAME, LIVEUPDATE_ZIP_ARCHIVE_FILENAME, LIVEUPDATE_BUNDLE_VER_FILENAME};
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

    static void LegacyCleanOldArchiveMountFormats(const char* app_support_path)
    {
        const char* filenames[] = {LIVEUPDATE_INDEX_FILENAME, LIVEUPDATE_INDEX_TMP_FILENAME, LIVEUPDATE_BUNDLE_VER_FILENAME,
                                   LIVEUPDATE_ARCHIVE_FILENAME, LIVEUPDATE_ARCHIVE_TMP_FILENAME};
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
        char archive_uri[DMPATH_MAX_PATH] = "mutable:";
        size_t scheme_len = strlen(archive_uri);
        char* path = archive_uri + scheme_len;
        dmPath::Concat(app_support_path, LIVEUPDATE_INDEX_FILENAME, path, sizeof(archive_uri)-scheme_len);

        dmURI::Parts uri;
        dmURI::Parse(archive_uri, &uri);

        dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64(uri.m_Scheme));
        if (!loader)
        {
            dmLogError("Failed to find '%s' loader", uri.m_Scheme);
            return 0;
        }

        dmResourceProvider::HArchive archive = 0;
        dmResourceProvider::Result result = dmResourceProvider::CreateMount(loader, &uri, g_LiveUpdate.m_ResourceBaseArchive, &archive);
        if (dmResourceProvider::RESULT_OK != result)
        {
            dmLogError("Failed to create new archive at '%s'", archive_uri);
            return 0;
        }

        if (archive)
        {
            dmResource::Result result = dmResourceMounts::AddMount(g_LiveUpdate.m_ResourceMounts, LIVEUPDATE_LEGACY_MOUNT_NAME, archive, LIVEUPDATE_LEGACY_MOUNT_PRIORITY, true);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Failed to mount legacy archive '%s': %s", archive_uri, dmResource::ResultToString(result));
            }
            else {
                // Flush the resource mounts to disc
                dmResourceMounts::SaveMounts(g_LiveUpdate.m_ResourceMounts, g_LiveUpdate.m_AppSupportPath);
            }
        }

        return archive;
    }

    static dmResourceProvider::HArchive LegacyLoadMutableArchive(const char* app_support_path, dmResourceProvider::HArchive base_archive)
    {
        dmResourceProvider::HArchiveLoader loader = LegacyGetLoader("mutable");
        if (!loader)
            return 0;

        const char* filenames[] = {LIVEUPDATE_INDEX_TMP_FILENAME, LIVEUPDATE_INDEX_FILENAME};
        for (int i = 0; i < DM_ARRAY_SIZE(filenames); ++i)
        {
            char index_uri[DMPATH_MAX_PATH] = "mutable:";
            size_t scheme_len = strlen(index_uri);
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
            else if (dmResourceProvider::RESULT_SIGNATURE_MISMATCH == result)
            {
                dmLogError("Cleaning out old archive formats '%s'", app_support_path);
                LegacyCleanOldArchiveMountFormats(app_support_path);
                break; // Let the user redownload the content again
            }
        }

        dmLogInfo("Found no legacy liveupdate index paths");

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

        const char* filenames[] = {LIVEUPDATE_ZIP_ARCHIVE_TMP_FILENAME, LIVEUPDATE_ZIP_ARCHIVE_FILENAME};
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

        dmLogInfo("Found no legacy liveupdate zip file references");
        return 0;
    }

    // The idea with this solution is to convert old formats into new mount format.
    // We do this by adding them to the mount system with a specific name.
    static dmResourceProvider::HArchive LegacyLoadArchive(dmResourceMounts::HContext mounts, dmResourceProvider::HArchive base_archive, const char* app_support_path)
    {
        dmResourceProvider::HArchive archive = LegacyLoadZipArchive(app_support_path, base_archive);
        if (archive)
        {
            // we have no need for the .arci/.arcd files anymore
            LegacyCleanOldArchiveMountFormats(app_support_path);
        }

        if (!archive)
        {
            archive = LegacyLoadMutableArchive(app_support_path, base_archive);
        }

        // We have no need for the .ref file anymore, since we'll store it in liveupdate.mounts instead
        LegacyCleanOldZipMountFormats(app_support_path);

        if (archive)
        {
            dmResource::Result result = dmResourceMounts::AddMount(mounts, LIVEUPDATE_LEGACY_MOUNT_NAME, archive, LIVEUPDATE_LEGACY_MOUNT_PRIORITY, true);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Failed to mount legacy archive: %s", dmResource::ResultToString(result));
            }
            else
            {
                // Flush the resource mounts to disc
                dmResourceMounts::SaveMounts(g_LiveUpdate.m_ResourceMounts, g_LiveUpdate.m_AppSupportPath);
            }
        }

        return archive;
    }

    static bool IsLiveupdateDisabled()
    {
        if (!g_LiveUpdate.m_JobThread)
        {
            dmLogError("Liveupdate function can't be called. Liveupdate disabled");
            return true;
        }
        return false;
    }

    // Legacy function
    static dmResource::Result LegacyStoreManifestToMutableArchive(dmResource::Manifest* manifest)
    {
        if (!g_LiveUpdate.m_LiveupdateArchive)
        {
            if (!manifest)
            {
                dmLogWarning("Trying to set a null manifest to a non existing liveupdate archive");
                return dmResource::RESULT_INVAL;
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
            return dmResource::RESULT_INVALID_DATA;
        }
        dmResourceProvider::GetManifest(g_LiveUpdate.m_LiveupdateArchive, &g_LiveUpdate.m_LiveupdateArchiveManifest);
        return dmResource::RESULT_OK;
    }

    // ******************************************************************
    // ** LiveUpdate async functions
    // ******************************************************************

    static bool PushAsyncJob(dmJobThread::FProcess process, dmJobThread::FCallback callback, void* jobctx, void* jobdata)
    {
        dmJobThread::PushJob(g_LiveUpdate.m_JobThread, process, callback, jobctx, jobdata);
        return true;
    }

    // ******************************************************************
    // ** LiveUpdate script functions
    // ******************************************************************

    // ******************************************************************************************************************************************

    struct ResourceInfo
    {
        ResourceInfo() {
            memset(this, 0, sizeof(*this));
        }
        dmResourceProvider::HArchive            m_Archive;
        void*                                   m_Resource; // points to external, ref-counted data from the Lua call. Released after the callback
        uint32_t                                m_ResourceLength;
        const char*                             m_ExpectedResourceDigest;
        uint32_t                                m_ExpectedResourceDigestLength;

        void                                    (*m_Callback)(bool, void*);
        void*                                   m_CallbackData;

        uint8_t                                 m_IsArchive:1;
        uint8_t                                 m_VerifyArchive:1;
    };

    // Called on the worker thread
    static int StoreResourceProcess(LiveUpdateCtx* jobctx, ResourceInfo* job)
    {
        if (jobctx->m_LiveupdateArchiveManifest == 0 || jobctx->m_LiveupdateArchive == 0)
        {
            dmLogWarning("No liveupdate mount found. Have to create one now");
            jobctx->m_LiveupdateArchive = LegacyCreateMutableArchive(jobctx->m_AppSupportPath, jobctx->m_ResourceBaseArchive);

            if (g_LiveUpdate.m_LiveupdateArchive)
                dmResourceProvider::GetManifest(g_LiveUpdate.m_LiveupdateArchive, &g_LiveUpdate.m_LiveupdateArchiveManifest);

            if (jobctx->m_LiveupdateArchiveManifest == 0 || jobctx->m_LiveupdateArchive == 0)
            {
                dmLogError("Still no liveupdate mount found. Skipping storing of resource: %s", job->m_ExpectedResourceDigest);
                return 0;
            }
        }

        dmhash_t digest_hash = dmHashBuffer64(job->m_ExpectedResourceDigest, job->m_ExpectedResourceDigestLength);
        dmhash_t url_hash = dmResource::GetUrlHashFromHexDigest(jobctx->m_LiveupdateArchiveManifest, digest_hash);

        if (!url_hash)
            return 0;

        // The path, is actually just a filename when it comes to these resources
        dmResourceProvider::Result result = dmResourceProvider::WriteFile(jobctx->m_LiveupdateArchive, url_hash, job->m_ExpectedResourceDigest,
                                                        (uint8_t*)job->m_Resource, job->m_ResourceLength);

        return dmResourceProvider::RESULT_OK == result;
    }
    // Called on the main thread (see dmJobThread::Update below)
    static void StoreResourceFinished(LiveUpdateCtx* jobctx, ResourceInfo* job, int result)
    {
        if (job->m_Callback)
            job->m_Callback(result == 1, job->m_CallbackData);
        delete job;
    }

    Result StoreResourceAsync(const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, void (*callback)(bool, void*), void* callback_data)
    {
        if (resource->m_Data == 0x0)
        {
            return RESULT_MEM_ERROR;
        }

        if (!resource->HasValidHeader())
        {
            dmLogError("Resource has invalid header: '%s'", expected_digest);
            return RESULT_INVALID_RESOURCE;
        }

        if(IsLiveupdateDisabled())
        {
            return RESULT_INVAL;
        }

        ResourceInfo* info = new ResourceInfo;
        info->m_Archive = g_LiveUpdate.m_LiveupdateArchive;
        // The file on disc has a dmResourceArchive::LiveUpdateResourceHeader prepended to it.
        // And we need to treat the whole blob as the resource
        info->m_Resource = resource->GetDataWithHeader();
        info->m_ResourceLength = resource->GetSizeWithHeader();
        info->m_ExpectedResourceDigest = expected_digest;
        info->m_ExpectedResourceDigestLength = expected_digest_length;
        info->m_Callback = callback;
        info->m_CallbackData = callback_data;

        bool res = dmLiveUpdate::PushAsyncJob((dmJobThread::FProcess)StoreResourceProcess, (dmJobThread::FCallback)StoreResourceFinished, (void*)&g_LiveUpdate, info);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    // ******************************************************************************************************************************************

    struct StoreManifestInfo
    {
        StoreManifestInfo() {
            memset(this, 0, sizeof(*this));
        }
        dmResourceProvider::HArchive            m_Archive;
        const uint8_t*                          m_Data; // points to external, ref-counted data from the Lua call. Released after the callback
        uint32_t                                m_DataLength;
        void                                    (*m_Callback)(int, void*);
        void*                                   m_CallbackData;
        uint8_t                                 m_Verify:1;
    };

    // Called on the worker thread
    static int StoreManifestProcess(LiveUpdateCtx* jobctx, StoreManifestInfo* job)
    {
        dmResource::Manifest* manifest = 0;
        dmResource::Result result = dmResource::LoadManifestFromBuffer(job->m_Data, job->m_DataLength, &manifest);

        if (result == dmResource::RESULT_OK)
        {
            if (job->m_Verify)
            {
                const char* public_key_path = dmResource::GetPublicKeyPath(g_LiveUpdate.m_ResourceFactory);
                result = dmResource::VerifyManifest(manifest, public_key_path);
                if (result != dmResource::RESULT_OK)
                {
                    dmLogError("Manifest verification failed. Manifest was not stored. %d %s", result, dmResource::ResultToString(result));
                }

                dmLogWarning("Currently disabled verification of existance of resources in liveupdate archive");
                //result = dmResource::VerifyResourcesBundled(dmResourceArchive::HArchiveIndexContainer archive, const dmResource::Manifest* manifest);

                // result = dmLiveUpdate::VerifyManifestReferences(manifest);
                // if (result != dmResource::RESULT_OK)
                // {
                //     dmLogError("Manifest references non existing resources. Manifest was not stored. %d %s", result, dmResource::ResultToString(result));
                // }
            }
            else {
                dmLogDebug("Skipping manifest validation");
            }
        }
        else
        {
            dmLogError("Failed to parse manifest, result: %s", dmResource::ResultToString(result));
        }

        // Store
        if (dmResource::RESULT_OK == result)
        {
            result = LegacyStoreManifestToMutableArchive(manifest);
        }
        dmResource::DeleteManifest(manifest);

        return result;
    }

    // Called on the main thread (see dmJobThread::Update below)
    static void StoreManifestFinished(LiveUpdateCtx* jobctx, StoreManifestInfo* job, int result)
    {
        dmLogInfo("Finishing manifest job");
        if (job->m_Callback)
            job->m_Callback(result, job->m_CallbackData);
        delete job;
    }

    Result StoreManifestAsync(const uint8_t* manifest_data, uint32_t manifest_len, void (*callback)(int, void*), void* callback_data)
    {
        if (!manifest_data || manifest_len == 0)
        {
            return RESULT_INVAL;
        }

        if(IsLiveupdateDisabled())
        {
            return RESULT_INVAL;
        }

        StoreManifestInfo* info = new StoreManifestInfo;
        info->m_Archive = g_LiveUpdate.m_LiveupdateArchive;
        // The file on disc has a dmResourceArchive::LiveUpdateResourceHeader prepended to it.
        // And we need to treat the whole blob as the resource
        info->m_Data = manifest_data;
        info->m_DataLength = manifest_len;
        info->m_Callback = callback;
        info->m_CallbackData = callback_data;
        info->m_Verify = true;

        bool res = dmLiveUpdate::PushAsyncJob((dmJobThread::FProcess)StoreManifestProcess, (dmJobThread::FCallback)StoreManifestFinished, (void*)&g_LiveUpdate, info);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    // ******************************************************************************************************************************************

    struct StoreArchiveInfo
    {
        StoreArchiveInfo() {
            memset(this, 0, sizeof(*this));
        }
        dmResourceProvider::HArchive    m_Archive;
        const char*                     m_Path; // The path to the zip file
        const char*                     m_Name; // The name of the mount
        void                            (*m_Callback)(const char*, int, void*);
        void*                           m_CallbackData;
        int                             m_Priority;
        uint8_t                         m_Verify:1;
    };

    // Called on the worker thread
    static int StoreArchiveProcess(LiveUpdateCtx* jobctx, StoreArchiveInfo* job)
    {
        if (job->m_Verify)
        {
            const char* public_key_path = dmResource::GetPublicKeyPath(g_LiveUpdate.m_ResourceFactory);
            dmResource::Result result = dmLiveUpdate::VerifyZipArchive(job->m_Path, public_key_path);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Zip archive verification failed. Archive was not stored. %d %s", result, dmResource::ResultToString(result));
                return RESULT_INVALID_RESOURCE;
            }
        }

        char archive_uri[DMPATH_MAX_PATH];
        dmSnPrintf(archive_uri, sizeof(archive_uri), "zip:%s", job->m_Path);

        dmURI::Parts uri;
        dmURI::Parse(archive_uri, &uri);

        dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64("zip"));
        if (!loader)
        {
            dmLogError("Failed to find 'mutable' loader");
            return RESULT_IO_ERROR;
        }

        dmResourceProvider::HArchive archive = 0;
        dmResourceProvider::Result result = dmResourceProvider::CreateMount(loader, &uri, g_LiveUpdate.m_ResourceBaseArchive, &archive);
        if (dmResourceProvider::RESULT_OK != result)
        {
            dmLogError("Failed to create new zip archive from '%s'", archive_uri);
            return RESULT_UNKNOWN;
        }

        if (archive)
        {
            dmResource::Result result = dmResourceMounts::AddMount(g_LiveUpdate.m_ResourceMounts, job->m_Name, archive, job->m_Priority, true);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Failed to mount zip archive: %s", dmResource::ResultToString(result));
            }
            else
            {
                // Flush the resource mounts to disc
                dmResourceMounts::SaveMounts(g_LiveUpdate.m_ResourceMounts, g_LiveUpdate.m_AppSupportPath);
            }
        }

        // Legacy. This makes less sense when using multiple mounted archives
        g_LiveUpdate.m_LiveupdateArchive = archive;

        return RESULT_OK;
    }

    // Called on the main thread (see dmJobThread::Update below)
    static void StoreArchiveFinished(LiveUpdateCtx* jobctx, StoreArchiveInfo* job, int result)
    {
        dmLogInfo("Finishing archive job: %d", result);
        if (job->m_Callback)
            job->m_Callback(job->m_Path, result, job->m_CallbackData);
        free((void*)job->m_Name);
        free((void*)job->m_Path);
        delete job;
    }

    Result StoreArchiveAsync(const char* path, void (*callback)(const char*, int, void*), void* callback_data, const char* mountname, int priority, bool verify_archive)
    {
        if (!dmSys::Exists(path)) {
            dmLogError("File does not exist: '%s'", path);
            return RESULT_INVALID_RESOURCE;
        }

        if(IsLiveupdateDisabled())
        {
            return RESULT_INVAL;
        }

        StoreArchiveInfo* info = new StoreArchiveInfo;
        info->m_Archive = g_LiveUpdate.m_LiveupdateArchive;
        info->m_Priority = priority;
        info->m_Name = strdup(mountname);
        info->m_Path = strdup(path);
        info->m_Callback = callback;
        info->m_CallbackData = callback_data;
        info->m_Verify = true;

        bool res = dmLiveUpdate::PushAsyncJob((dmJobThread::FProcess)StoreArchiveProcess, (dmJobThread::FCallback)StoreArchiveFinished, (void*)&g_LiveUpdate, info);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    // ******************************************************************
    // ** LiveUpdate add mount
    // ******************************************************************

    struct AddMountInfo
    {
        AddMountInfo() {
            memset(this, 0, sizeof(*this));
        }
        dmResourceProvider::HArchive    m_Archive;
        const char*                     m_Uri;
        const char*                     m_Name;
        void                            (*m_Callback)(const char*, const char*, int, void*);
        void*                           m_CallbackData;
        int                             m_Priority;
    };

    // Called on the worker thread
    static int AddMountProcess(LiveUpdateCtx* jobctx, AddMountInfo* job)
    {
        dmURI::Parts uri;
        dmURI::Parse(job->m_Uri, &uri);

        dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64(uri.m_Scheme));
        if (!loader)
        {
            dmLogError("Failed to find loader for scheme '%s'", uri.m_Scheme);
            return RESULT_IO_ERROR;
        }

        dmResourceProvider::HArchive archive = 0;
        dmResourceProvider::Result result = dmResourceProvider::CreateMount(loader, &uri, g_LiveUpdate.m_ResourceBaseArchive, &archive);
        if (dmResourceProvider::RESULT_OK != result)
        {
            dmLogError("Failed to create new mount from %s", job->m_Uri);
            return RESULT_UNKNOWN;
        }

        if (archive)
        {
            dmResource::Result result = dmResourceMounts::AddMount(g_LiveUpdate.m_ResourceMounts, job->m_Name, archive, job->m_Priority, true);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Failed to add mount for '%s': %s", job->m_Uri, dmResource::ResultToString(result));
            }
            else
            {
                // Flush the resource mounts to disc
                dmResourceMounts::SaveMounts(g_LiveUpdate.m_ResourceMounts, g_LiveUpdate.m_AppSupportPath);
            }
        }

        return RESULT_OK;
    }

    // Called on the main thread (see dmJobThread::Update below)
    static void AddMountFinished(LiveUpdateCtx* jobctx, AddMountInfo* job, int result)
    {
        dmLogInfo("Finishing add mount job: %d", result);
        if (job->m_Callback)
            job->m_Callback(job->m_Name, job->m_Uri, result, job->m_CallbackData);
        free((void*)job->m_Name);
        free((void*)job->m_Uri);
        delete job;
    }

    Result AddMountAsync(const char* name, const char* uri, int priority, void (*callback)(const char*, const char*, int, void*), void* callback_data)
    {
        if(IsLiveupdateDisabled())
        {
            return RESULT_INVAL;
        }

        AddMountInfo* info = new AddMountInfo;
        info->m_Archive = g_LiveUpdate.m_LiveupdateArchive;
        info->m_Priority = priority;
        info->m_Name = strdup(name);
        info->m_Uri = strdup(uri);
        info->m_Callback = callback;
        info->m_CallbackData = callback_data;

        bool res = dmLiveUpdate::PushAsyncJob((dmJobThread::FProcess)AddMountProcess, (dmJobThread::FCallback)AddMountFinished, (void*)&g_LiveUpdate, info);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    // ******************************************************************
    // ** LiveUpdate utility functions
    // ******************************************************************

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
    // **
    // ******************************************************************

    bool HasLiveUpdateMount()
    {
        dmMutex::HMutex mutex = dmResourceMounts::GetMutex(g_LiveUpdate.m_ResourceMounts);
        DM_MUTEX_SCOPED_LOCK(mutex);

        uint32_t count = dmResourceMounts::GetNumMounts(g_LiveUpdate.m_ResourceMounts);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmResourceMounts::SGetMountResult info;
            dmResource::Result result = dmResourceMounts::GetMountByIndex(g_LiveUpdate.m_ResourceMounts, i, &info);
            if (dmResource::RESULT_OK == result)
            {
                if (info.m_Priority >= 0)
                {
                    return true;
                }
            }
        }
        return false;
    }

    // ******************************************************************
    // ** LiveUpdate life cycle functions
    // ******************************************************************

    static dmExtension::Result InitializeLegacy(dmExtension::Params* params)
    {
        g_LiveUpdate.m_LiveupdateArchive = FindLiveupdateArchiveMount(g_LiveUpdate.m_ResourceMounts, LIVEUPDATE_LEGACY_MOUNT_NAME);
        if (!g_LiveUpdate.m_LiveupdateArchive)
        {
            g_LiveUpdate.m_LiveupdateArchive = LegacyLoadArchive(g_LiveUpdate.m_ResourceMounts, g_LiveUpdate.m_ResourceBaseArchive, g_LiveUpdate.m_AppSupportPath);
        }

        if (!g_LiveUpdate.m_LiveupdateArchive)
        {
            dmLogDebug("No liveupdate mount or archive found at startup");
        }

        g_LiveUpdate.m_LiveupdateArchiveManifest = 0;
        if (g_LiveUpdate.m_LiveupdateArchive)
        {
            dmResourceProvider::GetManifest(g_LiveUpdate.m_LiveupdateArchive, &g_LiveUpdate.m_LiveupdateArchiveManifest);
        }
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        dmResource::HFactory factory = params->m_ResourceFactory;

        if (params->m_L) // TODO: until unit tests have been updated with a Lua context
            ScriptInit(params->m_L, factory);

        g_LiveUpdate.m_ResourceFactory = factory;
        g_LiveUpdate.m_ResourceMounts = dmResource::GetMountsContext(factory);
        g_LiveUpdate.m_ResourceBaseArchive = GetBaseArchive(factory);

        if (!g_LiveUpdate.m_ResourceBaseArchive)
        {
            dmLogInfo("Could not find base .arci/.arcd. Liveupdate disabled");
            return dmExtension::RESULT_OK;
        }

        dmResource::HManifest manifest;
        dmResourceProvider::Result p_result = dmResourceProvider::GetManifest(g_LiveUpdate.m_ResourceBaseArchive, &manifest);
        if (dmResourceProvider::RESULT_OK != p_result)
        {
            dmLogError("Could not get base archive manifest project id. Liveupdate disabled");
            return dmExtension::RESULT_OK;
        }

        dmResource::Result r_result = dmResource::GetApplicationSupportPath(manifest, g_LiveUpdate.m_AppSupportPath, sizeof(g_LiveUpdate.m_AppSupportPath));
        if (dmResource::RESULT_OK != r_result)
        {
            dmLogError("Could not determine liveupdate folder. Liveupdate disabled");
            return dmExtension::RESULT_OK;
        }

        dmLogInfo("Liveupdate folder located at: %s", g_LiveUpdate.m_AppSupportPath);

        g_LiveUpdate.m_JobThread = dmJobThread::Create("liveupdate_jobs");

        // initialize legacy mode
        InitializeLegacy(params);

        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params* params)
    {
        if (g_LiveUpdate.m_JobThread)
            dmJobThread::Destroy(g_LiveUpdate.m_JobThread);
        g_LiveUpdate.m_JobThread = 0;
        g_LiveUpdate.m_ResourceFactory = 0;
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params* params)
    {
        if (!g_LiveUpdate.m_ResourceBaseArchive)
            return dmExtension::RESULT_OK;

        DM_PROFILE("LiveUpdate");
        dmJobThread::Update(g_LiveUpdate.m_JobThread); // Flushes finished async jobs', and calls any Lua callbacks
        return dmExtension::RESULT_OK;
    }
};

DM_DECLARE_EXTENSION(LiveUpdateExt, "LiveUpdate", 0, 0, dmLiveUpdate::Initialize, dmLiveUpdate::Update, 0, dmLiveUpdate::Finalize)
