function test_hash(hash_value, hash_value_hex)
    assert(hash_value == hash("test_value"))
    assert(hash_value_hex == hash_to_hex(hash_value))
    assert(hash("test_string1") ~= hash("test_string2") )
    assert(hash_to_hex(hash("test_string1")) ~= hash_to_hex(hash("test_string2")))
    print(tostring(hash_value))
    print("hash: " .. hash_value)

    local t = {}
    t[hash("x")] = 1
    assert(t[hash("x")] == 1)
    t[hash("x")] = 2
    assert(t[hash("x")] == 2)
    local key_count = 0
    for i,v in pairs(t)
    do
        key_count = key_count + 1
    end

    assert(key_count == 1)

end

functions = { test_hash = test_hash }
