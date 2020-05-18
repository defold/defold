-- Copyright 2020 The Defold Foundation
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

go.property(invalid_string, 2)
go.property("three_args", 1, 2)
go.property("unknown_type", "hello")

'go.property("no_prop", 123)'
"go.property('no_prop', 123)"

function update(self)

end
