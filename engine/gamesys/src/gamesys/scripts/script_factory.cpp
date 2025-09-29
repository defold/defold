// Copyright 2020-2025 The Defold Foundation
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

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/gamesys/script.h>
#include <dmsdk/gameobject/gameobject.h>
#include <gameobject/gameobject_props_lua.h>
#include <dmsdk/gameobject/script.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include <gamesys/gamesys_ddf.h>
#include "../gamesys_private.h"
#include "../components/comp_factory.h"
#include "resources/res_factory.h"

#include "script_factory.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# Factory API documentation
     *
     * Functions for controlling factory components which are used to
     * dynamically spawn game objects into the runtime.
     *
     * @document
     * @name Factory
     * @namespace factory
     * @language Lua
     */

    /*# Get factory status
     *
     * This returns status of the factory.
     *
     * Calling this function when the factory is not marked as dynamic loading always returns
     * factory.STATUS_LOADED.
     *
     * @name factory.get_status
     * @param [url] [type:string|hash|url] the factory component to get status from
     * @return status [type:constant] status of the factory component
     *
     * - `factory.STATUS_UNLOADED`
     * - `factory.STATUS_LOADING`
     * - `factory.STATUS_LOADED`
     *
     */
    /*# unloaded
     *
     * @name factory.STATUS_UNLOADED
     * @constant
     */
    /*# loading
     *
     * @name factory.STATUS_LOADING
     * @constant
     */
    /*# loaded
     *
     * @name factory.STATUS_LOADED
     * @constant
     */
    static int FactoryComp_GetStatus(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        HFactoryWorld world;
        HFactoryComponent component;
        dmScript::GetComponentFromLua(L, 1, FACTORY_EXT, (dmGameObject::HComponentWorld*)&world, (dmGameObject::HComponent*)&component, 0);

        dmGameSystem::CompFactoryStatus status = dmGameSystem::CompFactoryGetStatus(world, component);
        lua_pushinteger(L, (int)status);
        return 1;
    }

    /*# Unload resources previously loaded using factory.load
     *
     * This decreases the reference count for each resource loaded with factory.load. If reference is zero, the resource is destroyed.
     *
     * Calling this function when the factory is not marked as dynamic loading does nothing.
     *
     * @name factory.unload
     * @param [url] [type:string|hash|url] the factory component to unload
     *
     * @examples
     *
     * How to unload resources of a factory prototype loaded with factory.load
     *
     * ```lua
     * factory.unload("#factory")
     * ```
     */
    static int FactoryComp_Unload(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        HFactoryWorld world;
        HFactoryComponent component;
        dmScript::GetComponentFromLua(L, 1, FACTORY_EXT, (dmGameObject::HComponentWorld*)&world, (dmGameObject::HComponent*)&component, 0);

        bool success = dmGameSystem::CompFactoryUnload(world, component);
        if (!success)
        {
            return DM_LUA_ERROR("Error unloading factory resources");
        }
        return 0;
    }


    /*# Load resources of a factory prototype.
     *
     * Resources are referenced by the factory component until the existing (parent) collection is destroyed or factory.unload is called.
     *
     * Calling this function when the factory is not marked as dynamic loading does nothing.
     *
     * @name factory.load
     * @param [url] [type:string|hash|url] the factory component to load
     * @param [complete_function] [type:function(self, url, result)] function to call when resources are loaded.
     *
     * `self`
     * : [type:object] The current object.
     *
     * `url`
     * : [type:url] url of the factory component
     *
     * `result`
     * : [type:boolean] True if resources were loaded successfully
     *
     * @examples
     *
     * How to load resources of a factory prototype.
     *
     * ```lua
     * factory.load("#factory", function(self, url, result) end)
     * ```
     */
    static int FactoryComp_Load(lua_State* L)
    {
        int top = lua_gettop(L);

        if (top < 2 || !lua_isfunction(L, 2))
        {
            return luaL_error(L, "Argument #2 is expected to be completion function.");
        }

        dmMessage::URL receiver;
        HFactoryWorld world;
        HFactoryComponent component;
        dmScript::GetComponentFromLua(L, 1, FACTORY_EXT, (dmGameObject::HComponentWorld*)&world, (dmGameObject::HComponent*)&component, &receiver);

        if (dmGameSystem::CompFactoryIsLoading(world, component)) {
            dmLogError("Trying to load factory prototype resource when already loading.");
            return luaL_error(L, "Error loading factory resources");
        }

        lua_pushvalue(L, 2);
        int callback_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmScript::GetInstance(L);
        int self_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);
        dmScript::PushURL(L, receiver);
        int url_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

        bool success = dmGameSystem::CompFactoryLoad(world, component, callback_ref, self_ref, url_ref);
        if (!success)
        {
            dmScript::Unref(L, LUA_REGISTRYINDEX, callback_ref);
            dmScript::Unref(L, LUA_REGISTRYINDEX, self_ref);
            dmScript::Unref(L, LUA_REGISTRYINDEX, url_ref);
            return luaL_error(L, "Error loading factory resources");
        }

        assert(top == lua_gettop(L));
        return 0;
    }


    /*# make a factory create a new game object
     *
     * The URL identifies which factory should create the game object.
     * If the game object is created inside of the frame (e.g. from an update callback), the game object will be created instantly, but none of its component will be updated in the same frame.
     *
     * Properties defined in scripts in the created game object can be overridden through the properties-parameter below.
     * See go.property for more information on script properties.
     *
     * [icon:attention] Calling [ref:factory.create] on a factory that is marked as dynamic without having loaded resources
     * using [ref:factory.load] will synchronously load and create resources which may affect application performance.
     *
     * @name factory.create
     * @param url [type:string|hash|url] the factory that should create a game object.
     * @param [position] [type:vector3] the position of the new game object, the position of the game object calling `factory.create()` is used by default, or if the value is `nil`.
     * @param [rotation] [type:quaternion] the rotation of the new game object, the rotation of the game object calling `factory.create()` is used by default, or if the value is `nil`.
     * @param [properties] [type:table] the properties defined in a script attached to the new game object.
     * @param [scale] [type:number|vector3] the scale of the new game object (must be greater than 0), the scale of the game object containing the factory is used by default, or if the value is `nil`
     * @return id [type:hash] the global id of the spawned game object
     * @examples
     *
     * How to create a new game object:
     *
     * ```lua
     * function init(self)
     *     -- create a new game object and provide property values
     *     self.my_created_object = factory.create("#factory", nil, nil, {my_value = 1})
     *     -- communicate with the object
     *     msg.post(self.my_created_object, "hello")
     * end
     * ```
     *
     * And then let the new game object have a script attached:
     *
     * ```lua
     * go.property("my_value", 0)
     *
     * function init(self)
     *     -- do something with self.my_value which is now one
     * end
     * ```
     */
    static int FactoryComp_CreateWithMessage(lua_State* L, dmGameObject::HCollection collection, dmMessage::URL* receiver,
                                            dmhash_t id, dmGameObject::HPropertyContainer properties,
                                            const dmVMath::Point3& position, const dmVMath::Quat& rotation, const dmVMath::Vector3& scale)
    {
        uint8_t DM_ALIGNED(16) buffer[512];

        dmGameSystemDDF::Create* create_msg = (dmGameSystemDDF::Create*)buffer;
        create_msg->m_Id = id;
        create_msg->m_Position = position;
        create_msg->m_Rotation = rotation;
        create_msg->m_Scale3 = scale;

        uint8_t* prop_buffer = buffer + sizeof(dmGameSystemDDF::Create);
        uint32_t prop_buffer_size = DM_ARRAY_SIZE(buffer) - sizeof(dmGameSystemDDF::Create);
        uint32_t properties_size = 0;
        if (properties)
        {
            properties_size = dmGameObject::PropertyContainerGetMemorySize(properties);
            if (properties_size > prop_buffer_size)
                return luaL_error(L, "Properties of size %u bytes won't fit in the buffer of size %u", properties_size, prop_buffer_size);

            dmGameObject::PropertyContainerSerialize(properties, prop_buffer, prop_buffer_size);
        }

        dmMessage::URL sender;
        if (!dmScript::GetURL(L, &sender)) {
            return luaL_error(L, "factory.create can not be called from this script type");
        }

        dmMessage::Post(&sender, receiver, dmGameSystemDDF::Create::m_DDFDescriptor->m_NameHash, 0,
                        (uintptr_t)dmGameSystemDDF::Create::m_DDFDescriptor, buffer, sizeof(dmGameSystemDDF::Create) + properties_size, 0);

        return 0;
    }

    static int FactoryComp_Create(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = dmScript::CheckGOInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        HFactoryWorld world;
        HFactoryComponent component;
        dmMessage::URL receiver;
        dmScript::GetComponentFromLua(L, 1, FACTORY_EXT, (dmGameObject::HComponentWorld*)&world, (dmGameObject::HComponent*)&component, &receiver);

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

        dmGameObject::HPropertyContainer properties = 0;
        if (top >= 4 && lua_istable(L, 4))
        {
            properties = dmGameObject::PropertyContainerCreateFromLua(L, 4);
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

        dmhash_t id = dmGameObject::CreateInstanceId();

        // When calling factory.create() from a .gui_script (see engine/src/test/factory/factory.gui_script)
        bool msg_passing = dmGameObject::GetInstanceFromLua(L) == 0x0;
        if (msg_passing)
        {
            FactoryComp_CreateWithMessage(L, collection, &receiver, id, properties, position, rotation, scale);
            // We currently don't know if the creation succeeds
            dmScript::PushHash(L, id);
        }
        else
        {
            // Since the spawning will invoke any scripts on that new instance,
            // we need a way to restore the state
            dmScript::GetInstance(L);
            int ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

            dmGameObject::HInstance instance;
            dmGameObject::Result result = CompFactorySpawn(world, component, collection,
                                                            id, position, rotation, scale, properties, &instance);

            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            dmScript::SetInstance(L);
            dmScript::Unref(L, LUA_REGISTRYINDEX, ref);

            if (result == dmGameObject::RESULT_OK)
            {
                dmScript::PushHash(L, id);
            }
            else
            {
                lua_pushnil(L);
            }
        }

        dmGameObject::PropertyContainerDestroy(properties);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# changes the prototype for the factory
     *
     * Changes the prototype for the factory.
     *
     * @name factory.set_prototype
     * @param [url] [type:string|hash|url] the factory component
     * @param [prototype] [type:string|nil] the path to the new prototype, or `nil`
     *
     * @note
     *   - Requires the factory to have the "Dynamic Prototype" set
     *   - Cannot be set when the state is COMP_FACTORY_STATUS_LOADING
     *   - Setting the prototype to `nil` will revert back to the original prototype.
     *
     * @examples
     *
     * How to unload the previous prototypes resources, and then spawn a new game object
     *
     * ```lua
     * factory.unload("#factory") -- unload the previous resources
     * factory.set_prototype("#factory", "/main/levels/enemyA.goc")
     * local id = factory.create("#factory", go.get_world_position(), vmath.quat())
     * ```
     */
    static int FactoryComp_SetPrototype(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL url;
        HFactoryWorld world;
        HFactoryComponent component;
        dmScript::GetComponentFromLua(L, 1, FACTORY_EXT, (dmGameObject::HComponentWorld*)&world, (dmGameObject::HComponent*)&component, &url);

        if(!CompFactoryIsDynamicPrototype(world, component))
        {
            return luaL_error(L, "Cannot set prototype to a factory that doesn't have dynamic prototype set: '%s:%s#%s'",
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
        }

        if (dmGameSystem::CompFactoryIsLoading(world, component))
        {
            return luaL_error(L, "Cannot set prototype while factory is loading");
        }

        const char* path = 0;
        if (!lua_isnil(L, 2))
        {
            path = luaL_checkstring(L, 2);

            // check that the path is a .goc
            const char* ext = dmResource::GetExtFromPath(path);
            if (!ext || strcmp(ext, "goc") != 0)
            {
                return luaL_error(L, "Trying to set '%s' as prototype to '%s:%s#%s'. Only .goc resources are allowed",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
            }
        }

        dmResource::HFactory factory = CompFactoryGetResourceFactory(world);
        HFactoryResource default_resource = CompFactoryGetDefaultResource(world, component);
        HFactoryResource custom_resource = CompFactoryGetCustomResource(world, component);
        HFactoryResource new_resource = 0;
        HFactoryResource old_resource = custom_resource;

        if (path == 0 || strcmp(path, default_resource->m_PrototypePath) == 0) // We want to reset to the default prototype
        {
            new_resource = 0;
            old_resource = custom_resource;
        }
        else if (custom_resource && strcmp(path, custom_resource->m_PrototypePath) == 0) // we try to reset the currently set custom prototype
        {
            new_resource = custom_resource;
            old_resource = 0;
        }
        else { // We want to create a new resource

            dmResource::Result r = dmGameSystem::ResFactoryLoadResource(factory, path, true, true, &new_resource);
            if (dmResource::RESULT_OK != r)
            {
                return luaL_error(L, "Failed to load collection factory prototype %s", path);
            }
        }

        CompFactorySetResource(world, component, new_resource);

        if (old_resource)
        {
            dmGameSystem::ResFactoryDestroyResource(factory, old_resource);
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg FACTORY_COMP_FUNCTIONS[] =
    {
        {"create",            FactoryComp_Create},
        {"load",              FactoryComp_Load},
        {"unload",            FactoryComp_Unload},
        {"get_status",        FactoryComp_GetStatus},
        {"set_prototype",     FactoryComp_SetPrototype},
        {0, 0}
    };


    void ScriptFactoryRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "factory", FACTORY_COMP_FUNCTIONS);

        #define SETCONSTANT(value, name) \
                lua_pushnumber(L, (lua_Number) value); \
                lua_setfield(L, -2, #name);
        SETCONSTANT(COMP_FACTORY_STATUS_UNLOADED, STATUS_UNLOADED)
        SETCONSTANT(COMP_FACTORY_STATUS_LOADING, STATUS_LOADING)
        SETCONSTANT(COMP_FACTORY_STATUS_LOADED, STATUS_LOADED)
        #undef SETCONSTANT

        lua_pop(L, 1);
    }
}
