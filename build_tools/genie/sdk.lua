
local M = {}

local run = require('run')
local json = require('json')

local DYNAMO_HOME = os.getenv("DYNAMO_HOME")
--TODO: replace with DEFOLD_HOME

local function read_file(path)
    local f = assert(io.open(path, "rb"))
    local content = f:read("*all")
    f:close()
    return content
end

function M.get_sdk_info(target_platform)
    local output = '_sdkinfo.json'
    os.remove(output)
    local r = run.command(string.format("python %s/../../build_tools/sdk.py %s %s", DYNAMO_HOME, target_platform, output), true)
    local data = read_file(output)
    os.remove(output)
    return data
end

return M
