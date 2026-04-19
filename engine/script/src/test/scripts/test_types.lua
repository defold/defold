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

function test_types()
    -- check urls
    local empty_url = msg.url()
    local url = msg.url("main_collection", "/hero", "right_hand")
    local url_from_str = msg.url("main_collection:/hero2#left_hand")

    assert(types.is_url(empty_url), "Variable should be url")
    assert(types.is_url(url), "Variable should be url")
    assert(types.is_url(url_from_str), "Variable should be url")

    -- check vectors
    local empty_vector = vmath.vector()
    local vector = vmath.vector({ 1, 2, 4 })
    local long_vector = vmath.vector({ 10, 14, 19, -20, -11, -2, 132 })

    assert(types.is_vector(empty_vector), "Variable should be vector")
    assert(types.is_vector(vector), "Variable should be vector")
    assert(types.is_vector(long_vector), "Variable should be vector")

    -- check vector3
    local empty_vector3 = vmath.vector3()
    local non_empty_vector3 = vmath.vector3(13, 14.5, -7)

    assert(types.is_vector3(empty_vector3), "Variable should be vector3")
    assert(types.is_vector3(non_empty_vector3), "Variable should be vector3")

    -- check vector4
    local empty_vector4 = vmath.vector4()
    local non_empty_vector4 = vmath.vector4(33, -90, 34.2, 1.0)

    assert(types.is_vector4(empty_vector4), "Variable should be vector4")
    assert(types.is_vector4(non_empty_vector4), "Variable should be vector4")

    -- check matrix
    local identity_matrix = vmath.matrix4()
    local non_empty_matrix = vmath.matrix4_translation(non_empty_vector3)

    assert(types.is_matrix4(identity_matrix), "Variable should be matrix")
    assert(types.is_matrix4(non_empty_matrix), "Variable should be matrix")

    -- check quat
    local empty_quat = vmath.quat()
    local non_empty_quat = vmath.quat(1, 2, 3, 4)

    assert(types.is_quat(empty_quat), "Variable should be quaternion")
    assert(types.is_quat(non_empty_quat), "Variable should be quaternion")

    -- check hash
    local test_hash = hash("some_foo_text")

    assert(types.is_hash(test_hash), "Variable should be hash")

    -- check negative cases
    assert(not types.is_vector(non_empty_vector3), "Variable shouldn't be vector")
    assert(not types.is_vector(non_empty_vector4), "Variable shouldn't be vector")
    assert(not types.is_vector3(non_empty_matrix), "Variable shouldn't be vector3")
    assert(not types.is_hash(non_empty_quat), "Variable shouldn't be hash")
    assert(not types.is_quat(non_empty_vector3), "Variable shouldn't be quaternion")
    assert(not types.is_url(long_vector), "Variable shouldn't be url")
    assert(not types.is_matrix4(test_hash), "Variable shouldn't be matrix")
    assert(not types.is_vector4(identity_matrix), "Variable shouldn't be vector4")

    -- check empty arg
    assert(not types.is_vector(), "Empty arg is not vector")
    assert(not types.is_vector(), "Empty arg is not vector")
    assert(not types.is_vector3(), "Empty arg is not vector3")
    assert(not types.is_hash(), "Empty arg is not hash")
    assert(not types.is_quat(), "Empty arg is not quaternion")
    assert(not types.is_url(), "Empty arg is not url")
    assert(not types.is_matrix4(), "Empty arg is not matrix")
    assert(not types.is_vector4(), "Empty arg is not vector4")

    -- check nil
    assert(not types.is_vector(nil), "Nil is not vector")
    assert(not types.is_vector(nil), "Nil is not vector")
    assert(not types.is_vector3(nil), "Nil is not vector3")
    assert(not types.is_hash(nil), "Nil is not hash")
    assert(not types.is_quat(nil), "Nil is not quaternion")
    assert(not types.is_url(nil), "Nil is not url")
    assert(not types.is_matrix4(nil), "Nil is not matrix")
    assert(not types.is_vector4(nil), "Nil is not vector4")

    print("Types test done")
end

function test_excluded_types()
    -- check error when we exclude types script API
    assert(not types, "'types' modules shouldn't exist")
    print("Types null test done")
end

functions = { test_types = test_types, test_excluded_types = test_excluded_types }
