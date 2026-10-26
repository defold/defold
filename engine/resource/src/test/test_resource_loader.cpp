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

#include <stdlib.h>
#include <string.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/jobsystem.h>
#include <dlib/log.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#if defined(DM_HAS_THREADS)
#include <dlib/condition_variable.h>
#endif
#include "../resource.h"
#include "../resource_private.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

struct LoadedNode
{
    uint32_t m_Value;
    dmArray<void*> m_Dependencies;
};

class ResourceLoaderTest;

struct RecreateCaller
{
    ResourceLoaderTest* m_Test;
    uint32_t m_Mode;
    dmResource::Result m_Recreated;
    dmResource::Result m_Loaded;
};

class ResourceLoaderTest : public jc_test_params_class<int>
{
    public:
    void SetUp() override
    {
        m_Mutex = dmMutex::New();
#if defined(DM_HAS_THREADS)
        m_Changed = dmConditionVariable::New();
#endif
        m_Jobs = 0;
        m_Requests.SetSize(0);
        m_Transfer = false;
        m_Hints = true;
        m_Block = ~0u;
        m_Blocked = false;
        m_AllowBlocked = false;
        m_Decoded = 0;
        m_CheckLocks = false;
        m_GetDuringDestroy = false;
        m_CallbacksUnlocked = true;
        m_CreateDepth = 0;
        m_MaxCreateDepth = 0;
        m_HintOnly = ~0u;
        m_Transitive = false;
        m_PostReadsParent = false;
        m_PostLoadsSibling = false;
        m_SignalPost = false;
        m_RecreateFromPost = ~0u;
        m_RecreateResult = dmResource::RESULT_PENDING;
        m_PostPendingOnce = false;
        m_HoldPosts = false;
        m_ReplacePointer = false;
        m_CheckPostIdentity = false;
        m_PostIdentityCorrect = true;
        m_HeldPost = ~0u;
        m_FailedPost = ~0u;
        m_FailedPreload = ~0u;
        m_CheckedThreadNode = ~0u;
        m_SignalCreate = ~0u;
        m_WaitInCreate = false;
        m_ExpectedThread = dmThread::GetCurrentThread();
        m_CreationThreadCorrect = true;
        m_JobCallbackThreadCorrect = true;
        m_JobCallbacks = 0;
        m_JobReadsRoot = false;
        m_JobGetResult = dmResource::RESULT_PENDING;
        m_GetJobsStarted = 0;
        m_GetJobsFinished = 0;
        m_PreloadFromJob = false;
        m_Fences = 0;
        m_DecodedBytes = 0;
        memset(m_Preloads, 0, sizeof(m_Preloads));
        memset(m_Creates, 0, sizeof(m_Creates));
        memset(m_Destroys, 0, sizeof(m_Destroys));
        memset(m_Posts, 0, sizeof(m_Posts));
        memset(m_Notified, 0, sizeof(m_Notified));
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 8192;
        if (GetParam() >= 0)
        {
            JobSystemCreateParams jobs = {};
            jobs.m_ThreadCount = GetParam();
            m_Jobs = JobSystemCreate(&jobs);
            params.m_JobThreadContext = m_Jobs;
        }
        m_Factory = dmResource::NewFactory(&params, ".");
        ASSERT_NE((dmResource::HFactory)0, m_Factory);
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::RegisterType(m_Factory, "node", this, Preload, Create, 0, Destroy, Recreate));
    }

    void TearDown() override
    {
        Unblock();
        for (uint32_t i = 0; i < m_Requests.Size(); ++i)
            if (m_Requests[i])
                dmResource::DeletePreloader(m_Requests[i]);
        dmResource::DeleteFactory(m_Factory);
        JobSystemDestroy(m_Jobs);
        for (uint32_t i = 0; i < m_Files.Size(); ++i)
            free(m_Files[i]);
        m_Files.SetSize(0);
        ASSERT_EQ(0u, m_Decoded);
        ASSERT_EQ(0u, m_DecodedBytes);
#if defined(DM_HAS_THREADS)
        dmConditionVariable::Delete(m_Changed);
#endif
        dmMutex::Delete(m_Mutex);
    }

    static void Path(uint32_t id, char* path, uint32_t size)
    {
        dmSnPrintf(path, size, "/node_%u.node", id);
    }

    void Add(uint32_t id, const uint32_t* dependencies, uint32_t count)
    {
        AddSized(id, dependencies, count, (count + 2) * sizeof(uint32_t));
    }

    void AddSized(uint32_t id, const uint32_t* dependencies, uint32_t count, uint32_t bytes)
    {
        uint32_t* data = (uint32_t*)calloc(1, bytes);
        data[0] = id;
        data[1] = count;
        if (count)
            memcpy(data + 2, dependencies, count * sizeof(uint32_t));
        if (m_Files.Full())
            m_Files.OffsetCapacity(64);
        m_Files.Push(data);
        char path[64];
        Path(id, path, sizeof(path));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, path, bytes, data));
    }

    dmResource::HPreloader Request(uint32_t id)
    {
        char path[64];
        Path(id, path, sizeof(path));
        dmResource::HPreloader request = dmResource::NewPreloader(m_Factory, path);
        if (m_Requests.Full())
            m_Requests.OffsetCapacity(8);
        m_Requests.Push(request);
        return request;
    }

    void Delete(uint32_t index)
    {
        dmResource::DeletePreloader(m_Requests[index]);
        m_Requests[index] = 0;
    }

    dmResource::Result Complete(dmResource::HPreloader request)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        dmResource::Result result;
        do
        {
            result = dmResource::UpdatePreloader(request, 0, 0, 1000);
        } while (result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
        return result;
    }

    void Unblock()
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        m_AllowBlocked = true;
#if defined(DM_HAS_THREADS)
        dmConditionVariable::Signal(m_Changed);
#endif
    }

    static dmResource::Result Preload(const dmResource::ResourcePreloadParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
        const uint32_t* file = (const uint32_t*)params->m_Buffer;
        if (file[0] == self->m_FailedPreload)
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            ++self->m_Preloads[file[0]];
            return dmResource::RESULT_INVALID_DATA;
        }
        uint32_t* decoded;
        if (self->m_Transfer && params->m_IsBufferTransferrable)
        {
            decoded = (uint32_t*)params->m_Buffer;
            *params->m_IsBufferOwnershipTransferred = true;
        }
        else
        {
            decoded = (uint32_t*)new uint8_t[params->m_BufferSize];
            memcpy(decoded, file, params->m_BufferSize);
        }
        *params->m_PreloadData = decoded;
        if (self->m_Hints)
            for (uint32_t i = 0; i < decoded[1]; ++i)
            {
                char path[64];
                Path(decoded[i + 2], path, sizeof(path));
                dmResource::PreloadHint(params->m_HintInfo, path);
            }
        DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
        ++self->m_Preloads[decoded[0]];
        ++self->m_Decoded;
        self->m_DecodedBytes += params->m_BufferSize;
#if defined(DM_HAS_THREADS)
        if (decoded[0] == self->m_Block)
        {
            self->m_Blocked = true;
            dmConditionVariable::Signal(self->m_Changed);
            while (!self->m_AllowBlocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
        }
#endif
        return dmResource::RESULT_OK;
    }

    struct LockProbe
    {
        dmMutex::HMutex m_Mutex;
        bool m_Unlocked;
    };

    static void ProbeLock(void* user_data)
    {
        LockProbe* probe = (LockProbe*)user_data;
        probe->m_Unlocked = dmMutex::TryLock(probe->m_Mutex);
        if (probe->m_Unlocked)
            dmMutex::Unlock(probe->m_Mutex);
    }

    struct GetCaller
    {
        ResourceLoaderTest* m_Test;
        void* m_Resource;
        dmResource::Result m_Result;
        uint32_t m_Id;
    };

    static void GetFromThread(void* user_data)
    {
        GetCaller* caller = (GetCaller*)user_data;
#if defined(DM_HAS_THREADS)
        {
            DM_MUTEX_SCOPED_LOCK(caller->m_Test->m_Mutex);
            while (!caller->m_Test->m_AllowBlocked)
                dmConditionVariable::Wait(caller->m_Test->m_Changed, caller->m_Test->m_Mutex);
        }
#endif
        char path[64];
        Path(caller->m_Id, path, sizeof(path));
        caller->m_Result = dmResource::Get(caller->m_Test->m_Factory, path, &caller->m_Resource);
    }

    static int32_t GetFromJob(HJobContext, HJob, void* user_context, void* user_data)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)user_context;
        GetCaller* caller = (GetCaller*)user_data;
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            ++self->m_GetJobsStarted;
#if defined(DM_HAS_THREADS)
            dmConditionVariable::Broadcast(self->m_Changed);
            while (!self->m_AllowBlocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
#endif
        }
        char path[64];
        Path(caller->m_Id, path, sizeof(path));
        dmResource::HPreloader request = self->m_PreloadFromJob ? dmResource::NewPreloader(self->m_Factory, path) : 0;
        caller->m_Result = dmResource::Get(self->m_Factory, path, &caller->m_Resource);
        if (request)
            dmResource::DeletePreloader(request);
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            ++self->m_GetJobsFinished;
#if defined(DM_HAS_THREADS)
            dmConditionVariable::Broadcast(self->m_Changed);
#endif
        }
        return 0;
    }

    void StartGetJobs(GetCaller* callers, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            Job job = {};
            job.m_Process = GetFromJob;
            job.m_Callback = JobCallback;
            job.m_Context = this;
            job.m_Data = &callers[i];
            JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
        }
#if defined(DM_HAS_THREADS)
        if (JobSystemGetWorkerCount(m_Jobs))
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            while (m_GetJobsStarted != count)
                dmConditionVariable::Wait(m_Changed, m_Mutex);
        }
#endif
    }

    void FinishGetJobs(uint32_t count)
    {
        Unblock();
        if (!JobSystemGetWorkerCount(m_Jobs))
        {
            while (m_GetJobsFinished != count)
                JobSystemUpdate(m_Jobs, 0);
        }
#if defined(DM_HAS_THREADS)
        else
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            while (m_GetJobsFinished != count)
                dmConditionVariable::Wait(m_Changed, m_Mutex);
            ASSERT_EQ(0u, m_JobCallbacks);
        }
