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

#ifndef DM_RESOURCE_PROVIDER_H
#define DM_RESOURCE_PROVIDER_H

#include <stdint.h>
#include <dlib/hash.h>
#include <dlib/http_cache.h>
#include <dlib/uri.h>

#include "../resource.h"

namespace dmResource
{
    typedef struct Manifest* HManifest;
}

namespace dmResourceProvider
{
    enum Result
    {
        RESULT_OK                   = 0,
        RESULT_NOT_SUPPORTED        = -1,
        RESULT_NOT_FOUND            = -2,
        RESULT_IO_ERROR             = -3,
        RESULT_INVAL_ERROR          = -4,       // invalid argument
        RESULT_SIGNATURE_MISMATCH   = -5,
        RESULT_ERROR_UNKNOWN        = -1000,
    };

    struct ArchiveLoader;
    typedef struct Archive*         HArchive;
    typedef void*                   HArchiveInternal;
    typedef struct ArchiveLoader*   HArchiveLoader;


    // Api for the resource loaders
    typedef bool   (*FCanMount)(const dmURI::Parts* uri);
    typedef Result (*FMount)(const dmURI::Parts* uri, HArchive base_archive, HArchiveInternal* out_archive);
    typedef Result (*FUnmount)(HArchiveInternal archive);

    typedef Result (*FGetFileSize)(HArchiveInternal archive, dmhash_t path_hash, const char* path, uint32_t* file_size);
    typedef Result (*FReadFile)(HArchiveInternal archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len);
    typedef Result (*FReadFilePartial)(HArchiveInternal archive, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread);
    typedef Result (*FWriteFile)(HArchiveInternal archive, dmhash_t path_hash, const char* path, const uint8_t* buffer, uint32_t buffer_len);
    typedef Result (*FGetManifest)(HArchiveInternal, dmResource::HManifest*); // In order for other providers to get the base manifest
    typedef Result (*FSetManifest)(HArchiveInternal, dmResource::HManifest);  // In order to set a downloaded manifest to a provider


    // The resource loader types
    void            RegisterArchiveLoader(HArchiveLoader loader);
    void            ClearArchiveLoaders(HArchiveLoader loader);
    HArchiveLoader  FindLoaderByName(dmhash_t name_hash);
    bool            CanMountUri(HArchiveLoader loader, const dmURI::Parts* uri);

    // The archive operations
    Result CreateMount(HArchiveLoader loader, const dmURI::Parts* uri, HArchive base_archive, HArchive* out_archive);
    Result CreateMount(ArchiveLoader* loader, void* internal, HArchive* out_archive);
    Result Unmount(HArchive archive);
    Result GetUri(HArchive archive, dmURI::Parts* out_uri);
    Result GetManifest(HArchive archive, dmResource::HManifest* out_manifest);
    Result SetManifest(HArchive archive, dmResource::HManifest manifest);

    Result GetFileSize(HArchive archive, dmhash_t path_hash, const char* path, uint32_t* file_size);
    Result ReadFile(HArchive archive, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_len);
    Result ReadFilePartial(HArchive archive, dmhash_t path_hash, const char* path, uint32_t offset, uint32_t size, uint8_t* buffer, uint32_t* nread);
    Result WriteFile(HArchive archive, dmhash_t path_hash, const char* path, const uint8_t* buffer, uint32_t buffer_len);


    // Plugin API
    struct ArchiveLoaderParams
    {
        dmResource::HFactory    m_Factory;
        dmHttpCache::HCache     m_HttpCache;
    };

    typedef void (*FRegisterLoader)(ArchiveLoader*);
    typedef Result (*FInitializeLoader)(ArchiveLoaderParams*, ArchiveLoader*);
    typedef Result (*FFinalizeLoader)(ArchiveLoaderParams*, ArchiveLoader*);

    // Internal
    void Register(ArchiveLoader* loader, uint32_t size, const char* name,
                    FRegisterLoader register_fn,
                    FInitializeLoader initialize_fn,
                    FFinalizeLoader finalize_fn);

    Result InitializeLoaders(ArchiveLoaderParams*);
    Result FinalizeLoaders(ArchiveLoaderParams*);

    // Extension declaration helper. Internal
    #define DM_REGISTER_ARCHIVE_LOADER(symbol, desc, desc_size, name, register_fn, initialize_fn, finalize_fn) extern "C" void symbol () { \
        dmResourceProvider::Register((struct dmResourceProvider::ArchiveLoader*) &desc, desc_size, name, register_fn, initialize_fn, finalize_fn); \
    }

    // Archive loader desc bytesize declaration
    const uint32_t g_ExtensionDescBufferSize = 128;
    #define DM_RESOURCE_PROVIDER_PASTE_SYMREG(x, y) x ## y
    #define DM_RESOURCE_PROVIDER_PASTE_SYMREG2(x, y) DM_RESOURCE_PROVIDER_PASTE_SYMREG(x, y)

    // public
    #define DM_DECLARE_ARCHIVE_LOADER(symbol, name, register_fn, initialize_fn, finalize_fn) \
        uint8_t DM_ALIGNED(16) DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__)[dmResourceProvider::g_ExtensionDescBufferSize]; \
        DM_REGISTER_ARCHIVE_LOADER(symbol, DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__)), name, register_fn, initialize_fn, finalize_fn);
}

#endif // DM_RESOURCE_PROVIDER_H
