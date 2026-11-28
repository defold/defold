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

#include <stdlib.h>
#include <box3d/constants.h>
#include <box3d/defold_contacts.h>
#include <dmsdk/physics/box3d.hpp>
#include "physics.h"
#include "box3d/physics_box3d.h"
#include <dlib/log.h>
#include <dlib/array.h>
#include <dlib/profile.h>
#include <dlib/transform.h>
#include <dlib/configfile.h>


namespace dmPhysics
{
    using namespace dmVMath;
    struct Body;
    struct Context3D
    {
        NewContextParams   m_Params;
        DebugCallbacks     m_Debug;
        dmArray<HWorld3D>  m_Worlds;
        dmMessage::HSocket m_Socket;
        int                m_Substeps;
        int                m_WorkerCount;
    };
    struct World3D
    {
        HContext3D              m_Context;
        NewWorldParams          m_Params;
        b3WorldId               m_Id;
        dmArray<Body*>          m_Bodies;
        dmArray<RayCastRequest> m_Rays;
        uint64_t                m_Step;
        bool                    m_Paused;
        bool                    m_SkipComponentEvents;
        bool                    m_DrawDebug;
    };
    enum GeometryType
    {
        BOX, SPHERE, CAPSULE, HULL, MESH
    };
    struct Geometry
    {
        GeometryType m_Type;
        b3Vec3       m_Size;
        b3HullData*  m_Hull;
        b3MeshData*  m_Mesh;
        uint32_t     m_Refs;
    };
    struct Attachment
    {
        Geometry*   m_Geometry;
        b3Transform m_Local;
        b3ShapeId   m_Id;
        bool        m_Owned;
    };
    struct Body
    {
        HWorld3D            m_World;
        CollisionObjectData m_Data;
        dmArray<Attachment> m_Shapes;
        b3BodyId            m_Id;
        b3Vec3              m_Scale;
        uint32_t            m_Index;
        Vector3             m_Force;
        uint64_t            m_ForceStep;
        b3Vec3              m_DisabledLinearVelocity;
        b3Vec3              m_DisabledAngularVelocity;
        bool                m_AwakeOnEnable;
    };

    static b3Vec3 Vec(const Vector3& v)
    {
        b3Vec3 r = { v.getX(), v.getY(), v.getZ() };
        return r;
    }

    static Vector3 Vector(b3Vec3 v)
    {
        return Vector3(v.x, v.y, v.z);
    }

    static b3Quat Rot(const Quat& q)
    {
        b3Quat r = { { q.getX(), q.getY(), q.getZ() }, q.getW() };
        return r;
    }

    static Quat Rotation(b3Quat q)
    {
        return Quat(q.v.x, q.v.y, q.v.z, q.s);
    }

    static Body* Owner(b3ShapeId id)
    {
        return (Body*)b3Shape_GetUserData(id);
    }

    float GetBox3DScale(b3WorldId id)
    {
        return ((World3D*)b3World_GetUserData(id))->m_Context->m_Params.m_Scale;
    }

    void SetBox3DComponentEvents(b3WorldId id, bool enabled)
    {
        ((World3D*)b3World_GetUserData(id))->m_SkipComponentEvents = !enabled;
    }

    void SetBox3DPaused(b3WorldId id, bool paused)
    {
        ((World3D*)b3World_GetUserData(id))->m_Paused = paused;
    }

    void PrepareBox3DSleepingContacts(b3WorldId id)
    {
        dmBox3DPrepareSleepingContacts(id);
    }

    void SetBox3DTransform(b3BodyId id, b3Pos p, b3Quat q)
    {
        b3Body_SetTransform(id, p, q);
        Body* body = (Body*)b3Body_GetUserData(id);
        HWorld3D world = body->m_World;
        if (world->m_Params.m_SetWorldTransformCallback)
        {
            world->m_Params.m_SetWorldTransformCallback(body->m_Data.m_UserData, Point3(Vector(p) / world->m_Context->m_Params.m_Scale), Rotation(q));
        }
    }

    bool IsBox3DBackend()
    {
        return true;
    }

    HContext3D NewContext3D(const NewContextParams& params)
    {
        if (params.m_Scale < MIN_SCALE || params.m_Scale > MAX_SCALE)
        {
            return 0;
        }

        HContext3D context = new Context3D();
        context->m_Params = params;
        context->m_Substeps = params.m_Config ? dmConfigFile::GetInt(params.m_Config, "box3d.sub_step_count", 2) : 2;
        context->m_Substeps = dmMath::Max(1, context->m_Substeps);
        int workers = params.m_Config ? dmConfigFile::GetInt(params.m_Config, "box3d.worker_count", 0) : 0;
        if (workers < 0 || workers >= B3_MAX_WORKERS)
        {
            dmLogWarning("box3d.worker_count must be in the range 0 - %d and has been clamped.", B3_MAX_WORKERS - 1);
            workers = dmMath::Clamp(workers, 0, B3_MAX_WORKERS - 1);
        }

#if defined(DM_USE_SINGLE_THREAD) || (!defined(_WIN32) && !defined(__linux__) && !defined(__APPLE__) && !defined(__EMSCRIPTEN_PTHREADS__))
        if (workers > 0)
        {
            dmLogWarning("Box3D worker threads are unavailable on this platform. Using the calling thread.");
            workers = 0;
        }

#endif
        context->m_WorkerCount = 1 + workers;
        context->m_Worlds.SetCapacity(params.m_WorldCount);
        if (dmMessage::NewSocket(PHYSICS_SOCKET_NAME, &context->m_Socket) != dmMessage::RESULT_OK)
        {
            delete context;
            return 0;
        }

        return context;
    }

