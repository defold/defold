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
        char m_Path[RESOURCE_PATH_MAX];
        uint64_t m_PathHash;
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

        // If request should not be dereferenced until preloader is destroyed
        uint32_t m_Persist : 1;
    };

    // Internal data structure for passing parameters to postcreate function callbacks
    struct ResourcePostCreateParamsInternal
    {
        ResourcePostCreateParams m_Params;
        SResourceDescriptor m_ResourceDesc;
        bool m_Removed;
    };

    // The preloader will function even down to a value of 1 here (the root object)
    // and sets the limit of how large a dependencies tree can be stored. Since nodes
    // are always present with all their children inserted (unless there was not room)
    // the required size is something the sum of all children on each level down along
    // the largest branch.
    static const uint32_t MAX_PRELOADER_REQUESTS = 192;
    static const uint32_t SCRATCH_BUFFER_SIZE = 65536;
    static const uint32_t SCRATCH_BUFFER_THRESHOLD = 4096;

    struct ResourcePreloader
    {
        dmMutex::Mutex m_Mutex;
        PreloadRequest m_Request[MAX_PRELOADER_REQUESTS];

        // list of free nodes
        uint32_t m_Freelist[MAX_PRELOADER_REQUESTS];
        uint32_t m_FreelistSize;
        dmLoadQueue::HQueue m_LoadQueue;
        HFactory m_Factory;
        dmHashTable<uint64_t, PreloadRequest*> m_InProgress;

        // used instead of dynamic allocs as far as it lasts.
        char m_ScratchBuffer[SCRATCH_BUFFER_SIZE];
        uint32_t m_ScratchBufferPos;

        // post create state
        bool m_CreateComplete;
        uint32_t m_PostCreateCallbackIndex;
        dmArray<ResourcePostCreateParamsInternal> m_PostCreateCallbacks;

        // persisted resources
        dmArray<void*> m_PersistedResources;
    };

    static bool PreloadHintInternal(HPreloader, int32_t, const char*, bool);

    static void MakeNewRequest(PreloadRequest* request, HFactory factory, const char *name, bool persist)
    {
        memset(request, 0x00, sizeof(PreloadRequest));
        dmStrlCpy(request->m_Path, name, RESOURCE_PATH_MAX);
        request->m_PathHash = dmHashBuffer64(name, strlen(name));
        request->m_Parent = -1;
        request->m_FirstChild = -1;
        request->m_NextSibling = -1;
        request->m_LoadResult = RESULT_PENDING;
        request->m_Persist = persist;
    }

    static void PreloaderTreeInsert(ResourcePreloader* preloader, int32_t index, int32_t parent)
    {
        PreloadRequest* reqs = &preloader->m_Request[0];
        reqs[index].m_NextSibling = reqs[parent].m_FirstChild;
        reqs[index].m_Parent = parent;
        reqs[parent].m_FirstChild = index;
    }

    // Only supports removing the first child, which is all the preloader uses anyway.
    static void PreloaderRemoveLeaf(ResourcePreloader* preloader, int32_t index)
    {
        assert(preloader->m_FreelistSize < MAX_PRELOADER_REQUESTS);

        PreloadRequest* me = &preloader->m_Request[index];
        assert(me->m_FirstChild == -1);

        if (me->m_Resource)
        {
            if(me->m_Persist)
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
        ResourcePreloader* preloader = new ResourcePreloader();
        // root is always allocated.
        for (uint32_t i=0;i<MAX_PRELOADER_REQUESTS-1;i++)
            preloader->m_Freelist[i] = MAX_PRELOADER_REQUESTS-i-1;

        preloader->m_FreelistSize = MAX_PRELOADER_REQUESTS - 1;
        preloader->m_ScratchBufferPos = 0;

        preloader->m_Factory = factory;
        preloader->m_LoadQueue = dmLoadQueue::CreateQueue(factory);
        preloader->m_Mutex = dmMutex::New();

        preloader->m_PersistedResources.SetCapacity(names.Size());

        // Insert root.
        PreloadRequest* root = &preloader->m_Request[0];
        MakeNewRequest(root, factory, names[0], true);

        preloader->m_InProgress.SetCapacity(7, MAX_PRELOADER_REQUESTS);
        preloader->m_InProgress.Put(root->m_CanonicalPathHash, root);

        // Set up error condition
        Result r = CheckSuppliedResourcePath(names[0]);
        if (r != RESULT_OK)
        {
            root->m_LoadResult = r;
        }

        // Post create setup
        preloader->m_PostCreateCallbacks.SetSize(0);
        preloader->m_PostCreateCallbacks.SetCapacity(MAX_PRELOADER_REQUESTS);
        preloader->m_CreateComplete = false;
        preloader->m_PostCreateCallbackIndex = 0;

        // Add remaining items as children of root (first item).
        // This enables optimised loading in so that resources are not loaded multiple times and are released when they can be (internally pruning and sharing the request tree).
        for(uint32_t i = 1; i < names.Size(); ++i)
        {
            PreloadHintInternal(preloader, 0, names[i], true);
        }

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
        assert(preloader->m_InProgress.Get(req->m_CanonicalPathHash) != 0);
        preloader->m_InProgress.Erase(req->m_CanonicalPathHash);

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
                        preloader->m_PostCreateCallbacks.OffsetCapacity(MAX_PRELOADER_REQUESTS);
                    }
                    preloader->m_PostCreateCallbacks.SetSize(preloader->m_PostCreateCallbacks.Size()+1);
                    ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks.Back();
                    ip.m_Removed = false;
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


            // Remove from the post create callbacks if necessary
            for( uint32_t i = 0; i < preloader->m_PostCreateCallbacks.Size(); ++i )
            {
                ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[i];
                if (ip.m_ResourceDesc.m_Resource == tmp_resource.m_Resource)
                {
                    ip.m_Removed = true;
                    break;
                }
            }

            ResourceDestroyParams params;
            params.m_Factory = preloader->m_Factory;
            params.m_Context = resource_type->m_Context;
            params.m_Resource = &tmp_resource;
            resource_type->m_DestroyFunction(params);
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
                    assert(preloader->m_InProgress.Get(req->m_CanonicalPathHash) != 0);
                    preloader->m_InProgress.Erase(req->m_CanonicalPathHash);
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

    // Returns if anything completed
    static uint32_t PreloaderUpdateOneItem(HPreloader preloader, int32_t index)
    {
        if (index < 0)
        {
            // becuase we do recursion
            return 0;
        }

        DM_PROFILE(Resource, "PreloaderUpdateOneItem");
        PreloadRequest *req = &preloader->m_Request[index];

        if (req->m_LoadResult != RESULT_PENDING)
        {
            // Done means no children to worry about; go to sibling.
            return PreloaderUpdateOneItem(preloader, req->m_NextSibling);
        }

        // If it does not have a load request, it would have a buffer if it
        // had started a load.
        if (!req->m_LoadRequest && !req->m_Buffer && !req->m_Resource)
        {
            if (!req->m_CanonicalPathHash)
            {
                char canonical_path[RESOURCE_PATH_MAX];
                GetCanonicalPath(preloader->m_Factory, req->m_Path, canonical_path);
                req->m_CanonicalPathHash = dmHashBuffer64(canonical_path, strlen(canonical_path));
            }

            PreloadRequest** wait_on = preloader->m_InProgress.Get(req->m_CanonicalPathHash);
            if (wait_on)
            {
                // Already being loaded elsewhere; we can wait on that to complete, unless the item
                // exists above us in the tree; then the loop is infinite and we can abort
                int32_t parent = preloader->m_Request[index].m_Parent;
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
                    preloader->m_InProgress.Put(req->m_CanonicalPathHash, req);
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

        return PreloaderUpdateOneItem(preloader, req->m_NextSibling);
    }

    static Result PostCreateUpdateOneItem(HPreloader preloader, bool& empty_run)
    {
        empty_run = true;
        if (preloader->m_PostCreateCallbacks.Empty())
        {
            empty_run = false;
            return RESULT_OK;
        }
        Result ret = RESULT_OK;
        while(preloader->m_PostCreateCallbackIndex < preloader->m_PostCreateCallbacks.Size())
        {
            if(!preloader->m_PostCreateCallbacks[preloader->m_PostCreateCallbackIndex].m_Removed)
                break;
            ++preloader->m_PostCreateCallbackIndex;
        }
        if(preloader->m_PostCreateCallbackIndex == preloader->m_PostCreateCallbacks.Size())
        {
            empty_run = false;
            return RESULT_OK;
        }

        ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[preloader->m_PostCreateCallbackIndex];
        ResourcePostCreateParams& params = ip.m_Params;
        params.m_Resource = &ip.m_ResourceDesc;
        SResourceType *resource_type = (SResourceType*) params.m_Resource->m_ResourceType;
        if(resource_type->m_PostCreateFunction)
        {
            ret = resource_type->m_PostCreateFunction(params);
        }

        if (ret == RESULT_PENDING)
            return ret;
        ++preloader->m_PostCreateCallbackIndex;
        if (ret == RESULT_OK)
        {
            if(preloader->m_PostCreateCallbackIndex < preloader->m_PostCreateCallbacks.Size())
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

        dmMutex::Lock(preloader->m_Mutex);

        // depth first traversal
        Result ret = RESULT_PENDING;
        uint64_t start = dmTime::GetTime();
        uint32_t empty_runs = 0;
        bool empty_run;

        while (true)
        {
            Result result = PostCreateUpdateOneItem(preloader, empty_run);
            if(preloader->m_CreateComplete)
            {
                if(result != RESULT_PENDING)
                {
                    ret = result;
                    break;
                }
            }
            else
            {
                empty_run = !PreloaderUpdateOneItem(preloader, 0);
            }

            if (empty_run)
            {
                if (++empty_runs > 10)
                    break;

                // In case of non-threaded loading, we never get any empty runs really.
                // In case of threaded loading and loading small files, use up a little
                // more of our time waiting for files.
                dmMutex::Unlock(preloader->m_Mutex);
                dmTime::Sleep(1000);
                dmMutex::Lock(preloader->m_Mutex);
            }
            else
            {
                empty_runs = 0;
            }

            if (dmTime::GetTime() - start > soft_time_limit)
                break;
        }

        if(!preloader->m_CreateComplete)
        {
            // Check if root object is loaded, then everything is done.
            if (preloader->m_Request[0].m_LoadResult != RESULT_PENDING)
            {
                assert(preloader->m_Request[0].m_FirstChild == -1);
                ret = preloader->m_Request[0].m_LoadResult;

                if (ret == RESULT_OK && complete_callback)
                {
                    if(complete_callback)
                    {
                        if(complete_callback(complete_callback_params))
                        {
                            ret = RESULT_PENDING;
                        }
                        else
                        {
                            ret = RESULT_NOT_LOADED;
                        }
                    }
                }
                preloader->m_CreateComplete = true;

                if(ret != RESULT_PENDING)
                {
                    // flush out any already created resources
                    while(PostCreateUpdateOneItem(preloader, empty_run)==RESULT_PENDING)
                    {
                        if(empty_run)
                            dmTime::Sleep(250);
                    }
                }
            }
        }

        dmMutex::Unlock(preloader->m_Mutex);
        return ret;
    }

    void DeletePreloader(HPreloader preloader)
    {
        // Since Preload calls need their Create calls done and PostCreate calls must follow Create calls.
        // To fix this:
        // * Make Get calls insta-fail on RESULT_ABORTED or something
        // * Make Queue only return RESULT_ABORTED always.
        while (UpdatePreloader(preloader, 0, 0, 1000000) == RESULT_PENDING);

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
        assert(preloader->m_InProgress.Size() == 1);
        dmLoadQueue::DeleteQueue(preloader->m_LoadQueue);
        dmMutex::Delete(preloader->m_Mutex);
        delete preloader;
    }

    static bool PreloadHintInternal(HPreloader preloader, int32_t parent, const char* name, bool persisted)
    {

        // Ignore malformed paths that would fail anyway.
        if (CheckSuppliedResourcePath(name) != dmResource::RESULT_OK)
            return false;

        dmMutex::ScopedLock lk(preloader->m_Mutex);
        if (!preloader->m_FreelistSize)
        {
            // Preload queue is exhausted; this is not fatal.
            return false;
        }
        int32_t new_req = preloader->m_Freelist[--preloader->m_FreelistSize];
        PreloadRequest *req = &preloader->m_Request[new_req];
        MakeNewRequest(req, preloader->m_Factory, name, persisted);

        // Quick dedupe; we can do it safely on the same parent
        int32_t child = preloader->m_Request[parent].m_FirstChild;
        while (child != -1)
        {
            if (preloader->m_Request[child].m_PathHash == req->m_PathHash)
            {
                preloader->m_FreelistSize++;
                return true;
            }
            child = preloader->m_Request[child].m_NextSibling;
        }

        PreloaderTreeInsert(preloader, new_req, parent);
        return true;
    }

    // This function can be called from a different thread,
    // so this is where we also want the m_Mutex lock. No lock must be held
    // during this call.
    bool PreloadHint(HPreloadHintInfo info, const char* name)
    {
        if (!info || !name)
            return false;
        HPreloader preloader = info->m_Preloader;
        dmMutex::ScopedLock lk(preloader->m_Mutex);
        return PreloadHintInternal(preloader, info->m_Parent, name, false);
    }
}
