function test_hash(hash_value)
    assert(hash_value == hash("test_value"))
    assert(hash("test_string1") ~= hash("test_string2") )
    print(tostring(hash_value))
    print("hash: " .. hash_value)
end

functions = { test_hash = test_hash }