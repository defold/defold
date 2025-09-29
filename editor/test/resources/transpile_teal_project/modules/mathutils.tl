local Vector = require('modules.vector')

local m = {}

function m.distance(a: Vector, b: Vector): number
    return Vector.sub(b, a):len()
end

return m
