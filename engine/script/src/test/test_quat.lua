-- constructor
local q = vmath.quat(1, 2, 3, 4)

-- index
assert(q.x == 1, "q.x is not 1")
assert(q.y == 2, "q.y is not 2")
assert(q.z == 3, "q.z is not 3")
assert(q.w == 4, "q.w is not 4")

-- constructor
q = vmath.quat()
assert(q.x == 0, "q.x is not 0")
assert(q.y == 0, "q.y is not 0")
assert(q.z == 0, "q.z is not 0")
assert(q.w == 1, "q.w is not 1")

-- copy constructor
local q1 = vmath.quat(q)
-- equals
assert(q == q1, "q != q1")
-- concat (long fract-expansions)
q.x = math.pi + 1000
q.y = math.pi + 1000
q.z = math.pi + 1000
q.w = math.pi + 1000
print("q: " .. q)

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
q = vmath.quat(math.sin(math.pi/4), 0, 0, math.cos(math.pi/4)) * vmath.quat(math.sin(math.pi/4), 0, 0, math.cos(math.pi/4))
assert(math.abs(q.x - 1) < 0.000001 and q.y == 0 and q.z == 0 and q.w == 0, "quat * quat")

-- quat_from_to
q = vmath.quat_from_to(vmath.vector3(1, 0, 0), vmath.vector3(0, 1, 0))
assert(q.x == 0 and q.y == 0 and math.abs(q.z - math.sin(math.pi/4)) < 0.000001 and math.abs(q.w - math.cos(math.pi/4)) < 0.000001, "quat_from_start_to_end")

-- quat_axis_angle
q = vmath.quat_axis_angle(vmath.vector3(0, 0, 1), math.pi/2)
assert(q.x == 0 and q.y == 0 and math.abs(q.z - math.sin(math.pi/4)) < 0.000001 and math.abs(q.w - math.cos(math.pi/4)) < 0.000001, "quat_from_axis_angle")

-- quat_basis
q = vmath.quat_basis(vmath.vector3(1, 0, 0), vmath.vector3(0, 1, 0), vmath.vector3(0, 0, 1))
assert(q.x == 0 and q.y == 0 and q.z == 0 and q.w == 1, "quat_from_basis")

-- quat_rotation_x
q = vmath.quat_rotation_x(math.pi)
assert(q.x == 1 and q.y == 0 and q.z == 0 and math.abs(q.w) < 0.000001, "quat_from_rotation_x")

-- quat_rotation_y
q = vmath.quat_rotation_y(math.pi)
assert(q.x == 0 and q.y == 1 and q.z == 0 and math.abs(q.w) < 0.000001, "quat_from_rotation_y")

-- quat_rotation_z
q = vmath.quat_rotation_z(math.pi)
assert(q.x == 0 and q.y == 0 and q.z == 1 and math.abs(q.w) < 0.000001, "quat_from_rotation_z")

-- conj
q = vmath.conj(vmath.quat(1, 2, 3, 4))
assert(q.x == -1 and q.y == -2 and q.z == -3 and q.w == 4, "conj")

-- rotate
local v = vmath.rotate(vmath.quat(0, 0, math.sin(math.pi/4), math.cos(math.pi/4)), vmath.vector3(1, 0, 0))
assert(math.abs(v.x) < 0.000001 and math.abs(v.y - 1) < 0.000001 and v.z == 0, "rotate")

-- lerp
local q = vmath.lerp(0.5, vmath.quat(1, 0, 0, 0), vmath.quat(0, -1, 0, 0))
assert(q.x == 0.5 and q.y == -0.5 and q.z == 0 and q.w == 0, "lerp")

-- slerp
q = vmath.slerp(0.5, vmath.quat(1, 0, 0, 0), vmath.quat(0, -1, 0, 0))
local sq2 = math.sqrt(2)
assert(math.abs(q.x - 0.5 * sq2) < 0.000001 and math.abs(q.y + 0.5 * sq2) < 0.000001 and q.z == 0 and q.w == 0, "slerp")

-- length_sqr
q = vmath.quat(1, 2, 3, 4)
assert(vmath.length_sqr(q) == 1 + 4 + 9 + 16, "length_sqr")

-- length
q = vmath.quat(1, 2, 3, 4)
assert(math.abs(vmath.length(q) - math.sqrt(30)) < 0.000001, "length")

-- normalize
q = vmath.normalize(vmath.quat(1, 2, 3, 4))
assert(math.abs(vmath.length_sqr(q) - 1) < 0.0000001, "normalized not unit length")
local t = 1 / vmath.length(vmath.quat(1, 2, 3, 4))
assert(math.abs(q.x - t) < 0.000001 and math.abs(q.y - 2*t) < 0.000001 and math.abs(q.z - 3*t) < 0.000001 and math.abs(q.w - 4*t) < 0.000001, "normalize")

