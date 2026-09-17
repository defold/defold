local shared = require("modules.shared")
go.property("invalid-in-lua-module", 1)

local hash_double = "#"
local path_double = "/pla"
local socket_single = 'world:/pla'
local require_double = require("/pla")

local M = {}

function M.init(self)
    shared.init(self)
end

return M
