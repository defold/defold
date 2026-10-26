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
#include <dlib/object_pool.h>
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#if defined(DM_HAS_THREADS)
#include <dlib/condition_variable.h>
#endif

#include "resource.h"
#include "resource_private.h"
#include "resource_util.h"

namespace dmResource
{
    enum LoadWork
    {
        LOAD_WORK_READ,
        LOAD_WORK_CREATE,
        LOAD_WORK_POST,
        LOAD_WORK_COMPLETE,
        LOAD_WORK_COUNT
    };
} // namespace dmResource

struct ResourcePreloader
{
    dmResource::LoadContext* m_Context;
    dmArray<uint32_t> m_Roots;
    // Started loads stay owned until their cleanup, even if recreation detaches them.
    dmArray<uint32_t> m_Loads;
    uint64_t m_WorkCursor[dmResource::LOAD_WORK_COUNT];
    uint32_t m_CursorCaller;
    dmResource::Result m_Result;
    uint64_t m_NextPostTime;
    bool m_CallbackCalled;
    bool m_Complete;
    bool m_Blocking;
    bool m_Nested;
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
        HJob m_ReadJob;
        HPreloader m_Request;
        uint32_t m_RequestIndex;
        uint32_t m_Caller;
        uint32_t m_Selection;
        uint32_t m_QueueIndex[LOAD_WORK_COUNT];
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
        // Access under the graph mutex: adjacent flags share storage.
        uint8_t m_Discovered : 1;
        uint8_t m_CreateQueued : 1;
        uint8_t m_Created : 1;
        uint8_t m_PostDone : 1;
        uint8_t m_CompleteQueued : 1;
        // Cleanup is finished; graph readiness remains in m_Result.
        uint8_t m_Complete : 1;
        uint8_t m_Pin : 1;
        uint8_t m_PinQueued : 1;
        // Remains set while a callback yields scheduler ownership in a nested Get.
        uint8_t m_InCallback : 1;
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

    struct LoadQueueItem
    {
        uint64_t m_Sequence;
        uint32_t m_Node;
    };

    struct LoadQueue
    {
        dmArray<LoadQueueItem> m_Items;
        uint64_t m_NextSequence;
        uint32_t m_Front;
        uint32_t m_Count;
        LoadWork m_Work;
    };

    struct RecreateContext
    {
        LoadContext* m_Context;
        uint32_t m_Node;
        uint32_t m_PreviousCreating;
        RecreateContext* m_Previous;
        dmArray<uint32_t> m_Dependencies;
        uint32_t m_FirstDependency;
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
        dmHashTable64<uint32_t> m_ByPath;
        // Retain indices across callbacks and unlocked work: growth and Free move nodes.
        dmObjectPool<LoadNode> m_Nodes;
        dmArray<LoadEdge> m_Edges;
        dmArray<uint32_t> m_FreeEdges;
        dmArray<uint32_t> m_Visit;
        LoadQueue m_Queues[LOAD_WORK_COUNT];
        dmArray<uint32_t> m_ReadJobs;
        LoadQueue m_Pins;
        uint32_t m_Creating;
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

    // Sequence cursors survive storage compaction and never revisit a skipped
    // prefix. New dependencies rewind only to work that has become eligible.
    static void RewindWork(LoadContext* context, LoadNode* node, LoadWork work)
    {
        HPreloader request = context->m_SelectedRequest;
        uint32_t position = node->m_Load->m_QueueIndex[work];
        if (!request || position == INVALID_LOAD_INDEX || node->m_Load->m_Selection != context->m_Selection)
            return;
        if (work != LOAD_WORK_READ && node->m_Load->m_Caller != request->m_CursorCaller)
            return;
        uint64_t sequence = context->m_Queues[work].m_Items[position].m_Sequence;
        if (sequence < request->m_WorkCursor[work])
            request->m_WorkCursor[work] = sequence;
    }

    static void Push(LoadContext* context, LoadQueue& queue, uint32_t index)
    {
        LoadQueueItem item = { ++queue.m_NextSequence, index };
        uint32_t position = queue.m_Items.Size();
        Push(queue.m_Items, item);
        ++queue.m_Count;
        if (queue.m_Work != LOAD_WORK_COUNT)
        {
            LoadNode* node = context->m_Nodes.GetPtr(index);
            assert(node->m_Load->m_QueueIndex[queue.m_Work] == INVALID_LOAD_INDEX);
            node->m_Load->m_QueueIndex[queue.m_Work] = position;
            RewindWork(context, node, queue.m_Work);
        }
    }

    static void Remove(LoadContext* context, LoadQueue& queue, uint32_t position)
    {
        uint32_t index = queue.m_Items[position].m_Node;
        assert(index != INVALID_LOAD_INDEX);
        if (queue.m_Work != LOAD_WORK_COUNT)
            context->m_Nodes.Get(index).m_Load->m_QueueIndex[queue.m_Work] = INVALID_LOAD_INDEX;
        queue.m_Items[position].m_Node = INVALID_LOAD_INDEX;
        if (!--queue.m_Count)
        {
            queue.m_Items.SetSize(0);
            queue.m_Front = 0;
            return;
        }
        while (queue.m_Items[queue.m_Front].m_Node == INVALID_LOAD_INDEX)
            ++queue.m_Front;
        while (queue.m_Items.Back().m_Node == INVALID_LOAD_INDEX)
            queue.m_Items.Pop();
        // Removal never shifts the remaining entries. Compact only when at least
        // half the storage is unused, bounding memory and amortizing that work.
        if (queue.m_Items.Size() > 64 && queue.m_Count <= queue.m_Items.Size() / 2)
        {
            uint32_t count = 0;
            for (uint32_t i = queue.m_Front; i < queue.m_Items.Size(); ++i)
            {
                LoadQueueItem item = queue.m_Items[i];
                if (item.m_Node == INVALID_LOAD_INDEX)
                    continue;
                if (count != i)
                {
                    queue.m_Items[count] = item;
                    if (queue.m_Work != LOAD_WORK_COUNT)
                        context->m_Nodes.Get(item.m_Node).m_Load->m_QueueIndex[queue.m_Work] = count;
                }
                ++count;
            }
            queue.m_Items.SetSize(count);
            queue.m_Front = 0;
        }
    }

    static uint32_t Pop(LoadContext* context, LoadQueue& queue)
    {
        if (!queue.m_Count)
            return INVALID_LOAD_INDEX;
        uint32_t index = queue.m_Items[queue.m_Front].m_Node;
        Remove(context, queue, queue.m_Front);
        return index;
    }

    static uint32_t PopLast(LoadContext* context, LoadQueue& queue)
    {
        if (!queue.m_Count)
            return INVALID_LOAD_INDEX;
        uint32_t position = queue.m_Items.Size() - 1;
        uint32_t index = queue.m_Items[position].m_Node;
        Remove(context, queue, position);
        return index;
    }

