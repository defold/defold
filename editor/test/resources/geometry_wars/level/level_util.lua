-- Copyright 2020-2025 The Defold Foundation
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

local level_width = 1180
local level_height = 620
local screen_width = 1280
local screen_height = 720

function M.get_aabb()
	local padding_x = (screen_width - level_width) * 0.5
	local padding_y = (screen_height - level_height) * 0.5
	local level_min = vmath.vector3(padding_x, padding_y, 0)
	local level_max = vmath.vector3(screen_width - padding_x, screen_height - padding_y, 0)
	return level_min, level_max
end

function M.clamp(p)
	local min, max = M.get_aabb()
	if p.x < min.x then p.x = min.x end
	if p.y < min.y then p.y = min.y end
	if p.x > max.x then p.x = max.x end
	if p.y > max.y then p.y = max.y end
	return p
end

local function draw_box(min, max)
	local c = vmath.vector4(1, 1, 1, 1)
	msg.post("@render:", "draw_line", { start_point = vmath.vector3(min.x, min.y, 0), end_point = vmath.vector3(max.x, min.y, 0), color = c } )
	msg.post("@render:", "draw_line", { start_point = vmath.vector3(min.x, max.y, 0), end_point = vmath.vector3(max.x, max.y, 0), color = c } )
	msg.post("@render:", "draw_line", { start_point = vmath.vector3(min.x, min.y, 0), end_point = vmath.vector3(min.x, max.y, 0), color = c } )
	msg.post("@render:", "draw_line", { start_point = vmath.vector3(max.x, min.y, 0), end_point = vmath.vector3(max.x, max.y, 0), color = c } )
end

function M.draw()
	local min, max = M.get_aabb()
	draw_box(min, max)
	local margin = 5
	min.x = min.x - margin
	min.y = min.y - margin
	max.x = max.x + margin
	max.y = max.y + margin
	draw_box(min, max)
end

return M