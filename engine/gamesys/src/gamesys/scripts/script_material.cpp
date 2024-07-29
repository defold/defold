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

#include <graphics/graphics_ddf.h>

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

    /*# gets vertex attributes from a material
     * Returns a table of all the vertex attributes in the material. This function will return all the vertex attributes
     * that are used in the vertex shader of the material.
     *
     * @name material.get_vertex_attributes
     *
     * @param path [type:hash|string] The path to the resource
     * @return table [type:table] A table of tables, where each entry contains info about the vertex attributes:
     *
     * `name`
     * : [type:hash] the hashed name of the vertex attribute
     *
     * `value`
     * : [type:vmath.vector4|vmath.vector3|number] the value of the vertex attribute
     *
     * `normalize`
     * : [type:boolean] whether the value is normalized when passed into the shader
     *
     * `data_type`
     * : [type:number] the data type of the vertex attribute. Supported values:
     *
     *   - `material.DATA_TYPE_BYTE`
     *   - `material.DATA_TYPE_UNSIGNED_BYTE`
     *   - `material.DATA_TYPE_SHORT`
     *   - `material.DATA_TYPE_UNSIGNED_SHORT`
     *   - `material.DATA_TYPE_INT`
     *   - `material.DATA_TYPE_UNSIGNED_INT`
     *   - `material.DATA_TYPE_FLOAT`
     *
     * `coordinate_space`
     * : [type:number] the coordinate space of the vertex attribute. Supported values:
     *
     *   - `material.COORDINATE_SPACE_WORLD`
     *   - `material.COORDINATE_SPACE_LOCAL`
     *
     * `semantic_type`
     * : [type:number] the semantic type of the vertex attribute. Supported values:
     *
     *   - `material.SEMANTIC_TYPE_NONE`
     *   - `material.SEMANTIC_TYPE_POSITION`
     *   - `material.SEMANTIC_TYPE_TEXCOORD`
     *   - `material.SEMANTIC_TYPE_PAGE_INDEX`
     *   - `material.SEMANTIC_TYPE_COLOR`
     *   - `material.SEMANTIC_TYPE_NORMAL`
     *   - `material.SEMANTIC_TYPE_TANGENT`
     *
     * @examples
     * Get the vertex attributes from a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     local vertex_attributes = material.get_vertex_attributes(self.my_material)
     * end
     * ```
     */
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

    /*# gets texture samplers from a material
     * Returns a table of all the texture samplers in the material. This function will return all the texture samplers
     * that are used in both the vertex and the fragment shaders.
     *
     * @name material.get_samplers
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
     *   - `material.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `material.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `material.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `material.TEXTURE_WRAP_REPEAT`
     *
     * `v_wrap`
     * : [type:number] the v wrap mode of the texture sampler. Supported values:
     *
     *   - `material.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `material.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `material.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `material.TEXTURE_WRAP_REPEAT`
     *
     * `min_filter`
     * : [type:number] the min filter mode of the texture sampler. Supported values:
     *
     *   - `material.TEXTURE_FILTER_DEFAULT`
     *   - `material.TEXTURE_FILTER_NEAREST`
     *   - `material.TEXTURE_FILTER_LINEAR`
     *   - `material.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST`
     *   - `material.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR`
     *   - `material.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST`
     *   - `material.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR`
     *
     * `mag_filter`
     * : [type:number] the mag filter mode of the texture sampler
     *
     *   - `material.TEXTURE_FILTER_DEFAULT`
     *   - `material.TEXTURE_FILTER_NEAREST`
     *   - `material.TEXTURE_FILTER_LINEAR`
     *
     * `max_anisotropy`
     * : [type:number] the max anisotropy of the texture sampler
     *
     * @examples
     * Get the texture samplers from a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     local samplers = material.get_samplers(self.my_material)
     * end
     * ```
     */
    static int Material_GetSamplers(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        MaterialResource* material_res = CheckMaterialResource(L, 1);

        lua_newtable(L);

        for (int i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmRender::HSampler sampler = dmRender::GetMaterialSampler(material_res->m_Material, i);

            if (sampler)
            {
                lua_pushinteger(L, (lua_Integer) (i+1));
                lua_newtable(L);

                PushSampler(L, sampler);

                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    /*# gets shader constants from a material
     * Returns a table of all the shader contstants in the material. This function will return all the shader constants
     * that are used in both the vertex and the fragment shaders.
     *
     * @name material.get_constants
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
     * Get the shader constants from a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     local constants = material.get_constants(self.my_material)
     * end
     * ```
     */
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

    /*# gets textures associated with a material
     * Returns a table of all the textures from the material.
     *
     * @name material.get_textures
     *
     * @param path [type:hash|string] The path to the resource
     * @return table [type:table] A table of tables, where each entry contains info about the material textures:
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
     *   - `material.TEXTURE_TYPE_2D`
     *   - `material.TEXTURE_TYPE_2D_ARRAY`
     *   - `material.TEXTURE_TYPE_CUBE_MAP`
     *   - `material.TEXTURE_TYPE_IMAGE_2D`
     *
     * `flags`
     * : [type:number] the flags of the texture. This field is a bit mask of these supported flags:
     *
     *   - `material.TEXTURE_USAGE_HINT_NONE`
     *   - `material.TEXTURE_USAGE_HINT_SAMPLE`
     *   - `material.TEXTURE_USAGE_HINT_MEMORYLESS`
     *   - `material.TEXTURE_USAGE_HINT_STORAGE`
     *   - `material.TEXTURE_USAGE_HINT_INPUT`
     *   - `material.TEXTURE_USAGE_HINT_COLOR`
     *
     * @examples
     * Get the shader constants from a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     local constants = material.get_constants(self.my_material)
     * end
     * ```
     */
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

                PushTextureInfo(L, texture_res->m_Texture, material_res->m_TextureResourcePaths[i]);
                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    /*# sets a vertex attribute in a material
     * Sets a vertex attribute in a material, if the vertex attribute exists.
     *
     * @name material.set_vertex_attribute
     *
     * @param path [type:hash|string] The path to the resource
     * @param name [type:hash|string] The vertex attribute
     * @param args [type:table] A table of what to update in the vertex attribute (partial updates are supported). Supported entries:
     *
     * `value`
     * : [type:vmath.vector4|vmath.vector3|number] the value of the vertex attribute
     *
     * `normalize`
     * : [type:boolean] whether the value is normalized when passed into the shader
     *
     * `data_type`
     * : [type:number] the data type of the vertex attribute. Supported values:
     *
     *   - `material.DATA_TYPE_BYTE`
     *   - `material.DATA_TYPE_UNSIGNED_BYTE`
     *   - `material.DATA_TYPE_SHORT`
     *   - `material.DATA_TYPE_UNSIGNED_SHORT`
     *   - `material.DATA_TYPE_INT`
     *   - `material.DATA_TYPE_UNSIGNED_INT`
     *   - `material.DATA_TYPE_FLOAT`
     *
     * `coordinate_space`
     * : [type:number] the coordinate space of the vertex attribute. Supported values:
     *
     *   - `material.COORDINATE_SPACE_WORLD`
     *   - `material.COORDINATE_SPACE_LOCAL`
     *
     * `semantic_type`
     * : [type:number] the semantic type of the vertex attribute. Supported values:
     *
     *   - `material.SEMANTIC_TYPE_NONE`
     *   - `material.SEMANTIC_TYPE_POSITION`
     *   - `material.SEMANTIC_TYPE_TEXCOORD`
     *   - `material.SEMANTIC_TYPE_PAGE_INDEX`
     *   - `material.SEMANTIC_TYPE_COLOR`
     *   - `material.SEMANTIC_TYPE_NORMAL`
     *   - `material.SEMANTIC_TYPE_TANGENT`
     *
     *
     * @examples
     * Configures a vertex attribute in a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     material.set_sampler(self.my_material, "tint_attribute",
     *         { value         = vmath.vec4(1, 0, 0, 1),
     *           semantic_type = material.SEMANTIC_TYPE_COLOR,
     *           data_type     = material.DATA_TYPE_FLOAT })
     * end
     * ```
     */
    static int Material_SetVertexAttribute(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmRender::MaterialProgramAttributeInfo info;
        if (!dmRender::GetMaterialProgramAttributeInfo(material_res->m_Material, name_hash, info))
        {
            return luaL_error(L, "Material attribute '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmGraphics::VertexAttribute attribute = {};
        memcpy(&attribute, &info.m_Attribute, sizeof(dmGraphics::VertexAttribute));

        uint8_t values_bytes[sizeof(dmVMath::Vector4)] = {};
        float values_float[4] = {};

        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushvalue(L, 3);

        // parse value
        {
            lua_getfield(L, -1, "value");
            if (!lua_isnil(L, -1))
            {
                dmVMath::Vector4* v4 = dmScript::CheckVector4(L, -1);
                values_float[0] = v4->getX();
                values_float[1] = v4->getY();
                values_float[2] = v4->getZ();
                values_float[3] = v4->getW();
            }
            lua_pop(L, 1);
        }

        // parse normalize
        {
            lua_getfield(L, -1, "normalize");
            if (!lua_isnil(L, -1))
            {
                attribute.m_Normalize = lua_toboolean(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse data_type
        {
            lua_getfield(L, -1, "data_type");
            if (!lua_isnil(L, -1))
            {
                attribute.m_DataType = (dmGraphics::VertexAttribute::DataType) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse coordinate_space
        {
            lua_getfield(L, -1, "coordinate_space");
            if (!lua_isnil(L, -1))
            {
                attribute.m_CoordinateSpace = (dmGraphics::CoordinateSpace) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        // parse semantic_type
        {
            lua_getfield(L, -1, "semantic_type");
            if (!lua_isnil(L, -1))
            {
                attribute.m_SemanticType = (dmGraphics::VertexAttribute::SemanticType) lua_tointeger(L, -1);
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1); // args table

        for (int i = 0; i < attribute.m_ElementCount; ++i)
        {
            FloatToVertexAttributeDataType(values_float[i], attribute.m_DataType, values_bytes);
        }

        dmRender::SetMaterialProgramAttributes(material_res->m_Material, &attribute, 1);

        return 0;
    }

    /*# sets a texture sampler in a material
     * Sets a texture sampler in a material, if the sampler exists. Use this function to change the settings of a texture sampler.
     * To set an actual texture that should be bound to the sampler, use the `material.set_texture` function instead.
     *
     * @name material.set_sampler
     *
     * @param path [type:hash|string] The path to the resource
     * @param name [type:hash|string] The sampler name
     * @param args [type:table] A table of what to update in the sampler (partial updates are supported). Supported entries:
     *
     * `u_wrap`
     * : [type:number] the u wrap mode of the texture sampler. Supported values:
     *
     *   - `material.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `material.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `material.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `material.TEXTURE_WRAP_REPEAT`
     *
     * `v_wrap`
     * : [type:number] the v wrap mode of the texture sampler. Supported values:
     *
     *   - `material.TEXTURE_WRAP_CLAMP_TO_BORDER`
     *   - `material.TEXTURE_WRAP_CLAMP_TO_EDGE`
     *   - `material.TEXTURE_WRAP_MIRRORED_REPEAT`
     *   - `material.TEXTURE_WRAP_REPEAT`
     *
     * `min_filter`
     * : [type:number] the min filter mode of the texture sampler. Supported values:
     *
     *   - `material.TEXTURE_FILTER_DEFAULT`
     *   - `material.TEXTURE_FILTER_NEAREST`
     *   - `material.TEXTURE_FILTER_LINEAR`
     *   - `material.TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST`
     *   - `material.TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR`
     *   - `material.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST`
     *   - `material.TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR`
     *
     * `mag_filter`
     * : [type:number] the mag filter mode of the texture sampler
     *
     *   - `material.TEXTURE_FILTER_DEFAULT`
     *   - `material.TEXTURE_FILTER_NEAREST`
     *   - `material.TEXTURE_FILTER_LINEAR`
     *
     * `max_anisotropy`
     * : [type:number] the max anisotropy of the texture sampler
     *
     * @examples
     * Configures a sampler in a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     material.set_sampler(self.my_material, "texture_sampler", { u_wrap = material.TEXTURE_WRAP_REPEAT, v_wrap = material.TEXTURE_WRAP_MIRRORED_REPEAT })
     * end
     * ```
     */
    static int Material_SetSampler(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        uint32_t unit = dmRender::GetMaterialSamplerUnit(material_res->m_Material, name_hash);
        if (unit == dmRender::INVALID_SAMPLER_UNIT)
        {
            return luaL_error(L, "Material sampler '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmRender::HSampler sampler = dmRender::GetMaterialSampler(material_res->m_Material, unit);

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

        dmRender::SetMaterialSampler(material_res->m_Material, name_hash, unit, u_wrap, v_wrap, min_filter, mag_filter, max_anisotropy);

        return 0;
    }

    /*# sets a shader constant in a material
     * Sets a shader constant in a material, if the constant exists.
     *
     * @name material.set_constant
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
     * Set a shader constant in a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function update(self)
     *     -- update the 'tint' constant
     *     material.set_constant(self.my_material, "tint", { value = vmath.vector4(1, 0, 0, 1) })
     *     -- change the type of the 'view_proj' constant to CONSTANT_TYPE_USER_MATRIX4 so the renderer can set our custom data
     *     material.set_constant(self.my_material, "view_proj", { value = self.my_view_proj, type = material.CONSTANT_TYPE_USER_MATRIX4 })
     * end
     * ```
     */
    static int Material_SetConstant(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmRender::HConstant constant;
        if (!dmRender::GetMaterialProgramConstant(material_res->m_Material, name_hash, constant))
        {
            return luaL_error(L, "Material constant '%s' not found", dmHashReverseSafe64(name_hash));
        }

        luaL_checktype(L, 3, LUA_TTABLE);
        lua_pushvalue(L, 3);

        // parse value
        {
            lua_getfield(L, -1, "value");
            if (!lua_isnil(L, -1))
            {
                dmVMath::Vector4* v4 = dmScript::CheckVector4(L, -1);
                dmRender::SetMaterialProgramConstant(material_res->m_Material, name_hash, v4, 1);
            }
            lua_pop(L, 1);
        }

        // parse type
        {
            lua_getfield(L, -1, "type");
            if (!lua_isnil(L, -1))
            {
                dmRenderDDF::MaterialDesc::ConstantType type = (dmRenderDDF::MaterialDesc::ConstantType) lua_tointeger(L, -1);
                dmRender::SetMaterialProgramConstantType(material_res->m_Material, name_hash, type);
            }
            lua_pop(L, 1);
        }

        lua_pop(L, 1);

        return 0;
    }

    /*# sets a texture sampler in a material
     * Sets a texture sampler in a material, if the sampler exists.
     *
     * @name material.set_texture
     *
     * @param path [type:hash|string] The path to the resource
     * @param name [type:hash|string] The sampler name
     * @param args [type:table] A table of what to update. Supported entries:
     *
     * `texture`
     * : [type:hash|handle] the texture to set. Can be either a texture resource hash or a texture handle.
     *
     * @examples
     * Set a texture in a material from a resource
     *
     * ```lua
     * go.property("my_material", resource.material())
     * go.property("my_texture", resource.texture())
     *
     * function init(self)
     *     material.set_texture(self.my_material, "my_texture", { texture = self.my_texture })
     * end
     * ```
     */
    static int Material_SetTexture(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        uint32_t unit = dmRender::GetMaterialSamplerUnit(material_res->m_Material, name_hash);
        if (unit == dmRender::INVALID_SAMPLER_UNIT)
        {
            return luaL_error(L, "Material sampler '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmhash_t texture_path = dmScript::CheckHashOrString(L, 3);
        TextureResource* texture_res = (TextureResource*) CheckResource(L, g_MaterialModule.m_Factory, texture_path, "texturec");

        material_res->m_TextureResourcePaths[unit] = texture_path;

        if (material_res->m_Textures[unit] != texture_res)
        {
            if (material_res->m_Textures[unit])
            {
                dmResource::Release(g_MaterialModule.m_Factory, material_res->m_Textures[unit]);
            }
            dmResource::IncRef(g_MaterialModule.m_Factory, texture_res);
            material_res->m_Textures[unit] = texture_res;
        }

        return 0;
    }

    static const luaL_reg ScriptMaterial_methods[] =
    {
        {"get_vertex_attributes", Material_GetVertexAttributes},
        {"get_samplers",          Material_GetSamplers},
        {"get_constants",         Material_GetConstants},
        {"get_textures",          Material_GetTextures},

        {"set_vertex_attribute",  Material_SetVertexAttribute},
        {"set_sampler",           Material_SetSampler},
        {"set_constant",          Material_SetConstant},
        {"set_texture",           Material_SetTexture},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        luaL_register(L, LIB_NAME, ScriptMaterial_methods);

        /*
    #define SET_VERTEX_ATTRIBUTE_ENUM(ename, name) \
        lua_pushnumber(L, (lua_Number) dmGraphics:: ename); \
        lua_setfield(L, -2, #name);

        // VertexAttribute::DataType
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_BYTE,           DATA_TYPE_BYTE);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_UNSIGNED_BYTE,  DATA_TYPE_UNSIGNED_BYTE);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_SHORT,          DATA_TYPE_SHORT);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_UNSIGNED_SHORT, DATA_TYPE_UNSIGNED_SHORT);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_INT,            DATA_TYPE_INT);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_UNSIGNED_INT,   DATA_TYPE_UNSIGNED_INT);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::TYPE_FLOAT,          DATA_TYPE_FLOAT);

        // VertexAttribute::SemanticType
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_NONE,       SEMANTIC_TYPE_NONE);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_POSITION,   SEMANTIC_TYPE_POSITION);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_TEXCOORD,   SEMANTIC_TYPE_TEXCOORD);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX, SEMANTIC_TYPE_PAGE_INDEX);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_COLOR,      SEMANTIC_TYPE_COLOR);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_NORMAL,     SEMANTIC_TYPE_NORMAL);
        SET_VERTEX_ATTRIBUTE_ENUM(VertexAttribute::SEMANTIC_TYPE_TANGENT,    SEMANTIC_TYPE_TANGENT);
    #undef SET_VERTEX_ATTRIBUTE_ENUM

    #define SET_GRAPHICS_ENUM(name) \
        lua_pushnumber(L, (lua_Number) dmGraphics:: name); \
        lua_setfield(L, -2, #name);

        // VertexAttribute::CoordinateSpace
        SET_GRAPHICS_ENUM(COORDINATE_SPACE_WORLD);
        SET_GRAPHICS_ENUM(COORDINATE_SPACE_LOCAL);
    #undef SET_GRAPHICS_ENUM
        */

        lua_pop(L, 1);
    }

    void ScriptMaterialRegister(const ScriptLibContext& context)
    {
        LuaInit(context.m_LuaState);
        g_MaterialModule.m_Factory = context.m_Factory;
    }
}
