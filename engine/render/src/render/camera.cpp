// Copyright 2020-2024 The Defold Foundation
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

#include <assert.h>
#include <dlib/hash.h>
#include "render_private.h"

namespace dmRender
{
    HRenderCamera NewRenderCamera(HRenderContext render_context, const dmMessage::URL& camera_url)
    {
        HRenderCamera c = GetRenderCameraByUrl(render_context, camera_url);
        if (c != INVALID_RENDER_CAMERA)
        {
            return INVALID_RENDER_CAMERA;
        }

        if (render_context->m_RenderCameras.Full())
        {
            render_context->m_RenderCameras.OffsetCapacity(4);
        }

        RenderCamera camera = {};
        camera.m_URL = camera_url;
        render_context->m_RenderCameras.Push(camera);

        return (HRenderCamera)(render_context->m_RenderCameras.Size() - 1);
    }

    HRenderCamera GetRenderCameraByUrl(HRenderContext render_context, const dmMessage::URL& camera_url)
    {
        for (int i = 0; i < render_context->m_RenderCameras.Size(); ++i)
        {
            if (render_context->m_RenderCameras[i].m_URL.m_Socket   == camera_url.m_Socket &&
                render_context->m_RenderCameras[i].m_URL.m_Path     == camera_url.m_Path &&
                render_context->m_RenderCameras[i].m_URL.m_Fragment == camera_url.m_Fragment)
            {
                return i;
            }
        }
        return INVALID_RENDER_CAMERA;
    }

    void SetRenderCameraData(HRenderContext render_context, HRenderCamera camera, const dmVMath::Matrix4& view, const dmVMath::Matrix4& projection)
    {
        render_context->m_RenderCameras[camera].m_View       = view;
        render_context->m_RenderCameras[camera].m_Projection = projection;
    }
}
