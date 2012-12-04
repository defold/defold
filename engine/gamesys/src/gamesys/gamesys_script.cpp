#include "gamesys.h"

#include <dlib/log.h>
#include <physics/physics.h>

#include "components/comp_collision_object.h"

#include "scripts/script_particlefx.h"
#include "scripts/script_tilemap.h"

#include "physics_ddf.h"
#include "gamesys_ddf.h"
#include "sprite_ddf.h"

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
            dmMessage::Post(&sender, &receiver, dmPhysicsDDF::RequestRayCast::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmPhysicsDDF::RequestRayCast::m_DDFDescriptor, &request, sizeof(dmPhysicsDDF::RequestRayCast));
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
     * @param [scale] the scale of the new game object, can not be 0, the scale of the game object containing the factory is used by default (number)
     * @return the id of the spawned game object (hash)
     * @examples
     * <p>
     * How to create a new game object:
     * </p>
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
     * <p>
     * And then let the new game object have a script attached:
     * </p>
     * <pre>
     * go.property("my_value", 0)
     *
     * function init(self, params)
     *     -- do something with params.my_value which is now one
     * end
     * </pre>
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
            uint32_t actual_prop_buffer_size = 0;
            if (top >= 4)
            {
                uint8_t* prop_buffer = &buffer[msg_size];
                uint32_t prop_buffer_size = buffer_size - msg_size;
                actual_prop_buffer_size = dmGameObject::LuaTableToProperties(L, 4, prop_buffer, prop_buffer_size);
                if (actual_prop_buffer_size > prop_buffer_size)
                    return luaL_error(L, "the properties supplied to factory.create are too many.");
            }
            if (top >= 5 && !lua_isnil(L, 5))
            {
                request->m_Scale = luaL_checknumber(L, 5);
                if (request->m_Scale == 0.0f)
                {
                    return luaL_error(L, "The scale supplied to factory.create can not be 0.");
                }
            }
            else
            {
                request->m_Scale = dmGameObject::GetWorldScale(sender_instance);
            }
            request->m_Id = dmGameObject::GenerateUniqueInstanceId(collection);

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::Create::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor, buffer, msg_size + actual_prop_buffer_size);
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

    /*# make a sprite flip the animations horizontally or not
     * Which sprite to flip is identified by the URL.
     * If the currently playing animation is flipped by default, flipping it again will make it appear like the original texture.
     *
     * @name sprite.set_hflip
     * @param url the sprite that should flip its animations (url)
     * @param flip if the sprite should flip its animations or not (boolean)
     * @examples
     * <p>
     * How to flip a sprite so it faces the horizontal movement:
     * </p>
     * <pre>
     * function update(self, dt)
     *     -- calculate self.velocity somehow
     *     sprite.set_hflip("#sprite", self.velocity.x < 0)
     * end
     * </pre>
     * <p>It is assumed that the sprite component has id "sprite" and that the original animations faces right.</p>
     */
    int SpriteComp_SetHFlip(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::SetFlipHorizontal* request = (dmGameSystemDDF::SetFlipHorizontal*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::SetFlipHorizontal);

            request->m_Flip = (uint32_t)lua_toboolean(L, 2);

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "sprite.set_hflip is not available from this script-type.");
        }
    }

    /*# make a sprite flip the animations vertically or not
     * Which sprite to flip is identified by the URL.
     * If the currently playing animation is flipped by default, flipping it again will make it appear like the original texture.
     *
     * @name sprite.set_vflip
     * @param url the sprite that should flip its animations (url)
     * @param flip if the sprite should flip its animations or not (boolean)
     * @examples
     * <p>
     * How to flip a sprite in a game which negates gravity as a game mechanic:
     * </p>
     * <pre>
     * function update(self, dt)
     *     -- calculate self.up_side_down somehow
     *     sprite.set_vflip("#sprite", self.up_side_down)
     * end
     * </pre>
     * <p>It is assumed that the sprite component has id "sprite" and that the original animations are up-right.</p>
     */
    int SpriteComp_SetVFlip(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::SetFlipVertical* request = (dmGameSystemDDF::SetFlipVertical*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::SetFlipVertical);

            request->m_Flip = (uint32_t)lua_toboolean(L, 2);

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "sprite.set_vflip is not available from this script-type.");
        }
    }

    /*# set a shader constant for a sprite
     * The constant must be defined in the material assigned to the sprite.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until sprite.reset_constant is called.
     * Which sprite to set a constant for is identified by the URL.
     *
     * @name sprite.set_constant
     * @param url the sprite that should have a constant set (url)
     * @param name of the constant (string|hash)
     * @param value of the constant (vec4)
     * @examples
     * <p>
     * The following examples assumes that the sprite has id "sprite" and that the default-material in builtins is used.
     * If you assign a custom material to the sprite, you can set the constants defined there in the same manner.
     * </p>
     * <p>
     * How to tint a sprite to red:
     * </p>
     * <pre>
     * function init(self)
     *     sprite.set_constant("#sprite", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * </pre>
     */
    int SpriteComp_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            dmhash_t name_hash;
            if (lua_isstring(L, 2))
            {
                name_hash = dmHashString64(lua_tostring(L, 2));
            }
            else if (dmScript::IsHash(L, 2))
            {
                name_hash = dmScript::CheckHash(L, 2);
            }
            else
            {
                return luaL_error(L, "name must be either a hash or a string");
            }
            Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 3);

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::SetConstant* request = (dmGameSystemDDF::SetConstant*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::SetConstant);

            request->m_NameHash = name_hash;
            request->m_Value = *value;

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::SetConstant::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "sprite.set_constant is not available from this script-type.");
        }
    }

    /*# reset a shader constant for a sprite
     * The constant must be defined in the material assigned to the sprite.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which sprite to reset a constant for is identified by the URL.
     *
     * @name sprite.reset_constant
     * @param url the sprite that should have a constant reset (url)
     * @param name of the constant (string|hash)
     * @examples
     * <p>
     * The following examples assumes that the sprite has id "sprite" and that the default-material in builtins is used.
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     * </p>
     * <p>
     * How to reset the tinting of a sprite:
     * </p>
     * <pre>
     * function init(self)
     *     sprite.reset_constant("#sprite", "tint")
     * end
     * </pre>
     */
    int SpriteComp_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            dmhash_t name_hash;
            if (lua_isstring(L, 2))
            {
                name_hash = dmHashString64(lua_tostring(L, 2));
            }
            else if (dmScript::IsHash(L, 2))
            {
                name_hash = dmScript::CheckHash(L, 2);
            }
            else
            {
                return luaL_error(L, "name must be either a hash or a string");
            }

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::ResetConstant* request = (dmGameSystemDDF::ResetConstant*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::ResetConstant);

            request->m_NameHash = name_hash;

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::ResetConstant::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "sprite.reset_constant is not available from this script-type.");
        }
    }

    // Docs intentionally left out until we decide to go public with this function
    int SpriteComp_SetScale(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            Vectormath::Aos::Vector3* scale = dmScript::CheckVector3(L, 2);

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::SetScale* request = (dmGameSystemDDF::SetScale*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::SetScale);

            request->m_Scale = *scale;

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::SetScale::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "sprite.set_scale is not available from this script-type.");
        }
    }

    static const luaL_reg SPRITE_COMP_FUNCTIONS[] =
    {
            {"set_hflip",       SpriteComp_SetHFlip},
            {"set_vflip",       SpriteComp_SetVFlip},
            {"set_constant",    SpriteComp_SetConstant},
            {"reset_constant",  SpriteComp_ResetConstant},
            {"set_scale",       SpriteComp_SetScale},
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
        luaL_register(L, "sprite", SPRITE_COMP_FUNCTIONS);
        lua_pop(L, 1);

        ScriptParticleFXRegister(L);
        ScriptTileMapRegister(L);

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
