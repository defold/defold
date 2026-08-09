// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <dlib/array.h>
#include <dmsdk/dlib/hashtable.h>
#include <gameobject/gameobject.h>
#include <script/script.h>

#include "components/comp_collision_object.h"
#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "gamesys_private.h"
#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

//////////////////////////////////////////////////////////////////////////////
// btTypedConstraint
namespace dmGameSystem
{
#define BULLET3D_TYPE_NAME_CONSTRAINT "bullet3d_constraint"

    enum Bullet3DConstraintKind
    {
        BULLET3D_CONSTRAINT_POINT_TO_POINT = 1,
        BULLET3D_CONSTRAINT_HINGE,
        BULLET3D_CONSTRAINT_CONE_TWIST,
        BULLET3D_CONSTRAINT_GENERIC_6DOF,
        BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING,
        BULLET3D_CONSTRAINT_SLIDER,
        BULLET3D_CONSTRAINT_UNIVERSAL,
        BULLET3D_CONSTRAINT_HINGE2,
    };

    struct Bullet3DLuaConstraint
    {
        uint64_t m_Id;
    };

    struct Bullet3DConstraintMeta
    {
        btTypedConstraint*       m_Constraint;
        btDiscreteDynamicsWorld* m_World;
        void*                    m_ComponentWorld;
        btRigidBody*             m_BodyA;
        btRigidBody*             m_BodyB;
        uint8_t                  m_Kind;
        uint8_t                  m_InWorld : 1;
        uint8_t                  m_CollideConnected : 1;
        uint8_t                  m_AngularOnly : 1;
    };

    struct Bullet3DBodyState
    {
        btDiscreteDynamicsWorld* m_World;
        bool                     m_Enabled;
    };

    struct Bullet3DCreateInput
    {
        btDiscreteDynamicsWorld* m_World;
        void*                    m_ComponentWorld;
        btRigidBody*             m_BodyA;
        btRigidBody*             m_BodyB;
        bool                     m_CollideConnected;
    };

    static uint32_t                              TYPE_HASH_CONSTRAINT = 0;
    static uint64_t                              g_NextBullet3DConstraintId = 0;
    static dmHashTable64<Bullet3DConstraintMeta> g_Bullet3DConstraints;
    static dmHashTable64<Bullet3DBodyState>      g_Bullet3DBodyStates;

    static uint64_t                              PtrToKey(const void* pointer)
    {
        return (uint64_t)(uintptr_t)pointer;
    }

    template <typename T>
    static void ArrayPush(dmArray<T>* values, const T& value)
    {
        if (values->Full())
        {
            values->SetCapacity(values->Capacity() ? values->Capacity() * 2 : 16);
        }
        values->Push(value);
    }

    static int AbsIndex(lua_State* L, int index)
    {
        return index < 0 ? lua_gettop(L) + index + 1 : index;
    }

    static const char* ConstraintKindName(uint8_t kind)
    {
        switch (kind)
        {
            case BULLET3D_CONSTRAINT_POINT_TO_POINT:
                return "point_to_point";
            case BULLET3D_CONSTRAINT_HINGE:
                return "hinge";
            case BULLET3D_CONSTRAINT_CONE_TWIST:
                return "cone_twist";
            case BULLET3D_CONSTRAINT_GENERIC_6DOF:
                return "generic_6dof";
            case BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING:
                return "generic_6dof_spring";
            case BULLET3D_CONSTRAINT_SLIDER:
                return "slider";
            case BULLET3D_CONSTRAINT_UNIVERSAL:
                return "universal";
            case BULLET3D_CONSTRAINT_HINGE2:
                return "hinge2";
            default:
                return "unknown";
        }
    }

    static bool Is6DofKind(uint8_t kind)
    {
        return kind == BULLET3D_CONSTRAINT_GENERIC_6DOF ||
        kind == BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING ||
        kind == BULLET3D_CONSTRAINT_UNIVERSAL ||
        kind == BULLET3D_CONSTRAINT_HINGE2;
    }

    static void EnsureConstraintCapacity()
    {
        if (g_Bullet3DConstraints.Full())
        {
            g_Bullet3DConstraints.OffsetCapacity(32);
        }
    }

    static void EnsureBodyStateCapacity()
    {
        if (g_Bullet3DBodyStates.Full())
        {
            g_Bullet3DBodyStates.OffsetCapacity(64);
        }
    }

