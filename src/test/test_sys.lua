function test_sys()
    local folder = sys.get_savegame_folder("my_game")
    -- Get folder again, test mkdir
    folder = sys.get_savegame_folder("my_game")
    local name = folder .. "/" .. "save001.save"
    local f = io.open(name, "wb")

    local save = { high_score = 1234, location = vmath.vector3(1,2,3), xp = 99, name = "Mr Player" }
    local save_pickle = pickle.dumps(save)
    f:write(save_pickle)
    f:close()

    f = io.open(name, "rb")
    local save_prim = pickle.loads(f:read("*all"))
    f:close()

    assert(save['high_score'] == save_prim['high_score'])
    assert(save['location'] == save_prim['location'])
    assert(save['xp'] == save_prim['xp'])
    assert(save['name'] == save_prim['name'])
end

functions = { test_sys = test_sys }
