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
    /*# Collection factory API documentation
     *
     * Functions for controlling collection factory components which are
     * used to dynamically spawn collections into the runtime.
     *
     * @document
     * @name Collection factory
     * @namespace collectionfactory
     */

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

    /*# Get collection factory status
     *
     * This returns status of the collection factory.
     *
     * Calling this function when the factory is not marked as dynamic loading always returns COMP_COLLECTION_FACTORY_STATUS_LOADED.
     *
     * @name collectionfactory.get_status
     * @param [url] [type:string|hash|url] the collection factory component to get status from
     * @return status [type:constant] status of the collection factory component
     *
     * - `collectionfactory.STATUS_UNLOADED`
     * - `collectionfactory.STATUS_LOADING`
     * - `collectionfactory.STATUS_LOADED`
     *
     */
    /*# unloaded
     *
     * @name collectionfactory.STATUS_UNLOADED
     * @variable
     */
    /*# loading
     *
     * @name collectionfactory.STATUS_LOADING
     * @variable
     */
    /*# loaded
     *
     * @name collectionfactory.STATUS_LOADED
     * @variable
     */
    int CollectionFactoryComp_GetStatus(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, COLLECTION_FACTORY_EXT, &user_data, &receiver, 0);
        CollectionFactoryComponent* component = (CollectionFactoryComponent*) user_data;

        dmGameSystem::CompCollectionFactoryStatus status = dmGameSystem::CompCollectionFactoryGetStatus(component);
        lua_pushinteger(L, (int)status);
        return 1;
    }

    /*# Unload resources previously loaded using collectionfactory.load
     *
     * This decreases the reference count for each resource loaded with collectionfactory.load. If reference is zero, the resource is destroyed.
     *
     * Calling this function when the factory is not marked as dynamic loading does nothing.
     *
     * @name collectionfactory.unload
     * @param [url] [type:string|hash|url] the collection factory component to unload
     *
     * @examples
     *
     * How to unload resources of a collection factory prototype loaded with collectionfactory.load
     *
     * ```lua
     * collectionfactory.unload("#factory")
     * ```
     */
    int CollectionFactoryComp_Unload(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, COLLECTION_FACTORY_EXT, &user_data, &receiver, 0);
        CollectionFactoryComponent* component = (CollectionFactoryComponent*) user_data;

        bool success = dmGameSystem::CompCollectionFactoryUnload(collection, component);
        if (!success)
        {
            return DM_LUA_ERROR("Error unloading collection factory resources");
        }
        return 0;
    }

    /*# Load resources of a collection factory prototype.
     *
     * Resources loaded are referenced by the collection factory component until the existing (parent) collection is destroyed or collectionfactory.unload is called.
     *
     * Calling this function when the factory is not marked as dynamic loading does nothing.
     *
     * @name collectionfactory.load
     * @param [url] [type:string|hash|url] the collection factory component to load
     * @param [complete_function] [type:function(self, url, result))] function to call when resources are loaded.
     *
     * `self`
     * : [type:object] The current object.
     *
     * `url`
     * : [type:url] url of the collection factory component
     *
     * `result`
     * : [type:boolean] True if resource were loaded successfully
     *
     * @examples
     *
     * How to load resources of a collection factory prototype.
     *
     * ```lua
     * collectionfactory.load("#factory", function(self, url, result) end)
     * ```
     */
    int CollectionFactoryComp_Load(lua_State* L)
    {
        int top = lua_gettop(L);
        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        if (top < 2 || !lua_isfunction(L, 2))
        {
            return luaL_error(L, "Argument #2 is expected to be completion function.");
        }

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, COLLECTION_FACTORY_EXT, &user_data, &receiver, 0);
        CollectionFactoryComponent* component = (CollectionFactoryComponent*) user_data;

        lua_pushvalue(L, 2);
        component->m_PreloaderCallbackRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmScript::GetInstance(L);
        component->m_PreloaderSelfRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmScript::PushURL(L, receiver);
        component->m_PreloaderURLRef = dmScript::Ref(L, LUA_REGISTRYINDEX);

        bool success = dmGameSystem::CompCollectionFactoryLoad(collection, component);
        if (!success)
        {
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_PreloaderCallbackRef);
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_PreloaderSelfRef);
            dmScript::Unref(L, LUA_REGISTRYINDEX, component->m_PreloaderURLRef);
            return luaL_error(L, "Error loading collection factory resources");
        }

        assert(top == lua_gettop(L));
        return 0;
    }


    /*# Spawn a new instance of a collection into the existing collection.
     * The URL identifies the collectionfactory component that should do the spawning.
     *
     * Spawning is instant, but spawned game objects get their first update calls the following frame. The supplied parameters for position, rotation and scale
     * will be applied to the whole collection when spawned.
     *
     * Script properties in the created game objects can be overridden through
     * a properties-parameter table. The table should contain game object ids
     * (hash) as keys and property tables as values to be used when initiating each
     * spawned game object.
     *
     * See go.property for more information on script properties.
     *
     * The function returns a table that contains a key for each game object
     * id (hash), as addressed if the collection file was top level, and the
     * corresponding spawned instance id (hash) as value with a unique path
     * prefix added to each instance.
     *
     * [icon:attention] Calling [ref:collectionfactory.create] create on a collection factory that is marked as dynamic without having loaded resources
     * using [ref:collectionfactory.load] will synchronously load and create resources which may affect application performance.
     *
     * @name collectionfactory.create
     * @param url [type:string|hash|url] the collection factory component to be used
     * @param [position] [type:vector3] position to assign to the newly spawned collection
     * @param [rotation] [type:quaternion] rotation to assign to the newly spawned collection
     * @param [properties] [type:table] table of script properties to propagate to any new game object instances
     * @param [scale] [type:number] uniform scaling to apply to the newly spawned collection (must be greater than 0).
     * @return ids [type:table] a table mapping the id:s from the collection to the new instance id:s
     * @examples
     *
     * How to spawn a collection of game objects:
     *
     * ```lua
     * function init(self)
     *   -- Spawn a small group of enemies.
     *   local pos = vmath.vector3(100, 12.5, 0)
     *   local rot = vmath.quat_rotation_z(math.pi / 2)
     *   local scale = 0.5
     *   local props = {}
     *   props[hash("/enemy_leader")] = { health = 1000.0 }
     *   props[hash("/enemy_1")] = { health = 200.0 }
     *   props[hash("/enemy_2")] = { health = 400.0, color = hash("green") }
     *
     *   local self.enemy_ids = collectionfactory.create("#enemyfactory", pos, rot, props, scale)
     *   -- enemy_ids now map to the spawned instance ids:
     *   --
     *   -- pprint(self.enemy_ids)
     *   --
     *   -- DEBUG:SCRIPT:
     *   -- {
     *   --   hash: [/enemy_leader] = hash: [/collection0/enemy_leader],
     *   --   hash: [/enemy_1] = hash: [/collection0/enemy_1],
     *   --   hash: [/enemy_2] = hash: [/collection0/enemy_2]
     *   -- }
     *
     *   -- Send "attack" message to the leader. First look up its instance id.
     *   local leader_id = self.enemy_ids[hash("/enemy_leader")]
     *   msg.post(leader_id, "attack")
     * end
     * ```
     *
     * How to delete a spawned collection:
     *
     * ```lua
     * go.delete(self.enemy_ids)
     * ```
     */

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

        if (top >= 4 && !lua_isnil(L, 4))
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

        dmScript::GetInstance(L);
        int ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmGameObject::InstanceIdMap instances;
        bool success = dmGameObject::SpawnFromCollection(collection, component->m_Resource->m_CollectionDesc, &prop_bufs,
                                                         position, rotation, scale, &instances);

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        dmScript::SetInstance(L);
        dmScript::Unref(L, LUA_REGISTRYINDEX, ref);

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
        {"load",              CollectionFactoryComp_Load},
        {"unload",            CollectionFactoryComp_Unload},
        {"get_status",        CollectionFactoryComp_GetStatus},
        {0, 0}
    };


    void ScriptCollectionFactoryRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "collectionfactory", COLLECTION_FACTORY_COMP_FUNCTIONS);

        #define SETCONSTANT(value, name) \
                lua_pushnumber(L, (lua_Number) value); \
                lua_setfield(L, -2, #name);
        SETCONSTANT(COMP_COLLECTION_FACTORY_STATUS_UNLOADED, STATUS_UNLOADED)
        SETCONSTANT(COMP_COLLECTION_FACTORY_STATUS_LOADING, STATUS_LOADING)
        SETCONSTANT(COMP_COLLECTION_FACTORY_STATUS_LOADED, STATUS_LOADED)
        #undef SETCONSTANT

        lua_pop(L, 1);
    }
}