#endif
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (m_JobCallbacks != count && dmTime::GetMonotonicTime() < deadline)
            JobSystemUpdate(m_Jobs, 0);
        ASSERT_EQ(count, m_JobCallbacks);
        ASSERT_TRUE(m_JobCallbackThreadCorrect);
    }

    static dmResource::Result Create(const dmResource::ResourceCreateParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
        uint32_t* decoded = (uint32_t*)params->m_PreloadData;
        if (decoded[0] == self->m_CheckedThreadNode)
            self->m_CreationThreadCorrect = self->m_CreationThreadCorrect && dmThread::GetCurrentThread() == self->m_ExpectedThread;
#if defined(DM_HAS_THREADS)
        if (self->m_CheckLocks)
        {
            LockProbe probe = { dmResource::GetLoadMutex(params->m_Factory), false };
            dmThread::Thread thread = dmThread::New(ProbeLock, 0x8000, &probe, "ResourceProbe");
            dmThread::Join(thread);
            self->m_CallbacksUnlocked = self->m_CallbacksUnlocked && probe.m_Unlocked;
        }
#endif
        LoadedNode* resource = new LoadedNode();
        ++self->m_CreateDepth;
        if (self->m_CreateDepth > self->m_MaxCreateDepth)
            self->m_MaxCreateDepth = self->m_CreateDepth;
        resource->m_Value = decoded[0];
        ++self->m_Creates[decoded[0]];
        resource->m_Dependencies.SetCapacity(decoded[1] + 1);
        dmResource::Result result = dmResource::RESULT_OK;
#if defined(DM_HAS_THREADS)
        if (decoded[0] == self->m_SignalCreate)
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            self->m_Blocked = true;
            dmConditionVariable::Broadcast(self->m_Changed);
            while (self->m_WaitInCreate && !self->m_AllowBlocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
        }
#endif
        for (uint32_t i = 0; decoded[0] != self->m_HintOnly && i < decoded[1]; ++i)
        {
            char path[64];
            Path(decoded[i + 2], path, sizeof(path));
            void* dependency = 0;
            result = dmResource::Get(params->m_Factory, path, &dependency);
            if (result != dmResource::RESULT_OK)
                break;
            resource->m_Dependencies.Push(dependency);
        }
        if (result == dmResource::RESULT_OK && self->m_Transitive && decoded[0] == 0)
        {
            void* dependency = 0;
            result = dmResource::Get(params->m_Factory, "/node_2.node", &dependency);
            if (result == dmResource::RESULT_OK)
                resource->m_Dependencies.Push(dependency);
        }
        --self->m_CreateDepth;
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            --self->m_Decoded;
            self->m_DecodedBytes -= params->m_BufferSize;
        }
        delete[] (uint8_t*)decoded;
        if (result != dmResource::RESULT_OK)
        {
            for (uint32_t i = 0; i < resource->m_Dependencies.Size(); ++i)
                dmResource::Release(params->m_Factory, resource->m_Dependencies[i]);
            delete resource;
            return result;
        }
        ResourceDescriptorSetResource(params->m_Resource, resource);
        return result;
    }

    static dmResource::Result PostCreate(const dmResource::ResourcePostCreateParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
        LoadedNode* resource = (LoadedNode*)ResourceDescriptorGetResource(params->m_Resource);
        if (self->m_CheckPostIdentity)
        {
            HResourceDescriptor descriptor;
            dmResource::Result result = dmResource::GetDescriptor(params->m_Factory, params->m_Filename, &descriptor);
            if (result != dmResource::RESULT_OK || resource != ResourceDescriptorGetResource(descriptor))
            {
                self->m_PostIdentityCorrect = false;
                return dmResource::RESULT_INVALID_DATA;
            }
        }
        ++self->m_Posts[resource->m_Value];
        if (resource->m_Value == 0 && self->m_RecreateFromPost != ~0u)
            self->m_RecreateResult = self->Replace(0, 0, 0, self->m_RecreateFromPost);
        if (self->m_PostReadsParent && resource->m_Value == 1)
        {
            void* parent = 0;
            dmResource::Result result = dmResource::Get(params->m_Factory, "/node_0.node", &parent);
            if (result != dmResource::RESULT_OK)
                return result;
            dmResource::Release(params->m_Factory, parent);
        }
        if (self->m_PostLoadsSibling && resource->m_Value == 0)
        {
            self->m_HoldPosts = false;
#if defined(DM_HAS_THREADS)
            if (self->m_SignalPost)
            {
                DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
                self->m_Blocked = true;
                dmConditionVariable::Broadcast(self->m_Changed);
            }
#endif
            void* sibling = 0;
            dmResource::Result result = dmResource::Get(params->m_Factory, "/node_2.node", &sibling);
            if (result != dmResource::RESULT_OK)
                return result;
            dmResource::Release(params->m_Factory, sibling);
        }
        if (self->m_HoldPosts || resource->m_Value == self->m_HeldPost)
            return dmResource::RESULT_PENDING;
        if (self->m_PostPendingOnce && self->m_Posts[resource->m_Value] == 1)
            return dmResource::RESULT_PENDING;
        return resource->m_Value == self->m_FailedPost ? dmResource::RESULT_INVALID_DATA : dmResource::RESULT_OK;
    }

    static dmResource::Result Recreate(const dmResource::ResourceRecreateParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
        const uint32_t* data = (const uint32_t*)(params->m_Message ? params->m_Message : params->m_Buffer);
        LoadedNode* resource = (LoadedNode*)ResourceDescriptorGetResource(params->m_Resource);
#if defined(DM_HAS_THREADS)
        if (self->m_CheckLocks)
        {
            LockProbe probe = { dmResource::GetLoadMutex(params->m_Factory), false };
            dmThread::Thread thread = dmThread::New(ProbeLock, 0x8000, &probe, "RecreateProbe");
            dmThread::Join(thread);
            self->m_CallbacksUnlocked = self->m_CallbacksUnlocked && probe.m_Unlocked;
        }
        if (data[0] == self->m_SignalCreate)
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            self->m_Blocked = true;
            dmConditionVariable::Broadcast(self->m_Changed);
        }
#endif
        dmArray<void*> dependencies;
        dependencies.SetCapacity(data[1]);
        dmResource::Result result = dmResource::RESULT_OK;
        for (uint32_t i = 0; i < data[1]; ++i)
        {
            char path[64];
            Path(data[i + 2], path, sizeof(path));
            void* dependency = 0;
            result = dmResource::Get(params->m_Factory, path, &dependency);
            if (result != dmResource::RESULT_OK)
                break;
            dependencies.Push(dependency);
        }
        if (result == dmResource::RESULT_OK && self->m_ReplacePointer)
        {
            LoadedNode* replacement = new LoadedNode();
            replacement->m_Value = resource->m_Value;
            dependencies.Swap(replacement->m_Dependencies);
            ResourceDescriptorSetResource(params->m_Resource, replacement);
            ResourceDescriptorSetPrevResource(params->m_Resource, resource);
        }
        else if (result == dmResource::RESULT_OK)
            dependencies.Swap(resource->m_Dependencies);
        for (uint32_t i = 0; i < dependencies.Size(); ++i)
            dmResource::Release(params->m_Factory, dependencies[i]);
        return result;
    }

    dmResource::Result Replace(uint32_t id, const uint32_t* dependencies, uint32_t count, uint32_t mode)
    {
        char path[64];
        Path(id, path, sizeof(path));
        dmArray<uint32_t> data;
        data.SetCapacity(count + 2);
        data.Push(id);
        data.Push(count);
        for (uint32_t i = 0; i < count; ++i)
            data.Push(dependencies[i]);
        if (mode == 0)
        {
            dmResource::RemoveFile(m_Factory, path);
            Add(id, dependencies, count);
            return dmResource::ReloadResource(m_Factory, path, 0);
        }
        if (mode == 1)
            return dmResource::SetResource(m_Factory, dmHashString64(path), data.Begin(), data.Size() * sizeof(uint32_t));
        return dmResource::SetResource(m_Factory, dmHashString64(path), data.Begin());
    }

    static void RecreateFromThread(void* user_data)
    {
        GetCaller* caller = (GetCaller*)user_data;
        uint32_t dependency = 1;
        caller->m_Result = caller->m_Test->Replace(0, &dependency, 1, 1);
    }

#if defined(DM_HAS_THREADS)
    static void RecreateActiveResourceFromThread(void* user_data)
    {
        RecreateCaller* caller = (RecreateCaller*)user_data;
        caller->m_Recreated = caller->m_Test->Replace(0, 0, 0, caller->m_Mode);
    }

    static dmResource::Result PostCreateWithJoinedRecreate(const dmResource::ResourcePostCreateParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
        RecreateCaller caller = { self, self->m_RecreateFromPost, dmResource::RESULT_OK, dmResource::RESULT_OK };
        self->m_RecreateFromPost = ~0u;
        dmThread::Thread thread = dmThread::New(RecreateActiveResourceFromThread, 0x20000, &caller, "RecreateJoined");
        dmThread::Join(thread);
        self->m_RecreateResult = caller.m_Recreated;
        return PostCreate(params);
    }
#endif

    static void RecreateDuringCallback(void* user_data)
    {
#if defined(DM_HAS_THREADS)
        RecreateCaller* caller = (RecreateCaller*)user_data;
        ResourceLoaderTest* self = caller->m_Test;
        dmResource::HPreloader dependency = dmResource::NewPreloader(self->m_Factory, "/node_2.node");
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            self->m_GetJobsStarted = 1;
            dmConditionVariable::Broadcast(self->m_Changed);
            while (!self->m_Blocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
        }
        caller->m_Recreated = self->Replace(0, 0, 0, caller->m_Mode);
        caller->m_Loaded = self->Complete(dependency);
        dmResource::DeletePreloader(dependency);
#endif
    }

    static void RequestResidentFromThread(void* user_data)
    {
#if defined(DM_HAS_THREADS)
        GetCaller* caller = (GetCaller*)user_data;
        ResourceLoaderTest* self = caller->m_Test;
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            ++self->m_GetJobsStarted;
            dmConditionVariable::Broadcast(self->m_Changed);
            while (!self->m_AllowBlocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
        }
        caller->m_Result = dmResource::RESULT_OK;
        for (uint32_t i = 0; i < 1024; ++i)
        {
            dmResource::HPreloader request = dmResource::NewPreloader(self->m_Factory, "/node_0.node");
            dmResource::Result result = self->Complete(request);
            if (result != dmResource::RESULT_OK)
                caller->m_Result = result;
            dmResource::DeletePreloader(request);
        }
#endif
    }

    void EnablePostCreate()
    {
        ResourceTypeSetPostCreateFn(dmResource::FindResourceType(m_Factory, "node"), (FResourcePostCreate)PostCreate);
    }

    struct Notification
    {
        ResourceLoaderTest* m_Test;
        uint32_t m_Id;
    };

    static bool Notify(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        Notification* notification = (Notification*)params->m_UserData;
        notification->m_Test->m_Notified[notification->m_Id] = true;
        return true;
    }

    static bool NotifyAndReleasePost(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        Notification* notification = (Notification*)params->m_UserData;
        notification->m_Test->m_HeldPost = ~0u;
        return Notify(params);
    }

    static void CompleteAndReleasePostFromThread(void* user_data)
    {
        GetCaller* caller = (GetCaller*)user_data;
        ResourceLoaderTest* self = caller->m_Test;
        dmResource::HPreloader request = dmResource::NewPreloader(self->m_Factory, "/node_1.node");
        Notification notification = { self, 1 };
        dmResource::PreloaderCompleteCallbackParams params = { self->m_Factory, &notification };
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        do
        {
            caller->m_Result = dmResource::UpdatePreloader(request, NotifyAndReleasePost, &params, 1000);
        } while (caller->m_Result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
        dmResource::DeletePreloader(request);
    }

    static bool LoadAndReleaseSharedDependency(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        GetCaller* caller = (GetCaller*)params->m_UserData;
        caller->m_Result = dmResource::Get(params->m_Factory, "/node_2.node", &caller->m_Resource);
        if (caller->m_Resource)
            dmResource::Release(params->m_Factory, caller->m_Resource);
        caller->m_Resource = 0;
        caller->m_Test->m_HeldPost = ~0u;
        return caller->m_Result == dmResource::RESULT_OK;
    }

    static bool GetCreatedResource(const dmResource::PreloaderCompleteCallbackParams* params)
    {
        GetCaller* caller = (GetCaller*)params->m_UserData;
        caller->m_Result = dmResource::Get(params->m_Factory, "/node_0.node", &caller->m_Resource);
        return caller->m_Result == dmResource::RESULT_OK && !caller->m_Test->m_Posts[0];
    }

    dmResource::Result CompleteWithCallback(dmResource::HPreloader request, uint32_t id)
    {
        Notification notification = { this, id };
        dmResource::PreloaderCompleteCallbackParams params = { m_Factory, &notification };
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        dmResource::Result result;
        do
        {
            result = dmResource::UpdatePreloader(request, Notify, &params, 1000);
        } while (result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
        return result;
    }

    static int32_t FenceProcess(HJobContext context, HJob job, void* user_context, void* user_data)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)user_context;
        DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
        ++self->m_Fences;
#if defined(DM_HAS_THREADS)
        dmConditionVariable::Broadcast(self->m_Changed);
#endif
        return 0;
    }

    static void JobCallback(HJobContext context, HJob job, JobSystemStatus status, void* user_context, void* user_data, int32_t result)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)user_context;
        ++self->m_JobCallbacks;
        self->m_JobCallbackThreadCorrect = self->m_JobCallbackThreadCorrect && dmThread::GetCurrentThread() == self->m_ExpectedThread;
        if (self->m_JobReadsRoot)
        {
            void* resource = 0;
            self->m_JobGetResult = dmResource::Get(self->m_Factory, "/node_0.node", &resource);
            if (resource)
                dmResource::Release(self->m_Factory, resource);
        }
    }

    // A fence observes worker progress without pumping resource creation or
    // sleeping to guess whether the resource jobs have had enough time to run.
    void Fence()
    {
        uint32_t goal;
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            goal = m_Fences + 1;
        }
        Job job = {};
        job.m_Process = FenceProcess;
        job.m_Context = this;
        JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
        if (!JobSystemGetWorkerCount(m_Jobs))
        {
            while (m_Fences < goal)
                JobSystemUpdate(m_Jobs, 0);
        }
#if defined(DM_HAS_THREADS)
        else
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            while (m_Fences < goal)
                dmConditionVariable::Wait(m_Changed, m_Mutex);
        }
