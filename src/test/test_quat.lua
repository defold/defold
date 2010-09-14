-- constructor
local q = vec_math.quat(1, 2, 3, 4)

-- index
assert(q.x == 1, "q.x is not 1")
assert(q.y == 2, "q.y is not 2")
assert(q.z == 3, "q.z is not 3")
assert(q.w == 4, "q.w is not 4")

-- new index
q.x = 5
q.y = 6
q.z = 7
q.w = 8
assert(q.x == 5, "q.x is not 5")
assert(q.y == 6, "q.y is not 6")
assert(q.z == 7, "q.z is not 7")
assert(q.w == 8, "q.w is not 8")

-- mul
q = vec_math.quat(math.sin(math.pi/4), 0, 0, math.cos(math.pi/4)) * vec_math.quat(math.sin(math.pi/4), 0, 0, math.cos(math.pi/4))
assert(math.abs(q.x - 1) < 0.000001 and q.y == 0 and q.z == 0 and q.w == 0, "quat * quat")

-- quat_from_start_to_end
q = vec_math.quat_from_start_to_end(vec_math.vector3(1, 0, 0), vec_math.vector3(0, 1, 0))
assert(q.x == 0 and q.y == 0 and math.abs(q.z - math.sin(math.pi/4)) < 0.000001 and math.abs(q.w - math.cos(math.pi/4)) < 0.000001, "quat_from_start_to_end")

-- quat_from_axis_angle
q = vec_math.quat_from_axis_angle(vec_math.vector3(0, 0, 1), math.pi/2)
assert(q.x == 0 and q.y == 0 and math.abs(q.z - math.sin(math.pi/4)) < 0.000001 and math.abs(q.w - math.cos(math.pi/4)) < 0.000001, "quat_from_axis_angle")

-- quat_from_basis
q = vec_math.quat_from_basis(vec_math.vector3(1, 0, 0), vec_math.vector3(0, 1, 0), vec_math.vector3(0, 0, 1))
assert(q.x == 0 and q.y == 0 and q.z == 0 and q.w == 1, "quat_from_basis")

-- quat_from_rotation_x
q = vec_math.quat_from_rotation_x(math.pi)
assert(q.x == 1 and q.y == 0 and q.z == 0 and math.abs(q.w) < 0.000001, "quat_from_rotation_x")

-- quat_from_rotation_y
q = vec_math.quat_from_rotation_y(math.pi)
assert(q.x == 0 and q.y == 1 and q.z == 0 and math.abs(q.w) < 0.000001, "quat_from_rotation_y")

-- quat_from_rotation_z
q = vec_math.quat_from_rotation_z(math.pi)
assert(q.x == 0 and q.y == 0 and q.z == 1 and math.abs(q.w) < 0.000001, "quat_from_rotation_z")

-- conj
q = vec_math.conj(vec_math.quat(1, 2, 3, 4))
assert(q.x == -1 and q.y == -2 and q.z == -3 and q.w == 4, "conj")

-- rotate
local v = vec_math.rotate(vec_math.quat(0, 0, math.sin(math.pi/4), math.cos(math.pi/4)), vec_math.vector3(1, 0, 0))
assert(math.abs(v.x) < 0.000001 and math.abs(v.y - 1) < 0.000001 and v.z == 0, "rotate")

-- lerp
local q = vec_math.lerp(0.5, vec_math.quat(1, 0, 0, 0), vec_math.quat(0, -1, 0, 0))
assert(q.x == 0.5 and q.y == -0.5 and q.z == 0 and q.w == 0, "lerp")

-- slerp
q = vec_math.slerp(0.5, vec_math.quat(1, 0, 0, 0), vec_math.quat(0, -1, 0, 0))
local sq2 = math.sqrt(2)
assert(math.abs(q.x - 0.5 * sq2) < 0.000001 and math.abs(q.y + 0.5 * sq2) < 0.000001 and q.z == 0 and q.w == 0, "slerp")
