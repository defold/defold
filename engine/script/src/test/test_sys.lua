-- Copyright 2020-2022 The Defold Foundation
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

function test_sys()
    local filename = "save001.save"
    local max_table_size_v3 = 512 * 1024
    local file = sys.get_save_file("my_game", filename)
    -- Get file again, test mkdir
    file = sys.get_save_file("my_game", filename)
    result, error = os.remove(file)

    -- test resource missing, expected to return an empty table
    local data = sys.load(file)
    assert(#data == 0)

    -- save large table, it should not fail with v4+
    print("Saving large table")
    for i=1,65536 do data[i] = i end
    local ret, msg = pcall(function() sys.save(file, data) end)
    if not ret then
        print("An error occurred when calling sys.save()")
        print(msg)
        assert(false, "expected sys.save to work with data table exceeding max size " .. max_table_size_v3 .. " bytes")
    end
    -- load large table, it should not fail with v4+
    print("Loading large table")
    local ret, msg = pcall(function() sys.load(file) end)
    if not ret then
        print("An error occurred when calling sys.load()")
        print(msg)
        assert(false, "expected sys.load to work with data table exceeding max size " .. max_table_size_v3 .. " bytes")
    end

    -- save file with too long path (>1024 chars long), expected to fail
    print("Testing long filename")
    local valid_data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    local long_filename = "dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy.save"
    local long_file = sys.get_save_file("my_game", long_filename)

    ret, msg = pcall(function() sys.save(long_file, valid_data) end)
    if ret then
        assert(false, "expected lua error for sys.save with too long path")
    end

    -- save file
    print("Saving file")
    local data = { high_score = 1111, location = vmath.vector3(2,2,2), xp = 33, name = "First Save" }
    ret, msg = pcall(function() sys.save(file, data) end)
    if not ret then
        print("An error occurred when calling sys.save() on a small table")
        print(msg)
        assert(false, "expected sys.save to successfully save small table")
    end

    -- resave file (checking the atomic move works)
    data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    print("Resaving file")
    ret, msg = pcall(function() sys.save(file, data) end)
    if not ret then
        print(msg)
        assert(false, "expected sys.save() on an existing file to work")
    end

    -- reload saved file
    print("Reloading file")
    local data_prim
    ret, msg = pcall(function() data_prim = sys.load(file) end)
    if not ret then
        print(msg)
        assert(false, "expected sys.load() to work")
    end
    print("Asserting loaded values")
    assert(data['high_score'] == data_prim['high_score'])
    assert(data['location'] == data_prim['location'])
    assert(data['xp'] == data_prim['xp'])
    assert(data['name'] == data_prim['name'])


    -- get_config
    print("Testing get_config")
    assert(sys.get_config("main.does_not_exists") == nil)
    assert(sys.get_config("main.does_not_exists", "foobar") == "foobar")
    assert(sys.get_config("foo.value") == "123")
    assert(sys.get_config("foo.value", 456) == "123")

    -- load_resource
    print("Load existing resource")
    local test_resource = sys.load_resource("/src/test/test_resource.txt")
    assert(test_resource == "defold")

    print("Load missing resource")
    local does_not_exists = sys.load_resource("/does_not_exists")
    assert(does_not_exists == nil)

    -- get_sys_info
    print("Getting sys_info")
    local info = sys.get_sys_info()
    print("device_model: " .. info.device_model)
    print("system_name: " .. info.system_name)
    print("system_version: " .. info.system_version)
    print("language: " .. info.language)
    print("territory: " .. info.territory)
    print("gmt_offset: " .. info.gmt_offset)
    print("device_ident" .. info.device_ident)

    -- get_ifaddrs
    print("Getting ifaddrs")
    local addresses = sys.get_ifaddrs()
    for i, a in ipairs(addresses) do
        local s = string.format("%d ip:%16s mac:%20s up:%s running:%s name:%s", i, tostring(a.address), tostring(a.mac), tostring(a.up), tostring(a.running), a.name)
        print(s)
    end

    -- serialize/deserialize
    print("Testing serialize and deserialize")
    data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player", [1] = "lala" }
    local serialized = sys.serialize(data)
    local deserialized = sys.deserialize(serialized)
    for k, v in pairs(data) do
        assert(deserialized[k] == v)
    end

    print("Done")
end

functions = { test_sys = test_sys }
