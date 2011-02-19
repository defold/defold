#ifndef PHYSICS_3D_H
#define PHYSICS_3D_H

#include <dlib/array.h>

#include "physics.h"
#include "debug_draw_3d.h"

namespace dmPhysics
{
    struct World3D
    {
        World3D(HContext3D context, const NewWorldParams& params);
        ~World3D();

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
        CollisionCallback                       m_CollisionCallback;
        void*                                   m_CollisionCallbackUserData;
        ContactPointCallback                    m_ContactPointCallback;
        void*                                   m_ContactPointCallbackUserData;
    };

    struct Context3D
    {
        Context3D();

        dmArray<World3D*>           m_Worlds;
        DebugCallbacks              m_DebugCallbacks;
        Vectormath::Aos::Vector3    m_Gravity;
    };
}

#endif // PHYSICS_3D_H
