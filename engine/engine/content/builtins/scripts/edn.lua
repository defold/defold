-- Copyright 2020-2024 The Defold Foundation
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
  local result = '"'..val:gsub('[%z\1-\31\\"]', escape_char)..'"'
  strings_cache[val] = result
  strings_cache_count = strings_cache_count + 1
  return result
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
  local escaped_val = tostring(val):gsub('[%z\1-\31\\"]', escape_char)
  return string.format('#lua/userdata"%s %p"', escaped_val, val)
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
  [GuiScriptInstance] = true,
  [RenderScriptInstance] = true,
}

local metatable_to_primitive_encoder = {
  [GOScriptInstance] = encode_script,
  [GuiScriptInstance] = encode_script,
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

local function encode_edn_table(ref, ref_table)
  local encoded_kvs = {}
  for k, v in pairs(ref_table) do
    encoded_kvs[#encoded_kvs + 1] = encode_as_primitive(k)
    encoded_kvs[#encoded_kvs + 1] = encode_as_primitive(v)
  end
  local encoded_str = encode_string(str(ref, type(ref), debug.getmetatable(ref)))
  return "#lua/table{:content [" .. table.concat(encoded_kvs, " ") .. "] :string " .. encoded_str .. "}"
end

local function encode_refs(ref_to_table)
  local encoded_refs = {}
  for ref, ref_table in pairs(ref_to_table) do
    encoded_refs[#encoded_refs + 1] = encode_as_primitive(ref)
    encoded_refs[#encoded_refs + 1] = encode_edn_table(ref, ref_table)
  end
  return "{" .. table.concat(encoded_refs, " ") .. "}"
end

local function collect_refs(depth, val, internal_graph, edge_refs_set, registry)
  local val_type = type(val)
  local ref_table
  if val_type == "table" then
    ref_table = val
  elseif val_type == "userdata" then
    local mt = debug.getmetatable(val)
    if mt and has_data_in_registry[mt.__metatable] then
      ref_table = registry[mt.__get_instance_data_table_ref(val)]
    end
  end
  if ref_table then
    if depth > 0 then
      if not internal_graph[val] then
        internal_graph[val] = ref_table
        local next_depth = depth - 1
        for k, v in pairs(ref_table) do
          collect_refs(next_depth, k, internal_graph, edge_refs_set, registry)
          collect_refs(next_depth, v, internal_graph, edge_refs_set, registry)
        end
      end
    else -- depth 0, add to edges
      edge_refs_set[val] = true
    end
  end
end

---Build a references graph for a value up to a certain depth for serialization
---@param val any value to analyze
---@param params {maxlevel: integer?} params table, where maxlevel is max depth level of serialization
---@return table<any, table> internal_graph mapping from found references to a table of its contents
---@return table<string, any> edge_refs mapping from hexademical pointer strings to references that were found but will not be serialized
function M.analyze(val, params)
  local depth = params.maxlevel or 16
  local internal_graph = {}
  local edge_refs_set = {}
  collect_refs(depth, val, internal_graph, edge_refs_set, debug.getregistry())
  local edge_refs = {}
  for edge_ref, _ in pairs(edge_refs_set) do
    edge_refs[string.format("%p", edge_ref)] = edge_ref
  end
  return internal_graph, edge_refs
end

---Serialize a val to EDN, along with its internal graph that can be obtained with analyze
---@param val any
---@param internal_graph table<any, table> reference graph obtained from analyze
---@return string edn serialized object graph
function M.serialize(val, internal_graph)
  local result = "#lua/structure{:value " .. encode_as_primitive(val) .. " :refs " .. encode_refs(internal_graph) .. "}"
  if strings_cache_count > 5000000 then
    strings_cache_count = 0
    strings_cache = {}
  end
  return result
end

---Build a val reference graph up to a certain depth and serialize it to EDN
---@param val any value to serialize
---@param params {maxlevel: integer?} params table, where maxlevel is max depth level of serialization
---@return string edn serialized object graph
function M.encode(val, params)
  local err_trace
  local ok, result = xpcall(
    function()
      local graph = M.analyze(val, params)
      return M.serialize(val, graph)
    end,
    function(err)
      err_trace = debug.traceback()
      return err
    end)
  if ok then
    return result
  else
    print("Debugger: " .. result)
    if err_trace then
      print(err_trace)
    end
    return encode_nil()
  end
end

return M
