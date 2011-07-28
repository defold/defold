local nodes = {}

local function call_back2(node)
    gui.animate(node, gui.PROP_COLOR, vmath.vector4(1,1,1,0), gui.EASING_OUT, 0.8, 0)
end

local function call_back1(node)
    local d = 1.6 + math.random() * 0.4
    gui.animate(node, gui.PROP_COLOR, vmath.vector4(1,1,1,1), gui.EASING_IN, 0.6, 0.4, call_back2)
    local to = gui.get_position(node)
    to.y = to.y - 2
    gui.animate(node, gui.PROP_POSITION, to, gui.EASING_OUT, 0.2 + d, 0.2)
    gui.animate(node, gui.PROP_ROTATION, vmath.vector3(0, 0, 2*180-2*math.random()*360), gui.EASING_OUT, 0.1 + d, 0.2)
end

local function init1(self)
    local w = 0.12
    local h = 0.12
    local gap = 0.01
    for i=1, 64 do
        local x = (1-(w+gap)) * math.floor((i-1) % 8) / 7.0 + w/2 + gap/2
        local y = (1-(h+gap)) * math.floor((i-1) / 8) / 7.0 + h/2 + gap/2

        local dx = x - 0.5
        local dy = y - 0.8

--        nodes[i] = gui.new_box_node({0.5 + 2 * math.cos(6.24 * (i-1)/63), 0.5 + 2 * math.sin(6.24 * (i-1)/63), 0}, {w, h, 0})
        nodes[i] = gui.new_box_node(vmath.vector3(0.5 + 8 * dx, 0.5 + 8 * dy, 0), vmath.vector3(w*6, h*6, 0))
        local n = nodes[i]
        gui.set_texture(n, "checker")
        gui.set_color(n, vmath.vector4(0, 0, 0, 0))
        local d = math.random() * 0.2
        gui.animate(n, gui.PROP_POSITION, vmath.vector3(x, y, 0), gui.EASING_IN, 0.65 + d, 0.1, call_back1)
        gui.animate(n, gui.PROP_EXTENTS, vmath.vector3(w, h, 0), gui.EASING_IN, 0.65 + d, 0.1)
        gui.animate(n, gui.PROP_COLOR, vmath.vector4(math.random(), math.random(), math.random(),1), gui.EASING_IN, 0.65 + d, 0.1)
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

function init2(self)
    local w = 0.6
    local h = 0.12
    local gap = 0.01
    for i=0, 2 do
        local x = 0.5
        local y = 0.5

        nodes[i] = gui.new_box_node(vmath.vector3(x, y, 0), vmath.vector3(w, h, 0))
        gui.set_rotation(nodes[i], vmath.vector3(0, 0, i * 120))
        rotate_cb(nodes[i])
        gui.set_blend_mode(nodes[i], gui.BLEND_ADD)

        if (i == 0) then
            gui.set_color(nodes[i], vmath.vector4(1, 0, 0, 1))
        elseif (i == 1) then
            gui.set_color(nodes[i], vmath.vector4(0, 1, 0, 1))
        elseif (i == 2) then
            gui.set_color(nodes[i], vmath.vector4(0, 0, 1, 1))
        end
    end
end

function init(self)
    init1()
    --init2()
end

function update(self)
    --print(get_extents(nodes[0])[1])
end
