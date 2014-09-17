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
#define COLLISION_OBJECT_EXT "collisionobjectc"

namespace dmGameSystem
{
    struct PhysicsScriptContext
    {
        dmMessage::HSocket m_Socket;
        uint32_t m_ComponentIndex;
    };

    /*# requests a ray cast to be performed
     * Ray casts are used to test for intersections against collision objects in the physics world.
     * Which collision objects to hit is filtered by their collision groups and can be configured through <code>groups</code>.
     * The actual ray cast will be performed during the physics-update.
     * If an object is hit, the result will be reported via a <code>ray_cast_response</code> message.
     *
     * @name physics.ray_cast
     * @param from the world position of the start of the ray (vector3)
     * @param to the world position of the end of the ray (vector3)
     * @param groups a lua table containing the hashed groups for which to test collisions against (table)
     * @param [request_id] a number between 0-255 that will be sent back in the response for identification, 0 by default (number)
     * @examples
     * <p>
     * How to perform a ray cast:
     * </p>
     * <pre>
     * function init(self)
     *     self.my_interesting_groups = {hash("my_group1"), hash("my_group2")}
     * end
     *
     * function update(self, dt)
     *     -- request ray cast
     *     physics.ray_cast(interesting_start, interesting_end, self.my_interesting_groups)
     * end
     *
     * function on_message(self, message_id, message, sender)
     *     -- check for the response
     *     if message_id == hash("ray_cast_response") then
     *         -- act on the hit
     *     end
     * end
     * </pre>
     */
    int Physics_RayCast(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;

        // NOTE! .script instance is a requirement for this function, see DispatchMessages in comp_collision_object.cpp
        // This has been added as a part of DEF-700
        dmGameObject::HInstance sender_instance = CheckGoInstance(L, SCRIPT_TYPE_BIT_LOGIC);

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
        dmMessage::Post(&sender, &receiver, dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_NameHash, (uintptr_t)sender_instance, (uintptr_t)dmPhysicsDDF::RequestRayCast::m_DDFDescriptor, &request, sizeof(dmPhysicsDDF::RequestRayCast));
        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg PHYSICS_FUNCTIONS[] =
    {
        {"ray_cast",            Physics_RayCast},
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
        uint32_t co_resource_type;
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
