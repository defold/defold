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


#include <dlib/math.h>
#include "box2d_debug_draw.h"
#include "box2d_physics.h"

namespace dmPhysics
{
    static void UnpackRGB(b2HexColor color, float rgb[3])
    {
        // Extract RGB components
        rgb[0] = ((color >> 16) & 0xFF) / 255.0f;
        rgb[1] = ((color >> 8) & 0xFF) / 255.0f;
        rgb[2] = (color & 0xFF) / 255.0f;
    }

    static void DrawPolygon(const b2Vec2* vertices, int vertexCount, b2HexColor color, void* context)
    {
        DebugDraw2D* debug_draw_2d = (DebugDraw2D*)context;

        if (debug_draw_2d->m_Callbacks->m_DrawLines)
        {
            const uint32_t MAX_SEGMENT_COUNT = 16;
            dmVMath::Point3 points[MAX_SEGMENT_COUNT*2];
            float inv_scale = debug_draw_2d->m_Callbacks->m_InvScale;
            uint32_t segment_count = dmMath::Min(MAX_SEGMENT_COUNT, (uint32_t)vertexCount);
            for (uint32_t i = 0; i < segment_count; ++i)
            {
                FromB2(vertices[i], points[2*i], inv_scale);
                uint32_t j = (i + 1) % segment_count;
                FromB2(vertices[j], points[2*i + 1], inv_scale);
            }

            float rgb[3];
            UnpackRGB(color, rgb);

            (*debug_draw_2d->m_Callbacks->m_DrawLines)(points, segment_count * 2, dmVMath::Vector4(rgb[0], rgb[1], rgb[2], debug_draw_2d->m_Callbacks->m_Alpha), debug_draw_2d->m_Callbacks->m_UserData);
        }
    }

    static void DrawSolidPolygon( b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color, void* context )
    {
        DebugDraw2D* debug_draw_2d = (DebugDraw2D*)context;

        if (debug_draw_2d->m_Callbacks->m_DrawTriangles)
        {
            const uint32_t MAX_TRI_COUNT = 16;
            dmVMath::Point3 points[MAX_TRI_COUNT*3];

            b2Vec2 vertices_transformed[MAX_TRI_COUNT];

            uint32_t triangle_count = dmMath::Min(MAX_TRI_COUNT, (uint32_t)vertexCount);
            b2Vec2 b2_center = {};

            for (uint32_t i = 0; i < triangle_count; ++i)
            {
                vertices_transformed[i] = b2TransformPoint(transform, vertices[i]);
                b2_center += vertices_transformed[i];
            }
            b2_center.x /= triangle_count;
            b2_center.y /= triangle_count;
            float inv_scale = debug_draw_2d->m_Callbacks->m_InvScale;
            dmVMath::Vector3 center;
            FromB2(b2_center, center, inv_scale);
            for (uint32_t i = 0; i < triangle_count; ++i)
            {
                FromB2(vertices_transformed[i], points[3*i + 0], inv_scale);
                points[3*i + 1] = dmVMath::Point3(center);
                uint32_t j = (i + 1) % triangle_count;
                FromB2(vertices_transformed[j], points[3*i + 2], inv_scale);
            }

            float rgb[3];
            UnpackRGB(color, rgb);

            (*debug_draw_2d->m_Callbacks->m_DrawTriangles)(points, triangle_count * 3, dmVMath::Vector4(rgb[0], rgb[1], rgb[2], debug_draw_2d->m_Callbacks->m_Alpha), debug_draw_2d->m_Callbacks->m_UserData);
        }
    }

    static void DrawCircle( b2Vec2 center, float radius, b2HexColor color, void* context )
    {
        DebugDraw2D* debug_draw_2d = (DebugDraw2D*)context;
        if (debug_draw_2d->m_Callbacks->m_DrawLines)
        {
            float inv_scale = debug_draw_2d->m_Callbacks->m_InvScale;
            dmVMath::Point3 c;
            FromB2(center, c, inv_scale);
            radius *= inv_scale;
            const uint32_t MAX_SEGMENT_COUNT = 16;
            dmVMath::Point3 points[MAX_SEGMENT_COUNT * 2];
            float angle = 0.0f;
            float delta_angle = (float) (2.0 * M_PI / MAX_SEGMENT_COUNT);
            float s_a = sinf(angle);
            float c_a = cosf(angle);
            for (uint32_t i = 0; i < MAX_SEGMENT_COUNT; ++i)
            {
                points[i*2] = c + dmVMath::Vector3(c_a * radius, s_a * radius, 0.0f);
                angle += delta_angle;
                s_a = sinf(angle);
                c_a = cosf(angle);
                points[i*2 + 1] = c + dmVMath::Vector3(c_a * radius, s_a * radius, 0.0f);
            }
            float rgb[3];
            UnpackRGB(color, rgb);

            (*debug_draw_2d->m_Callbacks->m_DrawLines)(points, MAX_SEGMENT_COUNT * 2, dmVMath::Vector4(rgb[0], rgb[1], rgb[2], debug_draw_2d->m_Callbacks->m_Alpha), debug_draw_2d->m_Callbacks->m_UserData);
        }
    }

