#include "res_camera.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResCameraCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CameraResource* cam_resource = new CameraResource();
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &cam_resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            delete cam_resource;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        resource->m_Resource = (void*) cam_resource;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCameraDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CameraResource* cam_resource = (CameraResource*)resource->m_Resource;
        dmDDF::FreeMessage((void*) cam_resource->m_DDF);
        delete cam_resource;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCameraRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmGamesysDDF::CameraDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        CameraResource* cam_resource = (CameraResource*)resource->m_Resource;
        dmDDF::FreeMessage((void*)cam_resource->m_DDF);
        cam_resource->m_DDF = ddf;
        return dmResource::CREATE_RESULT_OK;
    }
}
