-- Copyright 2020-2022 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
-- 
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
-- 
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.

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

function test_json_encode()
    -- empty table becomes empty dict
    assert(json.encode({}) == "{}")
    -- table with nested table(s) becomes array of tables
    assert(json.encode({{}}) == "[{}]")
    -- table with elements becomes array of elements
    assert(json.encode({1,2,3}) == "[1,2,3]")
    -- a single nil element becomes null
    assert(json.encode(nil) == "null")
    -- however, a table with nil becomes empty table
    assert(json.encode({nil}) == "{}")
    -- interleaved nil becomse a null element (note: not in quotes)
    assert(json.encode({1,nil,2}) == "[1,null,2]")

    local v3_enc = json.encode(vmath.vector3())
    assert(startswith("\"object at", v3_enc))

    local f_enc = json.encode(function() end)
    assert(startswith("\"object at", f_enc))

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

function test_json()
    test_json_decode()
    test_json_encode()
    test_json_decode_encode()
end

functions = { test_json = test_json }
