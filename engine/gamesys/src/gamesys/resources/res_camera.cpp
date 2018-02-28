#include "res_camera.h"

namespace dmGameSystem
{
    dmResource::Result ResCameraCreate(const dmResource::ResourceCreateParams& params)
    {
        CameraResource* cam_resource = new CameraResource();
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &cam_resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            delete cam_resource;
            return dmResource::RESULT_FORMAT_ERROR;
        }
        params.m_Resource->m_Resource = (void*) cam_resource;

        params.m_Resource->m_ResourceSize = sizeof(CameraResource) + params.m_BufferSize;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCameraDestroy(const dmResource::ResourceDestroyParams& params)
    {
        CameraResource* cam_resource = (CameraResource*)params.m_Resource->m_Resource;
        dmDDF::FreeMessage((void*) cam_resource->m_DDF);
        delete cam_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCameraRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGamesysDDF::CameraDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        CameraResource* cam_resource = (CameraResource*)params.m_Resource->m_Resource;
        dmDDF::FreeMessage((void*)cam_resource->m_DDF);
        cam_resource->m_DDF = ddf;
        params.m_Resource->m_ResourceSize = sizeof(CameraResource) + params.m_BufferSize;
        return dmResource::RESULT_OK;
    }
}
