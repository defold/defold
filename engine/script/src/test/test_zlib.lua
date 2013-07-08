local function test_error()
    local ret, msg = pcall(function() zlib.inflate("x") end)
    if ret then
        assert(false, "expected zlib error")
    else
        print(msg)
    end
end

function test_zlib()
    assert(zlib.inflate(zlib.deflate("foo")) == "foo")
    test_error()
end

functions = { test_zlib = test_zlib }
