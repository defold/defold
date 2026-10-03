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
        if now >= p.next_sample then
            io.write("WEB_MEMORY " .. json.encode(sprite._get_snapshot_stats()) .. "\n")
            io.flush()
            p.next_sample = now + 2
        end
    end
end
function M.finish(self)
    local p = self.memory_probe
    if p.previous and phase(self) ~= "measure" and not p.reported then
        io.write("WEB_TIMING " .. json.encode(stats.summary(p.timing)) .. "\n")
        io.flush()
        p.reported = true
    end
end
return M
