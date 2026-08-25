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

function assert_fn(assert_that, error_string)
    if not assert_that then
        if error_string then
            print(error_string)
        end
        assert(assert_that)
    end
end

function assert_vec4(v_test,v_correct)
    local v = v_test.x == v_correct.x and
        v_test.y == v_correct.y and
        v_test.z == v_correct.z and
        v_test.w == v_correct.w
    local e = v and nil or tostring(v_test) .. " and " .. tostring(v_correct) .. " are not the same!"
    assert_fn(v, e)
end

function assert_vec3(v_test,v_correct)
    local v = v_test.x == v_correct.x and
        v_test.y == v_correct.y and
        v_test.z == v_correct.z
    local e = v and nil or tostring(v_test) .. " and " .. tostring(v_correct) .. " are not the same!"
    assert_fn(v, e)
end

function assert_mat4(m_test, m_correct)
	assert_vec4(m_test.c0, m_correct.c0)
	assert_vec4(m_test.c1, m_correct.c1)
	assert_vec4(m_test.c2, m_correct.c2)
	assert_vec4(m_test.c3, m_correct.c3)
end

function assert_number(v_test,v_correct)
    local v = v_test == v_correct
    local e = v and nil or tostring(v_test) .. " and " .. tostring(v_correct) .. " are not the same!"
    assert_fn(v, e)
end

function assert_not_nil(v_test)
    assert_fn(v_test ~= nil, "Value shouldn't be nil!")
end

function assert_error(func)
    local r, err = pcall(func)
    assert_fn(not r, err)
end

function assert_not_error(func)
    local r, err = pcall(func)
    assert_fn(r, err)
end
