#include "res_light.h"

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    dmResource::Result ResLightCreate(const dmResource::ResourceCreateParams& params)
    {
        dmGameSystemDDF::LightDesc* light_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &light_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::LightDesc** light_resource = new dmGameSystemDDF::LightDesc*;
        *light_resource = light_desc;
        params.m_Resource->m_Resource = (void*) light_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightDestroy(const dmResource::ResourceDestroyParams& params)
    {
        dmGameSystemDDF::LightDesc** light_resource = (dmGameSystemDDF::LightDesc**) params.m_Resource->m_Resource;
        dmDDF::FreeMessage(*light_resource);
        delete light_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameSystemDDF::LightDesc* light_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &light_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::LightDesc** light_resource = (dmGameSystemDDF::LightDesc**) params.m_Resource->m_Resource;
        dmDDF::FreeMessage(*light_resource);
        *light_resource = light_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        params.m_DataSize = sizeof(dmGameSystemDDF::LightDesc);
        return dmResource::RESULT_OK;
    }

}
