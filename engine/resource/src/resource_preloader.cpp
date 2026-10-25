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
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#if defined(DM_HAS_THREADS)
#include <dlib/condition_variable.h>
#endif

#include "resource.h"
#include "resource_private.h"
#include "resource_util.h"

struct ResourcePreloader
{
    dmResource::LoadContext* m_Context;
    dmArray<dmResource::LoadNode*> m_Roots;
    dmResource::Result m_CallbackResult;
    bool m_CallbackCalled;
    bool m_Holding;
    bool m_Blocking;
};

namespace dmResource
{
    static const uint32_t INVALID_LOAD_INDEX = 0xffffffff;
    static const uint64_t STAGED_INPUT_LIMIT = 4 * 1024 * 1024;

    // These buffers and callback parameters exist only while the resource is loading.
    struct LoadData
    {
        LoadBufferType m_Buffer;
        ResourceDescriptor m_Descriptor;
        LoadProfile m_Profile;
        void* m_PreloadData;
        const void* m_ProvidedData;
        uint32_t m_Caller;
        uint32_t m_Selection;
        uint64_t m_StagedBytes;
        uint32_t m_BufferSize;
        uint32_t m_FileSize;
        Result m_ReadResult;
        Result m_PreloadResult;
        bool m_Provided;
        bool m_BufferTransferred;
        bool m_Destroy;
        bool m_ReadStarted;
        bool m_InCreate;
    };

    struct LoadNode
    {
        char* m_Path;
        HResourceType m_Type;
        LoadData* m_Load;
        void* m_Resource;
        dmhash_t m_PathHash;
        uint32_t m_Index;
        uint32_t m_FirstDependency;
        uint32_t m_FirstDependent;
        uint32_t m_PendingDependencies;
        uint32_t m_PendingCompletions;
        uint32_t m_Holds;
        uint32_t m_References;
        uint32_t m_RootReferences;
        uint32_t m_Visited;
        Result m_CreateResult;
        Result m_Result;
        Result m_DependencyResult;
        bool m_Discovered;
        bool m_CreateQueued;
        bool m_Created;
        bool m_PostDone;
        bool m_CompleteQueued;
        bool m_Complete;
        bool m_Pin;
        bool m_PinQueued;
    };

    // One edge per distinct parent/dependency pair. Indices survive array growth.
    struct LoadEdge
    {
        uint32_t m_Parent;
        uint32_t m_Child;
        uint32_t m_NextDependency;
        uint32_t m_NextDependent;
        uint32_t m_PreviousDependent;
    };

    struct LoadQueue
    {
        dmArray<LoadNode*> m_Items;
        uint32_t m_Front;
    };

    struct RecreateContext
    {
        LoadContext* m_Context;
        LoadNode* m_Node;
        LoadNode* m_PreviousCreating;
        RecreateContext* m_Previous;
        dmArray<LoadNode*> m_Dependencies;
        uint32_t m_FirstDependency;
        Result m_DependencyResult;
    };

    enum LoadWork
    {
        LOAD_WORK_READ,
        LOAD_WORK_CREATE,
        LOAD_WORK_POST,
        LOAD_WORK_COMPLETE
    };

    struct LoadContext
    {
        HFactory m_Factory;
        HJobContext m_Jobs;
        dmMutex::HMutex m_Mutex;
#if defined(DM_HAS_THREADS)
        dmConditionVariable::HConditionVariable m_Changed;
        dmThread::TlsKey m_CallerKey;
#endif
        dmHashTable64<LoadNode*> m_ByPath;
        dmArray<LoadNode*> m_Nodes;
        dmArray<uint32_t> m_FreeNodes;
        dmArray<LoadEdge> m_Edges;
        dmArray<uint32_t> m_FreeEdges;
        dmArray<LoadNode*> m_Visit;
        LoadQueue m_Reads;
        LoadQueue m_Creates;
        LoadQueue m_Completions;
        LoadQueue m_Pins;
        LoadQueue m_Posts;
        LoadNode* m_Creating;
        RecreateContext* m_Recreating;
        HPreloader m_SelectedRequest;
        uint32_t m_Selection;
        uint32_t m_UpdateOwner;
        uint32_t m_NextCaller;
        uint32_t m_UpdateDepth;
        uint32_t m_ActiveReads;
        uint32_t m_ReadLimit;
        uint32_t m_VisitGeneration;
        uint32_t m_Requests;
        uint64_t m_StagedBytes;
    };

    template <typename T>
    static void Push(dmArray<T>& items, const T& item)
    {
        if (items.Full())
            items.SetCapacity(items.Capacity() ? items.Capacity() * 2 : 16);
        items.Push(item);
    }

    static void Push(LoadQueue& queue, LoadNode* node)
    {
        Push(queue.m_Items, node);
    }

    static LoadNode* Pop(LoadQueue& queue)
    {
        if (queue.m_Front == queue.m_Items.Size())
            return 0;
        LoadNode* node = queue.m_Items[queue.m_Front++];
        if (queue.m_Front == queue.m_Items.Size())
        {
            queue.m_Front = 0;
            queue.m_Items.SetSize(0);
        }
        return node;
    }

    static LoadNode* PopLast(LoadQueue& queue)
    {
        if (queue.m_Front == queue.m_Items.Size())
            return 0;
        LoadNode* node = queue.m_Items.Back();
        queue.m_Items.Pop();
        if (queue.m_Front == queue.m_Items.Size())
        {
            queue.m_Front = 0;
            queue.m_Items.SetSize(0);
        }
        return node;
    }

    // Called with the graph lock. Thread handles are not portable identities:
    // Windows GetCurrentThread returns the same pseudo-handle on every thread.
    static uint32_t GetCaller(LoadContext* context)
    {
#if defined(DM_HAS_THREADS)
        uintptr_t caller = (uintptr_t)dmThread::GetTlsValue(context->m_CallerKey);
        if (!caller)
        {
            caller = ++context->m_NextCaller;
            dmThread::SetTlsValue(context->m_CallerKey, (void*)caller);
        }
        return (uint32_t)caller;
#else
        (void)context;
        return 1;
#endif
    }

    static void Signal(LoadContext* context)
    {
#if defined(DM_HAS_THREADS)
        dmConditionVariable::Broadcast(context->m_Changed);
#endif
    }

