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

#ifndef DM_RESOURCE_PRIVATE_H
#define DM_RESOURCE_PRIVATE_H

#include <ddf/ddf.h>
#include "resource_archive.h"
#include "resource.h"

// Internal API that preloader needs to use.

// 1 - higher level mount info
// 3 - more debug info per file

//#define DM_RESOURCE_DBG_LOG_LEVEL 1

#if defined(DM_RESOURCE_DBG_LOG_LEVEL)
    #include <stdio.h> // for debug log
    #define DM_RESOURCE_DBG_LOG(__LEVEL__, ...) if ((__LEVEL__) <= DM_RESOURCE_DBG_LOG_LEVEL) { printf(__VA_ARGS__); fflush(stdout); }
#else
    #define DM_RESOURCE_DBG_LOG(__LEVEL__, ...)
#endif


namespace dmResource
{
    const uint32_t MAX_RESOURCE_TYPES = 128;

    struct SResourceType
    {
        SResourceType()
        {
            memset(this, 0, sizeof(*this));
        }
        dmhash_t            m_ExtensionHash;
        const char*         m_Extension;
        void*               m_Context;
        FResourcePreload    m_PreloadFunction;
        FResourceCreate     m_CreateFunction;
        FResourcePostCreate m_PostCreateFunction;
        FResourceDestroy    m_DestroyFunction;
        FResourceRecreate   m_RecreateFunction;
    };

    typedef dmArray<char> LoadBufferType;

    struct SResourceDescriptor;

    Result CheckSuppliedResourcePath(const char* name);

    // load with default internal buffer and its management, returns buffer ptr in 'buffer'
    Result LoadResource(HFactory factory, const char* path, const char* original_name, void** buffer, uint32_t* resource_size);
    // load with own buffer
    Result DoLoadResource(HFactory factory, const char* path, const char* original_name, uint32_t* resource_size, LoadBufferType* buffer);

    Result InsertResource(HFactory factory, const char* path, uint64_t canonical_path_hash, SResourceDescriptor* descriptor);
    uint32_t GetCanonicalPathFromBase(const char* base_dir, const char* relative_dir, char* buf);

    SResourceType* FindResourceType(SResourceFactory* factory, const char* extension);
    uint32_t GetRefCount(HFactory factory, void* resource);
    uint32_t GetRefCount(HFactory factory, dmhash_t identifier);

    // Platform specific implementation of archive and manifest loading. Data written into mount_info must
    // be provided for unloading and may contain information about memory mapping etc.
    Result MountArchiveInternal(const char* index_path, const char* data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info);
    void UnmountArchiveInternal(dmResourceArchive::HArchiveIndexContainer &archive, void* mount_info);
    Result MountManifest(const char* manifest_filename, void*& out_map, uint32_t& out_size);
    Result UnmountManifest(void *& map, uint32_t size);
    // Files mapped with this function should be unmapped with UnmapFile(...)
    Result MapFile(const char* filename, void*& map, uint32_t& size);
    Result UnmapFile(void*& map, uint32_t size);

    /**
     * In the case of an app-store upgrade, we dont want the runtime to load any existing local liveupdate.manifest.
     * We check this by persisting the bundled manifest signature to file the first time a liveupdate.manifest
     * is stored. At app start we check the current bundled manifest signature against the signature written to file.
     * If they don't match the bundle has changed, and we need to remove any liveupdate.manifest from the filesystem
     * and load the bundled manifest instead.
     */
    //Result BundleVersionValid(const Manifest* manifest, const char* bundle_ver_path);

    struct PreloadRequest;
    struct PreloadHintInfo
    {
        HPreloader m_Preloader;
        int32_t m_Parent;
    };

    struct TypeCreatorDesc
    {
        const char* m_Name;
        FResourceTypeRegister m_RegisterFn;
        FResourceTypeRegister m_DeregisterFn;
        TypeCreatorDesc* m_Next;
    };

    const TypeCreatorDesc* GetFirstTypeCreatorDesc();
}

#endif
