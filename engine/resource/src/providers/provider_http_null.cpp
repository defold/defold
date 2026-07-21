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

namespace dmResourceProviderHttp
{
    static bool MatchesUri(const dmURI::Parts* uri)
    {
        (void) uri;
        return false;
    }

    static dmResourceProvider::Result Mount(const dmURI::Parts* uri, dmResourceProvider::HArchive base_archive, dmResourceProvider::HArchiveInternal* out_archive)
    {
        (void) uri;
        (void) base_archive;
        (void) out_archive;
        return dmResourceProvider::RESULT_NOT_SUPPORTED;
    }

    static dmResourceProvider::Result Unmount(dmResourceProvider::HArchiveInternal archive)
    {
        (void) archive;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result GetFileSize(dmResourceProvider::HArchiveInternal archive, dmhash_t path_hash, const char* path, uint32_t* file_size)
    {
        (void) archive;
        (void) path_hash;
        (void) path;
        (void) file_size;
        return dmResourceProvider::RESULT_NOT_SUPPORTED;
    }

    static dmResourceProvider::Result ReadFile(dmResourceProvider::HArchiveInternal archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len)
    {
        (void) archive;
        (void) path_hash;
        (void) path;
        (void) buffer;
        (void) buffer_len;
        return dmResourceProvider::RESULT_NOT_SUPPORTED;
    }

    static dmResourceProvider::Result ReadFilePartial(dmResourceProvider::HArchiveInternal archive, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread)
    {
        (void) archive;
        (void) path_hash;
        (void) path;
        (void) offset;
        (void) size;
        (void) buffer;
        (void) nread;
        return dmResourceProvider::RESULT_NOT_SUPPORTED;
    }

    static dmResourceProvider::Result InitializeArchiveLoaderHttp(dmResourceProvider::ArchiveLoaderParams* params, dmResourceProvider::ArchiveLoader* loader)
    {
        (void) params;
        (void) loader;
        return dmResourceProvider::RESULT_OK;
    }

    static dmResourceProvider::Result FinalizeArchiveLoaderHttp(dmResourceProvider::ArchiveLoaderParams* params, dmResourceProvider::ArchiveLoader* loader)
    {
        (void) params;
        (void) loader;
        return dmResourceProvider::RESULT_OK;
    }

    static void SetupArchiveLoaderHttp(dmResourceProvider::ArchiveLoader* loader)
    {
        loader->m_CanMount        = MatchesUri;
        loader->m_Mount           = Mount;
        loader->m_Unmount         = Unmount;
        loader->m_GetFileSize     = GetFileSize;
        loader->m_ReadFile        = ReadFile;
        loader->m_ReadFilePartial = ReadFilePartial;
    }

    DM_DECLARE_ARCHIVE_LOADER(ResourceProviderHttp, "http", SetupArchiveLoaderHttp, InitializeArchiveLoaderHttp, FinalizeArchiveLoaderHttp);
}
