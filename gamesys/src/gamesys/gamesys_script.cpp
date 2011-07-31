#include "gamesys.h"

#include <dlib/log.h>
#include <physics/physics.h>

#include "components/comp_collision_object.h"

#include "physics_ddf.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    dmMessage::HSocket g_PhysicsSocket;

    /*# requests a ray cast to be performed
     * The ray cast will be performed during the physics-update. If an object is hit, the
     * result will be reported via a <code>ray_cast_response</code> message.
     *
     * @name physics.ray_cast
     * @param from the world position of the start of the ray (vector3)
     * @param to the world position of the end of the ray (vector3)
     * @param groups a lua table containing the hashed groups for which to test collisions against
     * @examples
     * How to perform a ray cast:
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
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            dmPhysicsDDF::RequestRayCast request;
            request.m_From = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 1));
            request.m_To = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 2));
            request.m_Mask = 0;
            luaL_checktype(L, 3, LUA_TTABLE);

            dmGameObject::HInstance sender_instance = (dmGameObject::HInstance)user_data;
            dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);
            uint32_t type;
            dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(dmGameObject::GetFactory(collection), "collisionobjectc", &type);
            if (fact_result != dmResource::FACTORY_RESULT_OK)
            {
                dmLogError("Unable to get resource type for '%s' (%d)", "collisionobjectc", fact_result);
                return luaL_error(L, "System error.");
            }
            void* world = dmGameObject::FindWorld(collection, type);

            lua_pushnil(L);
            while (lua_next(L, 3) != 0)
            {
                request.m_Mask |= CompCollisionGetGroupBitIndex(world, dmScript::CheckHash(L, -1));
                lua_pop(L, 1);
            }
            dmMessage::URL receiver;
            receiver.m_Socket = g_PhysicsSocket;
            dmMessage::Post(&sender, &receiver, dmHashString64(dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_Name), user_data, (uintptr_t)dmPhysicsDDF::RequestRayCast::m_DDFDescriptor, &request, sizeof(dmPhysicsDDF::RequestRayCast));
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "physics.ray_cast not allowed from this script.");
        }
    }

    static const luaL_reg PHYSICS_FUNCTIONS[] =
    {
        {"ray_cast",            Physics_RayCast},
        {0, 0}
    };

    void RegisterLibs(lua_State* L)
    {
        int top = lua_gettop(L);

        luaL_register(L, "physics", PHYSICS_FUNCTIONS);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        dmMessage::GetSocket(dmPhysics::PHYSICS_SOCKET_NAME, &g_PhysicsSocket);
    }
}