    void DeleteContext3D(HContext3D context)
    {
        while (!context->m_Worlds.Empty())
        {
            DeleteWorld3D(context, context->m_Worlds.Back());
        }

        dmMessage::DeleteSocket(context->m_Socket);
        delete context;
    }

    dmMessage::HSocket GetSocket3D(HContext3D context)
    {
        return context->m_Socket;
    }

    HWorld3D NewWorld3D(HContext3D context, const NewWorldParams& params)
    {
        if (context->m_Worlds.Full() || b3GetWorldCount() >= B3_MAX_WORLDS)
        {
            return 0;
        }

        HWorld3D world = new World3D();
        world->m_Context = context;
        world->m_Params = params;
        world->m_Bodies.SetCapacity(params.m_MaxCollisionObjectsCount);
        world->m_Rays.SetCapacity(context->m_Params.m_RayCastLimit3D);
        b3WorldDef def = b3DefaultWorldDef();
        def.gravity = Vec(context->m_Params.m_Gravity * context->m_Params.m_Scale);
        def.restitutionThreshold = context->m_Params.m_VelocityThreshold * context->m_Params.m_Scale;
        def.userData = world;
        def.workerCount = context->m_WorkerCount;
        world->m_Id = b3CreateWorld(&def);
        if (B3_IS_NULL(world->m_Id))
        {
            delete world;
            return 0;
        }

        dmBox3DCreateContactWork(world->m_Id);
        context->m_Worlds.Push(world);
        return world;
    }

    void DeleteWorld3D(HContext3D context, HWorld3D world)
    {
        while (!world->m_Bodies.Empty())
        {
            DeleteCollisionObject3D(world, world->m_Bodies.Back());
        }

        for (uint32_t i = 0; i < world->m_Rays.Size(); ++i)
        {
            free(world->m_Rays[i].m_UserData);
        }

        dmBox3DDestroyContactWork(world->m_Id);
        b3DestroyWorld(world->m_Id);
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
        {
            if (context->m_Worlds[i] == world)
            {
                context->m_Worlds.EraseSwap(i);
                break;
            }
        }

        delete world;
    }

    HContext3D GetContext3D(HWorld3D world)
    {
        return world->m_Context;
    }

    void* GetWorldContext3D(HWorld3D world)
    {
        return &world->m_Id;
    }

    void* GetCollisionObjectContext3D(HCollisionObject3D handle)
    {
        return &((Body*)handle)->m_Id;
    }

    static Geometry* NewGeometry(GeometryType type)
    {
        Geometry* shape = new Geometry();
        shape->m_Type = type;
        shape->m_Refs = 1;
        return shape;
    }

    HCollisionShape3D NewBoxShape3D(HContext3D context, const Vector3& half)
    {
        Geometry* geometry = NewGeometry(BOX);
        geometry->m_Size = Vec(half);
        b3BoxHull hull = b3MakeBoxHull(half.getX(), half.getY(), half.getZ());
        geometry->m_Hull = b3CloneHull(&hull.base);
        return geometry;
    }

    HCollisionShape3D NewSphereShape3D(HContext3D context, float radius)
    {
        Geometry* geometry = NewGeometry(SPHERE);
        geometry->m_Size.x = radius;
        return geometry;
    }

    HCollisionShape3D NewCapsuleShape3D(HContext3D context, float radius, float height)
    {
        Geometry* geometry = NewGeometry(CAPSULE);
        geometry->m_Size.x = radius;
        geometry->m_Size.y = 0.5f * height;
        return geometry;
    }

    HCollisionShape3D NewConvexHullShape3D(HContext3D context, const float* vertices, uint32_t count)
    {
        if (count < 4)
        {
            return 0;
        }

        b3HullData* hull = b3CreateHull((const b3Vec3*)vertices, count, count);
        if (!hull)
        {
            return 0;
        }

        Geometry* geometry = NewGeometry(HULL);
        geometry->m_Hull = hull;
        return geometry;
    }

    HCollisionShape3D NewTriangleMeshShape3D(HContext3D context, const float* vertices, uint32_t count, const uint32_t* indices, uint32_t index_count, CollisionObjectType type)
    {
        if (type != COLLISION_OBJECT_TYPE_STATIC || index_count < 3 || count < 3)
        {
            dmLogError("Box3D mesh shapes require a static body.");
            return 0;
        }

        b3MeshDef def = {};
        def.vertices = (b3Vec3*)vertices;
        def.stride = sizeof(b3Vec3);
        def.indices = (int32_t*)indices;
        def.vertexCount = count;
        def.triangleCount = index_count / 3;
        b3MeshData* mesh = b3CreateMesh(&def, 0, 0);
        if (!mesh)
        {
            return 0;
        }

        Geometry* geometry = NewGeometry(MESH);
        geometry->m_Mesh = mesh;
        return geometry;
    }

    void DeleteCollisionShape3D(HCollisionShape3D handle)
    {
        Geometry* geometry = (Geometry*)handle;
        if (--geometry->m_Refs)
        {
            return;
        }

        if (geometry->m_Hull)
        {
            b3DestroyHull(geometry->m_Hull);
        }

        if (geometry->m_Mesh)
        {
            b3DestroyMesh(geometry->m_Mesh);
        }

        delete geometry;
    }

