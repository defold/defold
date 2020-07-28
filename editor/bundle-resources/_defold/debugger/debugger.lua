-- Copyright 2020 The Defold Foundation
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

local edn      = require '_defold.debugger.edn'
local mobdebug = require '_defold.debugger.mobdebug'

local M = {}

local function identity(v)
  return v
end

local function onexit(code, close)
  msg.post("@system:", "exit", {code = code})
end

function M.start(port)
  mobdebug.line = identity
  mobdebug.dump = edn.encode
  -- mobdebug.onexit does os.exit which does not
  -- let the engine shut down properly. One step
  -- skipped is SSDP Deannounce, which makes the
  -- target linger longer than necessary.
  mobdebug.onexit = onexit
  if jit then
    jit.flush()
  end
  mobdebug.coro()

  -- TODO pcall?
  mobdebug.listen(port)

  -- for future "break on error" functionality
  --debug.traceback = function(thread, message, level) 
  --	mobdebug.pause()
  --end
end

return M
