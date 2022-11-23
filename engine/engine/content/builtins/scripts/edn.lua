--
-- EDN encoding for lua
--
-- see https://github.com/edn-format/edn
--

local M = {}

local tostring = tostring
local string = string
local debug = debug
local type = type
local table = table

local escape_char_map = {
  ["\\" ] = "\\\\",
  ["\"" ] = "\\\"",
  ["\n" ] = "\\n",
  ["\r" ] = "\\r",
  ["\t" ] = "\\t",
}

local function str(val)
  local mt = debug.getmetatable(val)
  if mt then 
    if mt.__metatable == NodeProxy then
      return string.format("%s%p", val, val)
    elseif type(val) == "table" and mt.__tostring then
      return string.format("%s %p", val, val)
    end
    return tostring(val)
  else
    return tostring(val)
  end
end

local function escape_char(c)
  return escape_char_map[c] or string.format("\\u%04x", c:byte())
end

local function encode_nil()
  return "nil"
end

local function encode_number(val)
  if val ~= val then
    return "##NaN"
  elseif val <= -math.huge then
    return "##-Inf"
  elseif val >= math.huge then
    return "##Inf"
  else
    return string.format("%.14g", val)
  end
end

local function encode_string(val)
  return '"'..val:gsub('[%z\1-\31\\"]', escape_char)..'"'
end

local function encode_table(val)
  return string.format('#lua/ref"%p"', val)
end

function encode_userdata(val)
  return '#lua/userdata"'..str(val)..'"'
end

function encode_function(val)
  return '#lua/function"'..str(val)..'"'
end

function encode_thread(val)
  return '#lua/thread"'..str(val)..'"'
end

local type_to_primitive_encoder = {
  ["nil"] = encode_nil,
  ["boolean"] = tostring,
  ["number"] = encode_number,
  ["string"] = encode_string,
  ["table"] = encode_table,
  ["userdata"] = encode_userdata,
  ["function"] = encode_function,
  ["thread"] = encode_thread
}

local function fail_on_unexpected_type(x)
  error("unexpected type '" .. type(x) .. "'")
end

local function encode_as_primitive(val)
  local encoder = type_to_primitive_encoder[type(val)] or fail_on_unexpected_type
  return encoder(val)
end

local function encode_table(val)
  local encoded_kvs = {}
  for k, v in pairs(val) do
    encoded_kvs[#encoded_kvs + 1] = encode_as_primitive(k) .. " " .. encode_as_primitive(v)
  end
  return "#lua/table{:content [" .. table.concat(encoded_kvs, " ") .. "] :string " .. encode_string(str(val)) .. "}"
end

local function encode_refs(refs)
  local encoded_refs = {}
  local val
  for i = 1, #refs do
    val = refs[i]
    encoded_refs[i] = encode_as_primitive(val).." "..encode_table(val)
  end
  return "{" .. table.concat(encoded_refs, " ") .. "}"
end

local function collect_refs(depth, val, refs, dups)
  if type(val) == "table" then
    if depth < 100 and not dups[val] then
      dups[val] = true
      refs[#refs + 1] = val
      local next_depth = depth + 1
      for k, v in pairs(val) do
        collect_refs(next_depth, k, refs, dups)
        collect_refs(next_depth, v, refs, dups)
      end
    end
  end
end

local function encode_structure(val)
  local dups = {}
  local refs = {}
  collect_refs(0, val, refs, dups)
  local encoded_refs = encode_refs(refs)
  local str = "#lua/structure{:value " .. encode_as_primitive(val) .. " :refs " .. encoded_refs .. "}"
  return str
end

function M.encode(val)
  local err_trace
  local success, error_or_result = xpcall(
  function ()
    return encode_structure(val)
  end,
  function (err)
    err_trace = debug.traceback();
    return err
  end)
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
