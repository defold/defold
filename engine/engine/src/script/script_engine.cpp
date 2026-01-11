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


#include <dmsdk/script/script.h>
#include "../engine_private.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmEngine
{
    dmEngine::HEngine g_Engine = 0;

/*#
 * Enables engine throttling.
 *
 * @note It will automatically wake up on input events
 * @note It will automatically throttle again after the cooldown period
 * @note It skips entire update+render loop on the main thread. E.g loading of assets, callbacks from threads (http)
 * @note On threaded systems, Sound will continue to play any started non-streaming sounds. (e.g. looping background music)
 *
 * @name sys.set_engine_throttle
 * @param enable [type:bool] true if throttling should be enabled
 * @param cooldown [type:number] the time period to do update + render for (seconds)
 *
 * @examples
 *
 * Disable throttling
 *
 * ```lua
 * sys.set_engine_throttle(false)
 * ```
 *
 * Enable throttling
 *
 * ```lua
 * sys.set_engine_throttle(true, 1.5)
 * ```
 *
 */
static int EngineSys_SetEngineThrottle(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    bool enable = true;
    if (lua_isboolean(L, 1))
    {
        enable = lua_toboolean(L, 1);
    }
    else
    {
        return DM_LUA_ERROR("Expected boolean as first argument");
    }

    float cooldown = 0.0f;
    if (enable)
    {
        cooldown = luaL_checknumber(L, 2);
    }

    dmEngine::SetEngineThrottle(g_Engine, enable, cooldown);
    return 0;
}


/*#
 * Disables rendering
 *
 * @note It will will leave the back buffer as-is
 *
 * @name sys.set_render_enable
 * @param enable [type:bool] true if throttling should be enabled
 *
 * @examples
 *
 * Disable rendering
 *
 * ```lua
 * sys.set_render_enable(false)
 * ```
 *
 */
static int EngineSys_SetRenderEnabled(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    bool enable = true;
    if (lua_isboolean(L, 1))
    {
        enable = lua_toboolean(L, 1);
    }
    else
    {
        return DM_LUA_ERROR("Expected boolean as first argument");
    }

    dmEngine::SetRenderEnabled(enable);
    return 0;
}


static const luaL_reg EngineSys_methods[] =
{
    {"set_engine_throttle", EngineSys_SetEngineThrottle},
    {"set_render_enabled", EngineSys_SetRenderEnabled},
    {0, 0}
};

void ScriptSysEngineInitialize(lua_State* L, dmEngine::HEngine engine)
{
    g_Engine = engine;
    int top = lua_gettop(L);
    (void)top;

    luaL_register(L, "sys", EngineSys_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}


void ScriptSysEngineFinalize(lua_State* L, dmEngine::HEngine engine)
{
    g_Engine = 0;
}

}
