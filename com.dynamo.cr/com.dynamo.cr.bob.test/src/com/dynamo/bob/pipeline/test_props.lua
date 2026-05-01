--go.property("should_be_removed1", 0)

  --go.property("should_be_removed2", 0)

--[[
go.property("should_be_removed3", 0)
--]]

-- The two following "hyphen-lines" should be two separate single-line comments
---[[
go.property("prop1", 0)
--]]

  go.property( "prop2" ,  0 )  
go.property('prop3',  0 )  
go.property('prop4', 0) -- trailing comment
go.property  ('prop5', 0)

go.property(invalid_string, 2)
go.property("three_args", 1, 2)
go.property("unknown_type", "hello")

local s1 = 'go.property("no_prop", 123)'
local s2 = "go.property('no_prop', 123)"

function update(self)

end