#endif
    }

    static void AsyncRequestFromThread(void* user_data)
    {
        GetCaller* caller = (GetCaller*)user_data;
        ResourceLoaderTest* self = caller->m_Test;
        dmResource::HPreloader request = dmResource::NewPreloader(self->m_Factory, "/node_1.node");
#if defined(DM_HAS_THREADS)
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            self->m_Blocked = true;
            dmConditionVariable::Broadcast(self->m_Changed);
            while (!self->m_AllowBlocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
        }
#endif
        caller->m_Result = self->Complete(request);
        dmResource::DeletePreloader(request);
    }

    static void RetryAfterCreateFromThread(void* user_data)
    {
#if defined(DM_HAS_THREADS)
        GetCaller* caller = (GetCaller*)user_data;
        ResourceLoaderTest* self = caller->m_Test;
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            while (!self->m_Blocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
            self->m_FailedPreload = ~0u;
        }
        dmResource::HPreloader request = dmResource::NewPreloader(self->m_Factory, "/node_1.node");
        {
            DM_MUTEX_SCOPED_LOCK(self->m_Mutex);
            self->m_AllowBlocked = true;
            dmConditionVariable::Broadcast(self->m_Changed);
            while (self->m_Blocked)
                dmConditionVariable::Wait(self->m_Changed, self->m_Mutex);
        }
        caller->m_Result = self->Complete(request);
        dmResource::DeletePreloader(request);
#else
        (void)user_data;
#endif
    }

    static dmResource::Result Destroy(const dmResource::ResourceDestroyParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
#if defined(DM_HAS_THREADS)
        if (self->m_CheckLocks)
        {
            LockProbe probe = { dmResource::GetLoadMutex(params->m_Factory), false };
            dmThread::Thread thread = dmThread::New(ProbeLock, 0x8000, &probe, "DestroyProbe");
            dmThread::Join(thread);
            self->m_CallbacksUnlocked = self->m_CallbacksUnlocked && probe.m_Unlocked;
        }
#endif
        LoadedNode* resource = (LoadedNode*)ResourceDescriptorGetResource(params->m_Resource);
        if (resource->m_Value == self->m_CheckedThreadNode)
            self->m_CreationThreadCorrect = self->m_CreationThreadCorrect && dmThread::GetCurrentThread() == self->m_ExpectedThread;
        ++self->m_Destroys[resource->m_Value];
        if (self->m_GetDuringDestroy && resource->m_Value == 0)
        {
            void* other = 0;
            dmResource::Result result = dmResource::Get(params->m_Factory, "/node_2.node", &other);
            self->m_CallbacksUnlocked = self->m_CallbacksUnlocked && result == dmResource::RESULT_OK;
            if (other)
                dmResource::Release(params->m_Factory, other);
        }
        for (uint32_t i = 0; i < resource->m_Dependencies.Size(); ++i)
            dmResource::Release(params->m_Factory, resource->m_Dependencies[i]);
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::HFactory m_Factory;
    HJobContext m_Jobs;
    dmMutex::HMutex m_Mutex;
#if defined(DM_HAS_THREADS)
    dmConditionVariable::HConditionVariable m_Changed;
#endif
    dmArray<dmResource::HPreloader> m_Requests;
    dmArray<uint32_t*> m_Files;
    uint32_t m_Preloads[8192];
    uint32_t m_Creates[8192];
    uint32_t m_Destroys[8192];
    uint32_t m_Posts[8192];
    bool m_Notified[8192];
    uint32_t m_Decoded;
    uint32_t m_CreateDepth;
    uint32_t m_MaxCreateDepth;
    uint32_t m_HintOnly;
    uint32_t m_Block;
    bool m_Blocked;
    bool m_AllowBlocked;
    bool m_Transfer;
    bool m_Hints;
    bool m_CheckLocks;
    bool m_GetDuringDestroy;
    bool m_CallbacksUnlocked;
    bool m_Transitive;
    bool m_PostReadsParent;
    bool m_PostLoadsSibling;
    bool m_SignalPost;
    uint32_t m_RecreateFromPost;
    dmResource::Result m_RecreateResult;
    bool m_PostPendingOnce;
    bool m_HoldPosts;
    bool m_ReplacePointer;
    bool m_CheckPostIdentity;
    bool m_PostIdentityCorrect;
    uint32_t m_HeldPost;
    uint32_t m_FailedPost;
    uint32_t m_FailedPreload;
    uint32_t m_CheckedThreadNode;
    uint32_t m_SignalCreate;
    bool m_WaitInCreate;
    dmThread::Thread m_ExpectedThread;
    bool m_CreationThreadCorrect;
    bool m_JobCallbackThreadCorrect;
    uint32_t m_JobCallbacks;
    bool m_JobReadsRoot;
    dmResource::Result m_JobGetResult;
    uint32_t m_GetJobsStarted;
    uint32_t m_GetJobsFinished;
    bool m_PreloadFromJob;
    uint32_t m_Fences;
    uint64_t m_DecodedBytes;
};

// Simultaneous roots, aliases and synchronous Get share the same in-flight resource and references.
TEST_P(ResourceLoaderTest, SharedInFlightResource)
{
    Add(0, 0, 0);
    dmResource::HPreloader first = Request(0);
    dmResource::HPreloader second = Request(0);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "//node_0.node", &resource));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(first));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(second));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(3u, dmResource::GetRefCount(m_Factory, resource));
    Delete(0);
    Delete(1);
    ASSERT_EQ(1u, dmResource::GetRefCount(m_Factory, resource));
    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Concurrent synchronous callers share one load and wake reliably with cooperative or threaded job contexts.
TEST_P(ResourceLoaderTest, ConcurrentSynchronousGet)
{
#if defined(DM_HAS_THREADS)
    uint32_t dependency = 1;
    Add(0, &dependency, 1);
    Add(1, 0, 0);
    GetCaller callers[4] = {};
    dmThread::Thread threads[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        callers[i].m_Test = this;
        threads[i] = dmThread::New(GetFromThread, 0x20000, &callers[i], "ResourceGet");
    }
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        m_AllowBlocked = true;
        dmConditionVariable::Broadcast(m_Changed);
    }
    for (uint32_t i = 0; i < 4; ++i)
        dmThread::Join(threads[i]);
    for (uint32_t i = 0; i < 4; ++i)
    {
        ASSERT_EQ(dmResource::RESULT_OK, callers[i].m_Result);
        ASSERT_EQ(callers[0].m_Resource, callers[i].m_Resource);
    }
    ASSERT_EQ(4u, dmResource::GetRefCount(m_Factory, callers[0].m_Resource));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Preloads[1]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    for (uint32_t i = 0; i < 4; ++i)
        dmResource::Release(m_Factory, callers[i].m_Resource);
    ASSERT_EQ(1u, m_Destroys[0]);
    ASSERT_EQ(1u, m_Destroys[1]);
#endif
}

// Synchronous loads progress when every worker is occupied by a caller, with shared dependencies loaded once.
TEST_P(ResourceLoaderTest, SynchronousGetFromSaturatedJobPool)
{
    if (!m_Jobs)
        return;
    uint32_t count = JobSystemGetWorkerCount(m_Jobs);
    if (!count)
        count = 1;
    GetCaller callers[4] = {};
    ASSERT_LE(count, 4u);
    uint32_t child = 0;
    Add(child, 0, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        Add(i + 1, &child, 1);
        callers[i].m_Test = this;
        callers[i].m_Id = i + 1;
    }
    StartGetJobs(callers, count);
    FinishGetJobs(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(dmResource::RESULT_OK, callers[i].m_Result);
        ASSERT_EQ(1u, m_Preloads[i + 1]);
        ASSERT_EQ(1u, m_Creates[i + 1]);
        dmResource::Release(m_Factory, callers[i].m_Resource);
    }
    ASSERT_EQ(1u, m_Preloads[child]);
    ASSERT_EQ(1u, m_Creates[child]);
    ASSERT_EQ(1u, m_Destroys[child]);
}

// A worker takes over its queued preload and discovers a dependency through a nested synchronous Get.
TEST_P(ResourceLoaderTest, SynchronousGetFromJobSharesQueuedPreload)
{
    if (!m_Jobs)
        return;
    m_Hints = false;
    m_PreloadFromJob = true;
    EnablePostCreate();
    m_PostPendingOnce = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(child, 0, 0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 0 };
    StartGetJobs(&caller, 1);
    FinishGetJobs(1);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    for (uint32_t i = 0; i < 2; ++i)
    {
        ASSERT_EQ(1u, m_Preloads[i]);
        ASSERT_EQ(1u, m_Creates[i]);
        ASSERT_EQ(2u, m_Posts[i]);
    }
    dmResource::Release(m_Factory, caller.m_Resource);
}

// Queued unrelated reads and an oversized parent must not prevent a worker from reading its own child.
TEST_P(ResourceLoaderTest, SynchronousGetFromJobUnderQueuedReadPressure)
{
    if (!m_Jobs || JobSystemGetWorkerCount(m_Jobs) != 1)
        return;
    Add(0, 0, 0);
    uint32_t child = 2;
    AddSized(1, &child, 1, 5 * 1024 * 1024);
    Add(child, 0, 0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    StartGetJobs(&caller, 1);
    dmResource::HPreloader unrelated = Request(0);
    FinishGetJobs(1);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(0u, m_Creates[0]);
    for (uint32_t i = 1; i < 3; ++i)
    {
        ASSERT_EQ(1u, m_Preloads[i]);
        ASSERT_EQ(1u, m_Creates[i]);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(unrelated));
    dmResource::Release(m_Factory, caller.m_Resource);
}

// A read failure reached from a worker completes the caller and releases its parent's decoded data.
TEST_P(ResourceLoaderTest, SynchronousGetFromJobPropagatesReadFailure)
{
    if (!m_Jobs)
        return;
    uint32_t child = 1;
    Add(0, &child, 1);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 0 };
    StartGetJobs(&caller, 1);
    FinishGetJobs(1);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, caller.m_Result);
    ASSERT_EQ((void*)0, caller.m_Resource);
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(0u, m_Decoded);
}

// Different roots retain one shared dependency without duplicate reads, creation or premature destruction.
TEST_P(ResourceLoaderTest, SharedDependency)
{
    uint32_t dependency = 2;
    Add(0, &dependency, 1);
    Add(1, &dependency, 1);
    Add(2, 0, 0);
    dmResource::HPreloader first = Request(0);
    dmResource::HPreloader second = Request(1);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(first));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(second));
    ASSERT_EQ(1u, m_Preloads[2]);
    ASSERT_EQ(1u, m_Creates[2]);
    Delete(0);
    ASSERT_EQ(0u, m_Destroys[2]);
    Delete(1);
    ASSERT_EQ(1u, m_Destroys[2]);
}

// A late shared leaf publishes success or failure through a wide diamond after its ancestors finish their own callbacks.
TEST_P(ResourceLoaderTest, SharedLeafCompletesWideGraph)
{
    EnablePostCreate();
    const uint32_t count = 2048;
    for (uint32_t failure = 0; failure < 2; ++failure)
    {
        uint32_t root = failure * (count + 2);
        uint32_t leaf = root + count + 1;
        m_HeldPost = leaf;
        m_FailedPost = failure ? leaf : ~0u;
        Add(leaf, 0, 0);
        dmArray<uint32_t> dependencies;
        dependencies.SetCapacity(count);
        for (uint32_t id = root + 1; id < leaf; ++id)
        {
            Add(id, &leaf, 1);
            dependencies.Push(id);
        }
        Add(root, dependencies.Begin(), dependencies.Size());
        dmResource::HPreloader request = Request(root);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Posts[root] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 1000000));
        ASSERT_EQ(1u, m_Posts[root]);
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 1000000));
        for (uint32_t id = root; id < leaf; ++id)
            ASSERT_EQ(1u, m_Posts[id]);
        m_HeldPost = ~0u;
        ASSERT_EQ(failure ? dmResource::RESULT_INVALID_DATA : dmResource::RESULT_OK, Complete(request));
        Delete(failure);
        for (uint32_t id = root; id <= leaf; ++id)
        {
            ASSERT_EQ(1u, m_Preloads[id]);
            ASSERT_EQ(1u, m_Creates[id]);
            ASSERT_EQ(1u, m_Destroys[id]);
        }
    }
}

