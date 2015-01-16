#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"
#include "../components/comp_collection_factory.h"

#include "script_collection_factory.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    static int HashTableIndex(lua_State* L)
    {
        if (lua_isstring(L, -1))
        {
            dmScript::PushHash(L, dmHashString64(lua_tostring(L, -1)));
            lua_rawget(L, -3);
            return 1;
        }
        else
        {
            lua_pushvalue(L, -1);
            lua_rawget(L, -3);
            return 1;
        }
    }

    static void InsertInstanceEntry(lua_State *L, const dmhash_t *key, dmhash_t *value)
    {
        dmScript::PushHash(L, *key);
        dmScript::PushHash(L, *value);
        lua_rawset(L, -3);
    }

    int CollectionFactoryComp_Create(lua_State* L)
    {
        int top = lua_gettop(L);
        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, COLLECTION_FACTORY_EXT, &user_data, &receiver, 0);
        CollectionFactoryComponent* component = (CollectionFactoryComponent*) user_data;

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

        const uint32_t buffer_size = 4096;
        uint8_t buffer[buffer_size];
        uint32_t buffer_pos = 0;

        dmGameObject::InstancePropertyBuffers prop_bufs;
        prop_bufs.SetCapacity(8, 32);

        if (top >= 4)
        {
            //
            if (lua_istable(L, 4))
            {
                // Read out all the property sets for all supplied game object instances
                lua_pushvalue(L, 4);
                lua_pushnil(L);
                while (lua_next(L, -2))
                {
                    dmhash_t instance_id = dmScript::CheckHash(L, -2);
                    uint32_t left = buffer_size - buffer_pos;

                    uint32_t size = dmScript::CheckTable(L, (char*)(buffer + buffer_pos), left, -1);
                    if (size > left)
                        return luaL_error(L, "the properties supplied to collectionfactory.create are too many.");

                    dmGameObject::InstancePropertyBuffer buf;
                    buf.property_buffer = buffer + buffer_pos;
                    buf.property_buffer_size = size;
                    buffer_pos = buffer_pos + size;

                    prop_bufs.Put(instance_id, buf);
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            else
            {
                return luaL_error(L, "expected table at argument #4 to collectionfactory.create");
            }
        }

        float scale;
        if (top >= 5 && !lua_isnil(L, 5))
        {
            scale = luaL_checknumber(L, 5);
            if (scale <= 0.0f)
            {
                return luaL_error(L, "The scale supplied to factory.create must be greater than 0.");
            }
        }
        else
        {
            scale = dmGameObject::GetWorldScale(sender_instance);
        }

        dmScript::GetInstance(L);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);

        dmGameObject::InstanceIdMap instances;
        bool success = dmGameObject::SpawnFromCollection(collection, component->m_Resource->m_CollectionFactoryDesc->m_Prototype, &prop_bufs,
                                                         position, rotation, scale, &instances);

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        dmScript::SetInstance(L);
        luaL_unref(L, LUA_REGISTRYINDEX, ref);

        // Construct return table
        if (success)
        {
            lua_newtable(L);
            lua_createtable(L, 0, 1);
            lua_pushcfunction(L, HashTableIndex);
            lua_setfield(L, -2, "__index");
            lua_setmetatable(L, -2);
            instances.Iterate(&InsertInstanceEntry, L);
        }
        else
        {
            // empty table
            lua_newtable(L);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg COLLECTION_FACTORY_COMP_FUNCTIONS[] =
    {
        {"create",            CollectionFactoryComp_Create},
        {0, 0}
    };


    void ScriptCollectionFactoryRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "collectionfactory", COLLECTION_FACTORY_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
