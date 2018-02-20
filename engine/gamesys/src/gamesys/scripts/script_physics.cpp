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

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define PHYSICS_CONTEXT_NAME "__PhysicsContext"

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
     * Which collision objects to hit is filtered by their collision groups and can be configured
     * through `groups`.
     * The actual ray cast will be performed during the physics-update.
     *
     * - If an object is hit, the result will be reported via a `ray_cast_response` message.
     * - If there is no object hit, the result will be reported via a `ray_cast_missed` message.
     *
     * @name physics.ray_cast
     * @param from [type:vector3] the world position of the start of the ray
     * @param to [type:vector3] the world position of the end of the ray
     * @param groups [type:table] a lua table containing the hashed groups for which to test collisions against
     * @param [request_id] [type:number] a number between 0-255 that will be sent back in the response for identification, 0 by default
     * @examples
     *
     * How to perform a ray cast:
     *
     * ```lua
     * function init(self)
     *     self.my_groups = {hash("my_group1"), hash("my_group2")}
     * end
     *
     * function update(self, dt)
     *     -- request ray cast
     *     physics.ray_cast(my_start, my_end, self.my_groups)
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
    static int Physics_RayCast(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);

        if (!dmScript::GetURL(L, &sender)) {
            return luaL_error(L, "could not find a requesting instance for physics.ray_cast");
        }
        dmPhysicsDDF::RequestRayCast request;
        request.m_From = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 1));
        request.m_To = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 2));
        request.m_Mask = 0;
        luaL_checktype(L, 3, LUA_TTABLE);

        lua_getglobal(L, PHYSICS_CONTEXT_NAME);
        PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
        lua_pop(L, 1);

        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);
        void* world = dmGameObject::GetWorld(collection, context->m_ComponentIndex);

        lua_pushnil(L);
        while (lua_next(L, 3) != 0)
        {
            request.m_Mask |= CompCollisionGetGroupBitIndex(world, dmScript::CheckHash(L, -1));
            lua_pop(L, 1);
        }
        request.m_RequestId = 0;
        if (top > 3)
        {
            request.m_RequestId = luaL_checkinteger(L, 4);
            if (request.m_RequestId > 255)
            {
                return luaL_error(L, "request_id must be between 0-255");
            }
        }
        dmMessage::URL receiver;
        dmMessage::ResetURL(receiver);
        receiver.m_Socket = context->m_Socket;
        dmMessage::Post(&sender, &receiver, dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_NameHash, (uintptr_t)sender_instance, (uintptr_t)dmPhysicsDDF::RequestRayCast::m_DDFDescriptor, &request, sizeof(dmPhysicsDDF::RequestRayCast), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    struct ContactPointUserData
    {
        lua_State* m_L;
        int m_Count;
        dmGameSystem::CollisionWorld* m_World;
    };

    static void ContactPointCallback(const dmPhysics::ContactPoint& cp, void* user_ctx)
    {
        ContactPointUserData* user_data = (ContactPointUserData*)user_ctx;

        lua_State* L = user_data->m_L;
        if (user_data->m_Count == 0) {
            lua_newtable(L); // Don't know in advance the number of results
        }

        dmGameSystem::CollisionComponent* component_a = (dmGameSystem::CollisionComponent*)cp.m_UserDataA;
        dmGameSystem::CollisionComponent* component_b = (dmGameSystem::CollisionComponent*)cp.m_UserDataB;

        lua_pushnumber(L, ++user_data->m_Count); // index of the array
        lua_createtable(L, 0, 10);

        dmScript::PushVector3(L, Vector3(cp.m_PositionA));
        lua_setfield(L, -2, "position");

        dmScript::PushVector3(L, cp.m_Normal);
        lua_setfield(L, -2, "normal");

        lua_pushnumber(L, cp.m_Distance);
        lua_setfield(L, -2, "distance");

        lua_pushnumber(L, cp.m_MassA);
        lua_setfield(L, -2, "mass");

        lua_pushnumber(L, cp.m_MassB);
        lua_setfield(L, -2, "other_mass");

        dmScript::PushHash(L, dmGameObject::GetIdentifier(dmGameSystem::CompCollisionGetGameObject(component_b)));
        lua_setfield(L, -2, "other_id");

        dmScript::PushVector3(L, Vector3(cp.m_PositionB));
        lua_setfield(L, -2, "other_position");

        dmScript::PushHash(L, dmGameSystem::GetLSBGroupHash(user_data->m_World, cp.m_GroupB));
        lua_setfield(L, -2, "other_group");

        dmScript::PushHash(L, dmGameSystem::GetLSBGroupHash(user_data->m_World, cp.m_GroupA));
        lua_setfield(L, -2, "own_group");

        lua_settable(L, -3); // Pop the child table
    }



    /*# collects contact points for a collision object
     *
     * Uses a collision object to check for a collision at a user defined position/rotation.
     * Can be used for checking if an object will collide with obstacles in the future.
     *
     * Returns a list of contact points.
     *
     * @note The result structs are compatible with the "contact_point_response" event, except that it doesn't contain the time related properties "applied_impulse", "relative_velocity" and "life_time".
     *
     * @note Doesn't change the state of the used collision object. The object is only temporarily put in another position/rotation.
     *
     * @name physics.contacts
     * @param url [type:url] the collision object component to test with
     * @param position [type:vector3] the world position of the collision object
     * @param rotation [type:quat] the world rotation of the collision object
     *
     * @return contact_point_response [type:table] a table with the following fields:
     *
     * - position [type:vector3] world position of the contact point
     * - normal [type:vector3] normal in world space of the contact point, which points from the other object towards the current object
     * - distance [type:number] the penetration distance between the objects, which is always positive
     * - mass [type:number] the mass of the current collision object in kg
     * - other_mass [type:number] the mass of the other collision object in kg
     * - other_id [type:hash] the id of the instance the collision object is in contact with
     * - other_position [type:vector3] the world position of the other collision object
     * - other_group [type:hash] the collision group of the other collision object (hash)
     * - own_group [type:hash] the collision group of the own collision object (hash)
     *
     * @examples
     *
     * How to perform the test:
     *
     * ```lua
     * function update(self, dt)
     *     local collisions = physics.contact("#collisionobject", go.get_position(), go.get_rotation())
     *
     *     -- Used by the handle_obstacle_contact() from the Platformer tutorial
     *     self.correction = vmath.vector3()
     *     self.ground_contact = false
     *     self.velocity = vmath.vector3()
     *
     *     if collisions ~= nil then
     *         for i = 1, #collisions do
     *             local collision = collisions[i]
     *             -- The handle_obstacle_contact() is from the Platformer tutorial
     *             handle_obstacle_contact(self, collision.normal, collision.distance)
     *         end
     *     end
     * end
     * ```
     */
    static int Physics_Contact(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        // * get url
        // * get component from url (same troubles as before with getting collection, world, then component etc)
        // * get (correct) physics shape from component
        // * do physics overlap test with shape and transform

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmScript::ResolveURL(L, 1, &receiver, 0);

        Point3 pos;
        if (lua_isnil(L, 2)) {
            pos = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 2));
        } else {
            pos = dmGameObject::GetPosition(instance);
        }

        Quat rot;
        if (lua_isnil(L, 3)) {
            rot = *dmScript::CheckQuat(L, 3);
        } else {
            rot = dmGameObject::GetRotation(instance);
        }

        CollisionWorld* world = 0;
        dmGameSystem::CollisionComponent* component = (dmGameSystem::CollisionComponent*)dmGameObject::GetComponentFromURL(receiver, (void**)&world);
        if (!component) {
            return DM_LUA_ERROR("Could not find instance %s:%s#%s", dmHashReverseSafe64(receiver.m_Socket), dmHashReverseSafe64(receiver.m_Path), dmHashReverseSafe64(receiver.m_Fragment));
        }

        PhysicsContext* physics_context = CompCollisionGetPhysicsContext(world);

        ContactPointUserData ctx;
        ctx.m_L = L;
        ctx.m_Count = 0;
        ctx.m_World = world;

        ContactPointTest3D(physics_context, world, component, pos, rot, ContactPointCallback, (void*)&ctx);

        if (ctx.m_Count == 0)
            lua_pushnil(L);

        return 1;
    }

    static const luaL_reg PHYSICS_FUNCTIONS[] =
    {
        {"ray_cast",            Physics_RayCast},
        {"contact",             Physics_Contact},
        {0, 0}
    };

    void ScriptPhysicsRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "physics", PHYSICS_FUNCTIONS);
        lua_pop(L, 1);

        bool result = true;

        PhysicsScriptContext* physics_context = new PhysicsScriptContext();
        dmMessage::Result socket_result = dmMessage::GetSocket(dmPhysics::PHYSICS_SOCKET_NAME, &physics_context->m_Socket);
        if (socket_result != dmMessage::RESULT_OK)
        {
            dmLogError("Could not retrieve the physics socket '%s': %d.", dmPhysics::PHYSICS_SOCKET_NAME, socket_result);
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
            lua_setglobal(L, PHYSICS_CONTEXT_NAME);
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

            lua_getglobal(L, PHYSICS_CONTEXT_NAME);
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

#undef PHYSICS_CONTEXT_NAME
#undef COLLISION_OBJECT_EXT
