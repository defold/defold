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

#include "../resources/res_material.h"
#include "../resources/res_texture.h"

#include "script_material.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    /*# Material API documentation
     *
     * Functions for interacting with materials.
     *
     * @document
     * @name Material
     * @namespace material
     */

    #define LIB_NAME "material"

    struct MaterialModule
    {
        dmResource::HFactory m_Factory;
    } g_MaterialModule;

    static inline MaterialResource* CheckMaterialResource(lua_State* L, int index)
    {
        dmhash_t path_hash = dmScript::CheckHashOrString(L, index);
        return (MaterialResource*) CheckResource(L, g_MaterialModule.m_Factory, path_hash, "materialc");
    }

    static void PushVertexAttribute(lua_State* L, const dmGraphics::VertexAttribute* attribute, const uint8_t* value_ptr)
    {
        float values[4] = {};
        VertexAttributeToFloats(attribute, value_ptr, values);

        if (attribute->m_ElementCount == 4)
        {
            dmVMath::Vector4 v(values[0], values[1], values[2], values[3]);
            dmScript::PushVector4(L, v);
        }
        else if (attribute->m_ElementCount == 3 || attribute->m_ElementCount == 2)
        {
            dmVMath::Vector3 v(values[0], values[1], values[2]);
            dmScript::PushVector3(L, v);
        }
        else if (attribute->m_ElementCount == 1)
        {
            lua_pushnumber(L, values[0]);
        }
        else
        {
            // We need to catch this error because it means there are type combinations
            // that we have added, but don't have support for here.
            assert("Not supported!");
        }
    }

    static int Material_GetVertexAttributes(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        MaterialResource* material_res = CheckMaterialResource(L, 1);

        const dmGraphics::VertexAttribute* attributes;
        uint32_t attribute_count;
        dmRender::GetMaterialProgramAttributes(material_res->m_Material, &attributes, &attribute_count);

        lua_newtable(L);

        for (uint32_t i = 0; i < attribute_count; ++i)
        {
            dmRender::MaterialProgramAttributeInfo info;
            dmRender::GetMaterialProgramAttributeInfo(material_res->m_Material, attributes[i].m_NameHash, info);

            lua_pushinteger(L, (lua_Integer) (i+1));
            lua_newtable(L);

            dmScript::PushHash(L, info.m_AttributeNameHash);
            lua_setfield(L, -2, "name");

            PushVertexAttribute(L, info.m_Attribute, info.m_ValuePtr);
            lua_setfield(L, -2, "value");

            lua_pushboolean(L, info.m_Attribute->m_Normalize);
            lua_setfield(L, -2, "normalize");

            lua_pushinteger(L, (lua_Integer) info.m_Attribute->m_DataType);
            lua_setfield(L, -2, "data_type");

            lua_pushinteger(L, (lua_Integer) info.m_Attribute->m_CoordinateSpace);
            lua_setfield(L, -2, "coordinate_space");

            lua_pushinteger(L, (lua_Integer) info.m_Attribute->m_SemanticType);
            lua_setfield(L, -2, "semantic_type");

            lua_rawset(L, -3);
        }


        return 1;
    }

    static int Material_GetSamplers(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        MaterialResource* material_res = CheckMaterialResource(L, 1);

        lua_newtable(L);

        for (int i = 0; i < material_res->m_NumTextures; ++i)
        {
            dmRender::HSampler sampler = dmRender::GetMaterialSampler(material_res->m_Material, i);

            if (sampler)
            {
                lua_pushinteger(L, (lua_Integer) (i+1));
                lua_newtable(L);

                PushSampler(L, sampler, material_res->m_Textures[i] ? material_res->m_Textures[i]->m_Texture : 0);

                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    static int Material_GetConstants(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        MaterialResource* material_res = CheckMaterialResource(L, 1);

        lua_newtable(L);

        uint32_t constant_count = dmRender::GetMaterialConstantCount(material_res->m_Material);
        for (int i = 0; i < constant_count; ++i)
        {
            dmhash_t name_hash;
            dmRender::GetMaterialConstantNameHash(material_res->m_Material, i, &name_hash);

            dmRender::HConstant constant;

            if (dmRender::GetMaterialProgramConstant(material_res->m_Material, name_hash, constant))
            {
                lua_pushinteger(L, (lua_Integer) (i+1));
                lua_newtable(L);

                PushRenderConstant(L, constant);

                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    static int Material_GetTextures(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        MaterialResource* material_res = CheckMaterialResource(L, 1);

        lua_newtable(L);

        for (int i = 0; i < material_res->m_NumTextures; ++i)
        {
            TextureResource* texture_res = material_res->m_Textures[i];
            if (texture_res)
            {
                lua_pushinteger(L, (lua_Integer) (i+1));
                lua_newtable(L);

                PushTextureInfo(L, texture_res->m_Texture);
                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    static const luaL_reg ScriptCompute_methods[] =
    {
        {"get_vertex_attributes", Material_GetVertexAttributes},
        {"get_samplers",          Material_GetSamplers},
        {"get_constants",         Material_GetConstants},
        {"get_textures",          Material_GetTextures},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        luaL_register(L, LIB_NAME, ScriptCompute_methods);
        lua_pop(L, 1);
    }

    void ScriptMaterialRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
        g_MaterialModule.m_Factory = context.m_Factory;
    }
}