    static void DrawSolidCircle( b2Transform transform, float radius, b2HexColor color, void* context )
    {
        DebugDraw2D* debug_draw_2d = (DebugDraw2D*)context;
        if (debug_draw_2d->m_Callbacks->m_DrawTriangles)
        {
            float inv_scale = debug_draw_2d->m_Callbacks->m_InvScale;
            dmVMath::Point3 c;
            FromB2(transform.p, c, inv_scale);
            radius *= inv_scale;
            const uint32_t MAX_TRI_COUNT = 16;
            dmVMath::Point3 points[MAX_TRI_COUNT * 3];
            float angle = 0.0f;
            float delta_angle = (float) (2.0 * M_PI / MAX_TRI_COUNT);
            float s_a = sinf(angle);
            float c_a = cosf(angle);
            for (uint32_t i = 0; i < MAX_TRI_COUNT; ++i)
            {
                points[i*3] = c;
                points[i*3 + 1] = c + dmVMath::Vector3(c_a * radius, s_a * radius, 0.0f);
                angle += delta_angle;
                s_a = sinf(angle);
                c_a = cosf(angle);
                points[i*3 + 2] = c + dmVMath::Vector3(c_a * radius, s_a * radius, 0.0f);
            }

            float rgb[3];
            UnpackRGB(color, rgb);

            (*debug_draw_2d->m_Callbacks->m_DrawTriangles)(points, MAX_TRI_COUNT * 3, dmVMath::Vector4(rgb[0], rgb[1], rgb[2], debug_draw_2d->m_Callbacks->m_Alpha), debug_draw_2d->m_Callbacks->m_UserData);
        }
    }

    static void DrawSolidCapsule( b2Vec2 p1, b2Vec2 p2, float radius, b2HexColor color, void* context )
    {

    }

    static void DrawSegment( b2Vec2 p1, b2Vec2 p2, b2HexColor color, void* context )
    {
        DebugDraw2D* debug_draw_2d = (DebugDraw2D*)context;
        if (debug_draw_2d->m_Callbacks->m_DrawLines)
        {
            float inv_scale = debug_draw_2d->m_Callbacks->m_InvScale;
            dmVMath::Point3 points[2];
            FromB2(p1, points[0], inv_scale);
            FromB2(p2, points[1], inv_scale);

            float rgb[3];
            UnpackRGB(color, rgb);

            (*debug_draw_2d->m_Callbacks->m_DrawLines)(points, 2, dmVMath::Vector4(rgb[0], rgb[1], rgb[2], debug_draw_2d->m_Callbacks->m_Alpha), debug_draw_2d->m_Callbacks->m_UserData);
        }
    }

    static void DrawTransform( b2Transform transform, void* context )
    {
        DebugDraw2D* debug_draw_2d = (DebugDraw2D*)context;
        if (debug_draw_2d->m_Callbacks->m_DrawLines)
        {
            b2Vec2 origin = b2TransformPoint(transform, {});
            b2Vec2 x = b2TransformPoint(transform,  {debug_draw_2d->m_Callbacks->m_DebugScale, 0.0f});
            b2Vec2 y = b2TransformPoint(transform, {0.0f, debug_draw_2d->m_Callbacks->m_DebugScale});

            DrawSegment(origin, x, b2_colorRed, context);
            DrawSegment(origin, y, b2_colorGreen, context);
        }
    }

    static void DrawPoint( b2Vec2 p, float size, b2HexColor color, void* context )
    {

    }

    static void DrawString(b2Vec2 p, const char* s, b2HexColor color, void* context)
    {

    }

    DebugDraw2D::DebugDraw2D(DebugCallbacks* callbacks)
    : m_Callbacks(callbacks)
    {
        memset(&m_DebugDraw, 0, sizeof(m_DebugDraw));

        m_DebugDraw.DrawPolygonFcn          = DrawPolygon;
        m_DebugDraw.DrawSolidPolygonFcn     = DrawSolidPolygon;
        m_DebugDraw.DrawCircleFcn           = DrawCircle;
        m_DebugDraw.DrawSolidCircleFcn      = DrawSolidCircle;
        m_DebugDraw.DrawSolidCapsuleFcn     = DrawSolidCapsule;
        m_DebugDraw.DrawSegmentFcn          = DrawSegment;
        m_DebugDraw.DrawTransformFcn        = DrawTransform;
        m_DebugDraw.DrawPointFcn            = DrawPoint;
        m_DebugDraw.DrawStringFcn           = DrawString;

        // Debug!
        const bool force_flag = false;
        m_DebugDraw.drawShapes           = force_flag;
        m_DebugDraw.drawJoints           = force_flag;
        m_DebugDraw.drawJointExtras      = force_flag;
        m_DebugDraw.drawMass             = force_flag;
        m_DebugDraw.drawContacts         = force_flag;
        m_DebugDraw.drawGraphColors      = force_flag;
        m_DebugDraw.drawContactNormals   = force_flag;
        m_DebugDraw.drawContactImpulses  = force_flag;
        m_DebugDraw.drawFrictionImpulses = force_flag;
        m_DebugDraw.context              = this;
    }
}
