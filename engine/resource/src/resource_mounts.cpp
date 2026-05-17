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

#include "resource_mounts.h"
#include "resource_manifest.h"
#include "resource_private.h" // for log
#include "providers/provider.h"
#include <resource/liveupdate_ddf.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/sys.h>
#include <stdlib.h> // qsort

namespace dmResourceMounts
{

struct ArchiveMount
{
    dmhash_t                        m_NameHash;
    dmResourceProvider::HArchive    m_Archive;
    int                             m_Priority;
};

struct CustomFile
{
    const void* m_Resource;
    uint32_t    m_Size;
};

struct ResourceMountsContext
{
    // The currently mounted archives, in sorted order
    dmArray<ArchiveMount>           m_Mounts;
    dmHashTable64<CustomFile>       m_CustomFiles;
    dmResourceProvider::HArchive    m_ResourceBaseArchive;
    dmMutex::HMutex                 m_Mutex;

};


static dmResource::Result ProviderResultToResult(dmResourceProvider::Result result)
{
    switch(result)
    {
    case dmResourceProvider::RESULT_OK:         return dmResource::RESULT_OK;
    case dmResourceProvider::RESULT_IO_ERROR:   return dmResource::RESULT_IO_ERROR;
    case dmResourceProvider::RESULT_NOT_FOUND:  return dmResource::RESULT_RESOURCE_NOT_FOUND;
    default:                                    return dmResource::RESULT_UNKNOWN_ERROR;
    }
}

static dmResource::Result DestroyMounts(HContext ctx);

HContext Create(dmResourceProvider::HArchive base_archive)
{
    ResourceMountsContext* ctx = new ResourceMountsContext;
    ctx->m_Mounts.SetCapacity(2);
    ctx->m_Mutex = dmMutex::New();
    ctx->m_ResourceBaseArchive = base_archive;
    return ctx;
}

void Destroy(HContext ctx)
{
    {
        DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
        DestroyMounts(ctx);

        ctx->m_CustomFiles.Clear();
    }
    dmMutex::Delete(ctx->m_Mutex);
    delete ctx;
}

static int MountSortCompare(const void* a, const void* b)
{
    const ArchiveMount* mount_a = (const ArchiveMount*) a;
    const ArchiveMount* mount_b = (const ArchiveMount*) b;
    if (mount_a->m_Priority != mount_b->m_Priority)
    {
        return mount_a->m_Priority > mount_b->m_Priority ? -1 : 1; // largest priority first
    }
    return 0;
}

static void SortMounts(dmArray<ArchiveMount>& mounts)
{
    qsort(mounts.Begin(), mounts.Size(), sizeof(ArchiveMount), MountSortCompare);
}

static void AddMountInternal(HContext ctx, const ArchiveMount& mount)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    if (ctx->m_Mounts.Full())
        ctx->m_Mounts.OffsetCapacity(2);

    ctx->m_Mounts.Push(mount);
    SortMounts(ctx->m_Mounts);

    DM_RESOURCE_DBG_LOG(1, "Added archive %p with prio %d\n", mount.m_Archive, mount.m_Priority);
}

static void DebugPrintMount(int level, const ArchiveMount& mount)
{
    dmURI::Parts uri;
    dmResourceProvider::GetUri(mount.m_Archive, &uri);
    DM_RESOURCE_DBG_LOG(level, "Mount:  prio: %3d  %s  p: %p  uri: %s:%s/%s\n", mount.m_Priority, dmHashReverseSafe64(mount.m_NameHash), mount.m_Archive, uri.m_Scheme, uri.m_Location, uri.m_Path);
}

dmMutex::HMutex GetMutex(HContext ctx)
{
    return ctx->m_Mutex;
}

dmResource::Result AddMount(HContext ctx, dmhash_t name_hash, dmResourceProvider::HArchive archive, int priority)
{
    SGetMountResult mount_info;
    if (dmResource::RESULT_OK == GetMountByNameHash(ctx, name_hash, &mount_info))
    {
        dmLogError("Mount with name already exists: '%s'", dmHashReverseSafe64(name_hash));
        return dmResource::RESULT_INVAL;
    }

    ArchiveMount mount;
    mount.m_NameHash = name_hash;
    mount.m_Priority = priority;
    mount.m_Archive  = archive;
    AddMountInternal(ctx, mount);
    return dmResource::RESULT_OK;
}

