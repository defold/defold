assert(type(vmath.vector) == "function", "vmath.vector not a function")

-- empty constructor
local v = vmath.vector()

-- size
assert(#v == 0, "v is not of size 0")

-- empty table in constructor
local v = vmath.vector( { } )

-- size
assert(#v == 0, "v is not of size 0")

-- constructor
local v = vmath.vector( { 8, 7, 6 } )

-- index
assert(v[1] == 8, "v[1] is not 8")
assert(v[2] == 7, "v[2] is not 7")
assert(v[3] == 6, "v[3] is not 6")

-- size
assert(#v == 3, "v is not of size 3")

-- new index
v[1] = 4
v[2] = 5
v[3] = 6
assert(v[1] == 4, "v[1] is not 4")
assert(v[2] == 5, "v[2] is not 5")
assert(v[3] == 6, "v[3] is not 6")



-- comparison
local v1 = vmath.vector( { 1, 2, 3 } )
local v2 = vmath.vector( { 1, 2, 3 } )
assert(v1 == v1, "v1 is not equal to itself")
assert(v1 ~= v2, "separate v1 and v2 are equal")