    static uint32_t FindCursor(const LoadQueue& queue, uint64_t sequence)
    {
        uint32_t first = queue.m_Front;
        uint32_t end = queue.m_Items.Size();
        if (!queue.m_Count || sequence > queue.m_Items.Back().m_Sequence)
            return end;
        if (sequence <= queue.m_Items[first].m_Sequence)
            return first;
        while (first < end)
        {
            uint32_t middle = first + (end - first) / 2;
            if (queue.m_Items[middle].m_Sequence < sequence)
                first = middle + 1;
            else
                end = middle;
        }
        return first;
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
        Push(context, context->m_Queues[LOAD_WORK_CREATE], node->m_Index);
    }

    static void QueueComplete(LoadContext* context, LoadNode* node)
    {
        // Cleanup belongs to this load; readiness also includes its dependencies.
        if (!node->m_PostDone || node->m_Complete || node->m_CompleteQueued)
            return;
        node->m_CompleteQueued = true;
        Push(context, context->m_Queues[LOAD_WORK_COMPLETE], node->m_Index);
    }

    static void QueuePin(LoadContext* context, LoadNode* node)
    {
        if (!node->m_Pin || node->m_PinQueued || (node->m_Holds && !node->m_RootReferences) || !node->m_PostDone)
            return;
        node->m_PinQueued = true;
        Push(context, context->m_Pins, node->m_Index);
    }

    static uint32_t FindRequestWork(HPreloader request, LoadWork work, bool remove);
    static void StartReads(LoadContext* context, HPreloader request);
    static void DestroyUnusedNodes(LoadContext* context, uint32_t root);

    // Requests retain their reachable resources, including hints consumed by a
    // later ancestor's Create. Propagate only when a node enters or leaves that set.
    static void ChangeHold(LoadContext* context, LoadNode* node, bool hold)
    {
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, node->m_Index);
        while (!context->m_Visit.Empty())
        {
            LoadNode* next = context->m_Nodes.GetPtr(context->m_Visit.Back());
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
                Push(context->m_Visit, context->m_Edges[edge].m_Child);
        }
    }

    static uint32_t VisitGeneration(LoadContext* context)
    {
        ++context->m_VisitGeneration;
        if (!context->m_VisitGeneration)
        {
            for (uint32_t i = 0; i < context->m_Nodes.Size(); ++i)
                context->m_Nodes.GetRawObjects()[i].m_Visited = 0;
            ++context->m_VisitGeneration;
        }
        return context->m_VisitGeneration;
    }

