local Nodes = {}

function cb2(node)
    Animate(node, COLOR, GetColor(node), {0,0,0,0}, EASING_IN, 0.8, 0)
end

function cb(node)
    local d = 1.2 + math.random() * 0.4
    Animate(node, COLOR, GetColor(node), {1,1,1,0}, EASING_IN, 0.6, 0.5, cb2)
    local from = GetPosition(node)
    local to = GetPosition(node)
    to[2] = to[2] - 2
    Animate(node, POSITION, from, to, EASING_OUT, 0.2 + d, 0.2)
end

function Init(self)
    local w = 0.12
    local h = 0.12
    local gap = 0.01
    for i=0, 63 do
        local x = (1-(w+gap)) * math.floor(i % 8) / 7.0 + w/2
        local y = (1-(h+gap)) * math.floor(i / 8) / 7.0 + h/2
        Nodes[i] = NewBoxNode({0.5, 2, 0}, {w, h, 0})
        SetColor(Nodes[i], {0,0,0,0})
        local d = math.random() * 0.2
        Animate(Nodes[i], POSITION, {0.5, 0.5, 0}, {x, y, 0}, EASING_IN, 0.65 + d, 2, cb)
        Animate(Nodes[i], COLOR, {0,0,0,0}, {math.random(),math.random(),math.random(),1}, EASING_INOUT, 0.65 + d, 2)
    end
end

function Update(self)
end
