local position = vmath.vector3(1, 2, 3)
local normalized = vmath.normalize(position)

---@type vector3
local correlated_vector = normalized

go.animate(
    ".",
    "position",
    go.PLAYBACK_ONCE_FORWARD,
    correlated_vector,
    go.EASING_LINEAR,
    1.0,
    0.0,
    function(self, url, property)
        local _ = self
        ---@type url
        local completed_url = url
        ---@type hash
        local completed_property = property
        pprint(completed_url, completed_property)
    end)

resource.create_texture(
    "/generated.texturec",
    {
        type = graphics.TEXTURE_TYPE_2D,
        width = 16,
        height = 16,
        depth = 1,
        format = graphics.TEXTURE_FORMAT_RGBA,
    })

local attributes = editor.external_file_attributes("README.md")
if attributes.exists and attributes.is_file then
    pprint(attributes.path)
end

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
