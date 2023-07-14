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
#include <resource/resource_util.h>     // BytesToHexString for debug printing
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

    struct LiveUpdateCtx
    {
        LiveUpdateCtx()
        {
            memset(this, 0x0, sizeof(*this));
        }

        char                            m_AppSupportPath[DMPATH_MAX_PATH]; // app support path using the project id as the folder name
        dmResource::HFactory            m_ResourceFactory;      // Resource system factory
        dmResourceMounts::HContext      m_ResourceMounts;       // The resource mounts
        dmResourceProvider::HArchive    m_ResourceBaseArchive;  // The "game.arcd" archive
        dmResourceProvider::HArchive    m_LiveupdateArchive;
        dmResource::HManifest           m_LiveupdateArchiveManifest;

        dmJobThread::HContext           m_JobThread;
    };

    LiveUpdateCtx g_LiveUpdate;

    // ******************************************************************
    // ** LiveUpdate legacy functions
    // ******************************************************************

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
    // TODO: Only do this until we persist the mounts
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

    struct ResourceInfo
    {
        ResourceInfo() {
            memset(this, 0, sizeof(*this));
        }
        dmResourceProvider::HArchive            m_Archive;
        dmResourceArchive::LiveUpdateResource   m_Resource; // points to external, ref-counted data
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
        dmLogInfo("Processing resource job '%s'", job->m_ExpectedResourceDigest);

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

        dmLogInfo("  url_hash '%s' -> %016llx", job->m_ExpectedResourceDigest, url_hash);

        if (!url_hash)
            return 0;

        // The path, is actually just a filename when it comes to these resources
        dmResourceProvider::Result result = dmResourceProvider::WriteFile(jobctx->m_LiveupdateArchive, url_hash, job->m_ExpectedResourceDigest,
                                                        (uint8_t*)&job->m_Resource, job->m_Resource.GetSizeWithHeader());

        return dmResourceProvider::RESULT_OK == result;
    }
    // Called on the main thread (see dmJobThread::Update below)
    static void StoreResourceFinished(LiveUpdateCtx* jobctx, ResourceInfo* job, int result)
    {
        dmLogInfo("Finishing resource job '%s'", job->m_ExpectedResourceDigest);

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

        ResourceInfo* info = new ResourceInfo;
        info->m_Archive = g_LiveUpdate.m_LiveupdateArchive;
        info->m_Resource.Set(*resource);
        info->m_ExpectedResourceDigest = expected_digest;
        info->m_ExpectedResourceDigestLength = expected_digest_length;
        info->m_Callback = callback;
        info->m_CallbackData = callback_data;

        bool res = dmLiveUpdate::PushAsyncJob((dmJobThread::FProcess)StoreResourceProcess, (dmJobThread::FCallback)StoreResourceFinished, (void*)&g_LiveUpdate, info);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    Result StoreArchiveAsync(const char* path, void (*callback)(bool, void*), void* callback_data, bool verify_archive)
    {
        // if (!dmSys::Exists(path)) {
        //     dmLogError("File does not exist: '%s'", path);
        //     return RESULT_INVALID_RESOURCE;
        // }

        // AsyncResourceRequest request;
        // request.m_CallbackData = callback_data;
        // request.m_Callback = callback;
        // request.m_Path = path;
        // request.m_IsArchive = 1;
        // request.m_VerifyArchive = verify_archive;
        // //request.m_Manifest = dmResource::GetManifest(g_LiveUpdate.m_ResourceFactory);
        // dmResourceProvider::GetManifest(g_LiveUpdate.m_ResourceBaseArchive, &request.m_Manifest);

        // bool res = AddAsyncResourceRequest(request);
        // return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
        return RESULT_INVALID_RESOURCE;
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

        g_LiveUpdate.m_LiveupdateArchiveManifest = 0;
        if (g_LiveUpdate.m_LiveupdateArchive)
        {
            dmResourceProvider::GetManifest(g_LiveUpdate.m_LiveupdateArchive, &g_LiveUpdate.m_LiveupdateArchiveManifest);
        }

        //dmLiveUpdate::AsyncInitialize(factory);
        g_LiveUpdate.m_JobThread = dmJobThread::Create("liveupdate_jobs");

        if (params->m_L) // TODO: until unit tests have been updated with a Lua context
            ScriptInit(params->m_L, factory);
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params* params)
    {
        dmJobThread::Destroy(g_LiveUpdate.m_JobThread);
        g_LiveUpdate.m_ResourceFactory = 0;
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params* params)
    {
        DM_PROFILE("LiveUpdate");
        dmJobThread::Update(g_LiveUpdate.m_JobThread); // Flushes finished async jobs', and calls any Lua callbacks
        return dmExtension::RESULT_OK;
    }
};

DM_DECLARE_EXTENSION(LiveUpdateExt, "LiveUpdate", 0, 0, dmLiveUpdate::Initialize, dmLiveUpdate::Update, 0, dmLiveUpdate::Finalize)
