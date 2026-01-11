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

#ifndef DM_BOX2D_DEFOLD_PHYSICS_H
#define DM_BOX2D_DEFOLD_PHYSICS_H

#include <Box2D/Dynamics/b2World.h>

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dmsdk/dlib/vmath.h>

#include "../physics.h"
#include "../physics_private.h"
#include "box2d_defold_debug_draw.h"

namespace dmPhysics
{
    class ContactListener : public b2ContactListener
    {
    public:
        ContactListener(HWorld2D world);

        virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse);

        void SetStepWorldContext(const StepWorldContext* context);

    private:
        HWorld2D m_World;
        /// Temporary context to be set before each stepping of the world
        const StepWorldContext* m_TempStepWorldContext;
    };

    struct World2D
    {
        World2D(HContext2D context, const NewWorldParams& params);

        OverlapCache                m_TriggerOverlaps;
        HContext2D                  m_Context;
        b2World                     m_World;
        dmArray<RayCastRequest>     m_RayCastRequests;
        DebugDraw2D                 m_DebugDraw;
        ContactListener             m_ContactListener;
        GetWorldTransformCallback   m_GetWorldTransformCallback;
        SetWorldTransformCallback   m_SetWorldTransformCallback;
        uint8_t                     m_AllowDynamicTransforms:1;
        uint8_t                     :7;
    };

    struct Context2D
    {
        Context2D();

        dmArray<World2D*>           m_Worlds;
        DebugCallbacks              m_DebugCallbacks;
        b2Vec2                      m_Gravity;
        dmMessage::HSocket          m_Socket;
        float                       m_Scale;
        float                       m_InvScale;
        float                       m_ContactImpulseLimit;
        float                       m_TriggerEnterLimit;
        float                       m_VelocityThreshold;
        int                         m_RayCastLimit;
        int                         m_TriggerOverlapCapacity;
        uint8_t                     m_AllowDynamicTransforms:1;
        uint8_t                     :7;
    };

    inline void ToB2(const dmVMath::Point3& p0, b2Vec2& p1, float scale)
    {
        p1.Set(p0.getX() * scale, p0.getY() * scale);
    }

    inline void ToB2(const dmVMath::Vector3& p0, b2Vec2& p1, float scale)
    {
        p1.Set(p0.getX() * scale, p0.getY() * scale);
    }

    inline void FromB2(const b2Vec2& p0, dmVMath::Vector3& p1, float inv_scale)
    {
        p1.setX(p0.x * inv_scale);
        p1.setY(p0.y * inv_scale);
        p1.setZ(0.0f);
    }

    inline void FromB2(const b2Vec2& p0, dmVMath::Point3& p1, float inv_scale)
    {
        p1.setX(p0.x * inv_scale);
        p1.setY(p0.y * inv_scale);
        p1.setZ(0.0f);
    }

    inline b2Vec2 TransformScaleB2(const b2Transform& t, float scale, const b2Vec2& p)
    {
        b2Vec2 pp = p;
        pp *= scale;
        return b2Mul(t, pp);
    }

    inline b2Vec2 FromTransformScaleB2(const b2Transform& t, float inv_scale, const b2Vec2& p)
    {
        b2Vec2 pp = p;
        pp *= inv_scale;
        return b2MulT(t, pp);
    }
}

#endif // DM_BOX2D_DEFOLD_PHYSICS_H
