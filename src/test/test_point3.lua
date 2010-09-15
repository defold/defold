-- constructor
local v = vec_math.point3(1, 2, 3)

-- vector of point
local v2 = vec_math.vector3(vec_math.point3(1,2,3))
assert(v2.x == 1, "v2.x is not 1")
assert(v2.y == 2, "v2.y is not 2")
assert(v2.z == 3, "v2.z is not 3")

-- index
assert(v.x == 1, "v.x is not 1")
assert(v.y == 2, "v.y is not 2")
assert(v.z == 3, "v.z is not 3")

-- new index
v.x = 4
v.y = 5
v.z = 6
assert(v.x == 4, "v.x is not 4")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 6, "v.z is not 6")

-- add
v = vec_math.point3(1, 2, 3) + vec_math.vector3(2, 3, 4)
assert(v.x == 3, "v.x is not 3")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 7, "v.z is not 7")

v = vec_math.vector3(2, 3, 4) + vec_math.point3(1, 2, 3)
assert(v.x == 3, "v.x is not 3")
assert(v.y == 5, "v.y is not 5")
assert(v.z == 7, "v.z is not 7")

-- sub
v = vec_math.point3(2, 3, 4) - vec_math.point3(1, 2, 3)
assert(v.x == 1, "v.x is not 1")
assert(v.y == 1, "v.y is not 1")
assert(v.z == 1, "v.z is not 1")

