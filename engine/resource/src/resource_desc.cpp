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

#include <dmsdk/resource/resource.h>
#include "resource_private.h"

dmhash_t ResourceDescriptorGetNameHash(HResourceDescriptor rd)
{
    return rd->m_NameHash;
}

void ResourceDescriptorSetResource(HResourceDescriptor rd, void* resource)
{
    rd->m_Resource = resource;
}

void* ResourceDescriptorGetResource(HResourceDescriptor rd)
{
    return rd->m_Resource;
}

void ResourceDescriptorSetPrevResource(HResourceDescriptor rd, void* resource)
{
    rd->m_PrevResource = resource;
}

void* ResourceDescriptorGetPrevResource(HResourceDescriptor rd)
{
    return rd->m_PrevResource;
}

void ResourceDescriptorSetResourceSize(HResourceDescriptor rd, uint32_t size)
{
    rd->m_ResourceSize = size;
}

uint32_t ResourceDescriptorGetResourceSize(HResourceDescriptor rd)
{
    return rd->m_ResourceSize;
}

HResourceType ResourceDescriptorGetType(HResourceDescriptor rd)
{
    return rd->m_ResourceType;
}

namespace dmResource
{
    dmhash_t GetNameHash(HResourceDescriptor rd)
    {
        return ResourceDescriptorGetNameHash(rd);
    }
    void SetResource(HResourceDescriptor rd, void* resource)
    {
        ResourceDescriptorSetResource(rd, resource);
    }
    void* GetResource(HResourceDescriptor rd)
    {
        return ResourceDescriptorGetResource(rd);
    }
    void SetPrevResource(HResourceDescriptor rd, void* resource)
    {
        ResourceDescriptorSetPrevResource(rd, resource);
    }
    void* GetPrevResource(HResourceDescriptor rd)
    {
        return ResourceDescriptorGetPrevResource(rd);
    }
    void SetResourceSize(HResourceDescriptor rd, uint32_t size)
    {
        ResourceDescriptorSetResourceSize(rd, size);
    }
    uint32_t GetResourceSize(HResourceDescriptor rd)
    {
        return ResourceDescriptorGetResourceSize(rd);
    }
    HResourceType GetType(HResourceDescriptor rd)
    {
        return ResourceDescriptorGetType(rd);
    }
}
