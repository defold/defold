#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "physics_ddf.h"
#include "../gamesys_private.h"

#include "components/comp_collision_object.h"

#include "script_physics.h"
#include <physics/physics.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

static const char PHYSICS_CONTEXT_NAME[] = "__PhysicsContext";
static uint32_t PHYSICS_CONTEXT_HASH = 0;

namespace dmGameSystem
{
    /*# Collision object physics API documentation
     *
     * Functions and messages for collision object physics interaction
     * with other objects (collisions and ray-casting) and control of
     * physical behaviors.
     *
     * @document
     * @name Collision object
     * @namespace physics
     */

    struct PhysicsScriptContext
    {
        dmMessage::HSocket m_Socket;
        uint32_t m_ComponentIndex;
    };

    /*# [type:number] collision object mass
     *
     * [mark:READ ONLY] Returns the defined physical mass of the collision object component as a number.
     *
     * @name mass
     * @property
     *
     * @examples
     *
     * How to query a collision object component's mass:
     *
     * ```lua
     * -- get mass from collision object component "boulder"
     * local mass = go.get("#boulder", "mass")
     * -- do something useful
     * assert(mass > 1)
     * ```
     */

    /*# [type:vector3] collision object linear velocity
     *
     * [mark:READ ONLY] Returns the current linear velocity of the collision object component as a vector3.
     * The velocity is measured in units/s (pixels/s).
     *
     * @name linear_velocity
     * @replaces request_velocity and velocity_response
     * @property
     *
     * @examples
     *
     * How to query a collision object component's linear velocity:
     *
     * ```lua
     * -- get linear velocity from collision object "collisionobject" in gameobject "ship"
     * local source = "ship#collisionobject"
     * local velocity = go.get(source, "linear_velocity")
     * -- apply the velocity on target game object "boulder"'s collision object as a force
     * local target = "boulder#collisionobject"
     * local pos = go.get_position(target)
     * msg.post(target, "apply_force", { force = velocity, position = pos })
     * ```
     */

    /*# [type:vector3] collision object angular velocity
     *
     * [mark:READ ONLY] Returns the current angular velocity of the collision object component as a [type:vector3].
     * The velocity is measured as a rotation around the vector with a speed equivalent to the vector length
     * in radians/s.
     *
     * @name angular_velocity
     * @replaces request_velocity and velocity_response
     * @property
     * @examples
     *
     * How to query a collision object component's angular velocity:
     *
     * ```lua
     * -- get angular velocity from collision object "collisionobject" in gameobject "boulder"
     * -- this is a 2d game so rotation around z is the only one available.
     * local velocity = go.get("boulder#collisionobject", "angular_velocity.z")
     * -- do something interesting
     * if velocity < 0 then
     *     -- clockwise rotation
     *     ...
     * else
     *     -- counter clockwise rotation
     *     ...
     * end
     * ```
     */

    /*# [type:number] collision object linear damping
     *
     * The linear damping value for the collision object. Setting this value alters the damping of
     * linear motion of the object. Valid values are between 0 (no damping) and 1 (full damping).
     *
     * @name linear_damping
     * @property
     * @examples
     *
     * How to increase a collision object component's linear damping:
     *
     * ```lua
     * -- get linear damping from collision object "collisionobject" in gameobject "floater"
     * local target = "floater#collisionobject"
     * local damping = go.get(target, "linear_damping")
     * -- increase it by 10% if it's below 0.9
     * if damping <= 0.9 then
     *     go.set(target, "linear_damping", damping * 1.1)
     * end
     * ```
     */

    /*# [type:number] collision object angular damping
     *
     * The angular damping value for the collision object. Setting this value alters the damping of
     * angular motion of the object (rotation). Valid values are between 0 (no damping) and 1 (full damping).
     *
     * @name angular_damping
     * @property
     * @examples
     *
     * How to decrease a collision object component's angular damping:
     *
     * ```lua
     * -- get angular damping from collision object "collisionobject" in gameobject "floater"
     * local target = "floater#collisionobject"
     * local damping = go.get(target, "angular_damping")
     * -- decrease it by 10%
     * go.set(target, "angular_damping", damping * 0.9)
     * ```
     */

