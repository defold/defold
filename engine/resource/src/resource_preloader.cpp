#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <dlib/align.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dlib/uri.h>
#include <dlib/time.h>
#include <dlib/mutex.h>

#include "resource.h"
#include "resource_private.h"
#include "async/load_queue.h"

namespace dmResource
{
    // The preloader works as follow; a tree is constructed with each resource to be loaded as a node in the tree.
    // The tree is stored in m_Requests and indices are used to point around in the tree.
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
    // Nodes store the LoadResult as RESULT_PENDING, which is invalid as any actual result.
    // These are the 'real' states a node can be in.
    //
    // => New (RESULT_PENDING, m_LoadRequest=0, m_Buffer)
    // => Waiting for load through load queue, (RESULT_PENDING, m_LoadRequest=<handle>)
    //    (Once the load completes, the resource preload will have run and populated the node with children)
    // => Preloaded, waiting on children (RESULT_PENDING, m_Buffer=<data>, m_PreloadData=<data>, m_FirstChild != -1)
    // => Created successfully, (RESULT_OK, m_Resource=<resource>, m_FirstChild == -1)
    // => Created with error, (neither RESULT_PENDING nor RESULT_OK)
    //
    // Nodes are scheduled for load in depth first order. Once they are loaded they might add to the tree, and then
    // those will be handled next.
    //
    // Once a node with children finds none of them are in PENDING state any longer, resource create will happen,
    // child nodes (which are done) are then erased and the tree is traversed upwards to see if the parent can be
    // completed in the same manner. (PreloaderTryPrune)
    //
    // With the threaded load queue, PreloadHint will be called from an external thread, so the whole tree data
    // is guarded by a mutex. It is the only function that will be called in this manner. If the tree is exhausted
    // for free nodes, they are just thrown away, and the final Create function will Get synchronously.
    struct PreloadRequest
    {
        // Always set.
        const char* m_Path;
        uint64_t m_CanonicalPathHash;

        int32_t m_Parent;
        int32_t m_FirstChild;
        int32_t m_NextSibling;

        // Set once resources have started loading, they have a load request
        dmLoadQueue::HRequest m_LoadRequest;

        // Set for items that are pending and waiting for child loads
        void *m_Buffer;
        uint32_t m_BufferSize;

        // Set once preload function has run
        SResourceType* m_ResourceType;
        void *m_PreloadData;

        // Set once load has completed
        Result m_LoadResult;
        void *m_Resource;
    };

    // Internal data structure for passing parameters to postcreate function callbacks
    struct ResourcePostCreateParamsInternal
    {
        ResourcePostCreateParams m_Params;
        SResourceDescriptor m_ResourceDesc;
        bool m_Destroy;
    };

    // The preloader will function even down to a value of 1 here (the root object)
    // and sets the limit of how large a dependencies tree can be stored. Since nodes
    // are always present with all their children inserted (unless there was not room)
    // the required size is something the sum of all children on each level down along
    // the largest branch.
    static const uint32_t MAX_PRELOADER_REQUESTS        = 1024;
    static const uint32_t SCRATCH_BUFFER_SIZE           = 65536;
    static const uint32_t SCRATCH_BUFFER_THRESHOLD      = 4096;

    typedef dmHashTable<uint64_t, uint32_t> TPathHashTable;

    static const uint32_t PATH_AVERAGE_LENGTH           = 40;
    static const uint32_t MAX_PRELOADER_PATHS           = MAX_PRELOADER_REQUESTS * 2;
    static const uint32_t PATH_BUFFER_TABLE_SIZE        = (MAX_PRELOADER_PATHS * 2) / 3;
    static const uint32_t PATH_BUFFER_TABLE_CAPACITY    = MAX_PRELOADER_PATHS;
    static const uint32_t PATH_BUFFER_HASHDATA_SIZE     = (PATH_BUFFER_TABLE_SIZE * sizeof(uint32_t)) + (PATH_BUFFER_TABLE_CAPACITY * sizeof(TPathHashTable::Entry));

    struct PendingHint
    {
        const char* m_Path;
        uint64_t m_PathHash;
        int32_t m_Parent;
    };

    struct ResourcePreloader
    {
        ResourcePreloader()
            : m_PathLookup(&m_LookupData[0], PATH_BUFFER_TABLE_SIZE, PATH_BUFFER_TABLE_CAPACITY)
            , m_PathDataUsed(0)
        { 
        }
        dmMutex::Mutex m_NewHintMutex;
        dmArray<PendingHint> m_NewHints;
        TPathHashTable m_PathLookup;
        uint8_t m_LookupData[PATH_BUFFER_HASHDATA_SIZE];
        char m_PathData[MAX_PRELOADER_PATHS * PATH_AVERAGE_LENGTH];
        uint32_t m_PathDataUsed;

