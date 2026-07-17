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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/uri.h>
#include <dlib/time.h>
#include <dlib/spinlock.h>

#include "block_allocator.h"
#include "resource.h"
#include "resource_private.h"
#include "resource_util.h"
#include "async/load_queue.h"

// The preloader maintains a bounded tree of active resource requests. The tree is stored in m_Request and
// indices are used for parent and sibling links. Resource hints that do not currently fit in the tree are
// stored in m_PendingHints until a request slot becomes available.
//
// 0) /rootcollection
//    1) /go1
//      3) /script1
//      4) /script2 PENDING
//    2) /go2
//      5) /tilemap
//        6) /textureset
//        7) /material
//
// RESULT_PENDING is used as the state for a request that has not produced a final resource result. Together
// with the other fields, a request can be in one of these states:
//
// => New (RESULT_PENDING, m_LoadRequest=0, m_Buffer=0)
// => Waiting for load through load queue, (RESULT_PENDING, m_LoadRequest=<handle>)
//    (Once the load completes, its preload function may add child hints)
// => Preloaded, waiting on children (RESULT_PENDING, m_Buffer=<data>, with admitted or queued children)
// => Created successfully, (RESULT_OK, m_Resource=<resource>, m_FirstChild == -1)
// => Created with error, (neither RESULT_PENDING nor RESULT_OK)
//
// Pending hints are consumed in LIFO order, so hints discovered by the current request are admitted before
// older sibling hints. This preserves depth-first loading: children are loaded and created before their parent.
//
// A parent is created only after both its admitted children and its queued child hints have been resolved.
// Completed leaves are retained until their parent is created and removed early so their request slots can be reused.
// After creating a parent, PreloaderTryPruneParent continues upward and recycles completed intermediate nodes.
//
// PreloadHint adds new items to a guarded dmArray. UpdatePreloader moves them to m_PendingHints after load
// completion and during normal updates. This keeps the synchronized state small and reduces lock contention.
//
// A path cache is also used to allow the path strings to be internalized without adding the full path length
// to each request item. It is synchronized by the hint-array spinlock, and its strings use stable allocations
// so PathDescriptor pointers remain valid when the cache grows.



typedef int16_t TRequestIndex;

// This bounds the number of request nodes resident in the active tree, not the total number of dependencies.
// A wide tree may contain more dependencies because completed siblings are removed and their slots are reused.
// At most MAX_PRELOADER_REQUESTS nodes from a single chain can be resident: while its deepest resource is loading,
// every parent on the path to it must remain resident so the preloader can later create the resources in
// child-to-parent order. If a deeper chain fills the tree, its next dependency is left for the blocked parent's
// Create callback to load synchronously, allowing the preloader to make progress without another request slot.

typedef dmHashTable<dmhash_t, uint32_t> TPathHashTable;
typedef dmHashTable<dmhash_t, bool> TPathInProgressTable;


static const uint32_t MAX_PRELOADER_REQUESTS         = 1024;
static const uint32_t PATH_IN_PROGRESS_TABLE_SIZE    = MAX_PRELOADER_REQUESTS / 3;
static const uint32_t PATH_IN_PROGRESS_CAPACITY      = MAX_PRELOADER_REQUESTS;
static const uint32_t PATH_IN_PROGRESS_HASHDATA_SIZE = (PATH_IN_PROGRESS_TABLE_SIZE * sizeof(uint32_t)) + (PATH_IN_PROGRESS_CAPACITY * sizeof(TPathInProgressTable::Entry));
static const uint32_t INITIAL_PATH_CAPACITY           = 1536;

// Identifies a resource path and type using preloader-owned strings that remain valid for the whole preload.
struct PathDescriptor
{
    const char* m_InternalizedName;
    const char* m_InternalizedCanonicalPath;
    HResourceType m_ResourceType;
    dmhash_t m_NameHash;
    dmhash_t m_CanonicalPathHash;
};

// Owns the descriptor and lifecycle state needed to run or unwind a deferred post-create callback.
struct ResourcePostCreateParamsInternal
{
    ResourcePostCreateParams m_Params;
    ResourceDescriptor m_ResourceDesc;
    bool m_Destroy;
};

// Describes a discovered dependency waiting to be linked beneath its active parent request.
struct PendingHint
{
    PathDescriptor m_PathDescriptor;
    TRequestIndex m_Parent;
};

// Holds a completed child's factory reference after its request slot has been recycled. The reference is
// released as soon as the parent has run Create and acquired any child references it needs.
struct RetainedResource
{
    void* m_Resource;
    TRequestIndex m_Parent;
};

// Represents one resident node in the bounded dependency tree and tracks its load/create state.
struct PreloadRequest
{
    PathDescriptor m_PathDescriptor;

    TRequestIndex m_Parent;
    TRequestIndex m_FirstChild;
    TRequestIndex m_NextSibling;
    uint16_t m_PendingChildCount;

    // Hints waiting outside m_Request. A request cannot be created while it still has queued children.
    uint32_t m_QueuedChildCount;

    // Set once resources have started loading, they have a load request
    dmLoadQueue::HRequest m_LoadRequest;

    // Set for items that are pending and waiting for children to complete
    void*       m_Buffer;
    uint32_t    m_BufferSize;
    uint32_t    m_FileSize;

    // Set once preload function has run
    void* m_PreloadData;

    // Set once load has completed
    dmResource::Result m_LoadResult;
    void* m_Resource;
};

// Owns the complete state of one asynchronous preload operation, including active requests and overflow hints.
struct ResourcePreloader
{
    ResourcePreloader()
        : m_InProgress(&m_PathInProgressData, PATH_IN_PROGRESS_TABLE_SIZE, PATH_IN_PROGRESS_CAPACITY)
    {
    }
    // Holds data written by preload callbacks and protected by m_SyncedDataSpinlock.
    struct SyncedData
    {
        SyncedData()
        {
            m_PathLookup.SetCapacity(INITIAL_PATH_CAPACITY / 3, INITIAL_PATH_CAPACITY);
            m_Paths.SetCapacity(INITIAL_PATH_CAPACITY);
        }
        dmArray<PendingHint> m_NewHints;
        TPathHashTable m_PathLookup;