    static Bullet3DLuaConstraint* CheckConstraintInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaConstraint*)dmScript::CheckUserType(L, index, TYPE_HASH_CONSTRAINT, "Expected user type " BULLET3D_TYPE_NAME_CONSTRAINT);
    }

    static Bullet3DLuaConstraint* ToConstraintInternal(lua_State* L, int index)
    {
        return (Bullet3DLuaConstraint*)dmScript::ToUserType(L, index, TYPE_HASH_CONSTRAINT);
    }

    static Bullet3DConstraintMeta* VerifyConstraintInternal(lua_State* L, Bullet3DLuaConstraint* lua_constraint, bool report_error)
    {
        Bullet3DConstraintMeta* meta = lua_constraint ? g_Bullet3DConstraints.Get(lua_constraint->m_Id) : 0;
        if (!meta && report_error)
        {
            luaL_error(L, "Invalid bullet3d constraint handle.");
        }
        return meta;
    }

    bool IsBullet3DConstraintValid(lua_State* L, int index)
    {
        return VerifyConstraintInternal(L, ToConstraintInternal(L, index), false) != 0;
    }

    static Bullet3DConstraintMeta* CheckConstraintMeta(lua_State* L, int index)
    {
        return VerifyConstraintInternal(L, CheckConstraintInternal(L, index), true);
    }

    static void CheckConstraintUnlocked(lua_State* L, const Bullet3DConstraintMeta* meta)
    {
        if (CompCollisionObjectIsBullet3DWorldLocked((dmGameObject::HComponentWorld)meta->m_ComponentWorld))
        {
            luaL_error(L, "The bullet3d world is locked while stepping.");
        }
    }

    static void CheckWorldUnlocked(lua_State* L, void* component_world)
    {
        if (CompCollisionObjectIsBullet3DWorldLocked((dmGameObject::HComponentWorld)component_world))
        {
            luaL_error(L, "The bullet3d world is locked while stepping.");
        }
    }

    static void ActivateConstraintBodies(Bullet3DConstraintMeta* meta)
    {
        if (meta->m_BodyA)
        {
            meta->m_BodyA->activate(true);
        }
        if (meta->m_BodyB)
        {
            meta->m_BodyB->activate(true);
        }
    }

    static bool IsBodyEnabled(btRigidBody* body)
    {
        if (!body)
        {
            return true;
        }
        Bullet3DBodyState* state = g_Bullet3DBodyStates.Get(PtrToKey(body));
        return state ? state->m_Enabled : body->getBroadphaseHandle() != 0;
    }

    static bool ShouldConstraintBeActive(const Bullet3DConstraintMeta* meta)
    {
        return IsBodyEnabled(meta->m_BodyA) && IsBodyEnabled(meta->m_BodyB);
    }

    static void RemoveConstraintFromWorld(Bullet3DConstraintMeta* meta)
    {
        if (meta->m_InWorld && meta->m_World && meta->m_Constraint)
        {
            meta->m_World->removeConstraint(meta->m_Constraint);
            meta->m_InWorld = 0;
        }
    }

    static void AddConstraintToWorld(Bullet3DConstraintMeta* meta)
    {
        if (!meta->m_InWorld && meta->m_World && meta->m_Constraint && ShouldConstraintBeActive(meta))
        {
            meta->m_World->addConstraint(meta->m_Constraint, meta->m_CollideConnected == 0);
            meta->m_InWorld = 1;
        }
    }

    static void DestroyConstraintId(uint64_t id)
    {
        Bullet3DConstraintMeta* meta = g_Bullet3DConstraints.Get(id);
        if (!meta)
        {
            return;
        }

        RemoveConstraintFromWorld(meta);
        delete meta->m_Constraint;
        g_Bullet3DConstraints.Erase(id);
    }

    static uint64_t RegisterConstraint(lua_State* L, const Bullet3DCreateInput& input, btTypedConstraint* constraint, uint8_t kind)
    {
        if (g_NextBullet3DConstraintId == UINT64_MAX)
        {
            delete constraint;
            luaL_error(L, "The bullet3d constraint identity space is exhausted.");
            return 0;
        }

        EnsureConstraintCapacity();
        uint64_t               id = ++g_NextBullet3DConstraintId;

        Bullet3DConstraintMeta meta = {};
        meta.m_Constraint = constraint;
        meta.m_World = input.m_World;
        meta.m_ComponentWorld = input.m_ComponentWorld;
        meta.m_BodyA = input.m_BodyA;
        meta.m_BodyB = input.m_BodyB;
        meta.m_Kind = kind;
        meta.m_CollideConnected = input.m_CollideConnected;

        g_Bullet3DConstraints.Put(id, meta);
        Bullet3DConstraintMeta* stored_meta = g_Bullet3DConstraints.Get(id);
        AddConstraintToWorld(stored_meta);
        ActivateConstraintBodies(stored_meta);
        return id;
    }

    static void PushConstraint(lua_State* L, uint64_t id)
    {
        Bullet3DLuaConstraint* lua_constraint = (Bullet3DLuaConstraint*)lua_newuserdata(L, sizeof(Bullet3DLuaConstraint));
        lua_constraint->m_Id = id;
        luaL_getmetatable(L, BULLET3D_TYPE_NAME_CONSTRAINT);
        lua_setmetatable(L, -2);
    }

    static void PushRigidBody(lua_State* L, btRigidBody* body)
    {
        if (!body || !body->getUserPointer())
        {
            lua_pushnil(L);
            return;
        }

        dmGameObject::HInstance instance = CompCollisionObjectGetInstance(body->getUserPointer());
        if (!instance)
        {
            lua_pushnil(L);
            return;
        }
        PushBullet3DCollisionObject(L, body, dmGameObject::GetCollection(instance), dmGameObject::GetIdentifier(instance));
    }

    static btScalar CheckFiniteScalar(lua_State* L, int index, const char* name)
    {
        lua_Number value = luaL_checknumber(L, index);
        btScalar   bullet_value = (btScalar)value;
        if (!isfinite((double)value) || !isfinite((double)bullet_value))
        {
            luaL_error(L, "%s must be finite and within Bullet's numeric range.", name);
            return btScalar(0.0f);
        }
        return bullet_value;
    }

    static btScalar CheckFiniteScalarInRange(lua_State* L, int index, const char* name, btScalar minimum, btScalar maximum)
    {
        btScalar value = CheckFiniteScalar(L, index, name);
        if (value < minimum || value > maximum)
        {
            luaL_error(L, "%s must be between %g and %g.", name, (double)minimum, (double)maximum);
        }
        return value;
    }

    static btScalar CheckScaledScalar(lua_State* L, int index, const char* name, btScalar scale)
    {
        btScalar value = CheckFiniteScalar(L, index, name);
        btScalar scaled = value * scale;
        if (!isfinite((double)scaled))
        {
            luaL_error(L, "%s is outside the supported range.", name);
        }
        return scaled;
    }

    static bool CheckBoolean(lua_State* L, int index, const char* name)
    {
        if (!lua_isboolean(L, index))
        {
            luaL_error(L, "%s must be a boolean.", name);
            return false;
        }
        return lua_toboolean(L, index) != 0;
    }

    static bool GetBooleanField(lua_State* L, int table_index, const char* name, bool default_value)
    {
        table_index = AbsIndex(L, table_index);
        lua_getfield(L, table_index, name);
        bool value = default_value;
        if (!lua_isnil(L, -1))
        {
            value = CheckBoolean(L, -1, name);
        }
        lua_pop(L, 1);
        return value;
    }

    static void RejectIneffectiveField(lua_State* L, int table_index, const char* name, const char* constraint_type)
    {
        table_index = AbsIndex(L, table_index);
        lua_getfield(L, table_index, name);
        bool present = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if (present)
        {
            luaL_error(L, "Field '%s' is ineffective for Bullet 2.77 %s constraints.", name, constraint_type);
        }
    }

    static btVector3 CheckFiniteVector3(lua_State* L, int index, btScalar scale, const char* name)
    {
        btVector3 value = CheckBullet3DVector3(L, index, scale);
        if (!isfinite((double)value.getX()) || !isfinite((double)value.getY()) || !isfinite((double)value.getZ()))
        {
            luaL_error(L, "%s must contain finite components.", name);
        }
        return value;
    }

    static btVector3 GetVector3Field(lua_State* L, int table_index, const char* name, btScalar scale, bool required, bool* present)
    {
        table_index = AbsIndex(L, table_index);
        lua_getfield(L, table_index, name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            if (present)
            {
                *present = false;
            }
            if (required)
            {
                luaL_error(L, "Missing required field '%s'.", name);
            }
            return btVector3(0.0f, 0.0f, 0.0f);
        }

        btVector3 value = CheckFiniteVector3(L, -1, scale, name);
        lua_pop(L, 1);
        if (present)
        {
            *present = true;
        }
        return value;
    }

    static btQuaternion GetQuaternionField(lua_State* L, int table_index, const char* name, bool required, bool* present)
    {
        table_index = AbsIndex(L, table_index);
        lua_getfield(L, table_index, name);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            if (present)
            {
                *present = false;
            }
            if (required)
            {
                luaL_error(L, "Missing required field '%s'.", name);
            }
            return btQuaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }

        btQuaternion value = CheckBullet3DFiniteQuat(L, -1, name);
        lua_pop(L, 1);
        if (present)
        {
            *present = true;
        }
        return value;
    }

    static btTransform GetTransformFields(lua_State* L, int table_index, const char* position_name, const char* rotation_name, bool required, bool* present)
    {
        bool         position_present = false;
        bool         rotation_present = false;
        btVector3    position = GetVector3Field(L, table_index, position_name, GetBullet3DPhysicsScale(), required, &position_present);
        btQuaternion rotation = GetQuaternionField(L, table_index, rotation_name, required, &rotation_present);
        if (position_present != rotation_present)
        {
            luaL_error(L, "Fields '%s' and '%s' must be supplied together.", position_name, rotation_name);
        }
        if (present)
        {
            *present = position_present;
        }
        return btTransform(rotation, position);
    }

    static void PushTransform(lua_State* L, const btTransform& transform)
    {
        PushBullet3DVector3(L, transform.getOrigin(), GetBullet3DInvPhysicsScale());
        PushBullet3DQuat(L, transform.getRotation());
    }

    static void* GetBodyComponentWorld(btRigidBody* body)
    {
        if (!body || !body->getUserPointer())
        {
            return 0;
        }
        dmGameObject::HInstance instance = CompCollisionObjectGetInstance(body->getUserPointer());
        if (!instance)
        {
            return 0;
        }
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
        uint32_t                  component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        return dmGameObject::GetWorld(collection, component_type_index);
    }

    static void RegisterBodyState(btDiscreteDynamicsWorld* world, btRigidBody* body)
    {
        if (!body || g_Bullet3DBodyStates.Get(PtrToKey(body)))
        {
            return;
        }
        EnsureBodyStateCapacity();
        Bullet3DBodyState state = { world, body->getBroadphaseHandle() != 0 };
        g_Bullet3DBodyStates.Put(PtrToKey(body), state);
    }

    static Bullet3DCreateInput CheckCreateInput(lua_State* L, bool body_b_required)
    {
        Bullet3DCreateInput input = {};
        input.m_BodyA = CheckBullet3DRigidBody(L, 1);
        input.m_BodyB = lua_isnoneornil(L, 2) ? 0 : CheckBullet3DRigidBody(L, 2);
        luaL_checktype(L, 3, LUA_TTABLE);

        if (body_b_required && !input.m_BodyB)
        {
            luaL_error(L, "body_b is required for this constraint type.");
        }
        if (input.m_BodyA == input.m_BodyB)
        {
            luaL_error(L, "body_a and body_b must be different rigid bodies.");
        }

        input.m_ComponentWorld = GetBodyComponentWorld(input.m_BodyA);
        if (!input.m_ComponentWorld)
        {
            luaL_error(L, "body_a does not belong to a live bullet3d world.");
        }
        input.m_World = (btDiscreteDynamicsWorld*)CompCollisionObjectGetBullet3DWorld((dmGameObject::HComponentWorld)input.m_ComponentWorld);
        if (!input.m_World)
        {
            luaL_error(L, "body_a does not belong to a live bullet3d world.");
        }
        if (input.m_BodyB)
        {
            void* body_b_world = GetBodyComponentWorld(input.m_BodyB);
            if (!body_b_world || body_b_world != input.m_ComponentWorld)
            {
                luaL_error(L, "Constraints can only connect bodies in the same bullet3d world.");
            }
        }

        CheckWorldUnlocked(L, input.m_ComponentWorld);
        input.m_CollideConnected = GetBooleanField(L, 3, "collide_connected", false);
        RegisterBodyState(input.m_World, input.m_BodyA);
        RegisterBodyState(input.m_World, input.m_BodyB);
        return input;
    }

    static int CheckAxis(lua_State* L, int index, int count, const char* name)
    {
        lua_Number value = luaL_checknumber(L, index);
        if (!isfinite((double)value) || floor((double)value) != value || value < 1 || value > count)
        {
            luaL_error(L, "%s must be an integer between 1 and %d.", name, count);
            return 0;
        }
        return (int)value - 1;
    }

    static btVector3 CheckUnitVector(lua_State* L, int index, const char* name)
    {
        btVector3 value = CheckFiniteVector3(L, index, 1.0f, name);
        btScalar  length_squared = value.length2();
        if (!(length_squared > SIMD_EPSILON) || !isfinite((double)length_squared))
        {
            luaL_error(L, "%s must be a finite, non-zero vector.", name);
        }
        value.normalize();
        return value;
    }

    static btVector3 GetUnitVectorField(lua_State* L, int table_index, const char* name)
    {
        table_index = AbsIndex(L, table_index);
        lua_getfield(L, table_index, name);
        if (lua_isnil(L, -1))
        {
            luaL_error(L, "Missing required field '%s'.", name);
        }
        btVector3 value = CheckUnitVector(L, -1, name);
        lua_pop(L, 1);
        return value;
    }

    static void CheckOrthogonalAxes(lua_State* L, const btVector3& axis1, const btVector3& axis2)
    {
        if (btFabs(axis1.dot(axis2)) > btScalar(1.0e-4f))
        {
            luaL_error(L, "axis1 and axis2 must be orthogonal.");
        }
    }

    static int Constraint_CreatePointToPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput      input = CheckCreateInput(L, false);
        btVector3                pivot_a = GetVector3Field(L, 3, "pivot_a", GetBullet3DPhysicsScale(), true, 0);
        bool                     pivot_b_present = false;
        btVector3                pivot_b = GetVector3Field(L, 3, "pivot_b", GetBullet3DPhysicsScale(), input.m_BodyB != 0, &pivot_b_present);

        btPoint2PointConstraint* constraint = input.m_BodyB ? new btPoint2PointConstraint(*input.m_BodyA, *input.m_BodyB, pivot_a, pivot_b) : new btPoint2PointConstraint(*input.m_BodyA, pivot_a);
        if (!input.m_BodyB && pivot_b_present)
        {
            constraint->setPivotB(pivot_b);
        }
        PushConstraint(L, RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_POINT_TO_POINT));
        return 1;
    }

    static int Constraint_CreateHinge(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, false);
        btTransform         frame_a = GetTransformFields(L, 3, "frame_a_position", "frame_a_rotation", true, 0);
        btTransform         frame_b;
        if (input.m_BodyB)
        {
            frame_b = GetTransformFields(L, 3, "frame_b_position", "frame_b_rotation", true, 0);
        }
        bool               use_reference_frame_a = GetBooleanField(L, 3, "use_reference_frame_a", false);
        bool               angular_only = GetBooleanField(L, 3, "angular_only", false);

        btHingeConstraint* constraint = input.m_BodyB ? new btHingeConstraint(*input.m_BodyA, *input.m_BodyB, frame_a, frame_b, use_reference_frame_a) : new btHingeConstraint(*input.m_BodyA, frame_a, use_reference_frame_a);
        constraint->setAngularOnly(angular_only);
        uint64_t id = RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_HINGE);
        g_Bullet3DConstraints.Get(id)->m_AngularOnly = angular_only;
        PushConstraint(L, id);
        return 1;
    }

    static int Constraint_CreateConeTwist(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, false);
        btTransform         frame_a = GetTransformFields(L, 3, "frame_a_position", "frame_a_rotation", true, 0);
        btTransform         frame_b;
        if (input.m_BodyB)
        {
            frame_b = GetTransformFields(L, 3, "frame_b_position", "frame_b_rotation", true, 0);
        }
        bool                   angular_only = GetBooleanField(L, 3, "angular_only", false);

        btConeTwistConstraint* constraint = input.m_BodyB ? new btConeTwistConstraint(*input.m_BodyA, *input.m_BodyB, frame_a, frame_b) : new btConeTwistConstraint(*input.m_BodyA, frame_a);
        if (!input.m_BodyB)
        {
            // Bullet 2.77 copies the body-A-local frame into the fixed body's frame.
            const_cast<btTransform&>(constraint->getBFrame()) = input.m_BodyA->getCenterOfMassTransform() * frame_a;
        }
        constraint->setMotorTargetInConstraintSpace(btQuaternion(0.0f, 0.0f, 0.0f, 1.0f));
        constraint->setAngularOnly(angular_only);
        uint64_t id = RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_CONE_TWIST);
        g_Bullet3DConstraints.Get(id)->m_AngularOnly = angular_only;
        PushConstraint(L, id);
        return 1;
    }

    static int Constraint_CreateGeneric6Dof(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, false);
        btTransform         frame_a = GetTransformFields(L, 3, "frame_a_position", "frame_a_rotation", true, 0);
        btTransform         frame_b;
        if (input.m_BodyB)
        {
            frame_b = GetTransformFields(L, 3, "frame_b_position", "frame_b_rotation", true, 0);
        }
        RejectIneffectiveField(L, 3, "use_linear_reference_frame_a", "generic 6-DOF");

        btGeneric6DofConstraint* constraint = input.m_BodyB ? new btGeneric6DofConstraint(*input.m_BodyA, *input.m_BodyB, frame_a, frame_b, true) : new btGeneric6DofConstraint(*input.m_BodyA, frame_a, true);
        PushConstraint(L, RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_GENERIC_6DOF));
        return 1;
    }

    static int Constraint_CreateGeneric6DofSpring(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, true);
        btTransform         frame_a = GetTransformFields(L, 3, "frame_a_position", "frame_a_rotation", true, 0);
        btTransform         frame_b = GetTransformFields(L, 3, "frame_b_position", "frame_b_rotation", true, 0);
        RejectIneffectiveField(L, 3, "use_linear_reference_frame_a", "generic spring 6-DOF");

        btGeneric6DofSpringConstraint* constraint = new btGeneric6DofSpringConstraint(*input.m_BodyA, *input.m_BodyB, frame_a, frame_b, true);
        PushConstraint(L, RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING));
        return 1;
    }

    static int Constraint_CreateSlider(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, false);
        btTransform         frame_a = GetTransformFields(L, 3, "frame_a_position", "frame_a_rotation", true, 0);
        btTransform         frame_b;
        if (input.m_BodyB)
        {
            frame_b = GetTransformFields(L, 3, "frame_b_position", "frame_b_rotation", true, 0);
        }
        bool                use_linear_reference_frame_a = GetBooleanField(L, 3, "use_linear_reference_frame_a", true);

        bool                native_use_linear_reference_frame_a = input.m_BodyB ? use_linear_reference_frame_a : !use_linear_reference_frame_a;
        btSliderConstraint* constraint = input.m_BodyB ? new btSliderConstraint(*input.m_BodyA, *input.m_BodyB, frame_a, frame_b, native_use_linear_reference_frame_a) : new btSliderConstraint(*input.m_BodyA, frame_a, native_use_linear_reference_frame_a);
        PushConstraint(L, RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_SLIDER));
        return 1;
    }

    static int Constraint_CreateUniversal(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, true);
        btVector3           anchor = GetVector3Field(L, 3, "anchor", GetBullet3DPhysicsScale(), true, 0);
        btVector3           axis1 = GetUnitVectorField(L, 3, "axis1");
        btVector3           axis2 = GetUnitVectorField(L, 3, "axis2");
        CheckOrthogonalAxes(L, axis1, axis2);

        btUniversalConstraint* constraint = new btUniversalConstraint(*input.m_BodyA, *input.m_BodyB, anchor, axis1, axis2);
        PushConstraint(L, RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_UNIVERSAL));
        return 1;
    }

    static int Constraint_CreateHinge2(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DCreateInput input = CheckCreateInput(L, true);
        btVector3           anchor = GetVector3Field(L, 3, "anchor", GetBullet3DPhysicsScale(), true, 0);
        btVector3           axis1 = GetUnitVectorField(L, 3, "axis1");
        btVector3           axis2 = GetUnitVectorField(L, 3, "axis2");
        CheckOrthogonalAxes(L, axis1, axis2);

        btHinge2Constraint* constraint = new btHinge2Constraint(*input.m_BodyA, *input.m_BodyB, anchor, axis1, axis2);
        btScalar            scale = GetBullet3DPhysicsScale();
        constraint->setLimit(2, -scale, scale);
        PushConstraint(L, RegisterConstraint(L, input, constraint, BULLET3D_CONSTRAINT_HINGE2));
        return 1;
    }

    static int Constraint_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, IsBullet3DConstraintValid(L, 1));
        return 1;
    }

    static int Constraint_IsActive(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, CheckConstraintMeta(L, 1)->m_InWorld != 0);
        return 1;
    }

    static int Constraint_Destroy(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DLuaConstraint*  lua_constraint = CheckConstraintInternal(L, 1);
        Bullet3DConstraintMeta* meta = VerifyConstraintInternal(L, lua_constraint, true);
        CheckConstraintUnlocked(L, meta);
        DestroyConstraintId(lua_constraint->m_Id);
        return 0;
    }

    static int Constraint_GetType(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushstring(L, ConstraintKindName(CheckConstraintMeta(L, 1)->m_Kind));
        return 1;
    }

    static int Constraint_GetBodyA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushRigidBody(L, CheckConstraintMeta(L, 1)->m_BodyA);
        return 1;
    }

    static int Constraint_GetBodyB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushRigidBody(L, CheckConstraintMeta(L, 1)->m_BodyB);
        return 1;
    }

    static int Constraint_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        PushBullet3DWorld(L, meta->m_World, meta->m_ComponentWorld);
        return 1;
    }

    static int Constraint_GetCollideConnected(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushboolean(L, CheckConstraintMeta(L, 1)->m_CollideConnected != 0);
        return 1;
    }

    static btPoint2PointConstraint* CheckPointToPoint(lua_State* L, Bullet3DConstraintMeta* meta)
    {
        if (meta->m_Kind != BULLET3D_CONSTRAINT_POINT_TO_POINT)
        {
            luaL_error(L, "Expected a point-to-point constraint.");
        }
        return (btPoint2PointConstraint*)meta->m_Constraint;
    }

    static int Constraint_GetPivots(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        btPoint2PointConstraint* constraint = CheckPointToPoint(L, CheckConstraintMeta(L, 1));
        PushBullet3DVector3(L, constraint->getPivotInA(), GetBullet3DInvPhysicsScale());
        PushBullet3DVector3(L, constraint->getPivotInB(), GetBullet3DInvPhysicsScale());
        return 2;
    }

    static int Constraint_SetPivots(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*  meta = CheckConstraintMeta(L, 1);
        btPoint2PointConstraint* constraint = CheckPointToPoint(L, meta);
        btVector3                pivot_a = CheckFiniteVector3(L, 2, GetBullet3DPhysicsScale(), "pivot_a");
        btVector3                pivot_b = CheckFiniteVector3(L, 3, GetBullet3DPhysicsScale(), "pivot_b");
        CheckConstraintUnlocked(L, meta);
        constraint->setPivotA(pivot_a);
        constraint->setPivotB(pivot_b);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static const btTransform* GetConstraintFrame(Bullet3DConstraintMeta* meta, bool frame_a)
    {
        bool reverse_one_body = !meta->m_BodyB && (meta->m_Kind == BULLET3D_CONSTRAINT_GENERIC_6DOF || meta->m_Kind == BULLET3D_CONSTRAINT_SLIDER);
        bool native_a = reverse_one_body ? !frame_a : frame_a;
        switch (meta->m_Kind)
        {
            case BULLET3D_CONSTRAINT_HINGE:
            {
                btHingeConstraint* constraint = (btHingeConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getAFrame() : &constraint->getBFrame();
            }
            case BULLET3D_CONSTRAINT_CONE_TWIST:
            {
                btConeTwistConstraint* constraint = (btConeTwistConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getAFrame() : &constraint->getBFrame();
            }
            case BULLET3D_CONSTRAINT_GENERIC_6DOF:
            case BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING:
            case BULLET3D_CONSTRAINT_UNIVERSAL:
            case BULLET3D_CONSTRAINT_HINGE2:
            {
                btGeneric6DofConstraint* constraint = (btGeneric6DofConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getFrameOffsetA() : &constraint->getFrameOffsetB();
            }
            case BULLET3D_CONSTRAINT_SLIDER:
            {
                btSliderConstraint* constraint = (btSliderConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getFrameOffsetA() : &constraint->getFrameOffsetB();
            }
            default:
                return 0;
        }
    }

    static btTransform* GetMutableConstraintFrame(Bullet3DConstraintMeta* meta, bool frame_a)
    {
        bool reverse_one_body = !meta->m_BodyB && (meta->m_Kind == BULLET3D_CONSTRAINT_GENERIC_6DOF || meta->m_Kind == BULLET3D_CONSTRAINT_SLIDER);
        bool native_a = reverse_one_body ? !frame_a : frame_a;
        switch (meta->m_Kind)
        {
            case BULLET3D_CONSTRAINT_HINGE:
            {
                btHingeConstraint* constraint = (btHingeConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getAFrame() : &constraint->getBFrame();
            }
            case BULLET3D_CONSTRAINT_GENERIC_6DOF:
            case BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING:
            {
                btGeneric6DofConstraint* constraint = (btGeneric6DofConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getFrameOffsetA() : &constraint->getFrameOffsetB();
            }
            case BULLET3D_CONSTRAINT_SLIDER:
            {
                btSliderConstraint* constraint = (btSliderConstraint*)meta->m_Constraint;
                return native_a ? &constraint->getFrameOffsetA() : &constraint->getFrameOffsetB();
            }
            default:
                return 0;
        }
    }

    static int Constraint_GetFrame(lua_State* L, bool frame_a)
    {
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        const btTransform*      frame = GetConstraintFrame(meta, frame_a);
        if (!frame)
        {
            return luaL_error(L, "This constraint type does not have local frames.");
        }
        PushTransform(L, *frame);
        return 2;
    }

    static int Constraint_GetFrameA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        return Constraint_GetFrame(L, true);
    }

    static int Constraint_GetFrameB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        return Constraint_GetFrame(L, false);
    }

    static int Constraint_SetFrame(lua_State* L, bool frame_a)
    {
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btVector3               position = CheckFiniteVector3(L, 2, GetBullet3DPhysicsScale(), "position");
        btQuaternion            rotation = CheckBullet3DFiniteQuat(L, 3, "rotation");
        btTransform*            frame = GetMutableConstraintFrame(meta, frame_a);
        if (!frame)
        {
            return luaL_error(L, "Frames cannot be changed for this constraint type.");
        }
        CheckConstraintUnlocked(L, meta);
        *frame = btTransform(rotation, position);
        if (Is6DofKind(meta->m_Kind))
        {
            ((btGeneric6DofConstraint*)meta->m_Constraint)->calculateTransforms();
        }
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetFrameA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        return Constraint_SetFrame(L, true);
    }

    static int Constraint_SetFrameB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        return Constraint_SetFrame(L, false);
    }

    static int Constraint_GetAngularOnly(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        if (meta->m_Kind == BULLET3D_CONSTRAINT_HINGE)
        {
            lua_pushboolean(L, ((btHingeConstraint*)meta->m_Constraint)->getAngularOnly());
        }
        else if (meta->m_Kind == BULLET3D_CONSTRAINT_CONE_TWIST)
        {
            lua_pushboolean(L, meta->m_AngularOnly != 0);
        }
        else
        {
            return luaL_error(L, "Expected a hinge or cone-twist constraint.");
        }
        return 1;
    }

    static int Constraint_SetAngularOnly(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        bool                    angular_only = CheckBoolean(L, 2, "angular_only");
        CheckConstraintUnlocked(L, meta);
        if (meta->m_Kind == BULLET3D_CONSTRAINT_HINGE)
        {
            ((btHingeConstraint*)meta->m_Constraint)->setAngularOnly(angular_only);
        }
        else if (meta->m_Kind == BULLET3D_CONSTRAINT_CONE_TWIST)
        {
            ((btConeTwistConstraint*)meta->m_Constraint)->setAngularOnly(angular_only);
        }
        else
        {
            return luaL_error(L, "Expected a hinge or cone-twist constraint.");
        }
        meta->m_AngularOnly = angular_only;
        ActivateConstraintBodies(meta);
        return 0;
    }

    static btHingeConstraint* CheckHinge(lua_State* L, Bullet3DConstraintMeta* meta)
    {
        if (meta->m_Kind != BULLET3D_CONSTRAINT_HINGE)
        {
            luaL_error(L, "Expected a hinge constraint.");
        }
        return (btHingeConstraint*)meta->m_Constraint;
    }

    static int Constraint_GetHingeAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckHinge(L, CheckConstraintMeta(L, 1))->getHingeAngle());
        return 1;
    }

    static int Constraint_GetHingeLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        btHingeConstraint* constraint = CheckHinge(L, CheckConstraintMeta(L, 1));
        lua_pushnumber(L, constraint->getLowerLimit());
        lua_pushnumber(L, constraint->getUpperLimit());
        return 2;
    }

    static int Constraint_SetHingeLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btHingeConstraint*      constraint = CheckHinge(L, meta);
        btScalar                lower = CheckFiniteScalar(L, 2, "lower");
        btScalar                upper = CheckFiniteScalar(L, 3, "upper");
        btScalar                bias = lua_isnoneornil(L, 4) ? btScalar(0.3f) : CheckFiniteScalarInRange(L, 4, "bias", 0.0f, 1.0f);
        btScalar                relaxation = lua_isnoneornil(L, 5) ? btScalar(1.0f) : CheckFiniteScalarInRange(L, 5, "relaxation", 0.0f, 1.0f);
        CheckConstraintUnlocked(L, meta);
        constraint->setLimit(lower, upper, btScalar(0.9f), bias, relaxation);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_GetHingeMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 3);
        btHingeConstraint* constraint = CheckHinge(L, CheckConstraintMeta(L, 1));
        lua_pushboolean(L, constraint->getEnableAngularMotor());
        lua_pushnumber(L, constraint->getMotorTargetVelosity());
        btScalar inv_scale = GetBullet3DInvPhysicsScale();
        lua_pushnumber(L, constraint->getMaxMotorImpulse() * inv_scale * inv_scale);
        return 3;
    }

    static int Constraint_SetHingeMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btHingeConstraint*      constraint = CheckHinge(L, meta);
        bool                    enabled = CheckBoolean(L, 2, "enabled");
        btScalar                target_velocity = CheckFiniteScalar(L, 3, "target_velocity");
        btScalar                scale = GetBullet3DPhysicsScale();
        btScalar                max_impulse = CheckScaledScalar(L, 4, "max_impulse", scale * scale);
        if (max_impulse < btScalar(0.0f))
        {
            return luaL_error(L, "max_impulse must not be negative.");
        }
        CheckConstraintUnlocked(L, meta);
        constraint->enableAngularMotor(enabled, target_velocity, max_impulse);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetHingeMotorTarget(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btHingeConstraint*      constraint = CheckHinge(L, meta);
        btScalar                target_angle = CheckFiniteScalar(L, 2, "target_angle");
        btScalar                time_step = CheckFiniteScalar(L, 3, "time_step");
        if (!(time_step > btScalar(0.0f)))
        {
            return luaL_error(L, "time_step must be greater than zero.");
        }
        CheckConstraintUnlocked(L, meta);
        constraint->setMotorTarget(target_angle, time_step);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetHingeAxis(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btHingeConstraint*      constraint = CheckHinge(L, meta);
        btVector3               axis = CheckUnitVector(L, 2, "axis");
        if (meta->m_BodyB)
        {
            return luaL_error(L, "set_hinge_axis only supports one-body hinges; set both local frames for a two-body hinge.");
        }
        CheckConstraintUnlocked(L, meta);
        constraint->setAxis(axis);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static btConeTwistConstraint* CheckConeTwist(lua_State* L, Bullet3DConstraintMeta* meta)
    {
        if (meta->m_Kind != BULLET3D_CONSTRAINT_CONE_TWIST)
        {
            luaL_error(L, "Expected a cone-twist constraint.");
        }
        return (btConeTwistConstraint*)meta->m_Constraint;
    }

    static void RefreshConeTwist(btConeTwistConstraint* constraint)
    {
        const btRigidBody& body_a = constraint->getRigidBodyA();
        const btRigidBody& body_b = constraint->getRigidBodyB();
        constraint->calcAngleInfo2(body_a.getCenterOfMassTransform(), body_b.getCenterOfMassTransform(), body_a.getInvInertiaTensorWorld(), body_b.getInvInertiaTensorWorld());
    }

    static int Constraint_GetConeTwistLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 3);
        btConeTwistConstraint* constraint = CheckConeTwist(L, CheckConstraintMeta(L, 1));
        lua_pushnumber(L, constraint->getSwingSpan1());
        lua_pushnumber(L, constraint->getSwingSpan2());
        lua_pushnumber(L, constraint->getTwistSpan());
        return 3;
    }

    static int Constraint_SetConeTwistLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btConeTwistConstraint*  constraint = CheckConeTwist(L, meta);
        btScalar                swing1 = CheckFiniteScalar(L, 2, "swing_span_1");
        btScalar                swing2 = CheckFiniteScalar(L, 3, "swing_span_2");
        btScalar                twist = CheckFiniteScalar(L, 4, "twist_span");
        if (swing1 < 0.0f || swing2 < 0.0f || twist < 0.0f)
        {
            return luaL_error(L, "Cone-twist spans must not be negative.");
        }
        btScalar softness = lua_isnoneornil(L, 5) ? btScalar(1.0f) : CheckFiniteScalarInRange(L, 5, "softness", 0.0f, 1.0f);
        btScalar bias = lua_isnoneornil(L, 6) ? btScalar(0.3f) : CheckFiniteScalarInRange(L, 6, "bias", 0.0f, 1.0f);
        btScalar relaxation = lua_isnoneornil(L, 7) ? btScalar(1.0f) : CheckFiniteScalarInRange(L, 7, "relaxation", 0.0f, 1.0f);
        CheckConstraintUnlocked(L, meta);
        constraint->setLimit(swing1, swing2, twist, softness, bias, relaxation);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_GetTwistAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btConeTwistConstraint* constraint = CheckConeTwist(L, CheckConstraintMeta(L, 1));
        RefreshConeTwist(constraint);
        lua_pushnumber(L, constraint->getTwistAngle());
        return 1;
    }

    static int Constraint_IsPastSwingLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btConeTwistConstraint* constraint = CheckConeTwist(L, CheckConstraintMeta(L, 1));
        RefreshConeTwist(constraint);
        lua_pushboolean(L, constraint->isPastSwingLimit());
        return 1;
    }

    static int Constraint_EnableConeTwistMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        bool                    enabled = CheckBoolean(L, 2, "enabled");
        CheckConstraintUnlocked(L, meta);
        CheckConeTwist(L, meta)->enableMotor(enabled);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetConeTwistMotorTarget(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btConeTwistConstraint*  constraint = CheckConeTwist(L, meta);
        btQuaternion            target = CheckBullet3DFiniteQuat(L, 2, "target");
        bool                    constraint_space = lua_isnoneornil(L, 3) ? false : CheckBoolean(L, 3, "constraint_space");
        CheckConstraintUnlocked(L, meta);
        if (constraint_space)
            constraint->setMotorTargetInConstraintSpace(target);
        else
            constraint->setMotorTarget(target);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static btGeneric6DofConstraint* Check6Dof(lua_State* L, Bullet3DConstraintMeta* meta)
    {
        if (!Is6DofKind(meta->m_Kind))
        {
            luaL_error(L, "Expected a 6-DOF-derived constraint.");
        }
        return (btGeneric6DofConstraint*)meta->m_Constraint;
    }

    static void Get6DofLimit(btGeneric6DofConstraint* constraint, int axis, btScalar* lower, btScalar* upper)
    {
        if (axis < 3)
        {
            btTranslationalLimitMotor* motor = constraint->getTranslationalLimitMotor();
            *lower = motor->m_lowerLimit[axis];
            *upper = motor->m_upperLimit[axis];
        }
        else
        {
            btRotationalLimitMotor* motor = constraint->getRotationalLimitMotor(axis - 3);
            *lower = motor->m_loLimit;
            *upper = motor->m_hiLimit;
        }
    }

    static int Constraint_GetLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        Bullet3DConstraintMeta*  meta = CheckConstraintMeta(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, meta);
        int                      axis = CheckAxis(L, 2, 6, "axis");
        btScalar                 lower;
        btScalar                 upper;
        Get6DofLimit(constraint, axis, &lower, &upper);
        btScalar output_scale = axis < 3 ? GetBullet3DInvPhysicsScale() : btScalar(1.0f);
        lua_pushnumber(L, lower * output_scale);
        lua_pushnumber(L, upper * output_scale);
        return 2;
    }

    static int Constraint_SetLimit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*  meta = CheckConstraintMeta(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, meta);
        int                      axis = CheckAxis(L, 2, 6, "axis");
        btScalar                 input_scale = axis < 3 ? GetBullet3DPhysicsScale() : btScalar(1.0f);
        btScalar                 lower = CheckScaledScalar(L, 3, "lower", input_scale);
        btScalar                 upper = CheckScaledScalar(L, 4, "upper", input_scale);
        CheckConstraintUnlocked(L, meta);
        constraint->setLimit(axis, lower, upper);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_IsLimited(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        int                     axis = CheckAxis(L, 2, 6, "axis");
        lua_pushboolean(L, Check6Dof(L, meta)->isLimited(axis));
        return 1;
    }

    static int Constraint_Get6DofAxis(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, CheckConstraintMeta(L, 1));
        int                      axis = CheckAxis(L, 2, 3, "axis");
        constraint->calculateTransforms();
        PushBullet3DVector3(L, constraint->getAxis(axis), 1.0f);
        return 1;
    }

    static int Constraint_Get6DofAngle(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, CheckConstraintMeta(L, 1));
        int                      axis = CheckAxis(L, 2, 3, "axis");
        constraint->calculateTransforms();
        lua_pushnumber(L, constraint->getAngle(axis));
        return 1;
    }

    static int Constraint_Get6DofPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, CheckConstraintMeta(L, 1));
        int                      axis = CheckAxis(L, 2, 3, "axis");
        constraint->calculateTransforms();
        lua_pushnumber(L, constraint->getRelativePivotPosition(axis) * GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int Constraint_Get6DofMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 4);
        Bullet3DConstraintMeta*  meta = CheckConstraintMeta(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, meta);
        int                      axis = CheckAxis(L, 2, 6, "axis");
        bool                     enabled;
        btScalar                 target_velocity;
        btScalar                 max_impulse;
        btScalar                 bounce = 0.0f;
        if (axis < 3)
        {
            btTranslationalLimitMotor* motor = constraint->getTranslationalLimitMotor();
            enabled = motor->m_enableMotor[axis];
            target_velocity = motor->m_targetVelocity[axis] * GetBullet3DInvPhysicsScale();
            max_impulse = motor->m_maxMotorForce[axis] * GetBullet3DInvPhysicsScale();
        }
        else
        {
            btRotationalLimitMotor* motor = constraint->getRotationalLimitMotor(axis - 3);
            enabled = motor->m_enableMotor;
            target_velocity = motor->m_targetVelocity;
            btScalar inv_scale = GetBullet3DInvPhysicsScale();
            max_impulse = motor->m_maxMotorForce * inv_scale * inv_scale;
            bounce = motor->m_bounce;
        }
        lua_pushboolean(L, enabled);
        lua_pushnumber(L, target_velocity);
        lua_pushnumber(L, max_impulse);
        lua_pushnumber(L, bounce);
        return 4;
    }

    static int Constraint_Set6DofMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*  meta = CheckConstraintMeta(L, 1);
        btGeneric6DofConstraint* constraint = Check6Dof(L, meta);
        int                      axis = CheckAxis(L, 2, 6, "axis");
        bool                     enabled = CheckBoolean(L, 3, "enabled");
        btScalar                 target_velocity = CheckFiniteScalar(L, 4, "target_velocity");
        btScalar                 scale = GetBullet3DPhysicsScale();
        if (axis < 3)
        {
            target_velocity = CheckScaledScalar(L, 4, "target_velocity", scale);
        }
        btScalar max_scale = axis < 3 ? scale : scale * scale;
        btScalar max_impulse = CheckScaledScalar(L, 5, "max_impulse", max_scale);
        if (max_impulse < btScalar(0.0f))
        {
            return luaL_error(L, "max_impulse must not be negative.");
        }
        btScalar bounce = lua_isnoneornil(L, 6) ? btScalar(0.0f) : CheckFiniteScalarInRange(L, 6, "bounce", 0.0f, 1.0f);
        if (axis < 3 && bounce != btScalar(0.0f))
        {
            return luaL_error(L, "bounce is only supported by angular 6-DOF motors.");
        }

        CheckConstraintUnlocked(L, meta);
        if (axis < 3)
        {
            btTranslationalLimitMotor* motor = constraint->getTranslationalLimitMotor();
            motor->m_enableMotor[axis] = enabled;
            motor->m_targetVelocity[axis] = target_velocity;
            motor->m_maxMotorForce[axis] = max_impulse;
        }
        else
        {
            btRotationalLimitMotor* motor = constraint->getRotationalLimitMotor(axis - 3);
            motor->m_enableMotor = enabled;
            motor->m_targetVelocity = target_velocity;
            motor->m_maxMotorForce = max_impulse;
            motor->m_bounce = bounce;
        }
        ActivateConstraintBodies(meta);
        return 0;
    }

    static btGeneric6DofSpringConstraint* CheckSpring6Dof(lua_State* L, Bullet3DConstraintMeta* meta)
    {
        if (meta->m_Kind != BULLET3D_CONSTRAINT_GENERIC_6DOF_SPRING && meta->m_Kind != BULLET3D_CONSTRAINT_HINGE2)
        {
            luaL_error(L, "Expected a spring 6-DOF or hinge2 constraint.");
        }
        return (btGeneric6DofSpringConstraint*)meta->m_Constraint;
    }

    static int Constraint_EnableSpring(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*        meta = CheckConstraintMeta(L, 1);
        btGeneric6DofSpringConstraint* constraint = CheckSpring6Dof(L, meta);
        int                            axis = CheckAxis(L, 2, 6, "axis");
        bool                           enabled = CheckBoolean(L, 3, "enabled");
        CheckConstraintUnlocked(L, meta);
        constraint->enableSpring(axis, enabled);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetSpringStiffness(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*        meta = CheckConstraintMeta(L, 1);
        btGeneric6DofSpringConstraint* constraint = CheckSpring6Dof(L, meta);
        int                            axis = CheckAxis(L, 2, 6, "axis");
        btScalar                       stiffness = CheckFiniteScalar(L, 3, "stiffness");
        if (stiffness < btScalar(0.0f))
        {
            return luaL_error(L, "stiffness must not be negative.");
        }
        CheckConstraintUnlocked(L, meta);
        constraint->setStiffness(axis, stiffness);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetSpringDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*        meta = CheckConstraintMeta(L, 1);
        btGeneric6DofSpringConstraint* constraint = CheckSpring6Dof(L, meta);
        int                            axis = CheckAxis(L, 2, 6, "axis");
        btScalar                       damping = CheckFiniteScalarInRange(L, 3, "damping", 0.0f, 1.0f);
        CheckConstraintUnlocked(L, meta);
        constraint->setDamping(axis, damping);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_SetSpringEquilibriumPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta*        meta = CheckConstraintMeta(L, 1);
        btGeneric6DofSpringConstraint* constraint = CheckSpring6Dof(L, meta);

        if (lua_isnoneornil(L, 2))
        {
            CheckConstraintUnlocked(L, meta);
            constraint->setEquilibriumPoint();
        }
        else
        {
            int axis = CheckAxis(L, 2, 6, "axis");
            if (lua_isnoneornil(L, 3))
            {
                CheckConstraintUnlocked(L, meta);
                constraint->setEquilibriumPoint(axis);
            }
            else
            {
                btScalar scale = axis < 3 ? GetBullet3DPhysicsScale() : btScalar(1.0f);
                btScalar value = CheckScaledScalar(L, 3, "value", scale);
                CheckConstraintUnlocked(L, meta);
                constraint->setEquilibriumPoint(axis, value);
            }
        }
        ActivateConstraintBodies(meta);
        return 0;
    }

    static btSliderConstraint* CheckSlider(lua_State* L, Bullet3DConstraintMeta* meta)
    {
        if (meta->m_Kind != BULLET3D_CONSTRAINT_SLIDER)
        {
            luaL_error(L, "Expected a slider constraint.");
        }
        return (btSliderConstraint*)meta->m_Constraint;
    }

    static int Constraint_GetSliderLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 4);
        btSliderConstraint* constraint = CheckSlider(L, CheckConstraintMeta(L, 1));
        btScalar            inv_scale = GetBullet3DInvPhysicsScale();
        lua_pushnumber(L, constraint->getLowerLinLimit() * inv_scale);
        lua_pushnumber(L, constraint->getUpperLinLimit() * inv_scale);
        lua_pushnumber(L, constraint->getLowerAngLimit());
        lua_pushnumber(L, constraint->getUpperAngLimit());
        return 4;
    }

    static int Constraint_SetSliderLimits(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btSliderConstraint*     constraint = CheckSlider(L, meta);
        btScalar                scale = GetBullet3DPhysicsScale();
        btScalar                lower_linear = CheckScaledScalar(L, 2, "lower_linear", scale);
        btScalar                upper_linear = CheckScaledScalar(L, 3, "upper_linear", scale);
        btScalar                lower_angular = CheckFiniteScalar(L, 4, "lower_angular");
        btScalar                upper_angular = CheckFiniteScalar(L, 5, "upper_angular");
        CheckConstraintUnlocked(L, meta);
        constraint->setLowerLinLimit(lower_linear);
        constraint->setUpperLinLimit(upper_linear);
        constraint->setLowerAngLimit(lower_angular);
        constraint->setUpperAngLimit(upper_angular);
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_GetSliderPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btSliderConstraint* constraint = CheckSlider(L, CheckConstraintMeta(L, 1));
        constraint->calculateTransforms(constraint->getRigidBodyA().getCenterOfMassTransform(), constraint->getRigidBodyB().getCenterOfMassTransform());
        constraint->testLinLimits();
        lua_pushnumber(L, constraint->getLinearPos() * GetBullet3DInvPhysicsScale());
        return 1;
    }

    static bool CheckSliderMotorAxis(lua_State* L, int index)
    {
        const char* axis = luaL_checkstring(L, index);
        if (strcmp(axis, "linear") == 0)
            return true;
        if (strcmp(axis, "angular") == 0)
            return false;
        luaL_error(L, "motor must be 'linear' or 'angular'.");
        return false;
    }

    static int Constraint_GetSliderMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 3);
        btSliderConstraint* constraint = CheckSlider(L, CheckConstraintMeta(L, 1));
        bool                linear = CheckSliderMotorAxis(L, 2);
        if (linear)
        {
            lua_pushboolean(L, constraint->getPoweredLinMotor());
            lua_pushnumber(L, constraint->getTargetLinMotorVelocity() * GetBullet3DInvPhysicsScale());
            lua_pushnumber(L, constraint->getMaxLinMotorForce() * GetBullet3DInvPhysicsScale());
        }
        else
        {
            btScalar inv_scale = GetBullet3DInvPhysicsScale();
            lua_pushboolean(L, constraint->getPoweredAngMotor());
            lua_pushnumber(L, constraint->getTargetAngMotorVelocity());
            lua_pushnumber(L, constraint->getMaxAngMotorForce() * inv_scale * inv_scale);
        }
        return 3;
    }

    static int Constraint_SetSliderMotor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        btSliderConstraint*     constraint = CheckSlider(L, meta);
        bool                    linear = CheckSliderMotorAxis(L, 2);
        bool                    enabled = CheckBoolean(L, 3, "enabled");
        btScalar                scale = GetBullet3DPhysicsScale();
        btScalar                target_velocity = CheckScaledScalar(L, 4, "target_velocity", linear ? scale : btScalar(1.0f));
        btScalar                max_force = CheckScaledScalar(L, 5, "max_force", linear ? scale : scale * scale);
        if (max_force < btScalar(0.0f))
        {
            return luaL_error(L, "max_force must not be negative.");
        }
        CheckConstraintUnlocked(L, meta);
        if (linear)
        {
            constraint->setPoweredLinMotor(enabled);
            constraint->setTargetLinMotorVelocity(target_velocity);
            constraint->setMaxLinMotorForce(max_force);
        }
        else
        {
            constraint->setPoweredAngMotor(enabled);
            constraint->setTargetAngMotorVelocity(target_velocity);
            constraint->setMaxAngMotorForce(max_force);
        }
        ActivateConstraintBodies(meta);
        return 0;
    }

    static int Constraint_GetUseLinearReferenceFrameA(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        bool                    use_linear_reference_frame_a = CheckSlider(L, meta)->getUseLinearReferenceFrameA();
        lua_pushboolean(L, meta->m_BodyB ? use_linear_reference_frame_a : !use_linear_reference_frame_a);
        return 1;
    }

    static bool IsWorldAxisJoint(uint8_t kind)
    {
        return kind == BULLET3D_CONSTRAINT_UNIVERSAL || kind == BULLET3D_CONSTRAINT_HINGE2;
    }

    static int Constraint_GetJointAnchors(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        if (!IsWorldAxisJoint(meta->m_Kind))
        {
            return luaL_error(L, "Expected a universal or hinge2 constraint.");
        }
        ((btGeneric6DofConstraint*)meta->m_Constraint)->calculateTransforms();
        if (meta->m_Kind == BULLET3D_CONSTRAINT_UNIVERSAL)
        {
            btUniversalConstraint* constraint = (btUniversalConstraint*)meta->m_Constraint;
            PushBullet3DVector3(L, constraint->getAnchor(), GetBullet3DInvPhysicsScale());
            PushBullet3DVector3(L, constraint->getAnchor2(), GetBullet3DInvPhysicsScale());
        }
        else
        {
            btHinge2Constraint* constraint = (btHinge2Constraint*)meta->m_Constraint;
            PushBullet3DVector3(L, constraint->getAnchor(), GetBullet3DInvPhysicsScale());
            PushBullet3DVector3(L, constraint->getAnchor2(), GetBullet3DInvPhysicsScale());
        }
        return 2;
    }

    static int Constraint_GetJointAxes(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        if (!IsWorldAxisJoint(meta->m_Kind))
        {
            return luaL_error(L, "Expected a universal or hinge2 constraint.");
        }
        if (meta->m_Kind == BULLET3D_CONSTRAINT_UNIVERSAL)
        {
            btUniversalConstraint* constraint = (btUniversalConstraint*)meta->m_Constraint;
            PushBullet3DVector3(L, constraint->getAxis1(), 1.0f);
            PushBullet3DVector3(L, constraint->getAxis2(), 1.0f);
        }
        else
        {
            btHinge2Constraint* constraint = (btHinge2Constraint*)meta->m_Constraint;
            PushBullet3DVector3(L, constraint->getAxis1(), 1.0f);
            PushBullet3DVector3(L, constraint->getAxis2(), 1.0f);
        }
        return 2;
    }

    static int Constraint_GetJointAngles(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        if (!IsWorldAxisJoint(meta->m_Kind))
        {
            return luaL_error(L, "Expected a universal or hinge2 constraint.");
        }
        ((btGeneric6DofConstraint*)meta->m_Constraint)->calculateTransforms();
        if (meta->m_Kind == BULLET3D_CONSTRAINT_UNIVERSAL)
        {
            btUniversalConstraint* constraint = (btUniversalConstraint*)meta->m_Constraint;
            lua_pushnumber(L, constraint->getAngle1());
            lua_pushnumber(L, constraint->getAngle2());
        }
        else
        {
            btHinge2Constraint* constraint = (btHinge2Constraint*)meta->m_Constraint;
            lua_pushnumber(L, constraint->getAngle1());
            lua_pushnumber(L, constraint->getAngle2());
        }
        return 2;
    }

    static int Constraint_tostring(lua_State* L)
    {
        Bullet3DConstraintMeta* meta = CheckConstraintMeta(L, 1);
        lua_pushfstring(L, "Bullet3D.%s(%s) = %p", BULLET3D_TYPE_NAME_CONSTRAINT, ConstraintKindName(meta->m_Kind), meta->m_Constraint);
        return 1;
    }

    static int Constraint_eq(lua_State* L)
    {
        Bullet3DLuaConstraint* a = ToConstraintInternal(L, 1);
        Bullet3DLuaConstraint* b = ToConstraintInternal(L, 2);
        lua_pushboolean(L, a && b && a->m_Id == b->m_Id);
        return 1;
    }

    static void CollectConstraintIdsForWorld(btDiscreteDynamicsWorld* world, dmArray<uint64_t>* ids)
    {
        dmHashTable64<Bullet3DConstraintMeta>::Iterator iter = g_Bullet3DConstraints.GetIterator();
        while (iter.Next())
        {
            if (iter.GetValue().m_World == world)
            {
                ArrayPush(ids, iter.GetKey());
            }
        }
    }

    static void CollectConstraintIdsForBody(btCollisionObject* object, dmArray<uint64_t>* ids)
    {
        dmHashTable64<Bullet3DConstraintMeta>::Iterator iter = g_Bullet3DConstraints.GetIterator();
        while (iter.Next())
        {
            const Bullet3DConstraintMeta& meta = iter.GetValue();
            if (meta.m_BodyA == object || meta.m_BodyB == object)
            {
                ArrayPush(ids, iter.GetKey());
            }
        }
    }

    void ScriptBullet3DInvalidateConstraintsForWorld(void* world_ptr)
    {
        btDiscreteDynamicsWorld* world = (btDiscreteDynamicsWorld*)world_ptr;
        dmArray<uint64_t>        ids;
        CollectConstraintIdsForWorld(world, &ids);
        for (uint32_t i = 0; i < ids.Size(); ++i)
        {
            DestroyConstraintId(ids[i]);
        }

        dmArray<uint64_t>                          body_keys;
        dmHashTable64<Bullet3DBodyState>::Iterator state_iter = g_Bullet3DBodyStates.GetIterator();
        while (state_iter.Next())
        {
            if (state_iter.GetValue().m_World == world)
            {
                ArrayPush(&body_keys, state_iter.GetKey());
            }
        }
        for (uint32_t i = 0; i < body_keys.Size(); ++i)
        {
            g_Bullet3DBodyStates.Erase(body_keys[i]);
        }
    }

    void ScriptBullet3DInvalidateConstraintsForCollisionObject(void* collision_object_ptr)
    {
        btCollisionObject* collision_object = (btCollisionObject*)collision_object_ptr;
        dmArray<uint64_t>  ids;
        CollectConstraintIdsForBody(collision_object, &ids);
        for (uint32_t i = 0; i < ids.Size(); ++i)
        {
            DestroyConstraintId(ids[i]);
        }
        if (g_Bullet3DBodyStates.Get(PtrToKey(collision_object)))
        {
            g_Bullet3DBodyStates.Erase(PtrToKey(collision_object));
        }
    }

    void ScriptBullet3DSetCollisionObjectEnabled(void* world_ptr, void* collision_object_ptr, bool enabled)
    {
        btDiscreteDynamicsWorld* world = (btDiscreteDynamicsWorld*)world_ptr;
        btCollisionObject*       collision_object = (btCollisionObject*)collision_object_ptr;
        if (!world || !collision_object)
        {
            return;
        }

        EnsureBodyStateCapacity();
        Bullet3DBodyState* existing_state = g_Bullet3DBodyStates.Get(PtrToKey(collision_object));
        if (existing_state)
        {
            existing_state->m_World = world;
            existing_state->m_Enabled = enabled;
        }
        else
        {
            Bullet3DBodyState state = { world, enabled };
            g_Bullet3DBodyStates.Put(PtrToKey(collision_object), state);
        }

        dmHashTable64<Bullet3DConstraintMeta>::Iterator iter = g_Bullet3DConstraints.GetIterator();
        while (iter.Next())
        {
            Bullet3DConstraintMeta* meta = g_Bullet3DConstraints.Get(iter.GetKey());
            if (meta->m_World == world && (meta->m_BodyA == collision_object || meta->m_BodyB == collision_object))
            {
                if (ShouldConstraintBeActive(meta))
                {
                    AddConstraintToWorld(meta);
                }
                else
                {
                    RemoveConstraintFromWorld(meta);
                }
            }
        }
    }

    static const luaL_reg Constraint_methods[] = {
        { 0, 0 }
    };

    static const luaL_reg Constraint_meta[] = {
        { "__tostring", Constraint_tostring },
        { "__eq", Constraint_eq },
        { 0, 0 }
    };

    static const luaL_reg Constraint_functions[] = {
        { "create_point_to_point", Constraint_CreatePointToPoint },
        { "create_hinge", Constraint_CreateHinge },
        { "create_cone_twist", Constraint_CreateConeTwist },
        { "create_generic_6dof", Constraint_CreateGeneric6Dof },
        { "create_generic_6dof_spring", Constraint_CreateGeneric6DofSpring },
        { "create_slider", Constraint_CreateSlider },
        { "create_universal", Constraint_CreateUniversal },
        { "create_hinge2", Constraint_CreateHinge2 },

        { "is_valid", Constraint_IsValid },
        { "is_active", Constraint_IsActive },
        { "destroy", Constraint_Destroy },
        { "get_type", Constraint_GetType },
        { "get_body_a", Constraint_GetBodyA },
        { "get_body_b", Constraint_GetBodyB },
        { "get_world", Constraint_GetWorld },
        { "get_collide_connected", Constraint_GetCollideConnected },

        { "get_pivots", Constraint_GetPivots },
        { "set_pivots", Constraint_SetPivots },

        { "get_frame_a", Constraint_GetFrameA },
        { "get_frame_b", Constraint_GetFrameB },
        { "set_frame_a", Constraint_SetFrameA },
        { "set_frame_b", Constraint_SetFrameB },
        { "get_angular_only", Constraint_GetAngularOnly },
        { "set_angular_only", Constraint_SetAngularOnly },

        { "get_hinge_angle", Constraint_GetHingeAngle },
        { "get_hinge_limits", Constraint_GetHingeLimits },
        { "set_hinge_limits", Constraint_SetHingeLimits },
        { "get_hinge_motor", Constraint_GetHingeMotor },
        { "set_hinge_motor", Constraint_SetHingeMotor },
        { "set_hinge_motor_target", Constraint_SetHingeMotorTarget },
        { "set_hinge_axis", Constraint_SetHingeAxis },

        { "get_cone_twist_limits", Constraint_GetConeTwistLimits },
        { "set_cone_twist_limits", Constraint_SetConeTwistLimits },
        { "get_twist_angle", Constraint_GetTwistAngle },
        { "is_past_swing_limit", Constraint_IsPastSwingLimit },
        { "enable_cone_twist_motor", Constraint_EnableConeTwistMotor },
        { "set_cone_twist_motor_target", Constraint_SetConeTwistMotorTarget },

        { "get_limit", Constraint_GetLimit },
        { "set_limit", Constraint_SetLimit },
        { "is_limited", Constraint_IsLimited },
        { "get_d6_axis", Constraint_Get6DofAxis },
        { "get_d6_angle", Constraint_Get6DofAngle },
        { "get_d6_position", Constraint_Get6DofPosition },
        { "get_d6_motor", Constraint_Get6DofMotor },
        { "set_d6_motor", Constraint_Set6DofMotor },

        { "enable_spring", Constraint_EnableSpring },
        { "set_spring_stiffness", Constraint_SetSpringStiffness },
        { "set_spring_damping", Constraint_SetSpringDamping },
        { "set_spring_equilibrium_point", Constraint_SetSpringEquilibriumPoint },

        { "get_slider_limits", Constraint_GetSliderLimits },
        { "set_slider_limits", Constraint_SetSliderLimits },
        { "get_slider_position", Constraint_GetSliderPosition },
        { "get_slider_motor", Constraint_GetSliderMotor },
        { "set_slider_motor", Constraint_SetSliderMotor },
        { "get_use_linear_reference_frame_a", Constraint_GetUseLinearReferenceFrameA },

        { "get_joint_anchors", Constraint_GetJointAnchors },
        { "get_joint_axes", Constraint_GetJointAxes },
        { "get_joint_angles", Constraint_GetJointAngles },
        { 0, 0 }
    };

    void ScriptBullet3DInitializeConstraint(lua_State* L)
    {
        TYPE_HASH_CONSTRAINT = dmScript::RegisterUserType(L, BULLET3D_TYPE_NAME_CONSTRAINT, Constraint_methods, Constraint_meta);

        lua_newtable(L);
        luaL_register(L, 0, Constraint_functions);
        lua_setfield(L, -2, "constraint");
    }

    void ScriptBullet3DFinalizeConstraint()
    {
        dmArray<uint64_t>                               ids;
        dmHashTable64<Bullet3DConstraintMeta>::Iterator iter = g_Bullet3DConstraints.GetIterator();
        while (iter.Next())
        {
            ArrayPush(&ids, iter.GetKey());
        }
        for (uint32_t i = 0; i < ids.Size(); ++i)
        {
            DestroyConstraintId(ids[i]);
        }

        TYPE_HASH_CONSTRAINT = 0;
        g_Bullet3DConstraints.Clear();
        g_Bullet3DBodyStates.Clear();
    }
} // namespace dmGameSystem

