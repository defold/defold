#include "physics.h"

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "Box2D/Box2D.h"

#include "physics_2d.h"

namespace dmPhysics
{
    Context2D::Context2D()
    : m_Worlds()
    , m_DebugCallbacks()
    , m_Gravity(0.0f, -10.0f, 0.0f)
    , m_Socket(0)
    {

    }

    World2D::World2D(HContext2D context, const NewWorldParams& params)
    : m_World(b2Vec2(context->m_Gravity.getX(), context->m_Gravity.getY()))
    , m_RayCastRequests()
    , m_DebugDraw(&context->m_DebugCallbacks)
    , m_ContactListener(this)
    , m_GetWorldTransformCallback(params.m_GetWorldTransformCallback)
    , m_SetWorldTransformCallback(params.m_SetWorldTransformCallback)
    {
        m_DebugDraw.SetFlags(~0u);
        m_RayCastRequests.SetCapacity(64);
    }

    ProcessRayCastResultCallback2D::ProcessRayCastResultCallback2D()
    : m_IgnoredUserData(0x0)
    , m_CollisionGroup((uint16_t)~0u)
    , m_CollisionMask((uint16_t)~0u)
    {

    }

    ProcessRayCastResultCallback2D::~ProcessRayCastResultCallback2D()
    {

    }

    float32 ProcessRayCastResultCallback2D::ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
    {
        // Never hit triggers
        if (fixture->IsSensor())
            return -1.f;
        if (fixture->GetBody()->GetUserData() == m_IgnoredUserData)
            return -1.f;
        else if ((fixture->GetFilterData().groupIndex & m_CollisionMask) && (fixture->GetFilterData().maskBits & m_CollisionGroup))
        {
            m_Response.m_Hit = 1;
            m_Response.m_Fraction = fraction;
            m_Response.m_CollisionObjectGroup = fixture->GetFilterData().groupIndex;
            m_Response.m_CollisionObjectUserData = fixture->GetBody()->GetUserData();
            m_Response.m_Normal = Vectormath::Aos::Vector3(normal.x, normal.y, 0.0f);
            m_Response.m_Position = Vectormath::Aos::Point3(point.x, point.y, 0.0f);
            return fraction;
        }
        else
            return -1.f;
    }

    ContactListener::ContactListener(HWorld2D world)
    : m_World(world)
    {

    }

