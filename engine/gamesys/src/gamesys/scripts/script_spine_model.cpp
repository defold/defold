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

#include <dmsdk/extension/extension.hpp>

namespace dmGamesystem
{

static int SpineModel_ThrowError(lua_State* L)
{
    return luaL_error(L, "spine module support has been removed from core. Support has been moved to https://github.com/defold/extension-spine");
}

static const luaL_reg SPINE_COMP_FUNCTIONS[] =
{
        {"play",    SpineModel_ThrowError},
        {"play_anim", SpineModel_ThrowError},
        {"cancel",  SpineModel_ThrowError},
        {"get_go",  SpineModel_ThrowError},
        {"set_skin",  SpineModel_ThrowError},
        {"set_ik_target_position", SpineModel_ThrowError},
        {"set_ik_target",   SpineModel_ThrowError},
        {"reset_ik_target",        SpineModel_ThrowError},
        {"set_constant",    SpineModel_ThrowError},
        {"reset_constant",  SpineModel_ThrowError},
        {0, 0}
};

static void SpineModel_ScriptRegister(lua_State* L)
{
    luaL_register(L, "spine", SPINE_COMP_FUNCTIONS);
    lua_pop(L, 1);
}

static dmExtension::Result SpineModel_AppInitialize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result SpineModel_AppFinalize(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result SpineModel_Initialize(dmExtension::Params* params)
{
    SpineModel_ScriptRegister(params->m_L);
    return dmExtension::RESULT_OK;
}

DM_DECLARE_EXTENSION(SpineExtStub, "SpineExtStub", SpineModel_AppInitialize, SpineModel_AppFinalize, SpineModel_Initialize, 0, 0, 0)

} // namespace