/*# Bullet constraint API
 *
 * Creates and controls Bullet constraints between Defold rigid bodies. A
 * constraint belongs to the supplied world and is destroyed automatically
 * with either body, with the world, or when the module is finalized. It is
 * temporarily removed from the native world while either linked body is
 * disabled and is restored when both bodies are enabled again. Dropping its
 * Lua userdata does not destroy the native constraint; call `destroy` for
 * early release.
 *
 * Creator positions and all other linear values use Defold units and are
 * converted with `physics.scale`. Angles are radians. Axes are one-based in
 * Lua: axes 1-3 are linear and axes 4-6 are angular. Mutating functions cannot
 * be called while the physics world is stepping.
 *
 * @document
 * @name bullet3d.constraint
 * @namespace bullet3d.constraint
 * @language Lua
 */

/*# Bullet typed constraint
 * @typedef
 * @name btTypedConstraint
 * @param value [type:userdata] opaque constraint handle
 */

/*# Create a point-to-point constraint
 *
 * `params.pivot_a` is required. `params.pivot_b` is required with `body_b`;
 * for a one-body constraint it is an optional world-space anchor. The params
 * table also accepts `collide_connected`, which defaults to `false`. The world
 * is derived from `body_a`; both bodies must belong to that same world.
 *
 * @name bullet3d.constraint.create_point_to_point
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody|nil] second body or world
 * @param params [type:table] pivots and options
 * @return constraint [type:btTypedConstraint] point-to-point constraint
 */

