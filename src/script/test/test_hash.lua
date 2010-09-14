function test_hash(hash_value)
    assert(hash_value == hash("test_value"))
end

functions = { test_hash = test_hash }