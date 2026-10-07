-- Bounded timing histogram and sparse snapshot accounting, identical in all modes.
local stats = require "benchmark.stats"
local M = {}
local function phase(self) return self.bench_phase or self.phase end
function M.init(self)
    self.memory_probe = {timing=stats.new(), next_sample=0}
end
function M.update(self)
    local p = self.memory_probe
    local now = sprite._snapshot_clock()
    if phase(self) == "measure" then
        if p.previous then stats.add(p.timing, (now-p.previous)*1000) end
        p.previous = now
        if sys.get_config_int("benchmark.memory_samples", 1) ~= 0 and now >= p.next_sample then
            io.write("WEB_MEMORY " .. json.encode(sprite._get_snapshot_stats()) .. "\n")
            io.flush()
            p.next_sample = now + 2
        end
    end
end
function M.finish(self)
    local p = self.memory_probe
    if p.previous and phase(self) ~= "measure" and not p.reported then
        -- Outside the measured interval: quiet timing runs still verify the
        -- bounded queue and retained capacities without periodic stdout calls.
        io.write("WEB_MEMORY_FINAL " .. json.encode(sprite._get_snapshot_stats()) .. "\n")
        io.flush()
        io.write("WEB_TIMING " .. json.encode(stats.summary(p.timing)) .. "\n")
        io.flush()
        p.reported = true
    end
end
-- Diagnostic runs only. The caller places these boundaries outside its timed
-- window. Engine render/game/GUI scripts share the Lua context in this build.
function M.collect(label)
    if sys.get_config_int("benchmark.memory_diagnostic", 0) == 0 then return end
    local allocated_before, lua_before = sprite._snapshot_memory()
    collectgarbage("collect")
    collectgarbage("collect") -- collect garbage produced by finalizers too
    local allocated_after, lua_after = sprite._snapshot_memory()
    io.write("WEB_MEMORY_GC " .. json.encode({phase=label, allocated_before=allocated_before,
        allocated_after=allocated_after, lua_before=lua_before, lua_after=lua_after}) .. "\n")
    io.flush()
end
return M
