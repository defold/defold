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
#include <dlib/spinlock.h>

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
    // Nodes are scheduled for load in depth first order. Once they are loaded they might add new child items to the
    // tree. Child items to a node will then be loaded and created before the parent node is created.
    //
    // Once a node with children finds none of them are in PENDING state any longer, resource create will happen,
    // child nodes (which are done) are then erased and the tree is traversed upwards to see if the parent can be
    // completed in the same manner. (PreloaderTryPruneParent)
    //
    // New items added in PreloadHint are added to a guarded dmArray and the preloader pops this array after it
    // has detected a completion of an item in the preloader queue. This keeps the syncronized state small and
    // reduces the contention on the syncronized lock.
    //
    // A path cache is also used to allow the path strings to be internalized without adding the full path length
    // to each request item. The path cache is also syncronized with the same spinlock as the new preloader hints array.
    // The path cache is not touched by the UpdatePreloader code, we keep the internalized pointers in the item.

    // If max number of preload items is reached or the path cache is full new items added to the preloader will
    // be thrown away and can potentially cause synced loading of those resources.

    struct PathDescriptor
    {
        const char* m_InternalizedName;
        uint64_t m_NameHash;
        const char* m_InternalizedCanonicalPath;
        uint64_t m_CanonicalPathHash;
    };

    typedef int16_t TRequestIndex;

    struct PreloadRequest
    {
        PathDescriptor m_PathDescriptor;

        TRequestIndex m_Parent;
        TRequestIndex m_FirstChild;
        TRequestIndex m_NextSibling;
        TRequestIndex m_PendingChildCount;

        // Set once resources have started loading, they have a load request
        dmLoadQueue::HRequest m_LoadRequest;

        // Set for items that are pending and waiting for children to complete
        void* m_Buffer;
        uint32_t m_BufferSize;

        // Set once preload function has run
        SResourceType* m_ResourceType;
        void* m_PreloadData;

        // Set once load has completed
        Result m_LoadResult;
        void* m_Resource;
    };

    // Internal data structure for passing parameters to postcreate function callbacks
    struct ResourcePostCreateParamsInternal
    {
        ResourcePostCreateParams m_Params;
        SResourceDescriptor m_ResourceDesc;
        bool m_Destroy;
    };

    // A simple block allocator for scratch memory to use for small resource instead
    // of pressuring malloc/free.
    //
    // It allocates blocks (or chunks) of memory via malloc and then allocates from
    // each block.
    //
    // It keeps track of the high and low range used in a block to try and reuse free
    // space but no real defragmentation is done.
    //
    // One block is always allocated, but other blocks are dynamically allocated/freed
    //
    // There is no thread syncronization - one BlockAllocator is created per preloader/thread.
    //
    // If there is no space in the blocks or the allocation is deemed to big (BLOCK_ALLOCATION_THRESHOLD)
    // it will fall back to malloc/free for the allocation

    static const uint32_t BLOCK_SIZE                   = 16384;
    static const uint32_t BLOCK_ALLOCATION_THRESHOLD   = BLOCK_SIZE / 2;
    static const uint16_t MAX_BLOCK_COUNT              = 8;
    static const uint32_t BLOCK_ALLOCATION_ALIGNEMENT  = sizeof(uint16_t);

    struct BlockData
    {
        uint32_t m_AllocationCount;
        uint32_t m_LowWaterMark;
        uint32_t m_HighWaterMark;
    };

    struct Block
    {
        uint8_t m_Data[BLOCK_SIZE];
    };

    struct BlockAllocator
    {
        BlockAllocator()
        {
            m_BlockDatas[0].m_AllocationCount = 0;
            m_BlockDatas[0].m_LowWaterMark    = 0;
            m_BlockDatas[0].m_HighWaterMark   = 0;
            m_Blocks[0]                       = &m_InitialBlock;
            for (uint16_t i = 1; i < MAX_BLOCK_COUNT; ++i)
            {
                m_Blocks[i] = 0x0;
            }
        }
        ~BlockAllocator()
        {
            assert(m_BlockDatas[0].m_AllocationCount == 0);
            for (uint16_t i = 1; i < MAX_BLOCK_COUNT; ++i)
            {
                assert(m_Blocks[i] == 0x0);
            }
        }
        BlockData m_BlockDatas[MAX_BLOCK_COUNT];
        Block* m_Blocks[MAX_BLOCK_COUNT];
        Block m_InitialBlock;
    };

    void* Allocate(BlockAllocator* block_allocator, uint32_t size)
    {
        uint32_t allocation_size = DM_ALIGN(sizeof(uint16_t) + size, BLOCK_ALLOCATION_ALIGNEMENT);
        if (allocation_size > BLOCK_ALLOCATION_THRESHOLD)
        {
            uint16_t* res = (uint16_t*)malloc(sizeof(uint16_t) + size);
            *res          = MAX_BLOCK_COUNT;
            return &res[1];
        }
        uint16_t first_free = MAX_BLOCK_COUNT;
        for (uint16_t block_index = 0; block_index < MAX_BLOCK_COUNT; ++block_index)
        {
            Block* block = block_allocator->m_Blocks[block_index];
            if (block == 0x0)
            {
                first_free = (first_free == MAX_BLOCK_COUNT) ? block_index : first_free;
                continue;
            }
            BlockData* block_data = &block_allocator->m_BlockDatas[block_index];
            if (block_data->m_LowWaterMark >= allocation_size)
            {
                block_data->m_LowWaterMark -= allocation_size;
                block_data->m_AllocationCount++;
                uint16_t* ptr = (uint16_t*)&block->m_Data[block_data->m_LowWaterMark];
                *ptr          = block_index;
                return &ptr[1];
            }
            if (block_data->m_HighWaterMark + allocation_size <= BLOCK_SIZE)
            {
                block_data->m_AllocationCount++;
                uint16_t* ptr = (uint16_t*)&block->m_Data[block_data->m_HighWaterMark];
                block_data->m_HighWaterMark += allocation_size;
                *ptr = block_index;
                return &ptr[1];
            }
        }
        if (first_free != MAX_BLOCK_COUNT)
        {
            Block* block                          = new Block;
            BlockData* block_data                 = &block_allocator->m_BlockDatas[first_free];
            block_data->m_AllocationCount         = 1;
            block_data->m_LowWaterMark            = 0;
            block_data->m_HighWaterMark           = allocation_size;
            uint16_t* ptr                         = (uint16_t*)&block->m_Data[0];
            *ptr                                  = first_free;
            block_allocator->m_Blocks[first_free] = block;
            return &ptr[1];
        }
        dmLogWarning("Scratch buffer is full when trying to allocate: %u bytes", size);
        uint16_t* res = (uint16_t*)malloc(sizeof(uint16_t) + size);
        *res          = MAX_BLOCK_COUNT;
        return &res[1];
    }

    void Free(BlockAllocator* block_allocator, void* data, uint32_t size)
    {
        uint16_t* ptr = (uint16_t*)data;
        --ptr;
        uint16_t block_index = *ptr;
        if (block_index == MAX_BLOCK_COUNT)
        {
            free(ptr);
            return;
        }
        assert(block_index < MAX_BLOCK_COUNT);
        uint16_t allocation_size = DM_ALIGN(sizeof(uint16_t) + size, BLOCK_ALLOCATION_ALIGNEMENT);
        Block* block             = block_allocator->m_Blocks[block_index];
        assert(block != 0x0);
        BlockData* block_data = &block_allocator->m_BlockDatas[block_index];
        assert(block_data->m_AllocationCount > 0);

        block_data->m_AllocationCount--;
        if (0 == block_data->m_AllocationCount)
        {
            if (block_index != 0)
            {
                delete block;
                block_allocator->m_Blocks[block_index] = 0x0;
            }
            return;
        }
        if ((uint8_t*)ptr == &block->m_Data[block_data->m_LowWaterMark])
        {
            block_data->m_LowWaterMark += allocation_size;
        }
        else if ((uint8_t*)ptr == &block->m_Data[block_data->m_HighWaterMark - allocation_size])
        {
            block_data->m_HighWaterMark -= allocation_size;
        }
    }

    // The preloader will function even down to a value of 1 here (the root object)
    // and sets the limit of how large a dependencies tree can be stored. Since nodes
    // are always present with all their children inserted (unless there was not room)
    // the required size is something the sum of all children on each level down along
    // the largest branch.

    static const uint32_t MAX_PRELOADER_REQUESTS = 1024;

    typedef dmHashTable<uint64_t, uint32_t> TPathHashTable;
    typedef dmHashTable<uint64_t, bool> TPathInProgressTable;

    static const uint32_t PATH_IN_PROGRESS_TABLE_SIZE    = 313;
    static const uint32_t PATH_IN_PROGRESS_CAPACITY      = MAX_PRELOADER_REQUESTS;
    static const uint32_t PATH_IN_PROGRESS_HASHDATA_SIZE = (PATH_IN_PROGRESS_TABLE_SIZE * sizeof(uint32_t)) + (PATH_IN_PROGRESS_CAPACITY * sizeof(TPathInProgressTable::Entry));
    static const uint32_t PATH_AVERAGE_LENGTH            = 40;
    static const uint32_t MAX_PRELOADER_PATHS            = 1536;
    static const uint32_t PATH_BUFFER_TABLE_SIZE         = 509;
    static const uint32_t PATH_BUFFER_TABLE_CAPACITY     = MAX_PRELOADER_PATHS;
    static const uint32_t PATH_BUFFER_HASHDATA_SIZE      = (PATH_BUFFER_TABLE_SIZE * sizeof(uint32_t)) + (PATH_BUFFER_TABLE_CAPACITY * sizeof(TPathHashTable::Entry));

    struct PendingHint
    {
        PathDescriptor m_PathDescriptor;
        TRequestIndex m_Parent;
    };

    struct ResourcePreloader
    {
        ResourcePreloader()
            : m_InProgress(&m_PathInProgressData, PATH_IN_PROGRESS_TABLE_SIZE, PATH_IN_PROGRESS_CAPACITY)
        {
        }
        struct SyncedData
        {
            SyncedData()
                : m_PathLookup(m_LookupData, PATH_BUFFER_TABLE_SIZE, PATH_BUFFER_TABLE_CAPACITY)
                , m_PathDataUsed(0)
            {
            }
            dmArray<PendingHint> m_NewHints;
            TPathHashTable m_PathLookup;
            uint8_t m_LookupData[PATH_BUFFER_HASHDATA_SIZE];
            char m_PathData[MAX_PRELOADER_PATHS * PATH_AVERAGE_LENGTH];
            uint32_t m_PathDataUsed;
        } m_SyncedData;

        dmSpinlock::lock_t m_SyncedDataSpinlock;

        PreloadRequest m_Request[MAX_PRELOADER_REQUESTS];

        // list of free nodes
        TRequestIndex m_Freelist[MAX_PRELOADER_REQUESTS];
        uint32_t m_FreelistSize;
        dmLoadQueue::HQueue m_LoadQueue;
        HFactory m_Factory;
        TPathInProgressTable m_InProgress;
        uint8_t m_PathInProgressData[PATH_IN_PROGRESS_HASHDATA_SIZE];

        // used instead of dynamic allocs as far as it lasts.
        BlockAllocator m_BlockAllocator;

        // post create state
        bool m_LoadQueueFull;
        bool m_CreateComplete;
        uint32_t m_PostCreateCallbackIndex;
        dmArray<ResourcePostCreateParamsInternal> m_PostCreateCallbacks;

        // How many of the initial resources where requested - they should not be release until preloader destruction
        TRequestIndex m_PersistResourceCount;

        // persisted resources
        dmArray<void*> m_PersistedResources;

        uint64_t m_PreloaderCreationTimeNS;
        uint64_t m_MainThreadTimeSpentNS;
    };

    const char* InternalizePath(ResourcePreloader::SyncedData* preloader_synced_data, uint64_t path_hash, const char* path, uint32_t path_len)
    {
        uint32_t* path_lookup = preloader_synced_data->m_PathLookup.Get(path_hash);
        if (path_lookup != 0x0)
        {
            return &preloader_synced_data->m_PathData[*path_lookup];
        }
        if (preloader_synced_data->m_PathLookup.Full())
        {
            return 0x0;
        }
        if (preloader_synced_data->m_PathDataUsed + path_len + 1 > sizeof(preloader_synced_data->m_PathData))
        {
            return 0x0;
        }
        char* result = &preloader_synced_data->m_PathData[preloader_synced_data->m_PathDataUsed];
        dmStrlCpy(result, path, path_len + 1);
        preloader_synced_data->m_PathLookup.Put(path_hash, preloader_synced_data->m_PathDataUsed);
        preloader_synced_data->m_PathDataUsed += path_len + 1;
        return result;
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
            return RESULT_INVALID_DATA;
        }
        out_path_descriptor.m_NameHash = dmHashBuffer64(name, name_len);

        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(name, canonical_path);
        uint32_t canonical_path_len             = strlen(canonical_path);
        out_path_descriptor.m_CanonicalPathHash = dmHashBuffer64(canonical_path, canonical_path_len);

        dmSpinlock::Lock(&preloader->m_SyncedDataSpinlock);
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
        dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);
        return RESULT_OK;
    }

    static bool IsPathInProgress(ResourcePreloader* preloader, const PathDescriptor* path_descriptor)
    {
        uint64_t path_hash = path_descriptor->m_CanonicalPathHash;
        bool* path_lookup  = preloader->m_InProgress.Get(path_hash);
        return path_lookup != 0x0;
    }

    static void MarkPathInProgress(ResourcePreloader* preloader, const PathDescriptor* path_descriptor)
    {
        uint64_t path_hash = path_descriptor->m_CanonicalPathHash;
        assert(preloader->m_InProgress.Get(path_hash) == 0x0);
        preloader->m_InProgress.Put(path_hash, true);
    }

    static void UnmarkPathInProgress(ResourcePreloader* preloader, const PathDescriptor* path_descriptor)
    {
        uint64_t path_hash = path_descriptor->m_CanonicalPathHash;
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

    static SResourceType* GetResourceType(HPreloader preloader, const PathDescriptor& path_descriptor, Result* out_load_result)
    {
        const char* ext = strrchr(path_descriptor.m_InternalizedName, '.');
        if (!ext)
        {
            dmLogWarning("Unable to load resource: '%s'. Missing file extension.", path_descriptor.m_InternalizedName);
            *out_load_result = RESULT_MISSING_FILE_EXTENSION;
            return 0;
        }
        else
        {
            SResourceType* resource_type = FindResourceType(preloader->m_Factory, ext + 1);
            if (resource_type)
            {
                return resource_type;
            }
            dmLogError("Unable to load resource: '%s'. Unknown resource type: %s", path_descriptor.m_InternalizedName, ext);
            *out_load_result = RESULT_UNKNOWN_RESOURCE_TYPE;
        }
        return 0;
    }

    static Result PreloadPathDescriptor(HPreloader preloader, TRequestIndex parent, const PathDescriptor& path_descriptor)
    {
        if (!preloader->m_FreelistSize)
        {
            // Preload queue is exhausted; this is not fatal.
            return RESULT_OUT_OF_MEMORY;
        }

        // Quick dedupe; we can do it safely on the same parent
        TRequestIndex child = preloader->m_Request[parent].m_FirstChild;
        while (child != -1)
        {
            if (preloader->m_Request[child].m_PathDescriptor.m_NameHash == path_descriptor.m_NameHash)
            {
                return RESULT_ALREADY_REGISTERED;
            }
            child = preloader->m_Request[child].m_NextSibling;
        }

        TRequestIndex new_req = preloader->m_Freelist[--preloader->m_FreelistSize];
        PreloadRequest* req   = &preloader->m_Request[new_req];
        memset(req, 0, sizeof(PreloadRequest));
        req->m_PathDescriptor    = path_descriptor;
        req->m_Parent            = -1;
        req->m_FirstChild        = -1;
        req->m_NextSibling       = -1;
        req->m_PendingChildCount = 0;
        req->m_LoadResult        = RESULT_PENDING;

        PreloaderTreeInsert(preloader, new_req, parent);

        req->m_ResourceType = GetResourceType(preloader, path_descriptor, &req->m_LoadResult);
        if (!req->m_ResourceType)
        {
            RemoveFromParentPendingCount(preloader, req);
        }

        if (req->m_LoadResult == RESULT_PENDING)
        {
            // Check for recursive resources, if it is, mark with loop error
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
        }
        return RESULT_OK;
    }

    static bool PopHints(HPreloader preloader)
    {
        uint32_t new_hint_count = 0;
        dmArray<PendingHint> new_hints;
        {
            dmSpinlock::Lock(&preloader->m_SyncedDataSpinlock);
            new_hints.Swap(preloader->m_SyncedData.m_NewHints);
            dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);
        }
        for (uint32_t i = 0; i < new_hints.Size(); ++i)
        {
            PendingHint* hint = &new_hints[i];
            uint32_t j        = i + 1;
            while (j < new_hints.Size())
            {
                PendingHint* test = &new_hints[j];
                if (test->m_PathDescriptor.m_NameHash == hint->m_PathDescriptor.m_NameHash &&
                    test->m_Parent == hint->m_Parent)
                {
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
            if (PreloadPathDescriptor(preloader, hint->m_Parent, hint->m_PathDescriptor) == RESULT_OK)
            {
                ++new_hint_count;
            }
        }
        return new_hint_count != 0;
    }

    static Result PreloadHintInternal(HPreloader preloader, TRequestIndex parent, const char* name)
    {
        PathDescriptor path_descriptor;
        Result res = MakePathDescriptor(preloader, name, path_descriptor);
        if (res != RESULT_OK)
        {
            return res;
        }
        res = PreloadPathDescriptor(preloader, parent, path_descriptor);
        return res;
    }

    // Only supports removing the first child, which is all the preloader uses anyway.
    static void PreloaderRemoveLeaf(ResourcePreloader* preloader, TRequestIndex index)
    {
        assert(preloader->m_FreelistSize < MAX_PRELOADER_REQUESTS);

        PreloadRequest* me = &preloader->m_Request[index];
        assert(me->m_FirstChild == -1);
        assert(me->m_PendingChildCount == 0);

        if (me->m_Resource)
        {
            if (index < preloader->m_PersistResourceCount)
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

        if (me->m_LoadResult == RESULT_PENDING)
        {
            RemoveFromParentPendingCount(preloader, me);
        }

        preloader->m_Freelist[preloader->m_FreelistSize++] = index;
    }

    static void RemoveChildren(ResourcePreloader* preloader, PreloadRequest* req)
    {
        while (req->m_FirstChild != -1)
        {
            PreloaderRemoveLeaf(preloader, req->m_FirstChild);
        }
        assert(req->m_PendingChildCount == 0);
    }

    HPreloader NewPreloader(HFactory factory, const dmArray<const char*>& names)
    {
        uint64_t start_ns = dmTime::GetTime();

        ResourcePreloader* preloader = new ResourcePreloader();
        // root is always allocated.
        for (uint32_t i = 0; i < MAX_PRELOADER_REQUESTS - 1; i++)
        {
            preloader->m_Freelist[i] = MAX_PRELOADER_REQUESTS - i - 1;
        }

        preloader->m_FreelistSize = MAX_PRELOADER_REQUESTS - 1;

        preloader->m_Factory         = factory;
        preloader->m_LoadQueue       = dmLoadQueue::CreateQueue(factory);
        dmSpinlock::Init(&preloader->m_SyncedDataSpinlock);

        preloader->m_PersistResourceCount = 0;
        preloader->m_PersistedResources.SetCapacity(names.Size());

        // Insert root.
        PreloadRequest* root = &preloader->m_Request[0];
        memset(root, 0x00, sizeof(PreloadRequest));

        root->m_LoadResult        = MakePathDescriptor(preloader, names[0], root->m_PathDescriptor);
        root->m_Parent            = -1;
        root->m_FirstChild        = -1;
        root->m_NextSibling       = -1;
        root->m_PendingChildCount = 0;
        preloader->m_PersistResourceCount++;

        if (root->m_LoadResult == RESULT_OK)
        {
            root->m_ResourceType = GetResourceType(preloader, root->m_PathDescriptor, &root->m_LoadResult);
        }

        // Post create setup
        preloader->m_PostCreateCallbacks.SetCapacity(MAX_PRELOADER_REQUESTS / 8);
        preloader->m_LoadQueueFull           = false;
        preloader->m_CreateComplete          = false;
        preloader->m_PostCreateCallbackIndex = 0;

        if (root->m_LoadResult == RESULT_OK)
        {
            root->m_LoadResult = RESULT_PENDING;
        }

        // Add remaining items as children of root (first item).
        // This enables optimised loading in so that resources are not loaded multiple times and are released when they can be (internally pruning and sharing the request tree).
        for (uint32_t i = 1; i < names.Size(); ++i)
        {
            if (strlen(names[i]) >= RESOURCE_PATH_MAX)
            {
                dmLogWarning("Passed too long path into NewPreloader: \"%s\"", names[i]);
                continue;
            }
            Result res = PreloadHintInternal(preloader, 0, names[i]);
            if (res == RESULT_OK)
            {
                preloader->m_PersistResourceCount++;
            }
        }

        uint64_t now_ns                      = dmTime::GetTime();
        uint64_t main_thread_time_ns         = now_ns - start_ns;
        preloader->m_PreloaderCreationTimeNS = start_ns;
        preloader->m_MainThreadTimeSpentNS   = main_thread_time_ns;

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
    // If buffer passed in is null it means to use the items internal buffer
    static void CreateResource(HPreloader preloader, PreloadRequest* req, void* buffer, uint32_t buffer_size)
    {
        assert(req->m_LoadResult == RESULT_PENDING);
        assert(req->m_PendingChildCount == 0);

        assert(req->m_ResourceType);

        SResourceDescriptor tmp_resource;
        memset(&tmp_resource, 0, sizeof(tmp_resource));

        SResourceType* resource_type = req->m_ResourceType;

        // obliged to call CreateFunction if Preload has been called, so always do this even when an error has occured
        tmp_resource.m_NameHash       = req->m_PathDescriptor.m_CanonicalPathHash;
        tmp_resource.m_ReferenceCount = 1;
        tmp_resource.m_ResourceType   = (void*)resource_type;

        ResourceCreateParams params;
        params.m_Factory     = preloader->m_Factory;
        params.m_Context     = resource_type->m_Context;
        params.m_PreloadData = req->m_PreloadData;
        params.m_Resource    = &tmp_resource;
        params.m_Filename    = req->m_PathDescriptor.m_InternalizedName;

        if (!buffer)
        {
            assert(req->m_Buffer);
            tmp_resource.m_ResourceSizeOnDisc = req->m_BufferSize;
            params.m_Buffer                   = req->m_Buffer;
            params.m_BufferSize               = req->m_BufferSize;
            req->m_LoadResult                 = resource_type->m_CreateFunction(params);

            Free(&preloader->m_BlockAllocator, req->m_Buffer, req->m_BufferSize);

            req->m_Buffer = 0;
        }
        else
        {
            tmp_resource.m_ResourceSizeOnDisc = buffer_size;
            params.m_Buffer                   = buffer;
            params.m_BufferSize               = buffer_size;
            req->m_LoadResult                 = resource_type->m_CreateFunction(params);
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
                ip.m_Params.m_Context                = resource_type->m_Context;
                ip.m_Params.m_PreloadData            = req->m_PreloadData;
                ip.m_Params.m_Resource               = 0;
                memcpy(&ip.m_ResourceDesc, &tmp_resource, sizeof(SResourceDescriptor));
            }
        }

        assert(req->m_Buffer == 0);
        req->m_PreloadData = 0;

        // This is set if precreate was run. Once past this step it is not going to be needed;
        // and we clear it to indicate loading is done.
        req->m_ResourceType = 0;
        RemoveFromParentPendingCount(preloader, req);

        // Children can now be removed (and their resources released) as they are not
        // needed any longer.
        RemoveChildren(preloader, req);

        if (req->m_LoadResult != RESULT_OK)
        {
            // Continue up to parent. All the stuff below is only for resources that were created
            return;
        }

        assert(tmp_resource.m_Resource);

        // Only two options from now on is to either destroy the resource or have it inserted
        bool destroy = false;

        // second need to destroy if already exists
        SResourceDescriptor* rd = GetByHash(preloader->m_Factory, req->m_PathDescriptor.m_CanonicalPathHash);
        if (rd)
        {
            // use already loaded then.
            rd->m_ReferenceCount++;
            req->m_Resource = rd->m_Resource;
            destroy         = true;
        }
        else
        {
            // insert the already loaded, can fail here
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
                        // Mark resource that is should be destroyed once postcreate has run.
                        ip.m_Destroy = true;
                        found        = true;

                        // NOTE: Loading multiple collection proxies with shared resources at the same
                        // time can result in these resources being loading more than once.
                        // We should fix so that this cannot happen since this is wasteful of both performance
                        // and memory usage. There is an issue filed for this: DEF-3088
                        // Once that has been implemented, the delayed destroy (m_Destroy) can be removed,
                        // it would no longer be needed since each resource would only be created/loaded once.
                        // Measurements from the amound of double-loading happening due to this in real games
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
                params.m_Context  = resource_type->m_Context;
                params.m_Resource = &tmp_resource;
                resource_type->m_DestroyFunction(params);
            }
        }

        req->m_ResourceType = 0;
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
        CreateResource(preloader, parent_req, 0, 0);
        UnmarkPathInProgress(preloader, &parent_req->m_PathDescriptor);
        PreloaderTryPruneParent(preloader, parent_req);
        return true;
    }

    // Ends the Load part of the resource and handles the result of the load
    // It will create the resource if it has no children, otherwise it will
    // copy the loaded buffer for later use.
    //
    // Returns true if the resource was created
    static bool FinishLoad(HPreloader preloader, PreloadRequest* req, dmLoadQueue::LoadResult& load_result, void* buffer, uint32_t buffer_size)
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

        // If no children, do the create step immediately with the buffer in place
        if (req->m_FirstChild == -1)
        {
            if (req->m_LoadResult == RESULT_PENDING)
            {
                // Create the resource using the loading buffer directly.
                CreateResource(preloader, req, buffer, buffer_size);
                created_resource = true;
            }
            UnmarkPathInProgress(preloader, &req->m_PathDescriptor);
            dmLoadQueue::FreeLoad(preloader->m_LoadQueue, req->m_LoadRequest);
            req->m_LoadRequest = 0;

            PreloaderTryPruneParent(preloader, req);
        }
        else
        {
            // Keep the loaded bytes until we have loaded all children
            req->m_Buffer = Allocate(&preloader->m_BlockAllocator, buffer_size);
            memcpy(req->m_Buffer, buffer, buffer_size);
            req->m_BufferSize = buffer_size;
            dmLoadQueue::FreeLoad(preloader->m_LoadQueue, req->m_LoadRequest);
            req->m_LoadRequest = 0;
        }
        return created_resource;
    }

    static bool DoPreloaderUpdateOneReq(HPreloader preloader, TRequestIndex index, PreloadRequest* req);

    static bool PreloaderUpdateOneItem(HPreloader preloader, TRequestIndex index)
    {
        DM_PROFILE(Resource, "PreloaderUpdateOneItem");
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
                case RESULT_MISSING_FILE_EXTENSION:
                case RESULT_UNKNOWN_RESOURCE_TYPE:
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
        DM_PROFILE(Resource, "DoPreloaderUpdateOneReq");

        assert(!req->m_Resource);

        // If loading it must finish first before trying to go down to children
        if (req->m_LoadRequest)
        {
            void* buffer;
            uint32_t buffer_size;

            // Can hold the buffer till we FreeLoad it
            dmLoadQueue::LoadResult res;
            dmLoadQueue::Result e = dmLoadQueue::EndLoad(preloader->m_LoadQueue, req->m_LoadRequest, &buffer, &buffer_size, &res);
            if (e == dmLoadQueue::RESULT_PENDING)
            {
                return false;
            }
            preloader->m_LoadQueueFull = false;

            if (FinishLoad(preloader, req, res, buffer, buffer_size))
            {
                return true;
            }
            return false;
        }

        // If has a buffer if is waiting for children to complete first
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
        SResourceDescriptor* rd = GetByHash(preloader->m_Factory, req->m_PathDescriptor.m_CanonicalPathHash);
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

        assert(req->m_ResourceType);

        dmLoadQueue::PreloadInfo info;
        info.m_HintInfo.m_Preloader = preloader;
        info.m_HintInfo.m_Parent    = index;
        info.m_Function             = req->m_ResourceType->m_PreloadFunction;
        info.m_Context              = req->m_ResourceType->m_Context;

        // Silently ignore if return null (means try later)"
        if ((req->m_LoadRequest = dmLoadQueue::BeginLoad(preloader->m_LoadQueue, req->m_PathDescriptor.m_InternalizedName, req->m_PathDescriptor.m_InternalizedCanonicalPath, &info)))
        {
            MarkPathInProgress(preloader, &req->m_PathDescriptor);
            return true;
        }

        preloader->m_LoadQueueFull = true;
        return false;
    }

    static Result PostCreateUpdateOneItem(HPreloader preloader)
    {
        ResourcePostCreateParamsInternal& ip = preloader->m_PostCreateCallbacks[preloader->m_PostCreateCallbackIndex];
        ResourcePostCreateParams& params     = ip.m_Params;
        params.m_Resource                    = &ip.m_ResourceDesc;
        SResourceType* resource_type         = (SResourceType*)params.m_Resource->m_ResourceType;
        Result ret                           = resource_type->m_PostCreateFunction(params);

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
            params.m_Context  = resource_type->m_Context;
            params.m_Resource = &ip.m_ResourceDesc;
            resource_type->m_DestroyFunction(params);
            ip.m_Destroy = false;
        }

        if (preloader->m_PostCreateCallbackIndex == preloader->m_PostCreateCallbacks.Size())
        {
            preloader->m_PostCreateCallbacks.SetSize(0);
            preloader->m_PostCreateCallbackIndex = 0;
        }

        return ret;
    }

    Result UpdatePreloader(HPreloader preloader, FPreloaderCompleteCallback complete_callback, PreloaderCompleteCallbackParams* complete_callback_params, uint32_t soft_time_limit)
    {
        DM_PROFILE(Resource, "UpdatePreloader");

        uint64_t start           = dmTime::GetTime();
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
                        // if main result is RESULT_OK pick up any errors from
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
                    uint64_t main_thread_elapsed_ns = dmTime::GetTime() - start;
                    preloader->m_MainThreadTimeSpentNS += main_thread_elapsed_ns;
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
                close_to_time_limit = (dmTime::GetTime() + 1000 - start) > soft_time_limit;
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
        } while (dmTime::GetTime() - start <= soft_time_limit);

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
        // This is not a super-important use-case, the only way to trigger this is to start a load and
        // then do unload before it completes or if you destroy the collection while loading.
        // The normal operation is to issue a load and progress once complete.
        while (UpdatePreloader(preloader, 0, 0, 1000000) == RESULT_PENDING)
        {
            dmLogWarning("Waiting for preloader to complete.");
        }

        uint64_t start_excluding_update_ns = dmTime::GetTime();

        // Release root and persisted resources
        preloader->m_PersistedResources.Push(preloader->m_Request[0].m_Resource);
        for (uint32_t i = 0; i < preloader->m_PersistedResources.Size(); ++i)
        {
            void* resource = preloader->m_PersistedResources[i];
            if (!resource)
                continue;
            Release(preloader->m_Factory, resource);
        }

        assert(preloader->m_FreelistSize == (MAX_PRELOADER_REQUESTS - 1));
        dmLoadQueue::DeleteQueue(preloader->m_LoadQueue);

        uint64_t now_ns                 = dmTime::GetTime();
        uint64_t preloader_load_time_ns = now_ns - preloader->m_PreloaderCreationTimeNS;
        uint64_t main_thread_time_ns    = now_ns - start_excluding_update_ns;
        preloader->m_MainThreadTimeSpentNS += main_thread_time_ns;

        dmLogWarning("\"%s\", %u, %u",
                     preloader->m_Request[0].m_PathDescriptor.m_InternalizedName,
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

        Result res = CheckSuppliedResourcePath(name);
        if (res != dmResource::RESULT_OK)
        {
            dmLogWarning("Invalid path into dmResource::PreloadHint: \"%s\"", name);
            return res;
        }
        uint32_t name_len = strlen(name);
        if (name_len >= RESOURCE_PATH_MAX)
        {
            dmLogWarning("Passed too long path into dmResource::PreloadHint: \"%s\"", name);
            return RESULT_INVALID_DATA;
        }

        PathDescriptor path_descriptor;

        path_descriptor.m_NameHash = dmHashBuffer64(name, name_len);

        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(name, canonical_path);
        uint32_t canonical_path_len         = strlen(canonical_path);
        path_descriptor.m_CanonicalPathHash = dmHashBuffer64(canonical_path, canonical_path_len);

        HPreloader preloader = info->m_Preloader;

        dmSpinlock::Lock(&preloader->m_SyncedDataSpinlock);
        path_descriptor.m_InternalizedName = InternalizePath(&preloader->m_SyncedData, path_descriptor.m_NameHash, name, name_len);
        if (path_descriptor.m_InternalizedName == 0x0)
        {
            dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);
            return false;
        }
        path_descriptor.m_InternalizedCanonicalPath = InternalizePath(&preloader->m_SyncedData, path_descriptor.m_CanonicalPathHash, canonical_path, canonical_path_len);
        if (path_descriptor.m_InternalizedCanonicalPath == 0x0)
        {
            dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);
            return false;
        }

        uint32_t new_hints_size = preloader->m_SyncedData.m_NewHints.Size();
        if (preloader->m_SyncedData.m_NewHints.Capacity() == new_hints_size)
        {
            preloader->m_SyncedData.m_NewHints.OffsetCapacity(32);
        }
        preloader->m_SyncedData.m_NewHints.SetSize(new_hints_size + 1);
        PendingHint& hint     = preloader->m_SyncedData.m_NewHints.Back();
        hint.m_PathDescriptor = path_descriptor;
        hint.m_Parent         = info->m_Parent;

        dmSpinlock::Unlock(&preloader->m_SyncedDataSpinlock);

        return true;
    }
} // namespace dmResource
