local record Vector
    x: number
    y: number
end

local mt = { __index = Vector }

function Vector.new(x: number, y: number): Vector
    assert(x ~= nil)
    assert(y ~= nil)

    return setmetatable({
        x = x,
        y = y,
    }, mt)
end

function Vector:add(other: Vector): Vector
    return Vector.new(self.x + other.x, self.y + other.y)
end

function Vector:sub(other: Vector): Vector
    return Vector.new(self.x - other.x, self.y - other.y)
end

function Vector:dot(other: Vector): number
    return self.x * other.x + self.y * other.y
end

function Vector:len(): number
    return math.sqrt(Vector.dot(self, self))
end

return Vector
