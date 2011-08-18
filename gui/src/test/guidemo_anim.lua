local nodes = {}

local function call_back2(self, node)
    gui.animate(node, gui.PROP_COLOR, vmath.vector4(1,1,1,0), gui.EASING_IN, 0.8, 0)
end

local function call_back1(self, node)
    local d = 1.6 + math.random() * 0.4
    gui.animate(node, gui.PROP_COLOR, vmath.vector4(1,1,1,1), gui.EASING_IN, 0.6, 0.4, call_back2)
    local to = gui.get_position(node)
    to.y = to.y - 200
    gui.animate(node, gui.PROP_POSITION, to, gui.EASING_IN, 0.2 + d, 0.2)
    gui.animate(node, gui.PROP_ROTATION, vmath.vector3(0, 0, 2*180-2*math.random()*360), gui.EASING_IN, 0.1 + d, 0.2)
end

local function init1(self)
    -- scale is not yet modified by ref-physical-resolution
    local width = gui.get_width()
    local height = gui.get_height()
    local margin = 4
    local nw = (width - margin*9)/8
    local nh = (height - margin*9)/8
    for i=1, 64 do
        local x = (nw + margin) * ((i - 1) % 8) + margin + nw * 0.5
        local y = (nh + margin) * math.floor((i - 1) / 8) + margin + nh * 0.5

        local p = vmath.vector3(x, y-height, 0)
        -- scale is not yet modified by ref-size, so multiply by 7
        local s = vmath.vector3(nw*6, nh*6, 0) * 7
        nodes[i] = gui.new_box_node(p, s)
        local n = nodes[i]
        gui.set_texture(n, "checker")
        gui.set_color(n, vmath.vector4(0, 0, 0, 0))
        local d = math.random() * 0.2
        gui.animate(n, gui.PROP_POSITION, vmath.vector3(x, y, 0), gui.EASING_OUT, 0.65 + d, 0.1, call_back1)
        gui.animate(n, gui.PROP_SIZE, vmath.vector3(nw, nh, 0) * 7, gui.EASING_OUT, 0.65 + d, 0.1)
        gui.animate(n, gui.PROP_COLOR, vmath.vector4(math.random(), math.random(), math.random(),1), gui.EASING_OUT, 0.65 + d, 0.1)
    end
end

function rotate_cb(node)
    to = gui.get_rotation(node)
    if (node == nodes[0]) then
        to.z = to.z + 45
    elseif (node == nodes[1]) then
        to.z = to.z - 45
    elseif (node == nodes[2]) then
        to.z = to.z + 45
    end

    gui.animate(node, gui.PROP_ROTATION, to, gui.EASING_NONE, 30/60, 0, rotate_cb)
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

function update(self)
end
