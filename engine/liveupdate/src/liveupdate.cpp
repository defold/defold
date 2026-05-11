// Copyright 2020-2026 The Defold Foundation
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
#include "script_liveupdate.h"

#include <stdlib.h>
#include <string.h>

#include <dlib/jobsystem.h>
#include <dlib/log.h>
#include <dmsdk/dlib/configfile.h>
#include <dmsdk/dlib/profile.h>
#include <dmsdk/extension/extension.hpp>

#include <resource/providers/provider.h>
#include <resource/resource.h>
#include <resource/resource_manifest.h>
#include <resource/resource_mounts.h>

namespace dmLiveUpdate
{
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
            DM_LU_RESULT_TO_STR(NOT_INITIALIZED);
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

        dmResourceMounts::HContext      m_ResourceMounts;                 // The resource mounts
        dmResourceProvider::HArchive    m_ResourceBaseArchive;            // The "game.arcd" archive
        HJobContext                     m_JobContext;                     // Worker queue for async mount operations
        bool                            m_IsEnabled;
        bool                            m_MainContextHasExcludedResources; // Bundled manifest has one or more excluded resources
    } g_LiveUpdate;

    static bool IsLiveupdateEnabled()
    {
        return g_LiveUpdate.m_IsEnabled;
    }

    static bool IsLiveupdateThreadDisabled()
    {
        if (!g_LiveUpdate.m_JobContext)
        {
            dmLogError("Liveupdate function can't be called. Liveupdate disabled");
            return true;
        }
        return false;
    }

    static bool PushAsyncJob(FJobProcess process, FJobCallback callback, void* jobctx, void* jobdata)
    {
        Job job = {0};
        job.m_Process = process;
        job.m_Callback = callback;
        job.m_Context = jobctx;
        job.m_Data = jobdata;
        HJob hjob = JobSystemCreateJob(g_LiveUpdate.m_JobContext, &job);
        JobSystemPushJob(g_LiveUpdate.m_JobContext, hjob);
        return true;
    }

    // ******************************************************************
    // ** LiveUpdate add mount
    // ******************************************************************

    typedef void (*FAddMountCallback)(dmhash_t, const char*, int, void*);

    struct AddMountInfo
    {
        AddMountInfo()
        {
            memset(this, 0, sizeof(*this));
        }

        const char*       m_Uri;          // URI of the archive to mount
        dmhash_t          m_NameHash;     // Mount name, hashed at the scripting boundary
        FAddMountCallback m_Callback;
        void*             m_CallbackData;
        int               m_Priority;
    };

    // Called on the worker thread
    static int AddMountProcess(HJobContext context, HJob hjob, LiveUpdateCtx* jobctx, AddMountInfo* job)
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
        dmResourceProvider::Result result = dmResourceProvider::CreateMount(loader, &uri, jobctx->m_ResourceBaseArchive, &archive);
        if (dmResourceProvider::RESULT_OK != result)
        {
            dmLogError("Failed to create new mount from %s", job->m_Uri);
            return RESULT_UNKNOWN;
        }

        if (archive)
        {
            dmResource::Result result = dmResourceMounts::AddMount(jobctx->m_ResourceMounts, job->m_NameHash, archive, job->m_Priority);
            if (dmResource::RESULT_OK != result)
            {
                dmLogError("Failed to add mount for '%s': %s", job->m_Uri, dmResource::ResultToString(result));
            }
        }

        return RESULT_OK;
    }

    // Called on the main thread (see JobSystemUpdate below)
    static void AddMountFinished(HJobContext context, HJob hjob, JobSystemStatus status, LiveUpdateCtx* jobctx, AddMountInfo* job, int result)
    {
        dmLogInfo("Finishing add mount job: %d", result);
        if (job->m_Callback)
            job->m_Callback(job->m_NameHash, job->m_Uri, result, job->m_CallbackData);
        free((void*)job->m_Uri);
        delete job;
    }

    Result AddMountAsync(dmhash_t name_hash, const char* uri, int priority, void (*callback)(dmhash_t, const char*, int, void*), void* callback_data)
    {
        if (!IsLiveupdateEnabled())
            return RESULT_NOT_INITIALIZED;
        if (IsLiveupdateThreadDisabled())
            return RESULT_NOT_INITIALIZED;

        AddMountInfo* info = new AddMountInfo;
        info->m_Priority     = priority;
        info->m_NameHash     = name_hash;
        info->m_Uri          = strdup(uri);
        info->m_Callback     = callback;
        info->m_CallbackData = callback_data;

        bool res = dmLiveUpdate::PushAsyncJob((FJobProcess)AddMountProcess, (FJobCallback)AddMountFinished, (void*)&g_LiveUpdate, info);
        return res == true ? RESULT_OK : RESULT_INVALID_RESOURCE;
    }

    Result RemoveMountSync(dmhash_t name_hash)
    {
        if (!IsLiveupdateEnabled())
            return RESULT_NOT_INITIALIZED;

        dmResourceMounts::HContext mounts = g_LiveUpdate.m_ResourceMounts;
        dmMutex::HMutex mutex = dmResourceMounts::GetMutex(mounts);
        DM_MUTEX_SCOPED_LOCK(mutex);

        dmResource::Result result = dmResourceMounts::RemoveAndUnmountByNameHash(mounts, name_hash);
        if (result != dmResource::RESULT_OK)
        {
            dmLogError("Failed to remove mount " DM_HASH_FMT ": %s (%d)", name_hash, dmResource::ResultToString(result), result);
            return dmLiveUpdate::ResourceResultToLiveupdateResult(result);
        }

        return RESULT_OK;
    }

    bool IsBuiltWithExcludedFiles()
    {
        return g_LiveUpdate.m_MainContextHasExcludedResources;
    }

    // ******************************************************************
    // ** LiveUpdate life cycle functions
    // ******************************************************************

    static dmExtension::Result Initialize(dmExtension::Params* params)
    {
        int32_t liveupdate_enabled = ConfigFileGetInt(params->m_ConfigFile, "liveupdate.enabled", 1);
        if (!liveupdate_enabled)
        {
            dmLogError("Liveupdate disabled due to project setting %s=%d", "liveupdate.enabled", liveupdate_enabled);
            return dmExtension::RESULT_OK;
        }
        g_LiveUpdate.m_IsEnabled = true;

        // We initialize scripting first, as we might want to use file/http providers
        JobSystemCreateParams job_thread_create_param = {0};
        job_thread_create_param.m_ThreadNamePrefix  = "liveupdate";
        job_thread_create_param.m_ThreadCount       = 1;

        g_LiveUpdate.m_JobContext = JobSystemCreate(&job_thread_create_param);

        dmResource::HFactory factory = (dmResource::HFactory)params->m_ResourceFactory;

        if (g_LiveUpdate.m_JobContext) // Make the liveupdate module `nil` if it isn't available
        {
            if (params->m_L) // TODO: until unit tests have been updated with a Lua context
                ScriptInit(params->m_L, factory);
        }

        g_LiveUpdate.m_ResourceMounts = dmResource::GetMountsContext(factory);
        g_LiveUpdate.m_ResourceBaseArchive = dmResource::GetBaseArchive(factory);
        g_LiveUpdate.m_MainContextHasExcludedResources = false;

        if (g_LiveUpdate.m_ResourceBaseArchive)
        {
            dmResource::HManifest manifest;
            dmResourceProvider::Result p_result = dmResourceProvider::GetManifest(g_LiveUpdate.m_ResourceBaseArchive, &manifest);
            if (dmResourceProvider::RESULT_OK != p_result)
            {
                dmLogError("Could not get base archive manifest. Liveupdate disabled");
                return dmExtension::RESULT_OK;
            }

            g_LiveUpdate.m_MainContextHasExcludedResources = dmResource::HasManifestExcludedEntries(manifest);
        }

        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Finalize(dmExtension::Params* params)
    {
        if (!IsLiveupdateEnabled())
            return dmExtension::RESULT_OK;

        if (g_LiveUpdate.m_JobContext)
            JobSystemDestroy(g_LiveUpdate.m_JobContext);
        g_LiveUpdate.m_JobContext = 0;
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result Update(dmExtension::Params* params)
    {
        if (!IsLiveupdateEnabled())
            return dmExtension::RESULT_OK;

        DM_PROFILE("LiveUpdate");
        if (g_LiveUpdate.m_JobContext)
            JobSystemUpdate(g_LiveUpdate.m_JobContext, 0); // Flushes finished async jobs, and calls any Lua callbacks
        return dmExtension::RESULT_OK;
    }
};

DM_DECLARE_EXTENSION(LiveUpdateExt, "LiveUpdate", 0, 0, dmLiveUpdate::Initialize, dmLiveUpdate::Update, 0, dmLiveUpdate::Finalize)
