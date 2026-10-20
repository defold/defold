-- pprint
print("testing pprint with circular ref")

local s = {
    bar = "It was a dark and stormy night,",
    gnat = 0
}

local t = {
    foo = "an old man was telling stories of circular references.",
    gnu = s
}

s.gnat = t

pprint(t)
