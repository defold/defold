#include "res_light.h"

#include "gamesys_ddf.h"

namespace dmGameSystem
{
    dmResource::Result ResLightCreate(dmResource::HFactory factory,
                                            void* context,
                                            const void* buffer, uint32_t buffer_size,
                                            void* preload_data,
                                            dmResource::SResourceDescriptor* resource,
                                            const char* filename)
    {
        dmGameSystemDDF::LightDesc* light_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &light_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::LightDesc** light_resource = new dmGameSystemDDF::LightDesc*;
        *light_resource = light_desc;
        resource->m_Resource = (void*) light_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightDestroy(dmResource::HFactory factory,
                                             void* context,
                                             dmResource::SResourceDescriptor* resource)
    {
        dmGameSystemDDF::LightDesc** light_resource = (dmGameSystemDDF::LightDesc**) resource->m_Resource;
        dmDDF::FreeMessage(*light_resource);
        delete light_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLightRecreate(dmResource::HFactory factory,
                                              void* context,
                                              const void* buffer, uint32_t buffer_size,
                                              dmResource::SResourceDescriptor* resource,
                                              const char* filename)
    {
        dmGameSystemDDF::LightDesc* light_desc;
        dmDDF::Result e  = dmDDF::LoadMessage(buffer, buffer_size, &light_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmGameSystemDDF::LightDesc** light_resource = (dmGameSystemDDF::LightDesc**) resource->m_Resource;
        dmDDF::FreeMessage(*light_resource);
        *light_resource = light_desc;
        return dmResource::RESULT_OK;
    }
}
