--
-- EDN encoding for lua
--
-- see https://github.com/edn-format/edn
--

local M = {}

local encode_value
local encode_key

local escape_char_map = {
  ["\\" ] = "\\\\",
  ["\"" ] = "\\\"",
  ["\n" ] = "\\n",
  ["\r" ] = "\\r",
  ["\t" ] = "\\t",
}

local function raw_tostring(val)
  local old_mt = debug.getmetatable(val)
  debug.setmetatable(val, nil)
  local raw_str = tostring(val)
  debug.setmetatable(val, old_mt)
  return raw_str
end

local function has_custom_tostring(val)
  local mt = debug.getmetatable(val)
  return mt ~= nil and mt.__tostring ~= nil
end

local function get_prefixed_addr(prefix, raw_string)
  local b, e = string.find(raw_string, prefix)
  if e ~= nil then
    return string.sub(raw_string, e)
  else
    return ""
  end
end

local function str(val)
  -- Can't call tostring on NodeProxy unless called from owning Gui script. See:
  -- https://jira.int.midasplayer.com/browse/DEF-532
  if getmetatable(val) == NodeProxy then
    local addr = get_prefixed_addr("userdata: ", raw_tostring(val))
    local success, res_or_error = pcall(function () return tostring(val) end)
    if success then
      return res_or_error .. " " .. addr
    else
      return "<foreign scene node> " .. addr
    end
  elseif type(val) == "table" and has_custom_tostring(val) then
    return tostring(val) .. " " .. get_prefixed_addr("table: ", raw_tostring(val))
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

local function encode_table_value(val, stack)
  local res = {}
  stack = stack or {}

  -- Circular reference?
  if stack[val] then
    return "#lua/ref{:tostring \"" .. str(val) .. "\"}"
  end

  stack[val] = true

  for k, v in pairs(val) do
    table.insert(res, encode_key(k, stack) .. " " .. encode_value(v, stack))
  end

  stack[val] = nil

  return "#lua/table{:tostring \"" .. str(val) .. "\", :data {" .. table.concat(res, ",") .. "}}"
end

local function encode_table_ref(val, stack)
  return "#lua/tableref{:tostring \"" .. str(val) .. "\"}"
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

local value_encoder_map = {
  ["nil"     ] = encode_nil,
  ["boolean" ] = str,
  ["number"  ] = encode_number,
  ["string"  ] = encode_string,
  ["table"   ] = encode_table_value,
  ["userdata"] = make_tagged_string_encoder("#lua/userdata"),
  ["function"] = make_tagged_string_encoder("#lua/function"),
  ["thread"  ] = make_tagged_string_encoder("#lua/thread"),
}

encode_value = function(val, stack)
  local t = type(val)
  local f = value_encoder_map[t]
  if f then
    return f(val, stack)
  end
  error("unexpected type '" .. t .. "'")
end

encode_key = function(val, stack)
  local t = type(val)
  if t == "table" then
    return encode_table_ref(val, stack)
  else
    return encode_value(val, stack)
  end
end

function M.encode(val)
  local err_trace
  local success, error_or_result = xpcall(function () return encode_value(val) end, function (err) err_trace = debug.traceback(); return err end)
  if success then
    return error_or_result
  else
    print("Debugger: " .. error_or_result)
    if err_trace ~= nil then
      print(err_trace)
    end
    return encode_nil()
  end
end

return M