// Discovery and loading have no previous 1024-node or 1536-path ceiling, including duplicate references.
TEST_P(ResourceLoaderTest, LargeDependencyGraph)
{
    const uint32_t count = 4096;
    dmArray<uint32_t> dependencies;
    dependencies.SetCapacity(count + 1);
    for (uint32_t i = 1; i <= count; ++i)
    {
        Add(i, 0, 0);
        dependencies.Push(i);
    }
    dependencies.Push(1);
    Add(0, dependencies.Begin(), dependencies.Size());
    dmResource::HPreloader request = Request(0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    for (uint32_t i = 0; i <= count; ++i)
    {
        ASSERT_EQ(1u, m_Preloads[i]);
        ASSERT_EQ(1u, m_Creates[i]);
    }
    Delete(0);
    for (uint32_t i = 0; i <= count; ++i)
        ASSERT_EQ(1u, m_Destroys[i]);
}

// Nested Get grows the node pool during Create, and a later load reuses the released slots.
TEST_P(ResourceLoaderTest, UnhintedGraphGrowthAndReuse)
{
    m_Hints = false;
    const uint32_t count = 257;
    uint32_t dependencies[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        dependencies[i] = i + 1;
        Add(i + 1, 0, 0);
    }
    Add(0, dependencies, count);
    for (uint32_t cycle = 0; cycle < 2; ++cycle)
    {
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
        for (uint32_t i = 0; i <= count; ++i)
        {
            ASSERT_EQ(cycle + 1, m_Preloads[i]);
            ASSERT_EQ(cycle + 1, m_Creates[i]);
        }
        Delete(cycle);
        for (uint32_t i = 0; i <= count; ++i)
            ASSERT_EQ(cycle + 1, m_Destroys[i]);
    }
}

// Removing an older node moves the pending node; reusing that slot must preserve its post queue and roots.
TEST_P(ResourceLoaderTest, PendingLoadSurvivesCompactionAndReuse)
{
    Add(0, 0, 0);
    Add(1, 0, 0);
    Add(2, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    EnablePostCreate();
    m_HoldPosts = true;
    dmResource::HPreloader pending = Request(1);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Posts[1] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(pending, 0, 0, 1000000));
    ASSERT_GT(m_Posts[1], 0u);
    Delete(0);
    dmResource::HPreloader replacement = Request(2);
    m_HoldPosts = false;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(pending));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(replacement));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_1.node", &resource));
    ASSERT_EQ(1u, ((LoadedNode*)resource)->m_Value);
    dmResource::Release(m_Factory, resource);
    Delete(1);
    Delete(2);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(1u, m_Preloads[i]);
        ASSERT_EQ(1u, m_Creates[i]);
        ASSERT_EQ(1u, m_Destroys[i]);
    }
}

// Cycles terminate through the shared graph and still consume every successful Preload allocation.
TEST_P(ResourceLoaderTest, DependencyCycle)
{
    uint32_t one = 1, zero = 0;
    Add(0, &one, 1);
    Add(1, &zero, 1);
    dmResource::HPreloader request = Request(0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, Complete(request));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Preloads[1]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_EQ(0u, m_Decoded);
}

// Missing children propagate errors without skipping cleanup of their parent's decoded payload.
TEST_P(ResourceLoaderTest, MissingDependency)
{
    uint32_t missing = 42;
    Add(0, &missing, 1);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(Request(0)));
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(0u, m_Decoded);
}

// A retained missing-file failure cannot block Get after content appears or remove the new resource on deletion.
TEST_P(ResourceLoaderTest, RetryAfterMissingFile)
{
    dmResource::HPreloader failed = Request(0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    Add(0, 0, 0);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &resource));
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    Delete(0);
    ASSERT_EQ(0u, m_Destroys[0]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    Delete(1);
    ASSERT_EQ(0u, m_Destroys[0]);
    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Supplied bytes start a fresh attempt while a previous missing-file request retains its terminal error.
TEST_P(ResourceLoaderTest, CreateAfterMissingFile)
{
    dmResource::HPreloader failed = Request(0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    uint32_t data[] = { 0, 0 };
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_Factory, "/node_0.node", data, sizeof(data), &resource));
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    Delete(0);
    void* cached = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &cached));
    ASSERT_EQ(resource, cached);
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    dmResource::Release(m_Factory, cached);
    ASSERT_EQ(0u, m_Destroys[0]);
    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Fresh parent requests rediscover a newly available child without changing the old request's failure.
TEST_P(ResourceLoaderTest, RetryAfterMissingDependency)
{
    uint32_t child = 1;
    Add(0, &child, 1);
    dmResource::HPreloader failed = Request(0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    Add(1, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    Delete(0);
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &resource));
    ASSERT_EQ(2u, m_Preloads[0]);
    ASSERT_EQ(2u, m_Creates[0]);
    ASSERT_EQ(1u, m_Preloads[1]);
    ASSERT_EQ(1u, m_Creates[1]);
    Delete(1);
    ASSERT_EQ(0u, m_Destroys[0]);
    ASSERT_EQ(0u, m_Destroys[1]);
    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(1u, m_Destroys[0]);
    ASSERT_EQ(1u, m_Destroys[1]);
}

// Failed dependencies stay shared within one attempt; a fresh request can retry after the failure is corrected.
TEST_P(ResourceLoaderTest, RetrySharedPreloadFailure)
{
    uint32_t parents[] = { 1, 2, 3, 4 };
    uint32_t leaf = 5;
    Add(0, parents, DM_ARRAY_SIZE(parents));
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(parents); ++i)
        Add(parents[i], &leaf, 1);
    Add(leaf, 0, 0);
    m_FailedPreload = leaf;
    dmResource::HPreloader failed = Request(0);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(failed));
    ASSERT_EQ(1u, m_Preloads[leaf]);
    m_FailedPreload = ~0u;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(failed));
    Delete(0);
    for (uint32_t i = 0; i <= leaf; ++i)
    {
        ASSERT_EQ(2u, m_Preloads[i]);
        ASSERT_EQ(i == leaf ? 1u : 2u, m_Creates[i]);
        ASSERT_EQ(0u, m_Destroys[i]);
    }
    Delete(1);
    for (uint32_t i = 0; i <= leaf; ++i)
        ASSERT_EQ(1u, m_Destroys[i]);
}

// An old parent's cleanup must not wait for a fresh dependency attempt owned by another requesting thread.
TEST_P(ResourceLoaderTest, RetryDuringFailedParentCreate)
{
#if defined(DM_HAS_THREADS)
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(child, 0, 0);
    m_FailedPreload = child;
    m_SignalCreate = 0;
    m_WaitInCreate = true;
    GetCaller retry = { this, 0, dmResource::RESULT_PENDING, child };
    dmThread::Thread thread = dmThread::New(RetryAfterCreateFromThread, 0x80000, &retry, "ResourceRetry");
    dmResource::Result result = Complete(Request(0));
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        m_Blocked = false;
        dmConditionVariable::Broadcast(m_Changed);
    }
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, result);
    ASSERT_EQ(dmResource::RESULT_OK, retry.m_Result);
    ASSERT_EQ(2u, m_Preloads[child]);
    ASSERT_EQ(1u, m_Creates[child]);
    ASSERT_EQ(1u, m_Destroys[child]);
#endif
}

// Preload-transferred buffers remain owned by the type and are not freed a second time by the scheduler.
TEST_P(ResourceLoaderTest, BufferOwnershipTransfer)
{
    m_Transfer = true;
    Add(0, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    Delete(0);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// An unloaded root rediscovers dependencies from its current override instead of reusing a baked closure.
TEST_P(ResourceLoaderTest, ChangedDependenciesAfterUnload)
{
    uint32_t dependency = 1;
    Add(0, &dependency, 1);
    Add(1, 0, 0);
    Add(2, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    Delete(0);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, "/node_0.node"));
    dependency = 2;
    Add(0, &dependency, 1);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(2u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Preloads[1]);
    ASSERT_EQ(1u, m_Preloads[2]);
}

// Reload and both SetResource forms replace old edges and detect cycles through newly acquired dependencies.
TEST_P(ResourceLoaderTest, RecreateReplacesDependencies)
{
    m_CheckLocks = true;
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 4;
        uint32_t previous = root + 1;
        uint32_t next = root + 2;
        Add(root, &previous, 1);
        Add(previous, 0, 0);
        Add(next, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(root)));
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(previous)));
        ASSERT_EQ(dmResource::RESULT_OK, Replace(root, &next, 1, mode));
        ASSERT_EQ(1u, m_Creates[root]);
        ASSERT_EQ(1u, m_Creates[next]);
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(next)));
        ASSERT_EQ(dmResource::RESULT_OK, Replace(previous, &root, 1, mode));
        ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, Replace(next, &root, 1, mode));
    }
    ASSERT_TRUE(m_CallbacksUnlocked);
}

// Replacement during pending PostCreate must update the descriptor before destroying the old resource.
TEST_P(ResourceLoaderTest, RecreateReplacesPendingResource)
{
    EnablePostCreate();
    m_ReplacePointer = true;
    m_CheckPostIdentity = true;
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        m_HoldPosts = true;
        Add(mode, 0, 0);
        dmResource::HPreloader request = Request(mode);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Posts[mode] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_NE(0u, m_Posts[mode]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(mode, 0, 0, mode));
        ASSERT_EQ(1u, m_Destroys[mode]);
        m_HoldPosts = false;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
        ASSERT_TRUE(m_PostIdentityCorrect);
        Delete(mode);
        ASSERT_EQ(2u, m_Destroys[mode]);
    }
}

// Cached requests acquire and release descriptor references while recreation replaces their resource pointer.
TEST_P(ResourceLoaderTest, RecreateWithConcurrentResidentRequests)
{
#if defined(DM_HAS_THREADS)
    Add(0, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    m_ReplacePointer = true;
    GetCaller callers[2] = {};
    dmThread::Thread threads[2];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(callers); ++i)
    {
        callers[i].m_Test = this;
        threads[i] = dmThread::New(RequestResidentFromThread, 0x20000, &callers[i], "ResidentRequest");
    }
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (m_GetJobsStarted != DM_ARRAY_SIZE(callers))
            dmConditionVariable::Wait(m_Changed, m_Mutex);
        m_AllowBlocked = true;
        dmConditionVariable::Broadcast(m_Changed);
    }
    dmResource::Result result = dmResource::RESULT_OK;
    for (uint32_t i = 0; i < 1024; ++i)
    {
        dmResource::Result recreated = Replace(0, 0, 0, 1);
        if (recreated != dmResource::RESULT_OK)
            result = recreated;
    }
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(callers); ++i)
        dmThread::Join(threads[i]);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(callers); ++i)
        ASSERT_EQ(dmResource::RESULT_OK, callers[i].m_Result);
    ASSERT_EQ(1u, dmResource::GetRefCount(m_Factory, dmHashString64("/node_0.node")));
    ASSERT_EQ(1024u, m_Destroys[0]);
    Delete(0);
    ASSERT_EQ(1025u, m_Destroys[0]);
#endif
}

// Detached pending subtrees must finish and release their resources, including when their PostCreate fails.
TEST_P(ResourceLoaderTest, RecreateDetachesPendingDependencies)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 3;
        uint32_t child = root + 1;
        uint32_t leaf = root + 2;
        m_HoldPosts = true;
        Add(root, &child, 1);
        Add(child, &leaf, 1);
        Add(leaf, 0, 0);
        dmResource::HPreloader request = Request(root);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[root] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[root]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(root, 0, 0, mode));
        m_HoldPosts = false;
        if (mode == 1)
            m_FailedPost = leaf;
        if (mode != 2)
            ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
        Delete(mode);
        for (uint32_t id = root; id <= leaf; ++id)
        {
            ASSERT_NE(0u, m_Posts[id]);
            ASSERT_EQ(1u, m_Destroys[id]);
        }
    }
}

// Reload and both SetResource forms must discard a completed error when they remove the failed dependency.
TEST_P(ResourceLoaderTest, RecreateClearsDetachedDependencyFailure)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 2;
        uint32_t child = root + 1;
        m_HoldPosts = true;
        m_HeldPost = root;
        m_FailedPost = child;
        Add(root, &child, 1);
        Add(child, 0, 0);
        dmResource::HPreloader request = Request(root);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[root] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[root]);
        m_HoldPosts = false;
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(Request(child)));
        ASSERT_EQ(dmResource::RESULT_OK, Replace(root, 0, 0, mode));
        Delete(mode * 2 + 1);
        ASSERT_EQ(1u, m_Destroys[child]);
        m_HeldPost = ~0u;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
        Delete(mode * 2);
        ASSERT_EQ(1u, m_Destroys[root]);
    }
}

// Successful recreation must retain a completed transitive error when it keeps the affected dependency.
TEST_P(ResourceLoaderTest, RecreateKeepsRetainedDependencyFailure)
{
    EnablePostCreate();
    m_HoldPosts = true;
    m_HeldPost = 0;
    m_FailedPost = 2;
    uint32_t child = 1;
    uint32_t leaf = 2;
    Add(0, &child, 1);
    Add(child, &leaf, 1);
    Add(leaf, 0, 0);
    dmResource::HPreloader request = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[0]);
    m_HoldPosts = false;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(Request(child)));
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, &child, 1, 1));
    m_HeldPost = ~0u;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
    Delete(1);
    Delete(0);
    for (uint32_t i = 0; i < 3; ++i)
        ASSERT_EQ(1u, m_Destroys[i]);
}

