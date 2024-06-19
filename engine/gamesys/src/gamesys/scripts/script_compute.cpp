// Copyright 2020-2024 The Defold Foundation
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

#include "../gamesys.h"
#include "../gamesys_private.h"

#include "../resources/res_compute.h"
#include "../resources/res_texture.h"

#include "script_compute.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    /*# Compute API documentation
     *
     * Functions for interacting with compute programs.
     *
     * @document
     * @name Compute
     * @namespace compute
     */

    #define LIB_NAME "compute"

#ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT

    struct ComputeModule
    {
        dmResource::HFactory m_Factory;
    } g_ComputeModule;

    static inline ComputeResource* CheckComputeResource(lua_State* L, int index)
    {
        dmhash_t path_hash = dmScript::CheckHashOrString(L, index);
        return (ComputeResource*) CheckResource(L, g_ComputeModule.m_Factory, path_hash, "computec");
    }

    static int Compute_GetSamplers(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        ComputeResource* compute_res = CheckComputeResource(L, 1);

        uint32_t sampler_index = 1;

        lua_newtable(L);

        for (int i = 0; i < compute_res->m_NumTextures; ++i)
        {
            dmRender::HSampler sampler = dmRender::GetComputeProgramSampler(compute_res->m_Program, i);

            if (sampler)
            {
                lua_pushinteger(L, (lua_Integer) sampler_index++);
                lua_newtable(L);

                PushSampler(L, sampler, compute_res->m_Textures[i] ? compute_res->m_Textures[i]->m_Texture : 0);

                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    static int Compute_GetConstants(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        ComputeResource* compute_res = CheckComputeResource(L, 1);

        lua_newtable(L);

        uint32_t constant_index = 1;

        uint32_t constant_count = dmRender::GetComputeProgramConstantCount(compute_res->m_Program);
        for (int i = 0; i < constant_count; ++i)
        {
            dmhash_t name_hash;
            dmRender::GetComputeProgramConstantNameHash(compute_res->m_Program, i, &name_hash);

            dmRender::HConstant constant;

            if (dmRender::GetComputeProgramConstant(compute_res->m_Program, name_hash, constant))
            {
                lua_pushinteger(L, (lua_Integer) constant_index++);
                lua_newtable(L);

                PushRenderConstant(L, constant);

                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    static int Compute_GetTextures(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        ComputeResource* compute_res = CheckComputeResource(L, 1);

        uint32_t texture_index = 1;

        lua_newtable(L);

        for (int i = 0; i < compute_res->m_NumTextures; ++i)
        {
            TextureResource* texture_res = compute_res->m_Textures[i];
            if (texture_res)
            {
                lua_pushinteger(L, (lua_Integer) texture_index++);
                lua_newtable(L);

                PushTextureInfo(L, texture_res->m_Texture);
                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    static const luaL_reg ScriptCompute_methods[] =
    {
        {"get_samplers",  Compute_GetSamplers},
        {"get_constants", Compute_GetConstants},
        {"get_textures",  Compute_GetTextures},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        luaL_register(L, LIB_NAME, ScriptCompute_methods);
        lua_pop(L, 1);
    }

    void ScriptComputeRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
        g_ComputeModule.m_Factory = context.m_Factory;
    }

#else // !DM_HAVE_PLATFORM_COMPUTE_SUPPORT
    void ScriptComputeRegister(const ScriptLibContext& context)
    {
        // Not supported on this platform
    }
#endif
}
