go.animate(
    ".",
    "position",
    "not-a-playback-mode",
    vmath.vector3(),
    go.EASING_LINEAR,
    1.0)

go.animate(
    ".",
    "position",
    123456,
    vmath.vector3(),
    go.EASING_LINEAR,
    1.0)

---@param method zip.METHOD
local function use_zip_method(method)
    pprint(method)
end

use_zip_method("not-a-method")

resource.create_texture(
    "/invalid.texturec",
    {
        type = graphics.TEXTURE_TYPE_2D,
        width = "sixteen",
    })

font.prewarm_text("/invalid.fontc", "text", 42)

local tcp_client = assert(socket.connect("127.0.0.1", 8000))
tcp_client:send(42)

go.set_rotation(vmath.vector4())

---@type string
local invalid_vector_division = vmath.vector3() / 2

---@type string
local invalid_matrix_product = vmath.matrix4() * vmath.matrix4()

---@type string
local invalid_transformed_vector = vmath.matrix4() * vmath.vector4()

---@type string
local invalid_quaternion_product = vmath.quat() * vmath.quat()

pprint(
    invalid_vector_division,
    invalid_matrix_product,
    invalid_transformed_vector,
    invalid_quaternion_product)
