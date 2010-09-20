#include "res_camera.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResCameraCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CameraProperties* properties = new CameraProperties();
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmCameraDDF_CameraDesc_DESCRIPTOR, (void**) &properties->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            delete properties;
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        resource->m_Resource = (void*) properties;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCameraDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        CameraProperties* properties = (CameraProperties*)resource->m_Resource;
        dmDDF::FreeMessage((void*) properties->m_DDF);
        delete properties;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResCameraRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        CameraProperties* properties = (CameraProperties*)resource->m_Resource;
        dmDDF::FreeMessage((void*)properties->m_DDF);
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmCameraDDF_CameraDesc_DESCRIPTOR, (void**) &properties->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        return dmResource::CREATE_RESULT_OK;
    }
}
