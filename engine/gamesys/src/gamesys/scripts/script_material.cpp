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

#include <extension/extension.hpp>
#include <render/material_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"

#include "../resources/res_material.h"
#include "../resources/res_texture.h"

extern "C"
{
    #include <lua/lua.h>
    #include <lua/lauxlib.h>
}

namespace dmGameSystem
{
    static dmGraphics::TextureType NormalizeBindableTextureType(dmGraphics::TextureType texture_type)
    {
        switch (texture_type)
        {
            case dmGraphics::TEXTURE_TYPE_TEXTURE_2D:       return dmGraphics::TEXTURE_TYPE_2D;
            case dmGraphics::TEXTURE_TYPE_TEXTURE_2D_ARRAY: return dmGraphics::TEXTURE_TYPE_2D_ARRAY;
            case dmGraphics::TEXTURE_TYPE_TEXTURE_3D:       return dmGraphics::TEXTURE_TYPE_3D;
            case dmGraphics::TEXTURE_TYPE_TEXTURE_CUBE:     return dmGraphics::TEXTURE_TYPE_CUBE_MAP;
            default:                                        return texture_type;
        }
    }

    static dmGraphics::CoordinateSpace ResolveMaterialCoordinateSpace(dmRender::HMaterial material, dmGraphics::CoordinateSpace coordinate_space)
    {
        if (coordinate_space != dmGraphics::COORDINATE_SPACE_DEFAULT)
        {
            return coordinate_space;
        }

        return dmRender::GetMaterialVertexSpace(material) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL
            ? dmGraphics::COORDINATE_SPACE_LOCAL
            : dmGraphics::COORDINATE_SPACE_WORLD;
    }

    /*# Material API documentation
     *
     * Functions for interacting with materials.
     *
     * @document
     * @language Lua
     * @name Material
     * @namespace material
     */

    #define LIB_NAME "material"

    struct MaterialModule
    {
        dmResource::HFactory m_Factory;
        dmArray<dmVMath::Vector4> m_ScratchValues;
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
     * : [type:vmath.vector4|vmath.vector3|vmath.matrix4|number|table] the value of the vertex attribute. Matrix attributes that do not map to `vmath.matrix4` are returned as a table of numbers.
     *
     * `normalize`
     * : [type:boolean] whether the value is normalized when passed into the shader
     *
     * `data_type`
     * : [type:number] the data type of the vertex attribute. Supported values:
     *
     *   - `graphics.DATA_TYPE_BYTE`
     *   - `graphics.DATA_TYPE_UNSIGNED_BYTE`
     *   - `graphics.DATA_TYPE_SHORT`
     *   - `graphics.DATA_TYPE_UNSIGNED_SHORT`
     *   - `graphics.DATA_TYPE_INT`
     *   - `graphics.DATA_TYPE_UNSIGNED_INT`
     *   - `graphics.DATA_TYPE_FLOAT`
     *
     * `coordinate_space`
     * : [type:number] the coordinate space of the vertex attribute. Supported values:
     *
     *   - `graphics.COORDINATE_SPACE_WORLD`
     *   - `graphics.COORDINATE_SPACE_LOCAL`
     *
     * `semantic_type`
     * : [type:number] the semantic type of the vertex attribute. Supported values:
     *
     *   - `graphics.SEMANTIC_TYPE_NONE`
     *   - `graphics.SEMANTIC_TYPE_POSITION`
     *   - `graphics.SEMANTIC_TYPE_TEXCOORD`
     *   - `graphics.SEMANTIC_TYPE_PAGE_INDEX`
     *   - `graphics.SEMANTIC_TYPE_COLOR`
     *   - `graphics.SEMANTIC_TYPE_NORMAL`
     *   - `graphics.SEMANTIC_TYPE_TANGENT`
     *   - `graphics.SEMANTIC_TYPE_WORLD_MATRIX`
     *   - `graphics.SEMANTIC_TYPE_NORMAL_MATRIX`
     *   - `graphics.SEMANTIC_TYPE_BONE_WEIGHTS`
     *   - `graphics.SEMANTIC_TYPE_BONE_INDICES`
     *   - `graphics.SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D`
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

