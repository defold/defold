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

#include "script_collectionproxy.h"

#include <dlib/dstrings.h>
#include <script/script.h>
#include <gameobject/script.h>
#include <gameobject/gameobject.h>
#include <dmsdk/gamesys/script.h>
#include <resource/resource_util.h>
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <components/comp_collection_proxy.h>

namespace dmGameSystem
{
    /*# Collection proxy API documentation
     *
     * Messages for controlling and interacting with collection proxies
     * which are used to dynamically load collections into the runtime.
     *
     * @document
     * @name Collection proxy
     * @namespace collectionproxy
     * @language Lua
     */

    static dmhash_t GetCollectionUrlHashFromCollectionProxy(lua_State* L, int index, dmResource::HFactory* factory)
    {
        dmMessage::URL sender;
        dmMessage::URL receiver;
        dmScript::ResolveURL(L, index, &receiver, 0x0);

        dmScript::GetURL(L, &sender);
        dmGameObject::HInstance sender_instance = dmGameSystem::CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);
        dmGameObject::HInstance receiver_instance = dmGameObject::GetInstanceFromIdentifier(collection, receiver.m_Path);

        if (receiver_instance == 0x0)
        {
            return 0;
        }

        uint16_t component_index = 0;
        dmGameObject::GetComponentIndex(receiver_instance, receiver.m_Fragment, &component_index);
        dmGameObject::HComponent user_data = 0;
        dmGameSystem::HCollectionProxyWorld world = 0;
        dmGameObject::GetComponentFromLua(L, index, collection, COLLECTION_PROXY_EXT, &user_data, &receiver, (dmGameObject::HComponentWorld*)&world);

        if (factory)
            *factory = dmGameObject::GetFactory(receiver_instance);

