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

#include <assert.h>
#include <stdlib.h>

#include "script_camera.h"

#include "../gamesys.h"
#include "../gamesys_private.h"
#include <gamesys/camera_ddf.h>

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    /*# Camera API documentation
     *
     * Functions for interacting with camera components.
     *
     * @document
     * @name Camera
     * @namespace camera
     * @language Lua
     */

    #define LIB_NAME "camera"

    /** DEPRECATED! makes camera active
    *
    * @name camera.acquire_focus
    * @param url [type:string|hash|url] url of camera component
    * @examples
    * ```lua
    * camera.acquire_focus("/observer#main_camera")
    * ```
    */
    static int Camera_AcquireFocus(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmGamesysDDF::AcquireCameraFocus msg;
        dmMessage::Post(&sender, &receiver, dmGamesysDDF::AcquireCameraFocus::m_DDFDescriptor->m_NameHash, 0, 0, (uintptr_t)dmGamesysDDF::AcquireCameraFocus::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    /** DEPRECATED! deactivate camera
    *
    * @name camera.release_focus
    * @param url [type:string|hash|url] url of camera component
    * @examples
    * ```lua
    * camera.release_focus("/observer#main_camera")
    * ```
    */
    static int Camera_ReleaseFocus(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmGamesysDDF::ReleaseCameraFocus msg;
        dmMessage::Post(&sender, &receiver, dmGamesysDDF::ReleaseCameraFocus::m_DDFDescriptor->m_NameHash, 0, 0, (uintptr_t)dmGamesysDDF::ReleaseCameraFocus::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    static const luaL_reg ScriptCamera_methods[] =
    {
        {"acquire_focus", Camera_AcquireFocus},
        {"release_focus", Camera_ReleaseFocus},
        {0, 0}
    };

    void ScriptCameraRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        int top = lua_gettop(L);

        luaL_register(L, LIB_NAME, ScriptCamera_methods);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }
}
