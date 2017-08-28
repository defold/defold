local edn      = require '_defold.debugger.edn'
local mobdebug = require '_defold.debugger.mobdebug'

local M = {}

local function identity(v)
  return v
end

function M.start(ips, port)
  mobdebug.line = identity
  mobdebug.dump = edn.encode
  if jit then
    jit.flush()
  end
  mobdebug.coro()

  -- replace original mobdebug.connect with one that raises an error
  -- on error instead of printing
  local original_connect = mobdebug.connect
  mobdebug.connect = function(controller_host, controller_port)
    sock, err = original_connect(controller_host, controller_port)
    -- pprint({sock = sock, err = err})
    if err then
      error(err)
    else
      return sock
    end
  end

  -- try to connect to debugger on each given ip
  local succeeded = false
  for i, ip in ipairs(ips) do
    -- print("Connecting to " .. ip)
    local status, err = pcall(function()
        mobdebug.start(ip, port)
    end)
    if status then
      succeeded = true
      break
    end
  end

  if not succeeded then
    error("Unable to connect to debugger")
  end


  --debug.traceback = function(thread, message, level) 
  --	mobdebug.pause()
  --end
end

return M
