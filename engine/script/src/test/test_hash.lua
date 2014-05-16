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

    assert(hashmd5("") == "d41d8cd98f00b204e9800998ecf8427e")
    assert(hashmd5("foo") == "acbd18db4cc2f85cedef654fccc4a4d8")
    assert(hashmd5("defold") == "01757dd6173a9e1b01714bb584ef00e5")

end

functions = { test_hash = test_hash }
