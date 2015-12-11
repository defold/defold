module gamesys

require render
require vmath
require gameobject
require io

struct SpriteContext
    render_context    : render.Context
    max_sprite_count  : uint32
    sub_pixels        : uint32
end


struct GetConstantResult
    value             : @vmath.Vector4
    element_ids       : @[4:uint64]
    element_index     : int
end

-- convenience helpers

local tmp_constant:render.Constant = render.Constant { }
local tmp_info:render.MaterialProgramConstantInfo = render.MaterialProgramConstantInfo { }

function get_material_constant(material:render.Material, name_hash:uint64, output:GetConstantResult, callback: *(any, uint64, render.Constant)->(bool), user_data:any):int
    local info = tmp_info
    local res = render.get_material_program_constant_info(material, name_hash, info)
    if not res then
        return gameobject.PROPERTY_RESULT_NOT_FOUND
    end

    for i=0,4 do
        output.element_ids[i] = info.element_ids[i]
    end

    local constant = tmp_constant
    local value:vmath.Vector4 = nil

    if callback(user_data, info.constant_id, constant) then
        output.element_index = info.element_index
        output.value = constant.value
        return gameobject.PROPERTY_RESULT_OK
    end

    render.get_material_program_constant(material, info.constant_id, constant)

    output.value = constant.value

    output.value = constant.value
    output.element_index = info.element_index
    return gameobject.PROPERTY_RESULT_OK
end

function set_material_constant(material:render.Material, name_hash:uint64, value:vmath.Vector4, callback: *(any, uint64, int, vmath.Vector4)->(bool), user_data:any):int
    local info = tmp_info
    local res = render.get_material_program_constant_info(material, name_hash, info)
    if not res then
        return gameobject.PROPERTY_RESULT_NOT_FOUND
    end
    local location = render.get_material_constant_location(material, info.constant_id)
    if location >= 0 then
        if info.constant_id == name_hash then
            callback(user_data, info.constant_id, -1, value)
        else
            callback(user_data, info.constant_id, info.element_index, value)
        end
    end
    return gameobject.PROPERTY_RESULT_OK
end

