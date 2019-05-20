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

    static int Physics_CreateJoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        //int type = luaL_checkinteger(L, 1);

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL urla;
        dmScript::ResolveURL(L, 1, &urla, 0);
        // dmMessage::URL urlb;
        // dmScript::ResolveURL(L, 5, &urlb, 0);
        // Vectormath::Aos::Point3 apos = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 4));
        // Vectormath::Aos::Point3 bpos = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 6));

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
        dmGameObject::HInstance instance_a = dmGameObject::GetInstanceFromIdentifier(collection, urla.m_Path);
        // dmGameObject::HInstance instance_b = dmGameObject::GetInstanceFromIdentifier(collection, urlb.m_Path);

        lua_getglobal(L, PHYSICS_CONTEXT_NAME);
        PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        uint32_t type_index_a;
        void* comp_a = dmGameObject::GetComponentFromInstance(instance_a, urla.m_Fragment, &type_index_a);
        // uint32_t type_index_b;
        // void* comp_b = dmGameObject::GetComponentFromInstance(instance_b, urlb.m_Fragment, &type_index_b);

        // Now check that both components are collision objects
        if (type_index_a != context->m_ComponentIndex) {
            return DM_LUA_ERROR("Url %s.%s is not a collision object", dmHashReverseSafe64(urla.m_Path), dmHashReverseSafe64(urla.m_Fragment));
        }
        // if (type_index_b != context->m_ComponentIndex) {
        //     return DM_LUA_ERROR("Url %s.%s is not a collision object", dmHashReverseSafe64(urlb.m_Path), dmHashReverseSafe64(urlb.m_Fragment));
        // }

        void* comp_world = dmGameObject::GetWorld(collection, context->m_ComponentIndex);
        bool is2D = CompCollisionIs2D(comp_world);
        if (!is2D) {
            return DM_LUA_ERROR("physics.create_joint() is currently only supported with 2D physics");
        }

        // dmPhysics::HWorld2D world = CompCollisionGetPhysicsWorld2D(comp_world);
        // dmPhysics::HCollisionObject2D obja = CompCollisionGetObject2D(comp_world, comp_a);
        // dmPhysics::HCollisionObject2D objb = CompCollisionGetObject2D(comp_world, comp_b);

        // dmPhysics::HJoint2D joint = 0;
        //if (type == dmPhysics::JOINT_TYPE_DISTANCE) {

            // float frequency = luaL_checknumber(L, 6);
            // float damping = luaL_checknumber(L, 7);
            // if (damping < 0.0f || damping > 1.0f ) {
                // return DM_LUA_ERROR("damping must be between 0 and 1");
            // }
            // bool collide_connected = lua_isnil(L, 8) ? true : luaL_checkinteger(L, 8) != 0;

            // bool CreateJoint(void* _world, void* _component, dmhash_t id)
            bool r = dmGameSystem::CreateJoint(comp_world, comp_a, joint_id);
            if (!r) {
                return DM_LUA_ERROR("could not create joint");
            }
        //}

        return 0;
    }

    // physics.connect_joint("obj_a#coll", "apa", posa - diff * 25, "obj_b#coll", posb + diff * 25)
    static int Physics_ConnectJoint(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        int type = luaL_checkinteger(L, 1);

        dmhash_t joint_id = dmScript::CheckHashOrString(L, 3);

        dmMessage::URL urla;
        dmScript::ResolveURL(L, 2, &urla, 0);
        dmMessage::URL urlb;
        dmScript::ResolveURL(L, 5, &urlb, 0);
        Vectormath::Aos::Point3 apos = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 4));
        Vectormath::Aos::Point3 bpos = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 6));

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
        dmGameObject::HInstance instance_a = dmGameObject::GetInstanceFromIdentifier(collection, urla.m_Path);
        dmGameObject::HInstance instance_b = dmGameObject::GetInstanceFromIdentifier(collection, urlb.m_Path);

        lua_getglobal(L, PHYSICS_CONTEXT_NAME);
        PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        uint32_t type_index_a;
        void* comp_a = dmGameObject::GetComponentFromInstance(instance_a, urla.m_Fragment, &type_index_a);
        uint32_t type_index_b;
        void* comp_b = dmGameObject::GetComponentFromInstance(instance_b, urlb.m_Fragment, &type_index_b);

        // Now check that both components are collision objects
        if (type_index_a != context->m_ComponentIndex) {
            return DM_LUA_ERROR("Url %s.%s is not a collision object", dmHashReverseSafe64(urla.m_Path), dmHashReverseSafe64(urla.m_Fragment));
        }
        if (type_index_b != context->m_ComponentIndex) {
            return DM_LUA_ERROR("Url %s.%s is not a collision object", dmHashReverseSafe64(urlb.m_Path), dmHashReverseSafe64(urlb.m_Fragment));
        }

        void* comp_world = dmGameObject::GetWorld(collection, context->m_ComponentIndex);
        bool is2D = CompCollisionIs2D(comp_world);
        if (!is2D) {
            return DM_LUA_ERROR("physics.connect_joint() is currently only supported with 2D physics");
        }

        if (type == dmPhysics::JOINT_TYPE_DISTANCE) {

            bool r = dmGameSystem::ConnectDistanceJoint(comp_world, comp_a, joint_id, apos, comp_b, bpos);
            if (!r) {
                return DM_LUA_ERROR("could not connect joint");
            }

        } else if (type == dmPhysics::JOINT_TYPE_ROPE) {

            bool r = dmGameSystem::ConnectRopeJoint(comp_world, comp_a, joint_id, apos, comp_b, bpos);
            if (!r) {
                return DM_LUA_ERROR("could not connect joint");
            }

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
        {0, 0}
    };

    void ScriptPhysicsRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "physics", PHYSICS_FUNCTIONS);

#define SETCONSTANT(name) \
    lua_pushnumber(L, (lua_Number) dmPhysics::name); \
    lua_setfield(L, -2, #name);\

        SETCONSTANT(JOINT_TYPE_DISTANCE)
        SETCONSTANT(JOINT_TYPE_ROPE)

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
