// Copyright 2020 The Defold Foundation
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

#include <dmsdk/extension/extension.h>
#include <assert.h>

namespace dmIACStub
{

static int IAC_ThrowError(lua_State* L)
{
    return luaL_error(L, "iac has been removed from core, please read /builtins/docs/iac.md for more information.");
}

static const luaL_reg IAC_methods[] =
{
    {"set_listener", IAC_ThrowError},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    lua_getglobal(L, "iac");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_register(L, "iac", IAC_methods);
    }
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result IAC_AppInitialize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result IAC_AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result IAC_Initialize(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(IACExt, "IAC", IAC_AppInitialize, IAC_AppFinalize, IAC_Initialize, 0, 0, 0)

} // namespace
