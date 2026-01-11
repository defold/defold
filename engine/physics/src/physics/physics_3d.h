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

#ifndef PHYSICS_3D_H
#define PHYSICS_3D_H

#include <dlib/array.h>

#include "physics.h"
#include "physics_private.h"
#include "debug_draw_3d.h"

#include "btBulletDynamicsCommon.h"

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
        uint8_t                                 m_AllowDynamicTransforms:1;
        uint8_t                                 :7;
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
        uint8_t                     m_AllowDynamicTransforms:1;
        uint8_t                     :7;
    };

    inline void ToBt(const dmVMath::Point3& p0, btVector3& p1, float scale)
    {
        p1.setValue(p0.getX() * scale, p0.getY() * scale, p0.getZ() * scale);
    }

    inline void ToBt(const dmVMath::Vector3& p0, btVector3& p1, float scale)
    {
        p1.setValue(p0.getX() * scale, p0.getY() * scale, p0.getZ() * scale);
    }

    inline void FromBt(const btVector3& p0, dmVMath::Point3& p1, float inv_scale)
    {
        p1.setX(p0.getX() * inv_scale);
        p1.setY(p0.getY() * inv_scale);
        p1.setZ(p0.getZ() * inv_scale);
    }

    inline void FromBt(const btVector3& p0, dmVMath::Vector3& p1, float inv_scale)
    {
        p1.setX(p0.getX() * inv_scale);
        p1.setY(p0.getY() * inv_scale);
        p1.setZ(p0.getZ() * inv_scale);
    }
}

#endif // PHYSICS_3D_H
