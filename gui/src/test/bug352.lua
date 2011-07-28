local state = {}
state.nodes = {}
state.score = 0
state.target_score = 0

function init(self)
    local n = gui.new_text_node(vmath.vector3(15, 60, 0), "" .. state.score)
    gui.set_font(n, "big_score")
    state.nodes.score = n
end

function update(self, dt)
end

local function fade_done(self, node)
    gui.delete_node(node)
end

function on_message(self, message_id, message)
    state.target_score = state.target_score + message.score
    local p = vmath.vector3(0, 0, 0)
    p.y = 640 - p.y
    local n = gui.new_text_node(p, tostring(message.score))
    gui.set_font(n, "score")
    local start_color = vmath.vector4(1, 0, 0, 0.9)
    local end_color = vmath.vector4(1, 0, 0, 0.0)
    gui.set_color(n, start_color)
    gui.animate(n, gui.PROP_COLOR, end_color, gui.EASING_NONE, 1, 0, fade_done)
end
