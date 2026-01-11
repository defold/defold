// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/align.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <gameobject/gameobject_props_lua.h>

#include <dmsdk/dlib/vmath.h>
#include <dmsdk/gamesys/script.h>
#include <dmsdk/gameobject/script.h>

#include "gamesys.h"
#include <gamesys/gamesys_ddf.h>
#include "../gamesys_private.h"
#include "../components/comp_collection_factory.h"
#include "resources/res_collection_factory.h"

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
     * @language Lua
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
     * @constant
     */
    /*# loading
     *
     * @name collectionfactory.STATUS_LOADING
     * @constant
     */
    /*# loaded
     *
     * @name collectionfactory.STATUS_LOADED
     * @constant
     */
    static int CollectionFactoryComp_GetStatus(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        CollectionFactoryComponent* component;
        dmScript::GetComponentFromLua(L, 1, COLLECTION_FACTORY_EXT, 0, (void**)&component, 0);

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
    static int CollectionFactoryComp_Unload(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        CollectionFactoryWorld* world;
        CollectionFactoryComponent* component;
        dmScript::GetComponentFromLua(L, 1, COLLECTION_FACTORY_EXT, (void**)&world, (void**)&component, 0);

        bool success = dmGameSystem::CompCollectionFactoryUnload(world, component);
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
     * @param [complete_function] [type:function(self, url, result)] function to call when resources are loaded.
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
    static int CollectionFactoryComp_Load(lua_State* L)
    {
        int top = lua_gettop(L);
        if (top < 2 || !lua_isfunction(L, 2))
        {
            return luaL_error(L, "Argument #2 is expected to be completion function.");
        }

        CollectionFactoryWorld* world;
        CollectionFactoryComponent* component;
        dmMessage::URL receiver;
        dmScript::GetComponentFromLua(L, 1, COLLECTION_FACTORY_EXT, (void**)&world, (void**)&component, &receiver);

        if (dmGameSystem::CompCollectionFactoryIsLoading(component)) {
            dmLogError("Trying to load collection factory resource when already loading.");
            return luaL_error(L, "Error loading collection factory resources");
        }

        lua_pushvalue(L, 2);
        int callback_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmScript::GetInstance(L);
        int self_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmScript::PushURL(L, receiver);
        int url_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

        bool success = dmGameSystem::CompCollectionFactoryLoad(world, component, callback_ref, self_ref, url_ref);
        if (!success)
        {
            dmScript::Unref(L, LUA_REGISTRYINDEX, callback_ref);
            dmScript::Unref(L, LUA_REGISTRYINDEX, self_ref);
            dmScript::Unref(L, LUA_REGISTRYINDEX, url_ref);
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
     * @param [scale] [type:number|vector3] uniform scaling to apply to the newly spawned collection (must be greater than 0).
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

    static int CollectionFactoryComp_Create(lua_State* L)
    {
        int top = lua_gettop(L);
        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        CollectionFactoryWorld* world;
        CollectionFactoryComponent* component;
        dmScript::GetComponentFromLua(L, 1, COLLECTION_FACTORY_EXT, (void**)&world, (void**)&component, 0);

        dmVMath::Point3 position;
        if (top >= 2 && !lua_isnil(L, 2))
        {
            position = dmVMath::Point3(*dmScript::CheckVector3(L, 2));
        }
        else
        {
            position = dmGameObject::GetWorldPosition(sender_instance);
        }

        dmVMath::Quat rotation;
        if (top >= 3 && !lua_isnil(L, 3))
        {
            rotation = *dmScript::CheckQuat(L, 3);
        }
        else
        {
            rotation = dmGameObject::GetWorldRotation(sender_instance);
        }

        dmGameObject::InstancePropertyContainers props;
        props.SetCapacity(8, 32);

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
                    dmGameObject::HPropertyContainer properties = dmGameObject::PropertyContainerCreateFromLua(L, -1);

                    props.Put(instance_id, properties);
                    lua_pop(L, 1);
                }
                lua_pop(L, 1);
            }
            else
            {
                return luaL_error(L, "expected table at argument #4 to collectionfactory.create");
            }
        }

        dmVMath::Vector3 scale;
        if (top >= 5 && !lua_isnil(L, 5))
        {
            // We check for zero in the ToTransform/ResetScale in transform.h
            dmVMath::Vector3* v = dmScript::ToVector3(L, 5);
            if (v != 0)
            {
                scale = *v;
            }
            else
            {
                float val = luaL_checknumber(L, 5);
                scale = dmVMath::Vector3(val, val, val);
            }
        }
        else
        {
            scale = dmGameObject::GetWorldScale(sender_instance);
        }

        dmScript::GetInstance(L);
        int ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

        dmGameObject::InstanceIdMap instances;
        dmGameObject::Result result = dmGameSystem::CompCollectionFactorySpawn(world, component, collection, nullptr, position, rotation, scale, &props, &instances);

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        dmScript::SetInstance(L);
        dmScript::Unref(L, LUA_REGISTRYINDEX, ref);

        // Construct return table
        if (result == dmGameObject::RESULT_OK)
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

        // Free the property containers
        dmGameObject::InstancePropertyContainers::Iterator iter(props);
        while (iter.Next())
        {
            dmGameObject::PropertyContainerDestroy(iter.GetValue());
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# changes the prototype for the collection factory
     *
     * Changes the prototype for the collection factory.
     * Setting the prototype to "nil" will revert back to the original prototype.
     *
     * @name collectionfactory.set_prototype
     * @param [url] [type:string|hash|url] the collection factory component
     * @param [prototype] [type:string|nil] the path to the new prototype, or `nil`
     *
     * @note
     *   - Requires the factory to have the "Dynamic Prototype" set
     *   - Cannot be set when the state is COMP_FACTORY_STATUS_LOADING
     *   - Setting the prototype to "nil" will revert back to the original prototype.
     *
     * @examples
     *
     * How to unload the previous prototypes resources, and then spawn a new collection
     *
     * ```lua
     * collectionfactory.unload("#factory") -- unload the previous resources
     * collectionfactory.set_prototype("#factory", "/main/levels/level1.collectionc")
     * local ids = collectionfactory.create("#factory", go.get_world_position(), vmath.quat())
     * ```
     */
    static int CollectionFactoryComp_SetPrototype(lua_State* L)
    {
        int top = lua_gettop(L);

        CollectionFactoryWorld* world;
        CollectionFactoryComponent* component;
        dmMessage::URL url; // for reporting errors only
        dmScript::GetComponentFromLua(L, 1, COLLECTION_FACTORY_EXT, (void**)&world, (void**)&component, &url);

        if(!CompCollectionFactoryIsDynamicPrototype(component))
        {
            return luaL_error(L, "Cannot set prototype to a collection factory that doesn't have dynamic prototype set: '%s:%s#%s'",
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
        }

        if (dmGameSystem::CompCollectionFactoryIsLoading(component))
        {
            return luaL_error(L, "Cannot set prototype while factory is loading");
        }

        dmResource::HFactory factory = CompCollectionFactoryGetResourceFactory(world);
        CollectionFactoryResource* default_resource = CompCollectionFactoryGetDefaultResource(component);
        CollectionFactoryResource* custom_resource = CompCollectionFactoryGetCustomResource(component);
        CollectionFactoryResource* new_resource = 0;
        CollectionFactoryResource* old_resource = custom_resource;

        const char* path = 0;
        dmhash_t path_hash = 0;
        if (!lua_isnil(L, 2))
        {
            path = luaL_checkstring(L, 2);
            path_hash = dmHashString64(path);

            // check that the path is a .collectionc
            const char* ext = dmResource::GetExtFromPath(path);
            if (!ext || strcmp(ext, "collectionc") != 0)
            {
                return luaL_error(L, "Trying to set '%s' as prototype to '%s:%s#%s'. Only .collectionc resources are allowed",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
            }
        }

        if (path == 0 || path_hash == default_resource->m_PrototypePathHash) // We want to reset to the default prototype
        {
            new_resource = 0;
            old_resource = custom_resource;
        }
        else if (custom_resource && path_hash == custom_resource->m_PrototypePathHash) // we try to reset the currently set custom prototype
        {
            new_resource = custom_resource;
            old_resource = 0;
        }
        else { // We want to create a new resource

            dmResource::Result r = dmGameSystem::ResCollectionFactoryLoadResource(factory, path, true, true, &new_resource);
            if (dmResource::RESULT_OK != r)
            {
                return luaL_error(L, "Failed to load collection factory prototype %s", path);
            }
        }

        CompCollectionFactorySetResource(component, new_resource);

        if (old_resource)
        {
            dmGameSystem::ResCollectionFactoryDestroyResource(factory, old_resource);
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg COLLECTION_FACTORY_COMP_FUNCTIONS[] =
    {
        {"create",            CollectionFactoryComp_Create},
        {"load",              CollectionFactoryComp_Load},
        {"unload",            CollectionFactoryComp_Unload},
        {"get_status",        CollectionFactoryComp_GetStatus},
        {"set_prototype",     CollectionFactoryComp_SetPrototype},
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