// Failed recreation keeps the old graph and its already recorded dependency error.
TEST_P(ResourceLoaderTest, FailedRecreateKeepsDependencyFailure)
{
    EnablePostCreate();
    m_HoldPosts = true;
    m_HeldPost = 0;
    m_FailedPost = 1;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(child, 0, 0);
    dmResource::HPreloader request = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[0]);
    m_HoldPosts = false;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(Request(child)));
    uint32_t missing = 99;
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Replace(0, &missing, 1, 1));
    m_HeldPost = ~0u;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
    Delete(1);
    Delete(0);
    ASSERT_EQ(1u, m_Destroys[0]);
    ASSERT_EQ(1u, m_Destroys[child]);
}

// Rollback must retain an old child's error even when nested loading produces it during recreation.
TEST_P(ResourceLoaderTest, FailedRecreateKeepsNewDependencyFailure)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 3;
        uint32_t child = root + 1;
        uint32_t replacement = root + 2;
        m_HoldPosts = true;
        m_HeldPost = root;
        m_FailedPost = child;
        Add(root, &child, 1);
        Add(child, 0, 0);
        Add(replacement, &child, 1);
        dmResource::HPreloader request = Request(root);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[root] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[root]);
        m_HoldPosts = false;
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Replace(root, &replacement, 1, mode));
        m_HeldPost = ~0u;
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
        Delete(mode);
        for (uint32_t id = root; id <= replacement; ++id)
            ASSERT_EQ(1u, m_Destroys[id]);
    }
}

// Removing a failed child repairs later lookups without changing an already completed request's failure.
TEST_P(ResourceLoaderTest, RecreateRepairsCompletedDependencyFailure)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 3;
        uint32_t child = root + 1;
        uint32_t leaf = root + 2;
        m_HoldPosts = true;
        m_FailedPost = leaf;
        Add(root, &child, 1);
        Add(child, &leaf, 1);
        Add(leaf, 0, 0);
        dmResource::HPreloader request = Request(root);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[root] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[root]);
        m_HoldPosts = false;
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
        ASSERT_EQ(dmResource::RESULT_OK, Replace(child, 0, 0, mode));
        for (uint32_t id = root; id <= child; ++id)
        {
            char path[64];
            Path(id, path, sizeof(path));
            void* resource = 0;
            ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path, &resource));
            dmResource::Release(m_Factory, resource);
            ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, dmHashString64(path), &resource));
            dmResource::Release(m_Factory, resource);
        }
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
        Delete(mode);
        for (uint32_t id = root; id <= leaf; ++id)
            ASSERT_EQ(1u, m_Destroys[id]);
    }
}

// Replacing a failed dependency with a pending one makes completed ancestors wait for the new final result.
TEST_P(ResourceLoaderTest, RecreateReplacesFailureWithPendingDependency)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 4;
        uint32_t child = root + 1;
        uint32_t failed = root + 2;
        uint32_t pending = root + 3;
        m_HoldPosts = true;
        m_FailedPost = failed;
        Add(root, &child, 1);
        Add(child, &failed, 1);
        Add(failed, 0, 0);
        Add(pending, 0, 0);
        dmResource::HPreloader original = Request(root);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[root] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(original, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[root]);
        m_HoldPosts = false;
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(original));
        m_HeldPost = pending;
        dmResource::HPreloader dependency = Request(pending);
        deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[pending] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(dependency, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[pending]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(child, &pending, 1, mode));
        dmResource::HPreloader later = Request(root);
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(later, 0, 0, 0));
        m_HeldPost = ~0u;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(dependency));
        ASSERT_EQ(dmResource::RESULT_OK, Complete(later));
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(original));
        for (uint32_t i = mode * 3; i < (mode + 1) * 3; ++i)
            Delete(i);
        for (uint32_t id = root; id <= pending; ++id)
            ASSERT_EQ(1u, m_Destroys[id]);
    }
}

// Recomputing dependency errors must not erase a failure from the resource's own PostCreate.
TEST_P(ResourceLoaderTest, RecreateKeepsCompletedPostCreateFailure)
{
    EnablePostCreate();
    m_FailedPost = 0;
    Add(0, 0, 0);
    dmResource::HPreloader request = Request(0);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, 1));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, dmResource::Get(m_Factory, "/node_0.node", &resource));
    ASSERT_EQ((void*)0, resource);
    Delete(0);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Detaching the last pending child must queue completion when the parent's own PostCreate already finished.
TEST_P(ResourceLoaderTest, RecreateDetachesLastPendingCompletion)
{
    EnablePostCreate();
    uint32_t child = 1;
    m_HeldPost = child;
    Add(0, &child, 1);
    Add(child, 0, 0);
    dmResource::HPreloader request = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Posts[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_NE(0u, m_Posts[0]);
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, 1));
    m_HeldPost = ~0u;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    ASSERT_EQ(1u, m_Destroys[child]);
    Delete(0);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Detaching a pending dependency from its initiating request must preserve another request's shared resource.
TEST_P(ResourceLoaderTest, RecreateDetachesSharedPendingDependency)
{
    EnablePostCreate();
    m_HoldPosts = true;
    uint32_t child = 2;
    Add(0, &child, 1);
    Add(1, &child, 1);
    Add(child, 0, 0);
    dmResource::HPreloader first = Request(0);
    dmResource::HPreloader second = Request(1);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while ((!m_Creates[0] || !m_Creates[1]) && dmTime::GetMonotonicTime() < deadline)
    {
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(first, 0, 0, 0));
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(second, 0, 0, 0));
    }
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, 1));
    m_HoldPosts = false;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(second));
    Delete(0);
    ASSERT_EQ(0u, m_Destroys[child]);
    Delete(1);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(1u, m_Creates[i]);
        ASSERT_EQ(1u, m_Destroys[i]);
    }
}

// A newly acquired dependency can already be queued before the request's cursor, without advancing its other parent.
TEST_P(ResourceLoaderTest, NestedGetSelectsEarlierQueuedDependency)
{
    if (GetParam() != 0)
        return;
    uint32_t child = 2;
    Add(0, &child, 1);
    Add(1, &child, 1);
    Add(child, 0, 0);
    dmResource::HPreloader background = Request(0);
    ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(background, 0, 0, 0));
    m_Hints = false;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(1)));
    ASSERT_EQ(0u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_EQ(1u, m_Creates[child]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(background));
    Delete(1);
    ASSERT_EQ(0u, m_Destroys[child]);
    Delete(0);
    ASSERT_EQ(1u, m_Destroys[child]);
}

// A foreground graph must finish without creating or finalizing an unrelated graph's queued leaves.
TEST_P(ResourceLoaderTest, DisjointRequestsWithQueuedLeaves)
{
    if (GetParam() != 0)
        return;
    const uint32_t count = 2048;
    uint32_t dependencies[count];
    for (uint32_t graph = 0; graph < 2; ++graph)
    {
        uint32_t root = graph * (count + 1);
        for (uint32_t i = 0; i < count; ++i)
        {
            dependencies[i] = root + i + 1;
            Add(dependencies[i], 0, 0);
        }
        Add(root, dependencies, count);
    }
    dmResource::HPreloader background = Request(0);
    ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(background, 0, 0, 0));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(count + 1)));
    for (uint32_t i = 0; i <= count; ++i)
    {
        ASSERT_EQ(0u, m_Creates[i]);
        ASSERT_EQ(1u, m_Creates[count + 1 + i]);
    }
    Delete(1);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(background));
    Delete(0);
    for (uint32_t i = 0; i < 2 * (count + 1); ++i)
        ASSERT_EQ(1u, m_Destroys[i]);
}

// Removing old hint-only dependencies during Recreate can move the parent being updated in the pool.
TEST_P(ResourceLoaderTest, RecreateCompactsHintOnlyDependencies)
{
    m_HintOnly = 0;
    const uint32_t count = 65;
    uint32_t dependencies[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        dependencies[i] = i + 1;
        Add(i + 1, 0, 0);
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(i + 1)));
    }
    Add(0, dependencies, count);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    for (uint32_t i = 0; i < count; ++i)
    {
        Delete(i);
        ASSERT_EQ(1u, m_Destroys[i + 1]);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, 1));
    uint32_t dependency = 1;
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, &dependency, 1, 1));
    ASSERT_EQ(2u, m_Creates[1]);
    ASSERT_EQ(1u, m_Creates[0]);
    Delete(count);
    ASSERT_EQ(1u, m_Destroys[0]);
    ASSERT_EQ(2u, m_Destroys[1]);
}

// Failed recreation preserves the previous graph and releases dependencies acquired by the failed attempt.
TEST_P(ResourceLoaderTest, FailedRecreatePreservesDependencies)
{
    uint32_t previous = 1;
    Add(0, &previous, 1);
    Add(1, 0, 0);
    Add(2, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(1)));
    uint32_t replacement[] = { 2, 99 };
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Replace(0, replacement, 2, 1));
    ASSERT_EQ(1u, m_Destroys[2]);
    ASSERT_EQ(0u, m_Destroys[1]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(2)));
    uint32_t root = 0;
    ASSERT_EQ(dmResource::RESULT_OK, Replace(2, &root, 1, 1));
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, Replace(1, &root, 1, 1));
}

// Recreate releases table locks and yields scheduler ownership while a foreign caller creates its dependency.
TEST_P(ResourceLoaderTest, RecreateWithForeignAffinity)
{
#if defined(DM_HAS_THREADS)
    Add(0, 0, 0);
    Add(1, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    m_CheckLocks = true;
    m_CheckedThreadNode = 1;
    m_SignalCreate = 0;
    dmResource::HPreloader request = Request(1);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 0 };
    dmThread::Thread thread = dmThread::New(RecreateFromThread, 0x20000, &caller, "RecreateGet");
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_Blocked)
            dmConditionVariable::Wait(m_Changed, m_Mutex);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_TRUE(m_CallbacksUnlocked);
    ASSERT_TRUE(m_CreationThreadCorrect);
#endif
}

// Recreation cannot replace an object still used by PostCreate, even during a nested Get on a foreign owner.
TEST_P(ResourceLoaderTest, RecreateDuringSuspendedPostCreate)
{
#if defined(DM_HAS_THREADS)
    EnablePostCreate();
    m_PostLoadsSibling = true;
    m_SignalPost = true;
    m_ReplacePointer = true;
    Add(0, 0, 0);
    Add(2, 0, 0);
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        m_Blocked = false;
        m_GetJobsStarted = 0;
        RecreateCaller caller = { this, mode, dmResource::RESULT_OK, dmResource::RESULT_PENDING };
        dmThread::Thread thread = dmThread::New(RecreateDuringCallback, 0x20000, &caller, "RecreatePost");
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            while (!m_GetJobsStarted)
                dmConditionVariable::Wait(m_Changed, m_Mutex);
        }
        dmResource::Result result = Complete(Request(0));
        dmThread::Join(thread);
        ASSERT_EQ(dmResource::RESULT_OK, result);
        ASSERT_EQ(dmResource::RESULT_OK, caller.m_Loaded);
        ASSERT_EQ(dmResource::RESULT_PENDING, caller.m_Recreated);
        ASSERT_EQ(mode * 2, m_Destroys[0]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, mode));
        ASSERT_EQ(mode * 2 + 1, m_Destroys[0]);
        Delete(mode);
        ASSERT_EQ(mode * 2 + 2, m_Destroys[0]);
    }
#endif
}

// Reentrant recreation from PostCreate returns pending without changing its object, including on wasm.
TEST_P(ResourceLoaderTest, RecreateFromActivePostCreate)
{
    EnablePostCreate();
    m_ReplacePointer = true;
    Add(0, 0, 0);
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        m_RecreateFromPost = mode;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
        ASSERT_EQ(dmResource::RESULT_PENDING, m_RecreateResult);
        ASSERT_EQ(mode * 2, m_Destroys[0]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, mode));
        Delete(mode);
        ASSERT_EQ(mode * 2 + 2, m_Destroys[0]);
    }
}

