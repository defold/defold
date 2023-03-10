-- Copyright 2020-2023 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

local function str(val, val_type, metatable)
  local success, error_or_result = pcall(function()
    if metatable then
      if val_type == "table" and metatable.__tostring then
        return string.format("%s %p", val, val)
      end
    end
    return tostring(val)
  end)
  if success then
    return error_or_result
  else
    print("Debugger caught error in __tostring: " .. error_or_result)
    local err_trace = debug.traceback();
    if err_trace ~= nil then
      print(err_trace)
    end
    return "???"
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

local strings_cache = {}
local strings_cache_count = 0
local function encode_string(val, val_type)
  if strings_cache[val] then
    return strings_cache[val]
  end
  local str = '"'..val:gsub('[%z\1-\31\\"]', escape_char)..'"'
  strings_cache[val] = str
  strings_cache_count = strings_cache_count + 1
  return str
end

local function encode_table(val, val_type)
  return string.format('#lua/ref"%p"', val)
end

local function encode_userdata(val, val_type, metatable)
  return '#lua/userdata"'..str(val, val_type, metatable)..'"'
end

local function encode_function(val, val_type, metatable)
  return '#lua/function"'..tostring(val)..'"'
end

local function encode_thread(val, val_type, metatable)
  return '#lua/thread"'..tostring(val)..'"'
end

local function encode_script(val, val_type)
  return '#lua/ref"' .. tostring(val) .. '"'
end

local function encode_node(val, val_type)
  return string.format('#lua/userdata"%s %p"', tostring(val), val)
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

local has_data_in_registry = {
  [GOScriptInstance] = true,
  -- [GuiScriptInstance] = true, (it doesn't work for *.gui_script)
  [RenderScriptInstance] = true,
}

local metatable_to_primitive_encoder = {
  [GOScriptInstance] = encode_script,
  -- [GuiScriptInstance] = encode_script, (it doesn't work for *.gui_script)
  [RenderScriptInstance] = encode_script,
  [NodeProxy] = encode_node
}

local function fail_on_unexpected_type(x, val_type)
  error("unexpected type '" .. val_type .. "'")
end

local function encode_as_primitive(val)
  local val_type = type(val)
  local mt = debug.getmetatable(val)
  local encoder = (mt and metatable_to_primitive_encoder[mt.__metatable]) or type_to_primitive_encoder[val_type] or fail_on_unexpected_type
  return encoder(val, val_type, mt)
end

local function encode_edn_table(val, key)
  local encoded_kvs = {}
  for k, v in pairs(val) do
    encoded_kvs[#encoded_kvs + 1] = encode_as_primitive(k)
    encoded_kvs[#encoded_kvs + 1] = encode_as_primitive(v)
  end
  local encoded_str = encode_string(str(key, type(key), debug.getmetatable(key)))
  return "#lua/table{:content [" .. table.concat(encoded_kvs, " ") .. "] :string " .. encoded_str .. "}"
end

local function encode_refs(refs, keys)
  local encoded_refs = {}
  local val
  for i = 1, #refs do
    val = refs[i]
    encoded_refs[#encoded_refs + 1] = encode_as_primitive(val)
    encoded_refs[#encoded_refs + 1] = encode_edn_table(keys[val], val)
  end
  return "{" .. table.concat(encoded_refs, " ") .. "}"
end

local registry
local function find_variables_in_registry(val)
  for i = 1, #registry do
    if registry[i] == val then
      return registry[i+1]
    end
  end
end

local function collect_refs(depth, val, refs, keys)
  local val_type = type(val)
  if val_type == "table" then
    if depth < 100 and not keys[val] then
      keys[val] = val
      refs[#refs + 1] = val
      local next_depth = depth + 1
      for k, v in pairs(val) do
        collect_refs(next_depth, k, refs, keys)
        collect_refs(next_depth, v, refs, keys)
      end
    end
  elseif val_type == "userdata" then
    local mt = debug.getmetatable(val)
    if mt and has_data_in_registry[mt.__metatable] then
      local script_val = find_variables_in_registry(val)
      if script_val then
        if depth < 100 and not keys[val] then
          keys[val] = script_val
          refs[#refs + 1] = val
          local next_depth = depth + 1
          for k, v in pairs(script_val) do
            collect_refs(next_depth, k, refs, keys)
            collect_refs(next_depth, v, refs, keys)
          end
        end
      end
    end
  end
end

local function encode_structure(val)
  local keys = {}
  local refs = {}
  collect_refs(0, val, refs, keys)
  local encoded_refs = encode_refs(refs, keys)
  local str = "#lua/structure{:value " .. encode_as_primitive(val) .. " :refs " .. encoded_refs .. "}"
  if strings_cache_count > 5000000 then
    strings_cache_count = 0
    strings_cache = {}
  end
  return str
end

function M.encode(val)
  registry = debug.getregistry()
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
