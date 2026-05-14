-- Copyright 2020-2026 The Defold Foundation
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

function test_hash(hash_value, hash_value_hex)
    assert(hash_value == hash("test_value"))
    assert(hash_value == hash(hash_value))
    assert(hash_value_hex == hash_to_hex(hash_value))
    assert(hash("test_string1") ~= hash("test_string2") )
    assert(hash_to_hex(hash("test_string1")) ~= hash_to_hex(hash("test_string2")))
    assert(hash("1976") == hash(1976))
    assert(hash("3.14") == hash(3.14))
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
