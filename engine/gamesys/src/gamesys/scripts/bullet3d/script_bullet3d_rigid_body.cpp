// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include <math.h>

#include <gameobject/gameobject.h>
#include <script/script.h>

#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "gamesys_private.h"
#include "script_bullet3d.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

//////////////////////////////////////////////////////////////////////////////
// btRigidBody
namespace dmGameSystem
{
    btRigidBody* CheckBullet3DRigidBody(lua_State* L, int index)
    {
        btCollisionObject* collision_object = CheckBullet3DCollisionObject(L, index);
        btRigidBody*       rigid_body = btRigidBody::upcast(collision_object);
        if (!rigid_body)
        {
            luaL_error(L, "Expected a Bullet rigid body; the collision object is a ghost trigger or another non-rigid object.");
            return 0;
        }
        return rigid_body;
    }

    static void Activate(btRigidBody* rigid_body)
    {
        rigid_body->activate();
    }

    static int RigidBody_IsValid(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btCollisionObject* collision_object = ToBullet3DCollisionObject(L, 1);
        lua_pushboolean(L, collision_object && btRigidBody::upcast(collision_object));
        return 1;
    }

    static int RigidBody_GetWorld(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        CheckBullet3DRigidBody(L, 1);

        dmGameObject::HCollection     collection = GetBullet3DCollisionObjectCollection(L, 1);
        uint32_t                      component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        dmGameObject::HComponentWorld component_world = dmGameObject::GetWorld(collection, component_type_index);
        PushBullet3DWorld(L, component_world ? CompCollisionObjectGetBullet3DWorld(component_world) : 0, component_world);
        return 1;
    }

