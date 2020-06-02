// Copyright 2020 The Defold Foundation
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

#ifndef PHYSICS_2D_H
#define PHYSICS_2D_H

#include <dlib/array.h>
#include <dlib/hashtable.h>

#include "physics.h"
#include "physics_private.h"
#include "debug_draw_2d.h"

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
        int                         m_RayCastLimit;
        int                         m_TriggerOverlapCapacity;
    };

    inline void ToB2(const Vectormath::Aos::Point3& p0, b2Vec2& p1, float scale)
    {
        p1.Set(p0.getX() * scale, p0.getY() * scale);
    }

    inline void ToB2(const Vectormath::Aos::Vector3& p0, b2Vec2& p1, float scale)
    {
        p1.Set(p0.getX() * scale, p0.getY() * scale);
    }

    inline void FromB2(const b2Vec2& p0, Vectormath::Aos::Vector3& p1, float inv_scale)
    {
        p1.setX(p0.x * inv_scale);
        p1.setY(p0.y * inv_scale);
        p1.setZ(0.0f);
    }

    inline void FromB2(const b2Vec2& p0, Vectormath::Aos::Point3& p1, float inv_scale)
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
}

#endif // PHYSICS_2D_H
