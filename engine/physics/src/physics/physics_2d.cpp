#include "physics.h"

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "Box2D/Box2D.h"

#include "physics_2d.h"

namespace dmPhysics
{
    Context2D::Context2D()
    : m_Worlds()
    , m_DebugCallbacks()
    , m_Gravity(0.0f, -10.0f)
    , m_Socket(0)
    , m_Scale(1.0f)
    , m_InvScale(1.0f)
    , m_ContactImpulseLimit(0.0f)
    , m_TriggerEnterLimit(0.0f)
    , m_RayCastLimit(0)
    , m_TriggerOverlapCapacity(0)
    {

    }

    World2D::World2D(HContext2D context, const NewWorldParams& params)
    : m_TriggerOverlaps(context->m_TriggerOverlapCapacity)
    , m_Context(context)
    , m_World(context->m_Gravity)
    , m_RayCastRequests()
    , m_DebugDraw(&context->m_DebugCallbacks)
    , m_ContactListener(this)
    , m_GetWorldTransformCallback(params.m_GetWorldTransformCallback)
    , m_SetWorldTransformCallback(params.m_SetWorldTransformCallback)
    , m_GetScaleCallback(params.m_GetScaleCallback)
    {
    	m_RayCastRequests.SetCapacity(context->m_RayCastLimit);
        OverlapCacheInit(&m_TriggerOverlaps);
    }

    ProcessRayCastResultCallback2D::ProcessRayCastResultCallback2D()
    : m_Context(0x0)
    , m_IgnoredUserData(0x0)
    , m_CollisionGroup((uint16_t)~0u)
    , m_CollisionMask((uint16_t)~0u)
    {

    }

    ProcessRayCastResultCallback2D::~ProcessRayCastResultCallback2D()
    {

    }