        PreloadRequest m_Request[MAX_PRELOADER_REQUESTS];

        // list of free nodes
        uint32_t m_Freelist[MAX_PRELOADER_REQUESTS];
        uint32_t m_FreelistSize;
        dmLoadQueue::HQueue m_LoadQueue;
        HFactory m_Factory;

        // used instead of dynamic allocs as far as it lasts.
        char m_ScratchBuffer[SCRATCH_BUFFER_SIZE];
        uint32_t m_ScratchBufferPos;

        // post create state
        bool m_CreateComplete;
        uint32_t m_PostCreateCallbackIndex;
        dmArray<ResourcePostCreateParamsInternal> m_PostCreateCallbacks;

        // How many of the initial resources where added - they should not be release until preloader destruction
        uint32_t m_PersistResourceCount;
    
        // persisted resources
        dmArray<void*> m_PersistedResources;

        uint64_t m_PreloaderCreationTimeNS;
        uint64_t m_MainThreadTimeSpentNS;
    };

    const char* InternalizePath(ResourcePreloader* preloader, uint64_t path_hash, const char* path, uint32_t path_len)
    {
        uint32_t* path_lookup = preloader->m_PathLookup.Get(path_hash);
        if (path_lookup != 0x0)
        {
            return &preloader->m_PathData[(*path_lookup) & 0x7fffffff];
        }
        if (preloader->m_PathLookup.Capacity() == preloader->m_PathLookup.Size())
        {
            return 0x0;
        }
        if (preloader->m_PathDataUsed + path_len + 1 >= sizeof(preloader->m_PathData))
        {
            return 0x0;
        }
        char* result = &preloader->m_PathData[preloader->m_PathDataUsed];
        dmStrlCpy(result, path, path_len + 1);
        preloader->m_PathLookup.Put(path_hash, preloader->m_PathDataUsed);
        preloader->m_PathDataUsed += path_len + 1;
        return result;
    }

    static bool IsPathInProgress(ResourcePreloader* preloader, uint64_t path_hash)
    {
        dmMutex::ScopedLock lk(preloader->m_NewHintMutex);
        uint32_t* path_lookup = preloader->m_PathLookup.Get(path_hash);
        assert(path_lookup != 0x0);
        return ((*path_lookup) & 0x80000000u) == 0x80000000u;
    }

    static void MarkPathInProgress(ResourcePreloader* preloader, uint64_t path_hash)
    {
        dmMutex::ScopedLock lk(preloader->m_NewHintMutex);
        uint32_t* path_lookup = preloader->m_PathLookup.Get(path_hash);
        assert(path_lookup != 0x0);
        assert(((*path_lookup) & 0x80000000u) == 0u);
        *path_lookup = (*path_lookup) | 0x80000000u;
    }

    static void UnmarkPathInProgress(ResourcePreloader* preloader, uint64_t path_hash)
    {
        dmMutex::ScopedLock lk(preloader->m_NewHintMutex);
        uint32_t* path_lookup = preloader->m_PathLookup.Get(path_hash);
        assert(path_lookup != 0x0);
        assert(((*path_lookup) & 0x80000000u) == 0x80000000u);
        *path_lookup = (*path_lookup) & 0x7fffffffu;
    }

    static void PreloaderTreeInsert(ResourcePreloader* preloader, int32_t index, int32_t parent)
    {
        PreloadRequest* reqs = &preloader->m_Request[0];
        reqs[index].m_NextSibling = reqs[parent].m_FirstChild;
        reqs[index].m_Parent = parent;
        reqs[parent].m_FirstChild = index;
    }

    static PreloadRequest* PreloadHashInternal(HPreloader preloader, int32_t parent, uint64_t path_hash, const char* name)
    {
        if (!preloader->m_FreelistSize)
        {
            // Preload queue is exhausted; this is not fatal.
            return 0x0;
        }

        // Quick dedupe; we can do it safely on the same parent
        int32_t child = preloader->m_Request[parent].m_FirstChild;
        while (child != -1)
        {
            if (preloader->m_Request[child].m_CanonicalPathHash == path_hash)
            {
                return &preloader->m_Request[child];
            }
            child = preloader->m_Request[child].m_NextSibling;
        }

        int32_t new_req = preloader->m_Freelist[--preloader->m_FreelistSize];
        PreloadRequest* req = &preloader->m_Request[new_req];
        memset(req, 0, sizeof(PreloadRequest));
        req->m_Path = name;
        req->m_CanonicalPathHash = path_hash;
        req->m_Parent = -1;
        req->m_FirstChild = -1;
        req->m_NextSibling = -1;
        req->m_LoadResult = RESULT_PENDING;

        PreloaderTreeInsert(preloader, new_req, parent);
        return req;
    }

