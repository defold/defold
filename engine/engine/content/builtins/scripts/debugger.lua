-- Copyright 2020-2022 The Defold Foundation
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

local edn      = require 'builtins.scripts.edn'
local mobdebug = require 'builtins.scripts.mobdebug'

local M = {}

local function identity(v)
  return v
end

local function onexit(code, close)
  msg.post("@system:", "exit", {code = code})
end

-- Lightweight hook layer

local mobdebug_hook
local mobdebug_on
local breakpoints = {}
local coroutines = {}; setmetatable(coroutines, {__mode = "k"}) -- "weak" keys
local continue_execution = true
local exit_current_lua_stack = true
local last_co = nil
local stack_after_suspend = 0

local function set_breakpoint(line, file)
  -- Mobdebug save breakpoints in [line][file] table
  -- but for our usecase it's better to have [file][line] table
  if not breakpoints[file] then
    breakpoints[file] = {}
  end
  breakpoints[file][line] = true
end

local function restore_mobdebug_hook(co)
  -- Restore mobdebug hook for line-by-line hook
  if co then
    debug.sethook(co, mobdebug_hook, "lcr")
  else
    debug.sethook(mobdebug_hook, "lcr")
  end
end

local function remove_breakpoint(line, file)
  if breakpoints[file] then
    breakpoints[file][line] = nil
  end
  if breakpoints[file] and not next(breakpoints[file]) then
    breakpoints[file] = nil
  end
end

-- This hook fires only on Lua functions calls and checks if this function has lines
-- we have breakpoints on
local function lightweight_hook()
  local caller = debug.getinfo(2,"S")
  local file = mobdebug.get_filename(caller.source)
  mobdebug.get_server_data(file)
  local brk = breakpoints[file]
  local co = coroutine.running()
  local in_co = false
  if co then
    -- If it's the first call after the [C] call - save function metricks
    if caller.lastlinedefined ~= -1 and coroutines[co] == false then
        -- Coroutine provide this info only on the first coroutine fn call
        -- we should save it for later usage
        coroutines[co] = {
          ["lines"] = debug.getinfo(2,"L"),
          ["file"] = file,
          ["caller"] = caller
        }
    end
    -- Make sure [C] function call was before the first coroutine call
    if not coroutines[co] and caller.lastlinedefined == -1 then
      coroutines[co] = false
    end
    if not brk then
      -- If it's a coroutine we should use pre-saved data
      -- because resuming of the coroutine doesn't profide needed information 
      brk = coroutines[co] and breakpoints[coroutines[co].file]
      in_co = true
    end
  end
  if brk then
    local lines = nil
    if not in_co then
      lines = debug.getinfo(2,"L")
    else
      lines = coroutines[co].lines
    end
    if lines.activelines then
      -- If this function has breakpoints then restore mobdebug callback
      for k, _ in pairs(lines.activelines) do
        if brk[k] then
          restore_mobdebug_hook(co)
          return
        end
      end
    end
  end
end

local function set_lightweight_hook(force, co)
  -- Lightweight hook may be set only if Lue stack exited and 
  -- user click Continue button in the Editor
  if (continue_execution and exit_current_lua_stack) or force then
    exit_current_lua_stack = false
    continue_execution = false
    if co then
      debug.sethook(co, nil, "lcr")
      debug.sethook(co, lightweight_hook, "c")
    else
      debug.sethook(nil, "lcr")
      debug.sethook(lightweight_hook, "c")
    end
  end
end

local function on_return_hook()
  -- Any function on return has deepness at least 3
  -- if there is no 4th element in the stack
  -- that means this is return from on of the engine's lifecycle functions
    if not debug.getinfo(4, "l") then
      exit_current_lua_stack = true
      set_lightweight_hook()
    end
end

local function command_hook(command)
  -- Listen commands to know when user click Continue button
  if command == "RUN" then
    continue_execution = true
    set_lightweight_hook()
    stack_after_suspend = 0
  elseif command == "SUSPEND" then
    if stack_after_suspend == 0 then
      stack_after_suspend = 1
    end
  elseif command == "STACK" then
    if stack_after_suspend == 1 then
      stack_after_suspend = 0
      restore_mobdebug_hook()
    end
  else
    stack_after_suspend = 0
  end
end

-- this hook under the mobdebug's hook is needed to setup lightweight hooks for
-- coroutines instead of the default mobdebug hook 
local function on()
  if mobdebug_on() then
    local co, main = coroutine.running()
    if co and not main then
      set_lightweight_hook(true, co)
    end
  end
end

-- end lightweight hook layer

function M.start(port)
  mobdebug.line = identity
  mobdebug.dump = edn.encode

  -- setbreakpoint_hook and removebreakpoint_hook
  -- should be added before starting server listening
  mobdebug.setbreakpoint_hook = set_breakpoint
  mobdebug.removebreakpoint_hook = remove_breakpoint

  -- mobdebug.onexit does os.exit which does not
  -- let the engine shut down properly. One step
  -- skipped is SSDP Deannounce, which makes the
  -- target linger longer than necessary.
  mobdebug.onexit = onexit
  if jit then
    jit.flush()
  end

  -- replace mobdebug hook with lightweight hook for coroutines
  mobdebug_on = mobdebug.on
  mobdebug.on = on
  mobdebug.coro()

  -- TODO pcall?
  mobdebug.listen(port)

  -- for future "break on error" functionality
  --debug.traceback = function(thread, message, level)
  --	mobdebug.pause()
  --end

  -- set up our lightweight hook
  -- command_hook should be set after starting server listening
  mobdebug.command_hook = command_hook
  mobdebug.on_return_hook = on_return_hook
  mobdebug_hook = debug.gethook()
  set_lightweight_hook(true)
end

return M
