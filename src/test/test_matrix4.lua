-- constructor
local m = vmath.matrix4()

-- index
print(m)
assert(m.m00 == 1, "m.m00 is not 1")
assert(m.m10 == 0, "m.m10 is not 0")
assert(m.m20 == 0, "m.m20 is not 0")
assert(m.m30 == 0, "m.m30 is not 0")
assert(m.m01 == 0, "m.m01 is not 0")
assert(m.m11 == 1, "m.m11 is not 1")
assert(m.m21 == 0, "m.m21 is not 0")
assert(m.m31 == 0, "m.m31 is not 0")
assert(m.m02 == 0, "m.m02 is not 0")
assert(m.m12 == 0, "m.m12 is not 0")
assert(m.m22 == 1, "m.m22 is not 1")
assert(m.m32 == 0, "m.m32 is not 0")
assert(m.m03 == 0, "m.m03 is not 0")
assert(m.m13 == 0, "m.m13 is not 0")
assert(m.m23 == 0, "m.m23 is not 0")
assert(m.m33 == 1, "m.m33 is not 1")

-- copy constructor
m.m00 = 2
local m1 = vmath.matrix4(m)
-- equals
assert(m == m1, "m != m1")
-- concat
print("m: " .. m)

-- new index
m.m10 = 5
m.m11 = 6
m.m12 = 7
m.m13 = 8
assert(m.m10 == 5, "q.m10 is not 5")
assert(m.m11 == 6, "q.m11 is not 6")
assert(m.m12 == 7, "q.m12 is not 7")
assert(m.m13 == 8, "q.m13 is not 8")

-- mul
m = vmath.matrix4()
m1 = vmath.matrix4(m)
m.m00 = 1
m.m11 = 2
m.m22 = 3
m.m33 = 4
m1.m00 = 5
m1.m11 = 6
m1.m22 = 7
m1.m33 = 8
m2 = m * m1
assert(m2.m00 == 5, "mul")
assert(m2.m11 == 12, "mul")
assert(m2.m22 == 21, "mul")
assert(m2.m33 == 32, "mul")