    static void QueueCreate(LoadContext* context, LoadNode* node)
    {
        if (!node->m_Discovered || node->m_PendingDependencies || node->m_Created || node->m_CreateQueued)
            return;
        node->m_CreateQueued = true;
        Push(context->m_Creates, node);
    }

    static void QueueComplete(LoadContext* context, LoadNode* node)
    {
        if (!node->m_PostDone || node->m_PendingCompletions || node->m_Complete || node->m_CompleteQueued)
            return;
        node->m_CompleteQueued = true;
        Push(context->m_Completions, node);
    }

    static void QueuePin(LoadContext* context, LoadNode* node)
    {
        if (!node->m_Pin || node->m_PinQueued || (node->m_Holds && !node->m_RootReferences) || !node->m_PostDone)
            return;
        node->m_PinQueued = true;
        Push(context->m_Pins, node);
    }

    static LoadNode* FindRequestWork(HPreloader request, LoadQueue& queue, LoadWork work, bool remove);
    static void StartReads(LoadContext* context, HPreloader request);

    // Requests retain their reachable resources, including hints consumed by a
    // later ancestor's Create. Propagate only when a node enters or leaves that set.
    static void ChangeHold(LoadContext* context, LoadNode* node, bool hold)
    {
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, node);
        while (!context->m_Visit.Empty())
        {
            LoadNode* next = context->m_Visit.Back();
            context->m_Visit.Pop();
            if (hold)
            {
                if (++next->m_Holds != 1)
                    continue;
            }
            else
            {
                assert(next->m_Holds);
                if (--next->m_Holds)
                    continue;
                QueuePin(context, next);
            }
            for (uint32_t edge = next->m_FirstDependency; edge != INVALID_LOAD_INDEX; edge = context->m_Edges[edge].m_NextDependency)
                Push(context->m_Visit, context->m_Nodes[context->m_Edges[edge].m_Child]);
        }
    }

    static uint32_t VisitGeneration(LoadContext* context)
    {
        ++context->m_VisitGeneration;
        if (!context->m_VisitGeneration)
        {
            for (uint32_t i = 0; i < context->m_Nodes.Size(); ++i)
                if (context->m_Nodes[i])
                    context->m_Nodes[i]->m_Visited = 0;
            ++context->m_VisitGeneration;
        }
        return context->m_VisitGeneration;
    }

    // Selection lives only in loading data. Extend it as workers discover new
    // edges, so a request need not traverse its entire graph for each operation.
    static void SelectDependencies(LoadContext* context, LoadNode* root)
    {
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, root);
        while (!context->m_Visit.Empty())
        {
            LoadNode* node = context->m_Visit.Back();
            context->m_Visit.Pop();
            if (node->m_Complete || node->m_Load->m_Selection == context->m_Selection)
                continue;
            node->m_Load->m_Selection = context->m_Selection;
            for (uint32_t edge = node->m_FirstDependency; edge != INVALID_LOAD_INDEX; edge = context->m_Edges[edge].m_NextDependency)
                Push(context->m_Visit, context->m_Nodes[context->m_Edges[edge].m_Child]);
        }
    }

    static void SelectRequest(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        if (context->m_SelectedRequest == request)
            return;
        if (!++context->m_Selection)
        {
            for (uint32_t i = 0; i < context->m_Nodes.Size(); ++i)
                if (context->m_Nodes[i] && context->m_Nodes[i]->m_Load)
                    context->m_Nodes[i]->m_Load->m_Selection = 0;
            ++context->m_Selection;
        }
        context->m_SelectedRequest = request;
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
            SelectDependencies(context, request->m_Roots[i]);
    }

    static bool DependsOn(LoadContext* context, LoadNode* node, LoadNode* dependency)
    {
        if (node->m_FirstDependency == INVALID_LOAD_INDEX && node != dependency)
            return false;
        uint32_t generation = VisitGeneration(context);
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, node);
        while (!context->m_Visit.Empty())
        {
            LoadNode* next = context->m_Visit.Back();
            context->m_Visit.Pop();
            if (next == dependency)
                return true;
            if (next->m_Visited == generation)
                continue;
            next->m_Visited = generation;
            for (uint32_t edge = next->m_FirstDependency; edge != INVALID_LOAD_INDEX; edge = context->m_Edges[edge].m_NextDependency)
                Push(context->m_Visit, context->m_Nodes[context->m_Edges[edge].m_Child]);
        }
        return false;
    }

    static LoadNode* FindOrCreate(LoadContext* context, const char* path, HResourceType type, uint32_t caller)
    {
        char canonical[RESOURCE_PATH_MAX];
        GetCanonicalPath(path, canonical, sizeof(canonical));
        dmhash_t hash = dmHashString64(canonical);
        LoadNode** found = context->m_ByPath.Get(hash);
        if (found)
            return *found;

        LoadNode* node = new LoadNode();
        node->m_Path = strdup(canonical);
        node->m_Load = new LoadData();
        node->m_Load->m_Caller = caller;
        LoadProfileBegin(&node->m_Load->m_Profile, IsLoadProfilingEnabled(context->m_Factory));
        node->m_PathHash = hash;
        node->m_FirstDependency = INVALID_LOAD_INDEX;
        node->m_FirstDependent = INVALID_LOAD_INDEX;
        node->m_CreateResult = RESULT_PENDING;
        node->m_Result = RESULT_PENDING;
        node->m_DependencyResult = CheckSuppliedResourcePath(path);
        node->m_Type = type;
        if (node->m_DependencyResult == RESULT_OK && !type)
        {
            const char* ext = GetExtFromPath(canonical);
            node->m_Type = ext ? FindResourceType(context->m_Factory, ext) : 0;
            if (!node->m_Type)
                node->m_DependencyResult = ext ? RESULT_UNKNOWN_RESOURCE_TYPE : RESULT_MISSING_FILE_EXTENSION;
        }
        if (context->m_FreeNodes.Empty())
        {
            node->m_Index = context->m_Nodes.Size();
            Push(context->m_Nodes, node);
        }
        else
        {
            node->m_Index = context->m_FreeNodes.Back();
            context->m_FreeNodes.Pop();
            context->m_Nodes[node->m_Index] = node;
        }
        if (context->m_ByPath.Full())
            context->m_ByPath.OffsetCapacity(context->m_ByPath.Capacity() ? context->m_ByPath.Capacity() : 16);
        context->m_ByPath.Put(hash, node);
        Push(context->m_Reads, node);
        return node;
    }

    static int32_t ReadResourceJob(HJobContext jobs, HJob job, void* user_context, void* user_data)
    {
        LoadContext* context = (LoadContext*)user_context;
        LoadNode* node = (LoadNode*)user_data;
        LoadData* load = node->m_Load;
        {
            DM_PROFILE("ResourceLoad");
            DM_PROFILE_DYN(node->m_Path, 0);
            if (!load->m_Provided)
                LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_QUEUE, load->m_Profile.m_StartTime);
            if (!load->m_Provided)
            {
                uint32_t size = ResourceTypeIsStreaming(node->m_Type) ? ResourceTypeGetPreloadSize(node->m_Type) : RESOURCE_INVALID_PRELOAD_SIZE;
                uint64_t start = LoadProfileTime(&load->m_Profile);
                load->m_ReadResult = LoadResourceToBuffer(context->m_Factory, node->m_Path, node->m_Path, size, &load->m_FileSize, &load->m_BufferSize, &load->m_Buffer);
                LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_READ, start);
                if (load->m_ReadResult == RESULT_OK && !load->m_BufferSize)
                    load->m_Buffer.SetCapacity(1);
            }
            if (load->m_ReadResult == RESULT_OK)
            {
                load->m_PreloadResult = RESULT_OK;
                if (node->m_Type->m_PreloadFunction)
                {
                    ResourcePreloadHintInfo hint = { context, node };
                    ResourcePreloadParams params = {};
                    params.m_Factory = context->m_Factory;
                    params.m_Type = node->m_Type;
                    params.m_Context = node->m_Type->m_Context;
                    params.m_Buffer = load->m_Provided ? load->m_ProvidedData : load->m_Buffer.Begin();
                    params.m_BufferSize = load->m_BufferSize;
                    params.m_FileSize = load->m_FileSize;
                    params.m_IsBufferPartial = load->m_BufferSize != load->m_FileSize;
                    params.m_IsBufferTransferrable = !load->m_Provided;
                    params.m_Filename = node->m_Path;
                    params.m_HintInfo = &hint;
                    params.m_PreloadData = &load->m_PreloadData;
                    params.m_IsBufferOwnershipTransferred = load->m_Provided ? 0 : &load->m_BufferTransferred;
                    DM_PROFILE("ResourcePreload");
                    uint64_t start = LoadProfileTime(&load->m_Profile);
                    load->m_PreloadResult = (Result)node->m_Type->m_PreloadFunction(&params);
                    LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_PRELOAD, start);
                }
            }
        }
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        // Types own their decoded allocation sizes. Input bytes provide
        // backpressure until Create/PostCreate consumes the associated payload.
        load->m_StagedBytes = load->m_Provided ? load->m_BufferSize : load->m_Buffer.Capacity();
        context->m_StagedBytes += load->m_StagedBytes;
        node->m_Discovered = true;
        QueueCreate(context, node);
        --context->m_ActiveReads;
        StartReads(context, 0);
        Signal(context);
        return 0;
    }

    // The graph is unbounded. Input-byte backpressure may overshoot by active
    // reads and an unfinished dependency chain, which must still reach a leaf.
    // A caller can advance its own chain when unrelated staged work is paused.
    static LoadNode* TakeRead(LoadContext* context, HPreloader request)
    {
        LoadNode* node;
        if (context->m_StagedBytes >= STAGED_INPUT_LIMIT)
        {
            if (context->m_ActiveReads)
                return 0;
            if (request)
            {
                if (FindRequestWork(request, context->m_Creates, LOAD_WORK_CREATE, false) || FindRequestWork(request, context->m_Posts, LOAD_WORK_POST, false))
                    return 0;
                node = FindRequestWork(request, context->m_Reads, LOAD_WORK_READ, true);
            }
            else
            {
                if (context->m_Creates.m_Front != context->m_Creates.m_Items.Size() || context->m_Posts.m_Front != context->m_Posts.m_Items.Size())
                    return 0;
                node = PopLast(context->m_Reads);
            }
        }
        else
            node = request ? FindRequestWork(request, context->m_Reads, LOAD_WORK_READ, true) : Pop(context->m_Reads);
        if (!node)
            return 0;
        if (!node->m_Load)
            node->m_Load = new LoadData();
        LoadData* load = node->m_Load;
        load->m_ReadStarted = true;
        load->m_ReadResult = RESULT_OK;
        load->m_PreloadResult = RESULT_PENDING;
        if (!node->m_Type)
        {
            load->m_ReadResult = node->m_DependencyResult;
            node->m_Discovered = true;
            QueueCreate(context, node);
        }
        else
            ++context->m_ActiveReads;
        return node;
    }

    static void StartReads(LoadContext* context, HPreloader request)
    {
        while (context->m_ActiveReads < context->m_ReadLimit)
        {
            LoadNode* node = TakeRead(context, request);
            if (!node)
                break;
            if (!node->m_Type)
                continue;
            Job job = {};
            job.m_Process = ReadResourceJob;
            job.m_Context = context;
            job.m_Data = node;
            JobSystemPushJob(context->m_Jobs, JobSystemCreateJob(context->m_Jobs, &job));
        }
    }

    static bool ReadInline(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        LoadNode* node;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            node = TakeRead(context, request);
        }
        // Zero-worker platforms advance only this loader's work, without
        // pumping another system's jobs or holding a lock around Preload.
        if (node && node->m_Type)
            ReadResourceJob(context->m_Jobs, 0, context, node);
        return node != 0;
    }

    LoadContext* NewLoadContext(HFactory factory)
    {
        LoadContext* context = new LoadContext();
        context->m_Factory = factory;
        context->m_Jobs = GetPreloadJobContext(factory);
        context->m_ReadLimit = JobSystemGetWorkerCount(context->m_Jobs);
        context->m_Mutex = dmMutex::New();
#if defined(DM_HAS_THREADS)
        context->m_Changed = dmConditionVariable::New();
        context->m_CallerKey = dmThread::AllocTls();
#endif
        return context;
    }

    static bool AddDependency(LoadContext* context, LoadNode* parent, LoadNode* child)
    {
        uint32_t dependency = parent->m_FirstDependency;
        uint32_t dependent = child->m_FirstDependent;
        // A duplicate occurs in both lists. Stop at the end of the shorter one
        // so large fan-outs and heavily shared leaves both remain inexpensive.
        while (dependency != INVALID_LOAD_INDEX && dependent != INVALID_LOAD_INDEX)
        {
            if (context->m_Edges[dependency].m_Child == child->m_Index || context->m_Edges[dependent].m_Parent == parent->m_Index)
                return true;
            dependency = context->m_Edges[dependency].m_NextDependency;
            dependent = context->m_Edges[dependent].m_NextDependent;
        }
        if (DependsOn(context, child, parent))
        {
            parent->m_DependencyResult = RESULT_RESOURCE_LOOP_ERROR;
            return false;
        }
        uint32_t index;
        if (context->m_FreeEdges.Empty())
        {
            index = context->m_Edges.Size();
            LoadEdge edge = {};
            Push(context->m_Edges, edge);
        }
        else
        {
            index = context->m_FreeEdges.Back();
            context->m_FreeEdges.Pop();
        }
        LoadEdge& edge = context->m_Edges[index];
        edge.m_Parent = parent->m_Index;
        edge.m_Child = child->m_Index;
        edge.m_NextDependency = parent->m_FirstDependency;
        edge.m_NextDependent = child->m_FirstDependent;
        edge.m_PreviousDependent = INVALID_LOAD_INDEX;
        if (child->m_FirstDependent != INVALID_LOAD_INDEX)
            context->m_Edges[child->m_FirstDependent].m_PreviousDependent = index;
        parent->m_FirstDependency = index;
        child->m_FirstDependent = index;
        ++child->m_References;
        if (parent->m_Holds)
            ChangeHold(context, child, true);
        if (!child->m_Created)
            ++parent->m_PendingDependencies;
        else if (child->m_CreateResult != RESULT_OK)
            parent->m_DependencyResult = child->m_CreateResult;
        if (!child->m_Complete)
            ++parent->m_PendingCompletions;
        else if (child->m_Result != RESULT_OK)
            parent->m_DependencyResult = child->m_Result;
        if (context->m_SelectedRequest && parent->m_Load && parent->m_Load->m_Selection == context->m_Selection)
            SelectDependencies(context, child);
        return true;
    }

    bool PreloadHint(HResourcePreloadHintInfo info, const char* path)
    {
        if (!info || !path || CheckSuppliedResourcePath(path) != RESULT_OK || strlen(path) >= RESOURCE_PATH_MAX)
            return false;
        LoadContext* context = info->m_Context;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        LoadNode* child = FindOrCreate(context, path, 0, info->m_Node->m_Load->m_Caller);
        bool added = AddDependency(context, info->m_Node, child);
        StartReads(context, 0);
        return added;
    }

    static bool RecordDependency(LoadContext* context, LoadNode* child)
    {
        LoadNode* parent = context->m_Creating;
        if (!AddDependency(context, parent, child))
            return false;
        RecreateContext* recreate = context->m_Recreating;
        if (recreate && recreate->m_Node == parent)
        {
            for (uint32_t i = 0; i < recreate->m_Dependencies.Size(); ++i)
                if (recreate->m_Dependencies[i] == child)
                    return true;
            Push(recreate->m_Dependencies, child);
        }
        return true;
    }

    Result RecordResourceLoadGet(LoadContext* context, dmhash_t path)
    {
        if (!context)
            return RESULT_OK;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        if (!context->m_UpdateDepth || context->m_UpdateOwner != GetCaller(context) || !context->m_Creating)
            return RESULT_OK;
        LoadNode** child = context->m_ByPath.Get(path);
        return !child || RecordDependency(context, *child) ? RESULT_OK : RESULT_RESOURCE_LOOP_ERROR;
    }

    static void Created(LoadContext* context, LoadNode* node)
    {
        node->m_Created = true;
        for (uint32_t i = node->m_FirstDependent; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependent)
        {
            LoadNode* parent = context->m_Nodes[context->m_Edges[i].m_Parent];
            assert(parent->m_PendingDependencies);
            --parent->m_PendingDependencies;
            if (node->m_CreateResult != RESULT_OK)
                parent->m_DependencyResult = node->m_CreateResult;
            QueueCreate(context, parent);
        }
        QueuePin(context, node);
        QueueComplete(context, node);
    }

    static void FreeBuffer(LoadData* load)
    {
        if (load->m_BufferTransferred)
            memset(&load->m_Buffer, 0, sizeof(load->m_Buffer));
        load->m_Buffer.SetCapacity(0);
    }

    // Called with the graph lock after the type has consumed its preload data.
    static void ReleaseStaging(LoadContext* context, LoadNode* node)
    {
        assert(context->m_StagedBytes >= node->m_Load->m_StagedBytes);
        context->m_StagedBytes -= node->m_Load->m_StagedBytes;
        node->m_Load->m_StagedBytes = 0;
        StartReads(context, 0);
    }

    static void CreateResourceNode(LoadContext* context, LoadNode* node)
    {
        LoadData* load = node->m_Load;
        Result result = load->m_ReadResult != RESULT_OK ? load->m_ReadResult : load->m_PreloadResult;
        bool post = false;
        if (result == RESULT_OK)
        {
            ResourceDescriptor& descriptor = load->m_Descriptor;
            descriptor.m_NameHash = node->m_PathHash;
            descriptor.m_ResourceType = node->m_Type;
            descriptor.m_ReferenceCount = 1;
            descriptor.m_ResourceSizeOnDisc = load->m_FileSize;
            ResourceCreateParams params = {};
            params.m_Factory = context->m_Factory;
            params.m_Type = node->m_Type;
            params.m_Context = node->m_Type->m_Context;
            params.m_Filename = node->m_Path;
            params.m_Buffer = load->m_Provided ? load->m_ProvidedData : load->m_Buffer.Begin();
            params.m_BufferSize = load->m_BufferSize;
            params.m_FileSize = load->m_FileSize;
            params.m_IsBufferPartial = load->m_BufferSize != load->m_FileSize;
            params.m_PreloadData = load->m_PreloadData;
            params.m_Resource = &descriptor;
            LoadNode* previous = context->m_Creating;
            context->m_Creating = node;
            {
                DM_PROFILE("ResourceCreate");
                DM_PROFILE_DYN(node->m_Path, 0);
                uint64_t start = LoadProfileTime(&load->m_Profile);
                result = (Result)node->m_Type->m_CreateFunction(&params);
                LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_CREATE, start);
            }
            context->m_Creating = previous;
            if (result == RESULT_OK)
            {
                post = node->m_Type->m_PostCreateFunction != 0;
                result = node->m_DependencyResult;
                if (result == RESULT_OK)
                {
                    DM_MUTEX_SCOPED_LOCK(GetLoadMutex(context->m_Factory));
                    result = InsertResource(context->m_Factory, node->m_Path, node->m_PathHash, &descriptor);
                    if (result == RESULT_OK)
                    {
                        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                        node->m_Resource = descriptor.m_Resource;
                        node->m_Pin = true;
                    }
                }
                load->m_Destroy = result != RESULT_OK;
            }
        }
        FreeBuffer(load);
        {
            DM_MUTEX_SCOPED_LOCK(GetLoadMutex(context->m_Factory));
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            if (node->m_Resource)
                for (uint32_t i = 0; i < node->m_RootReferences; ++i)
                    IncRef(context->m_Factory, node->m_Resource);
            load->m_InCreate = false;
            node->m_CreateResult = result;
            node->m_PostDone = !post;
            Created(context, node);
            if (post)
                Push(context->m_Posts, node);
            else
                ReleaseStaging(context, node);
        }
    }

    static void PostCreateResourceNode(LoadContext* context, LoadNode* node)
    {
        LoadData* load = node->m_Load;
        ResourcePostCreateParams params = {};
        params.m_Factory = context->m_Factory;
        params.m_Type = node->m_Type;
        params.m_Context = node->m_Type->m_Context;
        params.m_Filename = node->m_Path;
        params.m_PreloadData = load->m_PreloadData;
        params.m_Resource = &load->m_Descriptor;
        Result result;
        LoadNode* previous = context->m_Creating;
        context->m_Creating = node;
        {
            DM_PROFILE("ResourcePostCreate");
            DM_PROFILE_DYN(node->m_Path, 0);
            uint64_t start = LoadProfileTime(&load->m_Profile);
            result = (Result)node->m_Type->m_PostCreateFunction(&params);
            LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_POST_CREATE, start);
        }
        context->m_Creating = previous;
        if (result == RESULT_PENDING)
            return;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        if (node->m_CreateResult == RESULT_OK)
            node->m_CreateResult = result;
        node->m_PostDone = true;
        QueuePin(context, node);
        QueueComplete(context, node);
        ReleaseStaging(context, node);
    }

    static bool ProcessCompletion(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        LoadNode* node;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            node = FindRequestWork(request, context->m_Completions, LOAD_WORK_COMPLETE, true);
            if (!node)
                return false;
            node->m_Result = node->m_CreateResult;
            if (node->m_Result == RESULT_OK)
                node->m_Result = node->m_DependencyResult;
        }
        LoadData* load = node->m_Load;
        {
            DM_MUTEX_SCOPED_LOCK(GetLoadMutex(context->m_Factory));
            ResourceDescriptor* descriptor = FindByHash(context->m_Factory, node->m_PathHash);
            LogLoadProfile(&load->m_Profile, node->m_Path, node->m_Result, descriptor && descriptor->m_Resource == node->m_Resource ? &descriptor->m_LoadTimings : 0);
            if (descriptor && descriptor->m_Resource == node->m_Resource && load->m_Descriptor.m_ResourceSize)
                descriptor->m_ResourceSize = load->m_Descriptor.m_ResourceSize;
        }
        if (load->m_Destroy)
        {
            ResourceDestroyParams params = {};
            params.m_Factory = context->m_Factory;
            params.m_Type = node->m_Type;
            params.m_Context = node->m_Type->m_Context;
            params.m_Resource = &load->m_Descriptor;
            node->m_Type->m_DestroyFunction(&params);
        }
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        node->m_Load = 0;
        delete load;
        node->m_Complete = true;
        for (uint32_t i = node->m_FirstDependent; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependent)
        {
            LoadNode* parent = context->m_Nodes[context->m_Edges[i].m_Parent];
            assert(parent->m_PendingCompletions);
            --parent->m_PendingCompletions;
            if (node->m_Result != RESULT_OK)
                parent->m_DependencyResult = node->m_Result;
            QueueComplete(context, parent);
        }
        return true;
    }

    static void ReleasePins(LoadContext* context)
    {
        for (;;)
        {
            LoadNode* node;
            void* resource;
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                node = Pop(context->m_Pins);
                if (!node)
                    return;
                node->m_PinQueued = false;
                if ((node->m_Holds && !node->m_RootReferences) || !node->m_PostDone || !node->m_Pin)
                    continue;
                node->m_Pin = false;
                resource = node->m_Resource;
            }
            Release(context->m_Factory, resource);
        }
    }

    static void DestroyUnusedNodes(LoadContext* context, LoadNode* root);

    // Called under the resource-table lock when its last reference is released.
    // A newly discovered parent can still need this resource before its Create runs.
    bool RetainResourceLoadReference(LoadContext* context, dmhash_t path, void* resource)
    {
        if (!context)
            return false;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        LoadNode** found = context->m_ByPath.Get(path);
        if (!found || (*found)->m_Resource != resource)
            return false;
        LoadNode* node = *found;
        if (node->m_Holds || node->m_RootReferences || !node->m_PostDone)
        {
            node->m_Pin = true;
            QueuePin(context, node);
            return true;
        }
        node->m_Resource = 0;
        context->m_ByPath.Erase(path);
        DestroyUnusedNodes(context, node);
        return false;
    }

    static bool RootsCreated(HPreloader request)
    {
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
            if (!request->m_Roots[i]->m_Created)
                return false;
        return true;
    }

    static void CallComplete(HPreloader request, FPreloaderCompleteCallback callback, PreloaderCompleteCallbackParams* params)
    {
        if (request->m_CallbackCalled || !RootsCreated(request))
            return;
        request->m_CallbackCalled = true;
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
            if (request->m_Roots[i]->m_CreateResult != RESULT_OK)
                return;
        if (callback && !callback(params))
            request->m_CallbackResult = RESULT_NOT_LOADED;
    }

    // Each update advances only its request, preserving FIFO order and callback
    // affinity for shared dependencies without spending other requests' budgets.
    static LoadNode* FindRequestWork(HPreloader request, LoadQueue& queue, LoadWork work, bool remove)
    {
        LoadContext* context = request->m_Context;
        SelectRequest(request);
        uint32_t caller = GetCaller(context);
        for (uint32_t i = queue.m_Front; i < queue.m_Items.Size(); ++i)
        {
            LoadNode* node = queue.m_Items[i];
            if (node->m_Load->m_Selection != context->m_Selection || (work != LOAD_WORK_READ && node->m_Load->m_Caller != caller))
                continue;
            if (remove)
            {
                if (i == queue.m_Front)
                    Pop(queue);
                else
                {
                    memmove(queue.m_Items.Begin() + i, queue.m_Items.Begin() + i + 1, (queue.m_Items.Size() - i - 1) * sizeof(LoadNode*));
                    queue.m_Items.Pop();
                }
            }
            return node;
        }
        return 0;
    }

    // Snapshot this request's posts in FIFO order. Keep them in the shared queue
    // until called, since a nested Get can need to finish the same dependency.
    static void GetRequestPosts(HPreloader request, dmArray<LoadNode*>& posts)
    {
        LoadContext* context = request->m_Context;
        SelectRequest(request);
        uint32_t caller = GetCaller(context);
        for (uint32_t i = context->m_Posts.m_Front; i < context->m_Posts.m_Items.Size(); ++i)
        {
            LoadNode* node = context->m_Posts.m_Items[i];
            if (node->m_Load->m_Selection == context->m_Selection && node->m_Load->m_Caller == caller)
                Push(posts, node);
        }
    }

    // Completed requests release their holds without advancing unrelated work.
    static Result CompleteRequest(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        Result result = request->m_CallbackResult;
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
        {
            LoadNode* root = request->m_Roots[i];
            if (!root->m_Complete)
                return RESULT_PENDING;
            if (result == RESULT_OK)
                result = root->m_Result;
        }
        if (request->m_Holding)
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                ChangeHold(context, request->m_Roots[i], false);
            request->m_Holding = false;
        }
        ReleasePins(context);
        return result;
    }

    static Result UpdateResourceLoads(HPreloader request, FPreloaderCompleteCallback callback, PreloaderCompleteCallbackParams* params, uint32_t soft_time_limit)
    {
        DM_PROFILE("UpdateResourceLoads");
        LoadContext* context = request->m_Context;
        uint64_t start = dmTime::GetMonotonicTime();
        CallComplete(request, callback, params);
        Result result = CompleteRequest(request);
        if (result != RESULT_PENDING)
            return result;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            StartReads(context, request);
        }
        // Blocking PostCreate can depend on a general job callback. Normal
        // asynchronous loading leaves the shared pool's updates to its owner.
        bool update_jobs = false;
        if (request->m_Blocking)
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            update_jobs = FindRequestWork(request, context->m_Posts, LOAD_WORK_POST, false) != 0;
        }
        if (update_jobs)
        {
            LoadNode* creating = context->m_Creating;
            context->m_Creating = 0;
            JobSystemUpdate(context->m_Jobs, 1000);
            context->m_Creating = creating;
        }
        bool progressed = false;
        do
        {
            // Publish completed work before starting another creator. Otherwise
            // a small request can wait behind an unrelated large ready queue.
            if (ProcessCompletion(request))
                progressed = true;
            else
            {
                LoadNode* node;
                {
                    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                    node = FindRequestWork(request, context->m_Creates, LOAD_WORK_CREATE, true);
                    if (node)
                        node->m_Load->m_InCreate = true;
                }
                if (node)
                    CreateResourceNode(context, node);
                else if (context->m_ReadLimit || !ReadInline(request))
                    break;
                progressed = true;
            }
            CallComplete(request, callback, params);
            result = CompleteRequest(request);
            if (result != RESULT_PENDING)
                return result;
        } while (dmTime::GetMonotonicTime() - start < soft_time_limit);

        // Retry each pending PostCreate at most once per update. A pending GPU
        // upload must not turn a caller's frame budget into a polling interval.
        dmArray<LoadNode*> posts;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            if (context->m_Posts.m_Front != context->m_Posts.m_Items.Size())
                GetRequestPosts(request, posts);
        }
        for (uint32_t i = 0; i < posts.Size(); ++i)
        {
            if (progressed && dmTime::GetMonotonicTime() - start >= soft_time_limit)
                break;
            LoadNode* node = posts[i];
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                if (node->m_PostDone)
                    continue;
                LoadQueue& queue = context->m_Posts;
                for (uint32_t j = queue.m_Front; j < queue.m_Items.Size(); ++j)
                    if (queue.m_Items[j] == node)
                    {
                        if (j == queue.m_Front)
                            Pop(queue);
                        else
                        {
                            memmove(queue.m_Items.Begin() + j, queue.m_Items.Begin() + j + 1, (queue.m_Items.Size() - j - 1) * sizeof(LoadNode*));
                            queue.m_Items.Pop();
                        }
                        break;
                    }
            }
            PostCreateResourceNode(context, node);
            progressed = true;
            if (!node->m_PostDone)
                Push(context->m_Posts, node);
        }
        while ((!progressed || dmTime::GetMonotonicTime() - start < soft_time_limit) && ProcessCompletion(request))
        {
            progressed = true;
        }
        ReleasePins(context);
        return CompleteRequest(request);
    }

    Result UpdatePreloader(HPreloader request, FPreloaderCompleteCallback callback, PreloaderCompleteCallbackParams* params, uint32_t soft_time_limit)
    {
        LoadContext* context = request->m_Context;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            uint32_t thread = GetCaller(context);
            if (context->m_UpdateDepth && context->m_UpdateOwner != thread)
                return RESULT_PENDING;
            context->m_UpdateOwner = thread;
            ++context->m_UpdateDepth;
        }
        // One caller executes creation callbacks at a time. Ownership is recursive
        // for synchronous Get inside Create, without holding a mutex over callbacks.
        Result result = UpdateResourceLoads(request, callback, params, soft_time_limit);
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            --context->m_UpdateDepth;
            Signal(context);
        }
        return result;
    }

    static HPreloader NewRequest(HFactory factory)
    {
        HPreloader request = new ResourcePreloader();
        request->m_Context = GetLoadContext(factory);
        request->m_CallbackResult = RESULT_OK;
        request->m_Holding = true;
        DM_MUTEX_SCOPED_LOCK(request->m_Context->m_Mutex);
        ++request->m_Context->m_Requests;
        return request;
    }

    static Result AddRoot(HPreloader request, const char* path, HResourceType type, const void* data, uint32_t size, uint32_t file_size, bool provided, bool dependency)
    {
        LoadContext* context = request->m_Context;
        LoadNode* node;
        void* resident = 0;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            uint32_t caller = GetCaller(context);
            node = FindOrCreate(context, path, type, caller);
            bool new_load = !node->m_References && !node->m_Discovered;
            if (dependency && context->m_UpdateDepth && context->m_UpdateOwner == caller && context->m_Creating)
            {
                if (!RecordDependency(context, node))
                    return RESULT_RESOURCE_LOOP_ERROR;
            }
            // An unrelated main-thread job callback can re-enter Get while
            // its target's creator is suspended in a nested resource load.
            if (dependency && context->m_UpdateDepth && context->m_UpdateOwner == caller && node->m_Load && node->m_Load->m_InCreate)
                return RESULT_RESOURCE_LOOP_ERROR;
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                if (request->m_Roots[i] == node)
                    return RESULT_OK;
            if (provided && new_load)
            {
                node->m_Load->m_Provided = true;
                node->m_Load->m_ProvidedData = data;
                node->m_Load->m_BufferSize = size;
                node->m_Load->m_FileSize = file_size;
            }
            ++node->m_References;
            ++node->m_RootReferences;
            ChangeHold(context, node, true);
            QueuePin(context, node);
            Push(request->m_Roots, node);
            if (context->m_SelectedRequest == request)
                SelectDependencies(context, node);
            if (node->m_Created)
                resident = node->m_Resource;
            StartReads(context, request->m_Blocking ? request : 0);
        }
        if (resident)
            IncRef(context->m_Factory, resident);
        return RESULT_OK;
    }

    HPreloader NewPreloader(HFactory factory, const dmArray<const char*>& names)
    {
        HPreloader request = NewRequest(factory);
        for (uint32_t i = 0; i < names.Size(); ++i)
            AddRoot(request, names[i], 0, 0, 0, 0, false, false);
        return request;
    }

    HPreloader NewPreloader(HFactory factory, const char* name)
    {
        HPreloader request = NewRequest(factory);
        AddRoot(request, name, 0, 0, 0, 0, false, false);
        return request;
    }

    static void WaitForWork(HPreloader request)
    {
        LoadContext* context = request->m_Context;
#if defined(DM_HAS_THREADS)
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t caller = GetCaller(context);
        uint32_t depth = context->m_UpdateOwner == caller ? context->m_UpdateDepth : 0;
        LoadNode* creating = depth ? context->m_Creating : 0;
        RecreateContext* recreating = depth ? context->m_Recreating : 0;
        // A nested Get may need a resource assigned to another requesting
        // thread. Let that thread run while this creator is suspended.
        if (depth)
        {
            context->m_UpdateDepth = 0;
            context->m_Creating = 0;
            context->m_Recreating = 0;
            Signal(context);
        }
        bool ready = FindRequestWork(request, context->m_Creates, LOAD_WORK_CREATE, false) || FindRequestWork(request, context->m_Posts, LOAD_WORK_POST, false) || FindRequestWork(request, context->m_Completions, LOAD_WORK_COMPLETE, false);
        if (!context->m_ReadLimit)
            ready = ready || FindRequestWork(request, context->m_Reads, LOAD_WORK_READ, false);
        bool foreign = false;
        bool complete = true;
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
        {
            LoadNode* root = request->m_Roots[i];
            if (!root->m_Complete)
            {
                complete = false;
                foreign = foreign || root->m_Load->m_Caller != caller;
            }
        }
        if ((!complete && !ready && (context->m_ActiveReads || foreign)) || (context->m_UpdateDepth && context->m_UpdateOwner != caller))
            dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
        if (depth)
        {
            while (context->m_UpdateDepth && context->m_UpdateOwner != caller)
                dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
            context->m_UpdateOwner = caller;
            context->m_UpdateDepth = depth;
            context->m_Creating = creating;
            context->m_Recreating = recreating;
        }
#else
        (void)context;
#endif
    }

    static void DestroyUnusedNodes(LoadContext* context, LoadNode* root)
    {
        dmArray<LoadNode*> release;
        Push(release, root);
        while (!release.Empty())
        {
            LoadNode* node = release.Back();
            release.Pop();
            if (node->m_References || node->m_Resource)
                continue;
            assert(node->m_Complete && !node->m_Pin && !node->m_Load);
            LoadNode** found = context->m_ByPath.Get(node->m_PathHash);
            if (found && *found == node)
                context->m_ByPath.Erase(node->m_PathHash);
            for (uint32_t i = node->m_FirstDependency; i != INVALID_LOAD_INDEX;)
            {
                LoadEdge edge = context->m_Edges[i];
                LoadNode* child = context->m_Nodes[edge.m_Child];
                if (edge.m_PreviousDependent == INVALID_LOAD_INDEX)
                    child->m_FirstDependent = edge.m_NextDependent;
                else
                    context->m_Edges[edge.m_PreviousDependent].m_NextDependent = edge.m_NextDependent;
                if (edge.m_NextDependent != INVALID_LOAD_INDEX)
                    context->m_Edges[edge.m_NextDependent].m_PreviousDependent = edge.m_PreviousDependent;
                assert(child->m_References);
                --child->m_References;
                if (!child->m_References && !child->m_Resource)
                    Push(release, child);
                Push(context->m_FreeEdges, i);
                i = edge.m_NextDependency;
            }
            context->m_Nodes[node->m_Index] = 0;
            Push(context->m_FreeNodes, node->m_Index);
            free(node->m_Path);
            delete node;
        }
    }

    static void ReleaseNode(LoadContext* context, LoadNode* node)
    {
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        assert(node->m_References);
        --node->m_References;
        DestroyUnusedNodes(context, node);
    }

    RecreateContext* BeginResourceRecreate(LoadContext* context, dmhash_t path)
    {
        RecreateContext* recreate = new RecreateContext();
        recreate->m_Context = context;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t caller = GetCaller(context);
#if defined(DM_HAS_THREADS)
        while (context->m_UpdateDepth && context->m_UpdateOwner != caller)
            dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
#endif
        context->m_UpdateOwner = caller;
        ++context->m_UpdateDepth;
        recreate->m_PreviousCreating = context->m_Creating;
        recreate->m_Previous = context->m_Recreating;
        LoadNode** found = context->m_ByPath.Get(path);
        if (found)
        {
            recreate->m_Node = *found;
            recreate->m_FirstDependency = (*found)->m_FirstDependency;
            recreate->m_DependencyResult = (*found)->m_DependencyResult;
            ++(*found)->m_References;
            ChangeHold(context, *found, true);
        }
        context->m_Creating = recreate->m_Node;
        context->m_Recreating = recreate;
        return recreate;
    }

    void CommitResourceRecreate(RecreateContext* recreate, Result result, void* resource)
    {
        LoadContext* context = recreate->m_Context;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            LoadNode* node = recreate->m_Node;
            if (node)
            {
                uint32_t previous = INVALID_LOAD_INDEX;
                bool original = false;
                for (uint32_t i = node->m_FirstDependency; i != INVALID_LOAD_INDEX;)
                {
                    LoadEdge edge = context->m_Edges[i];
                    LoadNode* child = context->m_Nodes[edge.m_Child];
                    original = original || i == recreate->m_FirstDependency;
                    bool keep = original;
                    if (result == RESULT_OK)
                    {
                        keep = false;
                        for (uint32_t j = 0; j < recreate->m_Dependencies.Size(); ++j)
                            keep = keep || recreate->m_Dependencies[j] == child;
                    }
                    if (keep)
                        previous = i;
                    else
                    {
                        if (previous == INVALID_LOAD_INDEX)
                            node->m_FirstDependency = edge.m_NextDependency;
                        else
                            context->m_Edges[previous].m_NextDependency = edge.m_NextDependency;
                        if (edge.m_PreviousDependent == INVALID_LOAD_INDEX)
                            child->m_FirstDependent = edge.m_NextDependent;
                        else
                            context->m_Edges[edge.m_PreviousDependent].m_NextDependent = edge.m_NextDependent;
                        if (edge.m_NextDependent != INVALID_LOAD_INDEX)
                            context->m_Edges[edge.m_NextDependent].m_PreviousDependent = edge.m_PreviousDependent;
                        if (!child->m_Created)
                            --node->m_PendingDependencies;
                        if (!child->m_Complete)
                            --node->m_PendingCompletions;
                        ChangeHold(context, child, false);
                        --child->m_References;
                        DestroyUnusedNodes(context, child);
                        Push(context->m_FreeEdges, i);
                    }
                    i = edge.m_NextDependency;
                }
                if (result == RESULT_OK)
                    node->m_Resource = resource;
                else
                    node->m_DependencyResult = recreate->m_DependencyResult;
                ChangeHold(context, node, false);
                --node->m_References;
                DestroyUnusedNodes(context, node);
            }
            context->m_Creating = 0;
            context->m_Recreating = 0;
            context->m_SelectedRequest = 0;
        }
        ReleasePins(context);
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            context->m_Creating = recreate->m_PreviousCreating;
            context->m_Recreating = recreate->m_Previous;
        }
    }

    void EndResourceRecreate(RecreateContext* recreate)
    {
        LoadContext* context = recreate->m_Context;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        --context->m_UpdateDepth;
        Signal(context);
        delete recreate;
    }

    void DeletePreloader(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        request->m_Blocking = true;
        while (UpdatePreloader(request, 0, 0, 1000000) == RESULT_PENDING)
        {
            WaitForWork(request);
        }
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
        {
            LoadNode* root = request->m_Roots[i];
            void* resource;
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                --root->m_RootReferences;
                resource = root->m_Resource;
            }
            if (resource)
                Release(context->m_Factory, resource);
            ReleasePins(context);
            ReleaseNode(context, root);
        }
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            --context->m_Requests;
            if (context->m_SelectedRequest == request)
                context->m_SelectedRequest = 0;
        }
        delete request;
    }

    Result LoadResourceSync(HFactory factory, const char* path, HResourceType type, const void* data, uint32_t size, uint32_t file_size, bool provided, void** resource)
    {
        HPreloader request = NewRequest(factory);
        request->m_Blocking = true;
        Result result = AddRoot(request, path, type, data, size, file_size, provided, true);
        if (result != RESULT_OK)
        {
            DeletePreloader(request);
            return result;
        }
        while ((result = UpdatePreloader(request, 0, 0, 1000000)) == RESULT_PENDING)
        {
            WaitForWork(request);
        }
        if (result == RESULT_OK)
        {
            *resource = request->m_Roots[0]->m_Resource;
            IncRef(factory, *resource);
        }
        DeletePreloader(request);
        return result;
    }

    void DeleteLoadContext(LoadContext* context)
    {
        if (!context)
            return;
        assert(!context->m_Requests && !context->m_ActiveReads && !context->m_StagedBytes);
        // Remaining nodes describe externally retained resources. Their resource
        // ownership is unchanged; DeleteFactory reports any leaked references.
        for (uint32_t i = 0; i < context->m_Nodes.Size(); ++i)
            if (context->m_Nodes[i])
                context->m_Nodes[i]->m_Resource = 0;
        for (uint32_t i = 0; i < context->m_Nodes.Size(); ++i)
            if (context->m_Nodes[i])
                DestroyUnusedNodes(context, context->m_Nodes[i]);
        assert(context->m_ByPath.Empty());
#if defined(DM_HAS_THREADS)
        dmConditionVariable::Delete(context->m_Changed);
        dmThread::FreeTls(context->m_CallerKey);
#endif
        dmMutex::Delete(context->m_Mutex);
        delete context;
    }
} // namespace dmResource
