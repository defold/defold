-- constructor
local v = vmath.vector4(1, 2, 3, 4)

-- index
assert(v.x == 1, "v.x is not 1")
assert(v.y == 2, "v.y is not 2")
assert(v.z == 3, "v.z is not 3")
assert(v.w == 4, "v.w is not 4")

-- splat constructor
local v = vmath.vector4(10)

-- index
assert(v.x == 10, "v.x is not 10")
assert(v.y == 10, "v.y is not 10")
assert(v.z == 10, "v.z is not 10")
assert(v.w == 10, "v.w is not 10")

-- empty constructor
v = vmath.vector4()

-- index
assert(v.x == 0, "v.x is not 0")
assert(v.y == 0, "v.y is not 0")
assert(v.z == 0, "v.z is not 0")
assert(v.w == 0, "v.w is not 0")

-- copy constructor
local v1 = vmath.vector4(v)
-- equals
assert(v == v1, "v != v1")
-- concat (long fract-expansions)
v.x = math.pi + 1000
v.y = math.pi + 1000
v.z = math.pi + 1000
v.w = math.pi + 1000
print("v: " .. v)

-- new index
v.x = 5
v.y = 6
v.z = 7
v.w = 8
assert(v.x == 5, "v.x is not 5")
assert(v.y == 6, "v.y is not 6")
assert(v.z == 7, "v.z is not 7")
assert(v.w == 8, "v.w is not 8")

-- add
v = vmath.vector4(1, 2, 3, 4) + vmath.vector4(2, 3, 4, 5)
assert(v.x == 3, "v.x is not 3")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 7, "v.z is not 7")
assert(v.w == 9, "v.w is not 9")

-- sub
v = vmath.vector4(2, 3, 4, 5) - vmath.vector4(1, 2, 3, 4)
assert(v.x == 1, "v.x is not 1")
assert(v.y == 1, "v.y is not 1")
assert(v.z == 1, "v.z is not 1")
assert(v.w == 1, "v.w is not 1")

-- mul
v = vmath.vector4(1, 2, 3, 4) * 2
assert(v.x == 2, "v.x is not 2")
assert(v.y == 4, "v.y is not 4")
assert(v.z == 6, "v.z is not 6")
assert(v.w == 8, "v.w is not 8")
v = 2 * vmath.vector4(1, 2, 3, 4)
assert(v.x == 2, "v.x is not 2")
assert(v.y == 4, "v.y is not 4")
assert(v.z == 6, "v.z is not 6")
assert(v.w == 8, "v.w is not 8")

-- unm
v = -vmath.vector4(1, 2, 3, 4)
assert(v.x == -1, "v.x is not -1")
assert(v.y == -2, "v.y is not -2")
assert(v.z == -3, "v.z is not -3")
assert(v.w == -4, "v.w is not -4")

-- dot
local s = vmath.dot(vmath.vector4(1, 2, 3, 4), vmath.vector4(2, 3, 4, 5))
assert(s == 2 + 6 + 12 + 20, "dot")

-- length_sqr
v = vmath.vector4(1, 2, 3, 4)
assert(vmath.length_sqr(v) == 1 + 4 + 9 + 16, "length_sqr")

-- length
v = vmath.vector4(1, 2, 3, 4)
assert(math.abs(vmath.length(v) - math.sqrt(30)) < 0.000001, "length")

-- normalize
v = vmath.normalize(vmath.vector4(1, 2, 3, 4))
assert(math.abs(vmath.length_sqr(v) - 1) < 0.0000001, "normalized not unit length")
t = 1 / vmath.length(vmath.vector4(1, 2, 3, 4))
assert(math.abs(v.x - t) < 0.000001 and math.abs(v.y - 2*t) < 0.000001 and math.abs(v.z - 3*t) < 0.000001 and math.abs(v.w - 4*t) < 0.000001, "normalize")

-- lerp
v = vmath.lerp(0.5, vmath.vector4(1, 0, 0, 0), vmath.vector4(0, -1, 0, 0))
assert(v.x == 0.5 and v.y == -0.5 and v.z == 0, "lerp")

-- slerp
v = vmath.slerp(0.5, vmath.vector4(1, 0, 0, 0), vmath.vector4(0, -1, 0, 0))
local sq2 = math.sqrt(2)
assert(math.abs(v.x - 0.5 * sq2) < 0.000001 and math.abs(v.y + 0.5 * sq2) < 0.000001 and v.z == 0, "slerp")

-- mul_per_elem
v = vmath.mul_per_elem(vmath.vector4(1,2,3,4), vmath.vector4(5,6,7,8))
assert(v.x == 5, "v.x is not 5")
assert(v.y ==12, "v.y is not 12")
assert(v.z ==21, "v.z is not 21")
assert(v.w ==32, "v.w is not 32")
