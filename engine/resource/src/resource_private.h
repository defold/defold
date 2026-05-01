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

#ifndef DM_RESOURCE_PRIVATE_H
#define DM_RESOURCE_PRIVATE_H

#include <ddf/ddf.h>
#include "resource_archive.h"
#include "resource.h"
#include <dmsdk/resource/resource.h>

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


/**
 * Resource descriptor
 * @name ResourceDescriptor
 * @member m_NameHash [type: uint64_t] Hash of resource name
 * @member m_Resource [type: void*] Resource pointer. Must be unique and not NULL.
 * @member m_PrevResource [type: void*] Resource pointer. Resource pointer to a previous version of the resource, iff it exists. Only used when recreating resources.
 * @member m_ResourceSize [type: uint32_t] Resource size in memory. I.e. the payload of m_Resource
 */
struct ResourceDescriptor
{
    /// Hash of resource name
    uint64_t m_NameHash;
    /// Resource pointer. Must be unique and not NULL.
    void*    m_Resource;
    /// Resource pointer to a previous version of the resource, iff it exists. Only used when recreating resources.
    void*    m_PrevResource;
    /// Resource size in memory. I.e. the payload of m_Resource
    uint32_t m_ResourceSize;

    // private members
    HResourceType   m_ResourceType;
    uint32_t        m_ResourceSizeOnDisc;
    uint32_t        m_ReferenceCount;
    uint16_t        m_Version;
};

struct ResourceType
{
    ResourceType() // TODO: Will it be ok using C++ constructor, since this is a private header?
    {
        memset(this, 0, sizeof(*this));
        m_PreloadSize = RESOURCE_INVALID_PRELOAD_SIZE;
    }
    dmhash_t            m_ExtensionHash;
    const char*         m_Extension; // The suffix, without the '.'
    void*               m_Context;
    FResourcePreload    m_PreloadFunction;
    FResourceCreate     m_CreateFunction;
    FResourcePostCreate m_PostCreateFunction;
    FResourceDestroy    m_DestroyFunction;
    FResourceRecreate   m_RecreateFunction;
    uint32_t            m_PreloadSize;
    uint8_t             m_Index;
};

struct ResourceTypeContext
{
    HResourceFactory        m_Factory;
    dmHashTable64<void*>*   m_Contexts;
};

struct ResourcePreloadHintInfo
{
    HResourcePreloader      m_Preloader;
    int32_t                 m_Parent;
};

namespace dmResource
{

    Result CheckSuppliedResourcePath(const char* name);

#if !defined(DM_HAS_THREADS)
    // Only use for single threaded loading! (used in load_queue_sync.cpp)
    LoadBufferType* GetGlobalLoadBuffer(HFactory factory);
#endif

    // load with default internal buffer and its management, returns buffer ptr in 'buffer'
    Result LoadResource(HFactory factory, const char* path, const char* original_name, void** buffer, uint32_t* resource_size);

    // load directly to a user supplied buffer, and chunk size
    Result LoadResourceToBufferWithOffset(HFactory factory, const char* path, const char* original_name, uint32_t offset, uint32_t size, uint32_t* resource_size, uint32_t* buffer_size, LoadBufferType* buffer);

    Result InsertResource(HFactory factory, const char* path, uint64_t canonical_path_hash, HResourceDescriptor descriptor);

    HResourceType FindResourceType(HFactory factory, const char* extension);
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
    // Assets mapped with this function should be unmapped with UnmapAsset(...)
    Result MapAsset(const char* name, void*& out_asset, uint32_t& out_size, void*& out_map);
    Result UnmapAsset(void*& asset, uint32_t size);

    /**
     * In the case of an app-store upgrade, we dont want the runtime to load any existing local liveupdate.manifest.
     * We check this by persisting the bundled manifest signature to file the first time a liveupdate.manifest
     * is stored. At app start we check the current bundled manifest signature against the signature written to file.
     * If they don't match the bundle has changed, and we need to remove any liveupdate.manifest from the filesystem
     * and load the bundled manifest instead.
     */
    //Result BundleVersionValid(const Manifest* manifest, const char* bundle_ver_path);

}

#endif
