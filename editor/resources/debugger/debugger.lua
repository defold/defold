local edn      = require '_defold.debugger.edn'
local mobdebug = require '_defold.debugger.mobdebug'

local M = {}

local function identity(v)
  return v
end

function M.start(port)
  mobdebug.line = identity
  mobdebug.dump = edn.encode
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
