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


print("testing callstack generation")

local function testfn_with_a_very_long_name_3(value1, value2, value3)
    return testmodule.foo(value1, value2, value3)
end

local function testfn_with_a_very_long_name_2(value1, value2)
    testfn_with_a_very_long_name_3(value1, value2, value1 + value2)
end

local function testfn_with_a_very_long_name_(value)
    if value == 0 then
        testfn_with_a_very_long_name_2(value*2, value*3+1)
    else
        testfn_with_a_very_long_name_(value - 1)
    end
end

function do_crash()
    print("OUTPUT:", testfn_with_a_very_long_name_(200))
end

