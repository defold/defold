local nodes = {}
local rotation = 30

function rotate_start(self, n)
    gui.animate(n, gui.PROP_ROTATION, vmath.vector4(0, 0, rotation, 0), gui.EASING_INOUT, 0.4, 0, rotate_end)
end

function rotate_end(self, n)
    gui.animate(n, gui.PROP_ROTATION, vmath.vector4(0, 0, -rotation, 0), gui.EASING_INOUT, 0.4, 0, rotate_start)
end

local function init1(self)
    local w = 140
    local h = 140
    local xs = {20, 50, 80}
    local ys = {80, 50, 20}
    local pivots = {gui.PIVOT_NW, gui.PIVOT_N, gui.PIVOT_NE, gui.PIVOT_W, gui.PIVOT_CENTER, gui.PIVOT_E, gui.PIVOT_SW, gui.PIVOT_S, gui.PIVOT_SE}
    for y=1,3 do
        for x=1,3 do
            i = x + (y-1) * 3
            local p = vmath.vector3(xs[x], ys[y], 0)
            local s = vmath.vector3(w, h, 0)
            nodes[i] = gui.new_box_node(p, s)
            local n = nodes[i]
            gui.set_texture(n, "checker")
            local d = i/9
            gui.set_color(n, vmath.vector4(d, d, d, 1))
            gui.set_pivot(n, pivots[i])
            rotate_start(self, n)
        end
    end
end

function on_input(self, action_id, action)
    for i, n in ipairs(nodes) do
        gui.delete_node(n)
    end
    nodes = {}
    init1(self)
end

function init(self)
    init1()
end
