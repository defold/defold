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
go.property("text_type", "hello")
go.property("text_utf8", "Spelare åäö")
go.property("text_lua", "function init(self)\n\tprint('hello')\nend")
go.property("text_xml", '<root attr="value">å</root>')
go.property("text_json", '{"key":"value","enabled":true}')

local s1 = 'go.property("no_prop", 123)'
local s2 = "go.property('no_prop', 123)"

function update(self)

end
