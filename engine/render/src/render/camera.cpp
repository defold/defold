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
    const float EPS = 1e-6f;

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
        camera->m_Data.m_OrthographicMode = ORTHO_MODE_FIXED;

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
            if (c->m_Data.m_OrthographicMode == ORTHO_MODE_AUTO_FIT || c->m_Data.m_OrthographicMode == ORTHO_MODE_AUTO_COVER)
            {
                float zx = width / (display_scale * proj_width);
                float zy = height / (display_scale * proj_height);

                if (c->m_Data.m_OrthographicMode == ORTHO_MODE_AUTO_FIT)
                {
                    zoom = (zx < zy) ? zx : zy;
                }
                else if (c->m_Data.m_OrthographicMode == ORTHO_MODE_AUTO_COVER)
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

    Result CameraScreenToWorld(HRenderContext render_context, HRenderCamera camera_handle, float screen_x, float screen_y, float z, dmVMath::Vector3* out_world)
    {
        RenderCamera* camera = render_context->m_RenderCameras.Get(camera_handle);

        // Window size
        dmGraphics::HContext gc = GetGraphicsContext(render_context);
        float win_w = (float) dmGraphics::GetWindowWidth(gc);
        float win_h = (float) dmGraphics::GetWindowHeight(gc);
        if (win_w <= 0.0f)
        {
            win_w = 1.0f;
        }
        if (win_h <= 0.0f)
        {
            win_h = 1.0f;
        }

        // Viewport-aware screen -> Normalized Device Coordinates
        const dmVMath::Vector4& vp = camera->m_Data.m_Viewport;
        float vx = vp.getX() * win_w;
        float vy = vp.getY() * win_h;
        float vw = vp.getZ() * win_w;
        float vh = vp.getW() * win_h;
        if (vw <= 0.0f || vh <= 0.0f)
        {
            vx = 0.0f; vy = 0.0f; vw = win_w; vh = win_h;
        }
        float x_ndc = 2.0f * ((screen_x - vx) / vw) - 1.0f;
        float y_ndc = 2.0f * ((screen_y - vy) / vh) - 1.0f;

        dmVMath::Matrix4 inv_vp = dmVMath::Inverse(camera->m_ViewProjection);

        // Unproject near/far points and build pixel ray (world)
        dmVMath::Vector4 v_near_clip(x_ndc, y_ndc, -1.0f, 1.0f);
        dmVMath::Vector4 v_far_clip (x_ndc, y_ndc,  1.0f, 1.0f);
        dmVMath::Vector4 v0 = inv_vp * v_near_clip;
        dmVMath::Vector4 v1 = inv_vp * v_far_clip;
        float iw0 = 1.0f / v0.getW();
        float iw1 = 1.0f / v1.getW();
        dmVMath::Point3  p_near(v0.getX() * iw0, v0.getY() * iw0, v0.getZ() * iw0);
        dmVMath::Point3  p_far (v1.getX() * iw1, v1.getY() * iw1, v1.getZ() * iw1);
        dmVMath::Vector3 dir    = dmVMath::Normalize(p_far - p_near);

        // Camera forward (world)
        dmVMath::Vector3 forward = dmVMath::Rotate(camera->m_LastRotation, dmVMath::Vector3(0.0f, 0.0f, -1.0f));
        forward = dmVMath::Normalize(forward);

        // z is view-depth along forward (world units from camera plane)
        float denom = dmVMath::Dot(dir, forward);
        if (denom > -EPS && denom < EPS)
        {
            return RESULT_INVALID_PARAMETER;
        }

        dmVMath::Point3 cam_pos = camera->m_LastPosition;
        // Intersect the ray r(s) = p_near + s * dir with the view-depth plane at distance z along 'forward'
        float dot0 = dmVMath::Dot(p_near - cam_pos, forward);
        float s = (z - dot0) / denom;
        dmVMath::Point3 p_point = p_near + dir * s;
        *out_world = dmVMath::Vector3(p_point.getX(), p_point.getY(), p_point.getZ());
        return RESULT_OK;
    }

    Result CameraWorldToScreen(HRenderContext render_context, HRenderCamera camera_handle, const dmVMath::Vector3& world, dmVMath::Vector3* out_screen)
    {
        RenderCamera* camera = render_context->m_RenderCameras.Get(camera_handle);

        // Project world to clip space
        dmVMath::Vector4 world4(world.getX(), world.getY(), world.getZ(), 1.0f);
        dmVMath::Vector4 clip = camera->m_ViewProjection * world4;
        float w = clip.getW();
        if (w > -EPS && w < EPS)
        {
            return RESULT_INVALID_PARAMETER;
        }
        float inv_w = 1.0f / w;
        float x_ndc = clip.getX() * inv_w;
        float y_ndc = clip.getY() * inv_w;

        // Window size
        dmGraphics::HContext gc = GetGraphicsContext(render_context);
        float win_w = (float) dmGraphics::GetWindowWidth(gc);
        float win_h = (float) dmGraphics::GetWindowHeight(gc);
        if (win_w <= 0.0f)
        {
            win_w = 1.0f;
        }
        if (win_h <= 0.0f)
        {
            win_h = 1.0f;
        }

        // Viewport mapping (Normalized Device Coordinates -> screen pixels)
        const dmVMath::Vector4& vp = camera->m_Data.m_Viewport;
        float vx = vp.getX() * win_w;
        float vy = vp.getY() * win_h;
        float vw = vp.getZ() * win_w;
        float vh = vp.getW() * win_h;
        if (vw <= 0.0f || vh <= 0.0f)
        {
            vx = 0.0f; vy = 0.0f; vw = win_w; vh = win_h;
        }

        float sx = vx + (x_ndc + 1.0f) * 0.5f * vw;
        float sy = vy + (y_ndc + 1.0f) * 0.5f * vh;

        // View depth along camera forward
        dmVMath::Vector3 forward = dmVMath::Rotate(camera->m_LastRotation, dmVMath::Vector3(0.0f, 0.0f, -1.0f));
        forward = dmVMath::Normalize(forward);
        dmVMath::Point3 world_p(world.getX(), world.getY(), world.getZ());
        float z_view = dmVMath::Dot(world_p - camera->m_LastPosition, forward);

        *out_screen = dmVMath::Vector3(sx, sy, z_view);
        return RESULT_OK;
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
