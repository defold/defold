--
-- MobDebug -- Lua remote debugger
-- Copyright 2011-15 Paul Kulchenko
-- Based on RemDebug 1.0 Copyright Kepler Project 2005
--

-- use loaded modules or load explicitly on those systems that require that
local require = require
--local io = io or require "io"
--local table = table or require "table"
--local string = string or require "string"
--local coroutine = coroutine or require "coroutine"
--local debug = require "debug"
-- protect require "os" as it may fail on embedded systems without os module
local os = os or (function(module)
  local ok, res = pcall(require, module)
  return ok and res or nil
end)("os")

local mobdebug = {
  _NAME = "mobdebug",
  _VERSION = "0.70",
  _COPYRIGHT = "Paul Kulchenko",
  _DESCRIPTION = "Mobile Remote Debugger for the Lua programming language",
  port = os and os.getenv and tonumber((os.getenv("MOBDEBUG_PORT"))) or 8172,
  checkcount = 200,
  yieldtimeout = 0.02, -- yield timeout (s)
  connecttimeout = 2, -- connect timeout (s)
}

local HOOKMASK = "lcr"
local error = error
local getfenv = getfenv
local setfenv = setfenv
local loadstring = loadstring or load -- "load" replaced "loadstring" in Lua 5.2
local pairs = pairs
local setmetatable = setmetatable
local tonumber = tonumber
local unpack = table.unpack or unpack
local rawget = rawget
local gsub, sub, find = string.gsub, string.sub, string.find

-- if strict.lua is used, then need to avoid referencing some global
-- variables, as they can be undefined;
-- use rawget to avoid complaints from strict.lua at run-time.
-- it's safe to do the initialization here as all these variables
-- should get defined values (if any) before the debugging starts.
-- there is also global 'wx' variable, which is checked as part of
-- the debug loop as 'wx' can be loaded at any time during debugging.
local genv = _G or _ENV
local jit = rawget(genv, "jit")
local MOAICoroutine = rawget(genv, "MOAICoroutine")

-- ngx_lua debugging requires a special handling as its coroutine.*
-- methods use a different mechanism that doesn't allow resume calls
-- from debug hook handlers.
-- Instead, the "original" coroutine.* methods are used.
-- `rawget` needs to be used to protect against `strict` checks, but
-- ngx_lua hides those in a metatable, so need to use that.
local metagindex = getmetatable(genv) and getmetatable(genv).__index
local ngx = type(metagindex) == "table" and metagindex.rawget and metagindex:rawget("ngx") or nil
local corocreate = ngx and coroutine._create or coroutine.create
local cororesume = ngx and coroutine._resume or coroutine.resume
local coroyield = ngx and coroutine._yield or coroutine.yield
local corostatus = ngx and coroutine._status or coroutine.status
local corowrap = coroutine.wrap

if not setfenv then -- Lua 5.2+
  -- based on http://lua-users.org/lists/lua-l/2010-06/msg00314.html
  -- this assumes f is a function
  local function findenv(f)
    local level = 1
    repeat
      local name, value = debug.getupvalue(f, level)
      if name == '_ENV' then return level, value end
      level = level + 1
    until name == nil
    return nil end
  getfenv = function (f) return(select(2, findenv(f)) or _G) end
  setfenv = function (f, t)
    local level = findenv(f)
    if level then debug.setupvalue(f, level, t) end
    return f end
end

-- check for OS and convert file names to lower case on windows
-- (its file system is case insensitive, but case preserving), as setting a
-- breakpoint on x:\Foo.lua will not work if the file was loaded as X:\foo.lua.
-- OSX and Windows behave the same way (case insensitive, but case preserving).
-- OSX can be configured to be case-sensitive, so check for that. This doesn't
-- handle the case of different partitions having different case-sensitivity.
local win = os and os.getenv and (os.getenv('WINDIR') or (os.getenv('OS') or ''):match('[Ww]indows')) and true or false
local mac = not win and (os and os.getenv and os.getenv('DYLD_LIBRARY_PATH') or not io.open("/proc")) and true or false
local iscasepreserving = win or (mac and io.open('/library') ~= nil)

-- turn jit off based on Mike Pall's comment in this discussion:
-- http://www.freelists.org/post/luajit/Debug-hooks-and-JIT,2
-- "You need to turn it off at the start if you plan to receive
-- reliable hook calls at any later point in time."
if jit and jit.off then jit.off() end

local socket = require "builtins.scripts.socket"
local edn = require "builtins.scripts.edn"
local edn_edge_refs_cache = {}
local coro_debugger
local coro_debugee
local coroutines = {}; setmetatable(coroutines, {__mode = "k"}) -- "weak" keys
local events = { BREAK = 1, WATCH = 2, RESTART = 3, STACK = 4 }
local breakpoints = {}
local watches = {}
local lastsource
local lastfile
local watchescnt = 0
local abort -- default value is nil; this is used in start/loop distinction
local seen_hook = false
local checkcount = 0
local step_into = false
local step_over = false
local step_level = 0
local stack_level = 0
local server
local buf
local outputs = {}
local iobase = {print = print}
local basedir = ""
local deferror = "execution aborted at default debugee"
local debugee = function ()
  local a = 1
  for _ = 1, 10 do a = a + 1 end
  error(deferror)
end
local function q(s) return string.gsub(s, '([%(%)%.%%%+%-%*%?%[%^%$%]])','%%%1') end

