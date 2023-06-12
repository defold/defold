#include "resource_mounts.h"
#include "resource_private.h" // for log
#include "providers/provider.h"

#include <dlib/log.h>
#include <algorithm> // std::sort

namespace dmResourceMounts
{

struct ArchiveMount
{
    dmResourceProvider::HArchive    m_Archive;
    int                             m_Priority;
};

struct ResourceMountsContext
{
    // The currently mounted archives, in sorted order
    dmArray<ArchiveMount> m_Mounts;
} g_ResourceMountsContext;


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


HContext Create()
{
    ResourceMountsContext* ctx = new ResourceMountsContext;
    ctx->m_Mounts.SetCapacity(2);
    return ctx;
}

void Destroy(HContext ctx)
{
    DestroyArchives(ctx);
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
    DM_RESOURCE_DBG_LOG(level, "Mount:  prio: %3d  p: %p  uri: %s:%s/%s\n", mount.m_Priority, mount.m_Archive, uri.m_Scheme, uri.m_Location, uri.m_Path);
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

dmResource::Result AddMount(HContext ctx, dmResourceProvider::HArchive archive, int priority)
{
    // TODO: Make sure a uri wasn't already added!

    ArchiveMount mount;
    mount.m_Priority = priority;
    mount.m_Archive = archive;
    AddMountInternal(ctx, mount);
    return dmResource::RESULT_OK;
}

dmResource::Result RemoveMount(HContext ctx, dmResourceProvider::HArchive archive)
{
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        if (mount.m_Archive == archive)
        {
            ctx->m_Mounts.EraseSwap(i); // TODO: We'd like an Erase() function in dmArray, to keep the internal ordering
            SortMounts(ctx->m_Mounts);

            DM_RESOURCE_DBG_LOG(1, "Removed archive %p\n", archive);
            return dmResource::RESULT_OK;
        }
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

// TODO: Get mount info (iterator?)
dmResource::Result DestroyArchives(HContext ctx)
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

dmResource::Result GetResourceSize(HContext ctx, dmhash_t path_hash, const char* path, uint32_t* resource_size)
{
    uint32_t size = ctx->m_Mounts.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ArchiveMount& mount = ctx->m_Mounts[i];
        dmResourceProvider::Result result = dmResourceProvider::GetFileSize(mount.m_Archive, path_hash, path, resource_size);
        if (dmResourceProvider::RESULT_NOT_FOUND == result)
            continue;
        if (dmResourceProvider::RESULT_OK == result)
        {
            DM_RESOURCE_DBG_LOG(3, "GetResourceSize: %s (%u bytes)\n", path, *resource_size);
            DebugPrintMount(3, mount);
            return dmResource::RESULT_OK;
        }
        return ProviderResultToResult(result);
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}

dmResource::Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_size)
{
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

dmResource::Result ReadResource(HContext ctx, const char* path, dmhash_t path_hash, dmResource::LoadBufferType* buffer)
{
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


}

