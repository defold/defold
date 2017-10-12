
local function save(path)
	print("Writing a save file with header version " .. TABLE_VERSION_CURRENT)

	local s = 'abc\44foo\0bar'
	print(s)

	local t = {}
	-- test binary string as both key and value
	t["binary\0string"] = "payload\1\0\2\3"
	t["vector3"] = vmath.vector3(1, 2, 3)
	t["vector4"] = vmath.vector4(4, 5, 6, 7)
	t["quat"] 	= vmath.quat(1,2,3,4)
	t["number"] = 0.5
	t["hash"] 	= hash("hashed value")
	t["url"] 	= msg.url("a", "b", "c")

	-- note that the binary strings won't print ok before the fix
	pprint(t)
	
	--print("URL", "socket", t["url"].socket, "path", t["url"].path, "fragment", t["url"].fragment)

	if not sys.save(path, t) then
		print("Failed to write data to " .. path)
		assert(false)
	end
	print("Wrote save file to " .. path)
end


local function load(path)
	local t = sys.load(path)
	if not next(t) then
		print("Failed to read data from " .. path)
		assert(false)
	end

	print("Loaded save data from " .. path)
	pprint(t)

	print("Testing save data from table version " .. TABLE_VERSION_CURRENT)

	if TABLE_VERSION_CURRENT == 1 then
		assert( t["binary"] == "payload\1" )

	elseif TABLE_VERSION_CURRENT == 2 then
		assert( t["binary\0string"] == "payload\1\0\2\3" )
	end

	assert( t["vector3"] == vmath.vector3(1,2,3) )
	assert( t["vector4"] == vmath.vector4(4, 5, 6, 7) )
	assert( t["quat"] == vmath.quat(1,2,3,4) )
	assert( t["number"] == 0.5 )
	assert( t["hash"] == hash("hashed value") )
	assert( t["url"] == msg.url("a", "b", "c") )

end

if TEST_WRITE ~= nil then
	save( TEST_PATH )
else
	load( TEST_PATH )
end