/*# Create a hinge constraint
 *
 * The params table requires `frame_a_position` and `frame_a_rotation`, plus
 * the corresponding frame B fields for a two-body constraint. It optionally
 * accepts `use_reference_frame_a`, `angular_only`, and
 * `collide_connected`. The world is derived from `body_a`.
 *
 * @name bullet3d.constraint.create_hinge
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody|nil] second body or world
 * @param params [type:table] local frames and options
 * @return constraint [type:btTypedConstraint] hinge constraint
 */

/*# Create a cone-twist constraint
 *
 * The params table uses the same local-frame fields as a hinge and optionally
 * accepts `angular_only` and `collide_connected`. The world is derived from
 * `body_a`.
 *
 * @name bullet3d.constraint.create_cone_twist
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody|nil] second body or world
 * @param params [type:table] local frames and options
 * @return constraint [type:btTypedConstraint] cone-twist constraint
 */

/*# Create a generic six-degree-of-freedom constraint
 *
 * The params table requires local frame A and, for a two-body constraint,
 * local frame B. It optionally accepts
 * `collide_connected`. The world is derived from `body_a`. Bullet 2.77's active 6-DOF
 * solver ignores its legacy linear-reference-frame selector, so that field is
 * rejected rather than silently accepted.
 *
 * @name bullet3d.constraint.create_generic_6dof
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody|nil] second body or world
 * @param params [type:table] local frames and options
 * @return constraint [type:btTypedConstraint] generic 6-DOF constraint
 */

