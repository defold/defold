-- constructor
local v = vec_math.vector3(1, 2, 3)

-- index
assert(v.x == 1, "v.x is not 1")
assert(v.y == 2, "v.y is not 2")
assert(v.z == 3, "v.z is not 3")

-- copy constructor
local v1 = vec_math.vector3(v)
-- equals
assert(v == v1, "v != v1")
-- concat
print("v: " .. v)

-- new index
v.x = 4
v.y = 5
v.z = 6
assert(v.x == 4, "v.x is not 4")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 6, "v.z is not 6")

-- add
v = vec_math.vector3(1, 2, 3) + vec_math.vector3(2, 3, 4)
assert(v.x == 3, "v.x is not 3")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 7, "v.z is not 7")

-- sub
v = vec_math.vector3(2, 3, 4) - vec_math.vector3(1, 2, 3)
assert(v.x == 1, "v.x is not 1")
assert(v.y == 1, "v.y is not 1")
assert(v.z == 1, "v.z is not 1")

-- mul
v = vec_math.vector3(1, 2, 3) * 2
assert(v.x == 2, "v.x is not 2")
assert(v.y == 4, "v.y is not 4")
assert(v.z == 6, "v.z is not 6")

-- unm
v = -vec_math.vector3(1, 2, 3)
assert(v.x == -1, "v.x is not -1")
assert(v.y == -2, "v.y is not -2")
assert(v.z == -3, "v.z is not -3")

-- dot
local s = vec_math.dot(vec_math.vector3(1, 2, 3), vec_math.vector3(2, 3, 4))
assert(s == 2 + 6 + 12, "dot")

-- length_sqr
v = vec_math.vector3(1, 2, 3)
assert(vec_math.length_sqr(v) == 1 + 4 + 9, "length_sqr")

-- length
v = vec_math.vector3(2, 6, 9)
assert(vec_math.length(v) == 11, "length")

-- normalize
v = vec_math.normalize(vec_math.vector3(1.2, 1.6, 0))
assert(vec_math.length_sqr(v) == 1, "normalized not unit length")
assert(math.abs(v.x - 0.6) < 0.000001 and math.abs(v.y - 0.8) < 0.000001, "normalize")

-- cross
v = vec_math.cross(vec_math.vector3(1, 0, 0), vec_math.vector3(0, 1, 0))
assert(v.x == 0 and v.y == 0 and v.z == 1, "cross")

-- lerp
v = vec_math.lerp(0.5, vec_math.vector3(1, 0, 0), vec_math.vector3(0, -1, 0))
assert(v.x == 0.5 and v.y == -0.5 and v.z == 0, "lerp")

-- slerp
v = vec_math.slerp(0.5, vec_math.vector3(1, 0, 0), vec_math.vector3(0, -1, 0))
local sq2 = math.sqrt(2)
assert(math.abs(v.x - 0.5 * sq2) < 0.000001 and math.abs(v.y + 0.5 * sq2) < 0.000001 and v.z == 0, "slerp")