    static b3ShapeId CreateShape(Body* body, const Attachment& attachment)
    {
        const CollisionObjectData& data = body->m_Data;
        b3ShapeDef def = b3DefaultShapeDef();
        def.userData = body;
        def.baseMaterial.friction = data.m_Friction;
        def.baseMaterial.restitution = data.m_Restitution;
        def.filter.categoryBits = data.m_Group;
        def.filter.maskBits = data.m_Mask;
        def.isSensor = data.m_Type == COLLISION_OBJECT_TYPE_TRIGGER;
        // Box3D requires both shapes to subscribe. Defold delivers the event if either component subscribes.
        def.enableSensorEvents = true;
        def.enableContactEvents = data.m_EventCollision;
        def.enableHitEvents = data.m_EventContact;
        def.updateBodyMass = false;
        const Geometry* geometry = attachment.m_Geometry;
        b3Vec3 scale = b3MulSV(body->m_World->m_Context->m_Params.m_Scale, body->m_Scale);
        b3Transform t = attachment.m_Local;
        t.p = b3Mul(t.p, scale);
        switch (geometry->m_Type)
        {
            case SPHERE:
            {
                b3Sphere sphere = { t.p, geometry->m_Size.x * scale.x };
                return b3CreateSphereShape(body->m_Id, &def, &sphere);
            }
            case CAPSULE:
            {
                b3Vec3 offset = { 0, geometry->m_Size.y * scale.y, 0 };
                offset = b3RotateVector(t.q, offset);
                b3Capsule capsule = { b3Sub(t.p, offset), b3Add(t.p, offset), geometry->m_Size.x * scale.x };
                return b3CreateCapsuleShape(body->m_Id, &def, &capsule);
            }
            case BOX:
            case HULL:
            {
                return b3CreateTransformedHullShape(body->m_Id, &def, geometry->m_Hull, t, scale);
            }
            case MESH:
            {
                if (b3LengthSquared(t.p) > 0 || t.q.s != 1.0f)
                {
                    dmLogError("Box3D meshes require an identity local pose.");
                    return b3_nullShapeId;
                }

                return b3CreateMeshShape(body->m_Id, &def, geometry->m_Mesh, scale);
            }
        }

        return b3_nullShapeId;
    }

    static void UpdateMass(Body* body)
    {
        b3Body_ApplyMassFromShapes(body->m_Id);
        if (body->m_Data.m_Type != COLLISION_OBJECT_TYPE_DYNAMIC)
        {
            return;
        }

        float mass = b3Body_GetMass(body->m_Id);
        if (mass <= 0 || mass == body->m_Data.m_Mass)
        {
            return;
        }

        float factor = body->m_Data.m_Mass / mass;
        for (uint32_t i = 0; i < body->m_Shapes.Size(); ++i)
        {
            b3ShapeId shape = body->m_Shapes[i].m_Id;
            b3Shape_SetDensity(shape, b3Shape_GetDensity(shape) * factor, false);
        }

        b3Body_ApplyMassFromShapes(body->m_Id);
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& data, HCollisionShape3D* shapes, Vector3* translations, Quat* rotations, uint32_t count)
    {
        if (!count || (data.m_Type == COLLISION_OBJECT_TYPE_DYNAMIC) != (data.m_Mass > 0))
        {
            return 0;
        }

        Body* body = new Body();
        body->m_Data = data;
        body->m_Force = Vector3(0.0f);
        body->m_AwakeOnEnable = true;
        body->m_World = world;
        dmTransform::Transform transform;
        transform.SetIdentity();
        if (world->m_Params.m_GetWorldTransformCallback && data.m_UserData)
        {
            world->m_Params.m_GetWorldTransformCallback(data.m_UserData, transform);
        }

        body->m_Scale = Vec(transform.GetScale());
        b3BodyDef def = b3DefaultBodyDef();
        def.type = data.m_Type == COLLISION_OBJECT_TYPE_DYNAMIC ? b3_dynamicBody : data.m_Type == COLLISION_OBJECT_TYPE_STATIC ? b3_staticBody : b3_kinematicBody;
        def.userData = body;
        def.position = Vec(transform.GetTranslation() * world->m_Context->m_Params.m_Scale);
        def.rotation = Rot(transform.GetRotation());
        def.linearDamping = data.m_LinearDamping;
        def.angularDamping = data.m_AngularDamping;
        def.motionLocks.angularX = def.motionLocks.angularY = def.motionLocks.angularZ = data.m_LockedRotation;
        def.isBullet = data.m_Bullet;
        def.isEnabled = data.m_Enabled;
        body->m_Id = b3CreateBody(world->m_Id, &def);
        body->m_Shapes.SetCapacity(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            Attachment attachment = {};
            attachment.m_Geometry = (Geometry*)shapes[i];
            attachment.m_Local.p = translations ? Vec(translations[i]) : b3Vec3_zero;
            attachment.m_Local.q = rotations ? Rot(rotations[i]) : b3Quat_identity;
            attachment.m_Id = CreateShape(body, attachment);
            if (B3_IS_NULL(attachment.m_Id))
            {
                b3DestroyBody(body->m_Id);
                for (uint32_t j = 0; j < body->m_Shapes.Size(); ++j)
                {
                    DeleteCollisionShape3D(body->m_Shapes[j].m_Geometry);
                }

                delete body;
                return 0;
            }

            ++attachment.m_Geometry->m_Refs;
            body->m_Shapes.Push(attachment);
        }

        UpdateMass(body);
        body->m_Index = world->m_Bodies.Size();
        if (world->m_Bodies.Full())
        {
            world->m_Bodies.OffsetCapacity(16);
        }

        world->m_Bodies.Push(body);
        return body;
    }

