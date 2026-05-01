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

#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>
#include "resource_private.h"

/////////////////////////////////////////////////////////////////////

struct TypeCreatorDesc
{
    const char* m_Name;
    FResourceTypeRegister m_RegisterFn;
    FResourceTypeDeregister m_DeregisterFn;
    TypeCreatorDesc* m_Next;
};

void* ResourceTypeContextGetContextByHash(HResourceTypeContext ctx, dmhash_t name)
{
    void** out = ctx->m_Contexts->Get(name);
    if (out)
        return *out;
    return 0;
}

/////////////////////////////////////////////////////////////////////
void ResourceTypeReset(HResourceType type)
{
    memset(type, 0, sizeof(ResourceType));
    type->m_PreloadSize = RESOURCE_INVALID_PRELOAD_SIZE;
}

void ResourceTypeSetContext(HResourceType type, void* ctx)
{
    type->m_Context = ctx;
}

void* ResourceTypeGetContext(HResourceType type)
{
    return type->m_Context;
}

const char* ResourceTypeGetName(HResourceType type)
{
    return type->m_Extension;
}

dmhash_t ResourceTypeGetNameHash(HResourceType type)
{
    return type->m_ExtensionHash;
}

void ResourceTypeSetPreloadFn(HResourceType type, FResourcePreload fn)
{
    type->m_PreloadFunction = fn;
}

void ResourceTypeSetCreateFn(HResourceType type, FResourceCreate fn)
{
    type->m_CreateFunction = fn;
}

void ResourceTypeSetPostCreateFn(HResourceType type, FResourcePostCreate fn)
{
    type->m_PostCreateFunction = fn;
}

void ResourceTypeSetDestroyFn(HResourceType type, FResourceDestroy fn)
{
    type->m_DestroyFunction = fn;
}

void ResourceTypeSetRecreateFn(HResourceType type, FResourceRecreate fn)
{
    type->m_RecreateFunction = fn;
}

void ResourceTypeSetStreaming(HResourceType type, uint32_t preload_size)
{
    type->m_PreloadSize = preload_size;
}

bool ResourceTypeIsStreaming(HResourceType type)
{
    return type->m_PreloadSize != RESOURCE_INVALID_PRELOAD_SIZE;
}

uint32_t ResourceTypeGetPreloadSize(HResourceType type)
{
    return type->m_PreloadSize;
}

TypeCreatorDesc* g_ResourceTypeCreatorDescFirst = 0;

void ResourceRegisterTypeCreatorDesc(void* _desc,
                                    uint32_t desc_size,
                                    const char *name,
                                    FResourceTypeRegister register_fn,
                                    FResourceTypeDeregister deregister_fn)
{
    DM_STATIC_ASSERT(ResourceTypeCreatorDescBufferSize >= sizeof(struct TypeCreatorDesc), Invalid_Struct_Size);

    dmLogDebug("Registered resource type descriptor %s", name);
    TypeCreatorDesc* desc = (TypeCreatorDesc*)_desc;
    desc->m_Name = name;
    desc->m_RegisterFn = register_fn;
    desc->m_DeregisterFn = deregister_fn;
    desc->m_Next = g_ResourceTypeCreatorDescFirst;
    g_ResourceTypeCreatorDescFirst = desc;
}

namespace dmResource
{

void RegisterTypeCreatorDesc(void* desc,
                            uint32_t desc_size,
                            const char *name,
                            FResourceTypeRegister register_fn,
                            FResourceTypeDeregister deregister_fn)
{
    ResourceRegisterTypeCreatorDesc(desc, desc_size, name, register_fn, deregister_fn);
}

static const TypeCreatorDesc* GetFirstTypeCreatorDesc()
{
    return g_ResourceTypeCreatorDescFirst;
}


// add already registered types
Result RegisterTypes(HFactory factory, dmHashTable64<void*>* contexts)
{
    const TypeCreatorDesc* desc = GetFirstTypeCreatorDesc();
    while (desc)
    {
        if (contexts->Full()) {
            uint32_t capacity = contexts->Capacity() + 8;
            contexts->SetCapacity(capacity/2, capacity);
        }

        ResourceTypeContext ctx;
        ctx.m_Factory = factory;
        ctx.m_Contexts = contexts;

        HResourceType type = dmResource::AllocateResourceType(factory, desc->m_Name);
        type->m_Extension = desc->m_Name;
        type->m_ExtensionHash = dmHashString64(desc->m_Name);

        Result result = (Result)desc->m_RegisterFn(&ctx, type);
        if (result != RESULT_OK)
        {
            dmLogError("Failed to register type '%s': %s", desc->m_Name, ResultToString(result));
            return result;
        }

        result = dmResource::ValidateResourceType(type);
        if (dmResource::RESULT_OK != result)
        {
            dmResource::FreeResourceType(factory, type);
            dmLogDebug("Failed to register type '%s'", desc->m_Name);
        }
        else
        {
            dmLogDebug("Registered type '%s'", desc->m_Name);
        }
        desc = desc->m_Next;
    }
    return RESULT_OK;
}

Result DeregisterTypes(HFactory factory, dmHashTable64<void*>* contexts)
{
    const TypeCreatorDesc* desc = GetFirstTypeCreatorDesc();
    while (desc)
    {
        if (desc->m_DeregisterFn)
        {
            ResourceTypeContext ctx;
            ctx.m_Factory = factory;
            ctx.m_Contexts = contexts;

            HResourceType type = dmResource::FindResourceType(factory, desc->m_Name);
            Result result = (Result)desc->m_DeregisterFn(&ctx, type);
            if (result != RESULT_OK)
            {
                dmLogError("Failed to deregister type '%s': %s", desc->m_Name, ResultToString(result));
                return result;
            }
            dmLogDebug("Deregistered type '%s'", desc->m_Name);
        }

        desc = desc->m_Next;
    }
    return RESULT_OK;
}


} // namespace dmResource
