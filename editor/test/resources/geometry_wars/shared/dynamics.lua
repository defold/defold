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