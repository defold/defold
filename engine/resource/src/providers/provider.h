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
//#include "resource_private.h"
#include <dlib/hash.h>
#include <dlib/uri.h>

namespace dmResource
{
    struct Manifest;
}

namespace dmResourceProvider
{
    enum Result
    {
        RESULT_OK               = 0,
        RESULT_NOT_SUPPORTED    = -1,
        RESULT_NOT_FOUND        = -2,
        RESULT_IO_ERROR         = -3,
        RESULT_INVAL_ERROR      = -4,       // invalid argument
        RESULT_ERROR_UNKNOWN    = -1000,
    };

    typedef struct Archive* HArchive;
    typedef void*           HArchiveInternal;
    struct ArchiveLoader;

    typedef bool   (*FCanMount)(const dmURI::Parts* uri);
    typedef Result (*FMount)(const dmURI::Parts* uri, HArchiveInternal* out_archive);
    typedef Result (*FUnmount)(HArchiveInternal archive);
    // Gets the project identifier from the manifest (Should this be here?)
    //typedef Result (*FGetProjectIdentifier)(HArchive archive, char* buffer, uint32_t buffer_len);
    /*
     * @param archive
     * @param path The path is relative to the initial uri domain
     * @param file_size The uncompressed file size
     */
    typedef Result (*FGetFileSize)(HArchiveInternal archive, const char* path, uint32_t* file_size);

    typedef Result (*FReadFile)(HArchiveInternal, const char* path, uint8_t* buffer, uint32_t buffer_len);

//TODO: Do we really want this manifest outside of the archives?
    typedef Result (*FGetManifest)(HArchiveInternal, dmResource::Manifest**);

    // typedef Result (*FManifestLoad)(const char* archive_name, const char* app_path, const char* app_support_path, const dmResource::Manifest* previous, dmResource::Manifest** manifest);
    // typedef Result (*FArchiveLoad)(const dmResource::Manifest* manifest, const char* archive_name, const char* application_path, const char* application_support_path, HArchiveIndexContainer previous, HArchiveIndexContainer* out);
    // typedef Result (*FArchiveUnload)(HArchiveIndexContainer);
    // typedef Result (*FArchiveFindEntry)(HArchiveIndexContainer, const uint8_t*, uint32_t, EntryData*);
    // typedef Result (*FArchiveRead)(HArchiveIndexContainer, const uint8_t*, uint32_t, const EntryData*, void*);


    // The resource loader types
    void            RegisterArchiveLoader(ArchiveLoader* loader);
    void            ClearArchiveLoaders(ArchiveLoader* loader);
    ArchiveLoader*  FindLoaderByName(dmhash_t name_hash);
    ArchiveLoader*  FindLoaderByUri(const dmURI::Parts* uri);

    // The archive operations
    Result Mount(const dmURI::Parts* uri, HArchive* out_archive);
    Result Unmount(HArchive archive);
    Result GetFileSize(HArchive archive, const char* path, uint32_t* file_size);
    Result ReadFile(HArchive archive, const char* path, uint8_t* buffer, uint32_t buffer_len);
    Result GetManifest(HArchive archive, dmResource::Manifest** out_manifest);

    // Create an archive manually
    Result CreateMount(ArchiveLoader* loader, void* internal, HArchive* out_archive);

    // Plugin API

    // private
    void Register(ArchiveLoader* loader, uint32_t size, const char* name, void (*setup_fn)(ArchiveLoader*));

    /**
     * Extension declaration helper. Internal
     */
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

    /**
    * Archive loader desc bytesize declaration. Internal
    */
    const uint32_t g_ExtensionDescBufferSize = 128;

    // internal
    #define DM_RESOURCE_PROVIDER_PASTE_SYMREG(x, y) x ## y
    // internal
    #define DM_RESOURCE_PROVIDER_PASTE_SYMREG2(x, y) DM_RESOURCE_PROVIDER_PASTE_SYMREG(x, y)

    #define DM_DECLARE_ARCHIVE_LOADER(symbol, name, setup_fn) \
        uint8_t DM_ALIGNED(16) DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__)[dmResourceProvider::g_ExtensionDescBufferSize]; \
        DM_REGISTER_ARCHIVE_LOADER(symbol, DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__), sizeof(DM_RESOURCE_PROVIDER_PASTE_SYMREG2(symbol, __LINE__)), name, setup_fn);

}

#endif // DM_RESOURCE_PROVIDER_H