/*# Create a generic spring six-degree-of-freedom constraint
 *
 * Both bodies and both local frames are required. The params table optionally
 * accepts `collide_connected`. The world is derived from `body_a`. Bullet 2.77's active
 * spring 6-DOF solver ignores its legacy linear-reference-frame selector, so
 * that field is rejected rather than silently accepted.
 *
 * @name bullet3d.constraint.create_generic_6dof_spring
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody] second body
 * @param params [type:table] local frames and options
 * @return constraint [type:btTypedConstraint] spring 6-DOF constraint
 */

/*# Create a slider constraint
 *
 * The params table requires local frame A and, for a two-body constraint,
 * local frame B. It optionally accepts `use_linear_reference_frame_a` and
 * `collide_connected`. The world is derived from `body_a`.
 *
 * @name bullet3d.constraint.create_slider
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody|nil] second body or world
 * @param params [type:table] local frames and options
 * @return constraint [type:btTypedConstraint] slider constraint
 */

/*# Create a universal constraint
 *
 * Both bodies are required. The params table requires a world-space `anchor`
 * and non-zero, orthogonal `axis1` and `axis2` vectors. It optionally accepts
 * `collide_connected`. The world is derived from `body_a`.
 *
 * @name bullet3d.constraint.create_universal
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody] second body
 * @param params [type:table] anchor, axes, and options
 * @return constraint [type:btTypedConstraint] universal constraint
 */