    static int RigidBody_GetMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btScalar inverse_mass = CheckBullet3DRigidBody(L, 1)->getInvMass();
        lua_pushnumber(L, inverse_mass == btScalar(0.0) ? 0.0 : btScalar(1.0) / inverse_mass);
        return 1;
    }

    static int RigidBody_GetInverseMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckBullet3DRigidBody(L, 1)->getInvMass());
        return 1;
    }

    static btScalar CheckPositiveMass(lua_State* L, int index)
    {
        btScalar mass = CheckBullet3DScalar(L, index, 1.0f, "mass");
        if (!(mass > btScalar(0.0)))
        {
            luaL_error(L, "mass must be finite, greater than zero, and have a finite native inverse.");
            return btScalar(0.0);
        }
        btScalar inverse_mass = btScalar(1.0) / mass;
        if (!isfinite((double)inverse_mass))
        {
            luaL_error(L, "mass must be finite, greater than zero, and have a finite native inverse.");
            return btScalar(0.0);
        }
        return mass;
    }

    static void CheckLocalInertiaValue(lua_State* L, const btVector3& inertia)
    {
        if (!isfinite((double)inertia.getX()) || !isfinite((double)inertia.getY()) || !isfinite((double)inertia.getZ()) ||
            inertia.getX() < btScalar(0.0) || inertia.getY() < btScalar(0.0) || inertia.getZ() < btScalar(0.0) ||
            (inertia.getX() != btScalar(0.0) && !isfinite((double)(btScalar(1.0) / inertia.getX()))) ||
            (inertia.getY() != btScalar(0.0) && !isfinite((double)(btScalar(1.0) / inertia.getY()))) ||
            (inertia.getZ() != btScalar(0.0) && !isfinite((double)(btScalar(1.0) / inertia.getZ()))))
        {
            luaL_error(L, "local_inertia components must be finite, non-negative, and have finite native inverses when nonzero.");
        }
    }

    static btVector3 CheckLocalInertia(lua_State* L, int index)
    {
        float     scale = GetBullet3DPhysicsScale();
        btVector3 unscaled_inertia = CheckBullet3DVector3(L, index, 1.0f, "local_inertia");
        btVector3 inertia = unscaled_inertia * (scale * scale);
        if ((unscaled_inertia.getX() != btScalar(0.0) && inertia.getX() == btScalar(0.0)) ||
            (unscaled_inertia.getY() != btScalar(0.0) && inertia.getY() == btScalar(0.0)) ||
            (unscaled_inertia.getZ() != btScalar(0.0) && inertia.getZ() == btScalar(0.0)))
        {
            luaL_error(L, "local_inertia components must be finite, non-negative, and have finite native inverses when nonzero.");
        }
        CheckLocalInertiaValue(L, inertia);
        return inertia;
    }

    static btRigidBody* CheckDynamicRigidBody(lua_State* L, int index)
    {
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, index);
        if (rigid_body->isStaticOrKinematicObject())
        {
            luaL_error(L, "Mass properties can only be changed on a dynamic rigid body.");
            return 0;
        }
        return rigid_body;
    }

    static void CheckMassMutationAllowed(lua_State* L, int index)
    {
        dmGameObject::HCollection collection = GetBullet3DCollisionObjectCollection(L, index);
        if (!collection)
        {
            luaL_error(L, "The bullet3d rigid body is not owned by a Defold collection.");
            return;
        }
        uint32_t                      component_type_index = dmGameObject::GetComponentTypeIndex(collection, COLLISION_OBJECT_EXT_HASH);
        dmGameObject::HComponentWorld component_world = dmGameObject::GetWorld(collection, component_type_index);
        if (!component_world)
        {
            luaL_error(L, "The bullet3d rigid body's world no longer exists.");
            return;
        }
        if (CompCollisionObjectIsBullet3DWorldLocked(component_world))
        {
            luaL_error(L, "Cannot change mass properties while the bullet3d world is stepping.");
        }
    }

    static void SetMassProperties(btRigidBody* rigid_body, btScalar mass, const btVector3& local_inertia)
    {
        rigid_body->setMassProps(mass, local_inertia);
        rigid_body->updateInertiaTensor();
        rigid_body->activate(true);
    }

    static int RigidBody_GetLocalInertia(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        const btVector3& inverse_inertia = CheckBullet3DRigidBody(L, 1)->getInvInertiaDiagLocal();
        btVector3        local_inertia(inverse_inertia.getX() == btScalar(0.0) ? btScalar(0.0) : btScalar(1.0) / inverse_inertia.getX(),
                                inverse_inertia.getY() == btScalar(0.0) ? btScalar(0.0) : btScalar(1.0) / inverse_inertia.getY(),
                                inverse_inertia.getZ() == btScalar(0.0) ? btScalar(0.0) : btScalar(1.0) / inverse_inertia.getZ());
        float inv_scale = GetBullet3DInvPhysicsScale();
        PushBullet3DVector3(L, local_inertia, inv_scale * inv_scale);
        return 1;
    }

    static int RigidBody_SetMass(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckDynamicRigidBody(L, 1);
        btScalar     mass = CheckPositiveMass(L, 2);
        CheckMassMutationAllowed(L, 1);
        btVector3 local_inertia(0.0f, 0.0f, 0.0f);
        rigid_body->getCollisionShape()->calculateLocalInertia(mass, local_inertia);
        CheckLocalInertiaValue(L, local_inertia);
        SetMassProperties(rigid_body, mass, local_inertia);
        return 0;
    }

    static int RigidBody_SetMassProperties(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckDynamicRigidBody(L, 1);
        btScalar     mass = CheckPositiveMass(L, 2);
        btVector3    local_inertia = CheckLocalInertia(L, 3);
        CheckMassMutationAllowed(L, 1);
        SetMassProperties(rigid_body, mass, local_inertia);
        return 0;
    }

    static int RigidBody_GetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getLinearVelocity(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_SetLinearVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setLinearVelocity(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale(), "velocity"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_GetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getAngularVelocity(), 1.0f);
        return 1;
    }

    static int RigidBody_SetAngularVelocity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setAngularVelocity(CheckBullet3DVector3(L, 2, 1.0f, "velocity"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_GetDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        lua_pushnumber(L, rigid_body->getLinearDamping());
        lua_pushnumber(L, rigid_body->getAngularDamping());
        return 2;
    }

    static int RigidBody_SetDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        btScalar     linear = CheckBullet3DScalarInRange(L, 2, 1.0f, "linear", 0.0f, 1.0f);
        btScalar     angular = CheckBullet3DScalarInRange(L, 3, 1.0f, "angular", 0.0f, 1.0f);
        rigid_body->setDamping(linear, angular);
        return 0;
    }

    static int RigidBody_GetLinearDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckBullet3DRigidBody(L, 1)->getLinearDamping());
        return 1;
    }

    static int RigidBody_SetLinearDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setDamping(CheckBullet3DScalarInRange(L, 2, 1.0f, "damping", 0.0f, 1.0f), rigid_body->getAngularDamping());
        return 0;
    }

    static int RigidBody_GetAngularDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckBullet3DRigidBody(L, 1)->getAngularDamping());
        return 1;
    }

    static int RigidBody_SetAngularDamping(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setDamping(rigid_body->getLinearDamping(), CheckBullet3DScalarInRange(L, 2, 1.0f, "damping", 0.0f, 1.0f));
        return 0;
    }

    static int RigidBody_GetLinearFactor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getLinearFactor(), 1.0f);
        return 1;
    }

    static int RigidBody_SetLinearFactor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setLinearFactor(CheckBullet3DVector3(L, 2, 1.0f, "factor"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_GetAngularFactor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getAngularFactor(), 1.0f);
        return 1;
    }

    static int RigidBody_SetAngularFactor(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setAngularFactor(CheckBullet3DVector3(L, 2, 1.0f, "factor"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_GetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getGravity(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_SetGravity(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->setGravity(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale(), "gravity"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_GetFlags(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushinteger(L, CheckBullet3DRigidBody(L, 1)->getFlags());
        return 1;
    }

    static int RigidBody_SetFlags(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckBullet3DRigidBody(L, 1)->setFlags(luaL_checkinteger(L, 2));
        return 0;
    }

    static int RigidBody_HasFlag(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        int flag = luaL_checkinteger(L, 2);
        lua_pushboolean(L, (CheckBullet3DRigidBody(L, 1)->getFlags() & flag) == flag);
        return 1;
    }

    static int RigidBody_GetLinearSleepingThreshold(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckBullet3DRigidBody(L, 1)->getLinearSleepingThreshold() * GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_GetAngularSleepingThreshold(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        lua_pushnumber(L, CheckBullet3DRigidBody(L, 1)->getAngularSleepingThreshold());
        return 1;
    }

    static int RigidBody_SetSleepingThresholds(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        btScalar     linear = CheckBullet3DNonNegativeScalar(L, 2, GetBullet3DPhysicsScale(), "linear");
        btScalar     angular = CheckBullet3DNonNegativeScalar(L, 3, 1.0f, "angular");
        rigid_body->setSleepingThresholds(linear, angular);
        return 0;
    }

    static int RigidBody_GetTotalForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getTotalForce(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_GetTotalTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        float inv_scale = GetBullet3DInvPhysicsScale();
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getTotalTorque(), inv_scale * inv_scale);
        return 1;
    }

    static int RigidBody_GetCenterOfMassPosition(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        PushBullet3DVector3(L, CheckBullet3DRigidBody(L, 1)->getCenterOfMassPosition(), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_ApplyCentralForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->applyCentralForce(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale(), "force"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        btVector3    force = CheckBullet3DVector3(L, 2, scale, "force");
        btVector3    relative_position = CheckBullet3DVector3(L, 3, scale, "relative_position");
        rigid_body->applyForce(force, relative_position);
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        rigid_body->applyTorque(CheckBullet3DVector3(L, 2, scale * scale, "torque"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyCentralImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->applyCentralImpulse(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale(), "impulse"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        btVector3    impulse = CheckBullet3DVector3(L, 2, scale, "impulse");
        btVector3    relative_position = CheckBullet3DVector3(L, 3, scale, "relative_position");
        rigid_body->applyImpulse(impulse, relative_position);
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyTorqueImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        rigid_body->applyTorqueImpulse(CheckBullet3DVector3(L, 2, scale * scale, "impulse"));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ClearForces(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        CheckBullet3DRigidBody(L, 1)->clearForces();
        return 0;
    }

    static int RigidBody_GetVelocityInLocalPoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        btVector3    relative_position = CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale(), "relative_position");
        PushBullet3DVector3(L, rigid_body->getVelocityInLocalPoint(relative_position), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_GetAABB(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        btVector3 lower;
        btVector3 upper;
        CheckBullet3DRigidBody(L, 1)->getAabb(lower, upper);

        lua_newtable(L);
        PushBullet3DVector3(L, lower, GetBullet3DInvPhysicsScale());
        lua_setfield(L, -2, "lower");
        PushBullet3DVector3(L, upper, GetBullet3DInvPhysicsScale());
        lua_setfield(L, -2, "upper");
        return 1;
    }

    static const luaL_reg RigidBody_functions[] = {
        { "is_valid", RigidBody_IsValid },
        { "get_world", RigidBody_GetWorld },

        { "get_mass", RigidBody_GetMass },
        { "get_inverse_mass", RigidBody_GetInverseMass },
        { "set_mass", RigidBody_SetMass },
        { "get_local_inertia", RigidBody_GetLocalInertia },
        { "set_mass_properties", RigidBody_SetMassProperties },

        { "get_linear_velocity", RigidBody_GetLinearVelocity },
        { "set_linear_velocity", RigidBody_SetLinearVelocity },

        { "get_angular_velocity", RigidBody_GetAngularVelocity },
        { "set_angular_velocity", RigidBody_SetAngularVelocity },

        { "get_damping", RigidBody_GetDamping },
        { "set_damping", RigidBody_SetDamping },

        { "get_linear_damping", RigidBody_GetLinearDamping },
        { "set_linear_damping", RigidBody_SetLinearDamping },

        { "get_angular_damping", RigidBody_GetAngularDamping },
        { "set_angular_damping", RigidBody_SetAngularDamping },

        { "get_linear_factor", RigidBody_GetLinearFactor },
        { "set_linear_factor", RigidBody_SetLinearFactor },

        { "get_angular_factor", RigidBody_GetAngularFactor },
        { "set_angular_factor", RigidBody_SetAngularFactor },

        { "get_gravity", RigidBody_GetGravity },
        { "set_gravity", RigidBody_SetGravity },

        { "get_flags", RigidBody_GetFlags },
        { "set_flags", RigidBody_SetFlags },
        { "has_flag", RigidBody_HasFlag },

        { "get_linear_sleeping_threshold", RigidBody_GetLinearSleepingThreshold },
        { "get_angular_sleeping_threshold", RigidBody_GetAngularSleepingThreshold },
        { "set_sleeping_thresholds", RigidBody_SetSleepingThresholds },

        { "get_total_force", RigidBody_GetTotalForce },
        { "get_total_torque", RigidBody_GetTotalTorque },
        { "get_center_of_mass_position", RigidBody_GetCenterOfMassPosition },

        { "apply_central_force", RigidBody_ApplyCentralForce },
        { "apply_force", RigidBody_ApplyForce },
        { "apply_torque", RigidBody_ApplyTorque },

        { "apply_central_impulse", RigidBody_ApplyCentralImpulse },
        { "apply_impulse", RigidBody_ApplyImpulse },
        { "apply_torque_impulse", RigidBody_ApplyTorqueImpulse },

        { "clear_forces", RigidBody_ClearForces },
        { "get_velocity_in_local_point", RigidBody_GetVelocityInLocalPoint },
        { "get_aabb", RigidBody_GetAABB },
        { 0, 0 }
    };

    void ScriptBullet3DInitializeRigidBody(lua_State* L)
    {
        lua_newtable(L);
        luaL_register(L, 0, RigidBody_functions);
        lua_pushinteger(L, BT_DISABLE_WORLD_GRAVITY);
        lua_setfield(L, -2, "BT_DISABLE_WORLD_GRAVITY");
        lua_setfield(L, -2, "rigid_body");
    }

} // namespace dmGameSystem

/*# Bullet rigid body API
 *
 * Rigid body functions accept the collision object userdata returned by
 * `bullet3d.get_rigid_body()`. Passing a trigger ghost object raises an error.
 * Defold retains ownership of the collision shape, motion state, world
 * membership, and native user pointer. The shape's logical children can be
 * mutated through `bullet3d.shape`; shared resource shapes become per-instance
 * copies on first mutation. Mass and local inertia can be changed for dynamic
 * bodies without changing their Defold collision-object type.
 *
 * Linear quantities use Defold units. Angular velocity, damping, and factors
 * are unscaled. Torque and angular impulse use squared physics scale because
 * inertia scales with length squared. Floating-point and vector inputs must be
 * finite. Damping must be in `[0, 1]`, and sleeping thresholds must be
 * non-negative.
 *
 * @document
 * @name bullet3d.rigid_body
 * @namespace bullet3d.rigid_body
 * @language Lua
 */

/*# Disable automatic world gravity for a rigid body
 *
 * Set this bit with `bullet3d.rigid_body.set_flags()` before assigning custom
 * body gravity that must survive later world-gravity changes or re-adding the
 * body to a world.
 *
 * @name bullet3d.rigid_body.BT_DISABLE_WORLD_GRAVITY
 * @constant
 */

/*# Test whether a handle refers to a valid rigid body
 * @name bullet3d.rigid_body.is_valid
 * @param body [type:btRigidBody] rigid body
 * @return valid [type:boolean] rigid body validity
 */

/*# Get the body's world
 * @name bullet3d.rigid_body.get_world
 * @param body [type:btRigidBody] rigid body
 * @return world [type:btDiscreteDynamicsWorld] owning world
 */

/*# Get mass
 * @name bullet3d.rigid_body.get_mass
 * @param body [type:btRigidBody] rigid body
 * @return mass [type:number] mass, or zero for an infinite-mass body
 */

/*# Get inverse mass
 * @name bullet3d.rigid_body.get_inverse_mass
 * @param body [type:btRigidBody] rigid body
 * @return inverse_mass [type:number] inverse mass
 */

/*# Set mass and calculate local inertia
 *
 * Recalculates local inertia from the body's current collision shape. Only a
 * dynamic body can be changed; zero mass cannot be used to convert it into a
 * static body. Values too small to have a finite native inverse are rejected.
 * The body is activated after the update.
 *
 * @name bullet3d.rigid_body.set_mass
 * @param body [type:btRigidBody] dynamic rigid body
 * @param mass [type:number] finite mass greater than zero
 */

/*# Get local inertia
 *
 * Returns the diagonal local inertia in Defold mass-times-distance-squared
 * units. A zero component denotes an axis with zero inverse inertia.
 *
 * @name bullet3d.rigid_body.get_local_inertia
 * @param body [type:btRigidBody] rigid body
 * @return inertia [type:vector3] diagonal local inertia
 */

/*# Set explicit mass properties
 *
 * Sets mass and diagonal local inertia together, updates the world-space
 * inertia tensor, and activates the body. Only dynamic bodies are accepted.
 * A zero inertia component is allowed and disables angular response on that
 * local axis; negative, non-finite, or nonzero values too small to have a
 * finite native inverse are rejected.
 *
 * @name bullet3d.rigid_body.set_mass_properties
 * @param body [type:btRigidBody] dynamic rigid body
 * @param mass [type:number] finite mass greater than zero
 * @param local_inertia [type:vector3] finite non-negative diagonal local inertia in Defold units
 */

/*# Get linear velocity
 * @name bullet3d.rigid_body.get_linear_velocity
 * @param body [type:btRigidBody] rigid body
 * @return velocity [type:vector3] velocity in Defold units per second
 */

/*# Set linear velocity
 * @name bullet3d.rigid_body.set_linear_velocity
 * @param body [type:btRigidBody] rigid body
 * @param velocity [type:vector3] finite velocity in Defold units per second
 */

/*# Get angular velocity
 * @name bullet3d.rigid_body.get_angular_velocity
 * @param body [type:btRigidBody] rigid body
 * @return velocity [type:vector3] angular velocity in radians per second
 */

/*# Set angular velocity
 * @name bullet3d.rigid_body.set_angular_velocity
 * @param body [type:btRigidBody] rigid body
 * @param velocity [type:vector3] finite angular velocity in radians per second
 */

/*# Get linear and angular damping
 * @name bullet3d.rigid_body.get_damping
 * @param body [type:btRigidBody] rigid body
 * @return linear [type:number] linear damping
 * @return angular [type:number] angular damping
 */

/*# Set linear and angular damping
 * @name bullet3d.rigid_body.set_damping
 * @param body [type:btRigidBody] rigid body
 * @param linear [type:number] finite linear damping in `[0, 1]`
 * @param angular [type:number] finite angular damping in `[0, 1]`
 */

/*# Get linear damping
 * @name bullet3d.rigid_body.get_linear_damping
 * @param body [type:btRigidBody] rigid body
 * @return damping [type:number] linear damping
 */

/*# Set linear damping
 * @name bullet3d.rigid_body.set_linear_damping
 * @param body [type:btRigidBody] rigid body
 * @param damping [type:number] finite linear damping in `[0, 1]`
 */

/*# Get angular damping
 * @name bullet3d.rigid_body.get_angular_damping
 * @param body [type:btRigidBody] rigid body
 * @return damping [type:number] angular damping
 */

/*# Set angular damping
 * @name bullet3d.rigid_body.set_angular_damping
 * @param body [type:btRigidBody] rigid body
 * @param damping [type:number] finite angular damping in `[0, 1]`
 */

/*# Get the linear factor
 * @name bullet3d.rigid_body.get_linear_factor
 * @param body [type:btRigidBody] rigid body
 * @return factor [type:vector3] per-axis linear factor
 */

/*# Set the linear factor
 * @name bullet3d.rigid_body.set_linear_factor
 * @param body [type:btRigidBody] rigid body
 * @param factor [type:vector3] per-axis linear factor
 */

/*# Get the angular factor
 * @name bullet3d.rigid_body.get_angular_factor
 * @param body [type:btRigidBody] rigid body
 * @return factor [type:vector3] per-axis angular factor
 */

/*# Set the angular factor
 * @name bullet3d.rigid_body.set_angular_factor
 * @param body [type:btRigidBody] rigid body
 * @param factor [type:vector3] per-axis angular factor
 */

/*# Get body gravity
 * @name bullet3d.rigid_body.get_gravity
 * @param body [type:btRigidBody] rigid body
 * @return gravity [type:vector3] gravity in Defold units per second squared
 */

/*# Set body gravity
 *
 * A later `bullet3d.world.set_gravity()` call, or removing and re-adding the
 * body to a world, can overwrite custom body gravity unless the body's
 * `BT_DISABLE_WORLD_GRAVITY` flag is set.
 *
 * @name bullet3d.rigid_body.set_gravity
 * @param body [type:btRigidBody] rigid body
 * @param gravity [type:vector3] gravity in Defold units per second squared
 */

/*# Get rigid body flags
 * @name bullet3d.rigid_body.get_flags
 * @param body [type:btRigidBody] rigid body
 * @return flags [type:number] rigid body flags
 */

/*# Set rigid body flags
 * @name bullet3d.rigid_body.set_flags
 * @param body [type:btRigidBody] rigid body
 * @param flags [type:number] rigid body flags
 */

/*# Test a rigid body flag
 * @name bullet3d.rigid_body.has_flag
 * @param body [type:btRigidBody] rigid body
 * @param flag [type:number] flag or mask
 * @return set [type:boolean] `true` when all requested flag bits are set
 */

/*# Get the linear sleeping threshold
 * @name bullet3d.rigid_body.get_linear_sleeping_threshold
 * @param body [type:btRigidBody] rigid body
 * @return threshold [type:number] threshold in Defold units per second
 */

/*# Get the angular sleeping threshold
 * @name bullet3d.rigid_body.get_angular_sleeping_threshold
 * @param body [type:btRigidBody] rigid body
 * @return threshold [type:number] threshold in radians per second
 */

/*# Set the sleeping thresholds
 * @name bullet3d.rigid_body.set_sleeping_thresholds
 * @param body [type:btRigidBody] rigid body
 * @param linear [type:number] finite non-negative linear threshold in Defold units per second
 * @param angular [type:number] finite non-negative angular threshold in radians per second
 */

/*# Get total accumulated force
 * @name bullet3d.rigid_body.get_total_force
 * @param body [type:btRigidBody] rigid body
 * @return force [type:vector3] accumulated force in Defold units
 */

/*# Get total accumulated torque
 * @name bullet3d.rigid_body.get_total_torque
 * @param body [type:btRigidBody] rigid body
 * @return torque [type:vector3] accumulated torque in Defold squared units
 */

/*# Get the center-of-mass world position
 * @name bullet3d.rigid_body.get_center_of_mass_position
 * @param body [type:btRigidBody] rigid body
 * @return position [type:vector3] center-of-mass position in Defold units
 */

/*# Apply a force at the center of mass
 * @name bullet3d.rigid_body.apply_central_force
 * @param body [type:btRigidBody] rigid body
 * @param force [type:vector3] force in Defold units
 */

/*# Apply a force at a relative position
 *
 * `relative_position` is an offset from the body's center of mass expressed in
 * world axes, not a world position or body-local coordinate.
 *
 * @name bullet3d.rigid_body.apply_force
 * @param body [type:btRigidBody] rigid body
 * @param force [type:vector3] force in Defold units
 * @param relative_position [type:vector3] center-of-mass-relative offset in world axes and Defold units
 */

/*# Apply torque
 * @name bullet3d.rigid_body.apply_torque
 * @param body [type:btRigidBody] rigid body
 * @param torque [type:vector3] torque in Defold squared units
 */

/*# Apply an impulse at the center of mass
 * @name bullet3d.rigid_body.apply_central_impulse
 * @param body [type:btRigidBody] rigid body
 * @param impulse [type:vector3] impulse in Defold units
 */

/*# Apply an impulse at a relative position
 *
 * `relative_position` is an offset from the body's center of mass expressed in
 * world axes, not a world position or body-local coordinate.
 *
 * @name bullet3d.rigid_body.apply_impulse
 * @param body [type:btRigidBody] rigid body
 * @param impulse [type:vector3] impulse in Defold units
 * @param relative_position [type:vector3] center-of-mass-relative offset in world axes and Defold units
 */

/*# Apply a torque impulse
 * @name bullet3d.rigid_body.apply_torque_impulse
 * @param body [type:btRigidBody] rigid body
 * @param impulse [type:vector3] angular impulse in Defold squared units
 */

/*# Clear accumulated force and torque
 * @name bullet3d.rigid_body.clear_forces
 * @param body [type:btRigidBody] rigid body
 */

/*# Get velocity at a center-of-mass-relative point
 *
 * The relative position is expressed in world axes. Despite Bullet's legacy
 * function name, it is not a body-local coordinate.
 *
 * @name bullet3d.rigid_body.get_velocity_in_local_point
 * @param body [type:btRigidBody] rigid body
 * @param relative_position [type:vector3] center-of-mass-relative offset in world axes and Defold units
 * @return velocity [type:vector3] point velocity in Defold units per second
 */

/*# Get the world-space AABB
 * @name bullet3d.rigid_body.get_aabb
 * @param body [type:btRigidBody] rigid body
 * @return aabb [type:table] table whose `lower` and `upper` fields are world-space vector3 bounds in Defold units
 */
