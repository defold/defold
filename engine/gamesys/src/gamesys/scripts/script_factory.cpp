#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"
#include "../components/comp_factory.h"

#include "script_factory.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# make a factory create a new game object
     * The URL identifies which factory should create the game object.
     * If the game object is created inside of the frame (e.g. from an update callback), the game object will be created instantly, but none of its component will be updated in the same frame.
     *
     * Properties defined in scripts in the created game object can be overridden through the properties-parameter below.
     * See go.property for more information on script properties.
     *
     * @name factory.create
     * @param url the factory that should create a game object (url)
     * @param [position] the position of the new game object, the position of the game object containing the factory is used by default (vector3)
     * @param [rotation] the rotation of the new game object, the rotation of the game object containing the factory is used by default (quat)
     * @param [properties] the properties defined in a script attached to the new game object (table)
     * @param [scale] the scale of the new game object (must be greater than 0), the scale of the game object containing the factory is used by default (number or vector3)
     * @return the global id of the spawned game object (hash)
     * @examples
     * <p>
     * How to create a new game object:
     * </p>
     * <pre>
     * function init(self)
     *     self.my_created_object = factory.create("#factory", nil, nil, {my_value = 1})
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

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, FACTORY_EXT, &user_data, &receiver, 0);
        FactoryComponent* component = (FactoryComponent*) user_data;

        Vectormath::Aos::Point3 position;
        if (top >= 2 && !lua_isnil(L, 2))
        {
            position = Vectormath::Aos::Point3(*dmScript::CheckVector3(L, 2));
        }
        else
        {
            position = dmGameObject::GetWorldPosition(sender_instance);
        }
        Vectormath::Aos::Quat rotation;
        if (top >= 3 && !lua_isnil(L, 3))
        {
            rotation = *dmScript::CheckQuat(L, 3);
        }
        else
        {
            rotation = dmGameObject::GetWorldRotation(sender_instance);
        }
        const uint32_t buffer_size = 512;
        uint8_t buffer[buffer_size];
        uint32_t actual_prop_buffer_size = 0;
        uint8_t* prop_buffer = buffer;
        uint32_t prop_buffer_size = buffer_size;
        bool msg_passing = dmGameObject::GetInstanceFromLua(L) == 0x0;
        if (msg_passing) {
            const uint32_t msg_size = sizeof(dmGameSystemDDF::Create);
            prop_buffer = &(buffer[msg_size]);
            prop_buffer_size -= msg_size;
        }
        if (top >= 4)
        {
            actual_prop_buffer_size = dmScript::CheckTable(L, (char*)prop_buffer, prop_buffer_size, 4);
            if (actual_prop_buffer_size > prop_buffer_size)
                return luaL_error(L, "the properties supplied to factory.create are too many.");
        }

        Vector3 scale;
        if (top >= 5 && !lua_isnil(L, 5))
        {
            if (dmScript::IsVector3(L, 5))
            {
                scale = *dmScript::CheckVector3(L, 5);
            }
            else
            {
                float val = luaL_checknumber(L, 5);
                if (val <= 0.0f)
                {
                    return luaL_error(L, "The scale supplied to factory.create must be greater than 0.");
                }
                scale = Vector3(val, val, val);
            }
        }
        else
        {
            scale = dmGameObject::GetWorldScale(sender_instance);
        }
        dmhash_t id = dmGameObject::GenerateUniqueInstanceId(collection);

        if (msg_passing) {
            dmGameSystemDDF::Create* create_msg = (dmGameSystemDDF::Create*)buffer;
            create_msg->m_Id = id;
            create_msg->m_Position = position;
            create_msg->m_Rotation = rotation;
            create_msg->m_Scale3 = scale;
            dmMessage::URL sender;
            if (!dmScript::GetURL(L, &sender)) {
                luaL_error(L, "factory.create can not be called from this script type");
                return 1;
            }
            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::Create::m_DDFDescriptor->m_NameHash, (uintptr_t)sender_instance, (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor, buffer, sizeof(dmGameSystemDDF::Create) + actual_prop_buffer_size);
        } else {
            dmScript::GetInstance(L);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);
            dmGameObject::Spawn(collection, component->m_Resource->m_FactoryDesc->m_Prototype, id, buffer, actual_prop_buffer_size,
                    position, rotation, scale);
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            dmScript::SetInstance(L);
            luaL_unref(L, LUA_REGISTRYINDEX, ref);
        }

        dmScript::PushHash(L, id);
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
