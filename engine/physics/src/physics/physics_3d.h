#ifndef PHYSICS_3D_H
#define PHYSICS_3D_H

#include <dlib/array.h>

#include "physics.h"
#include "physics_private.h"
#include "debug_draw_3d.h"

namespace dmPhysics
{
    struct World3D
    {
        World3D(HContext3D context, const NewWorldParams& params);
        ~World3D();

        OverlapCache                            m_TriggerOverlaps;
        dmArray<RayCastRequest>                 m_RayCastRequests;
        DebugDraw3D                             m_DebugDraw;
        HContext3D                              m_Context;
        btDefaultCollisionConfiguration*        m_CollisionConfiguration;
        btCollisionDispatcher*                  m_Dispatcher;
        btAxisSweep3*                           m_OverlappingPairCache;
        btSequentialImpulseConstraintSolver*    m_Solver;
        btDiscreteDynamicsWorld*                m_DynamicsWorld;
        GetWorldTransformCallback               m_GetWorldTransform;
        SetWorldTransformCallback               m_SetWorldTransform;
        GetScaleCallback						m_GetScale;
    };

    struct Context3D
    {
        Context3D();

        dmArray<World3D*>           m_Worlds;
        DebugCallbacks              m_DebugCallbacks;
        btVector3                   m_Gravity;
        dmMessage::HSocket          m_Socket;
        float                       m_Scale;
        float                       m_InvScale;
        float                       m_ContactImpulseLimit;
        float                       m_TriggerEnterLimit;
        int                         m_RayCastLimit;
        int                         m_TriggerOverlapCapacity;
    };

    inline void ToBt(const Vectormath::Aos::Point3& p0, btVector3& p1, float scale)
    {
        p1.setValue(p0.getX() * scale, p0.getY() * scale, p0.getZ() * scale);
    }

    inline void ToBt(const Vectormath::Aos::Vector3& p0, btVector3& p1, float scale)
    {
        p1.setValue(p0.getX() * scale, p0.getY() * scale, p0.getZ() * scale);
    }

    inline void FromBt(const btVector3& p0, Vectormath::Aos::Point3& p1, float inv_scale)
    {
        p1.setX(p0.getX() * inv_scale);
        p1.setY(p0.getY() * inv_scale);
        p1.setZ(p0.getZ() * inv_scale);
    }

    inline void FromBt(const btVector3& p0, Vectormath::Aos::Vector3& p1, float inv_scale)
    {
        p1.setX(p0.getX() * inv_scale);
        p1.setY(p0.getY() * inv_scale);
        p1.setZ(p0.getZ() * inv_scale);
    }
}

#endif // PHYSICS_3D_H