    HCollisionObject3D NewCollisionObject3D(HWorld3D world, const CollisionObjectData& d, HCollisionShape3D* s, uint32_t n)
    {
        return NewCollisionObject3D(world, d, s, 0, 0, n);
    }

    void DeleteCollisionObject3D(HWorld3D world, HCollisionObject3D handle)
    {
        Body* body = (Body*)handle;
        b3DestroyBody(body->m_Id);
        for (uint32_t i = 0; i < body->m_Shapes.Size(); ++i)
        {
            DeleteCollisionShape3D(body->m_Shapes[i].m_Geometry);
        }

        world->m_Bodies.Back()->m_Index = body->m_Index;
        world->m_Bodies.EraseSwap(body->m_Index);
        delete body;
    }

    uint32_t GetCollisionShapes3D(HCollisionObject3D handle, HCollisionShape3D* out, uint32_t capacity)
    {
        Body* body = (Body*)handle;
        uint32_t n = dmMath::Min(body->m_Shapes.Size(), capacity);
        for (uint32_t i = 0; i < n; ++i)
        {
            out[i] = body->m_Shapes[i].m_Geometry;
        }

        return n;
    }

    HCollisionShape3D GetCollisionShape3D(HCollisionObject3D handle, uint32_t i)
    {
        Body* body = (Body*)handle;
        return i < body->m_Shapes.Size() ? body->m_Shapes[i].m_Geometry : 0;
    }

    void SetCollisionObjectUserData3D(HCollisionObject3D handle, void* p)
    {
        ((Body*)handle)->m_Data.m_UserData = p;
    }

    void* GetCollisionObjectUserData3D(HCollisionObject3D handle)
    {
        return ((Body*)handle)->m_Data.m_UserData;
    }

    void ApplyForce3D(HContext3D context, HCollisionObject3D handle, const Vector3& force, const Point3& p)
    {
        Body* body = (Body*)handle;
        b3Body_ApplyForce(body->m_Id, Vec(force * context->m_Params.m_Scale), Vec(Vector3(p) * context->m_Params.m_Scale), true);
        if (body->m_ForceStep != body->m_World->m_Step)
        {
            body->m_Force = Vector3(0.0f);
            body->m_ForceStep = body->m_World->m_Step;
        }

        body->m_Force += force;
    }

    Vector3 GetTotalForce3D(HContext3D context, HCollisionObject3D handle)
    {
        Body* body = (Body*)handle;
        return body->m_ForceStep == body->m_World->m_Step ? body->m_Force : Vector3(0.0f);
    }

    Point3 GetWorldPosition3D(HContext3D context, HCollisionObject3D handle)
    {
        return Point3(Vector(b3Body_GetPosition(((Body*)handle)->m_Id)) / context->m_Params.m_Scale);
    }

    Quat GetWorldRotation3D(HContext3D context, HCollisionObject3D handle)
    {
        return Rotation(b3Body_GetRotation(((Body*)handle)->m_Id));
    }

    Vector3 GetLinearVelocity3D(HContext3D context, HCollisionObject3D object)
    {
        Body* body = (Body*)object;
        return Vector(b3Body_IsEnabled(body->m_Id) ? b3Body_GetLinearVelocity(body->m_Id) : body->m_DisabledLinearVelocity) / context->m_Params.m_Scale;
    }

    Vector3 GetAngularVelocity3D(HContext3D context, HCollisionObject3D object)
    {
        Body* body = (Body*)object;
        return Vector(b3Body_IsEnabled(body->m_Id) ? b3Body_GetAngularVelocity(body->m_Id) : body->m_DisabledAngularVelocity);
    }

    void SetLinearVelocity3D(HContext3D context, HCollisionObject3D object, const Vector3& velocity)
    {
        Body* body = (Body*)object;
        b3Vec3 native = Vec(velocity * context->m_Params.m_Scale);
        if (b3Body_IsEnabled(body->m_Id))
        {
            b3Body_SetLinearVelocity(body->m_Id, native);
        }
        else
        {
            body->m_DisabledLinearVelocity = native;
        }
    }

    void SetAngularVelocity3D(HContext3D context, HCollisionObject3D object, const Vector3& velocity)
    {
        Body* body = (Body*)object;
        if (b3Body_IsEnabled(body->m_Id))
        {
            b3Body_SetAngularVelocity(body->m_Id, Vec(velocity));
        }
        else
        {
            body->m_DisabledAngularVelocity = Vec(velocity);
        }
    }

    bool IsEnabled3D(HCollisionObject3D object)
    {
        return b3Body_IsEnabled(((Body*)object)->m_Id);
    }

    void SetEnabled3D(HWorld3D world, HCollisionObject3D object, bool enabled)
    {
        Body* body = (Body*)object;
        b3BodyId id = body->m_Id;
        if (enabled == b3Body_IsEnabled(id))
        {
            return;
        }

        // Factory scripts may set velocities before the component joins the update list.
        // Box3D only stores velocity for enabled bodies.
        if (enabled)
        {
            b3Body_Enable(id);
            b3Body_SetLinearVelocity(id, body->m_DisabledLinearVelocity);
            b3Body_SetAngularVelocity(id, body->m_DisabledAngularVelocity);
            if (!body->m_AwakeOnEnable && body->m_Data.m_Type == COLLISION_OBJECT_TYPE_DYNAMIC)
            {
                b3Body_SetAwake(id, false);
            }
        }
        else
        {
            body->m_DisabledLinearVelocity = b3Body_GetLinearVelocity(id);
            body->m_DisabledAngularVelocity = b3Body_GetAngularVelocity(id);
            body->m_AwakeOnEnable = b3Body_IsAwake(id);
            b3Body_Disable(id);
        }
    }

