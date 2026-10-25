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
        m_PostWaitsForJob = false;
        m_HoldPosts = false;
        m_FailedPost = ~0u;
        m_CheckedThreadNode = ~0u;
        m_SignalCreate = ~0u;
        m_ExpectedThread = dmThread::GetCurrentThread();
        m_CreationThreadCorrect = true;
        m_JobCallbackThreadCorrect = true;
        m_JobCallbacks = 0;
        m_JobReadsRoot = false;
        m_JobGetResult = dmResource::RESULT_PENDING;
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
        ++self->m_Posts[resource->m_Value];
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
            void* sibling = 0;
            dmResource::Result result = dmResource::Get(params->m_Factory, "/node_2.node", &sibling);
            if (result != dmResource::RESULT_OK)
                return result;
            dmResource::Release(params->m_Factory, sibling);
        }
        if (self->m_HoldPosts)
            return dmResource::RESULT_PENDING;
        if (self->m_PostWaitsForJob && !self->m_JobCallbacks)
            return dmResource::RESULT_PENDING;
        return resource->m_Value == self->m_FailedPost ? dmResource::RESULT_INVALID_DATA : dmResource::RESULT_OK;
    }

    static dmResource::Result Recreate(const dmResource::ResourceRecreateParams* params)
    {
        const uint32_t* data = (const uint32_t*)(params->m_Message ? params->m_Message : params->m_Buffer);
        LoadedNode* resource = (LoadedNode*)ResourceDescriptorGetResource(params->m_Resource);
#if defined(DM_HAS_THREADS)
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
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
        if (result == dmResource::RESULT_OK)
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

    static dmResource::Result Destroy(const dmResource::ResourceDestroyParams* params)
    {
        ResourceLoaderTest* self = (ResourceLoaderTest*)params->m_Context;
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
    bool m_PostWaitsForJob;
    bool m_HoldPosts;
    uint32_t m_FailedPost;
    uint32_t m_CheckedThreadNode;
    uint32_t m_SignalCreate;
    dmThread::Thread m_ExpectedThread;
    bool m_CreationThreadCorrect;
    bool m_JobCallbackThreadCorrect;
    uint32_t m_JobCallbacks;
    bool m_JobReadsRoot;
    dmResource::Result m_JobGetResult;
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
    dmResource::HPreloader large = Request(1);
    dmResource::HPreloader small = Request(0);
    ASSERT_EQ(dmResource::RESULT_OK, CompleteWithCallback(small, 0));
    ASSERT_TRUE(m_Notified[0]);
    ASSERT_EQ(1u, m_Creates[0]);
    ASSERT_EQ(1u, m_Posts[0]);
    for (uint32_t i = 1; i < count + 2; ++i)
    {
        ASSERT_EQ(0u, m_Creates[i]);
        ASSERT_EQ(0u, m_Posts[i]);
    }
    ASSERT_EQ(dmResource::RESULT_OK, Complete(large));
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

// A synchronous load on the pool owner can complete a PostCreate waiting for a job's completion callback.
TEST_P(ResourceLoaderTest, SynchronousOwnerDispatchesPendingJobCallbacks)
{
    if (!m_Jobs)
        return;
    EnablePostCreate();
    m_PostWaitsForJob = true;
    Add(0, 0, 0);
    Job job = {};
    job.m_Process = FenceProcess;
    job.m_Callback = JobCallback;
    job.m_Context = this;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/node_0.node", &resource));
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

// A main-thread job callback cannot recurse into a suspended creator or add a false dependency to it.
TEST_P(ResourceLoaderTest, JobCallbackCannotReenterSuspendedCreator)
{
    if (!m_Jobs)
        return;
    EnablePostCreate();
    m_PostWaitsForJob = true;
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
    ASSERT_EQ(1u, m_JobCallbacks);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, m_JobGetResult);
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
    m_PostReadsParent = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(1, 0, 0);
    ASSERT_EQ(dmResource::RESULT_RESOURCE_LOOP_ERROR, Complete(Request(0)));
    ASSERT_EQ(1u, m_Posts[0]);
    ASSERT_EQ(1u, m_Posts[1]);
    ASSERT_EQ(0u, m_Decoded);
}

// Nested PostCreate Get can retry an earlier pending dependency while the outer update is still posting.
TEST_P(ResourceLoaderTest, NestedGetDuringPendingPostCreate)
{
    EnablePostCreate();
    m_HoldPosts = true;
    m_PostLoadsSibling = true;
    uint32_t child = 1;
    Add(0, &child, 1);
    Add(2, &child, 1);
    Add(1, 0, 0);
    ASSERT_EQ(dmResource::RESULT_OK, Complete(Request(0)));
    ASSERT_EQ(1u, m_Creates[2]);
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
