#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "script_factory.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
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
     * @param [scale] the scale of the new game object (must be greater than 0), the scale of the game object containing the factory is used by default (number)
     * @return the global id of the spawned game object (hash)
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
     * function init(self)
     *     -- do something with self.my_value which is now one
     * end
     * </pre>
     */
    int FactoryComp_Create(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        const uint32_t buffer_size = 512;
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
            char* prop_buffer = (char*)&buffer[msg_size];
            uint32_t prop_buffer_size = buffer_size - msg_size;
            actual_prop_buffer_size = dmScript::CheckTable(L, prop_buffer, prop_buffer_size, 4);
            if (actual_prop_buffer_size > prop_buffer_size)
                return luaL_error(L, "the properties supplied to factory.create are too many.");
        }
        if (top >= 5 && !lua_isnil(L, 5))
        {
            request->m_Scale = luaL_checknumber(L, 5);
            if (request->m_Scale <= 0.0f)
            {
                return luaL_error(L, "The scale supplied to factory.create must be greater than 0.");
            }
        }
        else
        {
            request->m_Scale = dmGameObject::GetWorldScale(sender_instance);
        }
        request->m_Id = dmGameObject::GenerateUniqueInstanceId(collection);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::Create::m_DDFDescriptor->m_NameHash, (uintptr_t)sender_instance, (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor, buffer, msg_size + actual_prop_buffer_size);
        dmScript::PushHash(L, request->m_Id);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg FACTORY_COMP_FUNCTIONS[] =
    {
        {"create",            FactoryComp_Create},
        {0, 0}
    };


    void ScriptFactoryRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "factory", FACTORY_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