// A PostCreate that joins a foreign recreation caller must receive pending without waiting for itself.
TEST_P(ResourceLoaderTest, RecreateFromJoinedPostCreate)
{
#if defined(DM_HAS_THREADS)
    ResourceTypeSetPostCreateFn(dmResource::FindResourceType(m_Factory, "node"), (FResourcePostCreate)PostCreateWithJoinedRecreate);
    m_ReplacePointer = true;
    Add(0, 0, 0);
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        m_RecreateFromPost = mode;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
        ASSERT_EQ(dmResource::RESULT_PENDING, m_RecreateResult);
        ASSERT_EQ(mode * 2, m_Destroys[0]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, mode));
        Delete(mode);
        ASSERT_EQ(mode * 2 + 2, m_Destroys[0]);
    }
#endif
}

// A suspended Recreate owns its original object until publication and cannot be replaced by the dependency owner.
TEST_P(ResourceLoaderTest, RecreateDuringSuspendedRecreate)
{
#if defined(DM_HAS_THREADS)
    m_ReplacePointer = true;
    Add(0, 0, 0);
    Add(2, 0, 0);
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
        m_SignalCreate = 0;
        m_Blocked = false;
        m_GetJobsStarted = 0;
        RecreateCaller caller = { this, mode, dmResource::RESULT_OK, dmResource::RESULT_PENDING };
        dmThread::Thread thread = dmThread::New(RecreateDuringCallback, 0x20000, &caller, "RecreateBusy");
        {
            DM_MUTEX_SCOPED_LOCK(m_Mutex);
            while (!m_GetJobsStarted)
                dmConditionVariable::Wait(m_Changed, m_Mutex);
        }
        uint32_t child = 2;
        dmResource::Result result = Replace(0, &child, 1, mode);
        dmThread::Join(thread);
        ASSERT_EQ(dmResource::RESULT_OK, result);
        ASSERT_EQ(dmResource::RESULT_OK, caller.m_Loaded);
        ASSERT_EQ(dmResource::RESULT_PENDING, caller.m_Recreated);
        ASSERT_EQ(mode * 3 + 1, m_Destroys[0]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(0, 0, 0, mode));
        Delete(mode);
        ASSERT_EQ(mode * 3 + 3, m_Destroys[0]);
    }
#endif
}

// PostCreate remains queued while a foreign Recreate is suspended, then resumes against the published replacement.
TEST_P(ResourceLoaderTest, PostCreateDuringSuspendedRecreate)
{
#if defined(DM_HAS_THREADS)
    EnablePostCreate();
    m_HeldPost = 0;
    m_ReplacePointer = true;
    m_CheckPostIdentity = true;
    Add(0, 0, 0);
    Add(1, 0, 0);
    dmResource::HPreloader original = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(original, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[0]);
    dmResource::HPreloader dependency = Request(1);
    m_SignalCreate = 0;
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 0 };
    dmThread::Thread thread = dmThread::New(RecreateFromThread, 0x20000, &caller, "RecreatePost");
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_Blocked)
            dmConditionVariable::Wait(m_Changed, m_Mutex);
    }
    dmResource::HPreloader alias = Request(0);
    Notification notification = { this, 0 };
    dmResource::PreloaderCompleteCallbackParams params = { m_Factory, &notification };
    uint32_t before = m_Posts[0];
    deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Notified[0] && dmTime::GetMonotonicTime() < deadline)
        dmResource::UpdatePreloader(alias, Notify, &params, 1000);
    uint32_t during = m_Posts[0];
    dmResource::Result loaded = Complete(dependency);
    dmThread::Join(thread);
    ASSERT_TRUE(m_Notified[0]);
    ASSERT_EQ(before, during);
    ASSERT_EQ(dmResource::RESULT_OK, loaded);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    m_HeldPost = ~0u;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(alias));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(original));
    ASSERT_TRUE(m_PostIdentityCorrect);
#endif
}

// Completed ancestors become pending when recreation adds a child and inherit its later PostCreate failure.
TEST_P(ResourceLoaderTest, RecreateAddsPendingDependencyFailure)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 3;
        uint32_t child = root + 1;
        uint32_t ancestor = root + 2;
        Add(root, 0, 0);
        Add(child, 0, 0);
        Add(ancestor, &root, 1);
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(ancestor)));
        m_HeldPost = child;
        dmResource::HPreloader request = Request(child);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[child] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[child]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(root, &child, 1, mode));
        for (uint32_t id = root; id <= ancestor; ++id)
        {
            char path[64];
            Path(id, path, sizeof(path));
            void* resource = 0;
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::Get(m_Factory, dmHashString64(path), &resource));
            ASSERT_EQ((void*)0, resource);
        }
        m_FailedPost = child;
        m_HeldPost = ~0u;
        ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(request));
        for (uint32_t id = root; id <= ancestor; ++id)
        {
            char path[64];
            Path(id, path, sizeof(path));
            void* resource = 0;
            ASSERT_EQ(dmResource::RESULT_INVALID_DATA, dmResource::Get(m_Factory, path, &resource));
            ASSERT_EQ((void*)0, resource);
        }
        Delete(mode * 2 + 1);
        Delete(mode * 2);
        ASSERT_EQ(1u, m_Destroys[child]);
    }
}

// Completed requests can be updated and deleted while a later reload's dependency is still pending.
TEST_P(ResourceLoaderTest, CompletedRequestIgnoresRecreatedDependencies)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 2;
        uint32_t child = root + 1;
        Add(root, 0, 0);
        Add(child, 0, 0);
        dmResource::HPreloader original = Request(root);
        ASSERT_EQ(dmResource::RESULT_OK, Complete(original));
        m_HeldPost = child;
        dmResource::HPreloader dependency = Request(child);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[child] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(dependency, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[child]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(root, &child, 1, mode));
        dmResource::HPreloader later = Request(root);
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(later, 0, 0, 0));
        uint32_t posts = m_Posts[child];
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::UpdatePreloader(original, 0, 0, 0));
        Delete(mode * 3);
        ASSERT_EQ(posts, m_Posts[child]);
        ASSERT_EQ(0u, m_Destroys[root]);
        ASSERT_EQ(0u, m_Destroys[child]);
        m_HeldPost = ~0u;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(later));
        ASSERT_EQ(dmResource::RESULT_OK, Complete(dependency));
        Delete(mode * 3 + 2);
        ASSERT_EQ(1u, m_Destroys[root]);
        ASSERT_EQ(0u, m_Destroys[child]);
        Delete(mode * 3 + 1);
        ASSERT_EQ(1u, m_Destroys[child]);
    }
}

// A recreated fan-out stays pending until every independently owned dependency has finished.
TEST_P(ResourceLoaderTest, RecreateWaitsForAllAddedDependencies)
{
    EnablePostCreate();
    Add(0, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    const uint32_t count = 64;
    uint32_t children[count];
    m_HoldPosts = true;
    for (uint32_t i = 0; i < count; ++i)
    {
        children[i] = i + 1;
        Add(i + 1, 0, 0);
        dmResource::HPreloader request = Request(i + 1);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[i + 1] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[i + 1]);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, children, count, 1));
    m_HoldPosts = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_EQ(dmResource::RESULT_OK, Complete(m_Requests[i + 1]));
        void* resource = 0;
        dmResource::Result result = dmResource::Get(m_Factory, dmHashString64("/node_0.node"), &resource);
        ASSERT_EQ(i + 1 == count ? dmResource::RESULT_OK : dmResource::RESULT_PENDING, result);
        if (resource)
            dmResource::Release(m_Factory, resource);
    }
    ASSERT_EQ(1u, m_Posts[0]);
}

// Get traverses finalized ancestors to finish dependencies added during recreation without re-running their callbacks.
TEST_P(ResourceLoaderTest, GetCompletesRecreatedDependencyGraph)
{
    EnablePostCreate();
    Add(0, 0, 0);
    Add(1, 0, 0);
    uint32_t root = 0;
    Add(2, &root, 1);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(2)));
    m_HeldPost = 1;
    dmResource::HPreloader request = Request(1);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[1] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[1]);
    uint32_t child = 1;
    ASSERT_EQ(dmResource::RESULT_OK, Replace(0, &child, 1, 1));
    uint32_t ancestor = 2;
    Add(3, &ancestor, 1);
    dmResource::HPreloader later = Request(3);
    deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Posts[3] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(later, 0, 0, 0));
    ASSERT_EQ(1u, m_Posts[3]);
    ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(later, 0, 0, 0));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::Get(m_Factory, dmHashString64("/node_3.node"), &resource));
    ASSERT_EQ((void*)0, resource);
    m_HeldPost = ~0u;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_3.node", &resource));
    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(later));
    ASSERT_EQ(1u, m_Posts[0]);
    ASSERT_EQ(1u, m_Posts[2]);
    ASSERT_EQ(1u, m_Posts[3]);
    ASSERT_EQ(1u, m_Creates[1]);
}

// Legacy unhinted Gets join the same scheduler and can complete recursively without creation locks.
TEST_P(ResourceLoaderTest, UnhintedDependency)
{
    m_Hints = false;
    uint32_t dependency = 1;
    Add(0, &dependency, 1);
    Add(1, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(1u, m_Preloads[1]);
    ASSERT_EQ(1u, m_Creates[1]);
}

// A nested Get loads its branch without nesting unrelated creators waiting for the same unhinted leaf.
TEST_P(ResourceLoaderTest, UnrelatedCreatorsDuringNestedGet)
{
    m_Hints = false;
    const uint32_t count = 128;
    uint32_t dependency = count;
    Add(dependency, 0, 0);
    for (uint32_t i = 0; i < count; ++i)
    {
        Add(i, &dependency, 1);
        Request(i);
    }
    for (uint32_t i = 0; i < m_Requests.Size(); ++i)
        ASSERT_EQ(dmResource::RESULT_OK, Complete(m_Requests[i]));
    ASSERT_EQ(1u, m_Preloads[dependency]);
    ASSERT_EQ(1u, m_Creates[dependency]);
    ASSERT_EQ(2u, m_MaxCreateDepth);
}

// A hint-only intermediary keeps a leaf available until an ancestor consumes it, as Lua imports do.
TEST_P(ResourceLoaderTest, TransitiveHintLifetime)
{
    m_HintOnly = 1;
    m_Transitive = true;
    uint32_t dependency = 1;
    Add(0, &dependency, 1);
    dependency = 2;
    Add(1, &dependency, 1);
    Add(2, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(1u, m_Preloads[2]);
    ASSERT_EQ(1u, m_Creates[2]);
    Delete(0);
    ASSERT_EQ(1u, m_Destroys[2]);
}

// Removing one request during a shared load preserves the other request and drains all owned jobs safely.
TEST_P(ResourceLoaderTest, DeleteDuringSharedLoad)
{
    uint32_t dependency = 1;
    Add(0, &dependency, 1);
    Add(1, 0, 0);
    Request(0);
    dmResource::HPreloader remaining = Request(0);
    Delete(0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(remaining));
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(0u, m_Destroys[0]);
    Delete(1);
    ASSERT_EQ(1u, m_Destroys[0]);
    ASSERT_EQ(1u, m_Destroys[1]);
}

// Deleting a completed request must not spend its cleanup budget creating unrelated resources.
TEST_P(ResourceLoaderTest, DeleteCompletedLeavesUnrelatedWorkPending)
{
    Add(0, 0, 0);
    Add(1, 0, 0);
    dmResource::HPreloader first = Request(0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(first));
    dmResource::HPreloader second = Request(1);
    ASSERT_EQ(0u, m_Creates[1]);
    Delete(0);
    ASSERT_EQ(0u, m_Creates[1]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(second));
    ASSERT_EQ(1u, m_Creates[1]);
}

// Updating a small request leaves an earlier, unrelated fan-out's creators and posts for its own update.
TEST_P(ResourceLoaderTest, RequestBudgetOnlyAdvancesReachableResources)
{
    EnablePostCreate();
    const uint32_t count = 64;
    uint32_t children[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        children[i] = i + 2;
        Add(i + 2, 0, 0);
    }
    Add(1, children, count);
    Add(0, 0, 0);
    dmResource::HPreloader large_request = Request(1);
    dmResource::HPreloader small_request = Request(0);
    ASSERT_EQ(dmResource::RESULT_OK, CompleteWithCallback(small_request, 0));
    ASSERT_TRUE(m_Notified[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Posts[0]);
    for (uint32_t i = 1; i < count + 2; ++i)
    {
        ASSERT_EQ(0u, m_Creates[i]);
        ASSERT_EQ(0u, m_Posts[i]);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(large_request));
}

// A zero-worker update checks its soft budget between reading and creating a resource.
TEST_P(ResourceLoaderTest, ZeroBudgetSeparatesReadAndCreate)
{
    if (!m_Jobs || JobSystemGetWorkerCount(m_Jobs))
        return;
    Add(0, 0, 0);
    dmResource::HPreloader request = Request(0);
    ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(0u, m_Creates[0]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
}

// A resident resource outlives its request and later requests reuse its node without another preload.
TEST_P(ResourceLoaderTest, ResidentResourceAfterRequestDeletion)
{
    Add(0, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &resource));
    Delete(0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    Delete(1);
    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Supplied-byte creation joins an existing file load and must not replace its job's input buffer.
TEST_P(ResourceLoaderTest, SuppliedBytesJoinInFlightLoad)
{
    Add(0, 0, 0);
    Request(0);
    uint32_t replacement[] = { 7, 0 };
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::CreateResource(m_Factory, "/node_0.node", replacement, sizeof(replacement), &resource));
    ASSERT_EQ(0u, ((LoadedNode*)resource)->m_Value);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(0u, m_Creates[7]);
    Delete(0);
    dmResource::Release(m_Factory, resource);
}

// Type callbacks can trigger nested resource creation during destruction without the factory lock held.
TEST_P(ResourceLoaderTest, CreationCallbacksWithoutResourceLock)
{
    m_CheckLocks = true;
    m_GetDuringDestroy = true;
    Add(0, 0, 0);
    Add(2, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    Delete(0);
    ASSERT_EQ(1u, m_Creates[2]);
    ASSERT_EQ(1u, m_Destroys[2]);
    ASSERT_TRUE(m_CallbacksUnlocked);
}

// A ready leaf can be created while a different branch is still inside dependency discovery.
TEST_P(ResourceLoaderTest, LeafProgressBeforeDiscoveryFinishes)
{
#if defined(DM_HAS_THREADS)
    if (GetParam() != 4)
        return;
    uint32_t dependencies[] = { 1, 2 };
    Add(0, dependencies, 2);
    Add(1, 0, 0);
    Add(2, 0, 0);
    m_Block = 2;
    dmResource::HPreloader request = Request(0);
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_Blocked)
            dmConditionVariable::Wait(m_Changed, m_Mutex);
    }
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[1] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 1000));
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_EQ(0u, m_Creates[0]);
    ASSERT_EQ(0u, m_Creates[2]);
    Unblock();
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
#endif
}

// A background synchronous Get cannot steal an unrelated request's creator or shared job callbacks.
TEST_P(ResourceLoaderTest, SynchronousCallerKeepsUnrelatedWorkOnOwner)
{
#if defined(DM_HAS_THREADS)
    if (!m_Jobs)
        return;
    m_CheckedThreadNode = 0;
    Add(0, 0, 0);
    Add(1, 0, 0);
    Job job = {};
    job.m_Process = FenceProcess;
    job.m_Callback = JobCallback;
    job.m_Context = this;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    dmResource::HPreloader request = Request(0);
    Unblock();
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    dmThread::Thread thread = dmThread::New(GetFromThread, 0x20000, &caller, "UnrelatedGet");
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(0u, m_Creates[0]);
    ASSERT_EQ(0u, m_JobCallbacks);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    ASSERT_TRUE(m_CreationThreadCorrect);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_JobCallbacks && dmTime::GetMonotonicTime() < deadline)
        JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, m_JobCallbacks);
    ASSERT_TRUE(m_JobCallbackThreadCorrect);
    dmResource::Release(m_Factory, caller.m_Resource);