    static bool PopHints(HPreloader preloader)
    {
        uint32_t new_hint_count = 0;
        dmArray<PendingHint> new_hints;
        {
            dmMutex::ScopedLock lk(preloader->m_NewHintMutex);
            new_hints.Swap(preloader->m_NewHints);
        }
        for (uint32_t i = 0; i < new_hints.Size(); ++i)
        {
            PendingHint* hint = &new_hints[i];
            assert(hint->m_Path >= preloader->m_PathData);
            assert(hint->m_Path < &preloader->m_PathData[preloader->m_PathDataUsed]);
            uint32_t j = i + 1;
            while (j < new_hints.Size())
            {
                PendingHint* test = &new_hints[j];
                if (test->m_PathHash == hint->m_PathHash &&
                    test->m_Parent == hint->m_Parent)
                {
                    assert(test->m_Path == hint->m_Path);
                    // Duplicate, ignore and go to next
                    hint = 0x0;
                    break;
                }
                ++j;
            }
            if (hint == 0x0)
            {
                continue;
            }
            // Ignore malformed paths that would fail anyway.
            if (CheckSuppliedResourcePath(hint->m_Path) != dmResource::RESULT_OK)
            {
                continue;
            }
            if (PreloadHashInternal(preloader, hint->m_Parent, hint->m_PathHash, hint->m_Path) != 0)
            {
                ++new_hint_count;
            }
        }
        return new_hint_count != 0;
    }

    static PreloadRequest* PreloadHintInternal(HPreloader preloader, int32_t parent, const char* name)
    {
        // Ignore malformed paths that would fail anyway.
        if (CheckSuppliedResourcePath(name) != dmResource::RESULT_OK)
        {
            return 0x0;
        }

        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(name, canonical_path);
        uint32_t path_len = strlen(canonical_path);
        uint64_t path_hash = dmHashBuffer64(canonical_path, path_len);
        const char* path = InternalizePath(preloader, path_hash, canonical_path, path_len);
        if (path == 0x0)
        {
            return 0x0;
        }

        return PreloadHashInternal(preloader, parent, path_hash, path);
    }

    // Only supports removing the first child, which is all the preloader uses anyway.
    static void PreloaderRemoveLeaf(ResourcePreloader* preloader, int32_t index)
    {
        assert(preloader->m_FreelistSize < MAX_PRELOADER_REQUESTS);

        PreloadRequest* me = &preloader->m_Request[index];
        assert(me->m_FirstChild == -1);

        if (me->m_Resource)
        {
            if(index < preloader->m_PersistResourceCount)
            {
                preloader->m_PersistedResources.Push(me->m_Resource);
            }
            else
            {
                Release(preloader->m_Factory, me->m_Resource);
            }
        }

        PreloadRequest* parent = &preloader->m_Request[me->m_Parent];
        assert(parent->m_FirstChild == index);
        parent->m_FirstChild = me->m_NextSibling;
        preloader->m_Freelist[preloader->m_FreelistSize++] = index;
    }

