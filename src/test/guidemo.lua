local nodes = {}

local function call_back2(node)
    animate(node, COLOR, {1,1,1,0}, EASING_OUT, 0.8, 0)
end

local function call_back1(node)
    local d = 1.6 + math.random() * 0.4
    animate(node, COLOR, {1,1,1,1}, EASING_IN, 0.6, 0.4, call_back2)
    local to = get_position(node)
    to[2] = to[2] - 2
    animate(node, POSITION, to, EASING_OUT, 0.2 + d, 0.2)
    animate(node, ROTATION, {0, 0, 2*180-2*math.random()*360}, EASING_OUT, 0.1 + d, 0.2)
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

--        nodes[i] = new_box_node({0.5 + 2 * math.cos(6.24 * (i-1)/63), 0.5 + 2 * math.sin(6.24 * (i-1)/63), 0}, {w, h, 0})
        nodes[i] = new_box_node({0.5 + 8 * dx, 0.5 + 8 * dy, 0}, {w*6, h*6, 0})
        local n = nodes[i]
        n.texture = "checker"
        set_color(n, {0, 0, 0, 0})
        local d = math.random() * 0.2
        animate(n, POSITION, {x, y, 0}, EASING_IN, 0.65 + d, 0.1, call_back1)
        animate(n, EXTENTS, {w, h, 0}, EASING_IN, 0.65 + d, 0.1)
        animate(n, COLOR, {math.random(), math.random(), math.random(),1}, EASING_IN, 0.65 + d, 0.1)
    end
end

function rotate_cb(node)
    to = get_rotation(node)
    if (node == nodes[0]) then
        to[3] = to[3] + 45
    elseif (node == nodes[1]) then
        to[3] = to[3] - 45
    elseif (node == nodes[2]) then
        to[3] = to[3] + 45
    end

    animate(node, ROTATION, to, EASING_NONE, 30/60, 0, rotate_cb)
end

function on_input(self, input_action)
    for i, n in ipairs(nodes) do
        delete_node(n)
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

        nodes[i] = new_box_node({x, y, 0}, {w, h, 0})
        set_rotation(nodes[i], {0, 0, i * 120})
        rotate_cb(nodes[i])
        nodes[i].blend_mode = BLEND_MODE_ADD

        if (i == 0) then
            set_color(nodes[i], {1, 0, 0, 1})
        elseif (i == 1) then
            set_color(nodes[i], {0, 1, 0, 1})
        elseif (i == 2) then
            set_color(nodes[i], {0, 0, 1, 1})
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
