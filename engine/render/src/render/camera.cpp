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
    HRenderCamera NewRenderCamera(HRenderContext render_context)
    {
        if (render_context->m_RenderCameras.Full())
        {
            render_context->m_RenderCameras.Allocate(4);
        }

        RenderCamera* camera = new RenderCamera();
        camera->m_URL        = dmMessage::URL();
        camera->m_Handle     = render_context->m_RenderCameras.Put(camera);

        memset(&camera->m_Data, 0, sizeof(camera->m_Data));
        camera->m_Data.m_Viewport = dmVMath::Vector4(0.0f, 0.0f, 1.0f, 1.0f);

        return camera->m_Handle;
    }

    void DeleteRenderCamera(HRenderContext context, HRenderCamera camera)
    {
        RenderCamera* c = context->m_RenderCameras.Get(camera);
        if (c)
        {
            delete c;
            context->m_RenderCameras.Release(camera);
        }
    }

    void SetRenderCameraURL(HRenderContext render_context, HRenderCamera camera, const dmMessage::URL& camera_url)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            c->m_URL = camera_url;
        }
    }

    void SetRenderCameraData(HRenderContext render_context, HRenderCamera camera, RenderCameraData data)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            c->m_Data = data;
        }
    }

    void SetRenderCameraMatrices(HRenderContext render_context, HRenderCamera camera, const dmVMath::Matrix4 view, const dmVMath::Matrix4 projection)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            c->m_View           = view;
            c->m_Projection     = projection;
            c->m_ViewProjection = projection * view;
        }
    }

    void SetRenderCameraMainCamera(HRenderContext render_context, HRenderCamera camera)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            for (int i = 0; i < render_context->m_RenderCameras.Capacity(); ++i)
            {
                RenderCamera* c_other = render_context->m_RenderCameras.GetByIndex(i);
                if (c_other == NULL)
                {
                    continue;
                }
                if (c_other == c)
                {
                    c_other->m_IsMainCamera = 1;
                    RenderScriptCameraSetMainCamera(c_other->m_URL);
                }
                else
                {
                    c_other->m_IsMainCamera = 0;
                }
            }
        }
    }

    RenderCameraData GetRenderCameraData(HRenderContext render_context, HRenderCamera camera)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            return c->m_Data;
        }

        return {};
    }

    // render_private.h
    RenderCamera* GetRenderCameraByUrl(HRenderContext render_context, const dmMessage::URL& camera_url)
    {
        for (int i = 0; i < render_context->m_RenderCameras.Capacity(); ++i)
        {
            RenderCamera* c = render_context->m_RenderCameras.GetByIndex(i);
            if (c && memcmp(&c->m_URL, &camera_url, sizeof(dmMessage::URL)) == 0)
            {
                return c;
            }
        }
        return 0x0;
    }
}
