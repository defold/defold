// Copyright 2020-2023 The Defold Foundation
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
#include <dlib/uri.h>

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
    Result WriteFile(HArchive archive, dmhash_t path_hash, const char* path, const uint8_t* buffer, uint32_t buffer_len);


    // Plugin API

    // Internal
    void Register(ArchiveLoader* loader, uint32_t size, const char* name, void (*setup_fn)(ArchiveLoader*));

    // Extension declaration helper. Internal
    #ifdef __GNUC__
        // Workaround for dead-stripping on OSX/iOS. The symbol "name" is explicitly exported. See wscript "exported_symbols"
        // Otherwise it's dead-stripped even though -no_dead_strip_inits_and_terms is passed to the linker
        // The bug only happens when the symbol is in a static library though
        #define DM_REGISTER_ARCHIVE_LOADER(symbol, desc, desc_size, name, setup_fn) extern "C" void __attribute__((constructor)) symbol () { \
            dmResourceProvider::Register((struct dmResourceProvider::ArchiveLoader*) &desc, desc_size, name, setup_fn); \
        }
    #else
        #define DM_REGISTER_ARCHIVE_LOADER(symbol, desc, desc_size, name, setup_fn) extern "C" void symbol () { \
            dmResourceProvider::Register((struct dmResourceProvider::ArchiveLoader*) &desc, desc_size, name, setup_fn); \
            }\
            int symbol ## Wrapper(void) { symbol(); return 0; } \
            __pragma(section(".CRT$XCU",read)) \
            __declspec(allocate(".CRT$XCU")) int (* _Fp ## symbol)(void) = symbol ## Wrapper;
    #endif

    // Archive loader desc bytesize declaration
    const uint32_t g_ExtensionDescBufferSize = 128;
    #define DM_RESOURCE_PROVIDER_PASTE_SYMREG(x, y) x ## y
    #define DM_RESOURCE_PROVIDER_PASTE_SYMREG2(x, y) DM_RESOURCE_PROVIDER_PASTE_SYMREG(x, y)

    // public
    #define DM_DECLARE_ARCHIVE_LOADER(symbol, name, setup_fn) \
        uint8_t DM_ALIGNED(16) DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__)[dmResourceProvider::g_ExtensionDescBufferSize]; \
        DM_REGISTER_ARCHIVE_LOADER(symbol, DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__)), name, setup_fn);
}

#endif // DM_RESOURCE_PROVIDER_H