local serpent = (function() ---- include Serpent module for serialization
local n, v = "serpent", "0.30" -- (C) 2012-17 Paul Kulchenko; MIT License
local c, d = "Paul Kulchenko", "Lua serializer and pretty printer"
local snum = {[tostring(1/0)]='1/0 --[[math.huge]]',[tostring(-1/0)]='-1/0 --[[-math.huge]]',[tostring(0/0)]='0/0'}
local badtype = {thread = true, userdata = true, cdata = true}
local getmetatable = debug and debug.getmetatable or getmetatable
local pairs = function(t) return next, t end -- avoid using __pairs in Lua 5.2+
local keyword, globals, G = {}, {}, (_G or _ENV)
for _,k in ipairs({'and', 'break', 'do', 'else', 'elseif', 'end', 'false',
  'for', 'function', 'goto', 'if', 'in', 'local', 'nil', 'not', 'or', 'repeat',
  'return', 'then', 'true', 'until', 'while'}) do keyword[k] = true end
for k,v in pairs(G) do globals[v] = k end -- build func to name mapping
for _,g in ipairs({'coroutine', 'debug', 'io', 'math', 'string', 'table', 'os'}) do
  for k,v in pairs(type(G[g]) == 'table' and G[g] or {}) do globals[v] = g..'.'..k end end

local function s(t, opts)
  local name, indent, fatal, maxnum = opts.name, opts.indent, opts.fatal, opts.maxnum
  local sparse, custom, huge = opts.sparse, opts.custom, not opts.nohuge
  local space, maxl = (opts.compact and '' or ' '), (opts.maxlevel or math.huge)
  local maxlen, metatostring = tonumber(opts.maxlength), opts.metatostring
  local iname, comm = '_'..(name or ''), opts.comment and (tonumber(opts.comment) or math.huge)
  local numformat = opts.numformat or "%.17g"
  local seen, sref, syms, symn = {}, {'local '..iname..'={}'}, {}, 0
  local function gensym(val) return '_'..(tostring(tostring(val)):gsub("[^%w]",""):gsub("(%d%w+)",
    -- tostring(val) is needed because __tostring may return a non-string value
    function(s) if not syms[s] then symn = symn+1; syms[s] = symn end return tostring(syms[s]) end)) end
  local function safestr(s) return type(s) == "number" and tostring(huge and snum[tostring(s)] or numformat:format(s))
    or type(s) ~= "string" and tostring(s) -- escape NEWLINE/010 and EOF/026
    or ("%q"):format(s):gsub("\010","n"):gsub("\026","\\026") end
  local function comment(s,l) return comm and (l or 0) < comm and ' --[['..select(2, pcall(tostring, s))..']]' or '' end
  local function globerr(s,l) return globals[s] and globals[s]..comment(s,l) or not fatal
    and safestr(select(2, pcall(tostring, s))) or error("Can't serialize "..tostring(s)) end
  local function safename(path, name) -- generates foo.bar, foo[3], or foo['b a r']
    local n = name == nil and '' or name
    local plain = type(n) == "string" and n:match("^[%l%u_][%w_]*$") and not keyword[n]
    local safe = plain and n or '['..safestr(n)..']'
    return (path or '')..(plain and path and '.' or '')..safe, safe end
  local alphanumsort = type(opts.sortkeys) == 'function' and opts.sortkeys or function(k, o, n) -- k=keys, o=originaltable, n=padding
    local maxn, to = tonumber(n) or 12, {number = 'a', string = 'b'}
    local function padnum(d) return ("%0"..tostring(maxn).."d"):format(tonumber(d)) end
    table.sort(k, function(a,b)
      -- sort numeric keys first: k[key] is not nil for numerical keys
      return (k[a] ~= nil and 0 or to[type(a)] or 'z')..(tostring(a):gsub("%d+",padnum))
           < (k[b] ~= nil and 0 or to[type(b)] or 'z')..(tostring(b):gsub("%d+",padnum)) end) end
  local function val2str(t, name, indent, insref, path, plainindex, level)
    local ttype, level, mt = type(t), (level or 0), getmetatable(t)
    local spath, sname = safename(path, name)
    local tag = plainindex and
      ((type(name) == "number") and '' or name..space..'='..space) or
      (name ~= nil and sname..space..'='..space or '')
    if seen[t] then -- already seen this element
      sref[#sref+1] = spath..space..'='..space..seen[t]
      return tag..'nil'..comment('ref', level) end
    -- protect from those cases where __tostring may fail
    if type(mt) == 'table' then
      local to, tr = pcall(function() return mt.__tostring(t) end)
      local so, sr = pcall(function() return mt.__serialize(t) end)
      if (opts.metatostring ~= false and to or so) then -- knows how to serialize itself
        seen[t] = insref or spath
        t = so and sr or tr
        ttype = type(t)
      end -- new value falls through to be serialized
    end
    if ttype == "table" then
      if level >= maxl then return tag..'{}'..comment('maxlvl', level) end
      seen[t] = insref or spath
      if next(t) == nil then return tag..'{}'..comment(t, level) end -- table empty
      if maxlen and maxlen < 0 then return tag..'{}'..comment('maxlen', level) end
      local maxn, o, out = math.min(#t, maxnum or #t), {}, {}
      for key = 1, maxn do o[key] = key end
      if not maxnum or #o < maxnum then
        local n = #o -- n = n + 1; o[n] is much faster than o[#o+1] on large tables
        for key in pairs(t) do if o[key] ~= key then n = n + 1; o[n] = key end end end
      if maxnum and #o > maxnum then o[maxnum+1] = nil end
      if opts.sortkeys and #o > maxn then alphanumsort(o, t, opts.sortkeys) end
      local sparse = sparse and #o > maxn -- disable sparsness if only numeric keys (shorter output)
      for n, key in ipairs(o) do
        local value, ktype, plainindex = t[key], type(key), n <= maxn and not sparse
        if opts.valignore and opts.valignore[value] -- skip ignored values; do nothing
        or opts.keyallow and not opts.keyallow[key]
        or opts.keyignore and opts.keyignore[key]
        or opts.valtypeignore and opts.valtypeignore[type(value)] -- skipping ignored value types
        or sparse and value == nil then -- skipping nils; do nothing
        elseif ktype == 'table' or ktype == 'function' or badtype[ktype] then
          if not seen[key] and not globals[key] then
            sref[#sref+1] = 'placeholder'
            local sname = safename(iname, gensym(key)) -- iname is table for local variables
            sref[#sref] = val2str(key,sname,indent,sname,iname,true) end
          sref[#sref+1] = 'placeholder'
          local path = seen[t]..'['..tostring(seen[key] or globals[key] or gensym(key))..']'
          sref[#sref] = path..space..'='..space..tostring(seen[value] or val2str(value,nil,indent,path))
        else
          out[#out+1] = val2str(value,key,indent,insref,seen[t],plainindex,level+1)
          if maxlen then
            maxlen = maxlen - #out[#out]
            if maxlen < 0 then break end
          end
        end
      end
      local prefix = string.rep(indent or '', level)
      local head = indent and '{\n'..prefix..indent or '{'
      local body = table.concat(out, ','..(indent and '\n'..prefix..indent or space))
      local tail = indent and "\n"..prefix..'}' or '}'
      return (custom and custom(tag,head,body,tail,level) or tag..head..body..tail)..comment(t, level)
    elseif badtype[ttype] then
      seen[t] = insref or spath
      return tag..globerr(t, level)
    elseif ttype == 'function' then
      seen[t] = insref or spath
      if opts.nocode then return tag.."function() --[[..skipped..]] end"..comment(t, level) end
      local ok, res = pcall(string.dump, t)
      local func = ok and "((loadstring or load)("..safestr(res)..",'@serialized'))"..comment(t, level)
      return tag..(func or globerr(t, level))
    else return tag..safestr(t) end -- handle all other types
  end
  local sepr = indent and "\n" or ";"..space
  local body = val2str(t, name, indent) -- this call also populates sref
  local tail = #sref>1 and table.concat(sref, sepr)..sepr or ''
  local warn = opts.comment and #sref>1 and space.."--[[incomplete output with shared/self-references skipped]]" or ''
  return not name and body..warn or "do local "..body..sepr..tail.."return "..name..sepr.."end"
end

local function deserialize(data, opts)
  local env = (opts and opts.safe == false) and G
    or setmetatable({}, {
        __index = function(t,k) return t end,
        __call = function(t,...) error("cannot call functions") end
      })
  local f, res = (loadstring or load)('return '..data, nil, nil, env)
  if not f then f, res = (loadstring or load)(data, nil, nil, env) end
  if not f then return f, res end
  if setfenv then setfenv(f, env) end
  return pcall(f)
end

local function merge(a, b) if b then for k,v in pairs(b) do a[k] = v end end; return a; end
return { _NAME = n, _COPYRIGHT = c, _DESCRIPTION = d, _VERSION = v, serialize = s,
  load = deserialize,
  dump = function(a, opts) return s(a, merge({name = '_', compact = true, sparse = true}, opts)) end,
  line = function(a, opts) return s(a, merge({sortkeys = true, comment = true}, opts)) end,
  block = function(a, opts) return s(a, merge({indent = '  ', sortkeys = true, comment = true}, opts)) end }
end)() ---- end of Serpent module

mobdebug.line = serpent.line
mobdebug.dump = serpent.dump
mobdebug.linemap = nil
mobdebug.loadstring = loadstring

local function removebasedir(path, basedir)
  if iscasepreserving then
    -- check if the lowercased path matches the basedir
    -- if so, return substring of the original path (to not lowercase it)
    return path:lower():find('^'..q(basedir:lower()))
      and path:sub(#basedir+1) or path
  else
    return string.gsub(path, '^'..q(basedir), '')
  end
end

local function stack(start)
  local function vars(f)
    local func = debug.getinfo(f, "f").func
    local i = 1
    local locals = {}
    -- get locals
    while true do
      local name, value = debug.getlocal(f, i)
      if not name then break end
      if string.sub(name, 1, 1) ~= '(' then
        locals[name] = {value, select(2,pcall(tostring,value))}
      end
      i = i + 1
    end
    -- get varargs (these use negative indices)
    i = 1
    while true do
      local name, value = debug.getlocal(f, -i)
      -- `not name` should be enough, but LuaJIT 2.0.0 incorrectly reports `(*temporary)` names here
      if not name or name ~= "(*vararg)" then break end
      locals[name:gsub("%)$"," "..i..")")] = {value, select(2,pcall(tostring,value))}
      i = i + 1
    end
    -- get upvalues
    i = 1
    local ups = {}
    while func do -- check for func as it may be nil for tail calls
      local name, value = debug.getupvalue(func, i)
      if not name then break end
      ups[name] = {value, select(2,pcall(tostring,value))}
      i = i + 1
    end
    return locals, ups
  end

  local stack = {}
  local linemap = mobdebug.linemap
  for i = (start or 0), 100 do
    local source = debug.getinfo(i, "Snl")
    if not source then break end

    local src = source.source
    if src:find("[@=]") == 1 then
      src = src:sub(2):gsub("\\", "/")
      if src:find("%./") == 1 then src = src:sub(3) end
    end

    table.insert(stack, { -- remove basedir from source
      {source.name, removebasedir(src, basedir),
       linemap and linemap(source.linedefined, source.source) or source.linedefined,
       linemap and linemap(source.currentline, source.source) or source.currentline,
       source.what, source.namewhat, source.short_src},
      vars(i+1)})
    if source.what == 'main' then break end
  end
  return stack
end

local function set_breakpoint(file, line, chunk)
  if file == '-' and lastfile then file = lastfile
  elseif iscasepreserving then file = string.lower(file) end
  if not breakpoints[line] then breakpoints[line] = {} end
  breakpoints[line][file] = chunk or true
  if mobdebug.setbreakpoint_hook then mobdebug.setbreakpoint_hook(line, file) end
end

local function remove_breakpoint(file, line)
  if file == '-' and lastfile then file = lastfile
  elseif file == '*' and line == 0 then breakpoints = {}
  elseif iscasepreserving then file = string.lower(file) end
  if breakpoints[line] then breakpoints[line][file] = nil end
  if mobdebug.removebreakpoint_hook then mobdebug.removebreakpoint_hook(line, file) end
end

---@return true | function | nil breakpoint
local function get_breakpoint(file, line)
  return breakpoints[line]
     and breakpoints[line][iscasepreserving and string.lower(file) or file]
end

local function restore_vars(vars)
  if type(vars) ~= 'table' then return end

  -- locals need to be processed in the reverse order, starting from
  -- the inner block out, to make sure that the localized variables
  -- are correctly updated with only the closest variable with
  -- the same name being changed
  -- first loop find how many local variables there is, while
  -- the second loop processes them from i to 1
  local i = 1
  while true do
    local name = debug.getlocal(3, i)
    if not name then break end
    i = i + 1
  end
  i = i - 1
  local written_vars = {}
  while i > 0 do
    local name = debug.getlocal(3, i)
    if not written_vars[name] then
      if string.sub(name, 1, 1) ~= '(' then
        debug.setlocal(3, i, rawget(vars, name))
      end
      written_vars[name] = true
    end
    i = i - 1
  end

  i = 1
  local func = debug.getinfo(3, "f").func
  while true do
    local name = debug.getupvalue(func, i)
    if not name then break end
    if not written_vars[name] then
      if string.sub(name, 1, 1) ~= '(' then
        debug.setupvalue(func, i, rawget(vars, name))
      end
      written_vars[name] = true
    end
    i = i + 1
  end
end

local function capture_vars(level, thread)
  level = (level or 0)+2 -- add two levels for this and debug calls
  local func = (thread and debug.getinfo(thread, level, "f") or debug.getinfo(level, "f") or {}).func
  if not func then return {} end

  local vars = {['...'] = {}}
  local i = 1
  while true do
    local name, value = debug.getupvalue(func, i)
    if not name then break end
    if string.sub(name, 1, 1) ~= '(' then vars[name] = value end
    i = i + 1
  end
  i = 1
  while true do
    local name, value
    if thread then
      name, value = debug.getlocal(thread, level, i)
    else
      name, value = debug.getlocal(level, i)
    end
    if not name then break end
    if string.sub(name, 1, 1) ~= '(' then vars[name] = value end
    i = i + 1
  end
  -- get varargs (these use negative indices)
  i = 1
  while true do
    local name, value
    if thread then
      name, value = debug.getlocal(thread, level, -i)
    else
      name, value = debug.getlocal(level, -i)
    end
    -- `not name` should be enough, but LuaJIT 2.0.0 incorrectly reports `(*temporary)` names here
    if not name or name ~= "(*vararg)" then break end
    vars['...'][i] = value
    i = i + 1
  end
  -- returned 'vars' table plays a dual role: (1) it captures local values
  -- and upvalues to be restored later (in case they are modified in "eval"),
  -- and (2) it provides an environment for evaluated chunks.
  -- getfenv(func) is needed to provide proper environment for functions,
  -- including access to globals, but this causes vars[name] to fail in
  -- restore_vars on local variables or upvalues with `nil` values when
  -- 'strict' is in effect. To avoid this `rawget` is used in restore_vars.
  setmetatable(vars, { __index = getfenv(func), __newindex = getfenv(func) })
  return vars
end

local function stack_depth(start_depth)
  for i = start_depth, 0, -1 do
    if debug.getinfo(i, "l") then return i+1 end
  end
  return start_depth
end

local function is_safe(stack_level)
  -- the stack grows up: 0 is getinfo, 1 is is_safe, 2 is debug_hook, 3 is user function
  if stack_level == 3 then return true end
  for i = 3, stack_level do
    -- return if it is not safe to abort
    local info = debug.getinfo(i, "S")
    if not info then return true end
    if info.what == "C" then return false end
  end
  return true
end

local function in_debugger()
  local this = debug.getinfo(1, "S").source
  -- only need to check few frames as mobdebug frames should be close
  for i = 3, 7 do
    local info = debug.getinfo(i, "S")
    if not info then return false end
    if info.source == this then return true end
  end
  return false
end

local function is_pending(peer)
  -- if there is something already in the buffer, skip check
  if not buf and checkcount >= mobdebug.checkcount then
    peer:settimeout(0) -- non-blocking
    buf = peer:receive(1)
    peer:settimeout() -- back to blocking
    checkcount = 0
  end
  return buf
end

local function readnext(peer, num)
  peer:settimeout(0) -- non-blocking
  local res, err, partial = peer:receive(num)
  peer:settimeout() -- back to blocking
  return res or partial or '', err
end

local function handle_breakpoint(peer)
  -- check if the buffer has the beginning of SETB/DELB command;
  -- this is to avoid reading the entire line for commands that
  -- don't need to be handled here.
  if not buf or not (buf:sub(1,1) == 'S' or buf:sub(1,1) == 'D') then return end

  -- check second character to avoid reading STEP or other S* and D* commands
  if #buf == 1 then buf = buf .. readnext(peer, 1) end
  if buf:sub(2,2) ~= 'E' then return end

  -- need to read few more characters
  buf = buf .. readnext(peer, 5-#buf)
  if buf ~= 'SETB ' and buf ~= 'DELB ' then return end

  local res, _, partial = peer:receive() -- get the rest of the line; blocking
  if not res then
    if partial then buf = buf .. partial end
    return
  end

  local _, _, cmd, file, line, condition = (buf..res):find("^([A-Z]+)%s+(.-)%s+(%d+)%s*(.*)$")
  if cmd == 'SETB' then set_breakpoint(file, tonumber(line), string.len(condition) > 0 and mobdebug.loadstring("return " .. condition) or nil)
  elseif cmd == 'DELB' then remove_breakpoint(file, tonumber(line))
  else
    -- this looks like a breakpoint command, but something went wrong;
    -- return here to let the "normal" processing to handle,
    -- although this is likely to not go well.
    return
  end

  buf = nil
end

local function normalize_path(file)
  local n
  repeat
    file, n = file:gsub("/+%.?/+","/") -- remove all `//` and `/./` references
  until n == 0
  -- collapse all up-dir references: this will clobber UNC prefix (\\?\)
  -- and disk on Windows when there are too many up-dir references: `D:\foo\..\..\bar`;
  -- handle the case of multiple up-dir references: `foo/bar/baz/../../../more`;
  -- only remove one at a time as otherwise `../../` could be removed;
  repeat
    file, n = file:gsub("[^/]+/%.%./", "", 1)
  until n == 0
  -- there may still be a leading up-dir reference left (as `/../` or `../`); remove it
  return (file:gsub("^(/?)%.%./", "%1"))
end

local function get_filename(file)
  -- technically, users can supply names that may not use '@',
  -- for example when they call loadstring('...', 'filename.lua').
  -- Unfortunately, there is no reliable/quick way to figure out
  -- what is the filename and what is the source code.
  -- If the name doesn't start with `@`, assume it's a file name if it's all on one line.
  if find(file, "^[@=]") or not find(file, "[\r\n]") then
    file = gsub(gsub(file, "^[@=]", ""), "\\", "/")
    -- normalize paths that may include up-dir or same-dir references
    -- if the path starts from the up-dir or reference,
    -- prepend `basedir` to generate absolute path to keep breakpoints working.
    -- ignore qualified relative path (`D:../`) and UNC paths (`\\?\`)
    if find(file, "^%.%./") then file = basedir..file end
    if find(file, "/%.%.?/") then file = normalize_path(file) end
    -- need this conversion to be applied to relative and absolute
    -- file names as you may write "require 'Foo'" to
    -- load "foo.lua" (on a case insensitive file system) and breakpoints
    -- set on foo.lua will not work if not converted to the same case.
    if iscasepreserving then file = string.lower(file) end
    if find(file, "^%./") then file = sub(file, 3)
    else file = gsub(file, "^"..q(basedir), "") end
    -- some file systems allow newlines in file names; remove these.
    file = gsub(file, "\n", ' ')
  else
    file = mobdebug.line(file)
  end
  return file
end

local function get_server_data(file)
  if not is_pending(server) then 
    checkcount = checkcount + 1
    return
  end
  checkcount = mobdebug.checkcount
  if corostatus(coro_debugger) == "suspended" then
    local state_vars = capture_vars(3)
    local status, res = cororesume(coro_debugger, nil, state_vars, file)
    if not status and res then
      error(res, 2) -- report any other (internal) errors back to the application 
    end
    -- handle 'stack' command that provides stack() information to the debugger
    while status and res == 'stack' do
      -- resume with the stack trace and variables
      if vars then restore_vars(vars) end -- restore vars so they are reflected in stack values
      local st = stack(4)
      if st[1] and st[1][1] and st[1][1][2] == "[C]" then
        table.remove(st, 1)
      end
      status, res = cororesume(coro_debugger, events.STACK, st, file, line)
    end
    handle_breakpoint(server)
  end
end

local function debug_hook(event, line)
  -- (1) LuaJIT needs special treatment. Because debug_hook is set for
  -- *all* coroutines, and not just the one being debugged as in regular Lua
  -- (http://lua-users.org/lists/lua-l/2011-06/msg00513.html),
  -- need to avoid debugging mobdebug's own code as LuaJIT doesn't
  -- always correctly generate call/return hook events (there are more
  -- calls than returns, which breaks stack depth calculation and
  -- 'step' and 'step over' commands stop working; possibly because
  -- 'tail return' events are not generated by LuaJIT).
  -- the next line checks if the debugger is run under LuaJIT and if
  -- one of debugger methods is present in the stack, it simply returns.
  if jit then
    -- when luajit is compiled with LUAJIT_ENABLE_LUA52COMPAT,
    -- coroutine.running() returns non-nil for the main thread.
    local coro, main = coroutine.running()
    if not coro or main then coro = 'main' end
    local disabled = coroutines[coro] == false
      or coroutines[coro] == nil and coro ~= (coro_debugee or 'main')
    if coro_debugee and disabled or not coro_debugee and (disabled or in_debugger())
    then return end
  end

  -- (2) check if abort has been requested and it's safe to abort
  if abort and is_safe(stack_level) then error(abort) end

  -- (3) also check if this debug hook has not been visited for any reason.
  -- this check is needed to avoid stepping in too early
  -- (for example, when coroutine.resume() is executed inside start()).
  if not seen_hook and in_debugger() then return end

  if event == "call" then
    stack_level = stack_level + 1
  elseif event == "return" or event == "tail return" then
    stack_level = stack_level - 1
    if mobdebug.on_return_hook then mobdebug.on_return_hook() end
  elseif event == "line" then
    if mobdebug.linemap then
      local ok, mappedline = pcall(mobdebug.linemap, line, debug.getinfo(2, "S").source)
      if ok then line = mappedline end
      if not line then return end
    end

    -- may need to fall through because of the following:
    -- (1) step_into
    -- (2) step_over and stack_level <= step_level (need stack_level)
    -- (3) breakpoint; check for line first as it's known; then for file
    -- (4) socket call (only do every Xth check)
    -- (5) at least one watch is registered
    if not (
      step_into or step_over or breakpoints[line] or watchescnt > 0
      or is_pending(server)
    ) then checkcount = checkcount + 1; return end

    checkcount = mobdebug.checkcount -- force check on the next command

    -- this is needed to check if the stack got shorter or longer.
    -- unfortunately counting call/return calls is not reliable.
    -- the discrepancy may happen when "pcall(load, '')" call is made
    -- or when "error()" is called in a function.
    -- in either case there are more "call" than "return" events reported.
    -- this validation is done for every "line" event, but should be "cheap"
    -- as it checks for the stack to get shorter (or longer by one call).
    -- start from one level higher just in case we need to grow the stack.
    -- this may happen after coroutine.resume call to a function that doesn't
    -- have any other instructions to execute. it triggers three returns:
    -- "return, tail return, return", which needs to be accounted for.
    stack_level = stack_depth(stack_level+1)

    local caller = debug.getinfo(2, "S")

    -- grab the filename and fix it if needed
    local file = lastfile
    if (lastsource ~= caller.source) then
      lastsource = caller.source
      file = get_filename(caller.source)

      -- set to true if we got here; this only needs to be done once per
      -- session, so do it here to at least avoid setting it for every line.
      seen_hook = true
      lastfile = file
    end

    if is_pending(server) then handle_breakpoint(server) end

    local vars, status, res
    if (watchescnt > 0) then
      vars = capture_vars(1)
      for index, value in pairs(watches) do
        setfenv(value, vars)
        local ok, fired = pcall(value)
        if ok and fired then
          status, res = cororesume(coro_debugger, events.WATCH, vars, file, line, index)
          break -- any one watch is enough; don't check multiple times
        end
      end
    end

    -- need to get into the "regular" debug handler, but only if there was
    -- no watch that was fired. If there was a watch, handle its result.
    local no_watch_fired = status == nil

    -- Before conditional breakpoints, getin was defined as: 
    --   no_watch_fired and (step or has_breakpoint(file, line) or is_pending(server))
    -- With conditional breakpoints, we might need to capture vars to check if we should perform a break.
    -- The following code up to `local getin = ...` basically unrolls that check so we can reuse/capture vars
    -- while checking the conditional breakpoint
    local step = no_watch_fired and
        (step_into
          -- when coroutine.running() return `nil` (main thread in Lua 5.1),
          -- step_over will equal 'main', so need to check for that explicitly.
          or (step_over and step_over == (coroutine.running() or 'main') and stack_level <= step_level))
    -- check breakpoint only if we are not stepping in
    local breakpoint = no_watch_fired and not step and get_breakpoint(file, line)
    local has_breakpoint
    if type(breakpoint) == "function" then
      vars = vars or capture_vars(1)
      setfenv(breakpoint, vars)
      local ok, breakpoint_result = pcall(breakpoint)
      has_breakpoint = ok and breakpoint_result
    else
      has_breakpoint = breakpoint
    end

    local getin = step or has_breakpoint or (no_watch_fired and is_pending(server))

    if getin then
      vars = vars or capture_vars(1)
      step_into = false
      step_over = false
      status, res = cororesume(coro_debugger, events.BREAK, vars, file, line)
    end

    -- handle 'stack' command that provides stack() information to the debugger
    while status and res == 'stack' do
      -- resume with the stack trace and variables
      if vars then restore_vars(vars) end -- restore vars so they are reflected in stack values
      status, res = cororesume(coro_debugger, events.STACK, stack(3), file, line)
    end

    -- need to recheck once more as resume after 'stack' command may
    -- return something else (for example, 'exit'), which needs to be handled
    if status and res and res ~= 'stack' then
      if not abort and res == "exit" then mobdebug.onexit(1, true); return end
      if not abort and res == "done" then mobdebug.done(); return end
      abort = res
      -- only abort if safe; if not, there is another (earlier) check inside
      -- debug_hook, which will abort execution at the first safe opportunity
      if is_safe(stack_level) then error(abort) end
    elseif not status and res then
      error(res, 2) -- report any other (internal) errors back to the application
    end

    if vars then restore_vars(vars) end

    -- last command requested Step Over/Out; store the current thread
    if step_over == true then step_over = coroutine.running() or 'main' end
  end
end

local function stringify_results(params, status, ...)
  if not status then return status, ... end -- on error report as it

  params = params or {}
  if params.nocode == nil then params.nocode = true end
  if params.comment == nil then params.comment = 1 end

  local t = {...}
  for i,v in pairs(t) do -- stringify each of the returned values
    local ok, res = pcall(mobdebug.line, v, params)
    if ok and res ~= nil then
      t[i] = res
    else
      t[i] = ("%q"):format(res):gsub("\010","n"):gsub("\026","\\026")
    end
  end
  -- stringify table with all returned values
  -- this is done to allow each returned value to be used (serialized or not)
  -- intependently and to preserve "original" comments
  return pcall(mobdebug.dump, t, {sparse = false})
end

local function isrunning()
  return coro_debugger and (corostatus(coro_debugger) == 'suspended' or corostatus(coro_debugger) == 'running')
end

-- this is a function that removes all hooks and closes the socket to
-- report back to the controller that the debugging is done.
-- the script that called `done` can still continue.
local function done()
  if not (isrunning() and server) then return end

  if not jit then
    for co, debugged in pairs(coroutines) do
      if debugged then debug.sethook(co) end
    end
  end

  debug.sethook()
  server:close()

  coro_debugger = nil -- to make sure isrunning() returns `false`
  seen_hook = nil -- to make sure that the next start() call works
  abort = nil -- to make sure that callback calls use proper "abort" value
end

local function respond_with_edn(val, analyze_params)
  local analyze_ok, graph, edge_refs = pcall(edn.analyze, val, analyze_params)
  if analyze_ok then
    for k, v in pairs(edge_refs) do
      edn_edge_refs_cache[k] = v
    end
    local serialize_ok, edn_string = pcall(edn.serialize, val, graph)
    if serialize_ok then
      server:send("200 OK " .. tostring(edn_string) .. "\n")
    else
      server:send("401 Error in Execution " .. tostring(#edn_string) .. "\n")
      server:send(edn_string)
    end
  else
    server:send("401 Error in Execution " .. tostring(#graph) .. "\n")
    server:send(graph)
  end
end

local function debugger_loop(sev, svars, sfile, sline)
  local command
  local app, osname
  local eval_env = svars or {}
  local function emptyWatch () return false end
  local loaded = {}
  for k in pairs(package.loaded) do loaded[k] = true end

  while true do
    local line, err
    local wx = rawget(genv, "wx") -- use rawread to make strict.lua happy
    if (wx or mobdebug.yield) and server.settimeout then server:settimeout(mobdebug.yieldtimeout) end
    while true do
      line, err = server:receive()
      if not line and err == "timeout" then
        -- yield for wx GUI applications if possible to avoid "busyness"
        app = app or (wx and wx.wxGetApp and wx.wxGetApp())
        if app then
          local win = app:GetTopWindow()
          local inloop = app:IsMainLoopRunning()
          osname = osname or wx.wxPlatformInfo.Get():GetOperatingSystemFamilyName()
          if win and not inloop then
            -- process messages in a regular way
            -- and exit as soon as the event loop is idle
            if osname == 'Unix' then wx.wxTimer(app):Start(10, true) end
            local exitLoop = function()
              win:Disconnect(wx.wxID_ANY, wx.wxID_ANY, wx.wxEVT_IDLE)
              win:Disconnect(wx.wxID_ANY, wx.wxID_ANY, wx.wxEVT_TIMER)
              app:ExitMainLoop()
            end
            win:Connect(wx.wxEVT_IDLE, exitLoop)
            win:Connect(wx.wxEVT_TIMER, exitLoop)
            app:MainLoop()
          end
        elseif mobdebug.yield then mobdebug.yield()
        end
      elseif not line and err == "closed" then
        error("Debugger connection closed", 0)
      else
        -- if there is something in the pending buffer, prepend it to the line
        if buf then line = buf .. line; buf = nil end
        break
      end
    end
    if server.settimeout then server:settimeout() end -- back to blocking
    command = string.sub(line, string.find(line, "^[A-Z]+"))
    if mobdebug.command_hook then mobdebug.command_hook(command) end
    if command == "SETB" then
      local _, file, breakpoint_line, condition = string.match(line, "^([A-Z]+)%s+(.-)%s+(%d+)%s*(.*)$")
      if file and breakpoint_line and condition then
        local chunk = string.len(condition) > 0 and mobdebug.loadstring("return " .. condition) or nil
        set_breakpoint(file, tonumber(breakpoint_line), chunk)
        server:send("200 OK\n")
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "DELB" then
      local _, _, _, file, line = string.find(line, "^([A-Z]+)%s+(.-)%s+(%d+)%s*$")
      if file and line then
        remove_breakpoint(file, tonumber(line))
        server:send("200 OK\n")
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "EXEC" then
      -- extract any optional parameters
      local params = string.match(line, "--%s*(%b{})%s*$")
      local _, _, chunk = string.find(line, "^[A-Z]+%s+(.+)$")
      if chunk then
        local func, res = mobdebug.loadstring(chunk)
        local status
        if func then
          local pfunc = params and loadstring("return "..params) -- use internal function
          params = pfunc and pfunc()
          params = (type(params) == "table" and params or {})
          local stack = tonumber(params.stack)
          -- if the requested stack frame is not the current one, then use a new capture
          -- with a specific stack frame: `capture_vars(0, coro_debugee)`
          local env = stack and coro_debugee and capture_vars(stack-1, coro_debugee) or eval_env
          setfenv(func, env)
          status, res = stringify_results(params, pcall(func, unpack(env['...'] or {})))
        end
        if status then
          if mobdebug.onscratch then mobdebug.onscratch(res) end
          server:send("200 OK " .. tostring(#res) .. "\n")
          server:send(res)
        else
          -- fix error if not set (for example, when loadstring is not present)
          if not res then res = "Unknown error" end
          server:send("401 Error in Expression " .. tostring(#res) .. "\n")
          server:send(res)
        end
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "LOAD" then
      local _, _, size, name = string.find(line, "^[A-Z]+%s+(%d+)%s+(%S.-)%s*$")
      size = tonumber(size)

      if abort == nil then -- no LOAD/RELOAD allowed inside start()
        if size > 0 then server:receive(size) end
        if sfile and sline then
          server:send("201 Started " .. sfile .. " " .. tostring(sline) .. "\n")
        else
          server:send("200 OK 0\n")
        end
      else
        -- reset environment to allow required modules to load again
        -- remove those packages that weren't loaded when debugger started
        for k in pairs(package.loaded) do
          if not loaded[k] then package.loaded[k] = nil end
        end

        if size == 0 and name == '-' then -- RELOAD the current script being debugged
          server:send("200 OK 0\n")
          coroyield("load")
        else
          -- receiving 0 bytes blocks (at least in luasocket 2.0.2), so skip reading
          local chunk = size == 0 and "" or server:receive(size)
          if chunk then -- LOAD a new script for debugging
            local func, res = mobdebug.loadstring(chunk, "@"..name)
            if func then
              server:send("200 OK 0\n")
              debugee = func
              coroyield("load")
            else
              server:send("401 Error in Expression " .. tostring(#res) .. "\n")
              server:send(res)
            end
          else
            server:send("400 Bad Request\n")
          end
        end
      end
    elseif command == "SETW" then
      local _, _, exp = string.find(line, "^[A-Z]+%s+(.+)%s*$")
      if exp then
        local func, res = mobdebug.loadstring("return(" .. exp .. ")")
        if func then
          watchescnt = watchescnt + 1
          local newidx = #watches + 1
          watches[newidx] = func
          server:send("200 OK " .. tostring(newidx) .. "\n")
        else
          server:send("401 Error in Expression " .. tostring(#res) .. "\n")
          server:send(res)
        end
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "DELW" then
      local _, _, index = string.find(line, "^[A-Z]+%s+(%d+)%s*$")
      index = tonumber(index)
      if index > 0 and index <= #watches then
        watchescnt = watchescnt - (watches[index] ~= emptyWatch and 1 or 0)
        watches[index] = emptyWatch
        server:send("200 OK\n")
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "RUN" then
      edn_edge_refs_cache = {}
      server:send("200 OK\n")

      local ev, vars, file, line, idx_watch = coroyield()
      eval_env = vars
      if ev == events.BREAK then
        server:send("202 Paused " .. file .. " " .. tostring(line) .. "\n")
      elseif ev == events.WATCH then
        server:send("203 Paused " .. file .. " " .. tostring(line) .. " " .. tostring(idx_watch) .. "\n")
      elseif ev == events.RESTART then
        -- nothing to do
      else
        server:send("401 Error in Execution " .. tostring(#file) .. "\n")
        server:send(file)
      end
    elseif command == "STEP" then
      edn_edge_refs_cache = {}
      server:send("200 OK\n")
      step_into = true

      local ev, vars, file, line, idx_watch = coroyield()
      eval_env = vars
      if ev == events.BREAK then
        server:send("202 Paused " .. file .. " " .. tostring(line) .. "\n")
      elseif ev == events.WATCH then
        server:send("203 Paused " .. file .. " " .. tostring(line) .. " " .. tostring(idx_watch) .. "\n")
      elseif ev == events.RESTART then
        -- nothing to do
      else
        server:send("401 Error in Execution " .. tostring(#file) .. "\n")
        server:send(file)
      end
    elseif command == "OVER" or command == "OUT" then
      edn_edge_refs_cache = {}
      server:send("200 OK\n")
      step_over = true

      -- OVER and OUT are very similar except for
      -- the stack level value at which to stop
      if command == "OUT" then step_level = stack_level - 1
      else step_level = stack_level end

      local ev, vars, file, line, idx_watch = coroyield()
      eval_env = vars
      if ev == events.BREAK then
        server:send("202 Paused " .. file .. " " .. tostring(line) .. "\n")
      elseif ev == events.WATCH then
        server:send("203 Paused " .. file .. " " .. tostring(line) .. " " .. tostring(idx_watch) .. "\n")
      elseif ev == events.RESTART then
        -- nothing to do
      else
        server:send("401 Error in Execution " .. tostring(#file) .. "\n")
        server:send(file)
      end
    elseif command == "BASEDIR" then
      local _, _, dir = string.find(line, "^[A-Z]+%s+(.+)%s*$")
      if dir then
        basedir = iscasepreserving and string.lower(dir) or dir
        -- reset cached source as it may change with basedir
        lastsource = nil
        server:send("200 OK\n")
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "SUSPEND" then
      -- do nothing; it already fulfilled its role
    elseif command == "DONE" then
      edn_edge_refs_cache = {}
      coroyield("done")
      return -- done with all the debugging
    elseif command == "STACK" then
      -- first check if we can execute the stack command
      -- as it requires yielding back to debug_hook it cannot be executed
      -- if we have not seen the hook yet as happens after start().
      -- in this case we simply return an empty result
      local vars, ev = {}
      if seen_hook then
        ev, vars = coroyield("stack")
      end
      if ev and ev ~= events.STACK then
        server:send("401 Error in Execution " .. tostring(#vars) .. "\n")
        server:send(vars)
      else
        local params = string.match(line, "--%s*(%b{})%s*$")
        local pfunc = params and loadstring("return "..params) -- use internal function
        params = pfunc and pfunc()
        params = (type(params) == "table" and params or {})
        if params.nocode == nil then params.nocode = true end
        if params.sparse == nil then params.sparse = false end
        -- take into account additional levels for the stack frames and data management
        if tonumber(params.maxlevel) then params.maxlevel = tonumber(params.maxlevel)+4 end
        respond_with_edn(vars, params)
      end
    elseif command == "REF" then
      local ref = string.match(line, "^[A-Z]+%s+(%S+)")
      local val = edn_edge_refs_cache[ref]
      if val then
        local params = string.match(line, "--%s*(%b{})%s*$")
        local pfunc = params and loadstring("return "..params)
        params = pfunc and pfunc()
        params = (type(params) == "table" and params or {})
        respond_with_edn(val, params)
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "OUTPUT" then
      local _, _, stream, mode = string.find(line, "^[A-Z]+%s+(%w+)%s+([dcr])%s*$")
      if stream and mode and stream == "stdout" then
        -- assign "print" in the global environment
        local default = mode == 'd'
        genv.print = default and iobase.print or corowrap(function()
          -- wrapping into coroutine.wrap protects this function from
          -- being stepped through in the debugger.
          -- don't use vararg (...) as it adds a reference for its values,
          -- which may affect how they are garbage collected
          while true do
            local tbl = {coroutine.yield()}
            if mode == 'c' then iobase.print(unpack(tbl)) end
            for n = 1, #tbl do
              tbl[n] = select(2, pcall(mobdebug.line, tbl[n], {nocode = true, comment = false})) end
            local file = table.concat(tbl, "\t").."\n"
            server:send("204 Output " .. stream .. " " .. tostring(#file) .. "\n" .. file)
          end
        end)
        if not default then genv.print() end -- "fake" print to start printing loop
        server:send("200 OK\n")
      else
        server:send("400 Bad Request\n")
      end
    elseif command == "EXIT" then
      server:send("200 OK\n")
      coroyield("exit")
    else
      server:send("400 Bad Request\n")
    end
  end
end

local function output(stream, data)
  if server then return server:send("204 Output "..stream.." "..tostring(#data).."\n"..data) end
end

local function connect(controller_host, controller_port)
  local sock, err = socket.tcp()
  if not sock then return nil, err end

  if sock.settimeout then sock:settimeout(mobdebug.connecttimeout) end
  local res, err = sock:connect(controller_host, tostring(controller_port))
  if sock.settimeout then sock:settimeout() end

  if not res then return nil, err end
  return sock
end

local lasthost, lastport

-- Starts a debug session by connecting to a controller
local function start(controller_host, controller_port)
  -- only one debugging session can be run (as there is only one debug hook)
  if isrunning() then return end

  lasthost = controller_host or lasthost
  lastport = controller_port or lastport

  controller_host = lasthost or "localhost"
  controller_port = lastport or mobdebug.port

  local err
  server, err = mobdebug.connect(controller_host, controller_port)
  if server then
    -- correct stack depth which already has some calls on it
    -- so it doesn't go into negative when those calls return
    -- as this breaks subsequence checks in stack_depth().
    -- start from 16th frame, which is sufficiently large for this check.
    stack_level = stack_depth(16)

    -- provide our own traceback function to report errors remotely
    -- but only under Lua 5.1/LuaJIT as it's not called under Lua 5.2+
    -- (http://lua-users.org/lists/lua-l/2016-05/msg00297.html)
    local function f() return function()end end
    if f() ~= f() then -- Lua 5.1 or LuaJIT
      local dtraceback = debug.traceback
      debug.traceback = function (...)
        if select('#', ...) >= 1 then
          local thr, err, lvl = ...
          if type(thr) ~= 'thread' then err, lvl = thr, err end
          local trace = dtraceback(err, (lvl or 1)+1)
          if genv.print == iobase.print then -- no remote redirect
            return trace
          else
            genv.print(trace) -- report the error remotely
            return -- don't report locally to avoid double reporting
          end
        end
        -- direct call to debug.traceback: return the original.
        -- debug.traceback(nil, level) doesn't work in Lua 5.1
        -- (http://lua-users.org/lists/lua-l/2011-06/msg00574.html), so
        -- simply remove first frame from the stack trace
        local tb = dtraceback("", 2) -- skip debugger frames
        -- if the string is returned, then remove the first new line as it's not needed
        return type(tb) == "string" and tb:gsub("^\n","") or tb
      end
    end
    coro_debugger = corocreate(debugger_loop)
    debug.sethook(debug_hook, HOOKMASK)
    seen_hook = nil -- reset in case the last start() call was refused
    step_into = true -- start with step command
    return true
  else
    print(("Could not connect to %s:%s: %s")
      :format(controller_host, controller_port, err or "unknown error"))
  end
end

-- Starts a debug session by waiting for a controller to connect
local function listen(port)
  -- only one debugging session can be run (as there is only one debug hook)
  if isrunning() then return end

  local err
  local listen_socket, err = socket.bind("*", port or mobdebug.port)

  if err then error(err) end

  print("Listening for debugger on " .. listen_socket:getsockname())

  server, err = listen_socket:accept()

  print("Debugger connected from " .. server:getsockname())

  listen_socket:close()

  if server then
    -- correct stack depth which already has some calls on it
    -- so it doesn't go into negative when those calls return
    -- as this breaks subsequence checks in stack_depth().
    -- start from 16th frame, which is sufficiently large for this check.
    stack_level = stack_depth(16)

    -- provide our own traceback function to report errors remotely
    -- but only under Lua 5.1/LuaJIT as it's not called under Lua 5.2+
    -- (http://lua-users.org/lists/lua-l/2016-05/msg00297.html)
    local function f() return function()end end
    if f() ~= f() then -- Lua 5.1 or LuaJIT
      local dtraceback = debug.traceback
      debug.traceback = function (...)
        if select('#', ...) >= 1 then
          local thr, err, lvl = ...
          if type(thr) ~= 'thread' then err, lvl = thr, err end
          local trace = dtraceback(err, (lvl or 1)+1)
          if genv.print == iobase.print then -- no remote redirect
            return trace
          else
            genv.print(trace) -- report the error remotely
            return -- don't report locally to avoid double reporting
          end
        end
        -- direct call to debug.traceback: return the original.
        -- debug.traceback(nil, level) doesn't work in Lua 5.1
        -- (http://lua-users.org/lists/lua-l/2011-06/msg00574.html), so
        -- simply remove first frame from the stack trace
        local tb = dtraceback("", 2) -- skip debugger frames
        -- if the string is returned, then remove the first new line as it's not needed
        return type(tb) == "string" and tb:gsub("^\n","") or tb
      end
    end
    coro_debugger = corocreate(debugger_loop)
    debug.sethook(debug_hook, HOOKMASK)
    seen_hook = nil -- reset in case the last start() call was refused
    step_into = true -- start with step command
    return true
  else
    print(("Could not connect to %s:%s: %s")
      :format(controller_host, controller_port, err or "unknown error"))
  end
end

local function controller(controller_host, controller_port, scratchpad)
  -- only one debugging session can be run (as there is only one debug hook)
  if isrunning() then return end

  lasthost = controller_host or lasthost
  lastport = controller_port or lastport

  controller_host = lasthost or "localhost"
  controller_port = lastport or mobdebug.port

  local exitonerror = not scratchpad
  local err
  server, err = mobdebug.connect(controller_host, controller_port)
  if server then
    local function report(trace, err)
      local msg = err .. "\n" .. trace
      server:send("401 Error in Execution " .. tostring(#msg) .. "\n")
      server:send(msg)
      return err
    end

    seen_hook = true -- allow to accept all commands
    coro_debugger = corocreate(debugger_loop)

    while true do
      step_into = true -- start with step command
      abort = false -- reset abort flag from the previous loop
      if scratchpad then checkcount = mobdebug.checkcount end -- force suspend right away

      coro_debugee = corocreate(debugee)
      debug.sethook(coro_debugee, debug_hook, HOOKMASK)
      local status, err = cororesume(coro_debugee, unpack(arg or {}))

      -- was there an error or is the script done?
      -- 'abort' state is allowed here; ignore it
      if abort then
        if tostring(abort) == 'exit' then break end
      else
        if status then -- normal execution is done
          break
        elseif err and not string.find(tostring(err), deferror) then
          -- report the error back
          -- err is not necessarily a string, so convert to string to report
          report(debug.traceback(coro_debugee), tostring(err))
          if exitonerror then break end
          -- check if the debugging is done (coro_debugger is nil)
          if not coro_debugger then break end
          -- resume once more to clear the response the debugger wants to send
          -- need to use capture_vars(0) to capture only two (default) level,
          -- as even though there is controller() call, because of the tail call,
          -- the caller may not exist for it;
          -- This is not entirely safe as the user may see the local
          -- variable from console, but they will be reset anyway.
          -- This functionality is used when scratchpad is paused to
          -- gain access to remote console to modify global variables.
          local status, err = cororesume(coro_debugger, events.RESTART, capture_vars(0))
          if not status or status and err == "exit" then break end
        end
      end
    end
  else
    print(("Could not connect to %s:%s: %s")
      :format(controller_host, controller_port, err or "unknown error"))
    return false
  end
  return true
end

local function scratchpad(controller_host, controller_port)
  return controller(controller_host, controller_port, true)
end

local function loop(controller_host, controller_port)
  return controller(controller_host, controller_port, false)
end

local function on()
  if not (isrunning() and server) then return end

  -- main is set to true under Lua5.2 for the "main" chunk.
  -- Lua5.1 returns co as `nil` in that case.
  local co, main = coroutine.running()
  if main then co = nil end
  if co then
    coroutines[co] = true
    debug.sethook(co, debug_hook, HOOKMASK)
    return true
  else
    if jit then coroutines.main = true end
    debug.sethook(debug_hook, HOOKMASK)
    return true
  end
end

local function off()
  if not (isrunning() and server) then return end

  -- main is set to true under Lua5.2 for the "main" chunk.
  -- Lua5.1 returns co as `nil` in that case.
  local co, main = coroutine.running()
  if main then co = nil end

  -- don't remove coroutine hook under LuaJIT as there is only one (global) hook
  if co then
    coroutines[co] = false
    if not jit then debug.sethook(co) end
  else
    if jit then coroutines.main = false end
    if not jit then debug.sethook() end
  end

  -- check if there is any thread that is still being debugged under LuaJIT;
  -- if not, turn the debugging off
  if jit then
    local remove = true
    for _, debugged in pairs(coroutines) do
      if debugged then remove = false; break end
    end
    if remove then debug.sethook() end
  end
end

-- Handles server debugging commands
local function handle(params, client, options)
  -- when `options.verbose` is not provided, use normal `print`; verbose output can be
  -- disabled (`options.verbose == false`) or redirected (`options.verbose == function()...end`)
  local verbose = not options or options.verbose ~= nil and options.verbose
  local print = verbose and (type(verbose) == "function" and verbose or print) or function() end
  local file, line, watch_idx
  local _, _, command = string.find(params, "^([a-z]+)")
  if command == "run" or command == "step" or command == "out"
  or command == "over" or command == "exit" then
    client:send(string.upper(command) .. "\n")
    client:receive() -- this should consume the first '200 OK' response
    while true do
      local done = true
      local breakpoint = client:receive()
      if not breakpoint then
        print("Program finished")
        return nil, nil, false
      end
      local _, _, status = string.find(breakpoint, "^(%d+)")
      if status == "200" then
        -- don't need to do anything
      elseif status == "202" then
        _, _, file, line = string.find(breakpoint, "^202 Paused%s+(.-)%s+(%d+)%s*$")
        if file and line then
          print("Paused at file " .. file .. " line " .. line)
        end
      elseif status == "203" then
        _, _, file, line, watch_idx = string.find(breakpoint, "^203 Paused%s+(.-)%s+(%d+)%s+(%d+)%s*$")
        if file and line and watch_idx then
          print("Paused at file " .. file .. " line " .. line .. " (watch expression " .. watch_idx .. ": [" .. watches[watch_idx] .. "])")
        end
      elseif status == "204" then
        local _, _, stream, size = string.find(breakpoint, "^204 Output (%w+) (%d+)$")
        if stream and size then
          local size = tonumber(size)
          local msg = size > 0 and client:receive(size) or ""
          print(msg)
          if outputs[stream] then outputs[stream](msg) end
          -- this was just the output, so go back reading the response
          done = false
        end
      elseif status == "401" then
        local _, _, size = string.find(breakpoint, "^401 Error in Execution (%d+)$")
        if size then
          local msg = client:receive(tonumber(size))
          print("Error in remote application: " .. msg)
          return nil, nil, msg
        end
      else
        print("Unknown error")
        return nil, nil, "Debugger error: unexpected response '" .. breakpoint .. "'"
      end
      if done then break end
    end
  elseif command == "done" then
    client:send(string.upper(command) .. "\n")
    -- no response is expected
  elseif command == "setb" or command == "asetb" then
    _, _, _, file, line = string.find(params, "^([a-z]+)%s+(.-)%s+(%d+)%s*$")
    if file and line then
      -- if this is a file name, and not a file source
      if not file:find('^".*"$') then
        file = string.gsub(file, "\\", "/") -- convert slash
        file = removebasedir(file, basedir)
      end
      client:send("SETB " .. file .. " " .. line .. "\n")
      if command == "asetb" or client:receive() == "200 OK" then
        set_breakpoint(file, line)
      else
        print("Error: breakpoint not inserted")
      end
    else
      print("Invalid command")
    end
  elseif command == "setw" then
    local _, _, exp = string.find(params, "^[a-z]+%s+(.+)$")
    if exp then
      client:send("SETW " .. exp .. "\n")
      local answer = client:receive()
      local _, _, watch_idx = string.find(answer, "^200 OK (%d+)%s*$")
      if watch_idx then
        watches[watch_idx] = exp
        print("Inserted watch exp no. " .. watch_idx)
      else
        local _, _, size = string.find(answer, "^401 Error in Expression (%d+)$")
        if size then
          local err = client:receive(tonumber(size)):gsub(".-:%d+:%s*","")
          print("Error: watch expression not set: " .. err)
        else
          print("Error: watch expression not set")
        end
      end
    else
      print("Invalid command")
    end
  elseif command == "delb" or command == "adelb" then
    _, _, _, file, line = string.find(params, "^([a-z]+)%s+(.-)%s+(%d+)%s*$")
    if file and line then
      -- if this is a file name, and not a file source
      if not file:find('^".*"$') then
        file = string.gsub(file, "\\", "/") -- convert slash
        file = removebasedir(file, basedir)
      end
      client:send("DELB " .. file .. " " .. line .. "\n")
      if command == "adelb" or client:receive() == "200 OK" then
        remove_breakpoint(file, line)
      else
        print("Error: breakpoint not removed")
      end
    else
      print("Invalid command")
    end
  elseif command == "delallb" then
    local file, line = "*", 0
    client:send("DELB " .. file .. " " .. tostring(line) .. "\n")
    if client:receive() == "200 OK" then
      remove_breakpoint(file, line)
    else
      print("Error: all breakpoints not removed")
    end
  elseif command == "delw" then
    local _, _, index = string.find(params, "^[a-z]+%s+(%d+)%s*$")
    if index then
      client:send("DELW " .. index .. "\n")
      if client:receive() == "200 OK" then
        watches[index] = nil
      else
        print("Error: watch expression not removed")
      end
    else
      print("Invalid command")
    end
  elseif command == "delallw" then
    for index, exp in pairs(watches) do
      client:send("DELW " .. index .. "\n")
      if client:receive() == "200 OK" then
        watches[index] = nil
      else
        print("Error: watch expression at index " .. index .. " [" .. exp .. "] not removed")
      end
    end
  elseif command == "eval" or command == "exec"
      or command == "load" or command == "loadstring"
      or command == "reload" then
    local _, _, exp = string.find(params, "^[a-z]+%s+(.+)$")
    if exp or (command == "reload") then
      if command == "eval" or command == "exec" then
        exp = (exp:gsub("%-%-%[(=*)%[.-%]%1%]", "") -- remove comments
                  :gsub("%-%-.-\n", " ") -- remove line comments
                  :gsub("\n", " ")) -- convert new lines
        if command == "eval" then exp = "return " .. exp end
        client:send("EXEC " .. exp .. "\n")
      elseif command == "reload" then
        client:send("LOAD 0 -\n")
      elseif command == "loadstring" then
        local _, _, _, file, lines = string.find(exp, "^([\"'])(.-)%1%s+(.+)")
        if not file then
           _, _, file, lines = string.find(exp, "^(%S+)%s+(.+)")
        end
        client:send("LOAD " .. tostring(#lines) .. " " .. file .. "\n")
        client:send(lines)
      else
        local file = io.open(exp, "r")
        if not file and pcall(require, "winapi") then
          -- if file is not open and winapi is there, try with a short path;
          -- this may be needed for unicode paths on windows
          winapi.set_encoding(winapi.CP_UTF8)
          local shortp = winapi.short_path(exp)
          file = shortp and io.open(shortp, "r")
        end
        if not file then return nil, nil, "Cannot open file " .. exp end
        -- read the file and remove the shebang line as it causes a compilation error
        local lines = file:read("*all"):gsub("^#!.-\n", "\n")
        file:close()

        local file = string.gsub(exp, "\\", "/") -- convert slash
        file = removebasedir(file, basedir)
        client:send("LOAD " .. tostring(#lines) .. " " .. file .. "\n")
        if #lines > 0 then client:send(lines) end
      end
      while true do
        local params, err = client:receive()
        if not params then
          return nil, nil, "Debugger connection " .. (err or "error")
        end
        local done = true
        local _, _, status, len = string.find(params, "^(%d+).-%s+(%d+)%s*$")
        if status == "200" then
          len = tonumber(len)
          if len > 0 then
            local status, res
            local str = client:receive(len)
            -- handle serialized table with results
            local func, err = loadstring(str)
            if func then
              status, res = pcall(func)
              if not status then err = res
              elseif type(res) ~= "table" then
                err = "received "..type(res).." instead of expected 'table'"
              end
            end
            if err then
              print("Error in processing results: " .. err)
              return nil, nil, "Error in processing results: " .. err
            end
            print(unpack(res))
            return res[1], res
          end
        elseif status == "201" then
          _, _, file, line = string.find(params, "^201 Started%s+(.-)%s+(%d+)%s*$")
        elseif status == "202" or params == "200 OK" then
          -- do nothing; this only happens when RE/LOAD command gets the response
          -- that was for the original command that was aborted
        elseif status == "204" then
          local _, _, stream, size = string.find(params, "^204 Output (%w+) (%d+)$")
          if stream and size then
            local size = tonumber(size)
            local msg = size > 0 and client:receive(size) or ""
            print(msg)
            if outputs[stream] then outputs[stream](msg) end
            -- this was just the output, so go back reading the response
            done = false
          end
        elseif status == "401" then
          len = tonumber(len)
          local res = client:receive(len)
          print("Error in expression: " .. res)
          return nil, nil, res
        else
          print("Unknown error")
          return nil, nil, "Debugger error: unexpected response after EXEC/LOAD '" .. params .. "'"
        end
        if done then break end
      end
    else
      print("Invalid command")
    end
  elseif command == "listb" then
    for l, v in pairs(breakpoints) do
      for f in pairs(v) do
        print(f .. ": " .. l)
      end
    end
  elseif command == "listw" then
    for i, v in pairs(watches) do
      print("Watch exp. " .. i .. ": " .. v)
    end
  elseif command == "suspend" then
    client:send("SUSPEND\n")
  elseif command == "stack" then
    local opts = string.match(params, "^[a-z]+%s+(.+)$")
    client:send("STACK" .. (opts and " "..opts or "") .."\n")
    local resp = client:receive()
    local _, _, status, res = string.find(resp, "^(%d+)%s+%w+%s+(.+)%s*$")
    if status == "200" then
      local func, err = loadstring(res)
      if func == nil then
        print("Error in stack information: " .. err)
        return nil, nil, err
      end
      local ok, stack = pcall(func)
      if not ok then
        print("Error in stack information: " .. stack)
        return nil, nil, stack
      end
      for _,frame in ipairs(stack) do
        print(mobdebug.line(frame[1], {comment = false}))
      end
      return stack
    elseif status == "401" then
      local _, _, len = string.find(resp, "%s+(%d+)%s*$")
      len = tonumber(len)
      local res = len > 0 and client:receive(len) or "Invalid stack information."
      print("Error in expression: " .. res)
      return nil, nil, res
    else
      print("Unknown error")
      return nil, nil, "Debugger error: unexpected response after STACK"
    end
  elseif command == "output" then
    local _, _, stream, mode = string.find(params, "^[a-z]+%s+(%w+)%s+([dcr])%s*$")
    if stream and mode then
      client:send("OUTPUT "..stream.." "..mode.."\n")
      local resp, err = client:receive()
      if not resp then
        print("Unknown error: "..err)
        return nil, nil, "Debugger connection error: "..err
      end
      local _, _, status = string.find(resp, "^(%d+)%s+%w+%s*$")
      if status == "200" then
        print("Stream "..stream.." redirected")
        outputs[stream] = type(options) == 'table' and options.handler or nil
      -- the client knows when she is doing, so install the handler
      elseif type(options) == 'table' and options.handler then
        outputs[stream] = options.handler
      else
        print("Unknown error")
        return nil, nil, "Debugger error: can't redirect "..stream
      end
    else
      print("Invalid command")
    end
  elseif command == "basedir" then
    local _, _, dir = string.find(params, "^[a-z]+%s+(.+)$")
    if dir then
      dir = string.gsub(dir, "\\", "/") -- convert slash
      if not string.find(dir, "/$") then dir = dir .. "/" end

      local remdir = dir:match("\t(.+)")
      if remdir then dir = dir:gsub("/?\t.+", "/") end
      basedir = dir

      client:send("BASEDIR "..(remdir or dir).."\n")
      local resp, err = client:receive()
      if not resp then
        print("Unknown error: "..err)
        return nil, nil, "Debugger connection error: "..err
      end
      local _, _, status = string.find(resp, "^(%d+)%s+%w+%s*$")
      if status == "200" then
        print("New base directory is " .. basedir)
      else
        print("Unknown error")
        return nil, nil, "Debugger error: unexpected response after BASEDIR"
      end
    else
      print(basedir)
    end
  elseif command == "help" then
    print("setb <file> <line>    -- sets a breakpoint")
    print("delb <file> <line>    -- removes a breakpoint")
    print("delallb               -- removes all breakpoints")
    print("setw <exp>            -- adds a new watch expression")
    print("delw <index>          -- removes the watch expression at index")
    print("delallw               -- removes all watch expressions")
    print("run                   -- runs until next breakpoint")
    print("step                  -- runs until next line, stepping into function calls")
    print("over                  -- runs until next line, stepping over function calls")
    print("out                   -- runs until line after returning from current function")
    print("listb                 -- lists breakpoints")
    print("listw                 -- lists watch expressions")
    print("eval <exp>            -- evaluates expression on the current context and returns its value")
    print("exec <stmt>           -- executes statement on the current context")
    print("load <file>           -- loads a local file for debugging")
    print("reload                -- restarts the current debugging session")
    print("stack                 -- reports stack trace")
    print("output stdout <d|c|r> -- capture and redirect io stream (default|copy|redirect)")
    print("basedir [<path>]      -- sets the base path of the remote application, or shows the current one")
    print("done                  -- stops the debugger and continues application execution")
    print("exit                  -- exits debugger and the application")
  else
    local _, _, spaces = string.find(params, "^(%s*)$")
    if spaces then
      return nil, nil, "Empty command"
    else
      print("Invalid command")
      return nil, nil, "Invalid command"
    end
  end
  return file, line
end

-- Starts debugging server
-- local function listen(host, port)
--   host = host or "*"
--   port = port or mobdebug.port

--   local socket = require "builtins.scripts.socket"

--   print("Lua Remote Debugger")
--   print("Run the program you wish to debug")

--   local server = socket.bind(host, port)
--   local client = server:accept()

--   client:send("STEP\n")
--   client:receive()

--   local breakpoint = client:receive()
--   local _, _, file, line = string.find(breakpoint, "^202 Paused%s+(.-)%s+(%d+)%s*$")
--   if file and line then
--     print("Paused at file " .. file )
--     print("Type 'help' for commands")
--   else
--     local _, _, size = string.find(breakpoint, "^401 Error in Execution (%d+)%s*$")
--     if size then
--       print("Error in remote application: ")
--       print(client:receive(size))
--     end
--   end

--   while true do
--     io.write("> ")
--     local file, line, err = handle(io.read("*line"), client)
--     if not file and err == false then break end -- completed debugging
--   end

--   client:close()
-- end

local cocreate
local function coro()
  if cocreate then return end -- only set once
  cocreate = cocreate or coroutine.create
  coroutine.create = function(f, ...)
    return cocreate(function(...)
      mobdebug.on()
      return f(...)
    end, ...)
  end
end

local moconew
local function moai()
  if moconew then return end -- only set once
  moconew = moconew or (MOAICoroutine and MOAICoroutine.new)
  if not moconew then return end
  MOAICoroutine.new = function(...)
    local thread = moconew(...)
    -- need to support both thread.run and getmetatable(thread).run, which
    -- was used in earlier MOAI versions
    local mt = thread.run and thread or getmetatable(thread)
    local patched = mt.run
    mt.run = function(self, f, ...)
      return patched(self,  function(...)
        mobdebug.on()
        return f(...)
      end, ...)
    end
    return thread
  end
end

-- make public functions available
mobdebug.setbreakpoint = set_breakpoint
mobdebug.removebreakpoint = remove_breakpoint
mobdebug.listen = listen
mobdebug.loop = loop
mobdebug.scratchpad = scratchpad
mobdebug.handle = handle
mobdebug.connect = connect
mobdebug.start = start
mobdebug.on = on
mobdebug.off = off
mobdebug.moai = moai
mobdebug.coro = coro
mobdebug.done = done
mobdebug.pause = function() step_into = true end
mobdebug.yield = nil -- callback
mobdebug.output = output
mobdebug.onexit = os and os.exit or done
mobdebug.onscratch = nil -- callback
mobdebug.basedir = function(b) if b then basedir = b end return basedir end

mobdebug.setbreakpoint_hook = nil
mobdebug.removebreakpoint_hook = nil
mobdebug.command_hook = nil
mobdebug.on_return_hook = nil
mobdebug.iscasepreserving = iscasepreserving
mobdebug.get_filename = get_filename
mobdebug.get_server_data = get_server_data

return mobdebug