    HPreloader NewPreloader(HFactory factory, const dmArray<const char*>& names)
    {
        uint64_t start_ns = dmTime::GetTime();

        ResourcePreloader* preloader = new ResourcePreloader();
        // root is always allocated.
        for (uint32_t i=0;i<MAX_PRELOADER_REQUESTS-1;i++)
            preloader->m_Freelist[i] = MAX_PRELOADER_REQUESTS-i-1;

        preloader->m_FreelistSize = MAX_PRELOADER_REQUESTS - 1;
        preloader->m_ScratchBufferPos = 0;

        preloader->m_Factory = factory;
        preloader->m_LoadQueue = dmLoadQueue::CreateQueue(factory);
        preloader->m_NewHintMutex = dmMutex::New();

        preloader->m_PersistResourceCount = 0;
        preloader->m_PersistedResources.SetCapacity(names.Size());

        // Insert root.
        PreloadRequest* root = &preloader->m_Request[0];
        memset(root, 0x00, sizeof(PreloadRequest));

        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(names[0], canonical_path);
        uint32_t path_len = strlen(canonical_path);
        uint64_t path_hash = dmHashBuffer64(canonical_path, path_len);
        root->m_Path = InternalizePath(preloader, path_hash, canonical_path, path_len);
        assert(root->m_Path != 0x0);
        root->m_CanonicalPathHash = path_hash;
        root->m_Parent = -1;
        root->m_FirstChild = -1;
        root->m_NextSibling = -1;
        root->m_LoadResult = RESULT_PENDING;
        preloader->m_PersistResourceCount++;

        // Set up error condition
        Result r = CheckSuppliedResourcePath(names[0]);
        if (r != RESULT_OK)
        {
            root->m_LoadResult = r;
        }

        // Post create setup
        preloader->m_PostCreateCallbacks.SetCapacity(MAX_PRELOADER_REQUESTS / 8);
        preloader->m_CreateComplete = false;
        preloader->m_PostCreateCallbackIndex = 0;

        // Add remaining items as children of root (first item).
        // This enables optimised loading in so that resources are not loaded multiple times and are released when they can be (internally pruning and sharing the request tree).
        for(uint32_t i = 1; i < names.Size(); ++i)
        {
            PreloadRequest* r = PreloadHintInternal(preloader, 0, names[i]);
            if (r != 0x0)
            {
                assert(r == &preloader->m_Request[i]);
                preloader->m_PersistResourceCount++;
            }
        }

        uint64_t now_ns = dmTime::GetTime();
        uint64_t main_thread_time_ns = now_ns - start_ns;
        preloader->m_PreloaderCreationTimeNS = start_ns;
        preloader->m_MainThreadTimeSpentNS = main_thread_time_ns;

        return preloader;
    }

    HPreloader NewPreloader(HFactory factory, const char* name)
    {
        const char* name_array[1] = {name};
        dmArray<const char*> names (name_array, 1, 1);
        return NewPreloader(factory, names);
    }

