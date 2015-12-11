module vmath

require math
require io

!align(16)
struct Vector3
    x:float
    y:float
    z:float
    padding:float
end

!align(16)
struct Vector4
    x:float
    y:float
    z:float
    w:float
end

!align(16)
struct Point3
    x:float
    y:float
    z:float
    padding:float
end

!align(16)
struct Quat
    x:float
    y:float
    z:float
    w:float
end

struct Transform
    rotation    : Quat
    translation : Point3
    scale       : Vector3
end

struct Matrix4
    d       : @[16:float] -- col0,col1,col2,col3
end

function length_sqr(x:float, y:float, z:float):float
    return x*x + y*y + z*z
end

function length_sqr(x:float, y:float, z:float, w:float):float
    return x*x + y*y + z*z + w*w
end

function normalize_z_scale(mtx:Matrix4)
    local mag_sqr = length_sqr(mtx.d[8], mtx.d[9], mtx.d[10], mtx.d[11]);
    if mag_sqr > 0.0f then
        local rec = 1.0f / math.sqrt(mag_sqr)
        mtx.d[8] = rec * mtx.d[8]
        mtx.d[9] = rec * mtx.d[9]
        mtx.d[10] = rec * mtx.d[10]
        mtx.d[11] = rec * mtx.d[11]
    end
end

-- src and out may be the same
function append_scale(out:Matrix4, src:Matrix4, scale:Vector3)
    out.d[0] = scale.x * src.d[0]
    out.d[1] = scale.x * src.d[1]
    out.d[2] = scale.x * src.d[2]
    out.d[3] = scale.x * src.d[3]

    out.d[4] = scale.y * src.d[4]
    out.d[5] = scale.y * src.d[5]
    out.d[6] = scale.y * src.d[6]
    out.d[7] = scale.y * src.d[7]

    out.d[8] = scale.z * src.d[8]
    out.d[9] = scale.z * src.d[9]
    out.d[10] = scale.z * src.d[10]
    out.d[11] = scale.z * src.d[11]

    out.d[12] = src.d[12]
    out.d[13] = src.d[13]
    out.d[14] = src.d[14]
    out.d[15] = src.d[15]
end

function diagonal(out:Matrix4, x:float, y:float, z:float, w:float):Matrix4
    out.d[0] = x
    out.d[1] = 0f
    out.d[2] = 0f
    out.d[3] = 0f
    out.d[4] = 0f
    out.d[5] = y
    out.d[6] = 0f
    out.d[7] = 0f
    out.d[8] = 0f
    out.d[9] = 0f
    out.d[10] = z
    out.d[11] = 0f
    out.d[12] = 0f
    out.d[13] = 0f
    out.d[14] = 0f
    out.d[15] = w
end

function identity(out:Matrix4):Matrix4
    diagonal(out, 1f, 1f, 1f, 1f)
end

function scaling(out:Matrix4, x:float, y:float, z:float):Matrix4
    diagonal(out, x, y , z, 1f)
end

function translation(out:Matrix4, x:float, y:float, z:float):Matrix4
    out.d[0] = 1f
    out.d[1] = 0f
    out.d[2] = 0f
    out.d[3] = 0f
    out.d[4] = 0f
    out.d[5] = 1f
    out.d[6] = 0f
    out.d[7] = 0f
    out.d[8] = 0f
    out.d[9] = 0f
    out.d[10] = 1f
    out.d[11] = 0f
    out.d[12] = x
    out.d[13] = y
    out.d[14] = z
    out.d[15] = 1f
end

function multiply(out:Vector4, mtx:Matrix4, vec:Vector4)
    local x = vec.x
    local y = vec.y
    local z = vec.z
    local w = vec.w
    local d = mtx.d
    out.x = d[0]*x + d[4]*y + d[8]*z + d[12]*w
    out.y = d[1]*x + d[5]*y + d[9]*z + d[13]*w
    out.z = d[2]*x + d[6]*y + d[10]*z + d[14]*w
    out.w = d[3]*x + d[7]*y + d[11]*z + d[15]*w
end

function multiply(out:Matrix4, l:Matrix4, r:Matrix4)
    local d = r.d
    local c0 = Vector4 { }
    local c1 = Vector4 { }
    local c2 = Vector4 { }
    local c3 = Vector4 { }
    multiply(c0, l, Vector4 { x = d[0], y = d[1], z = d[2], w = d[3] })
    multiply(c1, l, Vector4 { x = d[4], y = d[5], z = d[6], w = d[7] })
    multiply(c2, l, Vector4 { x = d[8], y = d[9], z = d[10], w = d[11] })
    multiply(c3, l, Vector4 { x = d[12], y = d[13], z = d[14], w = d[15] })
    out.d[0] = c0.x
    out.d[1] = c0.y
    out.d[2] = c0.z
    out.d[3] = c0.w
    out.d[4] = c1.x
    out.d[5] = c1.y
    out.d[6] = c1.z
    out.d[7] = c1.w
    out.d[8] = c2.x
    out.d[9] = c2.y
    out.d[10] = c2.z
    out.d[11] = c2.w
    out.d[12] = c3.x
    out.d[13] = c3.y
    out.d[14] = c3.z
    out.d[15] = c3.w
end

function multiply_no_z_scale(out:Matrix4, m1:Matrix4, m2:Matrix4)
    local tmp = Matrix4 { }
    for i=0,16 do
        tmp.d[i] = m1.d[i]
    end

    normalize_z_scale(tmp)

    local m2c3 = Vector4 { x = m2.d[12], y = m2.d[13], z = m2.d[14], w = m2.d[15] }
    local m2c3_2 = Vector4 { }

    multiply(m2c3_2, tmp, m2c3)
    multiply(out, m1, m2)

    out.d[12] = m2c3_2.x
    out.d[13] = m2c3_2.y
    out.d[14] = m2c3_2.z
    out.d[15] = m2c3_2.w
end

!symbol("SolQuatToMatrix")
extern quat_to_matrix(out:[16:float], x:float, y:float, z:float, w:float)

function transform_to_matrix(out:Matrix4, t:Transform)
    quat_to_matrix(out.d, t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w)
    out.d[12] = t.translation.x
    out.d[13] = t.translation.y
    out.d[14] = t.translation.z
    append_scale(out, out, t.scale)
end

-- Printing

function print(v:Vector4)
    io.println("== Vector4 = [" .. v.x .. ", " .. v.y .. ", " .. v.z .. ", " .. v.w .. "]")
end

function print(v:Vector3)
    io.println("== Vector3 = [" .. v.x .. ", " .. v.y .. ", " .. v.z .. "]")
end

function print(v:Point3)
    io.println("== Point3 = [" .. v.x .. ", " .. v.y .. ", " .. v.z .. "]")
end

function print(q:Quat)
    io.println("== Quat = [" .. q.x .. ", " .. q.y .. ", " .. q.z .. ", " .. q.w .. "]")
end

function print(m:Matrix4)
    io.println("== Matrix4 ==")
    for i=0,4 do
        io.println("[" .. m.d[0+i] .. ", " .. m.d[4+i] .. ", " .. m.d[8+i] .. ", " .. m.d[12+i] .. "]")
    end
    io.println("=============")
end
