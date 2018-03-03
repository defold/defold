
#include "res_collection_factory.h"

#include <gameobject/gameobject_ddf.h>
#include "gamesys_ddf.h"


namespace dmGameSystem
{
    static dmResource::Result AcquireCollectionDesc(dmResource::HFactory factory, dmGameSystemDDF::CollectionFactoryDesc* desc, CollectionFactoryResource* factory_res)
    {
        // get raw ddf
        void *msg;
        uint32_t msg_size;
        dmResource::Result r = dmResource::GetRaw(factory, desc->m_Prototype, &msg, &msg_size);
        if (r != dmResource::RESULT_OK) {
            dmLogError("failed to load collection prototype [%s]", desc->m_Prototype);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }
        // construct desc info from ddf
        factory_res->m_LoadDynamically = desc->m_LoadDynamically;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(msg, msg_size, (dmGameObjectDDF::CollectionDesc**)&factory_res->m_CollectionDesc);
        free(msg);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to parse collection prototype [%s]", desc->m_Prototype);
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

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::CollectionFactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        memset(factory_res, 0, sizeof(CollectionFactoryResource));
        dmResource::Result r = AcquireCollectionDesc(params.m_Factory, ddf, factory_res);
        dmDDF::FreeMessage(ddf);
        if (r != dmResource::RESULT_OK)
        {
            delete factory_res;
            return dmResource::RESULT_DDF_ERROR;
        }

        if((!factory_res->m_LoadDynamically) && (params.m_HintInfo))
        {
            dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) factory_res->m_CollectionDesc;
            for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
            {
                const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
                if (instance_desc.m_Prototype == 0x0)
                    continue;
                dmResource::PreloadHint(params.m_HintInfo, instance_desc.m_Prototype);
            }
        }

        *params.m_PreloadData = factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*)params.m_PreloadData;
        dmResource::Result res = AcquireResources(params.m_Factory, factory_res);
        if(res == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) factory_res;
            params.m_Resource->m_ResourceSize = sizeof(CollectionFactoryResource) + (factory_res->m_CollectionResources.Size()*sizeof(void*)) + params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, factory_res);
            ReleaseCollectionDesc(params.m_Factory, factory_res);
            delete factory_res;
        }
        return res;
    }

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams& params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, factory_res);
        ReleaseCollectionDesc(params.m_Factory, factory_res);
        delete factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameSystemDDF::CollectionFactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        CollectionFactoryResource tmp_factory_res;
        memset(&tmp_factory_res, 0, sizeof(CollectionFactoryResource));
        dmResource::Result r = AcquireCollectionDesc(params.m_Factory, ddf, &tmp_factory_res);
        dmDDF::FreeMessage(ddf);
        if (r == dmResource::RESULT_OK)
        {
            r = AcquireResources(params.m_Factory, &tmp_factory_res);
        }
        if (r == dmResource::RESULT_OK)
        {
            CollectionFactoryResource* factory_res = (CollectionFactoryResource*) params.m_Resource->m_Resource;
            ReleaseResources(params.m_Factory, factory_res);
            ReleaseCollectionDesc(params.m_Factory, factory_res);
            *factory_res = tmp_factory_res;
            params.m_Resource->m_ResourceSize = sizeof(CollectionFactoryResource) + (factory_res->m_CollectionResources.Size()*sizeof(void*)) + params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_factory_res);
            ReleaseCollectionDesc(params.m_Factory, &tmp_factory_res);
        }
        return r;
    }

    dmResource::Result ResCollectionFactoryGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        CollectionFactoryResource* res  = (CollectionFactoryResource*) params.m_Resource->m_Resource;
        params.m_SubResourceIds->SetCapacity(res->m_CollectionResources.Size());
        for (uint32_t i = 0; i < res->m_CollectionResources.Size(); ++i)
        {
            dmhash_t res_hash;
            if(dmResource::GetPath(params.m_Factory, res->m_CollectionResources[i], &res_hash)==dmResource::RESULT_OK)
            {
                params.m_SubResourceIds->Push(res_hash);
            }
        }
        return dmResource::RESULT_OK;
    }

}
