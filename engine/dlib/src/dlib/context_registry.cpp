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

#include <dlib/context_registry.h>

#include <dlib/hashtable.h>

struct ContextRegistry
{
    dmHashTable64<void*> m_Contexts;
};

static void EnsureSize(dmHashTable64<void*>* contexts)
{
    if (contexts->Full())
    {
        contexts->OffsetCapacity(4);
    }
}

HContextRegistry ContextRegistryCreate()
{
    return new ContextRegistry;
}

void ContextRegistryDestroy(HContextRegistry registry)
{
    delete registry;
}

int ContextRegistrySet(HContextRegistry registry, const char* name, void* context)
{
    return ContextRegistrySetByHash(registry, dmHashString64(name), context);
}

int ContextRegistrySetByHash(HContextRegistry registry, dmhash_t name_hash, void* context)
{
    if (context)
    {
        EnsureSize(&registry->m_Contexts);
        registry->m_Contexts.Put(name_hash, context);
    }
    else
    {
        void** pvalue = registry->m_Contexts.Get(name_hash);
        if (pvalue)
            registry->m_Contexts.Erase(name_hash);
    }
    return 0;
}

void* ContextRegistryGet(HContextRegistry registry, const char* name)
{
    return ContextRegistryGetByHash(registry, dmHashString64(name));
}

void* ContextRegistryGetByHash(HContextRegistry registry, dmhash_t name_hash)
{
    void** pcontext = registry->m_Contexts.Get(name_hash);
    if (pcontext != 0)
    {
        return *pcontext;
    }
    return 0;
}
