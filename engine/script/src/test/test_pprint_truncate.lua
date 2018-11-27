-- pprint
print("testing pprint truncate")

local f = {}
for i = 1,24 do
  f[i] = "Line "..tostring(i)..": This is a very long string that should be truncated in the output when using pprint, we should see a message rearding truncation at the end of this output"
end

pprint(f)
