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

#include "provider.h"
#include "provider_private.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/sys.h>
#include <dlib/path.h> // DMPATH_MAX_PATH

#include <stdio.h> // debug printf

namespace dmResourceProviderFile
{
    static const uint32_t FILE_PATH_MAX = DMPATH_MAX_PATH + dmResource::RESOURCE_PATH_MAX;

    struct FileProviderContext
    {
        char m_BasePath[DMPATH_MAX_PATH];
        char m_HostBasePath[DMPATH_MAX_PATH];
    };

    static bool MatchesUri(const dmURI::Parts* uri)
    {
        return strcmp(uri->m_Scheme, "file") == 0;
    }

    static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
    {
        if (!MatchesUri(uri))
            return dmResourceProvider::RESULT_NOT_SUPPORTED;

        char base_path[DMPATH_MAX_PATH];
        if (dmSnPrintf(base_path, sizeof(base_path), "%s%s", uri->m_Location, uri->m_Path) >= (int)sizeof(base_path))
        {
            return dmResourceProvider::RESULT_INVAL_ERROR;
        }

        // Anchor both lookup roots before application code can change the process cwd.
        const char* root = base_path[0] ? base_path : "/";
        FileProviderContext* archive = new FileProviderContext;
        dmSys::Result result = dmSys::GetAbsolutePath(root, archive->m_BasePath, sizeof(archive->m_BasePath));
        if (result != dmSys::RESULT_OK)
        {
            delete archive;
            return result == dmSys::RESULT_INVAL ? dmResourceProvider::RESULT_INVAL_ERROR : dmResourceProvider::RESULT_IO_ERROR;
        }

        // Keep drive and mount root separators when host names are normalized.
        char directory_path[DMPATH_MAX_PATH];
        char host_path[DMPATH_MAX_PATH];
        if (dmSnPrintf(directory_path, sizeof(directory_path), "%s/.", root) >= (int)sizeof(directory_path) ||
            dmSys::GetHostFileName(host_path, sizeof(host_path), directory_path) != dmSys::RESULT_OK ||
            dmSys::GetAbsolutePath(host_path, archive->m_HostBasePath, sizeof(archive->m_HostBasePath)) != dmSys::RESULT_OK)
        {
            // An unavailable host fallback must not prevent native files from loading.
            archive->m_HostBasePath[0] = 0;
        }

        *out_archive = (dmResourceProvider::HArchiveInternal)archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
    {
        delete (FileProviderContext*)archive;
        return dmResourceProvider::RESULT_OK;
    }

    static char* ResolveFilePath(const FileProviderContext* archive, const char* path, char* buffer, uint32_t buffer_len)
    {
        if (strlen(path) >= dmResource::RESOURCE_PATH_MAX)
        {
            return 0;
        }
        if (dmSnPrintf(buffer, buffer_len, "%s%s", archive->m_BasePath, path) >= (int)buffer_len)
        {
            return 0;
        }
        if (dmSys::ResourceExists(buffer))
        {
            return buffer;
        }
        if (archive->m_HostBasePath[0])
        {
            char host_path[dmResource::RESOURCE_PATH_MAX];
            dmPath::Normalize(path, host_path, sizeof(host_path));
            if (dmSnPrintf(buffer, buffer_len, "%s%s", archive->m_HostBasePath, host_path) >= (int)buffer_len)
            {
                return 0;
            }
            if (dmSys::ResourceExists(buffer))
            {
                return buffer;
            }
        }
        // on some platforms, performing operations on non existing files will halt the engine
        // so we're better off returning here immediately
        return 0;
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

    static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
    {
        FileProviderContext* archive = (FileProviderContext*)_archive;
        (void)path_hash;

        char path_buffer[FILE_PATH_MAX];
        const char* resolved_path = ResolveFilePath(archive, path, path_buffer, sizeof(path_buffer));
        if (!resolved_path) {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        dmSys::Result r = dmSys::ResourceSize(resolved_path, file_size);
        return SysResultToProviderResult(r);
    }

    static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
    {
        FileProviderContext* archive = (FileProviderContext*)_archive;
        (void)path_hash;

        char path_buffer[FILE_PATH_MAX];
        const char* resolved_path = ResolveFilePath(archive, path, path_buffer, sizeof(path_buffer));
        if (!resolved_path) {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        uint32_t file_size;
        dmSys::Result r = dmSys::LoadResource(resolved_path, buffer, buffer_len, &file_size);
        return SysResultToProviderResult(r);
    }

    static dmResourceProvider::Result ReadFilePartial(dmResourceProvider::HArchiveInternal _archive, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
    {
        FileProviderContext* archive = (FileProviderContext*)_archive;
        (void)path_hash;

        char path_buffer[FILE_PATH_MAX];
        const char* resolved_path = ResolveFilePath(archive, path, path_buffer, sizeof(path_buffer));
        if (!resolved_path) {
            return dmResourceProvider::RESULT_NOT_FOUND;
        }

        dmSys::Result r = dmSys::LoadResourcePartial(resolved_path, offset, size, buffer, nread);
        return SysResultToProviderResult(r);
    }

    static void SetupArchiveLoader(dmResourceProvider::ArchiveLoader* loader)
    {
        loader->m_CanMount          = MatchesUri;
        loader->m_Mount             = Mount;
        loader->m_Unmount           = Unmount;
        loader->m_GetFileSize       = GetFileSize;
        loader->m_ReadFile          = ReadFile;
        loader->m_ReadFilePartial   = ReadFilePartial;
    }

    DM_DECLARE_ARCHIVE_LOADER(ResourceProviderFile, "file", SetupArchiveLoader, 0, 0);
}