    /*# requests a ray cast to be performed
     *
     * Ray casts are used to test for intersections against collision objects in the physics world.
     * Collision objects of types kinematic, dynamic and static are tested against. Trigger objects
     * do not intersect with ray casts.
     * Which collision objects to hit is filtered by their collision groups and can be configured
     * through `groups`.
     * The actual ray cast will be performed during the physics-update.
     *
     * - If an object is hit, the result will be reported via a `ray_cast_response` message.
     * - If there is no object hit, the result will be reported via a `ray_cast_missed` message.
     *
     * @name physics.raycast_async
     * @param from [type:vector3] the world position of the start of the ray
     * @param to [type:vector3] the world position of the end of the ray
     * @param groups [type:table] a lua table containing the hashed groups for which to test collisions against
     * @param [request_id] [type:number] a number between [0,-255]. It will be sent back in the response for identification, 0 by default
     * @examples
     *
     * How to perform a ray cast asynchronously:
     *
     * ```lua
     * function init(self)
     *     self.my_groups = {hash("my_group1"), hash("my_group2")}
     * end
     *
     * function update(self, dt)
     *     -- request ray cast
     *     physics.raycast_async(my_start, my_end, self.my_groups)
     * end
     *
     * function on_message(self, message_id, message, sender)
     *     -- check for the response
     *     if message_id == hash("ray_cast_response") then
     *         -- act on the hit
     *     elseif message_id == hash("ray_cast_missed") then
     *         -- act on the miss
     *     end
     * end
     * ```
     */
    int Physics_RayCastAsync(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int top = lua_gettop(L);

        dmMessage::URL sender;
        if (!dmScript::GetURL(L, &sender)) {
            return luaL_error(L, "could not find a requesting instance for physics.raycast_async");
        }

        dmScript::GetGlobal(L, PHYSICS_CONTEXT_HASH);
        PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);
        void* world = dmGameObject::GetWorld(collection, context->m_ComponentIndex);

        Vectormath::Aos::Point3 from( *dmScript::CheckVector3(L, 1) );
        Vectormath::Aos::Point3 to( *dmScript::CheckVector3(L, 2) );

