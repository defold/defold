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

namespace dmResourceMounts
{
    typedef struct ResourceMountsContext* HContext;

    HContext    Create();
    void        Destroy(HContext context);

    dmResource::Result AddMount(HContext ctx, dmResourceProvider::HArchive archive, int priority);
    dmResource::Result RemoveMount(HContext ctx, dmResourceProvider::HArchive archive);
    dmResource::Result DestroyArchives(HContext ctx);

    dmResource::Result GetResourceSize(HContext ctx, dmhash_t path_hash, const char* path, uint32_t* resource_size);
    dmResource::Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, uint8_t* buffer, uint32_t buffer_size);
    dmResource::Result ReadResource(HContext ctx, dmhash_t path_hash, const char* path, dmResource::LoadBufferType* buffer);
}

#endif // DM_RESOURCE_MOUNTS_H