    bool IsSleeping3D(HCollisionObject3D object)
    {
        return !b3Body_IsAwake(((Body*)object)->m_Id);
    }

    void SetBox3DAwake(b3BodyId id, bool awake)
    {
        Body* body = (Body*)b3Body_GetUserData(id);
        if (b3Body_IsEnabled(id))
        {
            b3Body_SetAwake(id, awake);
        }
        else
        {
            body->m_AwakeOnEnable = awake;
        }
    }

    void Wakeup3D(HCollisionObject3D object)
    {
        SetBox3DAwake(((Body*)object)->m_Id, true);
    }

    void GetBox3DBodyMotion(b3BodyId id, Box3DBodyMotion* motion)
    {
        Body* body = (Body*)b3Body_GetUserData(id);
        HContext3D context = body->m_World->m_Context;
        b3WorldTransform t = b3Body_GetTransform(id);
        motion->m_Position = Vector(t.p) * (1.0f / context->m_Params.m_Scale);
        motion->m_Rotation = Rotation(t.q);
        motion->m_LinearVelocity = GetLinearVelocity3D(context, body);
        motion->m_AngularVelocity = GetAngularVelocity3D(context, body);
    }

    void SetBox3DBodyMotion(b3BodyId id, const Box3DBodyMotion& motion, bool awake)
    {
        Body* body = (Body*)b3Body_GetUserData(id);
        HContext3D context = body->m_World->m_Context;
        SetBox3DTransform(id, Vec(motion.m_Position * context->m_Params.m_Scale), Rot(motion.m_Rotation));
        SetLinearVelocity3D(context, body, motion.m_LinearVelocity);
        SetAngularVelocity3D(context, body, motion.m_AngularVelocity);
        SetBox3DAwake(id, awake);
    }

    void SetLockedRotation3D(HCollisionObject3D handle, bool lock)
    {
        b3BodyId id = ((Body*)handle)->m_Id;
        b3MotionLocks locks = b3Body_GetMotionLocks(id);
        locks.angularX = locks.angularY = locks.angularZ = lock;
        b3Body_SetMotionLocks(id, locks);
    }

    float GetLinearDamping3D(HCollisionObject3D handle)
    {
        return b3Body_GetLinearDamping(((Body*)handle)->m_Id);
    }

    void SetLinearDamping3D(HCollisionObject3D handle, float v)
    {
        b3Body_SetLinearDamping(((Body*)handle)->m_Id, v);
    }

    float GetAngularDamping3D(HCollisionObject3D handle)
    {
        return b3Body_GetAngularDamping(((Body*)handle)->m_Id);
    }

    void SetAngularDamping3D(HCollisionObject3D handle, float v)
    {
        b3Body_SetAngularDamping(((Body*)handle)->m_Id, v);
    }

    float GetMass3D(HCollisionObject3D handle)
    {
        return b3Body_GetMass(((Body*)handle)->m_Id);
    }

    uint16_t GetGroup3D(HCollisionObject3D handle)
    {
        return ((Body*)handle)->m_Data.m_Group;
    }

    static void ApplyFilter(Body* body)
    {
        for (uint32_t i = 0; i < body->m_Shapes.Size(); ++i)
        {
            b3ShapeId id = body->m_Shapes[i].m_Id;
            b3Filter f = b3Shape_GetFilter(id);
            f.categoryBits = body->m_Data.m_Group;
            f.maskBits = body->m_Data.m_Mask;
            b3Shape_SetFilter(id, f, true);
        }
    }

    void SetGroup3D(HWorld3D world, HCollisionObject3D handle, uint16_t group)
    {
        Body* body = (Body*)handle;
        body->m_Data.m_Group = group;
        ApplyFilter(body);
    }

    bool GetMaskBit3D(HCollisionObject3D handle, uint16_t bit)
    {
        return (((Body*)handle)->m_Data.m_Mask & bit) != 0;
    }

    void SetMaskBit3D(HWorld3D world, HCollisionObject3D handle, uint16_t bit, bool value)
    {
        Body* body = (Body*)handle;
        if (value)
        {
            body->m_Data.m_Mask |= bit;
        }
        else
        {
            body->m_Data.m_Mask &= ~bit;
        }

        ApplyFilter(body);
    }

    void SetGravity3D(HWorld3D world, const Vector3& v)
    {
        b3World_SetGravity(world->m_Id, Vec(v * world->m_Context->m_Params.m_Scale));
    }

    Vector3 GetGravity3D(HWorld3D world)
    {
        return Vector(b3World_GetGravity(world->m_Id)) / world->m_Context->m_Params.m_Scale;
    }

