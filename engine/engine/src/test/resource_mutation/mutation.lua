local M = { stages = {} }

local function texture_params(width)
    return { width = width, height = 1, type = graphics.TEXTURE_TYPE_2D,
             format = graphics.TEXTURE_FORMAT_RGBA }
end

local function pixels(width)
    return buffer.create(width, {{name = hash("rgba"), type = buffer.VALUE_TYPE_UINT8, count = 4}})
end

function M.init(buffer_path)
    M.buffer_path = buffer_path
    M.buffer_bytes = resource.load("/buffer/dummy.bufferc")
    M.texture_path = resource.create_texture("/mutation.texturec", texture_params(1), pixels(1))
end

-- Each call checks immediate read-after-write, not just whether the setter accepted the call.
function M.check(stage)
    local value = #M.stages + 2

    resource.set(M.buffer_path, M.buffer_bytes)
    assert(buffer.get_stream(resource.get_buffer(M.buffer_path), "str")[1] == 101)

    local replacement = buffer.create(1, {{name = hash("str"), type = buffer.VALUE_TYPE_UINT8, count = 1}})
    buffer.get_stream(replacement, "str")[1] = value
    resource.set_buffer(M.buffer_path, replacement, {transfer_ownership = true})
    assert(buffer.get_stream(resource.get_buffer(M.buffer_path), "str")[1] == value)

    resource.set_texture(M.texture_path, texture_params(value), pixels(value))
    assert(resource.get_texture_info(M.texture_path).width == value)

    M.stages[#M.stages + 1] = stage
end

function M.verify()
    assert(table.concat(M.stages, ",") ==
        "init,update,pre_render,render_message,render_update,final,post_update_message,post_render")
end

return M
