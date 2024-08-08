-- Copyright 2020-2024 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
--
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
--
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.

require "lua.test_helpers"

function make_constant(name, value, type)
    return {
        name = hash(name),
        value = value,
        type = type
    }
end

function make_vertex_attribute(name, value, data_type, coordinate_space, normalize, semantic_type)
    return {
        name = hash(name),
        value = value,
        data_type = data_type,
        coordinate_space = coordinate_space,
        normalize = normalize,
        semantic_type = semantic_type,
    }
end

function default_sampler(name)
    return {
        name = hash(name),
        u_wrap = graphics.TEXTURE_WRAP_CLAMP_TO_EDGE,
        v_wrap = graphics.TEXTURE_WRAP_CLAMP_TO_EDGE,
        min_filter = graphics.TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST,
        mag_filter = graphics.TEXTURE_FILTER_LINEAR,
        max_anisotropy = 1
    }
end

function assert_sampler(sampler, expected)
    assert(is_table(sampler), "Sampler is not a table")
    assert(is_table(expected), "Expected is not a table")
    assert(sampler.name           == expected.name)
    assert(sampler.u_wrap         == expected.u_wrap)
    assert(sampler.v_wrap         == expected.v_wrap)
    assert(sampler.min_filter     == expected.min_filter)
    assert(sampler.mag_filter     == expected.mag_filter)
    assert(sampler.max_anisotropy == expected.max_anisotropy)
end

function assert_vertex_attribute(attribute, expected)
    if is_vec4(attribute.value) then
        assert(is_vec4(expected.value))
        assert_vec4(attribute.value, expected.value)
    elseif is_vec3(attribute.value) then
        assert(is_vec3(expected.value))
        assert_vec3(attribute.value, expected.value)
    else
        assert(is_number(attribute.value))
        assert(is_number(expected.value))
        assert_number(attribute.value, expected.value)
    end

    assert(attribute.data_type == expected.data_type)
    assert(attribute.coordinate_space == expected.coordinate_space)
    assert(attribute.normalize == expected.normalize)
    assert(attribute.semantic_type == expected.semantic_type)
    assert(attribute.name == expected.name)
end

function assert_constant(constant, expected)

    if constant.type == material.CONSTANT_TYPE_USER or constant.type == material.CONSTANT_TYPE_USER_MATRIX4 then
        if is_table(constant.value) then
            assert(is_table(expected.value))
            if constant.type == material.CONSTANT_TYPE_USER then
               for k,v in pairs(constant.value) do
                    assert_vec4(v, expected.value[k])
                end
            elseif constant.type == material.CONSTANT_TYPE_USER_MATRIX4 then
                for k,v in pairs(constant.value) do
                    assert_mat4(v, expected.value[k])
                end
            end
        elseif is_mat4(constant.value) then
            assert(is_mat4(expected.value))
            assert_mat4(constant.value, expected.value)
        elseif is_vec4(constant.value) then
            assert(is_vec4(expected.value))
            assert_vec4(constant.value, expected.value)
        end
    end

    assert(constant.type == expected.type)
    assert(constant.name == expected.name)
end

function assert_texture_resource(texture, expected)
    assert(texture.path == expected.path)
    assert(texture.width == expected.width)
    assert(texture.height == expected.height)
    assert(texture.type == expected.type)
end

function test_module_get_samplers(module, resource, expected_samplers)
    local samplers = module.get_samplers(resource)
    for k,v in pairs(samplers) do
        assert_sampler(v, expected_samplers[k])
    end
end

function test_module_get_constants(module, resource, expected_constants)
    local constants = module.get_constants(resource)
    for k,v in pairs(constants) do
        assert_constant(v, expected_constants[k])
    end
end

function test_module_get_vertex_attributes(module, resource, expected_attributes)
    local attributes = module.get_vertex_attributes(resource)
    for k,v in pairs(attributes) do
        assert_vertex_attribute(v, expected_attributes[k])
    end
end

function test_module_get_textures(module, resource)
    local textures = module.get_textures(resource)
    assert(is_table(textures))
end

local function get_by_name(table, name)
    for k,v in pairs(table) do
        if v.name == hash(name) then
            return v
        end
    end

    assert(false, name .. " not found")
end

function test_module_set_sampler(module, resource, sampler_name)
    local samplers = module.get_samplers(resource)
    local s = get_by_name(samplers, sampler_name)
    s.u_wrap = graphics.TEXTURE_WRAP_REPEAT
    s.v_wrap = graphics.TEXTURE_WRAP_REPEAT
    module.set_sampler(resource, sampler_name, s)
    local s_set = get_by_name(module.get_samplers(resource), sampler_name)
    assert_sampler(s_set, s)
end

function test_module_set_vertex_attribute(module, resource, attribute_name, values, semantic_type)
    local attributes = module.get_vertex_attributes(resource)
    local a         = get_by_name(attributes, attribute_name)
    a.value         = values
    a.semantic_type = semantic_type
    module.set_vertex_attribute(resource, attribute_name, a)
    local a_set = get_by_name(module.get_vertex_attributes(resource), attribute_name)
    assert_vertex_attribute(a_set, a)
end

function test_module_set_constant(module, resource, constant_name, value)
    local constants = module.get_constants(resource)
    local c = get_by_name(constants, constant_name)
    c.value = value
    module.set_constant(resource, constant_name, c)
    local c_set = get_by_name(module.get_constants(resource), constant_name)
    assert_constant(c_set, c)
end

function test_module_set_texture(module, resource_name, texture_name, texture_type)
    local t_res_path = "/" .. texture_name .. ".texturec"
    local t_id = resource.create_texture(t_res_path, {
        width  = 128,
        height = 128,
        type   = texture_type,
        format = graphics.TEXTURE_FORMAT_RGBA,
    })
    module.set_texture(resource_name, texture_name, t_id)

    local textures = module.get_textures(resource_name)
    local t_set = nil
    for k,v in pairs(textures) do
        if v.path == hash(t_res_path) then
            t_set = v
        end
    end

    assert(t_set ~= nil)
    assert_texture_resource(t_set, { path = hash(t_res_path), width = 128, height = 128, type = texture_type })
end