    static bool ReplaceAttachment(Body* body, uint32_t index, Geometry* replacement, bool owned)
    {
        if (index >= body->m_Shapes.Size())
        {
            return false;
        }

        Attachment& attachment = body->m_Shapes[index];
        Attachment next = attachment;
        next.m_Geometry = replacement;
        next.m_Owned = owned;
        next.m_Id = CreateShape(body, next);
        if (B3_IS_NULL(next.m_Id))
        {
            return false;
        }

        if (!owned)
        {
            ++replacement->m_Refs;
        }

        b3DestroyShape(attachment.m_Id, false);
        DeleteCollisionShape3D(attachment.m_Geometry);
        attachment = next;
        UpdateMass(body);
        return true;
    }

    bool ReplaceCollisionShapeAtIndex3D(HCollisionObject3D handle, uint32_t index, HCollisionShape3D geometry)
    {
        return ReplaceAttachment((Body*)handle, index, (Geometry*)geometry, true);
    }

    void ReplaceShape3D(HCollisionObject3D handle, HCollisionShape3D old, HCollisionShape3D replacement)
    {
        Body* body = (Body*)handle;
        for (uint32_t i = 0; i < body->m_Shapes.Size(); ++i)
        {
            if (body->m_Shapes[i].m_Geometry == old)
            {
                ReplaceAttachment(body, i, (Geometry*)replacement, false);
            }
        }
    }

    void ReplaceShape3D(HContext3D context, HCollisionShape3D old, HCollisionShape3D replacement)
    {
        for (uint32_t i = 0; i < context->m_Worlds.Size(); ++i)
        {
            for (uint32_t j = 0; j < context->m_Worlds[i]->m_Bodies.Size(); ++j)
            {
                ReplaceShape3D(context->m_Worlds[i]->m_Bodies[j], old, replacement);
            }
        }
    }

    bool MakeCollisionShapeOwned3D(HCollisionObject3D handle, uint32_t index, HCollisionShape3D* out)
    {
        Body* body = (Body*)handle;
        if (index >= body->m_Shapes.Size())
        {
            return false;
        }

        Attachment& attachment = body->m_Shapes[index];
        if (!attachment.m_Owned)
        {
            Geometry* old = attachment.m_Geometry;
            if (old->m_Mesh)
            {
                return false;
            }

            Geometry* geometry = NewGeometry(old->m_Type);
            geometry->m_Size = old->m_Size;
            if (old->m_Hull)
            {
                geometry->m_Hull = b3CloneHull(old->m_Hull);
            }

            attachment.m_Geometry = geometry;
            attachment.m_Owned = true;
            DeleteCollisionShape3D(old);
        }

        *out = attachment.m_Geometry;
        return true;
    }

    void RefreshCollisionShape3D(HWorld3D world, HCollisionObject3D handle)
    {
        Body* body = (Body*)handle;
        for (uint32_t i = 0; i < body->m_Shapes.Size(); ++i)
        {
            Attachment& attachment = body->m_Shapes[i];
            b3ShapeId id = CreateShape(body, attachment);
            if (B3_IS_NON_NULL(id))
            {
                b3DestroyShape(attachment.m_Id, false);
                attachment.m_Id = id;
            }
        }

        UpdateMass(body);
    }

    void GetCollisionShapeRadius3D(HWorld3D world, HCollisionShape3D handle, float* r)
    {
        *r = ((Geometry*)handle)->m_Size.x;
    }

    void GetCollisionShapeHalfBoxExtents3D(HWorld3D world, HCollisionShape3D handle, float* v)
    {
        const b3Vec3& s = ((Geometry*)handle)->m_Size;
        v[0] = s.x;
        v[1] = s.y;
        v[2] = s.z;
    }

    void GetCollisionShapeCapsuleRadiusHeight3D(HWorld3D world, HCollisionShape3D handle, float* r, float* half)
    {
        *r = ((Geometry*)handle)->m_Size.x;
        *half = ((Geometry*)handle)->m_Size.y;
    }

    void SetCollisionShapeRadius3D(HWorld3D world, HCollisionShape3D handle, float r)
    {
        ((Geometry*)handle)->m_Size.x = r;
    }

    struct RayQuery
    {
        const RayCastRequest*     m_Request;
        dmArray<RayCastResponse>* m_Results;
        float                     m_Scale;
    };
    static float RayHit(b3ShapeId id, b3Pos point, b3Vec3 normal, float fraction, uint64_t material, int triangle, int child, void* ctx)
    {
        RayQuery* q = (RayQuery*)ctx;
        Body* body = Owner(id);
        if (body->m_Data.m_Type == COLLISION_OBJECT_TYPE_TRIGGER || body->m_Data.m_UserData == q->m_Request->m_IgnoredUserData)
        {
            return -1.0f;
        }

        RayCastResponse r;
        r.m_Hit = true;
        r.m_Position = Point3(Vector(point) / q->m_Scale);
        r.m_Normal = Vector(normal);
        r.m_Fraction = fraction;
        r.m_CollisionObjectUserData = body->m_Data.m_UserData;
        r.m_CollisionObjectGroup = body->m_Data.m_Group;
        dmArray<RayCastResponse>& results = *q->m_Results;
        if (!q->m_Request->m_ReturnAllResults && !results.Empty())
        {
            results[0] = r;
        }
        else
        {
            if (results.Full())
            {
                results.OffsetCapacity(8);
            }

            results.Push(r);
        }

        return q->m_Request->m_ReturnAllResults ? 1.0f : fraction;
    }

    static int CompareRayHits(const void* lhs, const void* rhs)
    {
        float a = ((const RayCastResponse*)lhs)->m_Fraction;
        float b = ((const RayCastResponse*)rhs)->m_Fraction;
        return (a > b) - (a < b);
    }

