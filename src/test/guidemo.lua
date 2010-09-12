local nodes = {}

local function call_back2(node)
    animate(node, COLOR, get_color(node), {0,0,0,0}, EASING_IN, 0.8, 0)
end

local function call_back1(node)
    local d = 1.3 + math.random() * 0.4
    animate(node, COLOR, get_color(node), {1,1,1,0}, EASING_IN, 0.6, 0.5, call_back2)
    local from = get_position(node)
    local to = get_position(node)
    to[2] = to[2] - 2
    animate(node, POSITION, from, to, EASING_OUT, 0.2 + d, 0.2)
    animate(node, ROTATION, {0,0,0}, {0,0,math.random()*360}, EASING_OUT, 0.2 + d, 0.2)
end

function init(self)
    local w = 0.12
    local h = 0.12
    local gap = 0.01
    for i=0, 63 do
        local x = (1-(w+gap)) * math.floor(i % 8) / 7.0 + w/2 + gap/2
        local y = (1-(h+gap)) * math.floor(i / 8) / 7.0 + h/2 + gap/2

        local dx = 0.5 - x
        local dy = 0.5 - y
        local dx = -dx
        local dy = -dy

--        nodes[i] = new_box_node({0.5 + 2 * math.cos(6.24 * i/63), 0.5 + 2 * math.sin(6.24 * i/63), 0}, {w, h, 0})
        nodes[i] = new_box_node({0.5 + 6 * dx, 0.5 + 6 * dy, 0}, {w, h, 0})
        nodes[i].texture = "checker"
        set_color(nodes[i], {0,0,0,0})
        local d = math.random() * 0.2
        animate(nodes[i], POSITION, {0.5, 0.5, 0}, {x, y, 0}, EASING_IN, 0.65 + d, 0.5, call_back1)
        animate(nodes[i], COLOR, {0, 0, 0, 0}, {math.random(), math.random(), math.random(),1}, EASING_IN, 0.65 + d, 0.5)
    end
end

function update(self)
end
