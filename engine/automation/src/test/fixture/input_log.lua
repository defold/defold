local M = {}
local actions = {
    [hash("space")] = "space",
    [hash("click")] = "pointer",
    [hash("touch")] = "touch",
    [hash("text")] = "text",
}
local sources = {}
for _, name in ipairs({"keyboard", "text", "mouse", "touch", "gamepad", "accelerometer"}) do
    sources[hash(name)] = name
end

function M.record(self, action_id, action)
    local input = {
        action = action_id and (actions[action_id] or "other") or (action.source == hash("accelerometer") and "accelerometer" or "move"),
        source = sources[action.source] or "unknown",
        pressed = action.pressed,
        released = action.released,
        frame = self.frame,
        x = action.x,
        y = action.y,
        text = action.text,
    }
    if #self.inputs == 256 then table.remove(self.inputs, 1) end
    table.insert(self.inputs, input)
    return input
end

return M
