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

#include <resource/resource.h>
#include <resource/resource_archive.h>
#include <dlib/log.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/condition_variable.h>
#include <dlib/array.h>
#include <dlib/zip.h>


namespace dmLiveUpdate
{
    /// Resource system factory, used for async locking of load mutex
    static dmResource::HFactory m_ResourceFactory = 0x0;
    /// How many elements to offset queue capacity with when full
    static const uint32_t m_JobQueueSizeIncrement = 32;

    /// The liveupdate thread and synchronization objects, used for sequentially processing async liveupdate resource requests
    static dmThread::Thread m_AsyncThread = 0x0;
    static dmMutex::HMutex  m_ConsumerThreadMutex;
    static dmConditionVariable::HConditionVariable m_ConsumerThreadCondition;
    static volatile bool m_ThreadJobComplete = false;
    static volatile bool m_Active = false;

    /// job input and output queues
    static dmArray<AsyncResourceRequest> m_JobQueue;
    static dmArray<AsyncResourceRequest> m_ThreadJobQueue;
    static AsyncResourceRequest m_TreadJob;
    static ResourceRequestCallbackData m_JobCompleteData;

    static Result VerifyArchive(const char* path)
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

    static Result StoreArchive(const char* path)
    {

        // Move zip file to "liveupdate.zip"
        // remove "liveupdate.dmanif" (LIVEUPDATE_MANIFEST_FILENAME)

        // also make sure the opposite code path does the same


        dmLogInfo("Archive Stored OK");

        return RESULT_OK;
    }

    static void ProcessRequest(AsyncResourceRequest &request)
    {
        m_JobCompleteData.m_CallbackData = request.m_CallbackData;
        m_JobCompleteData.m_Callback = request.m_Callback;
        m_JobCompleteData.m_Status = false;
        Result res = dmLiveUpdate::RESULT_OK;
        if (request.m_IsArchive)
        {
            res = VerifyArchive(request.m_Path);
            if (RESULT_OK == res)
                res = StoreArchive(request.m_Path);
            m_JobCompleteData.m_ArchiveIndexContainer = 0;
        }
        else if (request.m_Resource.m_Header != 0x0)
        {
            res = dmLiveUpdate::NewArchiveIndexWithResource(request.m_Manifest, request.m_ExpectedResourceDigest, request.m_ExpectedResourceDigestLength, &request.m_Resource, m_JobCompleteData.m_NewArchiveIndex);
            m_JobCompleteData.m_ArchiveIndexContainer = request.m_Manifest->m_ArchiveIndex;
        }
        else
        {
            res = dmLiveUpdate::RESULT_INVALID_HEADER;
        }
        m_JobCompleteData.m_Status = res == dmLiveUpdate::RESULT_OK ? true : false;
    }

    // Must be called on the Lua main thread
    static void ProcessRequestComplete()
    {
        if(m_JobCompleteData.m_ArchiveIndexContainer && m_JobCompleteData.m_Status)
        {
            dmLiveUpdate::SetNewArchiveIndex(m_JobCompleteData.m_ArchiveIndexContainer, m_JobCompleteData.m_NewArchiveIndex, true);
        }
        m_JobCompleteData.m_Callback(m_JobCompleteData.m_Status, m_JobCompleteData.m_CallbackData);
    }


#if !(defined(__EMSCRIPTEN__))
    static void AsyncThread(void* args)
    {
        (void)args;

        // Liveupdate async thread batch processing requested liveupdate tasks
        AsyncResourceRequest request;
        while (m_Active)
        {
            // Lock and sleep until signaled there is requests queued up
            {
                dmMutex::ScopedLock lk(m_ConsumerThreadMutex);
                while(m_ThreadJobQueue.Empty())
                    dmConditionVariable::Wait(m_ConsumerThreadCondition, m_ConsumerThreadMutex);
                if((m_ThreadJobComplete) || (!m_Active))
                    continue;
                request = m_ThreadJobQueue.Back();
                m_ThreadJobQueue.Pop();
            }
            ProcessRequest(request);
            m_ThreadJobComplete = true;
        }
    }