    void ContactListener::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse)
    {
        CollisionCallback collision_callback = m_TempStepWorldContext->m_CollisionCallback;
        ContactPointCallback contact_point_callback = m_TempStepWorldContext->m_ContactPointCallback;
        if (collision_callback != 0x0 || contact_point_callback != 0x0)
        {
            if (contact->IsTouching())
            {
                if (collision_callback)
                    collision_callback(contact->GetFixtureA()->GetUserData(), contact->GetFixtureA()->GetFilterData().groupIndex, contact->GetFixtureB()->GetUserData(), contact->GetFixtureB()->GetFilterData().groupIndex, m_TempStepWorldContext->m_CollisionUserData);
                if (contact_point_callback)
                {
                    b2WorldManifold world_manifold;
                    contact->GetWorldManifold(&world_manifold);
                    ContactPoint cp;
                    cp.m_PositionA = Vectormath::Aos::Point3(world_manifold.points[0].x, world_manifold.points[0].y, 0.0f);
                    cp.m_PositionB = Vectormath::Aos::Point3(world_manifold.points[1].x, world_manifold.points[1].y, 0.0f);
                    cp.m_UserDataA = contact->GetFixtureA()->GetBody()->GetUserData();
                    cp.m_UserDataB = contact->GetFixtureB()->GetBody()->GetUserData();
                    cp.m_Normal = Vectormath::Aos::Vector3(world_manifold.normal.x, world_manifold.normal.y, 0.0f);
                    b2Vec2 rv = contact->GetFixtureB()->GetBody()->GetLinearVelocity() - contact->GetFixtureA()->GetBody()->GetLinearVelocity();
                    cp.m_RelativeVelocity = Vectormath::Aos::Vector3(rv.x, rv.y, 0.0f);
                    cp.m_Distance = Vectormath::Aos::dist(cp.m_PositionA, cp.m_PositionB);
                    cp.m_AppliedImpulse = impulse->normalImpulses[0];
                    cp.m_InvMassA = 1.0f / contact->GetFixtureA()->GetBody()->GetMass();
                    cp.m_InvMassB = 1.0f / contact->GetFixtureB()->GetBody()->GetMass();
                    cp.m_GroupA = contact->GetFixtureA()->GetFilterData().groupIndex;
                    cp.m_GroupB = contact->GetFixtureB()->GetFilterData().groupIndex;
                    contact_point_callback(cp, m_TempStepWorldContext->m_ContactPointUserData);
                }
            }
        }
    }

    void ContactListener::SetStepWorldContext(const StepWorldContext* context)
    {
        m_TempStepWorldContext = context;
    }

    HContext2D NewContext2D(const NewContextParams& params)
    {
        Context2D* context = new Context2D();
        context->m_Worlds.SetCapacity(params.m_WorldCount);
        context->m_Gravity = params.m_Gravity;
        dmMessage::Result result = dmMessage::NewSocket(PHYSICS_SOCKET_NAME, &context->m_Socket);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogFatal("Could not create socket '%s'.", PHYSICS_SOCKET_NAME);
            DeleteContext2D(context);
            return 0x0;
        }
        return context;
    }

    void DeleteContext2D(HContext2D context)
    {
        if (!context->m_Worlds.Empty())
        {
            dmLogWarning("Deleting %ud 2d worlds since the context is deleted.", context->m_Worlds.Size());
            for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
                delete context->m_Worlds[i];
        }
        if (context->m_Socket != 0)
            dmMessage::DeleteSocket(context->m_Socket);
        delete context;
    }

    dmMessage::HSocket GetSocket2D(HContext2D context)
    {
        return context->m_Socket;
    }

    HWorld2D NewWorld2D(HContext2D context, const NewWorldParams& params)
    {
        if (context->m_Worlds.Full())
        {
            dmLogError("%s", "Physics world buffer full, world could not be created.");
            return 0x0;
        }
        World2D* world = new World2D(context, params);
        world->m_World.SetDebugDraw(&world->m_DebugDraw);
        world->m_World.SetContactListener(&world->m_ContactListener);
        context->m_Worlds.Push(world);
        return world;
    }

    void DeleteWorld2D(HContext2D context, HWorld2D world)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
            if (context->m_Worlds[i] == world)
                context->m_Worlds.EraseSwap(i);
        delete world;
    }

    void StepWorld2D(HWorld2D world, const StepWorldContext& context)
    {
        float dt = context.m_DT;
        // Update transforms of kinematic bodies
        if (world->m_GetWorldTransformCallback)
        {
            for (b2Body* body = world->m_World.GetBodyList(); body; body = body->GetNext())
            {
                if (body->GetType() == b2_kinematicBody)
                {
                    Vectormath::Aos::Point3 position;
                    Vectormath::Aos::Quat rotation;
                    (*world->m_GetWorldTransformCallback)(body->GetUserData(), position, rotation);
                    float angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
                    body->SetTransform(b2Vec2(position.getX(), position.getY()), angle);
                }
            }
        }
        world->m_ContactListener.SetStepWorldContext(&context);
        world->m_World.Step(dt, 10, 10);
        // Update transforms of dynamic bodies
        if (world->m_SetWorldTransformCallback)
        {
            for (b2Body* body = world->m_World.GetBodyList(); body; body = body->GetNext())
            {
                if (body->GetType() == b2_dynamicBody)
                {
                    Vectormath::Aos::Point3 position(body->GetPosition().x, body->GetPosition().y, 0.0f);
                    Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat::rotationZ(body->GetAngle());
                    (*world->m_SetWorldTransformCallback)(body->GetUserData(), position, rotation);
                }
            }
        }
        // Perform requested ray casts
        uint32_t size = world->m_RayCastRequests.Size();
        if (size > 0)
        {
            ProcessRayCastResultCallback2D callback;
            for (uint32_t i = 0; i < size; ++i)
            {
                RayCastRequest& request = world->m_RayCastRequests[i];
                b2Vec2 from(request.m_From.getX(), request.m_From.getY());
                b2Vec2 to(request.m_To.getX(), request.m_To.getY());
                callback.m_IgnoredUserData = request.m_IgnoredUserData;
                callback.m_CollisionMask = request.m_Mask;
                callback.m_Response.m_Hit = 0;
                world->m_World.RayCast(&callback, from, to);
                (*context.m_RayCastCallback)(callback.m_Response, request, context.m_RayCastUserData);
            }
            world->m_RayCastRequests.SetSize(0);
        }
        // Report sensor collisions
        if (context.m_CollisionCallback)
        {
            for (b2Contact* contact = world->m_World.GetContactList(); contact; contact = contact->GetNext())
            {
                if (contact->IsTouching() && (contact->GetFixtureA()->IsSensor() || contact->GetFixtureB()->IsSensor()))
                {
                    context.m_CollisionCallback(contact->GetFixtureA()->GetUserData(), contact->GetFixtureA()->GetFilterData().groupIndex, contact->GetFixtureB()->GetUserData(), contact->GetFixtureB()->GetFilterData().groupIndex, context.m_CollisionUserData);
                }
            }
        }
    }

    void DrawDebug2D(HWorld2D world)
    {
        world->m_World.DrawDebugData();
    }

    HCollisionShape2D NewCircleShape2D(float radius)
    {
        b2CircleShape* shape = new b2CircleShape();
        shape->m_p = b2Vec2(0.0f, 0.0f);
        shape->m_radius = radius;
        return shape;
    }

    HCollisionShape2D NewBoxShape2D(const Vectormath::Aos::Vector3& half_extents)
    {
        b2PolygonShape* shape = new b2PolygonShape();
        shape->SetAsBox(half_extents.getX(), half_extents.getY());
        return shape;
    }

    HCollisionShape2D NewPolygonShape2D(const float* vertices, uint32_t vertex_count)
    {
        b2PolygonShape* shape = new b2PolygonShape();
        shape->Set((b2Vec2*)vertices, vertex_count);
        return shape;
    }

    void DeleteCollisionShape2D(HCollisionShape2D shape)
    {
        delete (b2Shape*)shape;
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D shape)
    {
        if (shape == 0x0)
        {
            dmLogError("Collision objects must have a shape.");
            return 0x0;
        }
        switch (data.m_Type)
        {
        case COLLISION_OBJECT_TYPE_DYNAMIC:
            if (data.m_Mass == 0.0f)
            {
                dmLogError("Collision objects can not be dynamic and have zero mass.");
                return 0x0;
            }
            break;
        default:
            if (data.m_Mass > 0.0f)
            {
                dmLogError("Only dynamic collision objects can have a positive mass.");
                return 0x0;
            }
            break;
        }

        b2BodyDef def;
        if (world->m_GetWorldTransformCallback != 0x0)
        {
            if (data.m_UserData != 0x0)
            {
                Vectormath::Aos::Point3 position;
                Vectormath::Aos::Quat rotation;
                (*world->m_GetWorldTransformCallback)(data.m_UserData, position, rotation);
                def.position = b2Vec2(position.getX(), position.getY());
                def.angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
            }
            else
            {
                dmLogWarning("Collision object created at origin, this will result in a performance hit if multiple objects are created there in the same frame.");
            }
        }
        switch (data.m_Type)
        {
            case dmPhysics::COLLISION_OBJECT_TYPE_DYNAMIC:
                def.type = b2_dynamicBody;
                break;
            case dmPhysics::COLLISION_OBJECT_TYPE_STATIC:
                def.type = b2_staticBody;
                break;
            default:
                def.type = b2_kinematicBody;
                break;
        }
        def.userData = data.m_UserData;
        b2Body* body = world->m_World.CreateBody(&def);
        b2FixtureDef f_def;
        f_def.userData = data.m_UserData;
        f_def.filter.groupIndex = data.m_Group;
        f_def.filter.maskBits = data.m_Mask;
        f_def.shape = (b2Shape*)shape;
        b2MassData mass_data;
        f_def.shape->ComputeMass(&mass_data, 1.0f);
        f_def.density = data.m_Mass / mass_data.mass;
        f_def.friction = data.m_Friction;
        f_def.restitution = data.m_Restitution;
        f_def.isSensor = data.m_Type == COLLISION_OBJECT_TYPE_TRIGGER;
        b2Fixture* fixture = body->CreateFixture(&f_def);
        (void)fixture;
        return body;
    }

    void DeleteCollisionObject2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        b2Body* body = (b2Body*)collision_object;
        for (b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
        {
            body->DestroyFixture(fixture);
        }
        world->m_World.DestroyBody(body);
    }

    HCollisionShape2D GetCollisionShape2D(HCollisionObject2D collision_object)
    {
        return ((b2Body*)collision_object)->GetFixtureList()->GetShape();
    }

    void SetCollisionObjectUserData2D(HCollisionObject2D collision_object, void* user_data)
    {
        ((b2Body*)collision_object)->SetUserData(user_data);
    }

    void* GetCollisionObjectUserData2D(HCollisionObject2D collision_object)
    {
        return ((b2Body*)collision_object)->GetUserData();
    }

    void ApplyForce2D(HCollisionObject2D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position)
    {
        ((b2Body*)collision_object)->ApplyForce(b2Vec2(force.getX(), force.getY()), b2Vec2(position.getX(), position.getY()));
    }

    Vectormath::Aos::Vector3 GetTotalForce2D(HCollisionObject2D collision_object)
    {
        const b2Vec2& force = ((b2Body*)collision_object)->GetForce();
        return Vectormath::Aos::Vector3(force.x, force.y, 0.0f);
    }

    Vectormath::Aos::Point3 GetWorldPosition2D(HCollisionObject2D collision_object)
    {
        const b2Vec2& position = ((b2Body*)collision_object)->GetPosition();
        return Vectormath::Aos::Point3(position.x, position.y, 0.0f);
    }

    Vectormath::Aos::Quat GetWorldRotation2D(HCollisionObject2D collision_object)
    {
        float rotation = ((b2Body*)collision_object)->GetAngle();
        return Vectormath::Aos::Quat::rotationZ(rotation);
    }

    Vectormath::Aos::Vector3 GetLinearVelocity2D(HCollisionObject2D collision_object)
    {
        b2Vec2 lin_vel = ((b2Body*)collision_object)->GetLinearVelocity();
        return Vectormath::Aos::Vector3(lin_vel.x, lin_vel.y, 0.0f);
    }

    Vectormath::Aos::Vector3 GetAngularVelocity2D(HCollisionObject2D collision_object)
    {
        float ang_vel = ((b2Body*)collision_object)->GetAngularVelocity();
        return Vectormath::Aos::Vector3(0.0f, 0.0f, ang_vel);
    }

    void RequestRayCast2D(HWorld2D world, const RayCastRequest& request)
    {
        if (!world->m_RayCastRequests.Full())
            world->m_RayCastRequests.Push(request);
        else
            dmLogWarning("Ray cast query buffer is full (%d), ignoring request.", world->m_RayCastRequests.Capacity());
    }

    void SetDebugCallbacks2D(HContext2D context, const DebugCallbacks& callbacks)
    {
        context->m_DebugCallbacks = callbacks;
    }

    void ReplaceShape2D(HContext2D context, HCollisionShape2D old_shape, HCollisionShape2D new_shape)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
        {
            for (b2Body* body = context->m_Worlds[i]->m_World.GetBodyList(); body; body = body->GetNext())
            {
                if (body->GetFixtureList()->GetShape() == old_shape)
                {
                    b2MassData mass_data;
                    ((b2Shape*)new_shape)->ComputeMass(&mass_data, 1.0f);
                    b2FixtureDef def;
                    def.density = body->GetMass() / mass_data.mass;
                    def.filter = body->GetFixtureList()->GetFilterData();
                    def.friction = body->GetFixtureList()->GetFriction();
                    def.isSensor = body->GetFixtureList()->IsSensor();
                    def.restitution = body->GetFixtureList()->GetRestitution();
                    def.shape = (b2Shape*)new_shape;
                    def.userData = body->GetFixtureList()->GetUserData();
                    body->DestroyFixture(body->GetFixtureList());
                    body->CreateFixture(&def);
                    body->SetActive(true);
                }
            }
        }
    }
}
