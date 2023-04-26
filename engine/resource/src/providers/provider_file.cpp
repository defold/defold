#include "provider.h"
#include "provider_private.h"
// #include "../resource_archive.h"

// #include <dlib/dstrings.h>
// #include <dlib/endian.h>
#include <dlib/hash.h>
#include <dlib/log.h>
// #include <dlib/lz4.h>
// #include <dlib/memory.h>
#include <dlib/sys.h>
#include <dlib/path.h> // DMPATH_MAX_PATH

#include <stdio.h> // debug printf


namespace dmResourceProviderFile
{
    struct FileProviderContext
    {
        dmURI::Parts m_BaseUri;
    };

    static bool MatchesUri(const dmURI::Parts* uri)
    {
        return strcmp(uri->m_Scheme, "file") == 0
#if defined(__NX__)
            || strcmp(uri->m_Scheme, "data") == 0
            || strcmp(uri->m_Scheme, "host") == 0
#endif
        ;
    }

    static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
    {
        if (!MatchesUri(uri))
            return dmResourceProvider::RESULT_NOT_SUPPORTED;

        FileProviderContext* archive = new FileProviderContext;
        memcpy(&archive->m_BaseUri, uri, sizeof(dmURI::Parts));

        *out_archive = (dmResourceProvider::HArchiveInternal)archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
    {
        delete (FileProviderContext*)archive;
        return dmResourceProvider::RESULT_OK;
    }

    static char* ResolveFilePath(const char* path, char* buffer, uint32_t buffer_len)
    {
        if (dmSys::RESULT_OK != dmSys::ResolveMountFileName(buffer, buffer_len, path))
        {
            // on some platforms, performing operations on non existing files will halt the engine
            // so we're better off returning here immediately
            dmLogError("Failed to locate file '%s'", path);
            return 0;
        }
        return buffer;
    }

    static dmResourceProvider::Result SysResultToProviderResult(dmSys::Result r)
    {
        if (r == dmSys::RESULT_OK)
            return dmResourceProvider::RESULT_OK;
        else if (r == dmSys::RESULT_NOENT)
            return dmResourceProvider::RESULT_NOT_FOUND;
        else
            return dmResourceProvider::RESULT_IO_ERROR;
        return dmResourceProvider::RESULT_ERROR_UNKNOWN;
    }

    static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
    {
        (void)archive;
        (void)path_hash;

        char path_buffer[DMPATH_MAX_PATH];
        const char* resolved_path = ResolveFilePath(path, path_buffer, sizeof(path_buffer));
        if (!resolved_path) {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        dmSys::Result r = dmSys::ResourceSize(path, file_size);
        return SysResultToProviderResult(r);
    }

    static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
    {
        (void)archive;
        (void)path_hash;

        char path_buffer[DMPATH_MAX_PATH];
        const char* resolved_path = ResolveFilePath(path, path_buffer, sizeof(path_buffer));
        if (!resolved_path) {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        uint32_t file_size;
        dmSys::Result r = dmSys::LoadResource(resolved_path, buffer, buffer_len, &file_size);
        return SysResultToProviderResult(r);
    }

    static void SetupArchiveLoader(dmResourceProvider::ArchiveLoader* loader)
    {
        loader->m_CanMount      = MatchesUri;
        loader->m_Mount         = Mount;
        loader->m_Unmount       = Unmount;
        loader->m_GetFileSize   = GetFileSize;
        loader->m_ReadFile      = ReadFile;
    }

    DM_DECLARE_ARCHIVE_LOADER(ResourceProviderFile, "file", SetupArchiveLoader);
}