        // Individually allocated strings keep PathDescriptor pointers stable when this array grows.
        dmArray<char*> m_Paths;
    } m_SyncedData;

    dmSpinlock::Spinlock m_SyncedDataSpinlock;

    PreloadRequest m_Request[MAX_PRELOADER_REQUESTS];

    // list of free nodes
    TRequestIndex m_Freelist[MAX_PRELOADER_REQUESTS];
    uint32_t m_FreelistSize;
    dmLoadQueue::HQueue m_LoadQueue;
    dmResource::HFactory m_Factory;
    TPathInProgressTable m_InProgress;
    uint8_t m_PathInProgressData[PATH_IN_PROGRESS_HASHDATA_SIZE];

    // used instead of dynamic allocs as far as it lasts.
    dmBlockAllocator::HContext m_BlockAllocator;

    // post create state
    bool m_LoadQueueFull;
    bool m_CreateComplete;
    uint32_t m_PostCreateCallbackIndex;
    dmArray<ResourcePostCreateParamsInternal> m_PostCreateCallbacks;

    // How many of the initial resources where requested - they should not be release until preloader destruction
    TRequestIndex m_PersistResourceCount;

    dmArray<void*> m_PersistedResources;

    dmArray<RetainedResource> m_RetainedResources;

    // Overflow hints live here until a completed leaf returns a slot to m_Freelist.
    dmArray<PendingHint> m_PendingHints;
};

namespace dmResource
{
    // Return a stable, preloader-owned path string. Called while building descriptors for roots and hints.
    const char* InternalizePath(ResourcePreloader::SyncedData* preloader_synced_data, dmhash_t path_hash, const char* path, uint32_t path_len)
    {
        uint32_t* path_lookup = preloader_synced_data->m_PathLookup.Get(path_hash);
        if (path_lookup != 0x0)
        {
            return preloader_synced_data->m_Paths[*path_lookup];
        }
        if (preloader_synced_data->m_PathLookup.Full())
        {
            uint32_t capacity = preloader_synced_data->m_PathLookup.Capacity() * 2;
            preloader_synced_data->m_PathLookup.SetCapacity(capacity / 3, capacity);
            preloader_synced_data->m_Paths.SetCapacity(capacity);
        }
        char* result = (char*) malloc(path_len + 1);
        if (!result)
        {
            return 0x0;
        }
        dmStrlCpy(result, path, path_len + 1);
        uint32_t path_index = preloader_synced_data->m_Paths.Size();
        preloader_synced_data->m_Paths.Push(result);
        preloader_synced_data->m_PathLookup.Put(path_hash, path_index);
        return result;
    }

    static HResourceType GetResourceType(HPreloader preloader, const char* path)
    {
        const char* ext = strrchr(path, '.');
        if (!ext)
        {
            dmLogWarning("Unknown resource type: '%s'. Missing file extension.", path);
            return 0;
        }
        else
        {
            HResourceType resource_type = FindResourceType(preloader->m_Factory, ext + 1);
            if (resource_type)
            {
                assert(resource_type->m_CreateFunction);
                return resource_type;
            }
            dmLogError("Unknown resource type: '%s'. Unknown resource type: %s", path, ext);
        }
        return 0;
    }