        uint32_t mask = 0;
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 3) != 0)
        {
            mask |= CompCollisionGetGroupBitIndex(world, dmScript::CheckHash(L, -1));
            lua_pop(L, 1);
        }

        int request_id = 0;
        if (top > 3)
        {
            request_id = luaL_checkinteger(L, 4);
            if (request_id < 0 || request_id > 255)
            {
                return luaL_error(L, "request_id must be between 0-255");
            }
        }

        dmPhysicsDDF::RequestRayCast request;
        request.m_From = from;
        request.m_To = to;
        request.m_Mask = mask;
        request.m_RequestId = request_id;

        dmMessage::URL receiver;
        dmMessage::ResetURL(receiver);
        receiver.m_Socket = context->m_Socket;
        dmMessage::Post(&sender, &receiver, dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_NameHash, (uintptr_t)sender_instance, (uintptr_t)dmPhysicsDDF::RequestRayCast::m_DDFDescriptor, &request, sizeof(dmPhysicsDDF::RequestRayCast), 0);
        return 0;
    }

    /*# requests a ray cast to be performed
     *
     * Ray casts are used to test for intersections against collision objects in the physics world.
     * Collision objects of types kinematic, dynamic and static are tested against. Trigger objects
     * do not intersect with ray casts.
     * Which collision objects to hit is filtered by their collision groups and can be configured
     * through `groups`.
     *
     * @name physics.raycast
     * @param from [type:vector3] the world position of the start of the ray
     * @param to [type:vector3] the world position of the end of the ray
     * @param groups [type:table] a lua table containing the hashed groups for which to test collisions against
     * @return result [type:table] It returns a table. If missed it returns nil. See `ray_cast_response` for details on the returned values.
     * @examples
     *
     * How to perform a ray cast synchronously:
     *
     * ```lua
     * function init(self)
     *     self.my_groups = {hash("my_group1"), hash("my_group2")}
     * end
     *
     * function update(self, dt)
     *     -- request ray cast
     *     local result = physics.raycast(my_start, my_end, self.my_groups)
     *     if result ~= nil then
     *         -- act on the hit (see 'ray_cast_response')
     *     end
     * end
     * ```
     */
    int Physics_RayCast(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmMessage::URL sender;
        if (!dmScript::GetURL(L, &sender)) {
            return luaL_error(L, "could not find a requesting instance for physics.raycast");
        }

        dmScript::GetGlobal(L, PHYSICS_CONTEXT_HASH);
        PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);
        void* world = dmGameObject::GetWorld(collection, context->m_ComponentIndex);

        Vectormath::Aos::Point3 from( *dmScript::CheckVector3(L, 1) );
        Vectormath::Aos::Point3 to( *dmScript::CheckVector3(L, 2) );

        uint32_t mask = 0;
        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 3) != 0)
        {
            mask |= CompCollisionGetGroupBitIndex(world, dmScript::CheckHash(L, -1));
            lua_pop(L, 1);
        }

        dmPhysics::RayCastRequest request;
        dmPhysics::RayCastResponse response;
        request.m_From = from;
        request.m_To = to;
        request.m_Mask = mask;
        dmGameSystem::RayCast(world, request, response);

        if (response.m_Hit) {
            lua_newtable(L);
            lua_pushnumber(L, response.m_Fraction);
            lua_setfield(L, -2, "fraction");
            dmScript::PushVector3(L, Vectormath::Aos::Vector3(response.m_Position));
            lua_setfield(L, -2, "position");
            dmScript::PushVector3(L, response.m_Normal);
            lua_setfield(L, -2, "normal");

            dmhash_t group = dmGameSystem::GetLSBGroupHash(world, response.m_CollisionObjectGroup);
            dmScript::PushHash(L, group);
            lua_setfield(L, -2, "group");

            dmhash_t id = dmGameSystem::CompCollisionObjectGetIdentifier(response.m_CollisionObjectUserData);
            dmScript::PushHash(L, id);
            lua_setfield(L, -2, "id");
        } else {
            lua_pushnil(L);
        }
        return 1;
    }

    // Matches JointResult in physics.h
    static const char* PhysicsResultString[] = {
        "result ok",
        "not supported",
        "a joint with that id already exist",
        "joint id not found",
        "joint not connected",
        "unknown error",
    };

    static int GetCollisionObject(lua_State* L, dmGameObject::HCollection collection, dmMessage::URL url, void** comp, void** comp_world)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, url.m_Path);

        lua_getglobal(L, PHYSICS_CONTEXT_NAME);
        PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        uint32_t type_index;
        *comp = dmGameObject::GetComponentFromInstance(instance, url.m_Fragment, &type_index);

        // Now check that the component is a collision object
        if (type_index != context->m_ComponentIndex) {
            return DM_LUA_ERROR("url %s.%s is not a collision object", dmHashReverseSafe64(url.m_Path), dmHashReverseSafe64(url.m_Fragment));
        }

        *comp_world = dmGameObject::GetWorld(collection, context->m_ComponentIndex);
        if (!CompCollisionIs2D(*comp_world)) {
            return DM_LUA_ERROR("joints are currently only supported with 2D physics");
        }

        return 0;
    }

    static int Physics_CreateJoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL url;
        dmScript::ResolveURL(L, 1, &url, 0);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));

        void* comp = 0x0;
        void* comp_world = 0x0;
        GetCollisionObject(L, collection, url, &comp, &comp_world);

        dmPhysics::JointResult r = dmGameSystem::CreateJoint(comp_world, comp, joint_id);
        if (r != dmPhysics::RESULT_OK) {
            return DM_LUA_ERROR("could not create joint: %s (%d)", PhysicsResultString[r], r);
        }

        return 0;
    }

    static int UnpackFloatParam(lua_State* L, int table_index, const char* table_field, float& float_out)
    {
        DM_LUA_STACK_CHECK(L, 0);
        lua_getfield(L, table_index, table_field);
        int type = lua_type(L, -1);

        // return if the field was not found
        if (type == LUA_TNIL || type == LUA_TNONE) {
            lua_pop(L, 1);
            return 0;
        } else if (type != LUA_TNUMBER) {
            return DM_LUA_ERROR("physics.connect_joint property table field %s must be of number type.", table_field);
        }

        float_out = lua_tonumber(L, -1);
        lua_pop(L, 1);

        return 0;
    }

    static int UnpackVec3Param(lua_State* L, int table_index, const char* table_field, float float_out[3])
    {
        DM_LUA_STACK_CHECK(L, 0);
        lua_getfield(L, table_index, table_field);
        int type = lua_type(L, -1);
        bool vec3_type = dmScript::IsVector3(L, -1);

        // return if the field was not found
        if (type == LUA_TNIL || type == LUA_TNONE) {
            lua_pop(L, 1);
            return 0;
        } else if (!vec3_type) {
            return DM_LUA_ERROR("physics.connect_joint property table field %s must be of vmath.vector3 type.", table_field);
        }

        Vectormath::Aos::Vector3* v3 = dmScript::ToVector3(L, -1);
        float_out[0] = v3->getX();
        float_out[1] = v3->getY();
        float_out[2] = v3->getZ();
        lua_pop(L, 1);

        return 0;
    }

    static int UnpackBoolParam(lua_State* L, int table_index, const char* table_field, bool& bool_out)
    {
        DM_LUA_STACK_CHECK(L, 0);
        lua_getfield(L, table_index, table_field);
        int type = lua_type(L, -1);

        // return if the field was not found
        if (type == LUA_TNIL || type == LUA_TNONE) {
            lua_pop(L, 1);
            return 0;
        } else if (type != LUA_TBOOLEAN) {
            return DM_LUA_ERROR("physics.connect_joint property table field %s must be of bool type.", table_field);
        }

        bool_out = lua_toboolean(L, -1);
        lua_pop(L, 1);

        return 0;
    }

    static int UnpackConnectJointParams(lua_State* L, dmPhysics::JointType type, int table_index, dmPhysics::ConnectJointParams& params)
    {
        DM_LUA_STACK_CHECK(L, 0);

        // Fill with default values
        params = dmPhysics::ConnectJointParams(type);

        int table_index_type = lua_type(L, table_index);
        if (table_index_type == LUA_TNIL || table_index_type == LUA_TNONE) {
            // Early exit if table was nil (just returns default values from above).
            return 0;
        } else if (table_index_type != LUA_TTABLE) {
            return DM_LUA_ERROR("argument %d to physics.connect_joint must be either nil or table.", table_index);
        }

        // Common fields for all joints:
        UnpackBoolParam(L, table_index, "collide_connected", params.m_CollideConnected);

        switch (type)
        {
            case dmPhysics::JOINT_TYPE_SPRING:
                UnpackFloatParam(L, table_index, "length", params.m_SpringJointParams.m_Length);
                UnpackFloatParam(L, table_index, "frequency", params.m_SpringJointParams.m_FrequencyHz);
                UnpackFloatParam(L, table_index, "damping", params.m_SpringJointParams.m_DampingRatio);
                break;

            case dmPhysics::JOINT_TYPE_FIXED:
                UnpackFloatParam(L, table_index, "max_length", params.m_FixedJointParams.m_MaxLength);
                break;

            case dmPhysics::JOINT_TYPE_HINGE:
                UnpackFloatParam(L, table_index, "reference_angle", params.m_HingeJointParams.m_ReferenceAngle);
                UnpackFloatParam(L, table_index, "lower_angle", params.m_HingeJointParams.m_LowerAngle);
                UnpackFloatParam(L, table_index, "upper_angle", params.m_HingeJointParams.m_UpperAngle);
                UnpackFloatParam(L, table_index, "max_motor_torque", params.m_HingeJointParams.m_MaxMotorTorque);
                UnpackFloatParam(L, table_index, "motor_speed", params.m_HingeJointParams.m_MotorSpeed);
                UnpackBoolParam(L, table_index, "enable_limit", params.m_HingeJointParams.m_EnableLimit);
                UnpackBoolParam(L, table_index, "enable_motor", params.m_HingeJointParams.m_EnableMotor);
                break;

            case dmPhysics::JOINT_TYPE_SLIDER:
                UnpackVec3Param(L, table_index, "local_axis_a", params.m_SliderJointParams.m_LocalAxisA);
                UnpackFloatParam(L, table_index, "reference_angle", params.m_SliderJointParams.m_ReferenceAngle);
                UnpackBoolParam(L, table_index, "enable_limit", params.m_SliderJointParams.m_EnableLimit);
                UnpackFloatParam(L, table_index, "lower_translation", params.m_SliderJointParams.m_LowerTranslation);
                UnpackFloatParam(L, table_index, "upper_translation", params.m_SliderJointParams.m_UpperTranslation);
                UnpackBoolParam(L, table_index, "enable_motor", params.m_SliderJointParams.m_EnableMotor);
                UnpackFloatParam(L, table_index, "max_motor_force", params.m_SliderJointParams.m_MaxMotorForce);
                UnpackFloatParam(L, table_index, "motor_speed", params.m_SliderJointParams.m_MotorSpeed);
                break;

            default:
                return DM_LUA_ERROR("property table not implemented for joint type %d", type);
        }

        return 0;
    }

    static int Physics_ConnectJoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        int top = lua_gettop(L);

        dmPhysics::JointType type = (dmPhysics::JointType)luaL_checkinteger(L, 1);
        if (type >= dmPhysics::JOINT_TYPE_COUNT)
        {
            return DM_LUA_ERROR("unknown joint type: %d", type);
        }

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 3);

        dmMessage::URL url_a;
        dmScript::ResolveURL(L, 2, &url_a, 0);
        dmMessage::URL url_b;
        dmScript::ResolveURL(L, 5, &url_b, 0);
        Vectormath::Aos::Point3 pos_a = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 4));
        Vectormath::Aos::Point3 pos_b = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 6));

        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));

        void* comp_a = 0x0;
        void* comp_world_a = 0x0;
        GetCollisionObject(L, collection, url_a, &comp_a, &comp_world_a);
        void* comp_b = 0x0;
        void* comp_world_b = 0x0;
        GetCollisionObject(L, collection, url_b, &comp_b, &comp_world_b);

        if (comp_world_a != comp_world_b) {
            return DM_LUA_ERROR("joints can only be connected to collision objects within the same physics world");
        }

        // Unpack type specific joint connection paramaters
        dmPhysics::ConnectJointParams params(type);
        UnpackConnectJointParams(L, type, 7, params);
        dmPhysics::JointResult r = dmGameSystem::ConnectJoint(comp_world_a, comp_a, joint_id, pos_a, comp_b, pos_b, type, params);
        if (r != dmPhysics::RESULT_OK) {
            return DM_LUA_ERROR("could not connect joint: %s (%d)", PhysicsResultString[r], r);
        }

        return 0;

    }

    static int Physics_DisconnectJoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL url;
        dmScript::ResolveURL(L, 1, &url, 0);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));

        void* comp = 0x0;
        void* comp_world = 0x0;
        GetCollisionObject(L, collection, url, &comp, &comp_world);

        // Unpack type specific joint connection paramaters
        dmPhysics::JointResult r = dmGameSystem::DisconnectJoint(comp_world, comp, joint_id);
        if (r != dmPhysics::RESULT_OK) {
            return DM_LUA_ERROR("could not disconnect joint: %s (%d)", PhysicsResultString[r], r);
        }

        return 0;
    }

    static int Physics_GetJointProperties(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL url;
        dmScript::ResolveURL(L, 1, &url, 0);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));

        void* comp = 0x0;
        void* comp_world = 0x0;
        GetCollisionObject(L, collection, url, &comp, &comp_world);

        dmPhysics::JointType joint_type;
        dmPhysics::ConnectJointParams joint_params;
        dmPhysics::JointResult r = GetJointParams(comp_world, comp, joint_id, joint_type, joint_params);
        if (r != dmPhysics::RESULT_OK)
        {
            return DM_LUA_ERROR("unable to get joint properties for %s: %s (%d)", dmHashReverseSafe64(joint_id), PhysicsResultString[r], r);
        }

        lua_newtable(L);
        lua_pushboolean(L, joint_params.m_CollideConnected); lua_setfield(L, -2, "collide_connected");

        switch (joint_type)
        {
            case dmPhysics::JOINT_TYPE_SPRING:
                {
                    lua_pushnumber(L, joint_params.m_SpringJointParams.m_Length); lua_setfield(L, -2, "length");
                    lua_pushnumber(L, joint_params.m_SpringJointParams.m_FrequencyHz); lua_setfield(L, -2, "frequency");
                    lua_pushnumber(L, joint_params.m_SpringJointParams.m_DampingRatio); lua_setfield(L, -2, "damping");
                }
                break;
            case dmPhysics::JOINT_TYPE_FIXED:
                {
                    lua_pushnumber(L, joint_params.m_FixedJointParams.m_MaxLength); lua_setfield(L, -2, "max_length");
                }
                break;
            case dmPhysics::JOINT_TYPE_HINGE:
                {
                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_ReferenceAngle); lua_setfield(L, -2, "reference_angle");
                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_LowerAngle); lua_setfield(L, -2, "lower_angle");
                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_UpperAngle); lua_setfield(L, -2, "upper_angle");
                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_MaxMotorTorque); lua_setfield(L, -2, "max_motor_torque");
                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_MotorSpeed); lua_setfield(L, -2, "motor_speed");
                    lua_pushboolean(L, joint_params.m_HingeJointParams.m_EnableLimit); lua_setfield(L, -2, "enable_limit");
                    lua_pushboolean(L, joint_params.m_HingeJointParams.m_EnableMotor); lua_setfield(L, -2, "enable_motor");

                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_JointAngle); lua_setfield(L, -2, "joint_angle");
                    lua_pushnumber(L, joint_params.m_HingeJointParams.m_JointSpeed); lua_setfield(L, -2, "joint_speed");

                }
                break;
            case dmPhysics::JOINT_TYPE_SLIDER:
                {
                    Vectormath::Aos::Vector3 v(joint_params.m_SliderJointParams.m_LocalAxisA[0], joint_params.m_SliderJointParams.m_LocalAxisA[1], joint_params.m_SliderJointParams.m_LocalAxisA[2]);
                    dmScript::PushVector3(L, v);
                    lua_setfield(L, -2, "local_axis_a");
                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_ReferenceAngle); lua_setfield(L, -2, "reference_angle");
                    lua_pushboolean(L, joint_params.m_SliderJointParams.m_EnableLimit); lua_setfield(L, -2, "enable_limit");
                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_LowerTranslation); lua_setfield(L, -2, "lower_translation");
                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_UpperTranslation); lua_setfield(L, -2, "upper_translation");
                    lua_pushboolean(L, joint_params.m_SliderJointParams.m_EnableMotor); lua_setfield(L, -2, "enable_motor");
                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_MaxMotorForce); lua_setfield(L, -2, "max_motor_force");
                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_MotorSpeed); lua_setfield(L, -2, "motor_speed");

                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_JointTranslation); lua_setfield(L, -2, "joint_translation");
                    lua_pushnumber(L, joint_params.m_SliderJointParams.m_JointSpeed); lua_setfield(L, -2, "joint_speed");
                }
                break;
            default:
                return false;
        }

        return 1;
    }

    static int Phyics_UpdateJoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL url;
        dmScript::ResolveURL(L, 1, &url, 0);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);

        void* comp = 0x0;
        void* comp_world = 0x0;
        GetCollisionObject(L, collection, url, &comp, &comp_world);

        dmPhysics::JointType joint_type;
        dmPhysics::JointResult r = GetJointType(comp_world, comp, joint_id, joint_type);
        if (r != dmPhysics::RESULT_OK) {
            return DM_LUA_ERROR("unable to update joint, could not get joint type: %s (%d)", PhysicsResultString[r], r);
        }

        dmPhysics::ConnectJointParams joint_params(joint_type);
        UnpackConnectJointParams(L, joint_type, 3, joint_params);

        r = UpdateJoint(comp_world, comp, joint_id, joint_params);
        if (r != dmPhysics::RESULT_OK) {
            return DM_LUA_ERROR("unable to update joint properties: %s (%d)", PhysicsResultString[r], r);
        }

        return 0;
    }

    static const luaL_reg PHYSICS_FUNCTIONS[] =
    {
        {"ray_cast",        Physics_RayCastAsync}, // Deprecated
        {"raycast_async",   Physics_RayCastAsync},
        {"raycast",         Physics_RayCast},

        {"create_joint",    Physics_CreateJoint},
        {"connect_joint",   Physics_ConnectJoint},
        {"disconnect_joint", Physics_DisconnectJoint},
        {"get_joint_properties", Physics_GetJointProperties},
        {"update_joint",    Phyics_UpdateJoint},
        {0, 0}
    };

    void ScriptPhysicsRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "physics", PHYSICS_FUNCTIONS);

