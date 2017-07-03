#include "res_collection_factory.h"

#include <gameobject/gameobject_ddf.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResource(dmResource::HFactory factory, dmGameSystemDDF::CollectionFactoryDesc* desc, CollectionFactoryResource* factory_res)
    {
        factory_res->m_CollectionFactoryDesc = desc;

        void *msg;
        uint32_t msg_size;
        dmResource::Result r = dmResource::GetRaw(factory, factory_res->m_CollectionFactoryDesc->m_Prototype, &msg, &msg_size);
        if (r != dmResource::RESULT_OK) {
            dmLogError("failed to load collection [%s]", factory_res->m_CollectionFactoryDesc->m_Prototype);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(msg, msg_size, (dmGameObjectDDF::CollectionDesc**)&factory_res->m_CollectionDesc);
        if (e != dmDDF::RESULT_OK)
        {
            dmLogError("Failed to parse collection [%s]", factory_res->m_CollectionFactoryDesc->m_Prototype);
            return dmResource::RESULT_DDF_ERROR;
        }
        return dmResource::RESULT_OK;
    }

    void ReleaseResources(dmResource::HFactory factory, CollectionFactoryResource* factory_res)
    {
        if (factory_res->m_CollectionFactoryDesc != 0x0)
            dmDDF::FreeMessage(factory_res->m_CollectionFactoryDesc);
    }

    dmResource::Result ResCollectionFactoryPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::CollectionFactoryDesc* ddf;

        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
            return dmResource::RESULT_FORMAT_ERROR;

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Prototype);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionFactoryCreate(const dmResource::ResourceCreateParams& params)
    {
        CollectionFactoryResource* factory_res = new CollectionFactoryResource;
        dmResource::Result r = AcquireResource(params.m_Factory, (dmGameSystemDDF::CollectionFactoryDesc*)params.m_PreloadData, factory_res);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) factory_res;
        }
        else
        {
            ReleaseResources(params.m_Factory, factory_res);
        }
        return r;
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
        dmResource::Result r = AcquireResource(params.m_Factory, ddf, &tmp_factory_res);
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
