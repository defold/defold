-- constructor
local v = vmath.vector3(1, 2, 3)

-- index
assert(v.x == 1, "v.x is not 1")
assert(v.y == 2, "v.y is not 2")
assert(v.z == 3, "v.z is not 3")

-- splat constructor
local v = vmath.vector3(10)

-- index
assert(v.x == 10, "v.x is not 10")
assert(v.y == 10, "v.y is not 10")
assert(v.z == 10, "v.z is not 10")

-- empty constructor
v = vmath.vector3()

-- index
assert(v.x == 0, "v.x is not 0")
assert(v.y == 0, "v.y is not 0")
assert(v.z == 0, "v.z is not 0")

-- copy constructor
local v1 = vmath.vector3(v)
-- equals
assert(v == v1, "v != v1")
-- concat (long fract-expansions)
v.x = math.pi + 1000
v.y = math.pi + 1000
v.z = math.pi + 1000
print("v: " .. v)

-- new index
v.x = 4
v.y = 5
v.z = 6
assert(v.x == 4, "v.x is not 4")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 6, "v.z is not 6")

-- add
v = vmath.vector3(1, 2, 3) + vmath.vector3(2, 3, 4)
assert(v.x == 3, "v.x is not 3")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 7, "v.z is not 7")

-- sub
v = vmath.vector3(2, 3, 4) - vmath.vector3(1, 2, 3)
assert(v.x == 1, "v.x is not 1")
assert(v.y == 1, "v.y is not 1")
assert(v.z == 1, "v.z is not 1")

-- mul
v = vmath.vector3(1, 2, 3) * 2
assert(v.x == 2, "v.x is not 2")
assert(v.y == 4, "v.y is not 4")
assert(v.z == 6, "v.z is not 6")
v = 2 * vmath.vector3(1, 2, 3)
assert(v.x == 2, "v.x is not 2")
assert(v.y == 4, "v.y is not 4")
assert(v.z == 6, "v.z is not 6")

-- unm
v = -vmath.vector3(1, 2, 3)
assert(v.x == -1, "v.x is not -1")
assert(v.y == -2, "v.y is not -2")
assert(v.z == -3, "v.z is not -3")

-- dot
local s = vmath.dot(vmath.vector3(1, 2, 3), vmath.vector3(2, 3, 4))
assert(s == 2 + 6 + 12, "dot")

-- length_sqr
v = vmath.vector3(1, 2, 3)
assert(vmath.length_sqr(v) == 1 + 4 + 9, "length_sqr")

-- length
v = vmath.vector3(2, 6, 9)
assert(vmath.length(v) == 11, "length")

-- normalize
v = vmath.normalize(vmath.vector3(1.2, 1.6, 0))
assert(math.abs(vmath.length_sqr(v) - 1) < 0.0000001, "normalized not unit length")
assert(math.abs(v.x - 0.6) < 0.000001 and math.abs(v.y - 0.8) < 0.000001, "normalize")

-- cross
v = vmath.cross(vmath.vector3(1, 0, 0), vmath.vector3(0, 1, 0))
assert(v.x == 0 and v.y == 0 and v.z == 1, "cross")

-- lerp
v = vmath.lerp(0.5, vmath.vector3(1, 0, 0), vmath.vector3(0, -1, 0))
assert(v.x == 0.5 and v.y == -0.5 and v.z == 0, "lerp")

-- slerp
v = vmath.slerp(0.5, vmath.vector3(1, 0, 0), vmath.vector3(0, -1, 0))
local sq2 = math.sqrt(2)
assert(math.abs(v.x - 0.5 * sq2) < 0.000001 and math.abs(v.y + 0.5 * sq2) < 0.000001 and v.z == 0, "slerp")

-- mul_per_elem
v = vmath.mul_per_elem(vmath.vector3(1,2,3), vmath.vector3(5,6,7))
assert(v.x == 5, "v.x is not 5")
assert(v.y ==12, "v.y is not 12")
assert(v.z ==21, "v.z is not 21")