    // CreateResource operation ends either with
    //   1) Nothing, because waiting on dependencies           => RESULT_PENDING (unchanged)
    //   2) Having created the resource and free:d all buffers => RESULT_OK + m_Resource
    //   3) Having failed, (or created and destroyed), leaving => RESULT_SOME_ERROR + everything free:d
    //
    // If buffer passed in is null it means to use the items internal buffer
    //
    // Returns true if an actual create attempt was peformed
    static bool PreloaderTryCreateResource(HPreloader preloader, int32_t index, void* buffer, uint32_t buffer_size)
    {
        PreloadRequest *req = &preloader->m_Request[index];
        assert(req->m_LoadResult == RESULT_PENDING);

        assert(req->m_ResourceType);
        // Any pending result will block the creation. There could be a quick path here where
        // we abort once anything goes into error, but it simplifies the algorithm to not do that
        // (since we would need to prune whole subtrees and not just leaves)
        int32_t child = req->m_FirstChild;
        while (child != -1)
        {
            PreloadRequest *sub = &preloader->m_Request[child];
            if (sub->m_LoadResult == RESULT_PENDING)
            {
                return false;
            }
            child = sub->m_NextSibling;
        }

        // Previous to this we could abort with RESULT_PENDING, but not any longer; at this point the request
        // is going to be finished and cleaned up. We can then remove it from the in progress
        UnmarkPathInProgress(preloader, req->m_CanonicalPathHash);

        SResourceDescriptor tmp_resource;
        memset(&tmp_resource, 0, sizeof(tmp_resource));

        SResourceType *resource_type = req->m_ResourceType;
        if (resource_type)
        {
            // obliged to call CreateFunction if Preload has ben called, so always do this even when an error has occured
            tmp_resource.m_NameHash = req->m_CanonicalPathHash;
            tmp_resource.m_ReferenceCount = 1;
            tmp_resource.m_ResourceType = (void*) resource_type;

            ResourceCreateParams params;
            params.m_Factory = preloader->m_Factory;
            params.m_Context = resource_type->m_Context;
            params.m_PreloadData = req->m_PreloadData;
            params.m_Resource = &tmp_resource;
            params.m_Filename = req->m_Path;

            if (!buffer)
            {
                assert(req->m_Buffer);
                tmp_resource.m_ResourceSizeOnDisc = req->m_BufferSize;
                params.m_Buffer = req->m_Buffer;
                params.m_BufferSize = req->m_BufferSize;
                req->m_LoadResult = resource_type->m_CreateFunction(params);

                // unless we took it from the scratch buffer it needs to be free:d
                if (req->m_Buffer < preloader->m_ScratchBuffer || req->m_Buffer >= (preloader->m_ScratchBuffer + SCRATCH_BUFFER_SIZE))
                {
                    free(req->m_Buffer);
                }

                req->m_Buffer = 0;
            }
            else
            {
                tmp_resource.m_ResourceSizeOnDisc = buffer_size;
                params.m_Buffer = buffer;
                params.m_BufferSize = buffer_size;
                req->m_LoadResult = resource_type->m_CreateFunction(params);
            }

            if(req->m_LoadResult == RESULT_OK)
            {
                if(resource_type->m_PostCreateFunction)
                {
                    if(preloader->m_PostCreateCallbacks.Full())
                    {
                        preloader->m_PostCreateCallbacks.OffsetCapacity(MAX_PRELOADER_REQUESTS / 8);
                    }
                    preloader->m_PostCreateCallbacks.SetSize(preloader->m_PostCreateCallbacks.Size()+1);
                    ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks.Back();
                    ip.m_Destroy = false;
                    ip.m_Params.m_Factory = preloader->m_Factory;
                    ip.m_Params.m_Context = resource_type->m_Context;
                    ip.m_Params.m_PreloadData = req->m_PreloadData;
                    ip.m_Params.m_Resource = 0;
                    memcpy(&ip.m_ResourceDesc, &tmp_resource, sizeof(SResourceDescriptor));
                }
            }

            assert(req->m_Buffer == 0);
            req->m_PreloadData = 0;

            // This is set if precreate was run. Once past this step it is not going to be needed;
            // and we clear it to indicate loading is done.
            req->m_ResourceType = 0;
        }

        // Children can now be removed (and their resources released) as they are not
        // needed any longer.
        while (req->m_FirstChild != -1)
        {
            PreloaderRemoveLeaf(preloader, req->m_FirstChild);
        }

        if (req->m_LoadResult != RESULT_OK)
        {
            // Continue up to parent. All the stuff below is only for resources that were created
            return true;
        }

        assert(tmp_resource.m_Resource);

        // Only two options from now on is to either destroy the resource or have it inserted
        bool destroy = false;

        // second need to destroy if already exists
        SResourceDescriptor* rd = GetByHash(preloader->m_Factory, req->m_CanonicalPathHash);
        if (rd)
        {
            // use already loaded then.
            rd->m_ReferenceCount++;
            req->m_Resource = rd->m_Resource;
            destroy = true;
        }
        else
        {
            // insert the already loaded, can fail here
            req->m_LoadResult = InsertResource(preloader->m_Factory, req->m_Path, req->m_CanonicalPathHash, &tmp_resource);
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
            // Remove from the post create callbacks if necessary
            for( uint32_t i = 0; i < preloader->m_PostCreateCallbacks.Size(); ++i )
            {
                ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[i];
                if (ip.m_ResourceDesc.m_Resource == tmp_resource.m_Resource)
                {
                    // Mark resource that is should be destroyed once postcreate has run.
                    ip.m_Destroy = true;
                    found = true;

                    // NOTE: Loading multiple collection proxies with shared resources at the same
                    // time can result in these resources being loading more than once.
                    // We should fix so that this cannot happen since this is wasteful of both performance
                    // and memory usage. There is an issue filed for this: DEF-3088
                    // Once that has been implemented, the delayed destroy (m_Destroy) can be removed,
                    // it would no longer be needed since each resource would only be created/loaded once.

                    break;
                }
            }

            // Is the resource wasn't found in the PostCreate callbacks, we can destroy it right away.
            if (!found) {
                ResourceDestroyParams params;
                params.m_Factory = preloader->m_Factory;
                params.m_Context = resource_type->m_Context;
                params.m_Resource = &tmp_resource;
                resource_type->m_DestroyFunction(params);
            }
        }

        req->m_ResourceType = 0;
        return true;
    }

    // Try to prune the branch from specified leaf and up as far as possible
    // by actually creating the resources.
    static void PreloaderTryPrune(HPreloader preloader, int32_t index)
    {
        // Finalises up the chain as far as possible. TryCreate resource will
        // return true if tree was ready.
        while (index >= 0 && PreloaderTryCreateResource(preloader, index, 0, 0))
        {
            index = preloader->m_Request[index].m_Parent;
        }
    }

