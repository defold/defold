#include <assert.h>
#include <stdio.h> // debug: printf

#include "provider.h"
#include "provider_private.h"
#include "../resource_private.h" // for logging

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/static_assert.h>

namespace dmResourceProvider
{

static ArchiveLoader* g_ArchiveLoaders = 0;

// ****************************************
// Loaders

void ArchiveLoader::Verify()
{
    DM_STATIC_ASSERT(g_ExtensionDescBufferSize >= sizeof(ArchiveLoader), Invalid_Struct_Size); // If not, the extension api won't work
    assert(m_NameHash != 0);
    assert(m_Mount != 0);
    assert(m_Unmount != 0);
    assert(m_GetFileSize != 0);
    assert(m_ReadFile != 0);
}

void RegisterArchiveLoader(ArchiveLoader* loader)
{
    loader->Verify();
    loader->m_Next = g_ArchiveLoaders;
    g_ArchiveLoaders = loader;
}

void Register(ArchiveLoader* loader, uint32_t size, const char* name, void (*setup_fn)(ArchiveLoader*))
{
    memset(loader, 0, sizeof(ArchiveLoader));
    loader->m_NameHash = dmHashString64(name);

    setup_fn(loader);
    RegisterArchiveLoader(loader);
    DM_RESOURCE_DBG_LOG(2, "\nRegistered loader: %s %llx\n", name, loader->m_NameHash);
}

void ClearArchiveLoaders(ArchiveLoader* loader)
{
    g_ArchiveLoaders = 0;
}

ArchiveLoader* FindLoaderByName(dmhash_t name_hash)
{
    DM_RESOURCE_DBG_LOG(2, "\nFind loader: %s %llx\n", dmHashReverseSafe64(name_hash), name_hash);
    ArchiveLoader* loader = g_ArchiveLoaders;
    while (loader)
    {
        DM_RESOURCE_DBG_LOG(2, "  loader: %s %llx\n", dmHashReverseSafe64(loader->m_NameHash), loader->m_NameHash);
        if (loader->m_NameHash == name_hash)
            return loader;
        loader = loader->m_Next;
    }
    DM_RESOURCE_DBG_LOG(2, "\n  no loader found for name '%s'\n", dmHashReverseSafe64(name_hash));
    return 0;
}

bool CanMountUri(HArchiveLoader loader, const dmURI::Parts* uri)
{
    return loader->m_CanMount(uri);
}

// ****************************************
// Archives

static Result DoMount(ArchiveLoader* loader, const dmURI::Parts* uri, HArchive base_archive, HArchive* out_archive)
{
    void* internal;
    Result result = loader->m_Mount(uri, base_archive, &internal);
    if (result == RESULT_OK)
    {
        Archive* archive = new Archive;
        memcpy(&archive->m_Uri, uri, sizeof(dmURI::Parts));
        archive->m_Loader = loader;
        archive->m_Internal = internal;
        *out_archive = archive;
    }
    return result;
}

Result CreateMount(HArchiveLoader loader, const dmURI::Parts* uri, HArchive base_archive, HArchive* out_archive)
{
    if (!loader->m_CanMount(uri))
        return RESULT_NOT_SUPPORTED;
    return DoMount(loader, uri, base_archive, out_archive);
}

Result CreateMount(ArchiveLoader* loader, void* internal, HArchive* out_archive)
{
    Archive* archive = new Archive;
    memset(archive, 0, sizeof(Archive));
    archive->m_Loader = loader;
    archive->m_Internal = internal;
    *out_archive = archive;
    return RESULT_OK;
}

Result Unmount(HArchive archive)
{
    Result result = archive->m_Loader->m_Unmount(archive->m_Internal);
    delete archive;
    return result;
}

Result GetFileSize(HArchive archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
{
    return archive->m_Loader->m_GetFileSize(archive->m_Internal, path_hash, path, file_size);
}

Result ReadFile(HArchive archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
{
    return archive->m_Loader->m_ReadFile(archive->m_Internal, path_hash, path, buffer, buffer_len);
}

Result GetManifest(HArchive archive, dmResource::HManifest* out_manifest)
{
    if (archive->m_Loader->m_GetManifest)
        return archive->m_Loader->m_GetManifest(archive->m_Internal, out_manifest);
    return RESULT_NOT_SUPPORTED;
}

Result SetManifest(HArchive archive, dmResource::HManifest manifest)
{
    if (archive->m_Loader->m_SetManifest)
        return archive->m_Loader->m_SetManifest(archive->m_Internal, manifest);
    return RESULT_NOT_SUPPORTED;
}

Result GetUri(HArchive archive, dmURI::Parts* out_uri)
{
    memcpy(out_uri, &archive->m_Uri, sizeof(dmURI::Parts));
    return RESULT_OK;
}

Result WriteFile(HArchive archive, dmhash_t path_hash, const char* path, const uint8_t* buffer, uint32_t buffer_len)
{
    if (archive->m_Loader->m_WriteFile)
        return archive->m_Loader->m_WriteFile(archive->m_Internal, path_hash, path, buffer, buffer_len);
    dmLogError("Archive type '%s' doesn't support writing files", dmHashReverseSafe64(archive->m_Loader->m_NameHash));
    return RESULT_NOT_SUPPORTED;
}



} // namespace
