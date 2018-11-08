-- pprint
print("testing pprint")
pprint(123)
pprint("smore")
m = vmath.matrix4()
local t = {
    foo = 123,
    { x = 1,
      y = { n = vmath.vector3(), q = vmath.quat(0,0,0,1), m = vmath.matrix4() },
      z = 3 },
    "hello",
    { }
}
pprint(t)

pprint(t, "more", {a = 1, b = 2, c = 3}, 77, 78, 79, 80)

local n = 5
pprint(n)
