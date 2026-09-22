local bridge = require("modules.bridge")

local hash_single = '#'
local path_double = "/"
local socket_double = "world:/"

local M = {}

function M.init(self)
    bridge.init(self)
end

return M
