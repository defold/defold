#ifndef PHYSICS_DEBUG_DRAW_H
#define PHYSICS_DEBUG_DRAW_H

#include "btBulletDynamicsCommon.h"

#include "physics.h"

namespace dmPhysics
{
    class DebugDraw : public btIDebugDraw
    {
    public:
        DebugDraw();
        virtual ~DebugDraw();

        virtual void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color);
        virtual void drawContactPoint(const btVector3 &PointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color);
        virtual void reportErrorWarning(const char *warningString);
        virtual void draw3dText(const btVector3 &location, const char *textString);
        virtual void setDebugMode(int debugMode);
        virtual int getDebugMode() const;

        void SetRenderLine(RenderLine render_line);

    private:
        int m_DebugMode;
        RenderLine m_RenderLine;
    };
}

#endif