    float32 ProcessRayCastResultCallback2D::ReportFixture(b2Fixture* fixture, int32_t index, const b2Vec2& point, const b2Vec2& normal, float32 fraction)
    {
        // Never hit triggers
        if (fixture->IsSensor())
            return -1.f;
        if (fixture->GetBody()->GetUserData() == m_IgnoredUserData)
            return -1.f;
        else if ((fixture->GetFilterData(index).categoryBits & m_CollisionMask) && (fixture->GetFilterData(index).maskBits & m_CollisionGroup))
        {
            m_Response.m_Hit = 1;
            m_Response.m_Fraction = fraction;
            m_Response.m_CollisionObjectGroup = fixture->GetFilterData(index).categoryBits;
            m_Response.m_CollisionObjectUserData = fixture->GetBody()->GetUserData();
            FromB2(normal, m_Response.m_Normal, 1.0f); // Don't scale normal
            FromB2(point, m_Response.m_Position, m_Context->m_InvScale);
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
                // verify that the impulse is large enough to be reported
                float max_impulse = 0.0f;
                for (int32 i = 0; i < impulse->count; ++i)
                {
                    max_impulse = dmMath::Max(max_impulse, impulse->normalImpulses[i]);
                }
                // early out if the impulse is too small to be reported
                if (max_impulse < m_World->m_Context->m_ContactImpulseLimit)
                    return;

                b2Fixture* fixture_a = contact->GetFixtureA();
                b2Fixture* fixture_b = contact->GetFixtureB();
                int32_t index_a = contact->GetChildIndexA();
                int32_t index_b = contact->GetChildIndexB();

                if (collision_callback)
                {
                    collision_callback(fixture_a->GetUserData(),
                                       fixture_a->GetFilterData(index_a).categoryBits,
                                       fixture_b->GetUserData(),
                                       fixture_b->GetFilterData(index_b).categoryBits,
                                       m_TempStepWorldContext->m_CollisionUserData);
                }
                if (contact_point_callback)
                {
                    b2WorldManifold world_manifold;
                    contact->GetWorldManifold(&world_manifold);
                    float inv_scale = m_World->m_Context->m_InvScale;
                    int32 n_p = dmMath::Min(contact->GetManifold()->pointCount, impulse->count);
                    for (int32 i = 0; i < n_p; ++i)
                    {
                        ContactPoint cp;
                        FromB2(world_manifold.points[i], cp.m_PositionA, inv_scale);
                        FromB2(world_manifold.points[i], cp.m_PositionB, inv_scale);
                        cp.m_UserDataA = fixture_a->GetBody()->GetUserData();
                        cp.m_UserDataB = fixture_b->GetBody()->GetUserData();
                        FromB2(world_manifold.normal, cp.m_Normal, 1.0f); // Don't scale normal
                        b2Vec2 rv = fixture_b->GetBody()->GetLinearVelocity() - fixture_a->GetBody()->GetLinearVelocity();
                        FromB2(rv, cp.m_RelativeVelocity, inv_scale);
                        cp.m_Distance = contact->GetManifold()->points[i].distance * inv_scale;
                        cp.m_AppliedImpulse = impulse->normalImpulses[i] * inv_scale;
                        cp.m_MassA = fixture_a->GetBody()->GetMass();
                        cp.m_MassB = fixture_b->GetBody()->GetMass();
                        cp.m_GroupA = fixture_a->GetFilterData(index_a).categoryBits;
                        cp.m_GroupB = fixture_b->GetFilterData(index_b).categoryBits;
                        contact_point_callback(cp, m_TempStepWorldContext->m_ContactPointUserData);
                    }
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
        if (params.m_Scale < MIN_SCALE || params.m_Scale > MAX_SCALE)
        {
            dmLogFatal("Physics scale is outside the valid range %.2f - %.2f.", MIN_SCALE, MAX_SCALE);
            return 0x0;
        }
        Context2D* context = new Context2D();
        context->m_Worlds.SetCapacity(params.m_WorldCount);
        ToB2(params.m_Gravity, context->m_Gravity, params.m_Scale);
        context->m_Scale = params.m_Scale;
        context->m_InvScale = 1.0f / params.m_Scale;
        context->m_ContactImpulseLimit = params.m_ContactImpulseLimit * params.m_Scale;
        context->m_TriggerEnterLimit = params.m_TriggerEnterLimit * params.m_Scale;
        context->m_RayCastLimit = params.m_RayCastLimit2D;
        context->m_TriggerOverlapCapacity = params.m_TriggerOverlapCapacity;
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
        world->m_World.SetContinuousPhysics(false);
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

    static void UpdateOverlapCache(OverlapCache* cache, HContext2D context, b2Contact* contact_list, const StepWorldContext& step_context);

    void StepWorld2D(HWorld2D world, const StepWorldContext& step_context)
    {
        float dt = step_context.m_DT;
        HContext2D context = world->m_Context;
        float scale = context->m_Scale;
        // Epsilon defining what transforms are considered noise and not
        // Values are picked by inspection, current rot value is roughly equivalent to 1 degree
        const float POS_EPSILON = 0.00005f * scale;
        const float ROT_EPSILON = 0.00007f;
        // Update transforms of kinematic bodies
        if (world->m_GetWorldTransformCallback)
        {
            DM_PROFILE(Physics, "UpdateKinematic");
            for (b2Body* body = world->m_World.GetBodyList(); body; body = body->GetNext())
            {
                if (body->GetType() == b2_kinematicBody)
                {
                    Vectormath::Aos::Point3 old_position = GetWorldPosition2D(context, body);
                    Vectormath::Aos::Quat old_rotation = GetWorldRotation2D(context, body);
                    dmTransform::Transform world_transform;
                    (*world->m_GetWorldTransformCallback)(body->GetUserData(), world_transform);
                    Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                    // Ignore z-component
                    position.setZ(0.0f);
                    Vectormath::Aos::Quat rotation = world_transform.GetRotation();
                    float dp = distSqr(old_position, position);
                    float dr = norm(rotation - old_rotation);
                    if (dp > POS_EPSILON || dr > ROT_EPSILON)
                    {
                        float angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
                        b2Vec2 b2_position;
                        ToB2(position, b2_position, scale);
                        body->SetTransform(b2_position, angle);
                        body->SetSleepingAllowed(false);
                    }
                    else
                    {
                        body->SetSleepingAllowed(true);
                    }
                }

                if( body->GetType() != b2_staticBody )
                {
                    Vectormath::Aos::Vector3* shape_scales = 0;
                    uint32_t shape_count = 0;
                    Vectormath::Aos::Vector3 object_scale;
                    (*world->m_GetScaleCallback)(body->GetUserData(), &shape_scales, &shape_count, &object_scale);

                    b2Fixture* fix = body->GetFixtureList();
                    uint32_t i = 0;
                    while( fix && i < shape_count )
                    {
                        b2Shape* shape = fix->GetShape();
                        shape->m_radius = shape_scales[i].getX() * object_scale.getX();
                        fix = fix->GetNext();
                    }

                    // TODO: Only set this if we actually changed the scaling
                    if( shape_count > 0 )
                        body->SetSleepingAllowed(false);
                }
            }
        }
        {
            DM_PROFILE(Physics, "StepSimulation");
            world->m_ContactListener.SetStepWorldContext(&step_context);
            world->m_World.Step(dt, 10, 10);
            float inv_scale = world->m_Context->m_InvScale;
            // Update transforms of dynamic bodies
            if (world->m_SetWorldTransformCallback)
            {
                for (b2Body* body = world->m_World.GetBodyList(); body; body = body->GetNext())
                {
                    if (body->GetType() == b2_dynamicBody && body->IsActive())
                    {
                        Vectormath::Aos::Point3 position;
                        FromB2(body->GetPosition(), position, inv_scale);
                        Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat::rotationZ(body->GetAngle());
                        (*world->m_SetWorldTransformCallback)(body->GetUserData(), position, rotation);
                    }
                }
            }
        }
        // Perform requested ray casts
        uint32_t size = world->m_RayCastRequests.Size();
        if (size > 0)
        {
            DM_PROFILE(Physics, "RayCasts");
            ProcessRayCastResultCallback2D callback;
            callback.m_Context = world->m_Context;
            for (uint32_t i = 0; i < size; ++i)
            {
                RayCastRequest& request = world->m_RayCastRequests[i];
                b2Vec2 from;
                ToB2(request.m_From, from, scale);
                b2Vec2 to;
                ToB2(request.m_To, to, scale);
                callback.m_IgnoredUserData = request.m_IgnoredUserData;
                callback.m_CollisionMask = request.m_Mask;
                callback.m_Response.m_Hit = 0;
                world->m_World.RayCast(&callback, from, to);
                (*step_context.m_RayCastCallback)(callback.m_Response, request, step_context.m_RayCastUserData);
            }
            world->m_RayCastRequests.SetSize(0);
        }
        // Report sensor collisions
        if (step_context.m_CollisionCallback)
        {
            DM_PROFILE(Physics, "CollisionCallbacks");
            for (b2Contact* contact = world->m_World.GetContactList(); contact; contact = contact->GetNext())
            {
                b2Fixture* fixture_a = contact->GetFixtureA();
                b2Fixture* fixture_b = contact->GetFixtureB();
                if (contact->IsTouching() && (fixture_a->IsSensor() || fixture_b->IsSensor()))
                {
                    int32_t index_a = contact->GetChildIndexA();
                    int32_t index_b = contact->GetChildIndexB();
                    step_context.m_CollisionCallback(fixture_a->GetUserData(),
                                                fixture_a->GetFilterData(index_a).categoryBits,
                                                fixture_b->GetUserData(),
                                                fixture_b->GetFilterData(index_b).categoryBits,
                                                step_context.m_CollisionUserData);
                }
            }
        }
        UpdateOverlapCache(&world->m_TriggerOverlaps, context, world->m_World.GetContactList(), step_context);

        world->m_World.DrawDebugData();
    }

    void UpdateOverlapCache(OverlapCache* cache, HContext2D context, b2Contact* contact_list, const StepWorldContext& step_context)
    {
        DM_PROFILE(Physics, "TriggerCallbacks");
        OverlapCacheReset(cache);
        OverlapCacheAddData add_data;
        add_data.m_TriggerEnteredCallback = step_context.m_TriggerEnteredCallback;
        add_data.m_TriggerEnteredUserData = step_context.m_TriggerEnteredUserData;
        for (b2Contact* contact = contact_list; contact; contact = contact->GetNext())
        {
            b2Fixture* fixture_a = contact->GetFixtureA();
            b2Fixture* fixture_b = contact->GetFixtureB();
            if (contact->IsTouching() && (fixture_a->IsSensor() || fixture_b->IsSensor()))
            {
                float max_distance = 0.0f;
                b2Manifold* manifold = contact->GetManifold();
                for (int32 i = 0; i < manifold->pointCount; ++i)
                {
                    max_distance = dmMath::Max(max_distance, manifold->points[i].distance);
                }
                if (max_distance >= context->m_TriggerEnterLimit)
                {
                    b2Body* body_a = fixture_a->GetBody();
                    b2Body* body_b = fixture_b->GetBody();
                    add_data.m_ObjectA = body_a;
                    add_data.m_UserDataA = body_a->GetUserData();
                    add_data.m_ObjectB = body_b;
                    add_data.m_UserDataB = body_b->GetUserData();
                    int32_t index_a = contact->GetChildIndexA();
                    int32_t index_b = contact->GetChildIndexB();
                    add_data.m_GroupA = fixture_a->GetFilterData(index_a).categoryBits;
                    add_data.m_GroupB = fixture_b->GetFilterData(index_b).categoryBits;
                    OverlapCacheAdd(cache, add_data);
                }
            }
        }
        OverlapCachePruneData prune_data;
        prune_data.m_TriggerExitedCallback = step_context.m_TriggerExitedCallback;
        prune_data.m_TriggerExitedUserData = step_context.m_TriggerExitedUserData;
        OverlapCachePrune(cache, prune_data);
    }

    void SetDrawDebug2D(HWorld2D world, bool draw_debug)
    {
        int flags = 0;
        if (draw_debug)
        {
            flags = b2Draw::e_jointBit | b2Draw::e_pairBit | b2Draw::e_shapeBit;
        }

        world->m_DebugDraw.SetFlags(flags);
    }

    HCollisionShape2D NewCircleShape2D(HContext2D context, float radius)
    {
        b2CircleShape* shape = new b2CircleShape();
        shape->m_p = b2Vec2(0.0f, 0.0f);
        shape->m_radius = radius * context->m_Scale;
        return shape;
    }

    HCollisionShape2D NewBoxShape2D(HContext2D context, const Vectormath::Aos::Vector3& half_extents)
    {
        b2PolygonShape* shape = new b2PolygonShape();
        float scale = context->m_Scale;
        shape->SetAsBox(half_extents.getX() * scale, half_extents.getY() * scale);
        return shape;
    }

    HCollisionShape2D NewPolygonShape2D(HContext2D context, const float* vertices, uint32_t vertex_count)
    {
        b2PolygonShape* shape = new b2PolygonShape();
        float scale = context->m_Scale;
        const uint32_t elem_count = vertex_count * 2;
        float* v = new float[elem_count];
        for (uint32_t i = 0; i < elem_count; ++i)
        {
            v[i] = vertices[i] * scale;
        }
        shape->Set((b2Vec2*)v, vertex_count);
        delete [] v;
        return shape;
    }

    HHullSet2D NewHullSet2D(HContext2D context, const float* vertices, uint32_t vertex_count,
                            const HullDesc* hulls, uint32_t hull_count)
    {
        assert(sizeof(HullDesc) == sizeof(const b2HullSet::Hull));
        // NOTE: We cast HullDesc* to const b2HullSet::Hull* here
        // We assume that they have the same physical layout
        // NOTE: No scaling of the vertices here since they are already assumed to be in a virtual "unit-space"
        b2HullSet* hull_set = new b2HullSet((const b2Vec2*) vertices, vertex_count,
                             (const b2HullSet::Hull*) hulls, hull_count);
        return hull_set;
    }

    void DeleteHullSet2D(HHullSet2D hull_set)
    {
        delete (b2HullSet*) hull_set;
    }

    HCollisionShape2D NewGridShape2D(HContext2D context, HHullSet2D hull_set,
                                     const Vectormath::Aos::Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count)
    {
        float scale = context->m_Scale;
        b2Vec2 p;
        ToB2(position, p, scale);
        return new b2GridShape((b2HullSet*) hull_set, p, cell_width * scale, cell_height * scale, row_count, column_count);
    }

    void SetGridShapeHull(HCollisionObject2D collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags)
    {
        b2Body* body = (b2Body*) collision_object;
        b2Fixture* fixture = body->GetFixtureList();
        for (uint32_t i = 0; i < shape_index && fixture != 0x0; ++i)
        {
            fixture = fixture->GetNext();
        }
        assert(fixture != 0x0);
        assert(fixture->GetShape()->GetType() == b2Shape::e_grid);
        b2GridShape* grid_shape = (b2GridShape*) fixture->GetShape();
        b2GridShape::CellFlags f;
        f.m_FlipHorizontal = flags.m_FlipHorizontal;
        f.m_FlipVertical = flags.m_FlipVertical;
        grid_shape->SetCellHull(body, row, column, hull, f);
    }

    void SetCollisionObjectFilter(HCollisionObject2D collision_shape,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask)
    {
        b2Body* body = (b2Body*) collision_shape;
        b2Fixture* fixture = body->GetFixtureList();
        for (uint32_t i = 0; i < shape; ++i)
        {
            fixture = fixture->GetNext();
        }

        b2Filter filter = fixture->GetFilterData(child);
        filter.categoryBits = group;
        filter.maskBits = mask;
        fixture->SetFilterData(filter, child);
    }

    void DeleteCollisionShape2D(HCollisionShape2D shape)
    {
        delete (b2Shape*)shape;
    }

    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes, uint32_t shape_count)
    {
        return NewCollisionObject2D(world, data, shapes, 0, 0, shape_count);
    }

    Vectormath::Aos::Vector3 GetScale2D(HCollisionShape2D _shape)
    {
        b2Shape* shape = (b2Shape*)_shape;
        return Vectormath::Aos::Vector3(shape->m_radius);
    }

    /*
     * Quaternion to complex number transform
     *
     * A quaternion around axis (0,0,1) with angle alpha is given by:
     *
     * q = (0, 0, sin(alpha/2), cos(alpha/2))   (1)
     *
     * The corresponding complex number is given by
     *
     * c = (cos(alpha/2), sin(alpha/2))         (2)
     *
     * The "double angle rule":
     *
     *  sin(2x) = 2sin(x)cos(x)
     * <=>
     *  sin(x) = 2sin(x/2)cos(x/2)              (3)
     *
     * and
     *
     *  cos(2x) = 1 - 2sin^2(x)
     * <=>
     *  cos(x) = 1 - 2sin^2(x/2)                (4)
     *
     *  The complex representation from in terms of a quaternion.
     *  Identify terms using (3) and (4)
     *
     *  c = (1 - 2 * q.z * q.z, 2 * q.z * q.w)
     *
     */
    static b2Shape* TransformCopyShape(HContext2D context,
                                       const b2Shape* shape,
                                       const Vectormath::Aos::Vector3& translation,
                                       const Vectormath::Aos::Quat& rotation,
                                       float scale)
    {
        b2Vec2 t;
        ToB2(translation, t, context->m_Scale * scale);
        b2Rot r;
        r.SetComplex(1 - 2 * rotation.getZ() * rotation.getZ(), 2 * rotation.getZ() * rotation.getW());
        b2Transform transform(t, r);
        b2Shape* ret = 0;

        switch(shape->m_type)
        {
        case b2Shape::e_circle:
        {
            const b2CircleShape* circle_shape = (const b2CircleShape*) shape;
            b2CircleShape* circle_shape_prim = new b2CircleShape(*circle_shape);
            circle_shape_prim->m_p = TransformScaleB2(transform, scale, circle_shape->m_p);
            circle_shape_prim->m_radius *= scale;
            ret = circle_shape_prim;
        }
            break;

        case b2Shape::e_edge:
        {
            const b2EdgeShape* edge_shape = (const b2EdgeShape*) shape;
            b2EdgeShape* edge_shape_prim = new b2EdgeShape(*edge_shape);
            if (edge_shape_prim->m_hasVertex0)
                edge_shape_prim->m_vertex0 = TransformScaleB2(transform, scale, edge_shape->m_vertex0);

            edge_shape_prim->m_vertex1 = TransformScaleB2(transform, scale, edge_shape->m_vertex1);
            edge_shape_prim->m_vertex2 = TransformScaleB2(transform, scale, edge_shape->m_vertex2);

            if (edge_shape_prim->m_hasVertex3)
                edge_shape_prim->m_vertex3 = TransformScaleB2(transform, scale, edge_shape->m_vertex3);

            ret = edge_shape_prim;
        }
            break;

        case b2Shape::e_polygon:
        {
            const b2PolygonShape* poly_shape = (const b2PolygonShape*) shape;
            b2PolygonShape* poly_shape_prim = new b2PolygonShape(*poly_shape);

            b2Vec2 tmp[b2_maxPolygonVertices];
            int32 n = poly_shape->GetVertexCount();
            for (int32 i = 0; i < n; ++i)
            {
                tmp[i] = TransformScaleB2(transform, scale, poly_shape->GetVertex(i));
            }

            poly_shape_prim->Set(tmp, n);

            ret = poly_shape_prim;
        }
            break;

        case b2Shape::e_grid:
        {
            const b2GridShape* grid_shape = (const b2GridShape*) shape;
            b2GridShape* grid_shape_prim = new b2GridShape(grid_shape->m_hullSet, TransformScaleB2(transform, scale, grid_shape->m_position),
                    grid_shape->m_cellWidth * scale, grid_shape->m_cellHeight * scale, grid_shape->m_rowCount, grid_shape->m_columnCount);
            ret = grid_shape_prim;
        }
            break;

        default:
            ret = (b2Shape*) shape;
            break;
        }

        return ret;
    }

    static void FreeShape(const b2Shape* shape)
    {
        switch(shape->m_type)
        {
        case b2Shape::e_circle:
        {
            b2CircleShape* circle_shape = (b2CircleShape*) shape;
            delete circle_shape;
        }
        break;

        case b2Shape::e_edge:
        {
            b2EdgeShape* edge_shape = (b2EdgeShape*) shape;
            delete edge_shape;
        }
        break;

        case b2Shape::e_polygon:
        {
            b2PolygonShape* poly_shape = (b2PolygonShape*) shape;
            delete poly_shape;
        }
        break;

        case b2Shape::e_grid:
        {
            b2GridShape* grid_shape = (b2GridShape*) shape;
            delete grid_shape;
        }
        break;

        default:
            // pass
            break;
        }
    }

    /*
     * NOTE: In order to support shape transform we create a copy of shapes using the function TransformCopyShape() above
     * This is required as the transform is part of the shape and due to absence of a compound shape, aka list shape
     * with per-child transform.
     */
    HCollisionObject2D NewCollisionObject2D(HWorld2D world, const CollisionObjectData& data, HCollisionShape2D* shapes,
                                            Vectormath::Aos::Vector3* translations, Vectormath::Aos::Quat* rotations,
                                            uint32_t shape_count)
    {
        if (shape_count == 0)
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

        HContext2D context = world->m_Context;
        b2BodyDef def;
        float scale = 1.0f;
        if (world->m_GetWorldTransformCallback != 0x0)
        {
            if (data.m_UserData != 0x0)
            {
                dmTransform::Transform world_transform;
                (*world->m_GetWorldTransformCallback)(data.m_UserData, world_transform);
                Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat(world_transform.GetRotation());
                ToB2(position, def.position, context->m_Scale);
                def.angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
                scale = world_transform.GetUniformScale();
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
        def.linearDamping = data.m_LinearDamping;
        def.angularDamping = data.m_AngularDamping;
        def.fixedRotation = data.m_LockedRotation;
        def.active = data.m_Enabled;
        b2Body* body = world->m_World.CreateBody(&def);
        Vectormath::Aos::Vector3 zero_vec3 = Vectormath::Aos::Vector3(0);
        for (uint32_t i = 0; i < shape_count; ++i) {
            // Add shapes in reverse order. The fixture list in the body
            // is a single linked list and cells are prepended.
            uint32_t reverse_i = shape_count - i - 1;
            b2Shape* s = (b2Shape*)shapes[reverse_i];

            if (translations && rotations)
            {
                s = TransformCopyShape(context, s, translations[reverse_i], rotations[reverse_i], scale);
            }
            else
            {
                s = TransformCopyShape(context, s, zero_vec3, Vectormath::Aos::Quat::identity(), scale);
            }

            b2FixtureDef f_def;
            f_def.userData = data.m_UserData;
            f_def.filter.categoryBits = data.m_Group;
            f_def.filter.maskBits = data.m_Mask;
            f_def.shape = s;
            b2MassData mass_data;
            f_def.shape->ComputeMass(&mass_data, 1.0f);
            f_def.density = data.m_Mass / mass_data.mass;
            f_def.friction = data.m_Friction;
            f_def.restitution = data.m_Restitution;
            f_def.isSensor = data.m_Type == COLLISION_OBJECT_TYPE_TRIGGER;
            b2Fixture* fixture = body->CreateFixture(&f_def);
            (void)fixture;
        }
        return body;
    }

    void DeleteCollisionObject2D(HWorld2D world, HCollisionObject2D collision_object)
    {
        // NOTE: This code assumes stuff about internals in box2d.
        // When the next-pointer is cleared etc. Beware! :-)
        // DestroyBody() should be enough in general but we have to run over all fixtures in order to free allocated shapes
        // See comment above about shapes and transforms

        OverlapCacheRemove(&world->m_TriggerOverlaps, collision_object);
        b2Body* body = (b2Body*)collision_object;
        b2Fixture* fixture = body->GetFixtureList();
        while (fixture)
        {
            // We must save next fixture. The next pointer is set to null in DestoryFixture()
            b2Fixture* save_next = fixture->GetNext();

            b2Shape* shape = fixture->GetShape();
            body->DestroyFixture(fixture);
            FreeShape(shape); // NOTE: shape can't be freed prior to DestroyFixture
            fixture = save_next;
        }
        world->m_World.DestroyBody(body);
    }

    uint32_t GetCollisionShapes2D(HCollisionObject2D collision_object, HCollisionShape2D* out_buffer, uint32_t buffer_size)
    {
        b2Fixture* fixture = ((b2Body*)collision_object)->GetFixtureList();
        uint32_t i;
        for (i = 0; i < buffer_size && fixture != 0x0; ++i)
        {
            out_buffer[i] = fixture->GetShape();
            fixture = fixture->GetNext();
        }
        return i;
    }

    void SetCollisionObjectUserData2D(HCollisionObject2D collision_object, void* user_data)
    {
        ((b2Body*)collision_object)->SetUserData(user_data);
    }

    void* GetCollisionObjectUserData2D(HCollisionObject2D collision_object)
    {
        return ((b2Body*)collision_object)->GetUserData();
    }

    void ApplyForce2D(HContext2D context, HCollisionObject2D collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position)
    {
        float scale = context->m_Scale;
        b2Vec2 b2_force;
        ToB2(force, b2_force, scale);
        b2Vec2 b2_position;
        ToB2(position, b2_position, scale);
        ((b2Body*)collision_object)->ApplyForce(b2_force, b2_position);
    }

    Vectormath::Aos::Vector3 GetTotalForce2D(HContext2D context, HCollisionObject2D collision_object)
    {
        const b2Vec2& b2_force = ((b2Body*)collision_object)->GetForce();
        Vectormath::Aos::Vector3 force;
        FromB2(b2_force, force, context->m_InvScale);
        return force;
    }

    Vectormath::Aos::Point3 GetWorldPosition2D(HContext2D context, HCollisionObject2D collision_object)
    {
        const b2Vec2& b2_position = ((b2Body*)collision_object)->GetPosition();
        Vectormath::Aos::Point3 position;
        FromB2(b2_position, position, context->m_InvScale);
        return position;
    }

    Vectormath::Aos::Quat GetWorldRotation2D(HContext2D context, HCollisionObject2D collision_object)
    {
        float rotation = ((b2Body*)collision_object)->GetAngle();
        return Vectormath::Aos::Quat::rotationZ(rotation);
    }

    Vectormath::Aos::Vector3 GetLinearVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        b2Vec2 b2_lin_vel = ((b2Body*)collision_object)->GetLinearVelocity();
        Vectormath::Aos::Vector3 lin_vel;
        FromB2(b2_lin_vel, lin_vel, context->m_InvScale);
        return lin_vel;
    }

    Vectormath::Aos::Vector3 GetAngularVelocity2D(HContext2D context, HCollisionObject2D collision_object)
    {
        float ang_vel = ((b2Body*)collision_object)->GetAngularVelocity();
        return Vectormath::Aos::Vector3(0.0f, 0.0f, ang_vel);
    }

    bool IsEnabled2D(HCollisionObject2D collision_object)
    {
        return ((b2Body*)collision_object)->IsActive();
    }

    void SetEnabled2D(HWorld2D world, HCollisionObject2D collision_object, bool enabled)
    {
        DM_PROFILE(Physics, "SetEnabled");
        bool prev_enabled = IsEnabled2D(collision_object);
        // Avoid multiple adds/removes
        if (prev_enabled == enabled)
            return;
        b2Body* body = ((b2Body*)collision_object);
        body->SetActive(enabled);
        if (enabled)
        {
            body->SetAwake(true);
            if (world->m_GetWorldTransformCallback)
            {
                dmTransform::Transform world_transform;
                (*world->m_GetWorldTransformCallback)(body->GetUserData(), world_transform);
                Vectormath::Aos::Point3 position = Vectormath::Aos::Point3(world_transform.GetTranslation());
                Vectormath::Aos::Quat rotation = Vectormath::Aos::Quat(world_transform.GetRotation());
                float angle = atan2(2.0f * (rotation.getW() * rotation.getZ() + rotation.getX() * rotation.getY()), 1.0f - 2.0f * (rotation.getY() * rotation.getY() + rotation.getZ() * rotation.getZ()));
                b2Vec2 b2_position;
                ToB2(position, b2_position, world->m_Context->m_Scale);
                body->SetTransform(b2_position, angle);
            }
        }
        else
        {
            // Reset state
            body->SetAwake(false);
        }
    }

    bool IsSleeping2D(HCollisionObject2D collision_object)
    {
        b2Body* body = ((b2Body*)collision_object);
        return !body->IsAwake();
    }

    void SetLockedRotation2D(HCollisionObject2D collision_object, bool locked_rotation) {
        b2Body* body = ((b2Body*)collision_object);
        body->SetFixedRotation(locked_rotation);
        if (locked_rotation) {
            body->SetAngularVelocity(0.0f);
        }
    }

    float GetLinearDamping2D(HCollisionObject2D collision_object) {
        b2Body* body = ((b2Body*)collision_object);
        return body->GetLinearDamping();
    }

    void SetLinearDamping2D(HCollisionObject2D collision_object, float linear_damping) {
        b2Body* body = ((b2Body*)collision_object);
        body->SetLinearDamping(linear_damping);
    }

    float GetAngularDamping2D(HCollisionObject2D collision_object) {
        b2Body* body = ((b2Body*)collision_object);
        return body->GetAngularDamping();
    }

    void SetAngularDamping2D(HCollisionObject2D collision_object, float angular_damping) {
        b2Body* body = ((b2Body*)collision_object);
        body->SetAngularDamping(angular_damping);
    }

    float GetMass2D(HCollisionObject2D collision_object) {
        b2Body* body = ((b2Body*)collision_object);
        return body->GetMass();
    }

    void RequestRayCast2D(HWorld2D world, const RayCastRequest& request)
    {
        if (!world->m_RayCastRequests.Full())
        {
            // Verify that the ray is not 0-length
            // We need to remove the z-value before calculating length (DEF-1286)
            const Vectormath::Aos::Point3 from2d = Vectormath::Aos::Point3(request.m_From.getX(), request.m_From.getY(), 0.0);
            const Vectormath::Aos::Point3 to2d = Vectormath::Aos::Point3(request.m_To.getX(), request.m_To.getY(), 0.0);
            if (Vectormath::Aos::lengthSqr(to2d - from2d) <= 0.0f)
            {
                dmLogWarning("Ray had 0 length when ray casting, ignoring request.");
            }
            else
            {
                world->m_RayCastRequests.Push(request);
            }
        }
        else
        {
            dmLogWarning("Ray cast query buffer is full (%d), ignoring request.", world->m_RayCastRequests.Capacity());
        }
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
                b2Fixture* fixture = body->GetFixtureList();
                while (fixture)
                {
                    b2Fixture* next_fixture = fixture->GetNext();
                    if (fixture->GetShape() == old_shape)
                    {
                        b2MassData mass_data;
                        ((b2Shape*)new_shape)->ComputeMass(&mass_data, 1.0f);
                        b2FixtureDef def;
                        def.density = body->GetMass() / mass_data.mass;
                        def.filter = fixture->GetFilterData(0);
                        def.friction = fixture->GetFriction();
                        def.isSensor = fixture->IsSensor();
                        def.restitution = fixture->GetRestitution();
                        def.shape = (b2Shape*)new_shape;
                        def.userData = fixture->GetUserData();
                        b2Fixture* new_fixture = body->CreateFixture(&def);

                        // Copy filter data per child
                        b2Shape* new_b2_shape = (b2Shape*) new_shape;
                        b2Shape* old_b2_shape = fixture->GetShape();
                        if (new_b2_shape->m_filterPerChild)
                        {
                            uint32_t new_child_count = new_b2_shape->GetChildCount();
                            uint32_t old_child_count = old_b2_shape->GetChildCount();
                            for (uint32_t c = 0; c < new_child_count; ++c)
                            {
                                b2Filter filter;

                                if (c < old_child_count)
                                {
                                    filter = fixture->GetFilterData(c);
                                }
                                else
                                {
                                    // The new shape has more children than the old
                                    // Use filter data from the first child
                                    filter = fixture->GetFilterData(0);
                                }

                                new_fixture->SetFilterData(filter, c);
                            }
                        }

                        body->DestroyFixture(fixture);
                        body->SetActive(true);
                    }
                    fixture = next_fixture;
                }
            }
        }
    }
}