    static uint32_t PreloaderTryEndLoad(HPreloader preloader, int32_t index)
    {
        PreloadRequest *req = &preloader->m_Request[index];
        assert(req->m_LoadRequest != 0);

        void *buffer;
        uint32_t buffer_size;

        // Can hold the buffer till we FreeLoad it
        dmLoadQueue::LoadResult res;
        dmLoadQueue::Result e = dmLoadQueue::EndLoad(preloader->m_LoadQueue, req->m_LoadRequest, &buffer, &buffer_size, &res);
        if (e != dmLoadQueue::RESULT_PENDING)
        {
            // Pop any hints the load/preload of the item that may have been generated
            PopHints(preloader);

            // Propagate errors
            if (res.m_LoadResult != RESULT_OK)
            {
                req->m_LoadResult = res.m_LoadResult;
            }
            else if (res.m_PreloadResult != RESULT_OK)
            {
                req->m_LoadResult = res.m_PreloadResult;
            }

            // On error remove all children
            if (req->m_LoadResult != RESULT_PENDING)
            {
                while (req->m_FirstChild != -1)
                {
                    PreloaderRemoveLeaf(preloader, req->m_FirstChild);
                }
            }

            req->m_PreloadData = res.m_PreloadData;

            // If no children, do the create step immediately with the buffer in place
            if (req->m_FirstChild == -1)
            {
                if (req->m_LoadResult == RESULT_PENDING)
                {
                    // The difference here vs running PreloaderTryPrune is that we do the create resource in-place
                    // on the buffer.
                    bool res = PreloaderTryCreateResource(preloader, index, buffer, buffer_size);
                    assert(res);
                }
                else
                {
                    // This is cleared in the TryCreate call above if we call it.
                    UnmarkPathInProgress(preloader, req->m_CanonicalPathHash);
                }
                PreloaderTryPrune(preloader, req->m_Parent);
            }
            else
            {
                // We need to hold on to the data for a bit longer now.
                if (buffer_size < SCRATCH_BUFFER_THRESHOLD && buffer_size <= (SCRATCH_BUFFER_SIZE - preloader->m_ScratchBufferPos))
                {
                    req->m_Buffer = &preloader->m_ScratchBuffer[preloader->m_ScratchBufferPos];
                    preloader->m_ScratchBufferPos += DM_ALIGN(buffer_size, 16);
                }
                else
                {
                    req->m_Buffer = malloc(buffer_size);
                }
                memcpy(req->m_Buffer, buffer, buffer_size);
                req->m_BufferSize = buffer_size;
            }

            dmLoadQueue::FreeLoad(preloader->m_LoadQueue, req->m_LoadRequest);
            req->m_LoadRequest = 0;
            return 1;
        }

        return 0;
    }

    static uint32_t DoPreloaderUpdateOneReq(HPreloader preloader, int32_t index);

    // Returns 1 if an item is found that has completed its loading from the load queue
    static uint32_t PreloaderUpdateOneItem(HPreloader preloader, int32_t index)
    {
        DM_PROFILE(Resource, "PreloaderUpdateOneItem");
        while (index >= 0)
        {
            PreloadRequest *req = &preloader->m_Request[index];
            if (req->m_LoadResult == RESULT_PENDING)
            {
                if (DoPreloaderUpdateOneReq(preloader, index))
                {
                    return 1;
                }
            }
            index = req->m_NextSibling;
        }
        return 0;
    }

