#include "gamesys.h"

#include <dlib/log.h>
#include <physics/physics.h>

#include "components/comp_collision_object.h"

#include "physics_ddf.h"
#include "gamesys_ddf.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
#define PHYSICS_CONTEXT_NAME "__PhysicsContext"
#define COLLISION_OBJECT_EXT "collisionobjectc"

    struct PhysicsScriptContext
    {
        dmMessage::HSocket m_Socket;
        uint32_t m_ComponentIndex;
    };

    ScriptLibContext::ScriptLibContext()
    {
        memset(this, 0, sizeof(*this));
    }

    /*# requests a ray cast to be performed
     * Ray casts are used to test for intersections against collision objects in the physics world.
     * Which collision objects to hit is filtered by their collision groups and can be configured through <code>groups</code>.
     * The actual ray cast will be performed during the physics-update.
     * If an object is hit, the result will be reported via a <code>ray_cast_response</code> message.
     *
     * @name physics.ray_cast
     * @param from the world position of the start of the ray (vector3)
     * @param to the world position of the end of the ray (vector3)
     * @param groups a lua table containing the hashed groups for which to test collisions against
     * @param [request_id] a number between 0-255 that will be sent back in the response for identification, 0 by default
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

            lua_getglobal(L, PHYSICS_CONTEXT_NAME);
            PhysicsScriptContext* context = (PhysicsScriptContext*)lua_touserdata(L, -1);
            lua_pop(L, 1);

            dmGameObject::HInstance sender_instance = (dmGameObject::HInstance)user_data;
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
            dmMessage::Post(&sender, &receiver, dmHashString64(dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_Name), user_data, (uintptr_t)dmPhysicsDDF::RequestRayCast::m_DDFDescriptor, &request, sizeof(dmPhysicsDDF::RequestRayCast));
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "physics.ray_cast is not available from this script-type.");
        }
    }

    static const luaL_reg PHYSICS_FUNCTIONS[] =
    {
        {"ray_cast",            Physics_RayCast},
        {0, 0}
    };

    /*# make a factory create a new game object
     * Which factory to create the game object is identified by the URL.
     * The game object will be created between this frame and the next.
     *
     * Properties defined in scripts in the created game object can be overridden through the properties-parameter below.
     * See go.property for more information on script properties.
     *
     * @name factory.create
     * @param url the factory that should create a game object (url)
     * @param [position] the position of the new game object, the position of the game object containing the factory is used by default (vector3)
     * @param [rotation] the rotation of the new game object, the rotation of the game object containing the factory is used by default (quat)
     * @param [properties] the properties defined in a script attached to the new game object (table)
     * @return the id of the spawned game object (hash)
     * @examples
     * How to create a new game object:
     * <pre>
     * function init(self)
     *     self.my_created_object = factory.create("#factory", nil, nil, {my_value = 1})
     * end
     *
     * function update(self, dt)
     *     -- communicate with the object
     *     msg.post(msg.url(nil, self.my_created_object), "hello")
     * end
     * </pre>
     * And then let the new game object have a script attached:
     * <pre>
     * go.property("my_value", 0)
     *
     * function init(self, params)
     *     -- do something with params.my_value which is now one
     * end
     */
    int FactoryComp_Create(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            dmGameObject::HInstance sender_instance = (dmGameObject::HInstance)user_data;
            dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::Create* request = (dmGameSystemDDF::Create*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::Create);
            if (top >= 2 && !lua_isnil(L, 2))
            {
                request->m_Position = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 2));
            }
            else
            {
                request->m_Position = dmGameObject::GetWorldPosition(sender_instance);
            }
            if (top >= 3 && !lua_isnil(L, 3))
            {
                request->m_Rotation = *dmScript::CheckQuat(L, 3);
            }
            else
            {
                request->m_Rotation = dmGameObject::GetWorldRotation(sender_instance);
            }
            uint32_t prop_buffer_size = 0;
            if (top >= 4)
            {
                uint8_t* prop_buffer = &buffer[msg_size];
                prop_buffer_size = dmGameObject::LuaTableToProperties(L, 4, prop_buffer, buffer_size - msg_size);
                if (prop_buffer_size == 0)
                    return luaL_error(L, "the properties supplied to factory.create are too many.");
            }
            request->m_Id = dmGameObject::GenerateUniqueInstanceId(collection);

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmHashString64(dmGameSystemDDF::Create::m_DDFDescriptor->m_Name), user_data, (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor, buffer, msg_size + prop_buffer_size);
            assert(top == lua_gettop(L));
            dmScript::PushHash(L, request->m_Id);
            return 1;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "factory.create is not available from this script-type.");
        }
    }

    static const luaL_reg FACTORY_COMP_FUNCTIONS[] =
    {
        {"create",            FactoryComp_Create},
        {0, 0}
    };

    bool InitializeScriptLibs(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        dmResource::HFactory factory = context.m_Factory;
        dmGameObject::HRegister regist = context.m_Register;

        int top = lua_gettop(L);
        (void)top;

        bool result = true;

        luaL_register(L, "physics", PHYSICS_FUNCTIONS);
        lua_pop(L, 1);
        luaL_register(L, "factory", FACTORY_COMP_FUNCTIONS);
        lua_pop(L, 1);

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
            dmResource::Result fact_result = dmResource::GetTypeFromExtension(factory, COLLISION_OBJECT_EXT, &co_resource_type);
            if (fact_result != dmResource::RESULT_OK)
            {
                dmLogError("Unable to get resource type for '%s': %d.", COLLISION_OBJECT_EXT, fact_result);
                result = false;
            }
        }
        if (result)
        {
            dmGameObject::ComponentType* co_component_type = dmGameObject::FindComponentType(regist, co_resource_type, &physics_context->m_ComponentIndex);
            if (co_component_type == 0x0)
            {
                dmLogError("Could not find componet type '%s'.", COLLISION_OBJECT_EXT);
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

        assert(top == lua_gettop(L));
        return result;
    }

    void FinalizeScriptLibs(const ScriptLibContext& context)
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

#undef PHYSICS_CONTEXT_NAME
#undef COLLISION_OBJECT_EXT
}
