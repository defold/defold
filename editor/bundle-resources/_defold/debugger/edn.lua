--
-- EDN encoding for lua
--
-- see https://github.com/edn-format/edn
--

local M = {}

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

local function get_prefixed_addr(prefix, val)
  local raw_string = raw_tostring(val)
  local b, e = string.find(raw_string, prefix)
  if e ~= nil then
    return string.sub(raw_string, e + 1)
  else
    return ""
  end
end

local function str(val)
  if getmetatable(val) == NodeProxy then
    return tostring(val) .. get_prefixed_addr("userdata: ", val)
  elseif type(val) == "table" and has_custom_tostring(val) then
    return tostring(val) .. " " .. get_prefixed_addr("table: ", val)
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
  return '"' .. val:gsub('[%z\1-\31\\"]', escape_char) .. '"'
end

local function encode_as_tagged_string(tag, val)
  return tag .. encode_string(str(val))
end

local function make_tagged_string_encoder(tag)
  return function(val)
    return encode_as_tagged_string(tag, val)
  end
end

local function get_table_ref(t)
  return get_prefixed_addr("table: ", t)
end

local function make_ref_encoder(get_ref)
  return function(val)
    return encode_as_tagged_string("#lua/ref", get_ref(val))
  end
end

local type_to_primitive_encoder = {
  ["nil"] = encode_nil,
  ["boolean"] = tostring,
  ["number"] = encode_number,
  ["string"] = encode_string,
  ["table"] = make_ref_encoder(get_table_ref),
  ["userdata"] = make_tagged_string_encoder("#lua/userdata"),
  ["function"] = make_tagged_string_encoder("#lua/function"),
  ["thread"] = make_tagged_string_encoder("#lua/thread"),
}

local function fail_on_unexpected_type(x)
  error("unexpected type '" .. type(x) .. "'")
end

local function encode_as_primitive(val)
  local encoder = type_to_primitive_encoder[type(val)] or fail_on_unexpected_type
  return encoder(val)
end

local function collect_refs(val, refs)
  local t = type(val)
  if t == "table" then
    if not refs[val] then
      refs[val] = val
      for k, v in pairs(val) do
        collect_refs(k, refs)
        collect_refs(v, refs)
      end
    end
  end
  return refs
end

local function encode_table(val)
  local encoded_kvs = {}
  for k, v in pairs(val) do
    table.insert(
      encoded_kvs,
      encode_as_primitive(k) .. " " .. encode_as_primitive(v))
  end
  return "#lua/table{:content [" .. table.concat(encoded_kvs, " ") .. "] :string " .. encode_string(str(val)) .. "}"
end

local function encode_refs(refs)
  local encoded_refs = {}
  for k, v in pairs(refs) do
    table.insert(
      encoded_refs,
      encode_as_primitive(k) .. " " .. encode_table(v))
  end
  return "{" .. table.concat(encoded_refs, " ") .. "}"
end

local function encode_structure(val)
  local refs = {}
  local encoded_refs = encode_refs(collect_refs(val, refs))
  return "#lua/structure{:value " .. encode_as_primitive(val) .. " :refs " .. encoded_refs .. "}"
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
