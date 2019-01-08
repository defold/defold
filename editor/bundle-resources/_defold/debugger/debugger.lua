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
