M = {}

local function do_direct_calculation (a_property)
   if a_property == nil then
      print("a_property is nil, resetting")
      a_property = "DEFAULT"
   end
   
   return "hello from lua! " .. a_property
end

local function do_indirect_calculation (direct_calculation)
   return direct_calculation
end

M.type_name = "LuaTestNode"
M.properties = {a_property={}}
M.outputs = {direct_calculation={fun=do_direct_calculation, args={a_property={}}},
	     indirect_calculation={fun=do_indirect_calculation, args={direct_calculation={}}}}


return M
