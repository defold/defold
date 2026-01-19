-- Copyright 2020-2026 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
--
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
--
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.


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

	if TABLE_VERSION_CURRENT > 2 then
		-- test negative numeric keys
		t[-123] = "minus_onetwothree"
		t[123] 	= "plus_oneonetwothree"
	end

	-- note that the binary strings won't print ok before the fix
	pprint(t)

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

	if TABLE_VERSION_CURRENT > 2 then
		assert( t[-123] == "minus_onetwothree" )
		assert( t[123] == "plus_onetwothree" )
	end

end

if TEST_WRITE ~= nil then
	save( TEST_PATH )
else
	load( TEST_PATH )
end
