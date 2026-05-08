-- Copyright 2020-2026 The Defold Foundation
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

-- Helper: assert that `tbl` is a non-empty array (1-indexed, contiguous)
local function assert_non_empty_array(tbl, label)
    assert(type(tbl) == "table", label .. " should be a table, got " .. type(tbl))
    assert(#tbl > 0, label .. " should be a non-empty array")
    for i = 1, #tbl do
        assert(tbl[i] ~= nil, label .. "[" .. i .. "] should not be nil")
    end
end

-- Helper: assert that the value at `tbl[key]` is of the expected Lua type
local function assert_field_type(tbl, key, expected_type)
    local actual = type(tbl[key])
    assert(actual == expected_type,
           "field '" .. key .. "' expected " .. expected_type .. ", got " .. actual)
end

local function test_get_engine_adapters()
    print("test_get_engine_adapters")

    local adapters = graphics.get_engine_adapters()
    assert_non_empty_array(adapters, "adapters")

    -- The test binary links GRAPHICS_NULL, so the null adapter must be present.
    local has_null = false
    for i = 1, #adapters do
        assert(type(adapters[i]) == "string",
               "adapters[" .. i .. "] should be a string, got " .. type(adapters[i]))
        if adapters[i] == "null" then
            has_null = true
        end
    end
    assert(has_null, "expected a 'null' adapter to be registered, got: " ..
                     table.concat(adapters, ", "))
end

local function test_get_adapter_info_shape()
    print("test_get_adapter_info_shape")

    local info = graphics.get_adapter_info()
    assert(type(info) == "table", "info should be a table, got " .. type(info))

    -- Top-level shape
    assert_field_type(info, "family",     "string")
    assert_field_type(info, "limits",     "table")
    assert_field_type(info, "extensions", "table")
    assert_field_type(info, "features",   "table")

    -- The test harness initializes dmScript without a graphics context, so
    -- the family literal must be the documented "none" sentinel and the
    -- extensions/features arrays must be empty.
    assert(info.family == "none",
           "without a graphics context, family should be 'none', got: " .. info.family)
    assert(#info.extensions == 0,
           "extensions should be empty when no graphics context, got " .. #info.extensions)
    assert(#info.features == 0,
           "features should be empty when no graphics context, got " .. #info.features)
end

local function test_get_adapter_info_limits_keys()
    print("test_get_adapter_info_limits_keys")

    local limits = graphics.get_adapter_info().limits

    -- Every documented limits key must be present and numeric.
    local expected_keys = {
        "max_texture_size_2d",
        "max_texture_size_3d",
        "max_texture_size_cube",
        "max_texture_array_layers",
        "max_framebuffer_width",
        "max_framebuffer_height",
        "max_color_attachments",
        "max_samplers_per_stage",
        "max_textures_per_stage",
        "max_vertex_attributes",
        "max_vertex_buffers",
        "max_compute_workgroup_size_x",
        "max_compute_workgroup_size_y",
        "max_compute_workgroup_size_z",
        "max_compute_workgroup_invocations",
        "max_compute_shared_memory_size",
        "max_uniform_buffer_range",
        "max_storage_buffer_range",
    }

    for _, key in ipairs(expected_keys) do
        assert_field_type(limits, key, "number")
        assert(limits[key] >= 0,
               "limit '" .. key .. "' should be non-negative, got " .. tostring(limits[key]))
    end

    -- Without a context all numeric fields are zero-initialized.
    assert(limits.max_texture_size_2d == 0,
           "without a graphics context, max_texture_size_2d should be 0, got " ..
           tostring(limits.max_texture_size_2d))
end

local function test_adapter_family_is_known()
    print("test_adapter_family_is_known")

    -- get_adapter_info().family must be one of the documented adapter family
    -- literals. This catches a regression where AdapterFamilyToName falls
    -- through to "<unsupported>".
    local known = {
        none = true, ["null"] = true, opengl = true, opengles = true,
        vulkan = true, vendor = true, webgpu = true, directx = true
    }

    local info = graphics.get_adapter_info()
    assert(known[info.family],
           "unknown adapter family literal: '" .. tostring(info.family) .. "'")

    -- And every entry from get_engine_adapters must also be a known family.
    local adapters = graphics.get_engine_adapters()
    for i = 1, #adapters do
        assert(known[adapters[i]],
               "unknown adapter family in engine adapters list: '" ..
               tostring(adapters[i]) .. "'")
    end
end

local function test_graphics()
    test_get_engine_adapters()
    test_get_adapter_info_shape()
    test_get_adapter_info_limits_keys()
    test_adapter_family_is_known()
end

functions = { test_graphics = test_graphics }