    void RayCast3D(HWorld3D world, const RayCastRequest& request, dmArray<RayCastResponse>& results)
    {
        results.SetSize(0);
        RayQuery q = { &request, &results, world->m_Context->m_Params.m_Scale };
        b3QueryFilter f = b3DefaultQueryFilter();
        f.maskBits = request.m_Mask;
        b3Vec3 dir = Vec((request.m_To - request.m_From) * q.m_Scale);
        if (b3LengthSquared(dir) > 0)
        {
            b3World_CastRay(world->m_Id, Vec(Vector3(request.m_From) * q.m_Scale), dir, f, RayHit, &q);
        }

        if (results.Size() > 1)
        {
            qsort(results.Begin(), results.Size(), sizeof(RayCastResponse), CompareRayHits);
        }
    }

    bool RequestRayCast3D(HWorld3D world, const RayCastRequest& request)
    {
        if (world->m_Rays.Full())
        {
            return false;
        }

        world->m_Rays.Push(request);
        return true;
    }

    static void ReportEvents(HWorld3D world, const StepWorldContext& step)
    {
        const float scale = world->m_Context->m_Params.m_Scale;
        b3ContactEvents events = b3World_GetContactEvents(world->m_Id);
        for (int i = 0; step.m_CollisionCallback && i < events.beginCount; ++i)
        {
            const b3ContactBeginTouchEvent& e = events.beginEvents[i];
            Body* first = Owner(e.shapeIdA);
            Body* second = Owner(e.shapeIdB);
            if (!step.m_CollisionCallback(first->m_Data.m_UserData, first->m_Data.m_Group, second->m_Data.m_UserData, second->m_Data.m_Group, step.m_CollisionUserData))
            {
                break;
            }
        }

        for (int i = 0; step.m_ContactPointCallback && i < events.hitCount; ++i)
        {
            const b3ContactHitEvent& e = events.hitEvents[i];
            if (!b3Contact_IsValid(e.contactId))
            {
                continue;
            }

            Body* first = Owner(e.shapeIdA);
            Body* second = Owner(e.shapeIdB);
            b3ContactData contact = b3Contact_GetData(e.contactId);
            float impulse = 0;
            float separation = 0;
            for (int m = 0; m < contact.manifoldCount; ++m)
            {
                const b3Manifold& manifold = contact.manifolds[m];
                for (int p = 0; p < manifold.pointCount; ++p)
                {
                    impulse += manifold.points[p].totalNormalImpulse;
                    separation = dmMath::Min(separation, manifold.points[p].separation);
                }
            }

            ContactPoint point;
            point.m_AppliedImpulse = impulse / scale;
            if (point.m_AppliedImpulse < world->m_Context->m_Params.m_ContactImpulseLimit)
            {
                continue;
            }

            point.m_PositionA = point.m_PositionB = Point3(Vector(e.point) / scale);
            point.m_Normal = Vector(e.normal);
            point.m_Distance = -separation / scale;
            point.m_RelativeVelocity = Vector(b3Sub(b3Body_GetWorldPointVelocity(first->m_Id, e.point), b3Body_GetWorldPointVelocity(second->m_Id, e.point))) / scale;
            // The hit event records the approach speed before the solver removed it.
            point.m_RelativeVelocity += point.m_Normal * (e.approachSpeed / scale - dot(point.m_RelativeVelocity, point.m_Normal));
            point.m_UserDataA = first->m_Data.m_UserData;
            point.m_UserDataB = second->m_Data.m_UserData;
            point.m_GroupA = first->m_Data.m_Group;
            point.m_GroupB = second->m_Data.m_Group;
            point.m_MassA = first->m_Data.m_Mass;
            point.m_MassB = second->m_Data.m_Mass;
            if (!step.m_ContactPointCallback(point, step.m_ContactPointUserData))
            {
                break;
            }
        }

        b3SensorEvents sensors = b3World_GetSensorEvents(world->m_Id);
        for (int i = 0; step.m_TriggerEnteredCallback && i < sensors.beginCount; ++i)
        {
            const b3SensorBeginTouchEvent& e = sensors.beginEvents[i];
            Body* first = Owner(e.sensorShapeId);
            Body* second = Owner(e.visitorShapeId);
            if (!first->m_Data.m_EventTrigger && !second->m_Data.m_EventTrigger)
            {
                continue;
            }

            TriggerEnter event = { first->m_Data.m_UserData, second->m_Data.m_UserData, first->m_Data.m_Group, second->m_Data.m_Group };
            step.m_TriggerEnteredCallback(event, step.m_TriggerEnteredUserData);
        }

        for (int i = 0; step.m_TriggerExitedCallback && i < sensors.endCount; ++i)
        {
            const b3SensorEndTouchEvent& e = sensors.endEvents[i];
            // Box3D also queues exits on destruction. Their generation-tagged IDs may have expired.
            if (!b3Shape_IsValid(e.sensorShapeId) || !b3Shape_IsValid(e.visitorShapeId))
            {
                continue;
            }

            Body* first = Owner(e.sensorShapeId);
            Body* second = Owner(e.visitorShapeId);
            if (!first->m_Data.m_EventTrigger && !second->m_Data.m_EventTrigger)
            {
                continue;
            }

            TriggerExit event = { first->m_Data.m_UserData, second->m_Data.m_UserData, first->m_Data.m_Group, second->m_Data.m_Group };
            step.m_TriggerExitedCallback(event, step.m_TriggerExitedUserData);
        }
    }