    void AsyncUpdate()
    {
        if(m_Active && ((m_ThreadJobComplete) || (!m_JobQueue.Empty())))
        {
            // Process any completed jobs, lock resource loadmutex as we will swap (update) archive containers archiveindex data
            dmMutex::ScopedLock lk(m_ConsumerThreadMutex);
            if(m_ThreadJobComplete)
            {
                dmMutex::HMutex mutex = dmResource::GetLoadMutex(m_ResourceFactory);
                if(!dmMutex::TryLock(mutex))
                    return;
                ProcessRequestComplete();
                dmMutex::Unlock(mutex);
                m_ThreadJobComplete = false;
            }
            // Push any accumulated request queue batch to job queue
            if(!m_JobQueue.Empty())
            {
                if(m_ThreadJobQueue.Remaining() < m_JobQueue.Size())
                {
                    m_ThreadJobQueue.SetCapacity(m_ThreadJobQueue.Size() + m_JobQueue.Size() + m_JobQueueSizeIncrement);
                }
                m_ThreadJobQueue.PushArray(m_JobQueue.Begin(), m_JobQueue.Size());
                m_JobQueue.SetSize(0);
            }
            // Either conditions should signal worker thread to check for new jobs
            dmConditionVariable::Signal(m_ConsumerThreadCondition);
        }
    }

    bool AddAsyncResourceRequest(AsyncResourceRequest& request)
    {
        // Add single job to job queue that will be batch pushed to thread worker in AsyncUpdate
        if(!m_Active)
            return false;
        if(m_JobQueue.Full())
        {
            m_JobQueue.OffsetCapacity(m_JobQueueSizeIncrement);
        }
        m_JobQueue.Push(request);
        return true;
    }

    void AsyncInitialize(const dmResource::HFactory factory)
    {
        m_ResourceFactory = factory;
        m_JobQueue.SetCapacity(m_JobQueueSizeIncrement);
        m_JobQueue.SetSize(0);
        m_ThreadJobQueue.SetCapacity(m_JobQueueSizeIncrement);
        m_ThreadJobQueue.SetSize(0);
        m_ConsumerThreadMutex = dmMutex::New();
        m_ConsumerThreadCondition = dmConditionVariable::New();
        m_ThreadJobComplete = false;
        m_Active = true;
        m_AsyncThread = dmThread::New(AsyncThread, 0x80000, 0, "liveupdate");
    }

    void AsyncFinalize()
    {
        if(m_AsyncThread)
        {
            // When shutting down, discard any complete jobs as they are going to be mounted at boot time in any case
            m_Active = false;
            dmMutex::Lock(m_ConsumerThreadMutex);
            m_JobQueue.SetSize(0);
            m_ThreadJobQueue.SetSize(1);
            dmConditionVariable::Signal(m_ConsumerThreadCondition);
            dmMutex::Unlock(m_ConsumerThreadMutex);
            dmThread::Join(m_AsyncThread);
            dmConditionVariable::Delete(m_ConsumerThreadCondition);
            dmMutex::Delete(m_ConsumerThreadMutex);
            m_AsyncThread = 0;
        }
    }

#else

    void AsyncUpdate()
    {
        if(!m_JobQueue.Empty())
        {
            ProcessRequest(m_JobQueue.Back());
            m_JobQueue.Pop();
            ProcessRequestComplete();
        }
    }

    bool AddAsyncResourceRequest(AsyncResourceRequest& request)
    {
        // Add single job to job queue that will be batch pushed to thread worker in AsyncUpdate
        if(!m_Active)
            return false;
        if(m_JobQueue.Full())
        {
            m_JobQueue.OffsetCapacity(m_JobQueueSizeIncrement);
        }
        m_JobQueue.Push(request);
        return true;
    }

    void AsyncInitialize(const dmResource::HFactory factory)
    {
        m_ResourceFactory = factory;
        m_Active = true;
    }

    void AsyncFinalize()
    {
        m_Active = false;
        m_JobQueue.SetSize(0);
    }

#endif

};
