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

-- pprint
print("testing pprint")
pprint(123)
pprint("smore")
m = vmath.matrix4()
local t = {
    foo = 123,
    { x = 1,
      y = { n = vmath.vector3(), q = vmath.quat(0,0,0,1), m = vmath.matrix4() },
      z = 3 },
    "hello",
    { }
}
pprint(t)

pprint(t, "more", {a = 1, b = 2, c = 3}, 77, 78, 79, 80)

local n = 5
pprint(n)