    static void SyncInputs(HWorld3D world, float dt)
    {
        if (!world->m_Params.m_GetWorldTransformCallback)
        {
            return;
        }

        for (uint32_t i = 0; i < world->m_Bodies.Size(); ++i)
        {
            Body* body = world->m_Bodies[i];
            bool dynamic = body->m_Data.m_Type == COLLISION_OBJECT_TYPE_DYNAMIC;
            if (body->m_Data.m_Type == COLLISION_OBJECT_TYPE_STATIC || (dynamic && !world->m_Context->m_Params.m_AllowDynamicTransforms) || !b3Body_IsEnabled(body->m_Id))
            {
                continue;
            }

            dmTransform::Transform t;
            world->m_Params.m_GetWorldTransformCallback(body->m_Data.m_UserData, t);
            b3WorldTransform old = b3Body_GetTransform(body->m_Id);
            b3Vec3 p = Vec(t.GetTranslation() * world->m_Context->m_Params.m_Scale);
            b3Quat q = Rot(t.GetRotation());
            if (body->m_Data.m_Type == COLLISION_OBJECT_TYPE_KINEMATIC)
            {
                b3WorldTransform target = { p, q };
                b3Body_SetTargetTransform(body->m_Id, target, dt, true);
            }
            else if (b3LengthSquared(b3Sub(old.p, p)) > 1e-12f || fabsf(b3DotQuat(old.q, q)) < 0.9999999f)
            {
                b3Body_SetTransform(body->m_Id, p, q);
                b3Body_SetAwake(body->m_Id, true);
            }

            b3Vec3 scale = Vec(t.GetScale());
            if (b3LengthSquared(b3Sub(scale, body->m_Scale)) > 1e-12f)
            {
                body->m_Scale = scale;
                RefreshCollisionShape3D(world, body);
            }
        }
    }

    static void DrawBounds(b3AABB box, b3HexColor color, void* user_data)
    {
        HContext3D context = (HContext3D)user_data;
        const Vector3 lo = Vector(box.lowerBound) / context->m_Params.m_Scale;
        const Vector3 hi = Vector(box.upperBound) / context->m_Params.m_Scale;
        Point3 corners[8], edges[24];
        for (int i = 0; i < 8; ++i)
        {
            corners[i] = Point3(i & 1 ? hi.getX() : lo.getX(), i & 2 ? hi.getY() : lo.getY(), i & 4 ? hi.getZ() : lo.getZ());
        }

        int n = 0;
        for (int i = 0; i < 8; ++i)
        {
            for (int bit = 1; bit <= 4; bit *= 2)
            {
                if (!(i & bit))
                {
                    edges[n++] = corners[i];
                    edges[n++] = corners[i | bit];
                }
            }
        }

        Vector4 rgba(((color >> 16) & 255) / 255.0f, ((color >> 8) & 255) / 255.0f, (color & 255) / 255.0f, context->m_Debug.m_Alpha);
        context->m_Debug.m_DrawLines(edges, n, rgba, context->m_Debug.m_UserData);
    }

    bool StepWorld3D(HWorld3D world, const StepWorldContext& step)
    {
        bool stepped = !world->m_Paused && step.m_DT > 0;
        if (stepped)
        {
            SyncInputs(world, step.m_DT);
            {
                DM_PROFILE("Box3D.Step");
                b3World_Step(world->m_Id, step.m_DT, world->m_Context->m_Substeps);
            }
            dmBox3DAdvanceContactPreparation(world->m_Id);
            b3BodyEvents moved = b3World_GetBodyEvents(world->m_Id);
            for (int i = 0; world->m_Params.m_SetWorldTransformCallback && i < moved.moveCount; ++i)
            {
                const b3BodyMoveEvent& e = moved.moveEvents[i];
                Body* body = (Body*)e.userData;
                if (body->m_Data.m_Type == COLLISION_OBJECT_TYPE_DYNAMIC)
                {
                    world->m_Params.m_SetWorldTransformCallback(body->m_Data.m_UserData, Point3(Vector(e.transform.p) / world->m_Context->m_Params.m_Scale), Rotation(e.transform.q));
                }
            }

            ++world->m_Step;
            if (!world->m_SkipComponentEvents)
            {
                DM_PROFILE("Box3D.Events");
                ReportEvents(world, step);
            }
        }

        for (uint32_t i = 0; i < world->m_Rays.Size(); ++i)
        {
            RayCastRequest& ray = world->m_Rays[i];
            dmArray<RayCastResponse> results;
            RayCast3D(world, ray, results);
            RayCastResponse miss;
            if (step.m_RayCastCallback)
            {
                step.m_RayCastCallback(results.Empty() ? miss : results[0], ray, step.m_RayCastUserData);
            }
        }

        world->m_Rays.SetSize(0);
        if (world->m_DrawDebug && world->m_Context->m_Debug.m_DrawLines)
        {
            b3DebugDraw draw = b3DefaultDebugDraw();
            draw.drawBounds = true;
            draw.drawShapes = false;
            draw.DrawBoundsFcn = DrawBounds;
            draw.context = world->m_Context;
            b3World_Draw(world->m_Id, &draw, UINT64_MAX);
        }

        return stepped;
    }

    void SetDrawDebug3D(HWorld3D world, bool enabled)
    {
        world->m_DrawDebug = enabled;
    }

    void SetDebugCallbacks3D(HContext3D context, const DebugCallbacks& callbacks)
    {
        context->m_Debug = callbacks;
    }

}
