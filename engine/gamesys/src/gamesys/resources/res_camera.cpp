#include "res_camera.h"

namespace dmGameSystem
{
    dmResource::Result ResCameraCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CameraResource* cam_resource = new CameraResource();
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &cam_resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            delete cam_resource;
            return dmResource::RESULT_FORMAT_ERROR;
        }
        resource->m_Resource = (void*) cam_resource;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCameraDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CameraResource* cam_resource = (CameraResource*)resource->m_Resource;
        dmDDF::FreeMessage((void*) cam_resource->m_DDF);
        delete cam_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCameraRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmGamesysDDF::CameraDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        CameraResource* cam_resource = (CameraResource*)resource->m_Resource;
        dmDDF::FreeMessage((void*)cam_resource->m_DDF);
        cam_resource->m_DDF = ddf;
        return dmResource::RESULT_OK;
    }
}