    // Returns 1 if the item or any of its children is found that has completed its loading from the load queue
    static uint32_t DoPreloaderUpdateOneReq(HPreloader preloader, int32_t index)
    {
        DM_PROFILE(Resource, "DoPreloaderUpdateOneReq");

        PreloadRequest *req = &preloader->m_Request[index];
        // If it does not have a load request, it would have a buffer if it
        // had started a load.
        if (!req->m_LoadRequest && !req->m_Buffer && !req->m_Resource)
        {
            bool wait_on = IsPathInProgress(preloader, req->m_CanonicalPathHash);
            if (wait_on)
            {
                // Already being loaded elsewhere; we can wait on that to complete, unless the item
                // exists above us in the tree; then the loop is infinite and we can abort
                int32_t parent = req->m_Parent;
                int32_t go_up = parent;
                while (go_up != -1)
                {
                    if (preloader->m_Request[go_up].m_CanonicalPathHash == req->m_CanonicalPathHash)
                    {
                        req->m_LoadResult = RESULT_RESOURCE_LOOP_ERROR;
                        PreloaderTryPrune(preloader, parent);
                        return 1;
                    }
                    go_up = preloader->m_Request[go_up].m_Parent;
                }
                // OK to wait.
                return 0;
            }

            // It might have been loaded by unhinted resource Gets, just grab & bump refcount
            SResourceDescriptor* rd = GetByHash(preloader->m_Factory, req->m_CanonicalPathHash);
            if (rd)
            {
                rd->m_ReferenceCount++;
                req->m_Resource = rd->m_Resource;
                req->m_LoadResult = RESULT_OK;
                PreloaderTryPrune(preloader, req->m_Parent);
                return 1;
            }
            else
            {
                if (!req->m_ResourceType)
                {
                    const char* ext = strrchr(req->m_Path, '.');
                    if (!ext)
                    {
                        dmLogWarning("Unable to load resource: '%s'. Missing file extension.", req->m_Path);
                        req->m_LoadResult = RESULT_MISSING_FILE_EXTENSION;
                        PreloaderTryPrune(preloader, req->m_Parent);
                        return 1;
                    }

                    req->m_ResourceType = FindResourceType(preloader->m_Factory, ext + 1);
                    if (!req->m_ResourceType)
                    {
                        dmLogError("Unknown resource type: %s", ext);
                        req->m_LoadResult = RESULT_UNKNOWN_RESOURCE_TYPE;
                        PreloaderTryPrune(preloader, req->m_Parent);
                        return 1;
                    }
                }

                dmLoadQueue::PreloadInfo info;
                info.m_HintInfo.m_Preloader = preloader;
                info.m_HintInfo.m_Parent = index;
                info.m_Function = req->m_ResourceType->m_PreloadFunction;
                info.m_Context = req->m_ResourceType->m_Context;

                // Silently ignore if return null (means try later)"
                if ((req->m_LoadRequest = dmLoadQueue::BeginLoad(preloader->m_LoadQueue, req->m_Path, &info)))
                {
                    MarkPathInProgress(preloader, req->m_CanonicalPathHash);
                    return 1;
                }
            }
        }

        // If loading it must finish first before trying to go down to children
        if (req->m_LoadRequest)
        {
            if (PreloaderTryEndLoad(preloader, index))
                return 1;
        }
        else
        {
            // traverse depth first
            if (PreloaderUpdateOneItem(preloader, req->m_FirstChild))
                return 1;
        }

        return 0;
    }

    static Result PostCreateUpdateOneItem(HPreloader preloader, bool& empty_run)
    {
        empty_run = true;
        uint32_t post_create_size = preloader->m_PostCreateCallbacks.Size();
        if(preloader->m_PostCreateCallbackIndex == post_create_size)
        {
            return RESULT_OK;
        }
        if (post_create_size == 0)
        {
            return RESULT_OK;
        }

        Result ret = RESULT_OK;
        ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[preloader->m_PostCreateCallbackIndex];
        ResourcePostCreateParams& params = ip.m_Params;
        params.m_Resource = &ip.m_ResourceDesc;
        SResourceType *resource_type = (SResourceType*) params.m_Resource->m_ResourceType;
        if(resource_type->m_PostCreateFunction)
        {
            ret = resource_type->m_PostCreateFunction(params);
            empty_run = false;
        }

        if (ret == RESULT_PENDING)
            return ret;
        ++preloader->m_PostCreateCallbackIndex;
        if (ret == RESULT_OK)
        {
            // Resource has been marked for delayed destroy after PostCreate function has been run.
            if (ip.m_Destroy)
            {
                ResourceDestroyParams params;
                params.m_Factory = preloader->m_Factory;
                params.m_Context = resource_type->m_Context;
                params.m_Resource = &ip.m_ResourceDesc;
                resource_type->m_DestroyFunction(params);
                ip.m_Destroy = false;
                empty_run = false;
            }

            if(preloader->m_PostCreateCallbackIndex < post_create_size)
            {
                return RESULT_PENDING;
            }
            empty_run = false;
            preloader->m_PostCreateCallbacks.SetSize(0);
            preloader->m_PostCreateCallbackIndex = 0;
        }
        return ret;
    }