/*# Create a hinge2 constraint
 *
 * Both bodies are required. The params table requires a world-space `anchor`
 * and non-zero, orthogonal `axis1` and `axis2` vectors. Its initial linear
 * suspension travel is one Defold unit in either direction. It optionally
 * accepts `collide_connected`. The world is derived from `body_a`.
 *
 * @name bullet3d.constraint.create_hinge2
 * @param body_a [type:btRigidBody] first body
 * @param body_b [type:btRigidBody] second body
 * @param params [type:table] anchor, axes, and options
 * @return constraint [type:btTypedConstraint] hinge2 constraint
 */

/*# Test whether a constraint handle is valid
 * @name bullet3d.constraint.is_valid
 * @param constraint [type:btTypedConstraint] constraint handle
 * @return valid [type:boolean] true while the native constraint exists
 */

/*# Test whether a constraint is active in its world
 * @name bullet3d.constraint.is_active
 * @param constraint [type:btTypedConstraint] constraint
 * @return active [type:boolean] false while a linked body is disabled
 */

/*# Destroy a constraint
 * @name bullet3d.constraint.destroy
 * @param constraint [type:btTypedConstraint] constraint
 */

/*# Get the constraint type name
 * @name bullet3d.constraint.get_type
 * @param constraint [type:btTypedConstraint] constraint
 * @return type [type:string] registered constraint type
 */

