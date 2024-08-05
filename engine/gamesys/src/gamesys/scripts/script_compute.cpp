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
#include "script_material.h"

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
        dmResource::HFactory      m_Factory;
        dmArray<dmVMath::Vector4> m_ScratchValues;
    } g_ComputeModule;

    static inline ComputeResource* CheckComputeResource(lua_State* L, int index)
    {
        dmhash_t path_hash = dmScript::CheckHashOrString(L, index);
        return (ComputeResource*) CheckResource(L, g_ComputeModule.m_Factory, path_hash, "computec");
    }

    /*# gets texture samplers from a compute program
     * Returns a table of all the texture samplers in the compute program. This function will return all the texture samplers
     * that are available, even the ones that have not been specified in the compute resource.
     *
     * @name compute.get_samplers
     *
     * @param path [type:hash|string] The path to the resource
     * @return table [type:table] A table of tables, where each entry contains info about the texture samplers:
     *
     * `name`
     * : [type:hash] the hashed name of the texture sampler
     *
     * `u_wrap`
     * : [type:number] the u wrap mode of the texture sampler. Supported values:
     *
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `graphics.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `graphics.TEXTURE_WRAP_REPEAT`
     *
     * `v_wrap`
     * : [type:number] the v wrap mode of the texture sampler. Supported values:
     *
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `graphics.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `graphics.TEXTURE_WRAP_REPEAT`
     *
     * `min_filter`
     * : [type:number] the min filter mode of the texture sampler. Supported values:
     *
     *   - `graphics.TEXTURE_FILTER_DEFAULT`
     *   - `graphics.TEXTURE_FILTER_NEAREST`
     *   - `graphics.TEXTURE_FILTER_LINEAR`
     *   - `graphics.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST`
     *   - `graphics.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR`
     *   - `graphics.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST`
     *   - `graphics.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR`
     *
     * `mag_filter`
     * : [type:number] the mag filter mode of the texture sampler
     *
     *   - `graphics.TEXTURE_FILTER_DEFAULT`
     *   - `graphics.TEXTURE_FILTER_NEAREST`
     *   - `graphics.TEXTURE_FILTER_LINEAR`
     *
     * `max_anisotropy`
     * : [type:number] the max anisotropy of the texture sampler
     *
     * @examples
     * Get the texture samplers from a compute program resource
     *
     * ```lua
     * function init(self)
     *     local samplers = compute.get_samplers("/my_compute.computec")
     * end
     * ```
     */
    static int Compute_GetSamplers(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        ComputeResource* compute_res = CheckComputeResource(L, 1);

        uint32_t sampler_index = 1;

        lua_newtable(L);

        for (int i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmRender::HSampler sampler = dmRender::GetComputeProgramSampler(compute_res->m_Program, i);

            if (sampler)
            {
                lua_pushinteger(L, (lua_Integer) sampler_index++);
                lua_newtable(L);

                PushSampler(L, sampler);

                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    /*# gets shader constants from a compute program
     * Returns a table of all the shader contstants in the compute program.
     *
     * @name compute.get_constants
     *
     * @param path [type:hash|string] The path to the resource
     * @return table [type:table] A table of tables, where each entry contains info about the shader constants:
     *
     * `name`
     * : [type:hash] the hashed name of the constant
     *
     * `type`
     * : [type:number] the type of the constant. Supported values:
     *
     *   - `material.CONSTANT_TYPE_USER`
     *   - `material.CONSTANT_TYPE_USER_MATRIX4`
     *   - `material.CONSTANT_TYPE_VIEWPROJ`
     *   - `material.CONSTANT_TYPE_WORLD`
     *   - `material.CONSTANT_TYPE_TEXTURE`
     *   - `material.CONSTANT_TYPE_VIEW`
     *   - `material.CONSTANT_TYPE_PROJECTION`
     *   - `material.CONSTANT_TYPE_NORMAL`
     *   - `material.CONSTANT_TYPE_WORLDVIEW`
     *   - `material.CONSTANT_TYPE_WORLDVIEWPROJ`
     *
     * `value`
     * : [type:vmath.vector4|vmath.matrix4] the value(s) of the constant. If the constant is an array, the value will be a table of vmath.vector4 or vmath.matrix4 if the type is `material.CONSTANT_TYPE_USER_MATRIX4`.
     *
     * @examples
     * Get the shader constants from a compute program resource
     *
     * ```lua
     * function init(self)
     *     local constants = compute.get_constants("/my_compute.computec")
     * end
     * ```
     */
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

    /*# gets textures associated with a compute program
     * Returns a table of all the textures from the compute progrma.
     *
     * @name compute.get_textures
     *
     * @param path [type:hash|string] The path to the resource
     * @return table [type:table] A table of tables, where each entry contains info about the compute textures:
     *
     * `path`
     * : [type:hash] the resource path of the texture. Only available if the texture is a resource.
     *
     * `handle`
     * : [type:hash] the runtime handle of the texture.
     *
     * `width`
     * : [type:number] the width of the texture
     *
     * `height`
     * : [type:number] the height of the texture
     *
     * `depth`
     * : [type:number] the depth of the texture. Corresponds to the number of layers in an array texture.
     *
     * `mipmaps`
     * : [type:number] the number of mipmaps in the texture
     *
     * `type`
     * : [type:number] the type of the texture. Supported values:
     *
     *   - `graphics.TEXTURE_TYPE_2D`
     *   - `graphics.TEXTURE_TYPE_2D_ARRAY`
     *   - `graphics.TEXTURE_TYPE_CUBE_MAP`
     *   - `graphics.TEXTURE_TYPE_IMAGE_2D`
     *
     * `flags`
     * : [type:number] the flags of the texture. This field is a bit mask of these supported flags:
     *
     *   - `graphics.TEXTURE_USAGE_HINT_NONE`
     *   - `graphics.TEXTURE_USAGE_HINT_SAMPLE`
     *   - `graphics.TEXTURE_USAGE_HINT_MEMORYLESS`
     *   - `graphics.TEXTURE_USAGE_HINT_STORAGE`
     *   - `graphics.TEXTURE_USAGE_HINT_INPUT`
     *   - `graphics.TEXTURE_USAGE_HINT_COLOR`
     *
     * @examples
     * Get the shader constants from a material specified as a resource property
     *
     * ```lua
     * function init(self)
     *     local constants = compute.get_constants("/my_compute.computec")
     * end
     * ```
     */
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

                PushTextureInfo(L, texture_res->m_Texture, compute_res->m_TextureResourcePaths[i]);
                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    /*# sets a texture sampler in a compute program
     * Sets a texture sampler in a compute program, if the sampler exists. Use this function to change the settings of a texture sampler.
     * To set an actual texture that should be bound to the sampler, use the `compute.set_texture` function instead.
     *
     * @name compute.set_sampler
     *
     * @param path [type:hash|string] The path to the resource
     * @param name [type:hash|string] The sampler name
     * @param args [type:table] A table of what to update in the sampler (partial updates are supported). Supported entries:
     *
     * `u_wrap`
     * : [type:number] the u wrap mode of the texture sampler. Supported values:
     *
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `graphics.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `graphics.TEXTURE_WRAP_REPEAT`
     *
     * `v_wrap`
     * : [type:number] the v wrap mode of the texture sampler. Supported values:
     *
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `graphics.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `graphics.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `graphics.TEXTURE_WRAP_REPEAT`
     *
     * `min_filter`
     * : [type:number] the min filter mode of the texture sampler. Supported values:
     *
     *   - `graphics.TEXTURE_FILTER_DEFAULT`
     *   - `graphics.TEXTURE_FILTER_NEAREST`
     *   - `graphics.TEXTURE_FILTER_LINEAR`
     *   - `graphics.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST`
     *   - `graphics.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR`
     *   - `graphics.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST`
     *   - `graphics.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR`
     *
     * `mag_filter`
     * : [type:number] the mag filter mode of the texture sampler
     *
     *   - `graphics.TEXTURE_FILTER_DEFAULT`
     *   - `graphics.TEXTURE_FILTER_NEAREST`
     *   - `graphics.TEXTURE_FILTER_LINEAR`
     *
     * `max_anisotropy`
     * : [type:number] the max anisotropy of the texture sampler
     *
     * @examples
     * Configures a sampler in a compute program
     *
     * ```lua
     * function init(self)
     *     compute.set_sampler("/my_compute.computec", "texture_sampler", { u_wrap = graphics.TEXTURE_WRAP_REPEAT, v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT })
     * end
     * ```
     */
    static int Compute_SetSampler(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        ComputeResource* compute_res = CheckComputeResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        uint32_t unit = dmRender::GetComputeProgramSamplerUnit(compute_res->m_Program, name_hash);
        if (unit == dmRender::INVALID_SAMPLER_UNIT)
        {
            return luaL_error(L, "Compute program sampler '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmRender::HSampler sampler = dmRender::GetComputeProgramSampler(compute_res->m_Program, unit);

        uint32_t location;
        dmGraphics::TextureType texture_type;
        dmGraphics::TextureWrap u_wrap, v_wrap;
        dmGraphics::TextureFilter min_filter, mag_filter;
        float max_anisotropy;
        dmRender::GetSamplerInfo(sampler, &name_hash, &texture_type, &location, &u_wrap, &v_wrap, &min_filter, &mag_filter, &max_anisotropy);

        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushvalue(L, 3);

        GetSamplerParametersFromLua(L, &u_wrap, &v_wrap, &min_filter, &mag_filter, &max_anisotropy);

        lua_pop(L, 1);

        dmRender::SetComputeProgramSampler(compute_res->m_Program, name_hash, unit, u_wrap, v_wrap, min_filter, mag_filter, max_anisotropy);

        return 0;
    }

    /*# sets a shader constant in a compute program
     * Sets a shader constant in a compute program, if the constant exists.
     *
     * @name compute.set_constant
     *
     * @param path [type:hash|string] The path to the resource
     * @param name [type:hash|string] The constant name
     * @param args [type:table] A table of what to update in the constant (a constant can be partially updated). Supported entries:
     *
     * `type`
     * : [type:number] the type of the constant. Supported values:
     *
     *   - `material.CONSTANT_TYPE_USER`
     *   - `material.CONSTANT_TYPE_USER_MATRIX4`
     *   - `material.CONSTANT_TYPE_VIEWPROJ`
     *   - `material.CONSTANT_TYPE_WORLD`
     *   - `material.CONSTANT_TYPE_TEXTURE`
     *   - `material.CONSTANT_TYPE_VIEW`
     *   - `material.CONSTANT_TYPE_PROJECTION`
     *   - `material.CONSTANT_TYPE_NORMAL`
     *   - `material.CONSTANT_TYPE_WORLDVIEW`
     *   - `material.CONSTANT_TYPE_WORLDVIEWPROJ`
     *
     * `value`
     * : [type:vmath.vector4|vmath.matrix4] the value(s) of the constant. If the shader constant is an array, the amount of values to update depends on how many values that are passed in the 'value' field.
     *
     * @examples
     * Set a shader constant in a compute program
     *
     * ```lua
     * function update(self)
     *     -- update the 'tint' constant
     *     compute.set_constant("/my_compute.computec", "tint", { value = vmath.vector4(1, 0, 0, 1) })
     *     -- change the type of the 'view_proj' constant to CONSTANT_TYPE_USER_MATRIX4 so the renderer can set our custom data
     *     compute.set_constant("/my_compute.computec", "view_proj", { value = self.my_view_proj, type = material.CONSTANT_TYPE_USER_MATRIX4 })
     * end
     * ```
     */
    static int Compute_SetConstant(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        ComputeResource* compute_res = CheckComputeResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmRender::HConstant constant;
        if (!dmRender::GetComputeProgramConstant(compute_res->m_Program, name_hash, constant))
        {
            return luaL_error(L, "Compute program constant '%s' not found", dmHashReverseSafe64(name_hash));
        }

        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushvalue(L, 3);

        g_ComputeModule.m_ScratchValues.SetSize(0);

        dmRenderDDF::MaterialDesc::ConstantType type_from_value;

        GetConstantValuesFromLua(L, &type_from_value, &g_ComputeModule.m_ScratchValues);

        if (g_ComputeModule.m_ScratchValues.Size() != 0)
        {
            dmRender::SetComputeProgramConstant(compute_res->m_Program, name_hash,
                g_ComputeModule.m_ScratchValues.Begin(), g_ComputeModule.m_ScratchValues.Size());
        }

        if (dmRender::GetConstantType(constant) != type_from_value)
        {
            dmRender::SetComputeProgramConstantType(compute_res->m_Program, name_hash, type_from_value);
        }

        lua_pop(L, 1);
        return 0;
    }

    /*# sets a texture sampler in a compute program
     * Sets a texture sampler in a compute program, if the sampler exists.
     *
     * @name compute.set_texture
     *
     * @param path [type:hash|string] The path to the resource
     * @param name [type:hash|string] The sampler name
     * @param args [type:table] A table of what to update. Supported entries:
     *
     * `texture`
     * : [type:hash|handle] the texture to set. Can be either a texture resource hash or a texture handle.
     *
     * @examples
     * Set a texture in a compute program from a resource
     *
     * ```lua
     * go.property("my_texture", resource.texture())
     *
     * function init(self)
     *     compute.set_texture("/my_compute.computec", "my_texture", { texture = self.my_texture })
     * end
     * ```
     */
    static int Compute_SetTexture(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        ComputeResource* compute_res = CheckComputeResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        uint32_t unit = dmRender::GetComputeProgramSamplerUnit(compute_res->m_Program, name_hash);
        if (unit == dmRender::INVALID_SAMPLER_UNIT)
        {
            return luaL_error(L, "Compute program sampler '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmhash_t texture_path = dmScript::CheckHashOrString(L, 3);
        TextureResource* texture_res = (TextureResource*) CheckResource(L, g_ComputeModule.m_Factory, texture_path, "texturec");

        dmGraphics::TextureType texture_type_in = dmGraphics::GetTextureType(texture_res->m_Texture);
        dmRender::HSampler sampler              = dmRender::GetComputeProgramSampler(compute_res->m_Program, unit);

        uint32_t location;
        dmGraphics::TextureType texture_type;
        dmGraphics::TextureWrap u_wrap, v_wrap;
        dmGraphics::TextureFilter min_filter, mag_filter;
        float max_anisotropy;
        dmRender::GetSamplerInfo(sampler, &name_hash, &texture_type, &location, &u_wrap, &v_wrap, &min_filter, &mag_filter, &max_anisotropy);

        if (texture_type != texture_type_in)
        {
            return luaL_error(L, "Texture type mismatch. Can't bind a %s texture to a %s sampler",
                dmGraphics::GetTextureTypeLiteral(texture_type_in),
                dmGraphics::GetTextureTypeLiteral(texture_type));
        }

        compute_res->m_TextureResourcePaths[unit] = texture_path;
        compute_res->m_SamplerNames[unit]         = name_hash;

        if (compute_res->m_Textures[unit] != texture_res)
        {
            if (compute_res->m_Textures[unit])
            {
                dmResource::Release(g_ComputeModule.m_Factory, compute_res->m_Textures[unit]);
            }
            dmResource::IncRef(g_ComputeModule.m_Factory, texture_res);
            compute_res->m_Textures[unit] = texture_res;
        }

        // Recalculate number of textures (similar to res_material.cpp) by checking which samplers exist
        compute_res->m_NumTextures = 0;
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            uint32_t unit = dmRender::GetComputeProgramSamplerUnit(compute_res->m_Program, compute_res->m_SamplerNames[i]);
            if (unit == dmRender::INVALID_SAMPLER_UNIT)
            {
                continue;
            }
            compute_res->m_NumTextures++;
        }

        return 0;
    }

    static const luaL_reg ScriptCompute_methods[] =
    {
        {"get_samplers",  Compute_GetSamplers},
        {"get_constants", Compute_GetConstants},
        {"get_textures",  Compute_GetTextures},
        {"set_sampler",   Compute_SetSampler},
        {"set_constant",  Compute_SetConstant},
        {"set_texture",   Compute_SetTexture},
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
