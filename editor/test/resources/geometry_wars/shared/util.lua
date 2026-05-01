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

local M = {}

function M.hsl(h, s, l)
	c = (1 - math.abs(2 * l - 1)) * s
	hp = h / 60
	x = c * (1 - math.abs((hp % 2) - 1))
	r = 0
	g = 0
	b = 0
	if hp < 1 then
		r = c
		g = x
		b = 0
	elseif hp < 2 then
		r = x
		g = c
		b = 0
	elseif hp < 3 then
		r = 0
		g = c
		b = x
	elseif hp < 4 then
		r = 0
		g = x
		b = c
	elseif hp < 5 then
		r = x
		g = 0
		b = c
	elseif hp < 6 then
		r = c
		g = 0
		b = x
	end
	m = l - 0.5 * c
	return vmath.vector4(r+m, g+m, b+m, 1)
end

return M