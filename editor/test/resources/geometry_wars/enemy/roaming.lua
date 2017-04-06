local M = {}

function M.init(max_speed, max_angle_speed)
	local angle = math.random() * math.pi * 2
	local state = {
		angle_speed = 0,
		dir = vmath.vector3(math.cos(angle), math.sin(angle), 0),
		max_speed = max_speed,
		max_angle_speed = max_angle_speed
	}
	return state
end

function M.update(state, dt)
	local max_angle_speed = state.max_angle_speed
	local angle_speed = state.angle_speed
	local test_speed = (math.random() - 0.5) * 2 * max_angle_speed
	local delta = test_speed - angle_speed
	if delta >= 0 then
		delta = dt * max_angle_speed
	else
		delta = -dt * max_angle_speed
	end
	angle_speed = angle_speed + delta
	angle_speed = math.max(angle_speed, -max_angle_speed)
	angle_speed = math.min(angle_speed, max_angle_speed)
	local dir = state.dir
	local angle = math.atan2(dir.y, dir.x)
	angle = angle + angle_speed * dt
	state.dir = vmath.vector3(math.cos(angle), math.sin(angle), 0)
	state.angle_speed = angle_speed
	return dir * state.max_speed
end

return M