    // Selection lives only in loading data. Extend it as workers discover new
    // edges, so a request need not traverse its entire graph for each operation.
    static void SelectDependencies(LoadContext* context, uint32_t root)
    {
        uint32_t generation = 0;
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, root);
        while (!context->m_Visit.Empty())
        {
            LoadNode* node = context->m_Nodes.GetPtr(context->m_Visit.Back());
            context->m_Visit.Pop();
            if (node->m_Complete)
            {
                if (node->m_Result != RESULT_PENDING)
                    continue;
                if (!generation)
                    generation = VisitGeneration(context);
                if (node->m_Visited == generation)
                    continue;
                node->m_Visited = generation;
            }
            else
            {
                if (node->m_Load->m_Selection == context->m_Selection)
                    continue;
                node->m_Load->m_Selection = context->m_Selection;
                for (uint32_t work = 0; work < LOAD_WORK_COUNT; ++work)
                    RewindWork(context, node, (LoadWork)work);
            }
            for (uint32_t edge = node->m_FirstDependency; edge != INVALID_LOAD_INDEX; edge = context->m_Edges[edge].m_NextDependency)
                Push(context->m_Visit, context->m_Edges[edge].m_Child);
        }
    }

    static void SelectRequest(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        uint32_t caller = GetCaller(context);
        if (context->m_SelectedRequest == request && request->m_CursorCaller == caller)
            return;
        request->m_CursorCaller = caller;
        for (uint32_t work = 0; work < LOAD_WORK_COUNT; ++work)
            request->m_WorkCursor[work] = ~(uint64_t)0;
        if (!++context->m_Selection)
        {
            for (uint32_t i = 0; i < context->m_Nodes.Size(); ++i)
                if (context->m_Nodes.GetRawObjects()[i].m_Load)
                    context->m_Nodes.GetRawObjects()[i].m_Load->m_Selection = 0;
            ++context->m_Selection;
        }
        context->m_SelectedRequest = request;
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
            SelectDependencies(context, request->m_Roots[i]);
        for (uint32_t i = 0; i < request->m_Loads.Size(); ++i)
            SelectDependencies(context, request->m_Loads[i]);
    }

    static bool DependsOn(LoadContext* context, LoadNode* node, LoadNode* dependency)
    {
        if (node->m_FirstDependency == INVALID_LOAD_INDEX && node != dependency)
            return false;
        uint32_t generation = VisitGeneration(context);
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, node->m_Index);
        while (!context->m_Visit.Empty())
        {
            LoadNode* next = context->m_Nodes.GetPtr(context->m_Visit.Back());
            context->m_Visit.Pop();
            if (next == dependency)
                return true;
            if (next->m_Visited == generation)
                continue;
            next->m_Visited = generation;
            for (uint32_t edge = next->m_FirstDependency; edge != INVALID_LOAD_INDEX; edge = context->m_Edges[edge].m_NextDependency)
                Push(context->m_Visit, context->m_Edges[edge].m_Child);
        }
        return false;
    }

    // Cleanup callbacks must keep the failed attempt they already depend on,
    // even when another requester has replaced the path lookup with a retry.
    static uint32_t FindFailedDependency(LoadContext* context, uint32_t root, dmhash_t path)
    {
        uint32_t generation = VisitGeneration(context);
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, root);
        while (!context->m_Visit.Empty())
        {
            uint32_t index = context->m_Visit.Back();
            LoadNode* node = context->m_Nodes.GetPtr(index);
            context->m_Visit.Pop();
            if (node->m_PathHash == path && node->m_Complete && !node->m_Resource)
                return index;
            if (node->m_Visited == generation)
                continue;
            node->m_Visited = generation;
            for (uint32_t edge = node->m_FirstDependency; edge != INVALID_LOAD_INDEX; edge = context->m_Edges[edge].m_NextDependency)
                Push(context->m_Visit, context->m_Edges[edge].m_Child);
        }
        return INVALID_LOAD_INDEX;
    }

    static uint32_t FindOrCreate(LoadContext* context, const char* path, HResourceType type, uint32_t caller, HPreloader request)
    {
        char canonical[RESOURCE_PATH_MAX];
        GetCanonicalPath(path, canonical, sizeof(canonical));
        dmhash_t hash = dmHashString64(canonical);
        if (context->m_UpdateDepth && context->m_UpdateOwner == GetCaller(context) && context->m_Creating != INVALID_LOAD_INDEX &&
            context->m_Nodes.Get(context->m_Creating).m_DependencyResult != RESULT_OK &&
            (!context->m_Recreating || context->m_Recreating->m_Node != context->m_Creating))
        {
            uint32_t failed = FindFailedDependency(context, context->m_Creating, hash);
            if (failed != INVALID_LOAD_INDEX)
                return failed;
        }
        uint32_t* found = context->m_ByPath.Get(hash);
        if (found)
        {
            LoadNode* node = context->m_Nodes.GetPtr(*found);
            if (!node->m_Complete || node->m_Resource)
                return *found;
            // Keep sharing the failure within this request's graph. Fresh
            // requests can retry against newly available content.
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                if (DependsOn(context, context->m_Nodes.GetPtr(request->m_Roots[i]), node))
                    return *found;
            // The new attempt replaces only the path lookup. Existing owners
            // retain the failed node and its result until their normal cleanup.
        }

        if (context->m_Nodes.Full())
            context->m_Nodes.SetCapacity(context->m_Nodes.Capacity() ? context->m_Nodes.Capacity() * 2 : 16);
        uint32_t index = context->m_Nodes.Alloc();
        LoadNode* node = context->m_Nodes.GetPtr(index);
        memset(node, 0, sizeof(*node));
        node->m_Index = index;
        node->m_Path = strdup(canonical);
        node->m_Load = new LoadData();
        for (uint32_t work = 0; work < LOAD_WORK_COUNT; ++work)
            node->m_Load->m_QueueIndex[work] = INVALID_LOAD_INDEX;
        node->m_Load->m_Caller = caller;
        node->m_Load->m_Request = request;
        node->m_Load->m_RequestIndex = request->m_Loads.Size();
        Push(request->m_Loads, index);
        // Own the load and its dependency resources through terminal cleanup.
        node->m_References = 1;
        node->m_Holds = 1;
        LoadProfileBegin(&node->m_Load->m_Profile, IsLoadProfilingEnabled(context->m_Factory));
        node->m_PathHash = hash;
        node->m_FirstDependency = INVALID_LOAD_INDEX;
        node->m_FirstDependent = INVALID_LOAD_INDEX;
        node->m_CreateResult = RESULT_PENDING;
        node->m_Result = RESULT_PENDING;
        node->m_DependencyResult = RESULT_OK;
        node->m_Type = type;
        if (!type)
        {
            const char* ext = GetExtFromPath(canonical);
            node->m_Type = ext ? FindResourceType(context->m_Factory, ext) : 0;
            if (!node->m_Type)
                node->m_DependencyResult = ext ? RESULT_UNKNOWN_RESOURCE_TYPE : RESULT_MISSING_FILE_EXTENSION;
        }
        if (context->m_ByPath.Full())
            context->m_ByPath.OffsetCapacity(context->m_ByPath.Capacity() ? context->m_ByPath.Capacity() : 16);
        context->m_ByPath.Put(hash, index);
        Push(context, context->m_Queues[LOAD_WORK_READ], index);
        if (context->m_SelectedRequest == request)
            SelectDependencies(context, index);
        return index;
    }

    static void RemoveReadJob(LoadContext* context, uint32_t index)
    {
        for (uint32_t i = 0; i < context->m_ReadJobs.Size(); ++i)
        {
            if (context->m_ReadJobs[i] == index)
            {
                context->m_ReadJobs.EraseSwap(i);
                context->m_Nodes.Get(index).m_Load->m_ReadJob = 0;
                return;
            }
        }
        assert(false);
    }

    static void ReadResource(LoadContext* context, uint32_t index)
    {
        LoadData* load;
        const char* path;
        HResourceType type;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            LoadNode* node = context->m_Nodes.GetPtr(index);
            load = node->m_Load;
            if (load->m_ReadJob)
                RemoveReadJob(context, index);
            path = node->m_Path;
            type = node->m_Type;
        }
        {
            DM_PROFILE("ResourceLoad");
            DM_PROFILE_DYN(path, 0);
            if (!load->m_Provided)
                LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_QUEUE, load->m_Profile.m_StartTime);
            if (!load->m_Provided)
            {
                uint32_t size = ResourceTypeIsStreaming(type) ? ResourceTypeGetPreloadSize(type) : RESOURCE_INVALID_PRELOAD_SIZE;
                uint64_t start = LoadProfileTime(&load->m_Profile);
                load->m_ReadResult = LoadResourceToBuffer(context->m_Factory, path, path, size, &load->m_FileSize, &load->m_BufferSize, &load->m_Buffer);
                LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_READ, start);
            }
            if (load->m_ReadResult == RESULT_OK)
            {
                load->m_PreloadResult = RESULT_OK;
                if (type->m_PreloadFunction)
                {
                    ResourcePreloadHintInfo hint = { context, index };
                    ResourcePreloadParams params = {};
                    params.m_Factory = context->m_Factory;
                    params.m_Type = type;
                    params.m_Context = type->m_Context;
                    params.m_Buffer = load->m_Provided ? load->m_ProvidedData : load->m_Buffer.Begin();
                    params.m_BufferSize = load->m_BufferSize;
                    params.m_FileSize = load->m_FileSize;
                    params.m_IsBufferPartial = load->m_BufferSize != load->m_FileSize;
                    params.m_IsBufferTransferrable = !load->m_Provided;
                    params.m_Filename = path;
                    params.m_HintInfo = &hint;
                    params.m_PreloadData = &load->m_PreloadData;
                    params.m_IsBufferOwnershipTransferred = load->m_Provided ? 0 : &load->m_BufferTransferred;
                    DM_PROFILE("ResourcePreload");
                    uint64_t start = LoadProfileTime(&load->m_Profile);
                    load->m_PreloadResult = (Result)type->m_PreloadFunction(&params);
                    LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_PRELOAD, start);
                }
            }
        }
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        // Types own their decoded allocation sizes. Input bytes provide
        // backpressure until Create/PostCreate consumes the associated payload.
        load->m_StagedBytes = load->m_Provided ? load->m_BufferSize : load->m_Buffer.Capacity();
        context->m_StagedBytes += load->m_StagedBytes;
        LoadNode* node = context->m_Nodes.GetPtr(index);
        node->m_Discovered = true;
        QueueCreate(context, node);
        --context->m_ActiveReads;
        StartReads(context, 0);
        Signal(context);
    }

    static int32_t ReadResourceJob(HJobContext jobs, HJob job, void* user_context, void* user_data)
    {
        LoadContext* context = (LoadContext*)user_context;
        uint32_t index = (uint32_t)(uintptr_t)user_data;
        ReadResource(context, index);
        return 0;
    }

    // The graph is unbounded. Input-byte backpressure may overshoot by active
    // reads and an unfinished dependency chain, which must still reach a leaf.
    // A caller can advance its own chain when unrelated staged work is paused.
    static uint32_t TakeRead(LoadContext* context, HPreloader request)
    {
        uint32_t index;
        if (context->m_StagedBytes >= STAGED_INPUT_LIMIT)
        {
            if (context->m_ActiveReads && (!request || !request->m_Blocking))
                return INVALID_LOAD_INDEX;
            if (request)
            {
                if (FindRequestWork(request, LOAD_WORK_CREATE, false) != INVALID_LOAD_INDEX || FindRequestWork(request, LOAD_WORK_POST, false) != INVALID_LOAD_INDEX)
                    return INVALID_LOAD_INDEX;
                index = FindRequestWork(request, LOAD_WORK_READ, true);
            }
            else
            {
                if (context->m_Queues[LOAD_WORK_CREATE].m_Count || context->m_Queues[LOAD_WORK_POST].m_Count)
                    return INVALID_LOAD_INDEX;
                index = PopLast(context, context->m_Queues[LOAD_WORK_READ]);
            }
        }
        else
            index = request ? FindRequestWork(request, LOAD_WORK_READ, true) : Pop(context, context->m_Queues[LOAD_WORK_READ]);
        if (index == INVALID_LOAD_INDEX)
            return INVALID_LOAD_INDEX;
        LoadNode* node = context->m_Nodes.GetPtr(index);
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
        return index;
    }

    static void StartReads(LoadContext* context, HPreloader request)
    {
        while (context->m_ActiveReads < context->m_ReadLimit)
        {
            uint32_t index = TakeRead(context, request);
            if (index == INVALID_LOAD_INDEX)
                break;
            LoadNode* node = context->m_Nodes.GetPtr(index);
            if (!node->m_Type)
                continue;
            Job job = {};
            job.m_Process = ReadResourceJob;
            job.m_Context = context;
            job.m_Data = (void*)(uintptr_t)index;
            node->m_Load->m_ReadJob = JobSystemCreateJob(context->m_Jobs, &job);
            Push(context->m_ReadJobs, index);
            JobSystemPushJob(context->m_Jobs, node->m_Load->m_ReadJob);
        }
    }

    static uint32_t TakeQueuedRead(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        SelectRequest(request);
        for (uint32_t i = 0; i < context->m_ReadJobs.Size(); ++i)
        {
            uint32_t index = context->m_ReadJobs[i];
            LoadNode* node = context->m_Nodes.GetPtr(index);
            if (node->m_Load->m_Selection != context->m_Selection)
                continue;
            // Cancellation wins only before the job starts. Its process callback
            // will then be skipped, so this caller can consume the same read once.
            if (JobSystemCancelJob(context->m_Jobs, node->m_Load->m_ReadJob) == JOBSYSTEM_RESULT_CANCELED)
            {
                RemoveReadJob(context, index);
                return index;
            }
        }
        return INVALID_LOAD_INDEX;
    }

    static bool ReadInline(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        uint32_t index;
        bool read = false;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            index = TakeQueuedRead(request);
            if (index == INVALID_LOAD_INDEX)
                index = TakeRead(context, request);
            if (index != INVALID_LOAD_INDEX)
                read = context->m_Nodes.Get(index).m_Type != 0;
        }
        // Blocking callers and zero-worker platforms advance only their own
        // resource reads. A caller may itself occupy the last free job worker.
        if (read)
            ReadResource(context, index);
        return index != INVALID_LOAD_INDEX;
    }

    LoadContext* NewLoadContext(HFactory factory)
    {
        LoadContext* context = new LoadContext();
        context->m_Factory = factory;
        context->m_Creating = INVALID_LOAD_INDEX;
        context->m_Jobs = GetPreloadJobContext(factory);
        context->m_ReadLimit = JobSystemGetWorkerCount(context->m_Jobs);
        for (uint32_t work = 0; work < LOAD_WORK_COUNT; ++work)
            context->m_Queues[work].m_Work = (LoadWork)work;
        context->m_Pins.m_Work = LOAD_WORK_COUNT;
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
        else if (child->m_Result != RESULT_OK && child->m_Result != RESULT_PENDING)
            parent->m_DependencyResult = child->m_Result;
        if (context->m_SelectedRequest && parent->m_Load && parent->m_Load->m_Selection == context->m_Selection)
            SelectDependencies(context, child->m_Index);
        return true;
    }

    bool PreloadHint(HResourcePreloadHintInfo info, const char* path)
    {
        if (!info || !path || CheckSuppliedResourcePath(path) != RESULT_OK)
            return false;
        LoadContext* context = info->m_Context;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t caller = context->m_Nodes.Get(info->m_Node).m_Load->m_Caller;
        HPreloader request = context->m_Nodes.Get(info->m_Node).m_Load->m_Request;
        uint32_t child = FindOrCreate(context, path, 0, caller, request);
        bool added = AddDependency(context, context->m_Nodes.GetPtr(info->m_Node), context->m_Nodes.GetPtr(child));
        StartReads(context, 0);
        return added;
    }

    static bool RecordDependency(LoadContext* context, LoadNode* child)
    {
        LoadNode* parent = context->m_Nodes.GetPtr(context->m_Creating);
        if (!AddDependency(context, parent, child))
            return false;
        RecreateContext* recreate = context->m_Recreating;
        if (recreate && recreate->m_Node == parent->m_Index)
        {
            for (uint32_t i = 0; i < recreate->m_Dependencies.Size(); ++i)
                if (recreate->m_Dependencies[i] == child->m_Index)
                    return true;
            Push(recreate->m_Dependencies, child->m_Index);
        }
        return true;
    }

    Result RecordResourceLoadGet(LoadContext* context, dmhash_t path)
    {
        if (!context)
            return RESULT_OK;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t* child = context->m_ByPath.Get(path);
        if (!child)
            return RESULT_OK;
        // Creation and preloader completion callbacks can consume created
        // dependencies. External callers must observe the final load result.
        if (context->m_UpdateDepth && context->m_UpdateOwner == GetCaller(context))
        {
            if (context->m_Creating != INVALID_LOAD_INDEX && !RecordDependency(context, context->m_Nodes.GetPtr(*child)))
                return RESULT_RESOURCE_LOOP_ERROR;
            return context->m_Nodes.Get(*child).m_CreateResult;
        }
        return context->m_Nodes.Get(*child).m_Result;
    }

    static void Created(LoadContext* context, LoadNode* node)
    {
        node->m_Created = true;
        for (uint32_t i = node->m_FirstDependent; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependent)
        {
            LoadNode* parent = context->m_Nodes.GetPtr(context->m_Edges[i].m_Parent);
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

    static void CreateResourceNode(LoadContext* context, uint32_t index)
    {
        LoadData* load;
        const char* path;
        HResourceType type;
        dmhash_t path_hash;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            LoadNode* node = context->m_Nodes.GetPtr(index);
            load = node->m_Load;
            path = node->m_Path;
            type = node->m_Type;
            path_hash = node->m_PathHash;
        }
        Result result = load->m_ReadResult != RESULT_OK ? load->m_ReadResult : load->m_PreloadResult;
        bool post = false;
        if (result == RESULT_OK)
        {
            ResourceDescriptor& descriptor = load->m_Descriptor;
            descriptor.m_NameHash = path_hash;
            descriptor.m_ResourceType = type;
            descriptor.m_ReferenceCount = 1;
            descriptor.m_ResourceSizeOnDisc = load->m_FileSize;
            ResourceCreateParams params = {};
            params.m_Factory = context->m_Factory;
            params.m_Type = type;
            params.m_Context = type->m_Context;
            params.m_Filename = path;
            params.m_Buffer = load->m_Provided ? load->m_ProvidedData : load->m_Buffer.Begin();
            params.m_BufferSize = load->m_BufferSize;
            params.m_FileSize = load->m_FileSize;
            params.m_IsBufferPartial = load->m_BufferSize != load->m_FileSize;
            params.m_PreloadData = load->m_PreloadData;
            params.m_Resource = &descriptor;
            uint32_t previous = context->m_Creating;
            context->m_Creating = index;
            {
                DM_PROFILE("ResourceCreate");
                DM_PROFILE_DYN(path, 0);
                uint64_t start = LoadProfileTime(&load->m_Profile);
                result = (Result)type->m_CreateFunction(&params);
                LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_CREATE, start);
            }
            context->m_Creating = previous;
            if (result == RESULT_OK)
            {
                post = type->m_PostCreateFunction != 0;
                {
                    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                    result = context->m_Nodes.Get(index).m_DependencyResult;
                }
                if (result == RESULT_OK)
                {
                    DM_MUTEX_SCOPED_LOCK(GetLoadMutex(context->m_Factory));
                    result = InsertResource(context->m_Factory, path, path_hash, &descriptor);
                    if (result == RESULT_OK)
                    {
                        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                        LoadNode* node = context->m_Nodes.GetPtr(index);
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
            LoadNode* node = context->m_Nodes.GetPtr(index);
            if (node->m_Resource)
                for (uint32_t i = 0; i < node->m_RootReferences; ++i)
                    IncRef(context->m_Factory, node->m_Resource);
            load->m_InCreate = false;
            node->m_CreateResult = result;
            node->m_PostDone = !post;
            Created(context, node);
            if (post)
                Push(context, context->m_Queues[LOAD_WORK_POST], index);
            else
                ReleaseStaging(context, node);
        }
    }

    static void PostCreateResourceNode(LoadContext* context, uint32_t index)
    {
        LoadData* load;
        ResourcePostCreateParams params = {};
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            LoadNode* node = context->m_Nodes.GetPtr(index);
            load = node->m_Load;
            node->m_InCallback = true;
            // A recreation caller may already be waiting for scheduler ownership.
            Signal(context);
            params.m_Factory = context->m_Factory;
            params.m_Type = node->m_Type;
            params.m_Context = node->m_Type->m_Context;
            params.m_Filename = node->m_Path;
            params.m_PreloadData = load->m_PreloadData;
            params.m_Resource = &load->m_Descriptor;
        }
        Result result;
        uint32_t previous = context->m_Creating;
        context->m_Creating = index;
        {
            DM_PROFILE("ResourcePostCreate");
            DM_PROFILE_DYN(params.m_Filename, 0);
            uint64_t start = LoadProfileTime(&load->m_Profile);
            result = (Result)params.m_Type->m_PostCreateFunction(&params);
            LoadProfileAddTime(&load->m_Profile, LOAD_PROFILE_POST_CREATE, start);
        }
        context->m_Creating = previous;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        LoadNode* node = context->m_Nodes.GetPtr(index);
        node->m_InCallback = false;
        if (result == RESULT_PENDING)
            return;
        if (node->m_CreateResult == RESULT_OK)
            node->m_CreateResult = result;
        node->m_PostDone = true;
        QueuePin(context, node);
        QueueComplete(context, node);
        ReleaseStaging(context, node);
    }

    // Dependencies can still be finalizing or be added during recreation.
    // Keep this load's cleanup state separate from readiness of the graph.
    static Result GetNodeResult(LoadContext* context, LoadNode* node)
    {
        if (node->m_CreateResult != RESULT_OK)
            return node->m_CreateResult;
        if (node->m_DependencyResult != RESULT_OK)
            return node->m_DependencyResult;
        if (node->m_PendingCompletions)
            return RESULT_PENDING;
        for (uint32_t i = node->m_FirstDependency; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependency)
            if (context->m_Nodes.Get(context->m_Edges[i].m_Child).m_Result == RESULT_PENDING)
                return RESULT_PENDING;
        return RESULT_OK;
    }

    static void RefreshDependencyResults(LoadContext* context, uint32_t index);

    static bool ProcessCompletion(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        uint32_t index;
        LoadData* load;
        HResourceType type;
        const char* path;
        dmhash_t path_hash;
        void* resource;
        Result result;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            index = FindRequestWork(request, LOAD_WORK_COMPLETE, true);
            if (index == INVALID_LOAD_INDEX)
                return false;
            LoadNode* node = context->m_Nodes.GetPtr(index);
            node->m_Result = GetNodeResult(context, node);
            // These stages are finished even if a shared dependency is still
            // pending. Its owner will publish final readiness through the graph.
            result = node->m_Result == RESULT_PENDING ? node->m_CreateResult : node->m_Result;
            load = node->m_Load;
            type = node->m_Type;
            path = node->m_Path;
            path_hash = node->m_PathHash;
            resource = node->m_Resource;
        }
        {
            DM_MUTEX_SCOPED_LOCK(GetLoadMutex(context->m_Factory));
            ResourceDescriptor* descriptor = FindByHash(context->m_Factory, path_hash);
            LogLoadProfile(&load->m_Profile, path, result, descriptor && descriptor->m_Resource == resource ? &descriptor->m_LoadTimings : 0);
            if (descriptor && descriptor->m_Resource == resource && load->m_Descriptor.m_ResourceSize)
                descriptor->m_ResourceSize = load->m_Descriptor.m_ResourceSize;
        }
        if (load->m_Destroy)
        {
            ResourceDestroyParams params = {};
            params.m_Factory = context->m_Factory;
            params.m_Type = type;
            params.m_Context = type->m_Context;
            params.m_Resource = &load->m_Descriptor;
            type->m_DestroyFunction(&params);
        }
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        LoadNode* node = context->m_Nodes.GetPtr(index);
        HPreloader owner = load->m_Request;
        uint32_t position = load->m_RequestIndex;
        owner->m_Loads.EraseSwap(position);
        if (position < owner->m_Loads.Size())
            context->m_Nodes.Get(owner->m_Loads[position]).m_Load->m_RequestIndex = position;
        node->m_Load = 0;
        delete load;
        node->m_Complete = true;
        for (uint32_t i = node->m_FirstDependent; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependent)
        {
            LoadNode* parent = context->m_Nodes.GetPtr(context->m_Edges[i].m_Parent);
            assert(parent->m_PendingCompletions);
            --parent->m_PendingCompletions;
            if (node->m_Result != RESULT_OK && node->m_Result != RESULT_PENDING)
                parent->m_DependencyResult = node->m_Result;
            if (parent->m_Complete && parent->m_Result != GetNodeResult(context, parent))
                RefreshDependencyResults(context, parent->m_Index);
        }
        ChangeHold(context, node, false);
        --node->m_References;
        DestroyUnusedNodes(context, index);
        return true;
    }

    static void ReleasePins(LoadContext* context)
    {
        for (;;)
        {
            dmhash_t path;
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                uint32_t index = Pop(context, context->m_Pins);
                if (index == INVALID_LOAD_INDEX)
                    return;
                LoadNode* node = context->m_Nodes.GetPtr(index);
                node->m_PinQueued = false;
                if ((node->m_Holds && !node->m_RootReferences) || !node->m_PostDone || !node->m_Pin)
                    continue;
                node->m_Pin = false;
                path = node->m_PathHash;
            }
            ReleaseResourceReference(context->m_Factory, path);
        }
    }

    // Called under the resource-table lock when its last reference is released.
    // A newly discovered parent can still need this resource before its Create runs.
    bool RetainResourceLoadReference(LoadContext* context, dmhash_t path, void* resource)
    {
        if (!context)
            return false;
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t* found = context->m_ByPath.Get(path);
        if (!found || context->m_Nodes.Get(*found).m_Resource != resource)
            return false;
        uint32_t index = *found;
        LoadNode* node = context->m_Nodes.GetPtr(index);
        if (node->m_Holds || node->m_RootReferences || !node->m_PostDone)
        {
            node->m_Pin = true;
            QueuePin(context, node);
            return true;
        }
        node->m_Resource = 0;
        context->m_ByPath.Erase(path);
        DestroyUnusedNodes(context, index);
        return false;
    }

    static void CallComplete(HPreloader request, FPreloaderCompleteCallback callback, PreloaderCompleteCallbackParams* params)
    {
        if (request->m_CallbackCalled || request->m_Result != RESULT_OK)
            return;
        {
            DM_MUTEX_SCOPED_LOCK(request->m_Context->m_Mutex);
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                if (!request->m_Context->m_Nodes.Get(request->m_Roots[i]).m_Created)
                    return;
            request->m_CallbackCalled = true;
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                if (request->m_Context->m_Nodes.Get(request->m_Roots[i]).m_CreateResult != RESULT_OK)
                    return;
        }
        if (callback && !callback(params))
            request->m_Result = RESULT_NOT_LOADED;
    }

    // Each update advances only its request, preserving FIFO order and callback
    // affinity for shared dependencies without spending other requests' budgets.
    static uint32_t FindRequestWork(HPreloader request, LoadWork work, bool remove)
    {
        LoadContext* context = request->m_Context;
        LoadQueue& queue = context->m_Queues[work];
        SelectRequest(request);
        uint32_t caller = request->m_CursorCaller;
        uint64_t& cursor = request->m_WorkCursor[work];
        for (uint32_t i = FindCursor(queue, cursor); i < queue.m_Items.Size(); ++i)
        {
            LoadQueueItem item = queue.m_Items[i];
            cursor = item.m_Sequence + 1;
            if (item.m_Node == INVALID_LOAD_INDEX)
                continue;
            LoadNode* node = context->m_Nodes.GetPtr(item.m_Node);
            if (node->m_Load->m_Selection != context->m_Selection || (work != LOAD_WORK_READ && node->m_Load->m_Caller != caller))
                continue;
            if (work == LOAD_WORK_POST && node->m_InCallback)
                continue;
            if (remove)
                Remove(context, queue, i);
            else
                cursor = item.m_Sequence;
            return item.m_Node;
        }
        return INVALID_LOAD_INDEX;
    }

    // Snapshot this request's posts in FIFO order. Keep them in the shared queue
    // until called, since a nested Get can need to finish the same dependency.
    static void GetRequestPosts(HPreloader request, dmArray<uint32_t>& posts)
    {
        LoadContext* context = request->m_Context;
        LoadQueue& queue = context->m_Queues[LOAD_WORK_POST];
        for (;;)
        {
            uint32_t index = FindRequestWork(request, LOAD_WORK_POST, false);
            if (index == INVALID_LOAD_INDEX)
                return;
            LoadNode* node = context->m_Nodes.GetPtr(index);
            request->m_WorkCursor[LOAD_WORK_POST] = queue.m_Items[node->m_Load->m_QueueIndex[LOAD_WORK_POST]].m_Sequence + 1;
            // Nested callbacks may complete and detach another snapshotted load.
            ++node->m_References;
            Push(posts, index);
        }
    }

    // Completed requests release their holds without advancing unrelated work.
    static Result CompleteRequest(HPreloader request)
    {
        LoadContext* context = request->m_Context;
        Result result = request->m_Result;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            if (request->m_Complete)
                return request->m_Result;
            if (!request->m_Loads.Empty())
                return RESULT_PENDING;
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
            {
                LoadNode* root = context->m_Nodes.GetPtr(request->m_Roots[i]);
                if (!root->m_Complete || (!request->m_Nested && root->m_Result == RESULT_PENDING))
                    return RESULT_PENDING;
                if (result == RESULT_OK)
                    result = root->m_Result == RESULT_PENDING ? root->m_CreateResult : root->m_Result;
            }
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                ChangeHold(context, context->m_Nodes.GetPtr(request->m_Roots[i]), false);
            // Later recreation changes graph readiness, not this request's result.
            request->m_Result = result;
            request->m_Complete = true;
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
        bool progressed = false;
        do
        {
            // Publish completed work before starting another creator. Otherwise
            // a small request can wait behind an unrelated large ready queue.
            if (ProcessCompletion(request))
                progressed = true;
            else
            {
                uint32_t index;
                {
                    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                    index = FindRequestWork(request, LOAD_WORK_CREATE, true);
                    if (index != INVALID_LOAD_INDEX)
                        context->m_Nodes.Get(index).m_Load->m_InCreate = true;
                }
                if (index != INVALID_LOAD_INDEX)
                    CreateResourceNode(context, index);
                else if ((!request->m_Blocking && context->m_ReadLimit) || !ReadInline(request))
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
        dmArray<uint32_t> posts;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            if (context->m_Queues[LOAD_WORK_POST].m_Count)
                GetRequestPosts(request, posts);
        }
        for (uint32_t i = 0; i < posts.Size(); ++i)
        {
            if (progressed && dmTime::GetMonotonicTime() - start >= soft_time_limit)
                break;
            uint32_t index = posts[i];
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                if (context->m_Nodes.Get(index).m_PostDone || context->m_Nodes.Get(index).m_InCallback)
                    continue;
                uint32_t position = context->m_Nodes.Get(index).m_Load->m_QueueIndex[LOAD_WORK_POST];
                if (position != INVALID_LOAD_INDEX)
                    Remove(context, context->m_Queues[LOAD_WORK_POST], position);
            }
            request->m_NextPostTime = dmTime::GetMonotonicTime() + 1000;
            PostCreateResourceNode(context, index);
            progressed = true;
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                if (!context->m_Nodes.Get(index).m_PostDone)
                    Push(context, context->m_Queues[LOAD_WORK_POST], index);
            }
        }
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            for (uint32_t i = 0; i < posts.Size(); ++i)
            {
                LoadNode* node = context->m_Nodes.GetPtr(posts[i]);
                if (node->m_Load && context->m_SelectedRequest == request)
                    RewindWork(context, node, LOAD_WORK_POST);
                --node->m_References;
                DestroyUnusedNodes(context, posts[i]);
            }
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
            if (request->m_Complete)
                return request->m_Result;
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

    static HPreloader NewRequest(HFactory factory, bool blocking)
    {
        HPreloader request = new ResourcePreloader();
        request->m_Context = GetLoadContext(factory);
        request->m_Result = RESULT_OK;
        request->m_Blocking = blocking;
        DM_MUTEX_SCOPED_LOCK(request->m_Context->m_Mutex);
        // Like a cached Get in a resource callback, a nested load can consume
        // created dependencies whose PostCreate belongs to an outer request.
        // It still finishes all loads it starts before releasing this request.
        request->m_Nested = blocking && request->m_Context->m_UpdateDepth && request->m_Context->m_UpdateOwner == GetCaller(request->m_Context);
        ++request->m_Context->m_Requests;
        return request;
    }

    static Result AddRoot(HPreloader request, const char* path, HResourceType type, const void* data, uint32_t size, uint32_t file_size, bool provided, bool dependency)
    {
        Result result = CheckSuppliedResourcePath(path);
        if (result != RESULT_OK)
            return result;
        LoadContext* context = request->m_Context;
        bool resident = false;
        dmhash_t path_hash;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            uint32_t caller = GetCaller(context);
            uint32_t index = FindOrCreate(context, path, type, caller, request);
            LoadNode* node = context->m_Nodes.GetPtr(index);
            path_hash = node->m_PathHash;
            bool new_load = node->m_References == 1 && !node->m_Discovered;
            if (dependency && context->m_UpdateDepth && context->m_UpdateOwner == caller && context->m_Creating != INVALID_LOAD_INDEX)
            {
                if (!RecordDependency(context, node))
                    return RESULT_RESOURCE_LOOP_ERROR;
            }
            // Reentrant callbacks must not wait for a creator suspended in Get.
            if (dependency && context->m_UpdateDepth && context->m_UpdateOwner == caller && node->m_Load && node->m_Load->m_InCreate)
                return RESULT_RESOURCE_LOOP_ERROR;
            for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
                if (request->m_Roots[i] == index)
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
            Push(request->m_Roots, index);
            if (context->m_SelectedRequest == request)
                SelectDependencies(context, index);
            if (node->m_Created)
                resident = node->m_Resource != 0;
            StartReads(context, request->m_Blocking ? request : 0);
        }
        if (resident)
            AcquireResourceReference(context->m_Factory, path_hash);
        return RESULT_OK;
    }

    HPreloader NewPreloader(HFactory factory, const dmArray<const char*>& names)
    {
        HPreloader request = NewRequest(factory, false);
        for (uint32_t i = 0; i < names.Size(); ++i)
        {
            Result result = AddRoot(request, names[i], 0, 0, 0, 0, false, false);
            if (request->m_Result == RESULT_OK)
                request->m_Result = result;
        }
        return request;
    }

    HPreloader NewPreloader(HFactory factory, const char* name)
    {
        HPreloader request = NewRequest(factory, false);
        request->m_Result = AddRoot(request, name, 0, 0, 0, 0, false, false);
        return request;
    }

    static void WaitForWork(HPreloader request)
    {
        LoadContext* context = request->m_Context;
#if defined(DM_HAS_THREADS)
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t caller = GetCaller(context);
        uint32_t depth = context->m_UpdateOwner == caller ? context->m_UpdateDepth : 0;
        uint32_t creating = depth ? context->m_Creating : INVALID_LOAD_INDEX;
        RecreateContext* recreating = depth ? context->m_Recreating : 0;
        // A nested Get may need a resource assigned to another requesting
        // thread. Let that thread run while this creator is suspended.
        if (depth)
        {
            context->m_UpdateDepth = 0;
            context->m_Creating = INVALID_LOAD_INDEX;
            context->m_Recreating = 0;
            Signal(context);
        }
        bool ready = FindRequestWork(request, LOAD_WORK_CREATE, false) != INVALID_LOAD_INDEX || FindRequestWork(request, LOAD_WORK_COMPLETE, false) != INVALID_LOAD_INDEX;
        bool post = FindRequestWork(request, LOAD_WORK_POST, false) != INVALID_LOAD_INDEX;
        if (request->m_Blocking || !context->m_ReadLimit)
            ready = ready || FindRequestWork(request, LOAD_WORK_READ, false) != INVALID_LOAD_INDEX;
        if (request->m_Blocking && !ready)
            for (uint32_t i = 0; i < context->m_ReadJobs.Size(); ++i)
                ready = ready || context->m_Nodes.Get(context->m_ReadJobs[i]).m_Load->m_Selection == context->m_Selection;
        bool foreign = false;
        bool complete = request->m_Loads.Empty();
        for (uint32_t i = 0; i < request->m_Roots.Size(); ++i)
        {
            LoadNode* root = context->m_Nodes.GetPtr(request->m_Roots[i]);
            if (!root->m_Complete || (!request->m_Nested && root->m_Result == RESULT_PENDING))
            {
                complete = false;
                foreign = foreign || !root->m_Load || root->m_Load->m_Caller != caller;
            }
        }
        uint64_t now = dmTime::GetMonotonicTime();
        if (!ready && post && request->m_NextPostTime > now)
        {
            // PostCreate has no completion signal yet. Bound polling while
            // yielding scheduler ownership, but do not delay useful callbacks.
            dmMutex::Unlock(context->m_Mutex);
            dmTime::Sleep((uint32_t)(request->m_NextPostTime - now));
            dmMutex::Lock(context->m_Mutex);
        }
        else if ((!complete && !ready && !post && (context->m_ActiveReads || foreign)) || (context->m_UpdateDepth && context->m_UpdateOwner != caller))
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

    static void DestroyUnusedNodes(LoadContext* context, uint32_t root)
    {
        dmArray<uint32_t> release;
        Push(release, root);
        while (!release.Empty())
        {
            uint32_t index = release.Back();
            LoadNode* node = context->m_Nodes.GetPtr(index);
            release.Pop();
            if (node->m_References || node->m_Resource)
                continue;
            assert(node->m_Complete && !node->m_Pin && !node->m_Load);
            uint32_t* found = context->m_ByPath.Get(node->m_PathHash);
            if (found && *found == index)
                context->m_ByPath.Erase(node->m_PathHash);
            for (uint32_t i = node->m_FirstDependency; i != INVALID_LOAD_INDEX;)
            {
                LoadEdge edge = context->m_Edges[i];
                LoadNode* child = context->m_Nodes.GetPtr(edge.m_Child);
                if (edge.m_PreviousDependent == INVALID_LOAD_INDEX)
                    child->m_FirstDependent = edge.m_NextDependent;
                else
                    context->m_Edges[edge.m_PreviousDependent].m_NextDependent = edge.m_NextDependent;
                if (edge.m_NextDependent != INVALID_LOAD_INDEX)
                    context->m_Edges[edge.m_NextDependent].m_PreviousDependent = edge.m_PreviousDependent;
                assert(child->m_References);
                --child->m_References;
                if (!child->m_References && !child->m_Resource)
                    Push(release, edge.m_Child);
                Push(context->m_FreeEdges, i);
                i = edge.m_NextDependency;
            }
            free(node->m_Path);
            context->m_Nodes.Free(index, false);
        }
    }

    static void ReleaseNode(LoadContext* context, uint32_t index)
    {
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        LoadNode* node = context->m_Nodes.GetPtr(index);
        assert(node->m_References);
        --node->m_References;
        DestroyUnusedNodes(context, index);
    }

    // Recreate can remove an error or encounter one during nested loading.
    // Reconcile the retained graph on both commit and rollback, including final
    // results already cached by completed ancestors. Own callback errors remain.
    static void RefreshDependencyResults(LoadContext* context, uint32_t index)
    {
        context->m_Visit.SetSize(0);
        Push(context->m_Visit, index);
        while (!context->m_Visit.Empty())
        {
            LoadNode* node = context->m_Nodes.GetPtr(context->m_Visit.Back());
            context->m_Visit.Pop();
            Result dependency_result = RESULT_OK;
            for (uint32_t i = node->m_FirstDependency; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependency)
            {
                LoadNode* child = context->m_Nodes.GetPtr(context->m_Edges[i].m_Child);
                if (child->m_Created && child->m_CreateResult != RESULT_OK)
                    dependency_result = child->m_CreateResult;
                if (child->m_Complete && child->m_Result != RESULT_OK && child->m_Result != RESULT_PENDING)
                    dependency_result = child->m_Result;
            }
            node->m_DependencyResult = dependency_result;
            if (!node->m_Complete)
                continue;
            Result result = GetNodeResult(context, node);
            if (result == node->m_Result)
                continue;
            node->m_Result = result;
            for (uint32_t i = node->m_FirstDependent; i != INVALID_LOAD_INDEX; i = context->m_Edges[i].m_NextDependent)
                Push(context->m_Visit, context->m_Edges[i].m_Parent);
        }
    }

    RecreateContext* BeginResourceRecreate(LoadContext* context, dmhash_t path)
    {
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        uint32_t caller = GetCaller(context);
        uint32_t* found;
        for (;;)
        {
            found = context->m_ByPath.Get(path);
            // An active callback may be waiting for this caller. Reject before
            // waiting for ownership, and look up the target again after waking.
            if (found && context->m_Nodes.Get(*found).m_InCallback)
                return 0;
#if defined(DM_HAS_THREADS)
            if (context->m_UpdateDepth && context->m_UpdateOwner != caller)
            {
                dmConditionVariable::Wait(context->m_Changed, context->m_Mutex);
                continue;
            }
#endif
            break;
        }
        RecreateContext* recreate = new RecreateContext();
        recreate->m_Context = context;
        recreate->m_Node = INVALID_LOAD_INDEX;
        context->m_UpdateOwner = caller;
        ++context->m_UpdateDepth;
        recreate->m_PreviousCreating = context->m_Creating;
        recreate->m_Previous = context->m_Recreating;
        if (found)
        {
            recreate->m_Node = *found;
            LoadNode* node = context->m_Nodes.GetPtr(*found);
            node->m_InCallback = true;
            Signal(context);
            recreate->m_FirstDependency = node->m_FirstDependency;
            ++node->m_References;
            ChangeHold(context, node, true);
        }
        context->m_Creating = recreate->m_Node;
        context->m_Recreating = recreate;
        return recreate;
    }

    void CommitResourceRecreate(RecreateContext* recreate, Result result, const ResourceDescriptor* descriptor)
    {
        LoadContext* context = recreate->m_Context;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            if (recreate->m_Node != INVALID_LOAD_INDEX)
            {
                LoadNode* node = context->m_Nodes.GetPtr(recreate->m_Node);
                uint32_t previous = INVALID_LOAD_INDEX;
                bool original = false;
                for (uint32_t i = node->m_FirstDependency; i != INVALID_LOAD_INDEX;)
                {
                    LoadEdge edge = context->m_Edges[i];
                    LoadNode* child = context->m_Nodes.GetPtr(edge.m_Child);
                    original = original || i == recreate->m_FirstDependency;
                    bool keep = original;
                    if (result == RESULT_OK)
                    {
                        keep = false;
                        for (uint32_t j = 0; j < recreate->m_Dependencies.Size(); ++j)
                            keep = keep || recreate->m_Dependencies[j] == edge.m_Child;
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
                        DestroyUnusedNodes(context, edge.m_Child);
                        node = context->m_Nodes.GetPtr(recreate->m_Node);
                        Push(context->m_FreeEdges, i);
                    }
                    i = edge.m_NextDependency;
                }
                if (result == RESULT_OK)
                {
                    node->m_Resource = descriptor->m_Resource;
                    // PostCreate uses the load's descriptor until finalization.
                    // Publish replacement identity and size to both views before
                    // the previous resource is destroyed by the factory.
                    if (node->m_Load)
                    {
                        node->m_Load->m_Descriptor.m_Resource = descriptor->m_Resource;
                        node->m_Load->m_Descriptor.m_ResourceSize = descriptor->m_ResourceSize;
                    }
                }
                RefreshDependencyResults(context, recreate->m_Node);
            }
            context->m_Creating = recreate->m_PreviousCreating;
            context->m_Recreating = recreate->m_Previous;
            context->m_SelectedRequest = 0;
        }
    }

    void EndResourceRecreate(RecreateContext* recreate)
    {
        LoadContext* context = recreate->m_Context;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
            if (recreate->m_Node != INVALID_LOAD_INDEX)
            {
                LoadNode* node = context->m_Nodes.GetPtr(recreate->m_Node);
                node->m_InCallback = false;
                ChangeHold(context, node, false);
                --node->m_References;
                DestroyUnusedNodes(context, recreate->m_Node);
            }
            context->m_Creating = INVALID_LOAD_INDEX;
            context->m_Recreating = 0;
        }
        // Dropping pins may destroy resources or load new ones. The factory has
        // finished publication and released its table lock before entering End.
        ReleasePins(context);
        DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
        context->m_Creating = recreate->m_PreviousCreating;
        context->m_Recreating = recreate->m_Previous;
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
            uint32_t index = request->m_Roots[i];
            bool resident;
            dmhash_t path;
            {
                DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
                LoadNode* root = context->m_Nodes.GetPtr(index);
                --root->m_RootReferences;
                resident = root->m_Resource != 0;
                path = root->m_PathHash;
            }
            if (resident)
                ReleaseResourceReference(context->m_Factory, path);
            ReleasePins(context);
            ReleaseNode(context, index);
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
        HPreloader request = NewRequest(factory, true);
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
            dmhash_t path_hash;
            {
                DM_MUTEX_SCOPED_LOCK(request->m_Context->m_Mutex);
                path_hash = request->m_Context->m_Nodes.Get(request->m_Roots[0]).m_PathHash;
            }
            *resource = AcquireResourceReference(factory, path_hash);
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
        {
            LoadNode& node = context->m_Nodes.GetRawObjects()[i];
            assert(node.m_Complete && !node.m_Pin && !node.m_Load);
            free(node.m_Path);
        }
#if defined(DM_HAS_THREADS)
        dmConditionVariable::Delete(context->m_Changed);
        dmThread::FreeTls(context->m_CallerKey);
#endif
        dmMutex::Delete(context->m_Mutex);
        delete context;
    }
} // namespace dmResource
