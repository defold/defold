local position = vmath.vector3(1, 2, 3)
local normalized = vmath.normalize(position)

---@type vector3
local correlated_vector = normalized

---@type vector3
local divided_vector3 = position / 2

---@type vector4
local divided_vector4 = vmath.vector4(1, 2, 3, 4) / 2

local transform = vmath.matrix4_translation(position)

---@type matrix4
local combined_transform = transform * vmath.matrix4()

---@type vector4
local transformed_vector = transform * vmath.vector4(1, 2, 3, 1)

---@type matrix4
local scaled_transform = transform * 2

---@type matrix4
local reverse_scaled_transform = 2 * transform

---@type quaternion
local combined_rotation = vmath.quat() * vmath.quat_rotation_z(1)

go.set_rotation(combined_rotation)
pprint(
    divided_vector3,
    divided_vector4,
    combined_transform,
    transformed_vector,
    scaled_transform,
    reverse_scaled_transform)

go.animate(
    ".",
    "position",
    go.PLAYBACK_ONCE_FORWARD,
    correlated_vector,
    go.EASING_LINEAR,
    1.0,
    0.0,
    function(self, url, property)
        self.animation_completed = true
        ---@type url
        local completed_url = url
        ---@type hash
        local completed_property = property
        pprint(completed_url, completed_property)
    end)

gui.animate(
    gui.get_node("box"),
    "position",
    position,
    gui.EASING_LINEAR,
    1.0)

resource.create_texture(
    "/generated.texturec",
    {
        type = graphics.TEXTURE_TYPE_2D,
        width = 16,
        height = 16,
        depth = 1,
        format = graphics.TEXTURE_FORMAT_RGBA,
    })

model.play_anim(
    "#model",
    "idle",
    go.PLAYBACK_LOOP_FORWARD,
    {blend_duration = 0.2, playback_rate = 1.0},
    function(self, message_id, message, sender)
        local _ = self
        ---@type hash
        local completed_message = message_id
        ---@type message.model.model_animation_done
        local payload = message
        ---@type url
        local source = sender
        pprint(completed_message, payload.animation_id, source)
    end)

local tcp_client = assert(socket.connect("127.0.0.1", 8000))
tcp_client:settimeout(0.1)
tcp_client:send("ping")

local udp_socket = assert(socket.udp())
udp_socket:settimeout(0.1)
udp_socket:sendto("ping", "127.0.0.1", 8000)

resource.create_atlas(
    "/generated.texturesetc",
    {
        texture = "/main/my_texture.texturec",
        animations = {
            {
                id = "idle",
                width = 16,
                height = 16,
                frames = {1},
                playback = go.PLAYBACK_LOOP_FORWARD,
                fps = 30,
            },
        },
        geometries = {
            {
                vertices = {0, 0, 16, 0, 16, 16},
                uvs = {0, 0, 16, 0, 16, 16},
                indices = {0, 1, 2},
            },
        },
    })

local atlas_data = resource.get_atlas("/generated.texturesetc")
local first_frame = atlas_data.animations[1].frames[1]
pprint(first_frame, atlas_data.geometries[1].pivot_x)

---@param action on_input.action
local function inspect_action(action)
    if action.touch then
        pprint(action.touch[1].screen_x)
    end
    if action.gamepad_guid_info then
        pprint(action.gamepad_guid_info.vendor)
    end
end

inspect_action({pressed = true})

local collision_body = b2d.get_body("#collisionobject")
b2d.body.set_mass_data(
    collision_body,
    {
        mass = 1,
        center = vmath.vector3(),
        inertia = 0,
    })

local physics_world = b2d.get_world()
b2d.world.cast_ray(
    physics_world,
    vmath.vector3(),
    vmath.vector3())

render.clear({
    [graphics.BUFFER_TYPE_COLOR0_BIT] = vmath.vector4(),
    [graphics.BUFFER_TYPE_DEPTH_BIT] = 1,
})
