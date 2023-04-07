#include <assert.h>
#include <stdio.h> // debug: printf

#include "provider.h"
#include "provider_private.h"

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
    //assert(m_GetProjectIdentifier != 0);
    assert(m_GetFileSize != 0);
    assert(m_ReadFile != 0);
}

void RegisterArchiveLoader(ArchiveLoader* loader)
{
    loader->Verify();
    loader->m_Next = 0;
    g_ArchiveLoaders = loader;
}

void Register(ArchiveLoader* loader, uint32_t size, const char* name, void (*setup_fn)(ArchiveLoader*))
{
    loader->m_NameHash = dmHashString64(name);
    loader->m_Next = 0;
printf("\nRegistered archive loader: %s\n", name);

    setup_fn(loader);
    RegisterArchiveLoader(loader);
}

void ClearArchiveLoaders(ArchiveLoader* loader)
{
    g_ArchiveLoaders = 0;
}

ArchiveLoader* FindLoaderByName(dmhash_t name_hash)
{
    ArchiveLoader* loader = g_ArchiveLoaders;
    while (loader)
    {
        if (loader->m_NameHash == name_hash)
            return loader;
        loader = loader->m_Next;
    }
    return 0;
}

ArchiveLoader* FindLoaderByUri(const dmURI::Parts* uri)
{
    ArchiveLoader* loader = g_ArchiveLoaders;
    while (loader)
    {
        if (loader->m_CanMount(uri))
            return loader;
        loader = loader->m_Next;
    }
    return 0;
}

// ****************************************
// Archives

Result Mount(const dmURI::Parts* uri, HArchive* out_archive)
{
    ArchiveLoader* loader = FindLoaderByUri(uri);
    if (!loader)
    {
        dmLogError("Found no matching loader for '%s:/%s%s'", uri->m_Scheme, uri->m_Location, uri->m_Path);
        return RESULT_NOT_FOUND;
    }

    void* internal;
    Result result = loader->m_Mount(uri, &internal);
    if (result == RESULT_OK)
    {
        Archive* archive = new Archive;
        archive->m_Loader = loader;
        archive->m_Internal = internal;
        *out_archive = archive;
    }
    return result;
}

Result Unmount(HArchive archive)
{
    Result result = archive->m_Loader->m_Unmount(archive->m_Internal);
    delete archive;
    return result;
}

Result CreateMount(ArchiveLoader* loader, void* internal, HArchive* out_archive)
{
    Archive* archive = new Archive;
    archive->m_Loader = loader;
    archive->m_Internal = internal;
    *out_archive = archive;
    return RESULT_OK;
}

Result GetFileSize(HArchive archive, const char* path, uint32_t* file_size)
{
    return archive->m_Loader->m_GetFileSize(archive->m_Internal, path, file_size);
}

Result ReadFile(HArchive archive, const char* path, uint8_t* buffer, uint32_t buffer_len)
{
    return archive->m_Loader->m_ReadFile(archive->m_Internal, path, buffer, buffer_len);
}

Result GetManifest(dmResourceProvider::HArchive archive, dmResource::Manifest** out_manifest)
{
    return archive->m_Loader->m_GetManifest(archive->m_Internal, out_manifest);
}




} // namespace
