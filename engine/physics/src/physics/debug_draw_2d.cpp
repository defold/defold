#include "debug_draw_2d.h"

#include <dlib/math.h>

#include "physics_2d.h"

namespace dmPhysics
{
    DebugDraw2D::DebugDraw2D(DebugCallbacks* callbacks)
    : m_Callbacks(callbacks)
    {

    }

    void DebugDraw2D::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
    {
        if (m_Callbacks->m_DrawLines)
        {
            const uint32_t MAX_SEGMENT_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_SEGMENT_COUNT*2];
            float inv_scale = m_Callbacks->m_InvScale;
            uint32_t segment_count = dmMath::Min(MAX_SEGMENT_COUNT, (uint32_t)vertexCount);
            for (uint32_t i = 0; i < segment_count; ++i)
            {
                FromB2(vertices[i], points[2*i], inv_scale);
                uint32_t j = (i + 1) % segment_count;
                FromB2(vertices[j], points[2*i + 1], inv_scale);
            }
            (*m_Callbacks->m_DrawLines)(points, segment_count * 2, Vectormath::Aos::Vector4(color.r, color.g, color.b, m_Callbacks->m_Alpha), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
    {
        if (m_Callbacks->m_DrawTriangles)
        {
            const uint32_t MAX_TRI_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_TRI_COUNT*3];
            uint32_t triangle_count = dmMath::Min(MAX_TRI_COUNT, (uint32_t)vertexCount);
            b2Vec2 b2_center(0.0f, 0.0f);
            for (uint32_t i = 0; i < triangle_count; ++i)
            {
                b2_center += vertices[i];
            }
            b2_center.x /= triangle_count;
            b2_center.y /= triangle_count;
            float inv_scale = m_Callbacks->m_InvScale;
            Vectormath::Aos::Vector3 center;
            FromB2(b2_center, center, inv_scale);
            for (uint32_t i = 0; i < triangle_count; ++i)
            {
                FromB2(vertices[i], points[3*i + 0], inv_scale);
                points[3*i + 1] = Vectormath::Aos::Point3(center);
                uint32_t j = (i + 1) % triangle_count;
                FromB2(vertices[j], points[3*i + 2], inv_scale);
            }
            (*m_Callbacks->m_DrawTriangles)(points, triangle_count * 3, Vectormath::Aos::Vector4(color.r, color.g, color.b, m_Callbacks->m_Alpha), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
    {
        if (m_Callbacks->m_DrawLines)
        {
            float inv_scale = m_Callbacks->m_InvScale;
            Vectormath::Aos::Point3 c;
            FromB2(center, c, inv_scale);
            radius *= inv_scale;
            const uint32_t MAX_SEGMENT_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_SEGMENT_COUNT * 2];
            float angle = 0.0f;
            float delta_angle = (float) (2.0 * M_PI / MAX_SEGMENT_COUNT);
            float s_a = sinf(angle);
            float c_a = cosf(angle);
            for (uint32_t i = 0; i < MAX_SEGMENT_COUNT; ++i)
            {
                points[i*2] = c + Vectormath::Aos::Vector3(c_a * radius, s_a * radius, 0.0f);
                angle += delta_angle;
                s_a = sinf(angle);
                c_a = cosf(angle);
                points[i*2 + 1] = c + Vectormath::Aos::Vector3(c_a * radius, s_a * radius, 0.0f);
            }
            (*m_Callbacks->m_DrawLines)(points, MAX_SEGMENT_COUNT * 2, Vectormath::Aos::Vector4(color.r, color.g, color.b, m_Callbacks->m_Alpha), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
    {
        if (m_Callbacks->m_DrawTriangles)
        {
            float inv_scale = m_Callbacks->m_InvScale;
            Vectormath::Aos::Point3 c;
            FromB2(center, c, inv_scale);
            radius *= inv_scale;
            const uint32_t MAX_TRI_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_TRI_COUNT * 3];
            float angle = 0.0f;
            float delta_angle = (float) (2.0 * M_PI / MAX_TRI_COUNT);
            float s_a = sinf(angle);
            float c_a = cosf(angle);
            for (uint32_t i = 0; i < MAX_TRI_COUNT; ++i)
            {
                points[i*3] = c;
                points[i*3 + 1] = c + Vectormath::Aos::Vector3(c_a * radius, s_a * radius, 0.0f);
                angle += delta_angle;
                s_a = sinf(angle);
                c_a = cosf(angle);
                points[i*3 + 2] = c + Vectormath::Aos::Vector3(c_a * radius, s_a * radius, 0.0f);
            }
            (*m_Callbacks->m_DrawTriangles)(points, MAX_TRI_COUNT * 3, Vectormath::Aos::Vector4(color.r, color.g, color.b, m_Callbacks->m_Alpha), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
    {
        if (m_Callbacks->m_DrawLines)
        {
            float inv_scale = m_Callbacks->m_InvScale;
            Vectormath::Aos::Point3 points[2];
            FromB2(p1, points[0], inv_scale);
            FromB2(p2, points[1], inv_scale);
            (*m_Callbacks->m_DrawLines)(points, 2, Vectormath::Aos::Vector4(color.r, color.g, color.b, m_Callbacks->m_Alpha), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawTransform(const b2Transform& xf)
    {
        if (m_Callbacks->m_DrawLines)
        {
            b2Vec2 origin = b2Mul(xf, b2Vec2(0.0f, 0.0f));
            b2Vec2 x = b2Mul(xf, b2Vec2(m_Callbacks->m_DebugScale, 0.0f));
            b2Vec2 y = b2Mul(xf, b2Vec2(0.0f, m_Callbacks->m_DebugScale));
            DrawSegment(origin, x, b2Color(1.0f, 0.0f, 0.0f));
            DrawSegment(origin, y, b2Color(0.0f, 1.0f, 0.0f));
        }
    }

    void DebugDraw2D::DrawArrow(const b2Vec2& p, const b2Vec2& d, const b2Color& color)
    {
        if (m_Callbacks->m_DrawLines)
        {
            b2Vec2 dp = m_Callbacks->m_DebugScale * d;
            // Scale for later "un-scale"
            dp *= m_Callbacks->m_Scale;
            b2Vec2 n(dp.y, -dp.x);
            n *= 0.15f;
            b2Vec2 head = 0.35f * dp;
            b2Vec2 top = p + dp;
            DrawSegment(p, top, color);
            b2Vec2 v[3];
            v[0] = top;
            v[1] = top - head - n;
            v[2] = top - head + n;
            DrawSolidPolygon(v, 3, color);
        }
    }
}
