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
