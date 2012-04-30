--go.property("should_be_removed", 0)

  --go.property("should_be_removed", 0)

--[[
go.property("should_be_removed", 0)
--]]

-- The two following "hyphen-lines" should be two separate single-line comments
---[[
go.property("prop1", 123.456)
--]]
go.property("prop2", go.url ())
go.property('prop3', go.url( ))

go.property("three_args", 1, 2)
go.property("unknown_type", "hello")

'go.property("no_prop", 123)'
"go.property('no_prop', 123)"

function update(self) 

end
