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

#ifndef DM_RESOURCE_MOUNTS_H
#define DM_RESOURCE_MOUNTS_H

#include "resource.h"
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/mutex.h>

namespace dmResourceProvider
{
    typedef struct Archive*         HArchive;
}

namespace dmResourceMounts
{
    typedef struct ResourceMountsContext* HContext;

    HContext    Create(dmResourceProvider::HArchive base_archive);
    void        Destroy(HContext context);

    dmMutex::HMutex GetMutex(HContext ctx);

    dmResource::Result AddMount(HContext ctx, const char* name, dmResourceProvider::HArchive archive, int priority, bool persist);
    dmResource::Result RemoveMount(HContext ctx, dmResourceProvider::HArchive archive);
    dmResource::Result RemoveMountByName(HContext ctx, const char* name);

    dmResource::Result LoadMounts(HContext ctx, const char* app_support_path);
    dmResource::Result SaveMounts(HContext ctx, const char* app_support_path);

    dmResource::Result ResourceExists(HContext ctx, dmhash_t path_hash);
    dmResource::Result GetResourceSize(HContext ctx, dmhash_t path_hash, const char* path, uint32_t* resource_size);
    dmResource::Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_size);
    dmResource::Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, dmArray<char>* buffer);

    struct SGetMountResult
    {
        const char*                  m_Name;
        dmResourceProvider::HArchive m_Archive;
        int                          m_Priority;
        uint8_t                      m_Persist:1;
    };

    uint32_t GetNumMounts(HContext ctx);
    dmResource::Result GetMountByName(HContext ctx, const char* name, SGetMountResult* mount_info);
    dmResource::Result GetMountByIndex(HContext ctx, uint32_t index, SGetMountResult* mount_info);

    struct SGetDependenciesParams
    {
        dmhash_t m_UrlHash; // The requested url
        bool m_OnlyMissing; // Only report assets that aren't available in the mounts
        bool m_Recursive;   // Traverse down for each resource that has dependencies
    };

    struct SGetDependenciesResult
    {
        dmhash_t m_UrlHash;
        uint8_t* m_HashDigest;
        uint32_t m_HashDigestLength;
        bool     m_Missing;
    };

    typedef void (*FGetDependency)(void* context, const SGetDependenciesResult* result);
    dmResource::Result GetDependencies(HContext ctx, const SGetDependenciesParams* request, FGetDependency callback, void* callback_context);
}

#endif // DM_RESOURCE_MOUNTS_H