        const dmGraphics::VertexAttributeInfo* attributes;
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

            lua_pushinteger(L, (lua_Integer) ResolveMaterialCoordinateSpace(material_res->m_Material, info.m_Attribute->m_CoordinateSpace));
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
     * Returns a table of all the shader constants in the material. This function will return all the shader constants
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
     *   - `material.CONSTANT_TYPE_TIME`
     *   - `material.CONSTANT_TYPE_WORLD_INVERSE`
     *   - `material.CONSTANT_TYPE_VIEW_INVERSE`
     *   - `material.CONSTANT_TYPE_PROJECTION_INVERSE`
     *   - `material.CONSTANT_TYPE_VIEWPROJ_INVERSE`
     *   - `material.CONSTANT_TYPE_WORLDVIEW_INVERSE`
     *   - `material.CONSTANT_TYPE_WORLDVIEWPROJ_INVERSE`
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
     *   - `graphics.TEXTURE_TYPE_2D`
     *   - `graphics.TEXTURE_TYPE_2D_ARRAY`
     *   - `graphics.TEXTURE_TYPE_CUBE_MAP`
     *   - `graphics.TEXTURE_TYPE_IMAGE_2D`
     *   - `graphics.TEXTURE_TYPE_3D`
     *   - `graphics.TEXTURE_TYPE_IMAGE_3D`
     *
     * `flags`
     * : [type:number] the flags of the texture. This field is a bit mask of these supported flags:
     *
     *   - `graphics.TEXTURE_USAGE_FLAG_SAMPLE`
     *   - `graphics.TEXTURE_USAGE_FLAG_MEMORYLESS`
     *   - `graphics.TEXTURE_USAGE_FLAG_STORAGE`
     *   - `graphics.TEXTURE_USAGE_FLAG_INPUT`
     *   - `graphics.TEXTURE_USAGE_FLAG_COLOR`
     *
     * @examples
     * Get the textures from a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     local textures = material.get_textures(self.my_material)
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

                dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(dmRender::GetMaterialRenderContext(material_res->m_Material));
                PushTextureInfo(L, graphics_context, texture_res->m_Texture, material_res->m_TextureResourcePaths[i]);
                lua_rawset(L, -3);
            }
        }

        return 1;
    }

    /*# sets vertex attributes in a material
     * Sets vertex attributes in a material, if the vertex attributes exist.
     *
     * @name material.set_vertex_attributes
     *
     * @param path [type:hash|string] The path to the resource
     * @param attributes [type:table] A table keyed by vertex attribute name with args tables as values. Partial updates are supported. Supported entries:
     *
     * `value`
     * : [type:vmath.vector4|vmath.vector3|vmath.matrix4|number|table] the value of the vertex attribute. Use a table of numbers for matrix attributes that do not map to `vmath.matrix4`.
     *
     * `normalize`
     * : [type:boolean] whether the value is normalized when passed into the shader
     *
     * `data_type`
     * : [type:number] the data type of the vertex attribute. Supported values:
     *
     *   - `graphics.DATA_TYPE_BYTE`
     *   - `graphics.DATA_TYPE_UNSIGNED_BYTE`
     *   - `graphics.DATA_TYPE_SHORT`
     *   - `graphics.DATA_TYPE_UNSIGNED_SHORT`
     *   - `graphics.DATA_TYPE_INT`
     *   - `graphics.DATA_TYPE_UNSIGNED_INT`
     *   - `graphics.DATA_TYPE_FLOAT`
     *
     * `coordinate_space`
     * : [type:number] the coordinate space of the vertex attribute. Supported values:
     *
     *   - `graphics.COORDINATE_SPACE_DEFAULT`
     *   - `graphics.COORDINATE_SPACE_WORLD`
     *   - `graphics.COORDINATE_SPACE_LOCAL`
     *
     * `semantic_type`
     * : [type:number] the semantic type of the vertex attribute. Supported values:
     *
     *   - `graphics.SEMANTIC_TYPE_NONE`
     *   - `graphics.SEMANTIC_TYPE_POSITION`
     *   - `graphics.SEMANTIC_TYPE_TEXCOORD`
     *   - `graphics.SEMANTIC_TYPE_PAGE_INDEX`
     *   - `graphics.SEMANTIC_TYPE_COLOR`
     *   - `graphics.SEMANTIC_TYPE_NORMAL`
     *   - `graphics.SEMANTIC_TYPE_TANGENT`
     *   - `graphics.SEMANTIC_TYPE_WORLD_MATRIX`
     *   - `graphics.SEMANTIC_TYPE_NORMAL_MATRIX`
     *   - `graphics.SEMANTIC_TYPE_BONE_WEIGHTS`
     *   - `graphics.SEMANTIC_TYPE_BONE_INDICES`
     *   - `graphics.SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D`
     *
     *
     * @examples
     * Configures a vertex attribute in a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     material.set_vertex_attributes(self.my_material, {
     *         tint_attribute = { value = vmath.vec4(1, 0, 0, 1), semantic_type = graphics.SEMANTIC_TYPE_COLOR },
     *         weights        = { value = vmath.vec4(0, 1, 0, 0), semantic_type = graphics.SEMANTIC_TYPE_NONE }
     *     })
     * end
     * ```
     */
    static int Material_SetVertexAttributes(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);

        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            int args_index = lua_gettop(L);
            dmhash_t name_hash = 0;
            if (lua_type(L, -2) == LUA_TNUMBER)
            {
                lua_getfield(L, args_index, "name");
                name_hash = dmScript::CheckHashOrString(L, -1);
                lua_pop(L, 1);
            }
            else
            {
                name_hash = dmScript::CheckHashOrString(L, -2);
            }

            dmRender::MaterialProgramAttributeInfo info = {};
            if (!dmRender::GetMaterialProgramAttributeInfo(material_res->m_Material, name_hash, info))
            {
                return luaL_error(L, "Material attribute '%s' not found", dmHashReverseSafe64(name_hash));
            }

            dmGraphics::VertexAttribute attribute = {};
            attribute.m_NameHash        = info.m_Attribute->m_NameHash;
            attribute.m_DataType        = info.m_Attribute->m_DataType;
            attribute.m_VectorType      = info.m_Attribute->m_VectorType;
            attribute.m_StepFunction    = info.m_Attribute->m_StepFunction;
            attribute.m_CoordinateSpace = info.m_Attribute->m_CoordinateSpace;
            attribute.m_SemanticType    = info.m_Attribute->m_SemanticType;
            attribute.m_ElementCount    = info.m_Attribute->m_ElementCount;
            attribute.m_Normalize       = info.m_Attribute->m_Normalize;

            uint8_t values_bytes[sizeof(dmVMath::Matrix4)] = {};
            uint32_t values_size = 0;
            float values_float[16] = {};

            luaL_checktype(L, args_index, LUA_TTABLE);

            // parse value
            {
                lua_getfield(L, args_index, "value");
                if (!lua_isnil(L, -1))
                {
                    if (dmScript::IsVector4(L, -1))
                    {
                        dmVMath::Vector4* v4 = dmScript::CheckVector4(L, -1);
                        values_size = sizeof(dmVMath::Vector4);
                        memcpy(values_float, v4, values_size);
                    }
                    else if (dmScript::IsVector3(L, -1))
                    {
                        dmVMath::Vector3* v3 = dmScript::CheckVector3(L, -1);
                        values_size = sizeof(dmVMath::Vector3);
                        memcpy(values_float, v3, values_size);
                    }
                    else if (dmScript::IsMatrix4(L, -1))
                    {
                        dmVMath::Matrix4* m4 = dmScript::CheckMatrix4(L, -1);
                        values_size = sizeof(dmVMath::Matrix4);
                        memcpy(values_float, m4, values_size);
                    }
                    else if (lua_istable(L, -1))
                    {
                        uint32_t value_count = lua_objlen(L, -1);
                        if (value_count > sizeof(values_float) / sizeof(values_float[0]))
                        {
                            return luaL_error(L, "Too many values in vertex attribute '%s'", dmHashReverseSafe64(name_hash));
                        }
                        for (uint32_t i = 0; i < value_count; ++i)
                        {
                            lua_rawgeti(L, -1, i + 1);
                            values_float[i] = luaL_checknumber(L, -1);
                            lua_pop(L, 1);
                        }
                        values_size = value_count * sizeof(float);
                    }
                    else
                    {
                        values_float[0] = luaL_checknumber(L, -1);
                        values_size = sizeof(float);
                    }
                }
                lua_pop(L, 1);
            }

            // parse normalize
            {
                lua_getfield(L, args_index, "normalize");
                if (!lua_isnil(L, -1))
                {
                    attribute.m_Normalize = lua_toboolean(L, -1);
                }
                lua_pop(L, 1);
            }

            // parse data_type
            {
                lua_getfield(L, args_index, "data_type");
                if (!lua_isnil(L, -1))
                {
                    attribute.m_DataType = (dmGraphics::VertexAttribute::DataType) lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
            }

            // parse coordinate_space
            {
                lua_getfield(L, args_index, "coordinate_space");
                if (!lua_isnil(L, -1))
                {
                    attribute.m_CoordinateSpace = (dmGraphics::CoordinateSpace) lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
            }

            // parse semantic_type
            {
                lua_getfield(L, args_index, "semantic_type");
                if (!lua_isnil(L, -1))
                {
                    attribute.m_SemanticType = (dmGraphics::VertexAttribute::SemanticType) lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
            }

            if (values_size > 0)
            {
                uint32_t bytes_per_element = dmGraphics::DataTypeToByteWidth(attribute.m_DataType);
                for (int i = 0; i < attribute.m_ElementCount; ++i)
                {
                    FloatToVertexAttributeDataType(values_float[i], attribute.m_DataType, values_bytes + i * bytes_per_element);
                }

                attribute.m_Values.m_BinaryValues.m_Data = values_bytes;
                attribute.m_Values.m_BinaryValues.m_Count = bytes_per_element * attribute.m_ElementCount;
            }

            dmRender::SetMaterialProgramAttributes(material_res->m_Material, &attribute, 1);
            lua_pop(L, 1);
        }
        return 0;
    }

    /*# sets texture samplers in a material
     * Sets texture samplers in a material, if the samplers exist. Use this function to change the settings of texture samplers.
     * To set actual textures that should be bound to the samplers, use the `material.set_textures` function instead.
     *
     * @name material.set_samplers
     *
     * @param path [type:hash|string] The path to the resource
     * @param samplers [type:table] A table keyed by sampler name with args tables as values. Partial updates are supported. Supported entries:
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
     * Configures a sampler in a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function init(self)
     *     material.set_samplers(self.my_material, {
     *         texture_sampler = { u_wrap = graphics.TEXTURE_WRAP_REPEAT, v_wrap = graphics.TEXTURE_WRAP_MIRRORED_REPEAT }
     *     })
     * end
     * ```
     */
    static int SetMaterialSampler(lua_State* L, MaterialResource* material_res, dmhash_t name_hash, int args_index)
    {
        args_index = args_index < 0 ? lua_gettop(L) + args_index + 1 : args_index;

        uint32_t unit = dmRender::GetMaterialSamplerUnit(material_res->m_Material, name_hash);
        if (unit == dmRender::INVALID_SAMPLER_UNIT)
        {
            return luaL_error(L, "Material sampler '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmRender::HSampler sampler = dmRender::GetMaterialSampler(material_res->m_Material, unit);

        dmRender::SamplerInfo sampler_info = {};
        dmRender::GetSamplerInfo(sampler, &sampler_info);

        luaL_checktype(L, args_index, LUA_TTABLE);
        lua_pushvalue(L, args_index);

        GetSamplerParametersFromLua(L, &sampler_info.m_UWrap, &sampler_info.m_VWrap, &sampler_info.m_MinFilter, &sampler_info.m_MagFilter, &sampler_info.m_MaxAnisotropy);

        lua_pop(L, 1);

        dmRender::SetMaterialSampler(material_res->m_Material, name_hash, unit, sampler_info.m_UWrap, sampler_info.m_VWrap, sampler_info.m_MinFilter, sampler_info.m_MagFilter, sampler_info.m_MaxAnisotropy);

        return 0;
    }

    static int Material_SetSamplers(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);

        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            dmhash_t name_hash = dmScript::CheckHashOrString(L, -2);
            SetMaterialSampler(L, material_res, name_hash, lua_gettop(L));
            lua_pop(L, 1);
        }
        return 0;
    }

    /*# sets shader constants in a material
     * Sets shader constants in a material, if the constants exist.
     *
     * @name material.set_constants
     *
     * @param path [type:hash|string] The path to the resource
     * @param constants [type:table] A table keyed by constant name with args tables as values. Constants can be partially updated. Supported entries:
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
     *   - `material.CONSTANT_TYPE_TIME`
     *   - `material.CONSTANT_TYPE_WORLD_INVERSE`
     *   - `material.CONSTANT_TYPE_VIEW_INVERSE`
     *   - `material.CONSTANT_TYPE_PROJECTION_INVERSE`
     *   - `material.CONSTANT_TYPE_VIEWPROJ_INVERSE`
     *   - `material.CONSTANT_TYPE_WORLDVIEW_INVERSE`
     *   - `material.CONSTANT_TYPE_WORLDVIEWPROJ_INVERSE`
     *
     * `value`
     * : [type:vmath.vector4|vmath.vector3|vmath.matrix4|number|table] the value(s) of the constant. If the shader constant is an array, the amount of values to update depends on how many values that are passed in the 'value' field.
     *
     * @examples
     * Set a shader constant in a material specified as a resource property
     *
     * ```lua
     * go.property("my_material", resource.material())
     *
     * function update(self)
     *     -- update the 'tint' constant
     *     material.set_constants(self.my_material, {
     *         tint = { value = vmath.vector4(1, 0, 0, 1) }
     *     })
     *     -- change the type of the 'view_proj' constant to CONSTANT_TYPE_USER_MATRIX4 so the renderer can set our custom data
     *     material.set_constants(self.my_material, {
     *         view_proj = { value = self.my_view_proj, type = material.CONSTANT_TYPE_USER_MATRIX4 }
     *     })
     * end
     * ```
     */
    static int SetMaterialConstant(lua_State* L, MaterialResource* material_res, dmhash_t name_hash, int args_index)
    {
        args_index = args_index < 0 ? lua_gettop(L) + args_index + 1 : args_index;

        dmRender::HConstant constant;
        if (!dmRender::GetMaterialProgramConstant(material_res->m_Material, name_hash, constant))
        {
            return luaL_error(L, "Material constant '%s' not found", dmHashReverseSafe64(name_hash));
        }

        luaL_checktype(L, args_index, LUA_TTABLE);
        lua_pushvalue(L, args_index);

        dmRenderDDF::MaterialDesc::ConstantType type_from_value = dmRender::GetConstantType(constant);

        g_MaterialModule.m_ScratchValues.SetSize(0);

        GetConstantValuesFromLua(L, &type_from_value, &g_MaterialModule.m_ScratchValues);

        if (g_MaterialModule.m_ScratchValues.Size() != 0)
        {
            dmRender::SetMaterialProgramConstant(material_res->m_Material, name_hash,
                g_MaterialModule.m_ScratchValues.Begin(), g_MaterialModule.m_ScratchValues.Size());
        }

        if (dmRender::GetConstantType(constant) != type_from_value)
        {
            dmRender::SetMaterialProgramConstantType(material_res->m_Material, name_hash, type_from_value);
        }

        lua_pop(L, 1);

        return 0;
    }

    static int Material_SetConstants(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);

        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            dmhash_t name_hash = dmScript::CheckHashOrString(L, -2);
            SetMaterialConstant(L, material_res, name_hash, lua_gettop(L));
            lua_pop(L, 1);
        }
        return 0;
    }

    /*# sets textures in a material
     * Sets textures in a material, if the samplers exist.
     *
     * @name material.set_textures
     *
     * @param path [type:hash|string] The path to the resource
     * @param textures [type:table] A table keyed by sampler name with texture resources as values.
     *
     * @examples
     * Set a texture in a material from a resource
     *
     * ```lua
     * go.property("my_material", resource.material())
     * go.property("my_texture", resource.texture())
     *
     * function init(self)
     *     material.set_textures(self.my_material, {
     *         my_texture = self.my_texture
     *     })
     * end
     * ```
     */
    static int SetMaterialTexture(lua_State* L, MaterialResource* material_res, dmhash_t name_hash, int texture_index)
    {
        texture_index = texture_index < 0 ? lua_gettop(L) + texture_index + 1 : texture_index;

        uint32_t unit = dmRender::GetMaterialSamplerUnit(material_res->m_Material, name_hash);
        if (unit == dmRender::INVALID_SAMPLER_UNIT)
        {
            return luaL_error(L, "Material sampler '%s' not found", dmHashReverseSafe64(name_hash));
        }

        dmhash_t texture_path        = dmScript::CheckHashOrString(L, texture_index);
        TextureResource* texture_res = (TextureResource*) CheckResource(L, g_MaterialModule.m_Factory, texture_path, "texturec");

        dmGraphics::HContext graphics_context   = dmRender::GetGraphicsContext(dmRender::GetMaterialRenderContext(material_res->m_Material));
        dmGraphics::TextureType texture_type_in = dmGraphics::GetTextureType(graphics_context, texture_res->m_Texture);
        dmRender::HSampler sampler              = dmRender::GetMaterialSampler(material_res->m_Material, unit);

        dmRender::SamplerInfo sampler_info = {};
        dmRender::GetSamplerInfo(sampler, &sampler_info);

        dmGraphics::TextureType normalized_sampler_type = NormalizeBindableTextureType(sampler_info.m_TextureType);

        if (normalized_sampler_type != texture_type_in)
        {
            return luaL_error(L, "Texture type mismatch. Can't bind a %s texture to a %s sampler",
                dmGraphics::GetTextureTypeLiteral(texture_type_in),
                dmGraphics::GetTextureTypeLiteral(normalized_sampler_type));
        }

        material_res->m_TextureResourcePaths[unit] = texture_path;
        material_res->m_SamplerNames[unit]         = name_hash;

        if (material_res->m_Textures[unit] != texture_res)
        {
            if (material_res->m_Textures[unit])
            {
                dmResource::Release(g_MaterialModule.m_Factory, material_res->m_Textures[unit]);
            }
            dmResource::IncRef(g_MaterialModule.m_Factory, texture_res);
            material_res->m_Textures[unit] = texture_res;
        }

        // Recalculate number of textures (similar to res_material.cpp) by checking which samplers exist
        material_res->m_NumTextures = 0;
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            uint32_t unit = dmRender::GetMaterialSamplerUnit(material_res->m_Material, material_res->m_SamplerNames[i]);
            if (unit == dmRender::INVALID_SAMPLER_UNIT)
            {
                continue;
            }
            material_res->m_NumTextures++;
        }
        return 0;
    }

    static int Material_SetTextures(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        MaterialResource* material_res = CheckMaterialResource(L, 1);

        luaL_checktype(L, 2, LUA_TTABLE);
        lua_pushnil(L);
        while (lua_next(L, 2) != 0)
        {
            dmhash_t name_hash = dmScript::CheckHashOrString(L, -2);
            SetMaterialTexture(L, material_res, name_hash, lua_gettop(L));
            lua_pop(L, 1);
        }
        return 0;
    }

    static const luaL_reg ScriptMaterial_methods[] =
    {
        {"get_vertex_attributes", Material_GetVertexAttributes},
        {"get_samplers",          Material_GetSamplers},
        {"get_constants",         Material_GetConstants},
        {"get_textures",          Material_GetTextures},
        {"set_vertex_attributes", Material_SetVertexAttributes},
        {"set_samplers",          Material_SetSamplers},
        {"set_constants",         Material_SetConstants},
        {"set_textures",          Material_SetTextures},
        {0, 0}
    };

    static void LuaInit(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        luaL_register(L, LIB_NAME, ScriptMaterial_methods);

        #define SET_MATERIAL_DDF_ENUM_NAMED(enum_name, name) \
            lua_pushnumber(L, (lua_Number) dmRenderDDF::MaterialDesc:: enum_name); \
            lua_setfield(L, -2, #name);

        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_USER,          CONSTANT_TYPE_USER);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_USER_MATRIX4,  CONSTANT_TYPE_USER_MATRIX4);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_VIEWPROJ,      CONSTANT_TYPE_VIEWPROJ);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_WORLD,         CONSTANT_TYPE_WORLD);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_TEXTURE,       CONSTANT_TYPE_TEXTURE);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_VIEW,          CONSTANT_TYPE_VIEW);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_PROJECTION,    CONSTANT_TYPE_PROJECTION);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_NORMAL,        CONSTANT_TYPE_NORMAL);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_WORLDVIEW,     CONSTANT_TYPE_WORLDVIEW);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_WORLDVIEWPROJ, CONSTANT_TYPE_WORLDVIEWPROJ);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_TIME,          CONSTANT_TYPE_TIME);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_WORLD_INVERSE,  CONSTANT_TYPE_WORLD_INVERSE);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_VIEW_INVERSE,   CONSTANT_TYPE_VIEW_INVERSE);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_PROJECTION_INVERSE, CONSTANT_TYPE_PROJECTION_INVERSE);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_VIEWPROJ_INVERSE,   CONSTANT_TYPE_VIEWPROJ_INVERSE);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_WORLDVIEW_INVERSE,  CONSTANT_TYPE_WORLDVIEW_INVERSE);
        SET_MATERIAL_DDF_ENUM_NAMED(ConstantType::CONSTANT_TYPE_WORLDVIEWPROJ_INVERSE, CONSTANT_TYPE_WORLDVIEWPROJ_INVERSE);

        #undef SET_MATERIAL_DDF_ENUM_NAMED

        lua_pop(L, 1);
    }

    static dmExtension::Result ScriptMaterialInitialize(dmExtension::Params* params)
    {
        LuaInit(params->m_L);
        g_MaterialModule.m_Factory = params->m_ResourceFactory;
        return dmExtension::RESULT_OK;
    }

    static dmExtension::Result ScriptMaterialFinalize(dmExtension::Params* params)
    {
        g_MaterialModule.m_Factory = 0;
        return dmExtension::RESULT_OK;
    }

    DM_DECLARE_EXTENSION(ScriptMaterialExt, "ScriptMaterial", 0, 0, ScriptMaterialInitialize, 0, 0, ScriptMaterialFinalize)
}