/*# Get the first linked body
 * @name bullet3d.constraint.get_body_a
 * @param constraint [type:btTypedConstraint] constraint
 * @return body [type:btRigidBody] first body
 */

/*# Get the second linked body
 * @name bullet3d.constraint.get_body_b
 * @param constraint [type:btTypedConstraint] constraint
 * @return body [type:btRigidBody|nil] second body, or nil for a world constraint
 */

/*# Get the owning world
 * @name bullet3d.constraint.get_world
 * @param constraint [type:btTypedConstraint] constraint
 * @return world [type:btDiscreteDynamicsWorld] owning world
 */

/*# Get whether connected bodies can collide
 * @name bullet3d.constraint.get_collide_connected
 * @param constraint [type:btTypedConstraint] constraint
 * @return collide [type:boolean] whether connected bodies can collide
 */

/*# Get point-to-point pivots
 * @name bullet3d.constraint.get_pivots
 * @param constraint [type:btTypedConstraint] point-to-point constraint
 * @return pivot_a [type:vector3] local body-A pivot
 * @return pivot_b [type:vector3] local body-B pivot or world anchor
 */

/*# Set point-to-point pivots
 * @name bullet3d.constraint.set_pivots
 * @param constraint [type:btTypedConstraint] point-to-point constraint
 * @param pivot_a [type:vector3] local body-A pivot
 * @param pivot_b [type:vector3] local body-B pivot or world anchor
 */

/*# Get local frame A
 *
 * Returns position and rotation. For one-body generic 6-DOF and slider
 * constraints this is the user-body frame, despite Bullet storing it as its
 * native frame B.
 *
 * @name bullet3d.constraint.get_frame_a
 * @param constraint [type:btTypedConstraint] framed constraint
 * @return position [type:vector3] local position
 * @return rotation [type:quaternion] local rotation
 */

/*# Get local frame B
 * @name bullet3d.constraint.get_frame_b
 * @param constraint [type:btTypedConstraint] framed constraint
 * @return position [type:vector3] local position or world frame position
 * @return rotation [type:quaternion] local rotation or world frame rotation
 */

/*# Set local frame A
 * @name bullet3d.constraint.set_frame_a
 * @param constraint [type:btTypedConstraint] mutable framed constraint
 * @param position [type:vector3] local position
 * @param rotation [type:quaternion] local rotation
 */

/*# Set local frame B
 * @name bullet3d.constraint.set_frame_b
 * @param constraint [type:btTypedConstraint] mutable framed constraint
 * @param position [type:vector3] local position or world frame position
 * @param rotation [type:quaternion] local rotation or world frame rotation
 */

