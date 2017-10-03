
#include "res_collection_factory.h"

#include <gameobject/gameobject_ddf.h>
#include "gamesys_ddf.h"


namespace dmGameSystem
{
    static dmResource::Result CreateCollectionDesc(dmResource::HFactory factory, dmGameSystemDDF::CollectionFactoryDesc* desc, CollectionFactoryResource* factory_res)
    {
        void *msg;
        uint32_t msg_size;
        dmResource::Result r = dmResource::GetRaw(factory, desc->m_Prototype, &msg, &msg_size);
        if (r != dmResource::RESULT_OK) {
            dmLogError("failed to load collection prototype [%s]", desc->m_Prototype);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(msg, msg_size, (dmGameObjectDDF::CollectionDesc**)&factory_res->m_CollectionDesc);
        free(msg);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to parse collection prototype [%s]", desc->m_Prototype);
            return dmResource::RESULT_DDF_ERROR;
        }
        return dmResource::RESULT_OK;
    }

    dmResource::Result AcquireResource(dmResource::HFactory factory, dmGameSystemDDF::CollectionFactoryDesc* desc, CollectionFactoryResource* factory_res)
    {
        dmResource::Result r = CreateCollectionDesc(factory, desc, factory_res);
        return r;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionFactoryResource* factory_res)
    {
        if (factory_res->m_CollectionDesc != 0x0)
        {
            dmDDF::FreeMessage(factory_res->m_CollectionDesc);
            factory_res->m_CollectionDesc = 0;
        }
    }

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::CollectionFactoryDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        memset(factory_res, 0, sizeof(CollectionFactoryResource));
        factory_res->m_LoadDynamically = ddf->m_LoadDynamically;

        dmResource::Result r = CreateCollectionDesc(params.m_Factory, ddf, factory_res);
        dmDDF::FreeMessage(ddf);
        if (r != dmResource::RESULT_OK)
        {
            ReleaseResources(params.m_Factory, factory_res);
            delete factory_res;
            return dmResource::RESULT_DDF_ERROR;
        }

        if(!factory_res->m_LoadDynamically)
        {
            if(params.m_HintInfo)
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
        }

        *params.m_PreloadData = factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*)params.m_PreloadData;
        params.m_Resource->m_Resource = (void*) factory_res;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryDestroy(const dmResource::ResourceDestroyParams& params)
    {
        CollectionFactoryResource* factory_res = (CollectionFactoryResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, factory_res);
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
        dmResource::Result r = AcquireResource(params.m_Factory, ddf, &tmp_factory_res);
        dmDDF::FreeMessage(ddf);
        if (r == dmResource::RESULT_OK)
        {
            CollectionFactoryResource* factory_res = (CollectionFactoryResource*) params.m_Resource->m_Resource;
            ReleaseResources(params.m_Factory, factory_res);
            *factory_res = tmp_factory_res;
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_factory_res);
        }
        return r;
    }
}
