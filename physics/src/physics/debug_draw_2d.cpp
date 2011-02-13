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
            uint32_t segment_count = dmMath::Min(MAX_SEGMENT_COUNT, (uint32_t)vertexCount);
            for (uint32_t i = 0; i < segment_count; ++i)
            {
                points[2*i] = Vectormath::Aos::Point3(vertices[i].x, vertices[i].y, 0.0f);
                uint32_t j = (i + 1) % segment_count;
                points[2*i + 1] = Vectormath::Aos::Point3(vertices[j].x, vertices[j].y, 0.0f);
            }
            (*m_Callbacks->m_DrawLines)(points, segment_count * 2, Vectormath::Aos::Vector4(color.r, color.g, color.b, 1.0f), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
    {
        if (m_Callbacks->m_DrawTriangles)
        {
            const uint32_t MAX_TRI_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_TRI_COUNT*3];
            uint32_t triangle_count = dmMath::Min(MAX_TRI_COUNT, (uint32_t)vertexCount);
            b2Vec2 center(0.0f, 0.0f);
            for (uint32_t i = 0; i < triangle_count; ++i)
            {
                center += vertices[i];
            }
            center.x /= triangle_count;
            center.y /= triangle_count;
            for (uint32_t i = 0; i < triangle_count; ++i)
            {
                points[3*i] = Vectormath::Aos::Point3(vertices[i].x, vertices[i].y, 0.0f);
                uint32_t j = (i + 1) % triangle_count;
                points[3*i + 1] = Vectormath::Aos::Point3(vertices[j].x, vertices[j].y, 0.0f);
                points[3*i + 2] = Vectormath::Aos::Point3(center.x, center.y, 0.0f);
            }
            (*m_Callbacks->m_DrawTriangles)(points, triangle_count * 3, Vectormath::Aos::Vector4(color.r, color.g, color.b, 1.0f), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
    {
        if (m_Callbacks->m_DrawLines)
        {
            Vectormath::Aos::Point3 c(center.x, center.y, 0.0f);
            const uint32_t MAX_SEGMENT_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_SEGMENT_COUNT * 2];
            float angle = 0.0f;
            float delta_angle = 2.0f * M_PI / (MAX_SEGMENT_COUNT - 1);
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
            (*m_Callbacks->m_DrawLines)(points, MAX_SEGMENT_COUNT * 2, Vectormath::Aos::Vector4(color.r, color.g, color.b, 1.0f), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
    {
        if (m_Callbacks->m_DrawTriangles)
        {
            Vectormath::Aos::Point3 c(center.x, center.y, 0.0f);
            const uint32_t MAX_TRI_COUNT = 16;
            Vectormath::Aos::Point3 points[MAX_TRI_COUNT * 3];
            float angle = 0.0f;
            float delta_angle = 2.0f * M_PI / (MAX_TRI_COUNT - 1);
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
            (*m_Callbacks->m_DrawTriangles)(points, MAX_TRI_COUNT * 3, Vectormath::Aos::Vector4(color.r, color.g, color.b, 1.0f), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
    {
        if (m_Callbacks->m_DrawLines)
        {
            Vectormath::Aos::Point3 points[2] =
            {
                Vectormath::Aos::Point3(p1.x, p1.y, 0.0f),
                Vectormath::Aos::Point3(p2.x, p2.y, 0.0f)
            };
            (*m_Callbacks->m_DrawLines)(points, 2, Vectormath::Aos::Vector4(color.r, color.g, color.b, 1.0f), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw2D::DrawTransform(const b2Transform& xf)
    {
        if (m_Callbacks->m_DrawLines)
        {
            b2Vec2 origin = b2Mul(xf, b2Vec2(0.0f, 0.0f));
            b2Vec2 x = b2Mul(xf, b2Vec2(1.0f, 0.0f));
            b2Vec2 y = b2Mul(xf, b2Vec2(0.0f, 1.0f));
            DrawSegment(origin, x, b2Color(1.0f, 0.0f, 0.0f));
            DrawSegment(origin, y, b2Color(0.0f, 1.0f, 0.0f));
        }
    }
}