    Result UpdatePreloader(HPreloader preloader, FPreloaderCompleteCallback complete_callback, PreloaderCompleteCallbackParams* complete_callback_params, uint32_t soft_time_limit)
    {
        DM_PROFILE(Resource, "UpdatePreloader");

        uint64_t start = dmTime::GetTime();
        uint32_t empty_runs = 0;
        bool close_to_time_limit = soft_time_limit < 1000;

        do
        {
            Result root_result = preloader->m_Request[0].m_LoadResult;
            bool empty_run;
            Result post_create_result = PostCreateUpdateOneItem(preloader, empty_run);
            if (!empty_run)
            {
                empty_runs = 0;
                if (post_create_result != RESULT_PENDING && root_result == RESULT_OK)
                {
                    preloader->m_Request[0].m_LoadResult = post_create_result;
                }
                continue;
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
                        if(!complete_callback(complete_callback_params))
                        {
                            preloader->m_Request[0].m_LoadResult = RESULT_NOT_LOADED;
                        }
                        empty_runs = 0;
                        // We need to continue to do all post create functions
                        continue;
                    }
                }

                if (post_create_result == RESULT_OK)
                {
                    // All done!
//                    uint64_t main_thread_elapsed_ns = dmTime::GetTime() - start;
//                    preloader->m_MainThreadTimeSpentNS += main_thread_elapsed_ns;
                    return root_result;
                }
            }

            if (close_to_time_limit)
            {
                ++empty_runs;
                if (empty_runs > 10)
                {
                    break;
                }
            }
            else
            {
                close_to_time_limit = (dmTime::GetTime() + 1000 - start) > soft_time_limit;
                if (!close_to_time_limit)
                {
                    // In case of non-threaded loading, we never get any empty runs really.
                    // In case of threaded loading and loading small files, use up a little
                    // more of our time waiting for files.
                    dmTime::Sleep(1000);
                }
            }

        } while(dmTime::GetTime() - start <= soft_time_limit);

        uint64_t main_thread_elapsed_ns = dmTime::GetTime() - start;
        preloader->m_MainThreadTimeSpentNS += main_thread_elapsed_ns;
        return RESULT_PENDING;
    }

    void DeletePreloader(HPreloader preloader)
    {
        // Since Preload calls need their Create calls done and PostCreate calls must follow Create calls.
        // To fix this:
        // * Make Get calls insta-fail on RESULT_ABORTED or something
        // * Make Queue only return RESULT_ABORTED always.
        while (UpdatePreloader(preloader, 0, 0, 1000000) == RESULT_PENDING)
        {
            dmLogWarning("Waiting for preloader to complete.");
        }

        uint64_t start_excluding_update_ns = dmTime::GetTime();

        // Release root and persisted resources
        preloader->m_PersistedResources.Push(preloader->m_Request[0].m_Resource);
        for(uint32_t i = 0; i < preloader->m_PersistedResources.Size(); ++i)
        {
            void* resource = preloader->m_PersistedResources[i];
            if(!resource)
                continue;
            Release(preloader->m_Factory, resource);
        }

        assert(preloader->m_FreelistSize == (MAX_PRELOADER_REQUESTS-1));
        dmLoadQueue::DeleteQueue(preloader->m_LoadQueue);
        dmMutex::Delete(preloader->m_NewHintMutex);

        uint64_t now_ns = dmTime::GetTime();
        uint64_t preloader_load_time_ns = now_ns - preloader->m_PreloaderCreationTimeNS;
        uint64_t main_thread_time_ns = now_ns - start_excluding_update_ns;
        preloader->m_MainThreadTimeSpentNS += main_thread_time_ns;

        dmLogWarning("Preloading root \"%s\" took %u ms, spending %u ms in main thread",
            preloader->m_Request[0].m_Path,
            (uint32_t)(preloader_load_time_ns / 1000),
            (uint32_t)(preloader->m_MainThreadTimeSpentNS / 1000));

        delete preloader;
    }

    // This function can be called from a different thread.
    // No lock should be held during this call.
    bool PreloadHint(HPreloadHintInfo info, const char* name)
    {
        if (!info || !name)
            return false;
        assert(name[0] != 0);
        HPreloader preloader = info->m_Preloader;
        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(name, canonical_path);
        uint32_t path_len = strlen(canonical_path);
        uint64_t path_hash = dmHashBuffer64(canonical_path, path_len);

        dmMutex::ScopedLock lk(preloader->m_NewHintMutex);
        const char* path = InternalizePath(preloader, path_hash, canonical_path, path_len);
        if (path == 0)
        {
            return false;
        }
        PendingHint hint;
        hint.m_Path = path;
        hint.m_PathHash = path_hash;
        hint.m_Parent = info->m_Parent;
        if (preloader->m_NewHints.Capacity() == preloader->m_NewHints.Size())
        {
            preloader->m_NewHints.OffsetCapacity(32);
        }
        preloader->m_NewHints.Push(hint);
        return true;
    }
}