    Result MakePathDescriptor(ResourcePreloader* preloader, const char* name, PathDescriptor& out_path_descriptor)
    {
        if (name == 0x0)
        {
            return RESULT_INVALID_DATA;
        }
        // Ignore malformed paths that would fail anyway.
        Result res = CheckSuppliedResourcePath(name);
        if (res != dmResource::RESULT_OK)
        {
            return res;
        }
        uint32_t name_len = strlen(name);
        if (name_len >= RESOURCE_PATH_MAX)
        {
            dmLogError("Resource path is to long: (%s)", name);
            return RESULT_INVALID_DATA;
        }
        out_path_descriptor.m_NameHash = dmHashBuffer64(name, name_len);
        out_path_descriptor.m_ResourceType = GetResourceType(preloader, name);
        // We do not bail on out_path_descriptor.m_ResourceType being zero here, we want to
        // create a request so we can get a proper error code for the resource in the request

        char canonical_path[RESOURCE_PATH_MAX];
        uint32_t canonical_path_len = dmResource::GetCanonicalPath(name, canonical_path, sizeof(canonical_path));
        out_path_descriptor.m_CanonicalPathHash = dmHashBuffer64(canonical_path, canonical_path_len);

        DM_SPINLOCK_SCOPED_LOCK(preloader->m_SyncedDataSpinlock)
        {
            out_path_descriptor.m_InternalizedName = InternalizePath(&preloader->m_SyncedData, out_path_descriptor.m_NameHash, name, name_len);
            if (out_path_descriptor.m_InternalizedName == 0x0)
            {
                dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);
                return RESULT_OUT_OF_MEMORY;
            }
            out_path_descriptor.m_InternalizedCanonicalPath = InternalizePath(&preloader->m_SyncedData, out_path_descriptor.m_CanonicalPathHash, canonical_path, canonical_path_len);
            if (out_path_descriptor.m_InternalizedCanonicalPath == 0x0)
            {
                dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);
                return RESULT_OUT_OF_MEMORY;
            }
        }

        return RESULT_OK;
    }

    static bool IsPathInProgress(ResourcePreloader* preloader, const PathDescriptor* path_descriptor)
    {
        dmhash_t path_hash = path_descriptor->m_CanonicalPathHash;
        bool* path_lookup  = preloader->m_InProgress.Get(path_hash);
        return path_lookup != 0x0;
    }

    static void MarkPathInProgress(ResourcePreloader* preloader, const PathDescriptor* path_descriptor)
    {
        dmhash_t path_hash = path_descriptor->m_CanonicalPathHash;
        assert(preloader->m_InProgress.Get(path_hash) == 0x0);
        preloader->m_InProgress.Put(path_hash, true);
    }

    static void UnmarkPathInProgress(ResourcePreloader* preloader, const PathDescriptor* path_descriptor)
    {
        dmhash_t path_hash = path_descriptor->m_CanonicalPathHash;
        assert(preloader->m_InProgress.Get(path_hash) != 0x0);
        preloader->m_InProgress.Erase(path_hash);
    }

    static void PreloaderTreeInsert(ResourcePreloader* preloader, TRequestIndex index, TRequestIndex parent)
    {
        PreloadRequest* reqs      = &preloader->m_Request[0];
        reqs[index].m_NextSibling = reqs[parent].m_FirstChild;
        reqs[index].m_Parent      = parent;
        reqs[parent].m_FirstChild = index;
        reqs[parent].m_PendingChildCount += 1;
    }

    static void RemoveFromParentPendingCount(ResourcePreloader* preloader, PreloadRequest* req)
    {
        if (req->m_Parent != -1)
        {
            assert(preloader->m_Request[req->m_Parent].m_PendingChildCount > 0);
            preloader->m_Request[req->m_Parent].m_PendingChildCount -= 1;
        }
    }

    // Allocate and link an active request after the scheduler has made a request slot available.
    static Result PreloadPathDescriptor(HPreloader preloader, TRequestIndex parent, const PathDescriptor& path_descriptor)
    {
        // Quick deduplication, check if the child is already listed under the current parent
        TRequestIndex child = preloader->m_Request[parent].m_FirstChild;
        while (child != -1)
        {
            if (preloader->m_Request[child].m_PathDescriptor.m_NameHash == path_descriptor.m_NameHash)
            {
                return RESULT_ALREADY_REGISTERED;
            }
            child = preloader->m_Request[child].m_NextSibling;
        }

        if (!preloader->m_FreelistSize)
        {
            // Admission normally prevents this; keep the guard for direct callers.
            return RESULT_OUT_OF_MEMORY;
        }

        TRequestIndex new_req = preloader->m_Freelist[--preloader->m_FreelistSize];
        PreloadRequest* req   = &preloader->m_Request[new_req];
        memset(req, 0, sizeof(PreloadRequest));
        req->m_PathDescriptor    = path_descriptor;
        req->m_FirstChild        = -1;
        req->m_LoadResult        = RESULT_PENDING;

        PreloaderTreeInsert(preloader, new_req, parent);

        // Check for recursive resources, if it is, mark with loop error, the recursive load result will
        // be checked for in the PreloaderUpdateOneItem and the result will be propagated to the resource
        // preloaded creator.
        TRequestIndex go_up = parent;
        while (go_up != -1)
        {
            if (preloader->m_Request[go_up].m_PathDescriptor.m_CanonicalPathHash == path_descriptor.m_CanonicalPathHash)
            {
                req->m_LoadResult = RESULT_RESOURCE_LOOP_ERROR;
                assert(parent != -1);
                assert(preloader->m_Request[parent].m_PendingChildCount > 0);
                preloader->m_Request[parent].m_PendingChildCount -= 1;
                break;
            }
            go_up = preloader->m_Request[go_up].m_Parent;
        }
        return RESULT_OK;
    }

    // Check the overflow stack before queueing a hint that has not yet entered the active request tree.
    static bool IsPendingHintDuplicate(HPreloader preloader, const PendingHint& hint)
    {
        for (uint32_t i = 0; i < preloader->m_PendingHints.Size(); ++i)
        {
            const PendingHint& pending = preloader->m_PendingHints[i];
            if (pending.m_Parent == hint.m_Parent && pending.m_PathDescriptor.m_NameHash == hint.m_PathDescriptor.m_NameHash)
            {
                return true;
            }
        }
        return false;
    }

    // Add a discovered hint to the growable stack where it waits for request-slot admission.
    static void PushPendingHint(HPreloader preloader, const PendingHint& hint)
    {
        if (preloader->m_PendingHints.Full())
        {
            preloader->m_PendingHints.OffsetCapacity(128);
        }
        preloader->m_PendingHints.Push(hint);
    }

    // A full request tree can normally make progress by completing a leaf and recycling its slot. If every leaf
    // instead has queued children, no leaf can complete and no queued hint can be admitted. Drop the hints for one
    // such leaf so its Create callback loads those dependencies synchronously and frees the blocked request chain.
    static bool ResolveRequestTreeSlotDeadlock(HPreloader preloader)
    {
        if (preloader->m_FreelistSize || preloader->m_PendingHints.Empty())
        {
            return false;
        }

        for (uint32_t i = 0; i < MAX_PRELOADER_REQUESTS; ++i)
        {
            const PreloadRequest& request = preloader->m_Request[i];
            if (request.m_FirstChild == -1 && request.m_QueuedChildCount == 0)
            {
                return false;
            }
        }

        TRequestIndex blocked_parent = -1;
        for (uint32_t i = preloader->m_PendingHints.Size(); i > 0; --i)
        {
            const PendingHint& hint = preloader->m_PendingHints[i - 1];
            if (preloader->m_Request[hint.m_Parent].m_FirstChild == -1)
            {
                blocked_parent = hint.m_Parent;
                break;
            }
        }
        assert(blocked_parent != -1);

        PreloadRequest& parent_request = preloader->m_Request[blocked_parent];
        dmLogWarning("The preloader request tree is full while loading '%s'; queued dependencies will be loaded synchronously.",
                     parent_request.m_PathDescriptor.m_InternalizedName);

        for (uint32_t i = 0; i < preloader->m_PendingHints.Size();)
        {
            if (preloader->m_PendingHints[i].m_Parent == blocked_parent)
            {
                assert(parent_request.m_QueuedChildCount > 0);
                parent_request.m_QueuedChildCount--;
                preloader->m_PendingHints.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
        assert(parent_request.m_QueuedChildCount == 0);
        return true;
    }

    // Move the newest overflow hint into a free request slot during preloader updates. Returns true if a hint was
    // admitted, or if a full-tree deadlock was resolved by unblocking a leaf for synchronous dependency loading.
    // Returns false when there are no pending hints or the active tree can still make progress without a free slot.
    static bool AdmitPendingHint(HPreloader preloader)
    {
        if (preloader->m_PendingHints.Empty())
        {
            return false;
        }
        if (!preloader->m_FreelistSize)
        {
            return ResolveRequestTreeSlotDeadlock(preloader);
        }

        // LIFO makes hints discovered by the current request run before its remaining siblings.
        // This preserves depth-first loading and avoids filling every slot with a wide tree level.
        PendingHint hint = preloader->m_PendingHints.Back();
        preloader->m_PendingHints.Pop();
        assert(preloader->m_Request[hint.m_Parent].m_QueuedChildCount > 0);
        preloader->m_Request[hint.m_Parent].m_QueuedChildCount--;
        return PreloadPathDescriptor(preloader, hint.m_Parent, hint.m_PathDescriptor) == RESULT_OK;
    }

    // Transfer hints produced by preload callbacks into the update-thread stack, then admit one if possible.
    static bool PopHints(HPreloader preloader)
    {
        dmArray<PendingHint> new_hints;
        {
            DM_SPINLOCK_SCOPED_LOCK(preloader->m_SyncedDataSpinlock)
            new_hints.Swap(preloader->m_SyncedData.m_NewHints);
        }

        const uint32_t hint_count = new_hints.Size();
        const PendingHint* hints = new_hints.Begin();
        for (uint32_t i = 0; i < hint_count; ++i)
        {
            const PendingHint& hint = hints[i];
            if (IsPendingHintDuplicate(preloader, hint))
            {
                continue;
            }
            PushPendingHint(preloader, hint);
            preloader->m_Request[hint.m_Parent].m_QueuedChildCount++;
        }
        return AdmitPendingHint(preloader);
    }

    // Queue an additional initial resource supplied to NewPreloader as a child of its first request.
    static Result PreloadHintInternal(HPreloader preloader, TRequestIndex parent, const char* name)
    {
        PathDescriptor path_descriptor;
        Result res = MakePathDescriptor(preloader, name, path_descriptor);
        if (res != RESULT_OK)
        {
            return res;
        }
        PendingHint hint = { path_descriptor, parent };
        if (IsPendingHintDuplicate(preloader, hint))
        {
            return RESULT_ALREADY_REGISTERED;
        }
        PushPendingHint(preloader, hint);
        preloader->m_Request[parent].m_QueuedChildCount++;
        return RESULT_OK;
    }

    // Unlink a completed leaf from its parent's child list and return its slot to the freelist.
    static void PreloaderRemoveLeaf(ResourcePreloader* preloader, TRequestIndex index)
    {
        assert(preloader->m_FreelistSize < MAX_PRELOADER_REQUESTS);

        PreloadRequest* me = &preloader->m_Request[index];
        assert(me->m_FirstChild == -1);
        assert(me->m_PendingChildCount == 0);
        PreloadRequest* parent = &preloader->m_Request[me->m_Parent];

        TRequestIndex* child_link = &parent->m_FirstChild;
        while (*child_link != -1 && *child_link != index)
        {
            child_link = &preloader->m_Request[*child_link].m_NextSibling;
        }
        assert(*child_link == index);

        if (me->m_Resource)
        {
            if (index < preloader->m_PersistResourceCount)
            {
                if (preloader->m_PersistedResources.Full())
                {
                    preloader->m_PersistedResources.OffsetCapacity(128);
                }
                preloader->m_PersistedResources.Push(me->m_Resource);
            }
            else
            {
                Release(preloader->m_Factory, me->m_Resource);
            }
        }

        *child_link = me->m_NextSibling;

        if (me->m_LoadResult == RESULT_PENDING)
        {
            RemoveFromParentPendingCount(preloader, me);
        }

        preloader->m_Freelist[preloader->m_FreelistSize++] = index;
    }

    // Retain a completed resource and recycle its leaf slot while its parent is still waiting on other hints.
    static void RetainAndRemoveLeaf(ResourcePreloader* preloader, TRequestIndex index)
    {
        PreloadRequest* req = &preloader->m_Request[index];
        if (req->m_Resource)
        {
            // Keep the factory reference alive until the parent acquires the resource during Create.
            // The request node can then be recycled without causing a later synchronous reload.
            if (index < preloader->m_PersistResourceCount)
            {
                if (preloader->m_PersistedResources.Full())
                {
                    preloader->m_PersistedResources.OffsetCapacity(128);
                }
                preloader->m_PersistedResources.Push(req->m_Resource);
            }
            else
            {
                if (preloader->m_RetainedResources.Full())
                {
                    preloader->m_RetainedResources.OffsetCapacity(128);
                }
                RetainedResource retained = { req->m_Resource, req->m_Parent };
                preloader->m_RetainedResources.Push(retained);
            }
            req->m_Resource = 0;
        }
        PreloaderRemoveLeaf(preloader, index);
    }

    static void ReleaseRetainedResources(HPreloader preloader, TRequestIndex parent)
    {
        for (uint32_t i = 0; i < preloader->m_RetainedResources.Size();)
        {
            const RetainedResource& retained = preloader->m_RetainedResources[i];
            if (retained.m_Parent == parent)
            {
                Release(preloader->m_Factory, retained.m_Resource);
                preloader->m_RetainedResources.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
    }

    static void RemoveChildren(ResourcePreloader* preloader, PreloadRequest* req)
    {
        while (req->m_FirstChild != -1)
        {
            PreloaderRemoveLeaf(preloader, req->m_FirstChild);
        }
        assert(req->m_PendingChildCount == 0);

        TRequestIndex request_index = (TRequestIndex)(req - preloader->m_Request);
        ReleaseRetainedResources(preloader, request_index);
    }

    HPreloader NewPreloader(HFactory factory, const dmArray<const char*>& names)
    {
        ResourcePreloader* preloader = new ResourcePreloader();
        // root is always allocated so we don't add index zero in the free list
        for (uint32_t i = 0; i < MAX_PRELOADER_REQUESTS - 1; i++)
        {
            preloader->m_Freelist[i] = MAX_PRELOADER_REQUESTS - i - 1;
        }

        preloader->m_FreelistSize = MAX_PRELOADER_REQUESTS - 1;

        preloader->m_Factory         = factory;
        preloader->m_LoadQueue       = dmLoadQueue::CreateQueue(factory);
        dmSpinlock::Create(&preloader->m_SyncedDataSpinlock);

        preloader->m_PersistResourceCount = 0;
        preloader->m_PersistedResources.SetCapacity(names.Size());

        // Insert root.
        PreloadRequest* root = &preloader->m_Request[0];
        memset(root, 0x00, sizeof(PreloadRequest));

        root->m_LoadResult        = MakePathDescriptor(preloader, names[0], root->m_PathDescriptor);
        root->m_Parent            = -1;
        root->m_FirstChild        = -1;
        root->m_NextSibling       = -1;
        preloader->m_PersistResourceCount++;

        // Post create setup
        preloader->m_PostCreateCallbacks.SetCapacity(MAX_PRELOADER_REQUESTS / 8);
        preloader->m_LoadQueueFull           = false;
        preloader->m_CreateComplete          = false;
        preloader->m_PostCreateCallbackIndex = 0;

        preloader->m_BlockAllocator = dmBlockAllocator::CreateContext();

        if (root->m_LoadResult == RESULT_OK)
        {
            root->m_LoadResult = RESULT_PENDING;
        }

        // Add remaining items as children of root (first item).
        // This enables optimised loading in so that resources are not loaded multiple times
        // and are released when they can be (internally pruning and sharing the request tree).
        for (uint32_t i = 1; i < names.Size(); ++i)
        {
            Result res = PreloadHintInternal(preloader, 0, names[i]);
            if (res == RESULT_OK)
            {
                preloader->m_PersistResourceCount++;
            }
        }
        AdmitPendingHint(preloader);

        return preloader;
    }

    HPreloader NewPreloader(HFactory factory, const char* name)
    {
        const char* name_array[1] = { name };
        dmArray<const char*> names(name_array, 1, 1);
        return NewPreloader(factory, names);
    }

    // CreateResource operation ends either with
    //   1) Having created the resource and free:d all buffers => RESULT_OK + m_Resource
    //   2) Having failed, (or created and destroyed), leaving => RESULT_SOME_ERROR + everything free:d
    //
    // If buffer is null it means to use the items internal buffer
    static void CreateResource(HPreloader preloader, PreloadRequest* req, void* buffer, uint32_t buffer_size, uint32_t resource_size)
    {
        assert(req->m_LoadResult == RESULT_PENDING);
        assert(req->m_PendingChildCount == 0);

        assert(req->m_PathDescriptor.m_ResourceType);

        ResourceDescriptor tmp_resource;
        memset(&tmp_resource, 0, sizeof(tmp_resource));

        ResourceType* resource_type = req->m_PathDescriptor.m_ResourceType;

        // We must call CreateFunction if Preload function has been called, so always do this even when an error has occured
        tmp_resource.m_NameHash       = req->m_PathDescriptor.m_CanonicalPathHash;
        tmp_resource.m_ReferenceCount = 1;
        tmp_resource.m_ResourceType   = resource_type;

        ResourceCreateParams params;
        params.m_Factory     = preloader->m_Factory;
        params.m_Type        = resource_type;
        params.m_Context     = resource_type->m_Context;
        params.m_PreloadData = req->m_PreloadData;
        params.m_Resource    = &tmp_resource;
        params.m_Filename    = req->m_PathDescriptor.m_InternalizedName;
        params.m_FileSize                 = req->m_FileSize;

        if (!buffer)
        {
            assert(req->m_Buffer);
            tmp_resource.m_ResourceSizeOnDisc = req->m_BufferSize;
            params.m_Buffer                   = req->m_Buffer;
            params.m_BufferSize               = req->m_BufferSize;
            params.m_FileSize                 = req->m_FileSize;
            params.m_IsBufferPartial          = req->m_BufferSize != req->m_FileSize;
            req->m_LoadResult                 = (Result)resource_type->m_CreateFunction(&params);

            dmBlockAllocator::Free(preloader->m_BlockAllocator, req->m_Buffer, req->m_BufferSize);

            req->m_Buffer = 0;
        }
        else
        {
            tmp_resource.m_ResourceSizeOnDisc = buffer_size;
            params.m_Buffer                   = buffer;
            params.m_BufferSize               = buffer_size;
            params.m_FileSize                 = resource_size;
            params.m_IsBufferPartial          = buffer_size != resource_size;
            req->m_LoadResult                 = (Result)resource_type->m_CreateFunction(&params);
        }

        if (req->m_LoadResult == RESULT_OK)
        {
            if (resource_type->m_PostCreateFunction)
            {
                if (preloader->m_PostCreateCallbacks.Full())
                {
                    preloader->m_PostCreateCallbacks.OffsetCapacity(MAX_PRELOADER_REQUESTS / 8);
                }
                preloader->m_PostCreateCallbacks.SetSize(preloader->m_PostCreateCallbacks.Size() + 1);
                ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks.Back();
                ip.m_Destroy                         = false;
                ip.m_Params.m_Factory                = preloader->m_Factory;
                ip.m_Params.m_Type                   = resource_type;
                ip.m_Params.m_Context                = resource_type->m_Context;
                ip.m_Params.m_PreloadData            = req->m_PreloadData;
                ip.m_Params.m_Resource               = 0;
                memcpy(&ip.m_ResourceDesc, &tmp_resource, sizeof(ResourceDescriptor));
            }
        }

        assert(req->m_Buffer == 0);
        req->m_PreloadData = 0;

        RemoveFromParentPendingCount(preloader, req);

        // Children can now be removed (and both active and recycled child references released) as they are not
        // needed any longer and the parent resource has acquired its own references to the child resources.
        RemoveChildren(preloader, req);

        if (req->m_LoadResult != RESULT_OK)
        {
            // Continue up to parent. All the stuff below is only for resources that were created
            return;
        }

        assert(tmp_resource.m_Resource);

        // Only two options from now on is to either destroy the resource or have it inserted
        bool destroy = false;

        // If someone else has loaded the resource already, use that one and mark our loaded resource for destruction
        ResourceDescriptor* rd = FindByHash(preloader->m_Factory, req->m_PathDescriptor.m_CanonicalPathHash);
        if (rd)
        {
            // Use already loaded resource
            rd->m_ReferenceCount++;
            req->m_Resource = rd->m_Resource;
            destroy         = true;
        }
        else
        {
            // Insert the loaded and created resource, if insertion fails, mark the resource for detruction
            req->m_LoadResult = InsertResource(preloader->m_Factory, req->m_PathDescriptor.m_InternalizedName, req->m_PathDescriptor.m_CanonicalPathHash, &tmp_resource);
            if (req->m_LoadResult == RESULT_OK)
            {
                req->m_Resource = tmp_resource.m_Resource;
            }
            else
            {
                destroy = true;
            }
        }

        if (destroy)
        {
            assert(tmp_resource.m_Resource != 0);
            assert(resource_type != 0);

            bool found = false;

            // If the resource has a post create function, mark it as "to be destroyed"
            if (resource_type->m_PostCreateFunction)
            {
                for (uint32_t i = preloader->m_PostCreateCallbackIndex; i < preloader->m_PostCreateCallbacks.Size(); ++i)
                {
                    ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[i];
                    if (ip.m_ResourceDesc.m_Resource == tmp_resource.m_Resource)
                    {
                        // Mark resource that it should be destroyed once postcreate has been run.
                        ip.m_Destroy = true;
                        found        = true;

                        // NOTE: Loading multiple collection proxies with shared resources at the same
                        // time can result in these resources being loading more than once.
                        // We should fix so that this cannot happen since this is wasteful of both performance
                        // and memory usage. There is an issue filed for this: DEF-3088
                        // Once that has been implemented, the delayed destroy (m_Destroy) can be removed,
                        // it would no longer be needed since each resource would only be created/loaded once.
                        // Measurements from the amount of double-loading happening due to this in real games
                        // show that the triggered overhead is very low and it does not happen often so not
                        // a priority to fix.

                        break;
                    }
                }
            }

            // Is the resource wasn't found in the PostCreate callbacks, we can destroy it right away.
            if (!found)
            {
                ResourceDestroyParams params;
                params.m_Factory  = preloader->m_Factory;
                params.m_Type     = resource_type;
                params.m_Context  = resource_type->m_Context;
                params.m_Resource = &tmp_resource;
                resource_type->m_DestroyFunction(&params);
            }
        }
    }

    // Try to create the resource of the parent if all the child requests has been
    // resolved. We continue up the parent chain until we find a parent where all
    // children are not resolved and we break
    // Returns true if at least one parent in the chain is created
    static bool PreloaderTryPruneParent(HPreloader preloader, PreloadRequest* req)
    {
        TRequestIndex parent = req->m_Parent;
        if (parent == -1)
        {
            return false;
        }
        PreloadRequest* parent_req = &preloader->m_Request[parent];
        if (parent_req->m_PendingChildCount > 0)
        {
            return false;
        }
        if (parent_req->m_QueuedChildCount > 0)
        {
            // Some dependencies have not entered the bounded request tree yet.
            return false;
        }
        CreateResource(preloader, parent_req, 0, 0, 0);
        UnmarkPathInProgress(preloader, &parent_req->m_PathDescriptor);

        // The newly created parent can itself be a leaf whose parent still has queued or active
        // children. Retain and recycle it as well, otherwise wide trees fill m_Request with
        // completed intermediate nodes and pending hints can never acquire a slot.
        if (!PreloaderTryPruneParent(preloader, parent_req) && parent != 0)
        {
            RetainAndRemoveLeaf(preloader, parent);
        }
        return true;
    }

    // Ends the Load part of the resource and handles the result of the load
    // It will create the resource if it has no children, otherwise it will
    // copy the loaded buffer for later use when all the children has been created.
    //
    // Returns true if the resource was created
    static bool FinishLoad(HPreloader preloader, TRequestIndex index, PreloadRequest* req, dmLoadQueue::LoadResult& load_result, void* buffer, uint32_t buffer_size, uint32_t resource_size)
    {
        // Pop any hints the load/preload of the item that may have been generated
        PopHints(preloader);

        // Propagate errors
        if (load_result.m_LoadResult != RESULT_OK)
        {
            req->m_LoadResult = load_result.m_LoadResult;
        }
        else if (load_result.m_PreloadResult != RESULT_OK)
        {
            req->m_LoadResult = load_result.m_PreloadResult;
        }

        // On error remove all children
        if (req->m_LoadResult != RESULT_PENDING)
        {
            RemoveChildren(preloader, req);
            RemoveFromParentPendingCount(preloader, req);
        }

        req->m_PreloadData = load_result.m_PreloadData;

        bool created_resource = false;

        // Create only after both admitted and overflow children have been resolved.
        if (req->m_FirstChild == -1 && req->m_QueuedChildCount == 0)
        {
            if (req->m_LoadResult == RESULT_PENDING)
            {
                // Create the resource using the loading buffer directly.
                CreateResource(preloader, req, buffer, buffer_size, resource_size);
                created_resource = true;
            }
            UnmarkPathInProgress(preloader, &req->m_PathDescriptor);
            dmLoadQueue::FreeLoad(preloader->m_LoadQueue, req->m_LoadRequest);
            req->m_LoadRequest = 0;

            if (!PreloaderTryPruneParent(preloader, req) && index != 0)
            {
                RetainAndRemoveLeaf(preloader, index);
            }
        }
        else
        {
            // Keep the loaded bytes until we have loaded all children
            req->m_Buffer = dmBlockAllocator::Allocate(preloader->m_BlockAllocator, buffer_size);
            memcpy(req->m_Buffer, buffer, buffer_size);
            req->m_BufferSize = buffer_size;
            req->m_FileSize = resource_size;
            dmLoadQueue::FreeLoad(preloader->m_LoadQueue, req->m_LoadRequest);
            req->m_LoadRequest = 0;
        }
        return created_resource;
    }

    static bool DoPreloaderUpdateOneReq(HPreloader preloader, TRequestIndex index, PreloadRequest* req);

    // Find the first request that has is RESULT_PENDING and try to load it
    // Continue until all requests are checked or we have created a one resource
    static bool PreloaderUpdateOneItem(HPreloader preloader, TRequestIndex index)
    {
        DM_PROFILE("PreloaderUpdateOneItem");
        while (index >= 0)
        {
            PreloadRequest* req = &preloader->m_Request[index];
            switch (req->m_LoadResult)
            {
                case RESULT_PENDING:
                    if (DoPreloaderUpdateOneReq(preloader, index, req))
                    {
                        return true;
                    }
                    break;
                case RESULT_RESOURCE_LOOP_ERROR:
                    if (PreloaderTryPruneParent(preloader, req))
                    {
                        return true;
                    }
                    break;
                default:
                    break;
            }
            index = req->m_NextSibling;
        }
        return false;
    }

    // Returns true if the item or any of its children is found that has completed its loading from the load queue
    static bool DoPreloaderUpdateOneReq(HPreloader preloader, TRequestIndex index, PreloadRequest* req)
    {
        DM_PROFILE("DoPreloaderUpdateOneReq");

        assert(!req->m_Resource);

        if (req->m_PathDescriptor.m_ResourceType == 0)
        {
            req->m_LoadResult = RESULT_UNKNOWN_RESOURCE_TYPE;
            RemoveFromParentPendingCount(preloader, req);
            if (PreloaderTryPruneParent(preloader, req))
            {
                return true;
            }
            if (index != 0)
            {
                RetainAndRemoveLeaf(preloader, index);
            }
            return false;
        }

        // If loading it must finish first before trying to go down to children
        if (req->m_LoadRequest)
        {
            void* buffer;
            uint32_t buffer_size;
            uint32_t resource_size;

            // Can hold the buffer till we FreeLoad it
            dmLoadQueue::LoadResult res;
            dmLoadQueue::Result e = dmLoadQueue::EndLoad(preloader->m_LoadQueue, req->m_LoadRequest, &buffer, &buffer_size, &resource_size, &res);
            if (e == dmLoadQueue::RESULT_PENDING)
            {
                return false;
            }
            preloader->m_LoadQueueFull = false;

            if (FinishLoad(preloader, index, req, res, buffer, buffer_size, resource_size))
            {
                return true;
            }
            return false;
        }

        // It has a buffer if is waiting for children to complete first
        if (req->m_Buffer)
        {
            // traverse depth first
            if (PreloaderUpdateOneItem(preloader, req->m_FirstChild))
            {
                return true;
            }
            return false;
        }

        // It might have been loaded by unhinted resource Gets or loaded by a different preloader, just grab & bump refcount
        ResourceDescriptor* rd = FindByHash(preloader->m_Factory, req->m_PathDescriptor.m_CanonicalPathHash);
        if (rd)
        {
            rd->m_ReferenceCount++;
            req->m_Resource   = rd->m_Resource;
            req->m_LoadResult = RESULT_OK;
            RemoveChildren(preloader, req);
            RemoveFromParentPendingCount(preloader, req);
            if (PreloaderTryPruneParent(preloader, req))
            {
                return true;
            }
            if (index != 0)
            {
                RetainAndRemoveLeaf(preloader, index);
            }
            return false;
        }

        if (preloader->m_LoadQueueFull)
        {
            return false;
        }

        if (IsPathInProgress(preloader, &req->m_PathDescriptor))
        {
            // A different item in the resource tree is already loading the same resource, just wait
            return false;
        }

        dmLoadQueue::PreloadInfo info = {};
        info.m_HintInfo.m_Preloader = preloader;
        info.m_HintInfo.m_Parent    = index;
        info.m_CompleteFunction     = req->m_PathDescriptor.m_ResourceType->m_PreloadFunction;
        info.m_Context              = req->m_PathDescriptor.m_ResourceType->m_Context;
        info.m_Type                 = req->m_PathDescriptor.m_ResourceType;

        // If we can't add the request to the load queue it is because the queue is full
        // We will try again once we completed loading of an item via dmLoadQueue::EndLoad
        if ((req->m_LoadRequest = dmLoadQueue::BeginLoad(preloader->m_LoadQueue, req->m_PathDescriptor.m_InternalizedName, req->m_PathDescriptor.m_InternalizedCanonicalPath, &info)))
        {
            MarkPathInProgress(preloader, &req->m_PathDescriptor);
            return true;
        }

        preloader->m_LoadQueueFull = true;
        return false;
    }

    // Calls the PostCreate function of any pending resources
    // If the resource load is duplicate (indicated by ResourcePostCreateParamsInternal::m_Destroy)
    // the resource will be deleted but we still need to call the PostCreate function
    static Result PostCreateUpdateOneItem(HPreloader preloader)
    {
        ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[preloader->m_PostCreateCallbackIndex];
        ResourcePostCreateParams& params     = ip.m_Params;
        params.m_Resource                    = &ip.m_ResourceDesc;
        ResourceType* resource_type          = params.m_Resource->m_ResourceType;
        Result ret                           = (Result)resource_type->m_PostCreateFunction(&params);

        if (ret == RESULT_PENDING)
        {
            return ret;
        }
        ++preloader->m_PostCreateCallbackIndex;

        // Resource has been marked for delayed destroy after PostCreate function has been run.
        if (ip.m_Destroy)
        {
            ResourceDestroyParams params;
            params.m_Factory  = preloader->m_Factory;
            params.m_Type     = resource_type;
            params.m_Context  = resource_type->m_Context;
            params.m_Resource = &ip.m_ResourceDesc;
            resource_type->m_DestroyFunction(&params);
            ip.m_Destroy = false;
        }
        else {
            ResourceDescriptor* rd = FindByHash(preloader->m_Factory, params.m_Resource->m_NameHash);
            if (rd)
            {
                if (params.m_Resource->m_ResourceSize != 0)
                {
                    rd->m_ResourceSize = params.m_Resource->m_ResourceSize;
                }
            }
        }

        if (preloader->m_PostCreateCallbackIndex == preloader->m_PostCreateCallbacks.Size())
        {
            preloader->m_PostCreateCallbacks.SetSize(0);
            preloader->m_PostCreateCallbackIndex = 0;
        }

        return ret;
    }

    // Advance the preload operation until it completes or exhausts the soft time budget.
    // Each iteration:
    // - first advances deferred post-create callbacks by calling PostCreateUpdateOneItem()
    // - then walks the active request tree depth-first to start or finish loads and create
    //   resources whose children are ready using PreloaderUpdateOneItem()
    // - if the root is resolved it invokes the completion callback once and returns after
    //   all post-create work is done
    // - otherwise it transfers and admits newly discovered dependency hints using PopHints(),
    //   including full-tree deadlock recovery
    // - waiting briefly for asynchronous work or yielding with RESULT_PENDING when
    //   the time budget is reached.
    Result UpdatePreloader(HPreloader preloader, FPreloaderCompleteCallback complete_callback, PreloaderCompleteCallbackParams* complete_callback_params, uint32_t soft_time_limit)
    {
        DM_PROFILE("UpdatePreloader");

        uint64_t start           = dmTime::GetMonotonicTime();
        uint32_t empty_runs      = 0;
        bool close_to_time_limit = soft_time_limit < 1000;

        do
        {
            Result root_result        = preloader->m_Request[0].m_LoadResult;
            Result post_create_result = RESULT_OK;
            if (preloader->m_PostCreateCallbackIndex < preloader->m_PostCreateCallbacks.Size())
            {
                post_create_result = PostCreateUpdateOneItem(preloader);
                if (post_create_result != RESULT_PENDING)
                {
                    empty_runs = 0;
                    if (root_result == RESULT_OK)
                    {
                        // Just waiting for the post-create functions to complete
                        // If main result is RESULT_OK pick up any errors from
                        // post create function
                        preloader->m_Request[0].m_LoadResult = post_create_result;
                    }
                    continue;
                }
            }

            if (root_result == RESULT_PENDING)
            {
                if (PreloaderUpdateOneItem(preloader, 0))
                {
                    empty_runs = 0;
                    continue;
                }
            }
            else
            {
                if (!preloader->m_CreateComplete)
                {
                    // Root is resolved - all items has been created, we should now
                    // call the post-create function (if given) and then post-create
                    // of all created items
                    preloader->m_CreateComplete = true;
                    if (root_result == RESULT_OK && complete_callback)
                    {
                        if (!complete_callback(complete_callback_params))
                        {
                            preloader->m_Request[0].m_LoadResult = RESULT_NOT_LOADED;
                        }
                        empty_runs = 0;
                        // We need to continue to do all post create functions
                        continue;
                    }
                }

                if (post_create_result != RESULT_PENDING)
                {
                    // All done!
                    return root_result;
                }
            }

            if (PopHints(preloader))
            {
                empty_runs = 0;
                continue;
            }

            if (close_to_time_limit)
            {
                ++empty_runs;
                if (empty_runs > 3)
                {
                    break;
                }
            }
            else
            {
                close_to_time_limit = (dmTime::GetMonotonicTime() + 1000 - start) > soft_time_limit;
                if (close_to_time_limit)
                {
                    // Sleep a very short time, on windows it this small number just means "give up time slice"
                    dmTime::Sleep(1);
                    continue;
                }

                // In case of non-threaded loading, we never get any empty runs really.
                // In case of threaded loading and loading small files, use up a little
                // more of our time waiting for files to complete loading.
                dmTime::Sleep(1000);
            }
        } while (dmTime::GetMonotonicTime() - start <= soft_time_limit);

        return RESULT_PENDING;
    }

    void DeletePreloader(HPreloader preloader)
    {
        // Since Preload calls need their Create calls done and PostCreate calls must always follow Create calls.
        // To fix this:
        // * Make Get calls insta-fail on RESULT_ABORTED or something
        // * Make Queue only return RESULT_ABORTED always.
        // This is not a super-important use-case, the only way to trigger this is to start a load and
        // then do unload before it completes or if you destroy the collection while loading.
        // The normal operation is to issue a load and progress once complete.
        while (UpdatePreloader(preloader, 0, 0, 1000000) == RESULT_PENDING)
        {
            dmLogWarning("Waiting for preloader to complete.");
        }

        // Release root and persisted resources
        if (preloader->m_PersistedResources.Full())
        {
            preloader->m_PersistedResources.OffsetCapacity(1);
        }
        preloader->m_PersistedResources.Push(preloader->m_Request[0].m_Resource);
        for (uint32_t i = 0; i < preloader->m_PersistedResources.Size(); ++i)
        {
            void* resource = preloader->m_PersistedResources[i];
            if (!resource)
            {
                continue;
            }
            Release(preloader->m_Factory, resource);
        }
        for (uint32_t i = 0; i < preloader->m_RetainedResources.Size(); ++i)
        {
            Release(preloader->m_Factory, preloader->m_RetainedResources[i].m_Resource);
        }

        assert(preloader->m_FreelistSize == (MAX_PRELOADER_REQUESTS - 1));
        dmLoadQueue::DeleteQueue(preloader->m_LoadQueue);

        dmBlockAllocator::DeleteContext(preloader->m_BlockAllocator);

        for (uint32_t i = 0; i < preloader->m_SyncedData.m_Paths.Size(); ++i)
        {
            free(preloader->m_SyncedData.m_Paths[i]);
        }

        dmSpinlock::Destroy(&preloader->m_SyncedDataSpinlock);
        delete preloader;
    }

    bool PreloadHint(HResourcePreloadHintInfo info, const char* name)
    {
        if (!info || !name)
        {
            return false;
        }

        HPreloader preloader = info->m_Preloader;

        PathDescriptor path_descriptor;
        Result res = MakePathDescriptor(info->m_Preloader, name, path_descriptor);
        if (res != RESULT_OK)
        {
            return false;
        }

        DM_SPINLOCK_SCOPED_LOCK(preloader->m_SyncedDataSpinlock)
        uint32_t new_hints_size = preloader->m_SyncedData.m_NewHints.Size();
        if (preloader->m_SyncedData.m_NewHints.Capacity() == new_hints_size)
        {
            preloader->m_SyncedData.m_NewHints.OffsetCapacity(32);
        }
        preloader->m_SyncedData.m_NewHints.SetSize(new_hints_size + 1);
        PendingHint& hint     = preloader->m_SyncedData.m_NewHints.Back();
        hint.m_PathDescriptor = path_descriptor;
        hint.m_Parent         = info->m_Parent;

        return true;
    }
} // namespace dmResource
