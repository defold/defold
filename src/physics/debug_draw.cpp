#include "debug_draw.h"

#include <dlib/log.h>

namespace dmPhysics
{
    DebugDraw::DebugDraw()
    : m_DebugMode(0)
    , m_RenderLine(0x0)
    {
        m_DebugMode = DBG_NoDebug
                | DBG_DrawWireframe
                | DBG_DrawAabb
                | DBG_DrawFeaturesText
                | DBG_DrawContactPoints
                | DBG_DrawText
                | DBG_ProfileTimings
                | DBG_EnableSatComparison
                | DBG_EnableCCD
                | DBG_DrawConstraints
                | DBG_DrawConstraintLimits;
    }

    DebugDraw::~DebugDraw()
    {

    }

    void DebugDraw::drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color)
    {
        if (m_RenderLine != 0x0)
        {
            (*m_RenderLine)(m_Context, Vectormath::Aos::Point3(from.getX(), from.getY(), from.getZ()), Vectormath::Aos::Point3(to.getX(), to.getY(), to.getZ()), Vectormath::Aos::Vector4(color.getX(), color.getY(), color.getZ(), 1.0f));
        }
    }

    void DebugDraw::drawContactPoint(const btVector3 &PointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color)
    {
        using namespace Vectormath::Aos;

        Point3 p(PointOnB.getX(), PointOnB.getY(), PointOnB.getZ());
        Vector3 n(normalOnB.getX(), normalOnB.getY(), normalOnB.getZ());
        assert(lengthSqr(n) > 0.0f);
        Vector3 t1;
        if (fabs(n.getX()) < fabs(n.getY()))
        {
            t1 = Vector3::xAxis();
        }
        else
        {
            t1 = Vector3::yAxis();
        }
        Vector3 t2 = cross(n, t1);
        assert(lengthSqr(t2) > 0.0f);
        t2 = normalize(t2);
        t1 = cross(t2, n);
        // lifetime measures number of frames the contact point has existed (max 127)
        float alpha = 1.0f - (lifeTime/255.0f);
        Vector4 c(color.getX(), color.getY(), color.getZ(), alpha);
        (*m_RenderLine)(m_Context, p, p + n - t1, c);
        (*m_RenderLine)(m_Context, p, p + n + t1, c);
        (*m_RenderLine)(m_Context, p, p + n - t2, c);
        (*m_RenderLine)(m_Context, p, p + n + t2, c);
        (*m_RenderLine)(m_Context, p, p + distance * n, c);
    }

    void DebugDraw::reportErrorWarning(const char *warningString)
    {
        dmLogWarning(warningString);
    }

    void DebugDraw::draw3dText(const btVector3 &location, const char *textString)
    {
        dmLogInfo("[%.2f, %.2f, %.2f]: %s\n", location.getX(), location.getY(), location.getZ(), textString);
    }

    void DebugDraw::setDebugMode(int debugMode)
    {
        m_DebugMode = debugMode;
    }

    int DebugDraw::getDebugMode() const
    {
        return m_DebugMode;
    }

    void DebugDraw::SetRenderLine(void* context, RenderLine render_line)
    {
        m_RenderLine = render_line;
        m_Context = context;
    }
}
