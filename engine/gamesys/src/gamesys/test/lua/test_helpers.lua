-- Copyright 2020-2024 The Defold Foundation
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

function assert_error(func)
    local r, err = pcall(func)
    assert_fn(not r, err)
end

function assert_not_error(func)
    local r, err = pcall(func)
    assert_fn(r, err)
end

function make_mat4(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p)
    local m4 = vmath.matrix4()
    m4.m00 = a
    m4.m10 = b
    m4.m20 = c
    m4.m30 = d
    m4.m01 = e
    m4.m11 = f
    m4.m21 = g
    m4.m31 = h
    m4.m02 = i
    m4.m12 = j
    m4.m22 = k
    m4.m32 = l
    m4.m03 = m
    m4.m13 = n
    m4.m23 = o
    m4.m33 = p
    return m4
end

function is_mat4(v)
    return pcall(function()
        return v.m00 + v.m11 + v.m22 + v.m33
    end)
end

function is_vec4(v)
    return pcall(function()
        return v.x + v.y + v.z + v.w
    end)
end

function is_vec3(v)
    return pcall(function()
        return v.x + v.y + v.z
    end)
end

function is_number(v)
    return type(v) == "number"
end

function is_table(v)
    return type(v) == "table"
end
