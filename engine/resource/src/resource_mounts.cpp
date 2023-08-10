#include "resource_mounts.h"
#include "resource_mounts_file.h"
#include "resource_manifest.h"
#include "resource_private.h" // for log
#include "resource_mounts.h"
#include "providers/provider.h"
#include <resource/liveupdate_ddf.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/mutex.h>
#include <dlib/sys.h>
#include <algorithm> // std::sort

namespace dmResourceMounts
{

#if defined(__linux__) && !defined(__ANDROID__)
    #define DM_HASH_FMT "%016lx"
#else
    #define DM_HASH_FMT "%016llx"
#endif

const char* MOUNTS_FILENAME = "liveupdate.mounts";

const int MAX_NAME_LENGTH = 64;

struct ArchiveMount
{
    const char*                     m_Name;
    dmResourceProvider::HArchive    m_Archive;
    int                             m_Priority;
    bool                            m_Persist;
};

struct ResourceMountsContext
{
    // The currently mounted archives, in sorted order
    dmArray<ArchiveMount>           m_Mounts;
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
    }
    dmMutex::Delete(ctx->m_Mutex);
    delete ctx;
}

struct MountSortPred
{
    bool operator ()(const ArchiveMount& a, const ArchiveMount& b) const
    {
        return a.m_Priority > b.m_Priority; // largest priority first
    }
};

static void SortMounts(dmArray<ArchiveMount>& mounts)
{
    std::sort(mounts.Begin(), mounts.End(), MountSortPred());
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
    DM_RESOURCE_DBG_LOG(level, "Mount:  prio: %3d  %s  p: %p  uri: %s:%s/%s\n", mount.m_Priority, mount.m_Name, mount.m_Archive, uri.m_Scheme, uri.m_Location, uri.m_Path);
}

static void DebugPrintMounts(HContext ctx)
{
    DM_RESOURCE_DBG_LOG(2, "Mounts\n");
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        DebugPrintMount(2, ctx->m_Mounts[i]);
    }
}

dmMutex::HMutex GetMutex(HContext ctx)
{
    return ctx->m_Mutex;
}

dmResource::Result AddMount(HContext ctx, const char* name, dmResourceProvider::HArchive archive, int priority, bool persist)
{
    if (strlen(name) >= MAX_NAME_LENGTH)
    {
        dmLogError("Mount has too long name. Max character count is %d: '%s'", MAX_NAME_LENGTH, name);
        return dmResource::RESULT_INVAL;
    }

    SGetMountResult mount_info;
    if (dmResource::RESULT_OK == GetMountByName(ctx, name, &mount_info))
    {
        dmLogError("Mount with name already exists: '%s'", name);
        return dmResource::RESULT_INVAL;
    }

    ArchiveMount mount;
    mount.m_Name = strdup(name);
    mount.m_Priority = priority;
    mount.m_Archive = archive;
    mount.m_Persist = persist;
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

dmResource::Result RemoveMountByName(HContext ctx, const char* name)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (strcmp(mount.m_Name, name) == 0)
        {
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

static dmResource::Result LoadMount(HContext ctx, int priority, const char* name, const char* path, bool persist)
{
    dmURI::Parts uri;
    dmURI::Parse(path, &uri);

    dmResourceProvider::HArchiveLoader loader = dmResourceProvider::FindLoaderByName(dmHashString64(uri.m_Scheme));
    if (!loader)
    {
        dmLogError("Couldn't find loader for scheme '%s' (uri '%s')", uri.m_Scheme, path);
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    if (!dmResourceProvider::CanMountUri(loader, &uri))
    {
        dmLogError("Loader can't mount uri '%s'", path);
        return dmResource::RESULT_NOT_SUPPORTED;
    }

    dmResourceProvider::HArchive archive;
    dmResourceProvider::Result provider_result = dmResourceProvider::CreateMount(loader, &uri, ctx->m_ResourceBaseArchive, &archive);
    if (dmResourceProvider::RESULT_OK != provider_result)
    {
        dmLogError("Failed to create mount '%s' - '%s': %d", name, path, provider_result);
        return dmResource::RESULT_IO_ERROR;
    }

    dmResourceMounts::AddMount(ctx, name, archive, priority, persist);

    return dmResource::RESULT_OK;
}

dmResource::Result LoadMounts(HContext ctx, const char* app_support_path)
{
    char path[DMPATH_MAX_PATH];
    dmPath::Concat(app_support_path, MOUNTS_FILENAME, path, sizeof(path));

    if (!dmSys::Exists(path))
    {
        DM_RESOURCE_DBG_LOG(1, "No mount file detected: '%s'\n", path);
        return dmResource::RESULT_OK;
    }

    DM_RESOURCE_DBG_LOG(1, "Loading mounts '%s'\n", path);

    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex); // avoid reading while it might be written

    dmArray<MountFileEntry> entries;
    dmResource::Result result = ReadMountsFile(path, entries);
    if (dmResource::RESULT_OK != result)
    {
        dmLogError("Failed to read mounts file");
        return result;
    }

    uint32_t size = entries.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        MountFileEntry& entry = entries[i];

        DM_RESOURCE_DBG_LOG(2, "  mounting: '%s' %d '%s'\n", entry.m_Name, entry.m_Priority, entry.m_Uri);
        LoadMount(ctx, entry.m_Priority, entry.m_Name, entry.m_Uri, true);
    }

    FreeMountsFile(entries);
    return dmResource::RESULT_OK;
}

dmResource::Result SaveMounts(HContext ctx, const char* app_support_path)
{
    char path[DMPATH_MAX_PATH];
    dmPath::Concat(app_support_path, MOUNTS_FILENAME, path, sizeof(path));

    DM_RESOURCE_DBG_LOG(1, "Saving mounts '%s'\n", path);

    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    dmArray<MountFileEntry> entries;

    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (!mount.m_Persist)
            continue;

        if (entries.Full())
            entries.OffsetCapacity(8);

        dmURI::Parts uri;
        dmResourceProvider::GetUri(mount.m_Archive, &uri);

        char uri_str[1024];
        if (uri.m_Location[0] == '\0')
            dmSnPrintf(uri_str, sizeof(uri_str), "%s:%s", uri.m_Scheme, uri.m_Path);
        else
            dmSnPrintf(uri_str, sizeof(uri_str), "%s:%s/%s", uri.m_Scheme, uri.m_Location, uri.m_Path);

        MountFileEntry entry;
        entry.m_Name = strdup(mount.m_Name);
        entry.m_Uri = strdup(uri_str);
        entry.m_Priority = mount.m_Priority;

        entries.Push(entry);
    }

    dmResource::Result result = WriteMountsFile(path, entries);
    FreeMountsFile(entries);

    return result;
}

