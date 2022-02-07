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

namespace dmFacebookStub
{

static int Facebook_ThrowError(lua_State* L)
{
    return luaL_error(L, "facebook has been removed from core, please read /builtins/docs/facebook.md for more information.");
}

static const luaL_reg Facebook_methods[] =
{
    {"login", Facebook_ThrowError},
    {"logout", Facebook_ThrowError},
    {"access_token", Facebook_ThrowError},
    {"permissions", Facebook_ThrowError},
    {"request_read_permissions", Facebook_ThrowError},
    {"request_publish_permissions", Facebook_ThrowError},
    {"me", Facebook_ThrowError},
    {"post_event", Facebook_ThrowError},
    {"enable_event_usage", Facebook_ThrowError},
    {"disable_event_usage", Facebook_ThrowError},
    {"show_dialog", Facebook_ThrowError},
    {"login_with_read_permissions", Facebook_ThrowError},
    {"login_with_publish_permissions", Facebook_ThrowError},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    lua_getglobal(L, "facebook");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_register(L, "facebook", Facebook_methods);
    }
    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result Facebook_AppInitialize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Facebook_AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Facebook_Initialize(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(FacebookExt, "Facebook", Facebook_AppInitialize, Facebook_AppFinalize, Facebook_Initialize, 0, 0, 0)

} // namespace
