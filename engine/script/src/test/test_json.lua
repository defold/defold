function startswith(needle, haystack)
    return string.sub(haystack, 1, string.len(needle)) == needle;
end

function assert_error(js, expected)
    local ret, msg = pcall(function() json.decode(js) end)
    if ret then
        print("ASSERT failed for JSON '" .. js .. "'")
        print("  Expected error : " .. expected)
        print("  Actual         : call succeeded")
        assert(false)
    else
        local start = string.find(msg, ":", string.find(msg, ":") + 1) + 2
        local errmsg = string.sub(msg, start, string.len(msg))
        if not startswith(expected, errmsg) then
            print("ASSERT failed for JSON '" .. js .. "'")
            print("  Expected error : " .. expected)
            print("  Actual         : " .. errmsg)
            assert(false)
        end
        print(msg)
    end
end

function test_json_invalid_primitive()
    assert_error("Null", "Invalid JSON primitive: Null")
    assert_error("NULL", "Invalid JSON primitive: NULL")
    assert_error("True", "Invalid JSON primitive: True")
    assert_error("TRUE", "Invalid JSON primitive: TRUE")
    assert_error("False", "Invalid JSON primitive: False")
    assert_error("FALSE", "Invalid JSON primitive: FALSE")
    assert_error("defold", "Invalid JSON primitive: defold")
    assert_error("0.d3", "Invalid JSON primitive: 0.d3")
    assert_error("{1 2 3}", "Incomplete JSON object: {1 2 3}")
    assert_error("{1: 2, 3}", "Incomplete JSON object: {1: 2, 3}")
    assert_error("{ response = \"ok\" }", "Incomplete JSON object: { response = \"ok\" }")
end

function test_json_valid_primitive()
    assert(json.decode("null") == nil)
    assert(json.decode("true") == true)
    assert(json.decode("false") == false)

    assert(json.decode("10") == 10)
    assert(json.decode("010") == 10)
    assert(json.decode("-10") == -10)
    assert(json.decode("-010") == -10)
    assert(json.decode("0") == 0)
    assert(json.decode("-0") == 0)

    assert(json.decode("10.05") == 10.05)
    assert(json.decode("10.0") == 10)
    assert(json.decode("10.00") == 10)
    assert(json.decode("010.0") == 10)
    assert(json.decode("-10.05") == -10.05)
    assert(json.decode("-10.0") == -10)
    assert(json.decode("-10.00") == -10)
    assert(json.decode("-010.0") == -10)
    assert(json.decode("0.0") == 0)
    assert(json.decode("-0.0") == 0)
    assert(json.decode("00.0") == 0)
end

function test_syntax_error(js)
    local ret, msg = pcall(function() json.decode(js) end)
    if ret then
        assert(false, "expected lua error for " .. js)
    else
        print(msg)
    end
end

function test_json()
    assert(json.decode('"foo"') == "foo")
    assert(json.decode('"\\n"') == "\n")
    -- NOTE: The file must be in UTF-8 for the unicode test
    assert(json.decode('"x\\u00e5y\\u00e4z\\u00f6w"') == "xåyäzöw")
    local a = json.decode("[10,20,30]")
    assert(#a == 3)
    assert(a[1] == 10)
    assert(a[2] == 20)
    assert(a[3] == 30)

    local t = json.decode('{"x": 100, "y": 200}')
    assert(t.x == 100)
    assert(t.y == 200)

    local nested = json.decode('[[10,"x", 20],[30]]')
    assert(#nested == 2)
    assert(#nested[1] == 3)
    assert(nested[1][1] == 10)
    assert(nested[1][2] == "x")
    assert(nested[1][3] == 20)
    assert(#nested[2] == 1)
    assert(nested[2][1] == 30)

    test_syntax_error("[")
    test_syntax_error("]")
    test_syntax_error("{")
    test_syntax_error("")

    test_json_invalid_primitive()
    test_json_valid_primitive()
end

functions = { test_json = test_json }
