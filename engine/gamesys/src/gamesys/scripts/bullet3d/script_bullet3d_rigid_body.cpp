// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

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
        rigid_body->setLinearVelocity(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale()));
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
        rigid_body->setAngularVelocity(CheckBullet3DVector3(L, 2, 1.0f));
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
        CheckBullet3DRigidBody(L, 1)->setDamping(luaL_checknumber(L, 2), luaL_checknumber(L, 3));
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
        rigid_body->setDamping(luaL_checknumber(L, 2), rigid_body->getAngularDamping());
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
        rigid_body->setDamping(rigid_body->getLinearDamping(), luaL_checknumber(L, 2));
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
        rigid_body->setLinearFactor(CheckBullet3DVector3(L, 2, 1.0f));
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
        rigid_body->setAngularFactor(CheckBullet3DVector3(L, 2, 1.0f));
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
        rigid_body->setGravity(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale()));
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
        CheckBullet3DRigidBody(L, 1)->setSleepingThresholds(luaL_checknumber(L, 2) * GetBullet3DPhysicsScale(), luaL_checknumber(L, 3));
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
        rigid_body->applyCentralForce(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale()));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyForce(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        btVector3    force = CheckBullet3DVector3(L, 2, scale);
        btVector3    relative_position = CheckBullet3DVector3(L, 3, scale);
        rigid_body->applyForce(force, relative_position);
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyTorque(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        rigid_body->applyTorque(CheckBullet3DVector3(L, 2, scale * scale));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyCentralImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        rigid_body->applyCentralImpulse(CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale()));
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        btVector3    impulse = CheckBullet3DVector3(L, 2, scale);
        btVector3    relative_position = CheckBullet3DVector3(L, 3, scale);
        rigid_body->applyImpulse(impulse, relative_position);
        Activate(rigid_body);
        return 0;
    }

    static int RigidBody_ApplyTorqueImpulse(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        btRigidBody* rigid_body = CheckBullet3DRigidBody(L, 1);
        float        scale = GetBullet3DPhysicsScale();
        rigid_body->applyTorqueImpulse(CheckBullet3DVector3(L, 2, scale * scale));
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
        btVector3    relative_position = CheckBullet3DVector3(L, 2, GetBullet3DPhysicsScale());
        PushBullet3DVector3(L, rigid_body->getVelocityInLocalPoint(relative_position), GetBullet3DInvPhysicsScale());
        return 1;
    }

    static int RigidBody_GetAabb(lua_State* L)
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

    static const luaL_reg RIGID_BODY_FUNCTIONS[] = {
        { "is_valid", RigidBody_IsValid },
        { "get_world", RigidBody_GetWorld },
        { "get_mass", RigidBody_GetMass },
        { "get_inverse_mass", RigidBody_GetInverseMass },
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
        { "get_aabb", RigidBody_GetAabb },
        { 0, 0 }
    };

    void ScriptBullet3DInitializeRigidBody(lua_State* L)
    {
        lua_newtable(L);
        luaL_register(L, 0, RIGID_BODY_FUNCTIONS);
        lua_pushinteger(L, BT_DISABLE_WORLD_GRAVITY);
        lua_setfield(L, -2, "BT_DISABLE_WORLD_GRAVITY");
        lua_setfield(L, -2, "rigid_body");
    }

    void ScriptBullet3DFinalizeRigidBody()
    {
    }
} // namespace dmGameSystem

/*# Bullet rigid body API
 *
 * Rigid body functions accept the collision object userdata returned by
 * `bullet3d.get_rigid_body()`. Passing a trigger ghost object raises an error.
 * Defold retains ownership of mass properties, collision shape, motion state,
 * world membership, and the native user pointer.
 *
 * Linear quantities use Defold units. Angular velocity, damping, and factors
 * are unscaled. Torque and angular impulse use squared physics scale because
 * inertia scales with length squared.
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

/*# Get linear velocity
 * @name bullet3d.rigid_body.get_linear_velocity
 * @param body [type:btRigidBody] rigid body
 * @return velocity [type:vector3] velocity in Defold units per second
 */

/*# Set linear velocity
 * @name bullet3d.rigid_body.set_linear_velocity
 * @param body [type:btRigidBody] rigid body
 * @param velocity [type:vector3] velocity in Defold units per second
 */

/*# Get angular velocity
 * @name bullet3d.rigid_body.get_angular_velocity
 * @param body [type:btRigidBody] rigid body
 * @return velocity [type:vector3] angular velocity in radians per second
 */

/*# Set angular velocity
 * @name bullet3d.rigid_body.set_angular_velocity
 * @param body [type:btRigidBody] rigid body
 * @param velocity [type:vector3] angular velocity in radians per second
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
 * @param linear [type:number] linear damping
 * @param angular [type:number] angular damping
 */

/*# Get linear damping
 * @name bullet3d.rigid_body.get_linear_damping
 * @param body [type:btRigidBody] rigid body
 * @return damping [type:number] linear damping
 */

/*# Set linear damping
 * @name bullet3d.rigid_body.set_linear_damping
 * @param body [type:btRigidBody] rigid body
 * @param damping [type:number] linear damping
 */

/*# Get angular damping
 * @name bullet3d.rigid_body.get_angular_damping
 * @param body [type:btRigidBody] rigid body
 * @return damping [type:number] angular damping
 */

/*# Set angular damping
 * @name bullet3d.rigid_body.set_angular_damping
 * @param body [type:btRigidBody] rigid body
 * @param damping [type:number] angular damping
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
 * @param linear [type:number] linear threshold in Defold units per second
 * @param angular [type:number] angular threshold in radians per second
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
