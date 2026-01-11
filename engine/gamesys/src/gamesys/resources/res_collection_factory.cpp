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


#include "res_collection_factory.h"

#include <dmsdk/dlib/log.h>
#include <resource/resource.h>
#include <gameobject/gameobject_ddf.h>
#include <gamesys/gamesys_ddf.h>

namespace dmGameSystem
{
    CollectionFactoryResource& CollectionFactoryResource::operator=(CollectionFactoryResource& other)
    {
        m_CollectionDesc = other.m_CollectionDesc;
        m_CollectionResources.Swap(other.m_CollectionResources);
        m_LoadDynamically = other.m_LoadDynamically;
        return *this;
    }

    // load the .collectionc file
    static dmResource::Result AcquireCollectionDesc(dmResource::HFactory factory, const char* prototype, dmGameObjectDDF::CollectionDesc** out_desc)
    {
        // get raw ddf
        void *msg;
        uint32_t msg_size;
        dmResource::Result r = dmResource::GetRaw(factory, prototype, &msg, &msg_size);
        if (r != dmResource::RESULT_OK) {
            dmLogError("failed to load collection prototype [%s]", prototype);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(msg, msg_size, out_desc);
        free(msg);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to parse collection prototype [%s]", prototype);
            return dmResource::RESULT_DDF_ERROR;
        }
        return dmResource::RESULT_OK;
    }

    static void ReleaseCollectionDesc(dmResource::HFactory factory, CollectionFactoryResource* factory_res)
    {
        if (factory_res->m_CollectionDesc != 0x0)
        {
            dmDDF::FreeMessage(factory_res->m_CollectionDesc);
            factory_res->m_CollectionDesc = 0;
        }
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, CollectionFactoryResource* factory_res)
    {
        if(factory_res->m_LoadDynamically)
        {
            return dmResource::RESULT_OK;
        }
        // If the resource isn't loaded, the dmGameObject::SpawnFromCollection() will do it synchronously
        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) factory_res->m_CollectionDesc;
        uint32_t instance_count = collection_desc->m_Instances.m_Count;
        if(!instance_count)
        {
            return dmResource::RESULT_OK;
        }
        factory_res->m_CollectionResources.SetCapacity(instance_count);
        for (uint32_t i = 0; i < instance_count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            if (instance_desc.m_Prototype == 0x0)
            {
                continue;
            }
            void* resource;
            dmResource::Result r = dmResource::Get(factory, instance_desc.m_Prototype, &resource);
            if(r != dmResource::RESULT_OK)
            {
                return r;
            }
            factory_res->m_CollectionResources.Push(resource);
        }
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, CollectionFactoryResource* factory_res)
    {
        for (uint32_t i = 0; i < factory_res->m_CollectionResources.Size(); ++i)
        {
            dmResource::Release(factory, factory_res->m_CollectionResources[i]);
        }
        factory_res->m_CollectionResources.SetSize(0);
    }

    static dmResource::Result LoadResourceFromMemory(dmResource::HFactory factory, const uint8_t* buffer, uint32_t buffer_size, CollectionFactoryResource** out_res)
    {
        dmGameSystemDDF::CollectionFactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        memset(factory_res, 0, sizeof(CollectionFactoryResource));
        factory_res->m_LoadDynamically = ddf->m_LoadDynamically;
        factory_res->m_DynamicPrototype = ddf->m_DynamicPrototype;
        factory_res->m_PrototypePathHash = dmHashString64(ddf->m_Prototype);
        dmResource::Result r = AcquireCollectionDesc(factory, ddf->m_Prototype, (dmGameObjectDDF::CollectionDesc**)&factory_res->m_CollectionDesc);
        dmDDF::FreeMessage(ddf);
        *out_res = factory_res;
        return r;
    }

    // Loads a .collectionc into a resource description that the collection factory can use
    // The resources of the collection aren't actually loaded
    dmResource::Result ResCollectionFactoryLoadResource(dmResource::HFactory factory, const char* collectionc, bool load_dynamically, bool dynamic_prototype, CollectionFactoryResource** out_res)
    {
        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        memset(factory_res, 0, sizeof(CollectionFactoryResource));
        factory_res->m_LoadDynamically = load_dynamically;
        factory_res->m_DynamicPrototype = dynamic_prototype;
        factory_res->m_PrototypePathHash = dmHashString64(collectionc);
        dmResource::Result r = AcquireCollectionDesc(factory, collectionc, (dmGameObjectDDF::CollectionDesc**)&factory_res->m_CollectionDesc);
        *out_res = factory_res;
        return r;
    }

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams* params)
    {
        CollectionFactoryResource* factory_res = 0;
        dmResource::Result r = LoadResourceFromMemory(params->m_Factory, (const uint8_t*)params->m_Buffer, params->m_BufferSize, &factory_res);

        if (r != dmResource::RESULT_OK)
        {
            delete factory_res;
            return dmResource::RESULT_DDF_ERROR;
        }

        if((!factory_res->m_LoadDynamically) && (params->m_HintInfo))
        {
            dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) factory_res->m_CollectionDesc;
            for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
            {
                const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
                if (instance_desc.m_Prototype == 0x0)
                    continue;
                dmResource::PreloadHint(params->m_HintInfo, instance_desc.m_Prototype);
            }
        }

        *params->m_PreloadData = factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams* params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*)params->m_PreloadData;
        dmResource::Result res = AcquireResources(params->m_Factory, factory_res);
        if(res == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, factory_res);
            dmResource::SetResourceSize(params->m_Resource, sizeof(CollectionFactoryResource) + (factory_res->m_CollectionResources.Size()*sizeof(void*)) + params->m_BufferSize);
        }
        else
        {
            ReleaseResources(params->m_Factory, factory_res);
            ReleaseCollectionDesc(params->m_Factory, factory_res);
            delete factory_res;
        }
        return res;
    }

    void ResCollectionFactoryDestroyResource(dmResource::HFactory factory, CollectionFactoryResource* resource)
    {
        ReleaseResources(factory, resource);
        ReleaseCollectionDesc(factory, resource);
        delete resource;
    }

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams* params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*) dmResource::GetResource(params->m_Resource);
        ResCollectionFactoryDestroyResource(params->m_Factory, factory_res);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryRecreate(const dmResource::ResourceRecreateParams* params)
    {
        CollectionFactoryResource* tmp_factory_res = 0;
        dmResource::Result r = LoadResourceFromMemory(params->m_Factory, (const uint8_t*)params->m_Buffer, params->m_BufferSize, &tmp_factory_res);

        if (r == dmResource::RESULT_OK)
        {
            r = AcquireResources(params->m_Factory, tmp_factory_res);
        }
        if (r == dmResource::RESULT_OK)
        {
            CollectionFactoryResource* factory_res = (CollectionFactoryResource*) dmResource::GetResource(params->m_Resource);
            ReleaseResources(params->m_Factory, factory_res);
            ReleaseCollectionDesc(params->m_Factory, factory_res);
            *factory_res = *tmp_factory_res;
            delete tmp_factory_res;
            dmResource::SetResourceSize(params->m_Resource, sizeof(CollectionFactoryResource) + (factory_res->m_CollectionResources.Size()*sizeof(void*)) + params->m_BufferSize);
        }
        else
        {
            ResCollectionFactoryDestroyResource(params->m_Factory, tmp_factory_res);
        }
        return r;
    }
}
