--
-- EDN encoding for lua
--
-- see https://github.com/edn-format/edn
--

local M = {}

local encode

local escape_char_map = {
  ["\\" ] = "\\\\",
  ["\"" ] = "\\\"",
  ["\n" ] = "\\n",
  ["\r" ] = "\\r",
  ["\t" ] = "\\t",
}

local function str(val)
  -- Can't call tostring on NodeProxy unless called from owning Gui script. See:
  -- https://jira.int.midasplayer.com/browse/DEF-532
  if getmetatable(val) == NodeProxy then
    return "GuiNode"
  else
    return tostring(val)
  end
end

local function escape_char(c)
  return escape_char_map[c] or string.format("\\u%04x", c:byte())
end

local function encode_nil(val)
  return "nil"
end 

local function encode_number(val)
  -- Check for NaN, -inf and inf
  if val ~= val then
    return "#lua/number :nan"
  elseif val <= -math.huge then
    return "#lua/number :-inf"
  elseif val >= math.huge then
    return "#lua/number :+inf"
  else
    return string.format("#lua/number %.14g", val)
  end
end

local function encode_string(val)
  return '"' .. val:gsub('[%z\1-\31\\"]', escape_char) .. '"'
end

local function encode_table(val, stack)
  local res = {}
  stack = stack or {}

  -- Circular reference?
  if stack[val] then
    return "#lua/ref{:tostring \"" .. str(val) .. "\"}"
  end

  stack[val] = true

  for k, v in pairs(val) do
      table.insert(res, encode(k, stack) .. " " .. encode(v, stack))
  end

  stack[val] = nil
  
  return "#lua/table{:tostring \"" .. str(val) .. "\", :data {" .. table.concat(res, ",") .. "}}"
end

local function make_tagged_string_encoder(tag)
    return function(val)
        return tag .. encode_string(str(val))
    end
end

local function encode_userdata(val)
    return "#lua/userdata" .. encode_string(str(val))
end

local function encode_function(val)
  return "#lua/function" .. encode_string(str(val))
end

local type_func_map = {
  ["nil"     ] = encode_nil,
  ["boolean" ] = str,
  ["number"  ] = encode_number,
  ["string"  ] = encode_string,
  ["table"   ] = encode_table,
  ["userdata"] = make_tagged_string_encoder("#lua/userdata"),
  ["function"] = make_tagged_string_encoder("#lua/function"),
  ["thread"  ] = make_tagged_string_encoder("#lua/thread"),
}

encode = function(val, stack)
  local t = type(val)
  local f = type_func_map[t]
  if f then
    return f(val, stack)
  end
  error("unexpected type '" .. t .. "'")
end

function M.encode(val)
  return encode(val)
end

return M
