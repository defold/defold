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

--- clamp function

local num1 = 34
local num2 = -17.8
local num3 = 87.9
assert(vmath.clamp(num1, 0, 50) == 34)
assert(vmath.clamp(num2, 0, 50) == 0)
assert(vmath.clamp(num3, 0, 50) == 50)

local vec3_1 = vmath.vector3(-345, 55.6, -0.5)
local vec3_2 = vmath.vector3(167, 23, 1.3)
local vec3_3 = vmath.vector3(189.3, 333, 1)
local vec3_min = vmath.vector3(-12, -22, 0)
local vec3_max = vmath.vector3(125, 65, 1)


assert(vmath.clamp(vec3_1, vec3_min, vec3_max) == vmath.vector3(-12, 55.6, 0))
assert(vmath.clamp(vec3_2, vec3_min, vec3_max) == vmath.vector3(125, 23, 1))

print(vmath.clamp(vec3_3, vec3_min, vec3_max))
assert(vmath.clamp(vec3_3, vec3_min, vec3_max) == vmath.vector3(125, 65, 1))
assert(vmath.clamp(vec3_3, 0, 1) == vmath.vector3(1, 1, 1))

local vec4_1 = vmath.vector4(1, 2, 3, 4)
local vec4_2 = vmath.vector4(-0.5, -0.3, -0.1, 0)
local vec4_3 = vmath.vector4(34.3, 22.1, 90.4, 1)
local vec4_min = vmath.vector4(0, 0, 0, 1)
local vec4_max = vmath.vector4(5, 5, 5, 1)

print(vmath.clamp(vec4_1, vec4_min, vec4_max))
assert(vmath.clamp(vec4_1, vec4_min, vec4_max) == vmath.vector4(1, 2, 3, 1))

print(vmath.clamp(vec4_2, vec4_min, vec4_max))
assert(vmath.clamp(vec4_2, vec4_min, vec4_max) == vmath.vector4(0, 0, 0, 1))

print(vmath.clamp(vec4_3, vec4_min, vec4_max))
assert(vmath.clamp(vec4_3, vec4_min, vec4_max) == vmath.vector4(5, 5, 5, 1))

print(vmath.clamp(vec4_1, 86.2, 200.1))
assert(vmath.clamp(vec4_1, 86.2, 200.1) == vmath.vector4(86.2, 86.2, 86.2, 86.2))