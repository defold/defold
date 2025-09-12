// Copyright 2020-2025 The Defold Foundation
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
        camera->m_Data.m_OrthographicZoomMode = ORTHO_ZOOM_MODE_FIXED;

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

    void SetRenderCameraURL(HRenderContext render_context, HRenderCamera camera, const dmMessage::URL* camera_url)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            c->m_URL = *camera_url;
        }
    }

    void SetRenderCameraData(HRenderContext render_context, HRenderCamera camera, const RenderCameraData* data)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            c->m_Data = *data;
        }
    }

    void GetRenderCameraView(HRenderContext render_context, HRenderCamera camera, dmVMath::Matrix4* mtx)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            *mtx = c->m_View;
        }
    }

    void GetRenderCameraProjection(HRenderContext render_context, HRenderCamera camera, dmVMath::Matrix4* mtx)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            *mtx = c->m_Projection;
        }
    }

    float GetRenderCameraEffectiveAspectRatio(HRenderContext render_context, HRenderCamera camera)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);

        if (c->m_Data.m_AutoAspectRatio)
        {
            dmGraphics::HContext graphics_context = GetGraphicsContext(render_context);
            float width = (float) dmGraphics::GetWindowWidth(graphics_context);
            float height = (float) dmGraphics::GetWindowHeight(graphics_context);
            return width / height;
        }
        else
        {
            return c->m_Data.m_AspectRatio;
        }
    }

    void UpdateRenderCamera(HRenderContext render_context, HRenderCamera camera, const dmVMath::Point3* position, const dmVMath::Quat* rotation)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (!c)
        {
            return;
        }

        dmGraphics::HContext graphics_context = GetGraphicsContext(render_context);
        float width        = (float) dmGraphics::GetWindowWidth(graphics_context);
        float height       = (float) dmGraphics::GetWindowHeight(graphics_context);
        float aspect_ratio = c->m_Data.m_AspectRatio;

        if (c->m_Data.m_AutoAspectRatio)
        {
            aspect_ratio = width / height;
        }

        if (c->m_Data.m_OrthographicProjection)
        {
            float display_scale = dmGraphics::GetDisplayScaleFactor(graphics_context);

            // Determine zoom
            float zoom = c->m_Data.m_OrthographicZoom;

            // Project (reference) size from game.project
            float proj_width = (float) dmGraphics::GetWidth(graphics_context);
            float proj_height = (float) dmGraphics::GetHeight(graphics_context);

            // Fallbacks to avoid division by zero
            if (proj_width <= 0.0f)
            {
            	proj_width = 1.0f;
            }
            if (proj_height <= 0.0f)
            {
            	proj_height = 1.0f;
            }

            // Compute auto zoom if requested
            if (c->m_Data.m_OrthographicZoomMode == ORTHO_ZOOM_MODE_AUTO_FIT || c->m_Data.m_OrthographicZoomMode == ORTHO_ZOOM_MODE_AUTO_COVER)
            {
                float zx = width / (display_scale * proj_width);
                float zy = height / (display_scale * proj_height);

                if (c->m_Data.m_OrthographicZoomMode == ORTHO_ZOOM_MODE_AUTO_FIT)
                {
                    zoom = (zx < zy) ? zx : zy;
                }
                else if (c->m_Data.m_OrthographicZoomMode == ORTHO_ZOOM_MODE_AUTO_COVER)
                {
                    zoom = (zx > zy) ? zx : zy;
                }

                if (zoom <= 0.0f)
                {
                    zoom = 1.0f;
                }
            }

            float zoomed_width = width / display_scale / zoom;
            float zoomed_height = height / display_scale / zoom;

            float left = -zoomed_width / 2;
            float right = zoomed_width / 2;
            float bottom = -zoomed_height / 2;
            float top = zoomed_height / 2;
            c->m_Projection = Matrix4::orthographic(left, right, bottom, top, c->m_Data.m_NearZ, c->m_Data.m_FarZ);
        }
        else
        {
            c->m_Projection = Matrix4::perspective(c->m_Data.m_Fov, aspect_ratio, c->m_Data.m_NearZ, c->m_Data.m_FarZ);
        }

        const Point3 pos    = *position;
        const Quat q        = *rotation;
        Point3 look_at      = pos + dmVMath::Rotate(q, dmVMath::Vector3(0.0f, 0.0f, -1.0f));
        Vector3 up          = dmVMath::Rotate(q, dmVMath::Vector3(0.0f, 1.0f, 0.0f));
        c->m_View           = Matrix4::lookAt(pos, look_at, up);
        c->m_ViewProjection = c->m_Projection * c->m_View;
        c->m_LastPosition   = pos;
        c->m_LastRotation   = q;
        c->m_Dirty          = 0;
    }

    void GetRenderCameraData(HRenderContext render_context, HRenderCamera camera, RenderCameraData* data)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            *data = c->m_Data;
        }
    }

    void SetRenderCameraEnabled(HRenderContext render_context, HRenderCamera camera, bool value)
    {
        RenderCamera* c = render_context->m_RenderCameras.Get(camera);
        if (c)
        {
            c->m_Enabled = value;
        }
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
