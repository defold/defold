function test_sys()
    local filename = "save001.save"
    local file = sys.get_save_file("my_game", filename)
    -- Get file again, test mkdir
    file = sys.get_save_file("my_game", filename)
    result, error = os.remove(file)

    local data = sys.load(file)
    assert(#data == 0)

    local data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    local result = sys.save(file, data)
    assert(result)

    local data_prim = sys.load(file)

    assert(data['high_score'] == data_prim['high_score'])
    assert(data['location'] == data_prim['location'])
    assert(data['xp'] == data_prim['xp'])
    assert(data['name'] == data_prim['name'])

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

    -- get_ifaddrs
    local addresses = sys.get_ifaddrs()
    for i, a in ipairs(addresses) do
        local s = string.format("%d ip:%16s mac:%20s up:%s running:%s name:%s", i, tostring(a.address), tostring(a.mac), tostring(a.up), tostring(a.running), a.name)
        print(s)
    end

end

functions = { test_sys = test_sys }
