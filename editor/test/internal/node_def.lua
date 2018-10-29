M = {}

local function do_direct_calculation (a_property)
   return "hello from lua!"
end

local function do_indirect_calculation (direct_calculation)
   return direct_calculation
end

M.type_name = "LuaTestNode"
M.properties = {cake_property="hey"}
M.outputs = {direct_calculation={fun=do_direct_calculation,
				 args={direct_calculation=1}},
	     indirect_calculation={fun=do_indirect_calculation,
				   args={direct_calculation=1}}}
return M