/*# Test angular-only mode
 * @name bullet3d.constraint.get_angular_only
 * @param constraint [type:btTypedConstraint] hinge or cone-twist constraint
 * @return angular_only [type:boolean] angular-only state
 */

/*# Set angular-only mode
 * @name bullet3d.constraint.set_angular_only
 * @param constraint [type:btTypedConstraint] hinge or cone-twist constraint
 * @param angular_only [type:boolean] angular-only state
 */

/*# Get the current hinge angle
 * @name bullet3d.constraint.get_hinge_angle
 * @param constraint [type:btTypedConstraint] hinge constraint
 * @return angle [type:number] angle in radians
 */

/*# Get hinge angular limits
 * @name bullet3d.constraint.get_hinge_limits
 * @param constraint [type:btTypedConstraint] hinge constraint
 * @return lower [type:number] lower angle in radians
 * @return upper [type:number] upper angle in radians
 */

/*# Set hinge angular limits
 * @name bullet3d.constraint.set_hinge_limits
 * @param constraint [type:btTypedConstraint] hinge constraint
 * @param lower [type:number] lower angle in radians
 * @param upper [type:number] upper angle in radians
 * @param bias [type:number|nil] optional limit bias from 0 to 1
 * @param relaxation [type:number|nil] optional relaxation from 0 to 1
 */

/*# Get hinge motor settings
 * @name bullet3d.constraint.get_hinge_motor
 * @param constraint [type:btTypedConstraint] hinge constraint
 * @return enabled [type:boolean] motor state
 * @return target_velocity [type:number] angular target velocity
 * @return max_impulse [type:number] maximum angular motor impulse
 */

/*# Set hinge motor settings
 * @name bullet3d.constraint.set_hinge_motor
 * @param constraint [type:btTypedConstraint] hinge constraint
 * @param enabled [type:boolean] motor state
 * @param target_velocity [type:number] angular target velocity
 * @param max_impulse [type:number] non-negative maximum angular motor impulse
 */

/*# Set a hinge motor angle target
 * @name bullet3d.constraint.set_hinge_motor_target
 * @param constraint [type:btTypedConstraint] hinge constraint
 * @param target_angle [type:number] target angle in radians
 * @param time_step [type:number] positive step duration in seconds
 */

/*# Set the one-body hinge axis
 *
 * This function only supports hinges attached to the world. For a two-body
 * hinge, change both local frames with `set_frame_a` and `set_frame_b`.
 *
 * @name bullet3d.constraint.set_hinge_axis
 * @param constraint [type:btTypedConstraint] one-body hinge constraint
 * @param axis [type:vector3] non-zero axis in body-A space
 */

/*# Get cone-twist angular spans
 * @name bullet3d.constraint.get_cone_twist_limits
 * @param constraint [type:btTypedConstraint] cone-twist constraint
 * @return swing_span_1 [type:number] first swing span in radians
 * @return swing_span_2 [type:number] second swing span in radians
 * @return twist_span [type:number] twist span in radians
 */

/*# Set cone-twist angular spans
 * @name bullet3d.constraint.set_cone_twist_limits
 * @param constraint [type:btTypedConstraint] cone-twist constraint
 * @param swing_span_1 [type:number] non-negative first swing span
 * @param swing_span_2 [type:number] non-negative second swing span
 * @param twist_span [type:number] non-negative twist span
 * @param softness [type:number|nil] optional softness from 0 to 1
 * @param bias [type:number|nil] optional bias from 0 to 1
 * @param relaxation [type:number|nil] optional relaxation from 0 to 1
 */

/*# Get the current cone-twist twist angle
 * @name bullet3d.constraint.get_twist_angle
 * @param constraint [type:btTypedConstraint] cone-twist constraint
 * @return angle [type:number] twist angle in radians
 */

/*# Test whether a cone-twist is past its swing limit
 * @name bullet3d.constraint.is_past_swing_limit
 * @param constraint [type:btTypedConstraint] cone-twist constraint
 * @return past_limit [type:boolean] swing-limit state
 */

/*# Enable or disable the cone-twist motor
 * @name bullet3d.constraint.enable_cone_twist_motor
 * @param constraint [type:btTypedConstraint] cone-twist constraint
 * @param enabled [type:boolean] motor state
 */

/*# Set the cone-twist motor target
 * @name bullet3d.constraint.set_cone_twist_motor_target
 * @param constraint [type:btTypedConstraint] cone-twist constraint
 * @param target [type:quaternion] target orientation
 * @param constraint_space [type:boolean|nil] target is already in constraint space
 */

/*# Get a 6-DOF axis limit
 *
 * Axes 1-3 return linear limits in Defold units. Axes 4-6 return angular
 * limits in radians.
 *
 * @name bullet3d.constraint.get_limit
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @return lower [type:number] lower limit
 * @return upper [type:number] upper limit
 */

/*# Set a 6-DOF axis limit
 * @name bullet3d.constraint.set_limit
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @param lower [type:number] lower limit
 * @param upper [type:number] upper limit
 */

/*# Test whether a 6-DOF axis is limited
 * @name bullet3d.constraint.is_limited
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @return limited [type:boolean] limit state
 */

/*# Get a current 6-DOF angular axis
 * @name bullet3d.constraint.get_d6_axis
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based angular-axis index from 1 to 3
 * @return direction [type:vector3] world-space unit axis
 */

/*# Get a current 6-DOF angle
 * @name bullet3d.constraint.get_d6_angle
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based angular-axis index from 1 to 3
 * @return angle [type:number] current angle in radians
 */

/*# Get a current 6-DOF linear position
 * @name bullet3d.constraint.get_d6_position
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based linear-axis index from 1 to 3
 * @return position [type:number] relative position in Defold units
 */

/*# Get 6-DOF motor settings
 *
 * Axes 1-3 are linear and axes 4-6 are angular. Bounce is zero for linear
 * motors because Bullet only implements it for angular motors.
 *
 * @name bullet3d.constraint.get_d6_motor
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @return enabled [type:boolean] motor state
 * @return target_velocity [type:number] target velocity
 * @return max_impulse [type:number] maximum motor impulse
 * @return bounce [type:number] angular bounce from 0 to 1
 */

/*# Set 6-DOF motor settings
 * @name bullet3d.constraint.set_d6_motor
 * @param constraint [type:btTypedConstraint] 6-DOF-derived constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @param enabled [type:boolean] motor state
 * @param target_velocity [type:number] target velocity
 * @param max_impulse [type:number] non-negative maximum motor impulse
 * @param bounce [type:number|nil] optional angular bounce from 0 to 1
 */

/*# Enable or disable a spring axis
 * @name bullet3d.constraint.enable_spring
 * @param constraint [type:btTypedConstraint] spring 6-DOF or hinge2 constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @param enabled [type:boolean] spring state
 */

/*# Set spring stiffness
 *
 * Stiffness is Bullet's solver tuning coefficient, not a force or torque
 * value, and is therefore independent of `physics.scale` for every axis.
 *
 * @name bullet3d.constraint.set_spring_stiffness
 * @param constraint [type:btTypedConstraint] spring 6-DOF or hinge2 constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @param stiffness [type:number] non-negative stiffness
 */

/*# Set spring damping
 * @name bullet3d.constraint.set_spring_damping
 * @param constraint [type:btTypedConstraint] spring 6-DOF or hinge2 constraint
 * @param axis [type:number] one-based axis from 1 to 6
 * @param damping [type:number] damping from 0 to 1
 */

/*# Set spring equilibrium points
 *
 * With no axis, captures all current transforms. With an axis and no value,
 * captures that axis. Linear values use Defold units and angular values use
 * radians.
 *
 * @name bullet3d.constraint.set_spring_equilibrium_point
 * @param constraint [type:btTypedConstraint] spring 6-DOF or hinge2 constraint
 * @param axis [type:number|nil] optional one-based axis from 1 to 6
 * @param value [type:number|nil] optional explicit equilibrium value
 */

/*# Get slider limits
 * @name bullet3d.constraint.get_slider_limits
 * @param constraint [type:btTypedConstraint] slider constraint
 * @return lower_linear [type:number] lower linear limit in Defold units
 * @return upper_linear [type:number] upper linear limit in Defold units
 * @return lower_angular [type:number] lower angular limit in radians
 * @return upper_angular [type:number] upper angular limit in radians
 */

/*# Set slider limits
 * @name bullet3d.constraint.set_slider_limits
 * @param constraint [type:btTypedConstraint] slider constraint
 * @param lower_linear [type:number] lower linear limit in Defold units
 * @param upper_linear [type:number] upper linear limit in Defold units
 * @param lower_angular [type:number] lower angular limit in radians
 * @param upper_angular [type:number] upper angular limit in radians
 */

/*# Get the current slider position
 * @name bullet3d.constraint.get_slider_position
 * @param constraint [type:btTypedConstraint] slider constraint
 * @return position [type:number] current linear position in Defold units
 */

/*# Get slider motor settings
 * @name bullet3d.constraint.get_slider_motor
 * @param constraint [type:btTypedConstraint] slider constraint
 * @param motor [type:string] `linear` or `angular`
 * @return enabled [type:boolean] motor state
 * @return target_velocity [type:number] target velocity
 * @return max_force [type:number] maximum motor force
 */

/*# Set slider motor settings
 * @name bullet3d.constraint.set_slider_motor
 * @param constraint [type:btTypedConstraint] slider constraint
 * @param motor [type:string] `linear` or `angular`
 * @param enabled [type:boolean] motor state
 * @param target_velocity [type:number] target velocity
 * @param max_force [type:number] non-negative maximum motor force
 */

/*# Get the slider linear reference-frame choice
 * @name bullet3d.constraint.get_use_linear_reference_frame_a
 * @param constraint [type:btTypedConstraint] slider constraint
 * @return use_frame_a [type:boolean] true when linear calculations reference frame A
 */

/*# Get universal or hinge2 anchors
 * @name bullet3d.constraint.get_joint_anchors
 * @param constraint [type:btTypedConstraint] universal or hinge2 constraint
 * @return anchor_a [type:vector3] world-space anchor on body A
 * @return anchor_b [type:vector3] world-space anchor on body B
 */

/*# Get universal or hinge2 axes
 * @name bullet3d.constraint.get_joint_axes
 * @param constraint [type:btTypedConstraint] universal or hinge2 constraint
 * @return axis_1 [type:vector3] first world-space unit axis
 * @return axis_2 [type:vector3] second world-space unit axis
 */

/*# Get universal or hinge2 angles
 * @name bullet3d.constraint.get_joint_angles
 * @param constraint [type:btTypedConstraint] universal or hinge2 constraint
 * @return angle_1 [type:number] first angle in radians
 * @return angle_2 [type:number] second angle in radians
 */
