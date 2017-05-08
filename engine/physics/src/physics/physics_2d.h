#ifndef DMPHYSICS_2D_H
#define DMPHYSICS_2D_H

#include <dmsdk/dlib/array.h>
#include <dmsdk/physics/physics.h>
#include "physics_private.h"
#include "debug_draw_2d.h"

namespace dmPhysics2D
{
    struct Context2D;

    class ContactListener : public b2ContactListener
    {
    public:
        ContactListener(dmPhysics::HContext context);

        virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse);

        void SetStepWorldContext(const dmPhysics::StepWorldContext* context);

    private:
        Context2D* m_Context;
        /// Temporary context to be set before each stepping of the world
        const dmPhysics::StepWorldContext* m_TempStepWorldContext;
    };

    struct World2D
    {
        World2D(dmPhysics::HContext context, const dmPhysics::NewWorldParams& params);

        dmPhysics::OverlapCache     m_TriggerOverlaps;
        Context2D*                  m_Context;
        b2World                     m_World;
        dmArray<dmPhysics::RayCastRequest>     m_RayCastRequests;
        DebugDraw2D                 m_DebugDraw;
        ContactListener             m_ContactListener;
        dmPhysics::GetWorldTransformCallback   m_GetWorldTransformCallback;
        dmPhysics::SetWorldTransformCallback   m_SetWorldTransformCallback;
    };

    struct Context2D
    {
        Context2D();

        dmArray<World2D*>           m_Worlds;
        dmPhysics::DebugCallbacks              m_DebugCallbacks;
        b2Vec2                      m_Gravity;
        //dmMessage::HSocket          m_Socket;
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

        Context2D* m_Context;
        dmPhysics::RayCastResponse m_Response;
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

#endif // DMPHYSICS_2D_H
