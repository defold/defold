function test_sys()
    local filename = "save001.save"
    local max_table_size = 512 * 1024
    local file = sys.get_save_file("my_game", filename)
    -- Get file again, test mkdir
    file = sys.get_save_file("my_game", filename)
    result, error = os.remove(file)

    -- test resource missing, expected to return an empty table
    local data = sys.load(file)
    assert(#data == 0)

    -- save file exceeding max buffer size, expected to fail
    for i=1,max_table_size+1 do data[i] = i end
    local ret, msg = pcall(function() sys.save(file, data) end)
    if ret then assert(false, "expected lua error for sys.save with data table exceeding max size " .. max_table_size .. " bytes") end

    -- save file with too long path (>1024 chars long), expected to fail
    local valid_data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    local long_filename = "dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy_dummy.save"
    local long_file = sys.get_save_file("my_game", long_filename)

    ret, msg = pcall(function() sys.save(long_file, valid_data) end)
    if ret then assert(false, "expected lua error for sys.save with too long path") end

    -- save file
    local data = { high_score = 1111, location = vmath.vector3(2,2,2), xp = 33, name = "First Save" }
    local result = sys.save(file, data)
    assert(result)

    -- resave file (checking the atomic move works)
    data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    result = sys.save(file, data)
    assert(result)

    -- reload saved file
    local data_prim = sys.load(file)

    assert(data['high_score'] == data_prim['high_score'])
    assert(data['location'] == data_prim['location'])
    assert(data['xp'] == data_prim['xp'])
    assert(data['name'] == data_prim['name'])

    -- load file exceeding max buffer size, expected to fail
    fh = io.open(file, "a+")
    for i=1,(max_table_size/8) do fh:write("deadbeef") end
    fh:close()
    local ret, msg = pcall(function() sys.load(file, data) end)
    if ret then assert(false, "expected lua error for sys.load with data table exceeding max size " .. max_table_size .. " bytes") end

    -- get_config
    assert(sys.get_config("main.does_not_exists") == nil)
    assert(sys.get_config("main.does_not_exists", "foobar") == "foobar")
    assert(sys.get_config("foo.value") == "123")
    assert(sys.get_config("foo.value", 456) == "123")

    -- load_resource
    local test_resource = sys.load_resource("/src/test/test_resource.txt")
    assert(test_resource == "defold")

    local does_not_exists = sys.load_resource("/does_not_exists")
    assert(does_not_exists == nil)

    -- get_sys_info
    local info = sys.get_sys_info()
    print("device_model: " .. info.device_model)
    print("system_name: " .. info.system_name)
    print("system_version: " .. info.system_version)
    print("language: " .. info.language)
    print("territory: " .. info.territory)
    print("gmt_offset: " .. info.gmt_offset)
    print("device_ident" .. info.device_ident)

    -- get_ifaddrs
    local addresses = sys.get_ifaddrs()
    for i, a in ipairs(addresses) do
        local s = string.format("%d ip:%16s mac:%20s up:%s running:%s name:%s", i, tostring(a.address), tostring(a.mac), tostring(a.up), tostring(a.running), a.name)
        print(s)
    end

end

functions = { test_sys = test_sys }
