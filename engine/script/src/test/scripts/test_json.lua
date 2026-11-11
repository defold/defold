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
        -- replace any known mounts that might confuse this test
        msg = string.gsub(msg, "host:/", "")
        msg = string.gsub(msg, "save:/", "")
        msg = string.gsub(msg, "data:/", "")
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
    print("Expected parsing errors ->")
    assert_error("Null", "Expected value but found invalid token at character 1")
    assert_error("NULL", "Expected value but found invalid token at character 1")
    assert_error("True", "Expected value but found invalid token at character 1")
    assert_error("TRUE", "Expected value but found invalid token at character 1")
    assert_error("False", "Expected value but found invalid token at character 1")
    assert_error("FALSE", "Expected value but found invalid token at character 1")
    assert_error("defold", "Expected value but found invalid token at character 1")
    assert_error("0.d3", "Expected the end but found invalid token at character 3")
    assert_error("{1 2 3}", "Expected object key string but found T_NUMBER at character 2")
    assert_error("{1: 2, 3}", "Expected object key string but found T_NUMBER at character 2")
    assert_error("{ response = \"ok\" }", "Expected object key string but found invalid token at character 3")
    print("<- end of expected parsing errors.")
end

function test_json_valid_primitive()
    assert(json.decode("null") == nil)
    assert(json.decode("null", { decode_null_as_userdata = false }) == nil)
    assert(json.decode("null", { decode_null_as_userdata = true }) == json.null)
    assert(json.decode("true") == true)
    assert(json.decode("false") == false)

    assert(json.decode("10") == 10)
    assert(json.decode("-10") == -10)
    assert(json.decode("0") == 0)
    assert(json.decode("-0") == 0)

    assert(json.decode("10.05") == 10.05)
    assert(json.decode("10.0") == 10)
    assert(json.decode("10.00") == 10)
    assert(json.decode("010.0") == 10)
    assert(json.decode("-10.05") == -10.05)
    assert(json.decode("-10.0") == -10)
    assert(json.decode("-10.00") == -10)
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

