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

M.drag = 0.3

function M.init()
	return {acc = vmath.vector3()}
end

function M.update(data, velocity, dt)
	-- integrate
	local v = velocity + data.acc * dt
	-- drag
	v = (1 - M.drag * dt) * v
	-- clear
	data.acc = vmath.vector3()
	return v
end

function M.add_acc(data, acc)
	data.acc = data.acc + acc
end

function M.on_message(data, message_id, message, sender)
	if message_id == hash("add_force") then
		M.add_acc(data, message.force)
	end
end

return M