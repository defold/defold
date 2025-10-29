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
 *
 * @name sys.set_render_enabled
 * @param enable [type:bool] true if rendering should be enabled
 *
 * @examples
 *
 * Disable rendering
 *
 * ```lua
 * sys.set_render_enabled(false)
 * ```
 *
 * Enable rendering
 *
 * ```lua
 * sys.set_render_enabled(true)
 * ```
 *
 */
static int EngineSys_SetRenderEnabled(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    bool enable = true;
    if (lua_isboolean(L, -1))
    {
        enable = lua_toboolean(L, -1);
    }
    else
    {
        return DM_LUA_ERROR("Expected boolean as first argument");
    }

    dmEngine::SetRenderEnable(g_Engine, enable);
    return 0;
}

static const luaL_reg EngineSys_methods[] =
{
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