function test_json_decode()
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

    local t = json.decode('{"foo":"bar","num":16}\0hello')
    assert(t.foo == "bar")
    assert(t.num == 16)

    local nested = json.decode('[[10,"x", 20],[30]]')
    assert(#nested == 2)
    assert(#nested[1] == 3)
    assert(nested[1][1] == 10)
    assert(nested[1][2] == "x")
    assert(nested[1][3] == 20)
    assert(#nested[2] == 1)
    assert(nested[2][1] == 30)

    -- https://github.com/defold/defold/issues/8317 
    local t = json.decode('{"key":null}')
    assert(t[key] == nil)

    print("Expected parsing errors ->")
    -- Truncated containers must report the end position, not an uninitialized token index.
    assert_error("[", "Expected value but found T_END at character 2")
    test_syntax_error("]")
    assert_error("{", "Expected object key string but found T_END at character 2")
    assert_error("", "Expected value but found T_END at character 1")

    test_syntax_error("{")
    test_syntax_error("Null")
    test_syntax_error("NULL")
    test_syntax_error("True")
    test_syntax_error("TRUE")
    test_syntax_error("False")
    test_syntax_error("FALSE")
    test_syntax_error("defold")
    test_syntax_error("0.d3")
    test_syntax_error("{1 2 3}")
    test_syntax_error("{1: 2, 3}")
    test_syntax_error("{ response = \"ok\" }")
    test_syntax_error("{ 'data': 'asd' }")

    print("<- end of expected parsing errors.")

    test_json_invalid_primitive()
    test_json_valid_primitive()
end

function assert_string(expected, actual)
    if expected ~= actual then
        print("Expected: '" .. expected .. "'")
        print("Got:      '" .. actual .. "'")
    end
    assert(expected == actual)
end

function assert_startswith(needle, haystack)
    local ret = startswith(needle, haystack)
    if not ret then
        print("Couldn't find: '" .. needle .. "'")
        print("in string '" .. haystack .. "'")
    end
    assert(ret)
end

function test_json_encode()
    -- empty table becomes empty dict
    assert(json.encode({}) == "{}")
    assert(json.encode({}, { encode_empty_table_as_object = true }) == "{}")
    assert(json.encode({}, { encode_empty_table_as_object = false }) == "[]")
    -- table with nested table(s) becomes array of tables
    assert(json.encode({{}}) == "[{}]")
    -- table with elements becomes array of elements
    assert(json.encode({1,2,3}) == "[1,2,3]")
    -- a single nil element becomes null
    assert(json.encode(nil) == "null")
    -- however, a table with nil becomes empty table
    assert(json.encode({nil}) == "{}")
    -- interleaved nil becomes a null element (note: not in quotes)
    assert(json.encode({1,nil,2}) == "[1,null,2]")

    local v3_enc = json.encode(vmath.vector3()) -- user data
    assert_string("\"vmath.vector3(0, 0, 0)\"", v3_enc)

    local f_enc = json.encode(function() end) -- function
    assert_startswith("\"function: ", f_enc)

    -- Test passing no arguments, which should fail
    local ret, msg = pcall(function() json.encode() end)
    assert(ret == false)
end

function test_json_decode_encode()
    local tbl      = { x = 100, y == 200, sub = { z = 10, w = 20 } }
    local tbl_json = json.encode(tbl)
    local tbl_lua  = json.decode(tbl_json)

    assert(tbl.x     == tbl_lua.x)
    assert(tbl.y     == tbl_lua.y)
    assert(tbl.sub.z == tbl_lua.sub.z)
    assert(tbl.sub.w == tbl_lua.sub.w)
end

-- Preserve existing Lua JSON behavior while decoding into a reusable buffer.
function test_json_decoder_compatibility()
    assert(json.decode('{"key":1,"\\u006bey":2}').key == 2)
    assert(json.decode('{"a\\u0000b":"x\\u0000y"}')["a\0b"] == "x\0y")
    assert(json.decode('"' .. string.char(255) .. '"') == string.char(255))
    assert(json.decode("0x10") == 16)
    assert(json.decode("1e309") == math.huge)
    assert(1 / json.decode("-0") == -math.huge)
    local deep = json.decode(string.rep("[", 65) .. "1" .. string.rep("]", 65))
    for i = 1, 65 do deep = deep[1] end
    assert(deep == 1)
    local escaped = string.rep("long prefix ", 30) .. string.rep('\\n\\u0000\\ud83d\\ude00', 1024)
    local expected = string.rep("long prefix ", 30) .. string.rep("\n\0" .. string.char(240,159,152,128), 1024)
    local values = json.decode('["' .. escaped .. '","plain","\\t","","\\u0061"]')
    assert(values[1] == expected and values[2] == "plain" and values[3] == "\t" and values[4] == "" and values[5] == "a")
    for _, invalid in ipairs({'"\\u"', '"\\ud800"', '"\\udc00"', '"\\x"', '["ok",', '{"x":1,}'}) do
        assert(not pcall(json.decode, invalid))
    end
end

-- Retain legacy Lua numeric syntax, raw string bytes, and the container depth boundary without a fallback parser.
function test_json_decoder_legacy_syntax()
    for text, expected in pairs({["+1.25"]=1.25, ["-.5"]=-0.5, ["01."]=1, ["01.e+2"]=100,
                                 ["0XfF"]=255, ["-0x1.fp+3"]=-15.5, ["0x.8"]=0.5,
                                 ["0x1.00000000000008001p0"]=1.0000000000000002}) do
        assert(json.decode(text) == expected, text)
    end
    assert(json.decode("0x1p-1074") == 2^-1074)
    assert(json.decode("0x1p-1075") == 0)
    assert(json.decode("0x1.00000000000000001p-1075") == 2^-1074)
    assert(1 / json.decode("-0.") == -math.huge)
    for _, text in ipairs({"inf", "Infinity", "+INF", "1e9999", "0x1p99999"}) do
        assert(json.decode(text) == math.huge)
    end
    for _, text in ipairs({"nan", "NaN(payload)", "-NaN", "+NAN"}) do
        local value = json.decode(text)
        assert(value ~= value)
    end
    for byte = 1, 31 do
        local value = "x" .. string.char(byte) .. "y"
        assert(json.decode('"' .. value .. '"') == value)
        assert(json.decode('"\\n' .. value .. '"') == "\n" .. value)
    end
    local deep = json.decode(string.rep("[", 1000) .. "1" .. string.rep("]", 1000))
    for i = 1, 1000 do deep = deep[1] end
    assert(deep == 1)
    assert(not pcall(json.decode, string.rep("[", 1001) .. "0" .. string.rep("]", 1001)))
    for _, text in ipairs({".5", "+", "-", "1e+", "0x", "0x1p", '"x\0y"', "[1\0]"}) do
        assert(not pcall(json.decode, text), text)
    end
    assert(json.decode("[1,2]\0ignored")[2] == 2)
end

-- Preserve encoder byte escaping, fourteen-digit numbers, table classification, and error recovery.
function test_json_encoder_compatibility()
    assert(json.encode(-1 / math.huge) == "-0")
    assert(json.encode(99999999999999) == "99999999999999")
    assert(json.encode(-99999999999999) == "-99999999999999")
    assert(json.encode(100000000000000) == "1e+14")
    assert(json.encode(math.pi) == "3.1415926535898")
    assert(json.encode(0.00001) == "1e-05")
    assert(json.encode("\0\31/" .. string.char(127, 255)) == '"\\u0000\\u001f\\/\\u007f' .. string.char(255) .. '"')
    local short = { [8] = "b", [9] = "t", [10] = "n", [12] = "f", [13] = "r", [34] = '"', [47] = "/", [92] = "\\" }
    for byte = 0, 255 do
        local value = string.char(byte)
        local expected = short[byte] and "\\" .. short[byte] or
                         (byte < 32 or byte == 127) and string.format("\\u%04x", byte) or value
        assert(json.encode(value) == '"' .. expected .. '"')
    end
    local escaped = "\0\n\t/\\\"" .. string.char(127, 255)
    for prefix = 0, 530 do
        local value = string.rep("a", prefix) .. escaped .. string.rep("z", 700)
        assert(json.decode(json.encode(value)) == value)
    end
    assert(json.encode({[0] = 7}) == '{"0":7}')
    assert(json.encode({[1] = 1, [10] = 2}) == "[1,null,null,null,null,null,null,null,null,2]")
    assert(json.encode(setmetatable({1,2}, {__index=function() error("must not call") end})) == "[1,2]")
    local cycle = {}; cycle.self = cycle
    for _, value in ipairs({cycle, {[11]=1}, {[false]=1}, math.huge, -math.huge, 0/0}) do
        assert(not pcall(json.encode, value))
        assert(json.encode({1,2,3}) == "[1,2,3]")
    end
end

-- Decoded arrays must remain writable tables as they grow, including null holes.
function test_json_decoder_array_growth()
    for _, text in ipairs({"[]", "[ ]", "{}", "{ }"}) do
        local value = json.decode(text)
        assert(type(value) == "table" and next(value) == nil)
        value[1] = 7
        assert(value[1] == 7)
        value.key = "written"
        assert(value.key == "written")
    end
    for _, size in ipairs({1, 2, 3, 4, 5, 17, 129}) do
        local numbers = {}
        for i = 1, size do numbers[i] = tostring(i) end
        local value = json.decode("[" .. table.concat(numbers, ",") .. "]")
        assert(type(value) == "table" and #value == size)
        for i = 1, size do assert(value[i] == i) end
        value[size + 1] = size + 1
        value[1] = -1
        assert(#value == size + 1 and value[1] == -1)
        local copy = json.decode(json.encode(value))
        assert(copy[1] == -1 and copy[size + 1] == size + 1)
    end
    for _, keep_null in ipairs({false, true}) do
        local value = json.decode('[null,1,null,{},[],[2,3,4],null]',
                                  {decode_null_as_userdata = keep_null})
        local expected_null = keep_null and json.null or nil
        assert(value[1] == expected_null and value[3] == expected_null and value[7] == expected_null)
        assert(value[2] == 1 and next(value[4]) == nil and next(value[5]) == nil)
        assert(#value[6] == 3 and value[6][3] == 4)
        value[3] = "filled"
        value[5][1] = "nested write"
        assert(value[3] == "filled" and value[5][1] == "nested write")
    end
    for _, text in ipairs({"[", "[1,", "[1,2,", "[1,2,3,]", '{"x":[1,2,}'}) do
        assert(not pcall(json.decode, text))
        assert(json.decode("[1,2,3]")[3] == 3)
    end
end


-- Decoded objects must retain fields and normal writes as their hash storage grows.
function test_json_decoder_object_growth()
    for _, size in ipairs({1, 2, 3, 4, 5, 8, 9, 17, 129}) do
        local fields = {}
        for i = 1, size do fields[i] = '"key' .. i .. '":' .. i end
        local value = json.decode("{" .. table.concat(fields, ",") .. "}")
        assert(type(value) == "table")
        local count = 0
        for key, number in pairs(value) do
            assert(key == "key" .. number)
            count = count + 1
        end
        assert(count == size)
        for i = 1, size do assert(value["key" .. i] == i) end
        value.key1 = -1
        value.extra = "written"
        local copy = json.decode(json.encode(value))
        assert(copy.key1 == -1 and copy.extra == "written")
        value.key1 = nil
        assert(value.key1 == nil and value.extra == "written")
    end
    for _, keep_null in ipairs({false, true}) do
        local options = {decode_null_as_userdata = keep_null}
        local value = json.decode('{"a":1,"a":null,"b":{},"c":{"d":[1,2,3]}}', options)
        local expected_null = keep_null and json.null or nil
        assert(value.a == expected_null and next(value.b) == nil and value.c.d[3] == 3)
        value.b.key = "nested write"
        assert(value.b.key == "nested write")
        local only_null = json.decode('{"a":null}', options)
        assert(only_null.a == expected_null)
        if not keep_null then assert(next(only_null) == nil) end
    end
    for _, text in ipairs({"{", '{"key":', '{"key":1,', '{"key":1,}', '{"key":{"x":]}'}) do
        assert(not pcall(json.decode, text))
        assert(json.decode('{"key":7}').key == 7)
    end
end


-- Whitespace at value/container boundaries must preserve values and error positions.
function test_json_decoder_whitespace()
    local tokens = {"", "{", '"values"', ":", "[", "1", ",", "true", ",",
                    "{", '"x"', ":", '" a b "', "}", ",", "[", "]", ",", "null", "]",
                    ",", '"empty"', ":", "{", "}", "}", ""}
    for _, gap in ipairs({"", " ", "\t", "\r", "\n", " \t\r\n "}) do
        local text = table.concat(tokens, gap)
        for _, keep_null in ipairs({false, true}) do
            local value = json.decode(text, {decode_null_as_userdata = keep_null})
            local expected_null = keep_null and json.null or nil
            assert(value.values[1] == 1 and value.values[2] == true)
            assert(value.values[3].x == " a b " and next(value.values[4]) == nil)
            assert(value.values[5] == expected_null and next(value.empty) == nil)
            value.empty.key = "written"
            assert(value.empty.key == "written")
        end
        assert(json.decode(gap .. "123" .. gap) == 123)
        assert(json.decode(gap .. "false" .. gap) == false)
        assert(json.decode(gap .. '" a b "' .. gap) == " a b ")
        assert(json.decode(gap .. "[1,2]" .. gap .. "\0ignored")[2] == 2)
    end
    assert_error(" \t ", "Expected value but found T_END at character 4")
    assert_error(" \t [ 1, \n ]", "Expected value but found T_ARR_END at character 11")
    assert_error('{ \n "x" \t : \r }', "Expected value but found T_OBJ_END at character 15")
    assert_error(" \n [1 \t 2]", "Expected comma or array end but found T_NUMBER at character 9")
    assert_error(' { "x" \t 7 }', "Expected colon but found T_NUMBER at character 10")
    assert_error(" [1, \n ", "Expected value but found T_END at character 8")
    for _, invalid_gap in ipairs({string.char(11), string.char(12)}) do
        assert(not pcall(json.decode, invalid_gap .. "1"))
        assert(not pcall(json.decode, "[1," .. invalid_gap .. "2]"))
    end
end


function test_json()
    test_json_decode()
    test_json_encode()
    test_json_decode_encode()
    test_json_decoder_whitespace()
    test_json_decoder_object_growth()
    test_json_decoder_array_growth()
    test_json_decoder_compatibility()
    test_json_decoder_legacy_syntax()
    test_json_encoder_compatibility()
end

functions = { test_json = test_json }