        return dmGameSystem::GetCollectionUrlHashFromComponent(world, dmGameObject::GetIdentifier(receiver_instance), component_index);
    }

    struct GetResourceHashContext
    {
        lua_State* m_L;
        int        m_Index;
    };

    static void GetResourceHashCallback(void* context, const dmResource::SGetDependenciesResult* result)
    {
        GetResourceHashContext* ctx = (GetResourceHashContext*)context;
        lua_State* L = ctx->m_L;

        char hash_buffer[64*2+1];
        dmResource::BytesToHexString(result->m_HashDigest, result->m_HashDigestLength, hash_buffer, sizeof(hash_buffer));

        lua_pushnumber(L, ctx->m_Index++);
        lua_pushlstring(L, hash_buffer, result->m_HashDigestLength*2);
        lua_settable(L, -3);
    }

    static int CollectionProxy_GetResourcesInternal(lua_State* L, bool only_missing)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmResource::HFactory factory;
        dmhash_t url_hash = GetCollectionUrlHashFromCollectionProxy(L, 1, &factory);

        if (url_hash == 0)
        {
            return DM_LUA_ERROR("Unable to find collection proxy component.");
        }

        lua_newtable(L);

        GetResourceHashContext ctx;
        ctx.m_L = L;
        ctx.m_Index = 1;

        dmResource::SGetDependenciesParams params;
        params.m_UrlHash = url_hash;
        params.m_OnlyMissing = only_missing;
        params.m_Recursive = false;
        params.m_IncludeRequestedUrl = true;
        dmResource::GetDependencies(factory, &params, GetResourceHashCallback, &ctx);

        return 1;
    }

    // See doc in comp_collection_proxy.cpp
    static int CollectionProxy_MissingResources(lua_State* L)
    {
        return CollectionProxy_GetResourcesInternal(L, true);
    }

    // See doc in comp_collection_proxy.cpp
    static int CollectionProxy_GetResources(lua_State* L)
    {
        return CollectionProxy_GetResourcesInternal(L, false);
    }

    /*# changes the collection for a collection proxy.
     * 
     * The collection should be loaded by the collection proxy.
     * Setting the collection to "nil" will revert it back to the original collection.
     * 
     * The collection proxy shouldn't be loaded and should have the 'Exclude' checkbox checked.
     * This functionality is designed to simplify the management of Live Update resources.
     *
     * @name collectionproxy.set_collection
     * @param [url] [type:string|hash|url] the collection proxy component
     * @param [prototype] [type:string|nil] the path to the new collection, or `nil`
     * @return success [type:boolean] collection change was successful
     * @return code [type:number] one of the collectionproxy.RESULT_* codes if unsuccessful
     *
     * @examples
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * local ok, error = collectionproxy.set_collection("/go#collectionproxy", "/LU/3.collectionc")
     *  if ok then
     *      print("The collection has been changed to /LU/3.collectionc")
     *  else
     *      print("Error changing collection to /LU/3.collectionc ", error)
     *  end
     *  msg.post("/go#collectionproxy", "load")
     *  msg.post("/go#collectionproxy", "init")
     *  msg.post("/go#collectionproxy", "enable")
     * ```
     */
    static int CollectionProxy_SetCollection(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 2)
        CollectionProxyWorld* world;
        CollectionProxyComponent* component;
        dmMessage::URL url; // for reporting errors only
        dmScript::GetComponentFromLua(L, 1, COLLECTION_PROXY_EXT, (void**)&world, (void**)&component, &url);


        const char* path = 0;
        if (!lua_isnil(L, 2))
        {
            path = luaL_checkstring(L, 2);

            // check that the path is a .collectionc
            const char* ext = dmResource::GetExtFromPath(path);
            if (!ext || strcmp(ext, "collectionc") != 0)
            {
                return luaL_error(L, "Trying to set '%s' as collection to '%s:%s#%s'. Only .collectionc resources are allowed",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
            }
        }

        SetCollectionForProxyPathResult result = CollectionProxySetCollectionPath(world, component, path);

        switch (result)
        {
            case SET_COLLECTION_PATH_RESULT_OK:
                lua_pushboolean(L, 1);
                lua_pushnil(L);
                break;
            case SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING:
                lua_pushboolean(L, 0);
                lua_pushnumber(L, SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING);
                dmLogError("Cannot set collection `%s` for a collectionproxy '%s:%s#%s' while it's loading",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
                break;
            case SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED:
                lua_pushboolean(L, 0);
                lua_pushnumber(L, SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED);
                dmLogError("Cannot set collection `%s` for the already loaded collectionproxy '%s:%s#%s'",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
                break;
            case SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED:
                lua_pushboolean(L, 0);
                lua_pushnumber(L, SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED);
                dmLogError("Cannot set collection `%s` for a collectionproxy '%s:%s#%s' which isn't excluded",
                                        path,
                                        dmMessage::GetSocketName(url.m_Socket),
                                        dmHashReverseSafe64(url.m_Path),
                                        dmHashReverseSafe64(url.m_Fragment));
                break;
        }

        return 2;
    }

    static const luaL_reg Module_methods[] =
    {
        {"missing_resources", CollectionProxy_MissingResources},
        {"get_resources", CollectionProxy_GetResources},
        {"set_collection", CollectionProxy_SetCollection},
        {0, 0}
    };

    /*# collection proxy is loading now
     * It's impossible to change the collection while the collection proxy is loading.
     * @name collectionproxy.RESULT_LOADING
     * @constant
     */

    /*# collection proxy is already loaded
     * It's impossible to change the collection if the collection is already loaded.
     * @name collectionproxy.RESULT_ALREADY_LOADED
     * @constant
     */

    /*# collection proxy isn't excluded
     * It's impossible to change the collection for a proxy that isn't excluded.
     * @name collectionproxy.RESULT_NOT_EXCLUDED
     * @constant
     */

    static void LuaInit(lua_State* L)
    {
        int top = lua_gettop(L);
        luaL_register(L, "collectionproxy", Module_methods);

        lua_pushnumber(L, (lua_Number) SET_COLLECTION_PATH_RESULT_COLLECTION_LOADING);
        lua_setfield(L, -2, "RESULT_LOADING");


        lua_pushnumber(L, (lua_Number) SET_COLLECTION_PATH_RESULT_COLLECTION_ALREADY_LOADED);
        lua_setfield(L, -2, "RESULT_ALREADY_LOADED");


        lua_pushnumber(L, (lua_Number) SET_COLLECTION_PATH_RESULT_COLLECTION_NOT_EXCLUDED);
        lua_setfield(L, -2, "RESULT_NOT_EXCLUDED");

        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }

    void ScriptCollectionProxyFinalize(const ScriptLibContext& context)
    {
    }

    void ScriptCollectionProxyRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
    }

    /*# sets the time-step for update
     *
     * Post this message to a collection-proxy-component to modify the time-step used when updating the collection controlled by the proxy.
     * The time-step is modified by a scaling `factor` and can be incremented either continuously or in discrete steps.
     *
     * The continuous mode can be used for slow-motion or fast-forward effects.
     *
     * The discrete mode is only useful when scaling the time-step to pass slower than real time (`factor` is below 1).
     * The time-step will then be set to 0 for as many frames as the scaling demands and then take on the full real-time-step for one frame,
     * to simulate pulses. E.g. if `factor` is set to `0.1` the time-step would be 0 for 9 frames, then be 1/60 for one
     * frame, 0 for 9 frames, and so on. The result in practice is that the game looks like it's updated at a much lower frequency than 60 Hz,
     * which can be useful for debugging when each frame needs to be inspected.
     *
     * @message
     * @name set_time_step
     * @param factor [type:number] time-step scaling factor
     * @param mode [type:number] time-step mode: 0 for continuous and 1 for discrete
     * @examples
     *
     * The examples assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * Update the collection twice as fast:
     *
     * ```lua
     * msg.post("#proxy", "set_time_step", {factor = 2, mode = 0})
     * ```
     *
     * Update the collection twice as slow:
     *
     * ```lua
     * msg.post("#proxy", "set_time_step", {factor = 0.5, mode = 0})
     * ```
     *
     * Simulate 1 FPS for the collection:
     *
     * ```lua
     * msg.post("#proxy", "set_time_step", {factor = 1/60, mode = 1})
     * ```
     */

    /*# tells a collection proxy to start loading the referenced collection
     *
     * Post this message to a collection-proxy-component to start the loading of the referenced collection.
     * When the loading has completed, the message [ref:proxy_loaded] will be sent back to the script.
     *
     * A loaded collection must be initialized (message [ref:init]) and enabled (message [ref:enable]) in order to be simulated and drawn.
     *
     * @message
     * @name load
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("start_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- enable the collection and let the loader know
     *         msg.post(sender, "enable")
     *         msg.post(self.loader, message_id)
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to start asynchronous loading of the referenced collection
     *
     * Post this message to a collection-proxy-component to start background loading of the referenced collection.
     * When the loading has completed, the message [ref:proxy_loaded] will be sent back to the script.
     *
     * A loaded collection must be initialized (message [ref:init]) and enabled (message [ref:enable]) in order to be simulated and drawn.
     *
     * @message
     * @name async_load
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("start_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "async_load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- enable the collection and let the loader know
     *         msg.post(sender, "enable")
     *         msg.post(self.loader, message_id)
     *     end
     * end
     * ```
     */

    /*# reports that a collection proxy has loaded its referenced collection
     *
     * This message is sent back to the script that initiated a collection proxy load when the referenced
     * collection is loaded. See documentation for [ref:load] for examples how to use.
     *
     * @message
     * @name proxy_loaded
     */

    /*# tells a collection proxy to initialize the loaded collection
     * Post this message to a collection-proxy-component to initialize the game objects and components in the referenced collection.
     * Sending [ref:enable] to an uninitialized collection proxy automatically initializes it.
     * The [ref:init] message simply provides a higher level of control.
     *
     * @message
     * @name init
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("load_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- only initialize the proxy at this point since we want to enable it at a later time for some reason
     *         msg.post(sender, "init")
     *         -- let loader know
     *         msg.post(self.loader, message_id)
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to enable the referenced collection
     * Post this message to a collection-proxy-component to enable the referenced collection, which in turn enables the contained game objects and components.
     * If the referenced collection was not initialized prior to this call, it will automatically be initialized.
     *
     * @message
     * @name enable
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assume the script belongs to an instance with collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("start_level") then
     *         -- some script tells us to start loading the level
     *         msg.post("#proxy", "load")
     *         -- store sender for later notification
     *         self.loader = sender
     *     elseif message_id == hash("proxy_loaded") then
     *         -- enable the collection and let the loader know
     *         msg.post(sender, "enable")
     *         msg.post(self.loader, "level_started")
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to disable the referenced collection
     * Post this message to a collection-proxy-component to disable the referenced collection, which in turn disables the contained game objects and components.
     *
     * @message
     * @name disable
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("end_level") then
     *         local proxy = msg.url("#proxy")
     *         msg.post(proxy, "disable")
     *         msg.post(proxy, "final")
     *         msg.post(proxy, "unload")
     *         -- store sender for later notification
     *         self.unloader = sender
     *     elseif message_id == hash("proxy_unloaded") then
     *         -- let unloader know
     *         msg.post(self.unloader, "level_ended")
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to finalize the referenced collection
     * Post this message to a collection-proxy-component to finalize the referenced collection, which in turn finalizes the contained game objects and components.
     *
     * @message
     * @name final
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("end_level") then
     *         local proxy = msg.url("#proxy")
     *         msg.post(proxy, "disable")
     *         msg.post(proxy, "final")
     *         msg.post(proxy, "unload")
     *         -- store sender for later notification
     *         self.unloader = sender
     *     elseif message_id == hash("proxy_unloaded") then
     *         -- let unloader know
     *         msg.post(self.unloader, "level_ended")
     *     end
     * end
     * ```
     */

    /*# tells a collection proxy to start unloading the referenced collection
     *
     * Post this message to a collection-proxy-component to start the unloading of the referenced collection.
     * When the unloading has completed, the message [ref:proxy_unloaded] will be sent back to the script.
     *
     * @message
     * @name unload
     * @examples
     *
     * In this example we use a collection proxy to load/unload a level (collection).
     *
     * The example assumes the script belongs to an instance with a collection-proxy-component with id "proxy".
     *
     * ```lua
     * function on_message(self, message_id, message, sender)
     *     if message_id == hash("end_level") then
     *         local proxy = msg.url("#proxy")
     *         msg.post(proxy, "disable")
     *         msg.post(proxy, "final")
     *         msg.post(proxy, "unload")
     *         -- store sender for later notification
     *         self.unloader = sender
     *     elseif message_id == hash("proxy_unloaded") then
     *         -- let unloader know
     *         msg.post(self.unloader, "level_ended")
     *     end
     * end
     * ```
     */

    /*# reports that a collection proxy has unloaded its referenced collection
     *
     * This message is sent back to the script that initiated an unload with a collection proxy when
     * the referenced collection is unloaded. See documentation for [ref:unload] for examples how to use.
     *
     * @message
     * @name proxy_unloaded
     */

    /*# return an indexed table of all the resources of a collection proxy
     *
     * return an indexed table of resources for a collection proxy where the
     * referenced collection has been excluded using LiveUpdate. Each entry is a
     * hexadecimal string that represents the data of the specific resource.
     * This representation corresponds with the filename for each individual
     * resource that is exported when you bundle an application with LiveUpdate
     * functionality.
     *
     * @namespace collectionproxy
     * @name collectionproxy.get_resources
     * @param collectionproxy [type:url] the collectionproxy to check for resources.
     * @return resources [type:table] the resources, or an empty list if the
     * collection was not excluded.
     *
     * @examples
     *
     * ```lua
     * local function print_resources(self, cproxy)
     *     local resources = collectionproxy.get_resources(cproxy)
     *     for _, v in ipairs(resources) do
     *         print("Resource: " .. v)
     *     end
     * end
     * ```
     */

    /*# return an array of missing resources for a collection proxy
     *
     * return an array of missing resources for a collection proxy. Each
     * entry is a hexadecimal string that represents the data of the specific
     * resource. This representation corresponds with the filename for each
     * individual resource that is exported when you bundle an application with
     * LiveUpdate functionality. It should be considered good practise to always
     * check whether or not there are any missing resources in a collection proxy
     * before attempting to load the collection proxy.
     *
     * @namespace collectionproxy
     * @name collectionproxy.missing_resources
     * @param collectionproxy [type:url] the collectionproxy to check for missing
     * resources.
     * @return resources [type:table] the missing resources
     *
     * @examples
     *
     * ```lua
     * function init(self)
     * end
     *
     * local function callback(self, id, response)
     *     local expected = self.resources[id]
     *     if response ~= nil and response.status == 200 then
     *         print("Successfully downloaded resource: " .. expected)
     *         resource.store_resource(response.response)
     *     else
     *         print("Failed to download resource: " .. expected)
     *         -- error handling
     *     end
     * end
     *
     * local function download_resources(self, cproxy)
     *     self.resources = {}
     *     local resources = collectionproxy.missing_resources(cproxy)
     *     for _, v in ipairs(resources) do
     *         print("Downloading resource: " .. v)
     *
     *         local uri = "http://example.defold.com/" .. v
     *         local id = http.request(uri, "GET", callback)
     *         self.resources[id] = v
     *     end
     * end
     * ```
     */
};
