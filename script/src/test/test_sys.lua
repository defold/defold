function test_sys()
    local folder = sys.get_save_folder("my_game")
    -- Get folder again, test mkdir
    folder = sys.get_save_folder("my_game")
    local file = folder .. "/" .. "save001.save"

    local data = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    local result = sys.save(file, data)
    assert(result)

    local data_prim = sys.load(file)

    assert(data['high_score'] == data_prim['high_score'])
    assert(data['location'] == data_prim['location'])
    assert(data['xp'] == data_prim['xp'])
    assert(data['name'] == data_prim['name'])
end

functions = { test_sys = test_sys }
