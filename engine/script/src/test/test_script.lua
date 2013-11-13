-- pprint
print("testing pprint")
pprint(123)
m = vmath.matrix4()
local t = {
    foo = 123,
    { x = 1,
      y = { n = vmath.vector3(), q = vmath.quat(0,0,0,1), m = vmath.matrix4() },
      z = 3 },
    "hello"
}
pprint(t)