// Assumes mutex lock is held
static dmResource::Result RemoveMountByIndexInternal(HContext ctx, uint32_t index)
{
    if (index >= ctx->m_Mounts.Size())
        return dmResource::RESULT_RESOURCE_NOT_FOUND;

    ctx->m_Mounts.EraseSwap(index); // TODO: We'd like an Erase() function in dmArray, to keep the internal ordering
    SortMounts(ctx->m_Mounts);

    DM_RESOURCE_DBG_LOG(1, "Removed archive index %d\n", index);
    return dmResource::RESULT_OK;
}

dmResource::Result RemoveMount(HContext ctx, dmResourceProvider::HArchive archive)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (mount.m_Archive == archive)
        {
            return RemoveMountByIndexInternal(ctx, i);
        }
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result RemoveAndUnmountByNameHash(HContext ctx, dmhash_t name_hash)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (mount.m_NameHash == name_hash)
        {
            dmResourceProvider::Unmount(mount.m_Archive);
            return RemoveMountByIndexInternal(ctx, i);
        }
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

uint32_t GetNumMounts(HContext ctx)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
    return ctx->m_Mounts.Size();
}

dmResource::Result GetMountByIndex(HContext ctx, uint32_t index, SGetMountResult* mount_info)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
    if (index >= ctx->m_Mounts.Size())
        return dmResource::RESULT_INVAL;

    ArchiveMount& mount = ctx->m_Mounts[index];
    mount_info->m_NameHash = mount.m_NameHash;
    mount_info->m_Archive = mount.m_Archive;
    mount_info->m_Priority = mount.m_Priority;
    return dmResource::RESULT_OK;
}

dmResource::Result GetMountByNameHash(HContext ctx, dmhash_t name_hash, SGetMountResult* mount_info)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (mount.m_NameHash == name_hash)
        {
            mount_info->m_NameHash = mount.m_NameHash;
            mount_info->m_Archive = mount.m_Archive;
            mount_info->m_Priority = mount.m_Priority;
            return dmResource::RESULT_OK;
        }
    }
    return dmResource::RESULT_INVAL;
}

static dmResource::Result DestroyMounts(HContext ctx)
{
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        dmResourceProvider::Unmount(mount.m_Archive);
    }
    ctx->m_Mounts.SetSize(0);
    return dmResource::RESULT_OK;
}

