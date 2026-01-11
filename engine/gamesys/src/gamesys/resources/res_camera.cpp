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

#include "res_camera.h"

namespace dmGameSystem
{
    dmResource::Result ResCameraCreate(const dmResource::ResourceCreateParams* params)
    {
        CameraResource* cam_resource = new CameraResource();
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &cam_resource->m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            delete cam_resource;
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::SetResource(params->m_Resource, cam_resource);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCameraDestroy(const dmResource::ResourceDestroyParams* params)
    {
        CameraResource* cam_resource = (CameraResource*)dmResource::GetResource(params->m_Resource);
        dmDDF::FreeMessage((void*) cam_resource->m_DDF);
        delete cam_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCameraRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmGamesysDDF::CameraDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmGamesysDDF_CameraDesc_DESCRIPTOR, (void**) &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        CameraResource* cam_resource = (CameraResource*)dmResource::GetResource(params->m_Resource);
        dmDDF::FreeMessage((void*)cam_resource->m_DDF);
        cam_resource->m_DDF = ddf;
        return dmResource::RESULT_OK;
    }
}