#define SETCONSTANT(name) \
    lua_pushnumber(L, (lua_Number) dmPhysics::name); \
    lua_setfield(L, -2, #name);\

        SETCONSTANT(JOINT_TYPE_SPRING)
        SETCONSTANT(JOINT_TYPE_FIXED)
        SETCONSTANT(JOINT_TYPE_HINGE)
        SETCONSTANT(JOINT_TYPE_SLIDER)

 #undef SETCONSTANT

        lua_pop(L, 1);

        bool result = true;

        PhysicsScriptContext* physics_context = new PhysicsScriptContext();
        dmMessage::Result socket_result = dmMessage::GetSocket(dmPhysics::PHYSICS_SOCKET_NAME, &physics_context->m_Socket);
        if (socket_result != dmMessage::RESULT_OK)
        {
            result = false;
        }
        dmResource::ResourceType co_resource_type;
        if (result)
        {
            dmResource::Result fact_result = dmResource::GetTypeFromExtension(context.m_Factory, COLLISION_OBJECT_EXT, &co_resource_type);
            if (fact_result != dmResource::RESULT_OK)
            {
                dmLogError("Unable to get resource type for '%s': %d.", COLLISION_OBJECT_EXT, fact_result);
                result = false;
            }
        }
        if (result)
        {
            dmGameObject::ComponentType* co_component_type = dmGameObject::FindComponentType(context.m_Register, co_resource_type, &physics_context->m_ComponentIndex);
            if (co_component_type == 0x0)
            {
                dmLogError("Could not find component type '%s'.", COLLISION_OBJECT_EXT);
                result = false;
            }
        }
        if (result)
        {
            lua_pushlightuserdata(L, physics_context);
            PHYSICS_CONTEXT_HASH = dmScript::SetGlobal(L, PHYSICS_CONTEXT_NAME);
        }
        else
        {
            delete physics_context;
        }
    }

    void ScriptPhysicsFinalize(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        if (L != 0x0)
        {
            int top = lua_gettop(L);
            (void)top;

            dmScript::GetGlobal(L, PHYSICS_CONTEXT_HASH);
            PhysicsScriptContext* physics_context = (PhysicsScriptContext*)lua_touserdata(L, -1);
            lua_pop(L, 1);
            if (physics_context != 0x0)
            {
                delete physics_context;
            }

            assert(top == lua_gettop(L));
        }
    }

}