#endif
}

// The asynchronous shared queue also leaves a different thread's resource creation to that caller.
TEST_P(ResourceLoaderTest, AsynchronousCallerKeepsUnrelatedWorkOnOwner)
{
#if defined(DM_HAS_THREADS)
    Add(0, 0, 0);
    Add(1, 0, 0);
    dmResource::HPreloader request = Request(0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    dmThread::Thread thread = dmThread::New(AsyncRequestFromThread, 0x20000, &caller, "AsyncRequest");
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_Blocked)
            dmConditionVariable::Wait(m_Changed, m_Mutex);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    ASSERT_EQ(0u, m_Creates[1]);
    Unblock();
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(1u, m_Creates[1]);
#endif
}

// A nested Get yields creation ownership so another request's thread can create its shared dependency.
TEST_P(ResourceLoaderTest, NestedGetWithForeignAffinity)
{
#if defined(DM_HAS_THREADS)
    m_Hints = false;
    m_CheckedThreadNode = 0;
    m_SignalCreate = 1;
    Add(0, 0, 0);
    uint32_t dependency = 0;
    Add(1, &dependency, 1);
    dmResource::HPreloader request = Request(0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    Unblock();
    dmThread::Thread thread = dmThread::New(GetFromThread, 0x20000, &caller, "NestedGet");
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_Blocked)
            dmConditionVariable::Wait(m_Changed, m_Mutex);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(1u, m_Preloads[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_TRUE(m_CreationThreadCorrect);
    dmResource::Release(m_Factory, caller.m_Resource);
#endif
}

// Resource loading with zero workers does not execute another system's queued work.
TEST_P(ResourceLoaderTest, ZeroWorkersLeaveSharedJobsToOwner)
{
    if (!m_Jobs || JobSystemGetWorkerCount(m_Jobs))
        return;
    Add(0, 0, 0);
    Job job = {};
    job.m_Process = FenceProcess;
    job.m_Callback = JobCallback;
    job.m_Context = this;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &resource));
    ASSERT_EQ(0u, m_Fences);
    ASSERT_EQ(0u, m_JobCallbacks);
    JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, m_Fences);
    ASSERT_EQ(1u, m_JobCallbacks);
    dmResource::Release(m_Factory, resource);
}

// Pending PostCreate advances without dispatching unrelated shared job callbacks.
TEST_P(ResourceLoaderTest, SynchronousPostCreateLeavesSharedCallbacksToOwner)
{
    if (!m_Jobs)
        return;
    EnablePostCreate();
    m_PostPendingOnce = true;
    Add(0, 0, 0);
    Job job = {};
    job.m_Process = FenceProcess;
    job.m_Callback = JobCallback;
    job.m_Context = this;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &resource));
    ASSERT_EQ(0u, m_JobCallbacks);
    ASSERT_EQ(2u, m_Posts[0]);
    if (!JobSystemGetWorkerCount(m_Jobs))
        ASSERT_EQ(0u, m_Fences);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_JobCallbacks && dmTime::GetMonotonicTime() < deadline)
        JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, m_JobCallbacks);
    ASSERT_TRUE(m_JobCallbackThreadCorrect);
    dmResource::Release(m_Factory, resource);
}

// The first resource Get can start on a background thread without taking ownership of shared callbacks.
TEST_P(ResourceLoaderTest, FirstSynchronousCallerPreservesPoolCallbackOwner)
{
#if defined(DM_HAS_THREADS)
    if (!m_Jobs)
        return;
    EnablePostCreate();
    m_PostPendingOnce = true;
    Add(1, 0, 0);
    Job job = {};
    job.m_Process = FenceProcess;
    job.m_Callback = JobCallback;
    job.m_Context = this;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    Unblock();
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    dmThread::Thread thread = dmThread::New(GetFromThread, 0x20000, &caller, "FirstGet");
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(0u, m_JobCallbacks);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_JobCallbacks && dmTime::GetMonotonicTime() < deadline)
        JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, m_JobCallbacks);
    ASSERT_TRUE(m_JobCallbackThreadCorrect);
    dmResource::Release(m_Factory, caller.m_Resource);
#endif
}

// Nested synchronous loading finishes before shared callbacks can re-enter the same resource.
TEST_P(ResourceLoaderTest, NestedGetLeavesSharedCallbacksToOwner)
{
    if (!m_Jobs)
        return;
    EnablePostCreate();
    m_PostPendingOnce = true;
    m_Hints = false;
    m_JobReadsRoot = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(1, 0, 0);
    Job job = {};
    job.m_Process = FenceProcess;
    job.m_Callback = JobCallback;
    job.m_Context = this;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(0u, m_JobCallbacks);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_JobCallbacks && dmTime::GetMonotonicTime() < deadline)
        JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, m_JobCallbacks);
    ASSERT_EQ(dmResource::RESULT_OK, m_JobGetResult);
    ASSERT_EQ(1u, m_Creates[0]);
}

// Completion cleanup for an unrelated failed creator remains on that resource's requesting thread.
TEST_P(ResourceLoaderTest, FailedCompletionStaysOnOwner)
{
#if defined(DM_HAS_THREADS)
    m_CheckedThreadNode = 0;
    m_HintOnly = 0;
    uint32_t missing = 1;
    Add(0, &missing, 1);
    Add(2, 0, 0);
    dmResource::HPreloader request = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(0u, m_Destroys[0]);
    Unblock();
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 2 };
    dmThread::Thread thread = dmThread::New(GetFromThread, 0x20000, &caller, "CompletionGet");
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(0u, m_Destroys[0]);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(request));
    ASSERT_EQ(1u, m_Destroys[0]);
    ASSERT_TRUE(m_CreationThreadCorrect);
    dmResource::Release(m_Factory, caller.m_Resource);
#endif
}

// An unrelated root's PostCreate waits for that request's own completion callback.
TEST_P(ResourceLoaderTest, UnrelatedPostCreateFollowsOwnCallback)
{
    EnablePostCreate();
    uint32_t child = 2;
    Add(0, &child, 1);
    Add(1, 0, 0);
    Add(2, 0, 0);
    dmResource::HPreloader first = Request(0);
    dmResource::HPreloader second = Request(1);
    ASSERT_EQ(dmResource::RESULT_OK, CompleteWithCallback(first, 0));
    ASSERT_TRUE(m_Notified[0]);
    ASSERT_FALSE(m_Notified[1]);
    ASSERT_EQ(0u, m_Posts[1]);
    ASSERT_EQ(dmResource::RESULT_OK, CompleteWithCallback(second, 1));
    ASSERT_TRUE(m_Notified[1]);
    ASSERT_EQ(1u, m_Posts[1]);
}

// Cached Gets in PostCreate detect cycles through already-created nodes instead of hanging completion.
TEST_P(ResourceLoaderTest, PostCreateDependencyCycle)
{
    EnablePostCreate();
    m_HoldPosts = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(1, 0, 0);
    dmResource::HPreloader request = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    uint32_t parent_posts = m_Posts[0];
    uint32_t child_posts = m_Posts[1];
    m_PostReadsParent = true;
    m_HoldPosts = false;
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, Complete(request));
    ASSERT_EQ(parent_posts + 1, m_Posts[0]);
    ASSERT_EQ(child_posts + 1, m_Posts[1]);
    ASSERT_EQ(0u, m_Decoded);
}

// Cached lookups must wait for PostCreate and propagate its failure without exposing a partial resource.
TEST_P(ResourceLoaderTest, GetWaitsForPendingPostCreate)
{
    EnablePostCreate();
    m_PostPendingOnce = true;
    m_FailedPost = 1;
    for (uint32_t id = 0; id < 2; ++id)
    {
        Add(id, 0, 0);
        dmResource::HPreloader request = Request(id);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Posts[id] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        ASSERT_EQ(1u, m_Posts[id]);
        char path[64];
        Path(id, path, sizeof(path));
        void* resource = 0;
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::Get(m_Factory, dmHashString64(path), &resource));
        ASSERT_EQ((void*)0, resource);
        dmResource::Result expected = id ? dmResource::RESULT_INVALID_DATA : dmResource::RESULT_OK;
        ASSERT_EQ(expected, dmResource::Get(m_Factory, path, &resource));
        ASSERT_EQ(2u, m_Posts[id]);
        ASSERT_EQ(expected, Complete(request));
        if (resource)
            dmResource::Release(m_Factory, resource);
        else
            ASSERT_EQ(1u, id);
        Delete(id);
        ASSERT_EQ(1u, m_Destroys[id]);
    }
}

