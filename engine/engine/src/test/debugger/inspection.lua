-- Copyright 2020-2026 The Defold Foundation
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

local M = {}

-- The DAP client edits these fields before allowing the callback to continue.
local function inspect(self)
    self.score = 41
    self.items = {"one"}
    self.cycle = self
    self.action = function() end
    local calls = 0
    local yielded = sys.get_config_int("test.debugger_yielded", 0) == 1
    local function run(self)
        local opaque = newproxy(true)
        local mt = getmetatable(opaque)
        mt.__index = function() calls = calls + 1 end
        mt.__tostring = function() calls = calls + 1; return "opaque" end
        mt.__get_instance_data_table_ref = function() calls = calls + 1 end
        if yielded then
            coroutine.yield()
        end
        self.score = self.score -- inspect
        assert(self.score == 99 and self.items[1] == "two" and self.label == "done")
        assert(self.cycle == self and opaque ~= nil and calls == 0)
    end
    if yielded then
        local co = coroutine.create(run)
        assert(coroutine.resume(co, self))
        local marker = 1 -- suspended
        assert(coroutine.resume(co))
    else
        run(self)
    end
    assert(calls == 0)
end

local function replaced_getter(self)
    self.score = 41
    local calls = 0
    local mt = debug.getmetatable(self)
    local getter = mt.__get_instance_data_table_ref
    local replacement = function() calls = calls + 1; return getter(self) end
    local marker = 1 -- getter
    assert(calls == 0 and self.score == 41 and marker == 1)
end

-- Issue #7750 needs a real engine reboot, not just a DAP disconnect/reconnect.
-- Keep updating while detached so the client can attach to each fresh runtime.
local function reboot(self)
    self.score = 41
    local generation = sys.get_config_int("test.debugger_generation", 0)
    local port = debugger.start()
    timer.delay(0.01, true, function()
        local score = self.score -- reboot-inspect
        if self.action then
            assert(score == 99)
            if self.action == "reboot" then
                -- Reboot replaces argv; the engine requires the project last.
                sys.reboot("--config=test.debugger_generation=" .. (generation + 1),
                    "--config=debugger.port=" .. port,
                    sys.get_config_string("test.debugger_project"))
            else
                sys.exit(0)
            end
        end
    end)
end

function M.run(self, kind)
    if sys.get_config_string("test.debugger_instance") ~= kind then
        return
    end
    local case = sys.get_config_string("test.debugger_case")
    local test = inspect
    if case == "getter" then
        test = replaced_getter
    elseif case == "reboot" then
        test = reboot
    end
    local ok, message = pcall(test, self)
    if not ok then
        print(message)
    end
    if not ok or case ~= "reboot" then
        sys.exit(ok and 0 or 1)
    end
end

return M
