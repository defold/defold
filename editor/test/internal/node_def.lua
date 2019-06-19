M = {}

local function do_direct_calculation (a_property)
   return "property value in output function: " .. a_property
end

local function do_indirect_calculation (direct_calculation)
   return "value from other output function: " .. direct_calculation
end

local function nop ()
end

M.type_name = "LuaTestNode"

M.properties = {a_property={}}

M.outputs = {direct_calculation={fun=do_direct_calculation,
				 args={a_property=0}},
	     indirect_calculation={fun=do_indirect_calculation,
				   args={direct_calculation=0}},
	     hello={fun=function () return "cake" end, args={}}

	    }

M.inputs = {some_input={},
	    some_array_input={array={}}}

return M
