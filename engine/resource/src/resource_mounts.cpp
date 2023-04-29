#include "resource_mounts.h"
#include <dlib/log.h>

namespace dmResourceMounts
{

#define DEBUG_LOG 1
#if defined(DEBUG_LOG)
    #define LOG(...) dmLogInfo(__VA_ARGS__)
#else
    #define LOG(...)
#endif

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

static void AddMountInternal(HContext ctx, const ArchiveMount& mount)
{
    if (ctx->m_Mounts.Full())
        ctx->m_Mounts.OffsetCapacity(2);

    uint32_t size = ctx->m_Mounts.Size();
    ctx->m_Mounts.SetSize(size + 1);

    uint32_t insert_index = 0;
    ArchiveMount* array = ctx->m_Mounts.Begin();
    for (uint32_t i = 0; i < size; ++i, ++insert_index)
    {
        if (array[i].m_Priority <= mount.m_Priority)
            continue;

        // shift all remaining items right one step
        uint32_t num_to_move = size - i;
        memmove(array+i+1, array+i, sizeof(ArchiveMount) * num_to_move);
    }
    ctx->m_Mounts[insert_index] = mount;
    LOG("Added archive %p with prio %d", mount.m_Archive, mount.m_Priority);
}

dmResource::Result AddMount(HContext ctx, dmResourceProvider::HArchive archive, int priority)
{
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
            // shift all remaining items left one step
            uint32_t num_to_move = size - i - 1;
            ArchiveMount* array = ctx->m_Mounts.Begin();
            memmove(array+i, array+i+1, sizeof(ArchiveMount) * num_to_move);
            ctx->m_Mounts.SetSize(size-1);
            LOG("Removed archive %p", archive);
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
            return dmResource::RESULT_OK;
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
            return dmResource::RESULT_OK;
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
            return ProviderResultToResult(result);
        }
    }
    return dmResource::RESULT_RESOURCE_NOT_FOUND;
}


}