dmResource::Result GetMountByIndex(HContext ctx, uint32_t index, SGetMountResult* mount_info)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
    if (index >= ctx->m_Mounts.Size())
        return dmResource::RESULT_INVAL;

    ArchiveMount& mount = ctx->m_Mounts[index];
    mount_info->m_Name = mount.m_Name;
    mount_info->m_Archive = mount.m_Archive;
    mount_info->m_Priority = mount.m_Priority;
    return dmResource::RESULT_OK;
}

dmResource::Result GetMountByName(HContext ctx, const char* name, SGetMountResult* mount_info)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (strcmp(mount.m_Name, name) == 0)
            return GetMountByIndex(ctx, i, mount_info);
    }
    return dmResource::RESULT_INVAL;
}

static dmResource::Result DestroyMounts(HContext ctx)
{
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        free((void*)mount.m_Name);
        dmResourceProvider::Unmount(mount.m_Archive);
    }
    ctx->m_Mounts.SetSize(0);
    return dmResource::RESULT_OK;
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
            DM_RESOURCE_DBG_LOG(3, "GetResourceSize OK: %s  " DM_HASH_FMT " (%u bytes)\n", path, path_hash, *resource_size);
            DebugPrintMount(3, mount);
            return dmResource::RESULT_OK;
        }
        return ProviderResultToResult(result);
    }
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
            DM_RESOURCE_DBG_LOG(3, "ReadResource: %s (%u bytes)\n", path, buffer_size);
            DebugPrintMount(3, mount);
            return dmResource::RESULT_OK;
        }
        return ProviderResultToResult(result);
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result ReadResource(HContext ctx, const char* path, dmhash_t path_hash, dmArray<char>* buffer)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    uint32_t resource_size;
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        dmResourceProvider::Result result = dmResourceProvider::GetFileSize(mount.m_Archive, path_hash, path, &resource_size);
        if (dmResourceProvider::RESULT_OK == result)
        {
            if (buffer->Capacity() < resource_size)
                buffer->SetCapacity(resource_size);
            buffer->SetSize(resource_size);

            result = dmResourceProvider::ReadFile(mount.m_Archive, path_hash, path, (uint8_t*)buffer->Begin(), resource_size);
            DM_RESOURCE_DBG_LOG(3, "ReadResource: %s (%u bytes) - result %d\n", path, resource_size, result);
            DebugPrintMount(3, mount);
            return ProviderResultToResult(result);
        }
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

struct DependencyIterContext
{
    dmHashTable64<bool> m_Visited;
    HContext            m_Mounts;
    FGetDependency      m_UserCallback;
    void*               m_UserCallbackCtx;
    bool                m_OnlyMissing;
    bool                m_Resursive;
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

    return GetDependenciesInternal(&iter_ctx, request->m_UrlHash);
}

}

#undef DM_HASH_FMT