static dmResource::Result GetCustomResourceSize(HContext ctx, dmhash_t path_hash, const char* path, uint32_t* resource_size)
{
    // Lock should already be held
    CustomFile* file = ctx->m_CustomFiles.Get(path_hash);
    if (file)
    {
        *resource_size = file->m_Size;
        DM_RESOURCE_DBG_LOG(3, "GetResourceSize OK: %s  " DM_HASH_FMT " (%u bytes) (custom file)\n", path, path_hash, *resource_size);
        return dmResource::RESULT_OK;
    }

    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

static dmResource::Result ReadCustomResource(HContext ctx, dmhash_t path_hash, uint8_t* buffer, uint32_t buffer_size)
{
    // Lock should already be held
    CustomFile* file = ctx->m_CustomFiles.Get(path_hash);
    if (file)
    {
        if (file->m_Size > buffer_size)
            return dmResource::RESULT_INVAL; // Shouldn't really happen as we just queried the size

        memcpy(buffer, file->m_Resource, buffer_size);

        DM_RESOURCE_DBG_LOG(3, "ReadResource OK: %s  " DM_HASH_FMT " (%u bytes) (custom file)\n", dmHashReverseSafe64(path_hash), path_hash, buffer_size);
        return dmResource::RESULT_OK;
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

static dmResource::Result ReadCustomResourcePartial(HContext ctx, dmhash_t path_hash, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
{
    // Lock should already be held
    CustomFile* file = ctx->m_CustomFiles.Get(path_hash);
    if (file)
    {
        if ((offset+size) > file->m_Size)
        {
            offset = dmMath::Min(offset, file->m_Size);
            size = file->m_Size - offset;
        }

        memcpy(buffer, ((uint8_t*)file->m_Resource) + offset, size);
        *nread = size;

        DM_RESOURCE_DBG_LOG(3, "ReadResource OK: %s  " DM_HASH_FMT " (offset: %u  size: %u) (custom file)\n", dmHashReverseSafe64(path_hash), path_hash, offset, size);
        return dmResource::RESULT_OK;
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result GetResourceSize(HContext ctx, dmhash_t path_hash, const char* path, uint32_t* resource_size)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        dmResourceProvider::Result result = dmResourceProvider::GetFileSize(mount.m_Archive, path_hash, path, resource_size);
        if (dmResourceProvider::RESULT_NOT_FOUND == result)
            continue;
        if (dmResourceProvider::RESULT_OK == result)
        {
            DM_RESOURCE_DBG_LOG(3, "GetResourceSize OK: %s  " DM_HASH_FMT " (%u bytes) - mount %s\n", path, path_hash, *resource_size, dmHashReverseSafe64(mount.m_NameHash));
            DebugPrintMount(3, mount);
            return dmResource::RESULT_OK;
        }
        return ProviderResultToResult(result);
    }

    if (!ctx->m_CustomFiles.Empty())
        return GetCustomResourceSize(ctx, path_hash, path, resource_size);

    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result ResourceExists(HContext ctx, dmhash_t path_hash)
{
    uint32_t resource_size;
    return GetResourceSize(ctx, path_hash, 0, &resource_size);
}

dmResource::Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_size)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        dmResourceProvider::Result result = dmResourceProvider::ReadFile(mount.m_Archive, path_hash, path, buffer, buffer_size);
        if (dmResourceProvider::RESULT_NOT_FOUND == result)
            continue;
        if (dmResourceProvider::RESULT_OK == result)
        {
            DM_RESOURCE_DBG_LOG(3, "%s: %s (%u bytes) - mount %s\n", __FUNCTION__, path, buffer_size, dmHashReverseSafe64(mount.m_NameHash));
            DebugPrintMount(3, mount);
            return dmResource::RESULT_OK;
        }
        return ProviderResultToResult(result);
    }

    if (!ctx->m_CustomFiles.Empty())
        return ReadCustomResource(ctx, path_hash, buffer, buffer_size);

    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result ReadResourcePartial(HContext ctx, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t num_mounts = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < num_mounts; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        dmResourceProvider::Result result = dmResourceProvider::ReadFilePartial(mount.m_Archive, path_hash, path, offset, size, buffer, nread);
        if (dmResourceProvider::RESULT_NOT_FOUND == result)
            continue;
        if (dmResourceProvider::RESULT_OK == result)
        {
            DM_RESOURCE_DBG_LOG(3, "%s: %s (offset: %u  size: %u)\n", __FUNCTION__, path, offset, size);
            DebugPrintMount(3, mount);
            return dmResource::RESULT_OK;
        }
        return ProviderResultToResult(result);
    }

    if (!ctx->m_CustomFiles.Empty())
        return ReadCustomResourcePartial(ctx, path_hash, offset, size, buffer, nread);

    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

// ****************************************
// Custom files

dmResource::Result AddFile(HContext context, dmhash_t path_hash, uint32_t size, const void* resource)
{
    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);

    CustomFile* prevfile = context->m_CustomFiles.Get(path_hash);
    if (prevfile)
        return dmResource::RESULT_ALREADY_REGISTERED;

    CustomFile file;
    file.m_Size = size;
    file.m_Resource = resource;
    if (context->m_CustomFiles.Full())
    {
        uint32_t capacity = context->m_CustomFiles.Capacity() + 8;
        context->m_CustomFiles.SetCapacity((capacity*2)/3, capacity);
    }
    context->m_CustomFiles.Put(path_hash, file);
    return dmResource::RESULT_OK;
}

dmResource::Result RemoveFile(HContext context, dmhash_t path_hash)
{
    DM_MUTEX_SCOPED_LOCK(context->m_Mutex);
    CustomFile* file = context->m_CustomFiles.Get(path_hash);
    if (!file)
        return dmResource::RESULT_RESOURCE_NOT_FOUND;

    context->m_CustomFiles.Erase(path_hash);
    return dmResource::RESULT_OK;
}

// ****************************************


struct DependencyIterContext
{
    dmHashTable64<bool> m_Visited;
    HContext            m_Mounts;
    FGetDependency      m_UserCallback;
    void*               m_UserCallbackCtx;
    bool                m_OnlyMissing;
    bool                m_Resursive;
    bool                m_IncludeRequestedUrl;
};

// Returns true if existed.
// Returns false if it didn't exist. Also adds to the set
static bool UpdateVisited(dmHashTable64<bool>& t, dmhash_t url_hash, bool exists)
{
    bool* visited = t.Get(url_hash);
    if (visited)
    {
        *visited = exists;
        return true;
    }

    if (t.Full())
    {
        uint32_t capacity = t.Capacity();
        capacity += 32;
        uint32_t table_size = (capacity*2)/3;
        t.SetCapacity(table_size, capacity);
    }
    t.Put(url_hash, exists);
    return exists; // it didn't exist
}

static dmResource::Result GetDependenciesInternal(DependencyIterContext* ctx, const dmhash_t url_hash)
{
    ResourceMountsContext* mounts_ctx = (ResourceMountsContext*)ctx->m_Mounts;
    DM_MUTEX_SCOPED_LOCK(mounts_ctx->m_Mutex);

    uint32_t num_mounts = mounts_ctx->m_Mounts.Size();
    bool only_missing = ctx->m_OnlyMissing;
    bool include_requested_url =  ctx->m_IncludeRequestedUrl;

    dmArray<dmhash_t> dependencies; // allocate it outside of the loop

    for (uint32_t i = 0; i < num_mounts; ++i)
    {
        ArchiveMount& mount = mounts_ctx->m_Mounts[i];

        dmResource::Manifest* manifest;
        dmResourceProvider::Result presult = dmResourceProvider::GetManifest(mount.m_Archive, &manifest);
        if (presult != dmResourceProvider::RESULT_OK)
            continue;

        dependencies.SetSize(0);
        dmResource::Result result = dmResource::GetDependencies(manifest, url_hash, dependencies);
        if (dmResource::RESULT_RESOURCE_NOT_FOUND == result)
            continue;

        if (include_requested_url) {
            // include only once
            include_requested_url = false;
            dependencies.OffsetCapacity(1);
            int size = dependencies.Size();
            dependencies.SetSize(size + 1);
            dependencies[size] = url_hash;
        }

        uint32_t hash_len = dmResource::GetEntryHashLength(manifest);

        for (uint32_t d = 0; d < dependencies.Size(); ++d)
        {
            dmhash_t dep_hash = dependencies[d];
            bool* visited = ctx->m_Visited.Get(dep_hash);
            if (visited)
                continue;

            bool exists = true;
            if (only_missing)
            {
                exists = ResourceExists(mounts_ctx, dep_hash) == dmResource::RESULT_OK;
            }

            UpdateVisited(ctx->m_Visited, dep_hash, exists);

            // If we only want the missing ones
            if (only_missing && exists)
                continue;


            SGetDependenciesResult result = {0};
            result.m_UrlHash = dep_hash;
            result.m_Missing = !exists;
            dmLiveUpdateDDF::ResourceEntry* entry = dmResource::FindEntry(manifest, dep_hash);
            if (!entry)
            {
                // NOTE: It is important that we don't remove "liveupdate" resource dependencies from the manifest
                // Otherwise we cannot get the actual path to report back to the user for the download
                dmLogError("Couldn't get entry for dependency " DM_HASH_FMT " in manifest from " DM_HASH_FMT "", dep_hash, url_hash);
                ctx->m_UserCallback(ctx->m_UserCallbackCtx, &result);
                continue;
            }

            result.m_HashDigest        = entry->m_Hash.m_Data.m_Data;
            result.m_HashDigestLength  = hash_len;

            ctx->m_UserCallback(ctx->m_UserCallbackCtx, &result);

            if (ctx->m_Resursive && entry->m_Dependants.m_Count > 0)
            {
                // If we need to check other resources, we need to loop over the mounts again
                GetDependenciesInternal(ctx, dep_hash);
            }
        }
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result GetDependencies(HContext ctx, const SGetDependenciesParams* request, FGetDependency callback, void* callback_context)
{
    DependencyIterContext iter_ctx;
    iter_ctx.m_Mounts = ctx;
    iter_ctx.m_UserCallback = callback;
    iter_ctx.m_UserCallbackCtx = callback_context;
    iter_ctx.m_Resursive = request->m_Recursive;
    iter_ctx.m_OnlyMissing = request->m_OnlyMissing;
    iter_ctx.m_IncludeRequestedUrl = request->m_IncludeRequestedUrl;

    return GetDependenciesInternal(&iter_ctx, request->m_UrlHash);
}

}