// Nested Get can share a pending dependency without blocking the outer callback that releases it; later failures still propagate.
TEST_P(ResourceLoaderTest, NestedGetSharedPendingPost)
{
    EnablePostCreate();
    m_Transitive = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(1, 0, 0);
    Add(2, &child, 1);
    for (uint32_t failure = 0; failure < 2; ++failure)
    {
        m_Notified[0] = false;
        m_HeldPost = child;
        m_FailedPost = failure ? child : ~0u;
        dmResource::HPreloader request = Request(0);
        Notification notification = { this, 0 };
        dmResource::PreloaderCompleteCallbackParams params = { m_Factory, &notification };
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        dmResource::Result result;
        do
        {
            result = dmResource::UpdatePreloader(request, NotifyAndReleasePost, &params, 1000);
        } while (result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
        ASSERT_EQ(failure ? dmResource::RESULT_INVALID_DATA : dmResource::RESULT_OK, result);
        ASSERT_TRUE(m_Notified[0]);
        ASSERT_EQ(failure + 1, m_Posts[2]);
        Delete(failure);
        for (uint32_t i = 0; i < 3; ++i)
        {
            ASSERT_EQ(failure + 1, m_Preloads[i]);
            ASSERT_EQ(failure + 1, m_Creates[i]);
            ASSERT_EQ(failure + 1, m_Destroys[i]);
        }
    }
}

// Nested Get can finish a dependency owned by another caller while that dependency's shared child still awaits PostCreate.
TEST_P(ResourceLoaderTest, NestedGetWithForeignPendingPost)
{
#if defined(DM_HAS_THREADS)
    EnablePostCreate();
    m_Hints = false;
    m_HeldPost = 2;
    m_CheckedThreadNode = 0;
    m_SignalCreate = 1;
    Add(2, 0, 0);
    uint32_t child = 2;
    Add(0, &child, 1);
    child = 0;
    Add(1, &child, 1);
    dmResource::HPreloader pending = Request(2);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[2] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(pending, 0, 0, 0));
    ASSERT_EQ(1u, m_Creates[2]);
    dmResource::HPreloader request = Request(0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    dmThread::Thread thread = dmThread::New(CompleteAndReleasePostFromThread, 0x20000, &caller, "NestedPost");
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        while (!m_Blocked)
            dmConditionVariable::Wait(m_Changed, m_Mutex);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_TRUE(m_Notified[1]);
    ASSERT_TRUE(m_CreationThreadCorrect);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(pending));
    Delete(1);
    Delete(0);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(1u, m_Preloads[i]);
        ASSERT_EQ(1u, m_Creates[i]);
        ASSERT_EQ(1u, m_Destroys[i]);
    }
#endif
}

// Recreate can load a new intermediary sharing an unfinished dependency, while external lookups still wait for readiness.
TEST_P(ResourceLoaderTest, RecreateGetSharedPendingPost)
{
    EnablePostCreate();
    for (uint32_t mode = 0; mode < 3; ++mode)
    {
        uint32_t root = mode * 3;
        uint32_t child = root + 1;
        uint32_t middle = root + 2;
        Add(root, 0, 0);
        Add(child, 0, 0);
        Add(middle, &child, 1);
        ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(root)));
        m_HeldPost = child;
        dmResource::HPreloader pending = Request(child);
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!m_Creates[child] && dmTime::GetMonotonicTime() < deadline)
            ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(pending, 0, 0, 0));
        ASSERT_EQ(1u, m_Creates[child]);
        ASSERT_EQ(dmResource::RESULT_OK, Replace(root, &middle, 1, mode));
        ASSERT_EQ(1u, m_Posts[middle]);
        char path[64];
        Path(root, path, sizeof(path));
        void* resource = 0;
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::Get(m_Factory, dmHashString64(path), &resource));
        ASSERT_EQ((void*)0, resource);
        m_HeldPost = ~0u;
        ASSERT_EQ(dmResource::RESULT_OK, Complete(pending));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, dmHashString64(path), &resource));
        dmResource::Release(m_Factory, resource);
        Delete(mode * 2);
        Delete(mode * 2 + 1);
        for (uint32_t i = root; i <= middle; ++i)
        {
            ASSERT_EQ(1u, m_Preloads[i]);
            ASSERT_EQ(1u, m_Creates[i]);
            ASSERT_EQ(1u, m_Destroys[i]);
        }
    }
}

// Completion callbacks can release a newly loaded intermediary before its shared dependency finishes, without stranding cleanup.
TEST_P(ResourceLoaderTest, CompletionCallbackLoadsSharedPendingPost)
{
    EnablePostCreate();
    m_HeldPost = 1;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(1, 0, 0);
    Add(2, &child, 1);
    dmResource::HPreloader request = Request(0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 2 };
    dmResource::PreloaderCompleteCallbackParams params = { m_Factory, &caller };
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    dmResource::Result result;
    do
    {
        result = dmResource::UpdatePreloader(request, LoadAndReleaseSharedDependency, &params, 1000);
    } while (result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(1u, m_Posts[2]);
    ASSERT_EQ(1u, m_Destroys[2]);
    Delete(0);
    for (uint32_t i = 0; i < 3; ++i)
    {
        ASSERT_EQ(1u, m_Preloads[i]);
        ASSERT_EQ(1u, m_Creates[i]);
        ASSERT_EQ(1u, m_Destroys[i]);
    }
}

// Completion callbacks can still acquire created roots before PostCreate, as collection initialization requires.
TEST_P(ResourceLoaderTest, CompletionCallbackGetsCreatedResource)
{
    EnablePostCreate();
    Add(0, 0, 0);
    dmResource::HPreloader request = Request(0);
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 0 };
    dmResource::PreloaderCompleteCallbackParams params = { m_Factory, &caller };
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    dmResource::Result result;
    do
    {
        result = dmResource::UpdatePreloader(request, GetCreatedResource, &params, 1000);
    } while (result == dmResource::RESULT_PENDING && dmTime::GetMonotonicTime() < deadline);
    ASSERT_EQ(dmResource::RESULT_OK, result);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_NE((void*)0, caller.m_Resource);
    dmResource::Release(m_Factory, caller.m_Resource);
}

// Reject original paths before canonicalization can turn them into the name of a different resource.
TEST_P(ResourceLoaderTest, RejectOverlongRootPaths)
{
    char prefix[dmResource::RESOURCE_PATH_MAX - 1];
    memset(prefix, 'a', sizeof(prefix));
    prefix[0] = '/';
    memcpy(prefix + sizeof(prefix) - 6, ".node", 6);
    uint32_t data[] = { 0, 0 };
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, prefix, sizeof(data), data));
    void* valid = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, prefix, &valid));
    char path[dmResource::RESOURCE_PATH_MAX + 16];
    dmSnPrintf(path, sizeof(path), "%s/extra.node", prefix);
    void* resource = valid;
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, dmResource::Get(m_Factory, path, &resource));
    ASSERT_EQ((void*)0, resource);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, dmResource::CreateResource(m_Factory, path, data, sizeof(data), &resource));
    ASSERT_EQ((void*)0, resource);
    dmResource::HPreloader request = dmResource::NewPreloader(m_Factory, path);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, CompleteWithCallback(request, 0));
    dmResource::DeletePreloader(request);
    dmArray<const char*> names;
    names.SetCapacity(2);
    names.Push(path);
    names.Push(prefix);
    request = dmResource::NewPreloader(m_Factory, names);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, CompleteWithCallback(request, 0));
    dmResource::DeletePreloader(request);
    ASSERT_FALSE(m_Notified[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    dmResource::Release(m_Factory, valid);
    ASSERT_EQ(1u, m_Destroys[0]);
}

// Nested PostCreate Get grows the pool and retries an earlier pending dependency while the outer update is posting.
TEST_P(ResourceLoaderTest, NestedGetDuringPendingPostCreate)
{
    EnablePostCreate();
    m_HoldPosts = true;
    m_PostLoadsSibling = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(1, 0, 0);
    const uint32_t count = 128;
    uint32_t dependencies[count];
    dependencies[0] = child;
    for (uint32_t i = 1; i < count; ++i)
    {
        dependencies[i] = i + 2;
        Add(i + 2, 0, 0);
    }
    Add(2, dependencies, count);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    for (uint32_t i = 0; i <= count + 1; ++i)
        ASSERT_EQ(1u, m_Creates[i]);
    ASSERT_EQ(1u, m_Posts[0]);
    ASSERT_GT(m_Posts[1], 1u);
}

// A late hint-only parent inherits a shared child's error even if the child has already completed.
TEST_P(ResourceLoaderTest, CompletedDependencyFailure)
{
    dmResource::HPreloader failed = Request(1);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(failed));
    uint32_t child = 1;
    m_HintOnly = 0;
    Add(0, &child, 1);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, Complete(Request(0)));
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(0u, m_Decoded);
}

// Late dependants also receive errors from a previously completed PostCreate callback.
TEST_P(ResourceLoaderTest, CompletedDependencyPostCreateFailure)
{
    EnablePostCreate();
    m_FailedPost = 1;
    Add(1, 0, 0);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(Request(1)));
    uint32_t child = 1;
    m_HintOnly = 0;
    Add(0, &child, 1);
    ASSERT_EQ(dmResource::RESULT_INVALID_DATA, Complete(Request(0)));
    ASSERT_EQ(1u, m_Preloads[1]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_EQ(0u, m_Decoded);
}

// Staged inputs stop admitting an entire decoded fan-out while ordinary worker fences still progress.
TEST_P(ResourceLoaderTest, StagingBackpressure)
{
    if (!m_Jobs || JobSystemGetWorkerCount(m_Jobs) != 1)
        return;
    const uint32_t count = 16;
    const uint32_t bytes = 1024 * 1024;
    uint32_t children[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        children[i] = i + 1;
        AddSized(i + 1, 0, 0, bytes);
    }
    Add(0, children, count);
    dmResource::HPreloader request = Request(0);
    for (uint32_t i = 0; i < count * 2; ++i)
        Fence();
    {
        DM_MUTEX_SCOPED_LOCK(m_Mutex);
        ASSERT_LT(m_DecodedBytes, (uint64_t)bytes * count);
        ASSERT_GT(m_Decoded, 1u);
    }
    ASSERT_EQ(0u, m_Creates[0]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    ASSERT_EQ(0u, m_Decoded);
}

// An oversized parent chain must reach a leaf even when its own retained input exceeds the byte budget.
TEST_P(ResourceLoaderTest, OversizedDependencyChainProgress)
{
    const uint32_t bytes = 5 * 1024 * 1024;
    uint32_t child = 1;
    AddSized(0, &child, 1, bytes);
    child = 2;
    AddSized(1, &child, 1, bytes);
    Add(2, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Creates[1]);
    ASSERT_EQ(1u, m_Creates[2]);
}

// An unrelated caller can finish its own branch while a different request fills the staging budget.
TEST_P(ResourceLoaderTest, SynchronousProgressUnderStagingPressure)
{
#if defined(DM_HAS_THREADS)
    if (!m_Jobs)
        return;
    AddSized(0, 0, 0, 5 * 1024 * 1024);
    Add(1, 0, 0);
    dmResource::HPreloader request = Request(0);
    Fence();
    Fence();
    Unblock();
    GetCaller caller = { this, 0, dmResource::RESULT_PENDING, 1 };
    dmThread::Thread thread = dmThread::New(GetFromThread, 0x20000, &caller, "BudgetGet");
    dmThread::Join(thread);
    ASSERT_EQ(dmResource::RESULT_OK, caller.m_Result);
    ASSERT_EQ(0u, m_Creates[0]);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
    dmResource::Release(m_Factory, caller.m_Resource);
#endif
}

// A zero soft budget retries at most one pending post per update and rotates through all ready posts.
TEST_P(ResourceLoaderTest, PostCreateBudgetAndFairness)
{
    EnablePostCreate();
    m_HoldPosts = true;
    const uint32_t count = 64;
    uint32_t children[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        children[i] = i + 1;
        Add(i + 1, 0, 0);
    }
    Add(0, children, count);
    dmResource::HPreloader request = Request(0);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!m_Creates[0] && dmTime::GetMonotonicTime() < deadline)
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 1000000));
    ASSERT_EQ(1u, m_Creates[0]);
    uint32_t before[count + 1];
    memcpy(before, m_Posts, sizeof(before));
    for (uint32_t i = 0; i <= count; ++i)
    {
        uint32_t previous = 0;
        for (uint32_t j = 0; j <= count; ++j)
            previous += m_Posts[j];
        ASSERT_EQ(dmResource::RESULT_PENDING, dmResource::UpdatePreloader(request, 0, 0, 0));
        uint32_t after = 0;
        for (uint32_t j = 0; j <= count; ++j)
            after += m_Posts[j];
        ASSERT_EQ(previous + 1, after);
    }
    for (uint32_t i = 0; i <= count; ++i)
        ASSERT_EQ(before[i] + 1, m_Posts[i]);
    m_HoldPosts = false;
    ASSERT_EQ(dmResource::RESULT_OK, Complete(request));
}

const int resource_loader_workers[] = { -1, 0, 1, 4 };
INSTANTIATE_TEST_CASE_P(WorkerCounts, ResourceLoaderTest, jc_test_values_in(resource_loader_workers));

extern "C" void dmExportedSymbols();
int main(int argc, char** argv)
{
    dmExportedSymbols();
    dmLog::LogParams log_params;
    dmLog::LogInitialize(&log_params);
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmLog::LogFinalize();
    return result;
}
