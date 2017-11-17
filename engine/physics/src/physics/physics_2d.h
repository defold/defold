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
        GetScaleCallback            m_GetScaleCallback;
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

    class ProcessRayCastResultCallback2D : public b2RayCastCallback
    {
    public:
        ProcessRayCastResultCallback2D();
        virtual ~ProcessRayCastResultCallback2D();

        /// Called for each fixture found in the query. You control how the ray cast
        /// proceeds by returning a float:
        /// return -1: ignore this fixture and continue
        /// return 0: terminate the ray cast
        /// return fraction: clip the ray to this point
        /// return 1: don't clip the ray and continue
        /// @param fixture the fixture hit by the ray
        /// @param point the point of initial intersection
        /// @param normal the normal vector at the point of intersection
        /// @return -1 to filter, 0 to terminate, fraction to clip the ray for
        /// closest hit, 1 to continue
        virtual float32 ReportFixture(b2Fixture* fixture, int32 index, const b2Vec2& point, const b2Vec2& normal, float32 fraction);

        HContext2D m_Context;
        RayCastResponse m_Response;
        void* m_IgnoredUserData;
        uint16_t m_CollisionGroup;
        uint16_t m_CollisionMask;
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
