module res_sprite

require io
require math
require reflect
require resource
require render
require ddf
require hash
require array

require sprite_ddf
require texture_set_ddf
require res_textureset

struct SpriteResource
    path                   : String
    desc                   : sprite_ddf.SpriteDesc
    texture_set            : res_textureset.TextureSetResource
    unpacked_uvs           : [float]
    material               : render.Material
    default_animation      : int
end

function acquire_resources(factory:resource.Factory, sprite:SpriteResource):int
    local tile_set = resource.ResourceDescriptor { }
    local res0:int = resource.get(factory, sprite.desc.tile_set, tile_set)
    if res0 ~= 0 then
        io.println("SOL: Failed to load texture set " .. sprite.desc.tile_set .. " error " .. res0)
        return res0
    else
        sprite.texture_set = tile_set.resource as res_textureset.TextureSetResource
        -- Unpack from byte array to float array
        local coords = sprite.texture_set.texture_set.tex_coords
        sprite.unpacked_uvs = [#coords/4:float]
        array.array_copy(sprite.unpacked_uvs, coords)
    end

    local material = resource.ResourceDescriptor { }
    local res1:int = resource.get(factory, sprite.desc.material, material)
    if res1 ~= 0 then
        io.println("SOL: Failed to load material " .. sprite.desc.material)
        return res1
    else
        sprite.material = material.resource as render.Material
    end

    sprite.default_animation = res_textureset.get_animation_by_id(sprite.texture_set, hash.hash_string64(sprite.desc.default_animation))
    if sprite.default_animation < 0 then
        io.println("SOL: Default animation not found [" .. sprite.desc.default_animation .. "]")
        return resource.RESULT_FORMAT_ERROR
    end

    return resource.RESULT_OK
end

function release_resources(factory:resource.Factory, sprite:SpriteResource)
    if not nil(sprite.material) then
        resource.release(factory, sprite.material)
        sprite.material = nil
    end
    if not nil(sprite.texture_set) then
        resource.release(factory, sprite.texture_set)
        sprite.texture_set = nil
    end
end

!export("res_sprite_create")
function res_sprite_create(params : resource.ResourceCreateParams) : int

    local desc = ddf.from_buffer(reflect.type_of(sprite_ddf.SpriteDesc{ }), params.buffer, params.data_offset, params.data_size) as sprite_ddf.SpriteDesc
    if nil == desc then
        return resource.RESULT_FORMAT_ERROR
    end

    local sprite:SpriteResource = SpriteResource {
        desc = desc,
        path = params.filename
    }

    local acq:int = acquire_resources(params.factory, sprite)
    if acq == resource.RESULT_OK then
        params.resource = sprite
        return resource.RESULT_OK
    else
        release_resources(params.factory, sprite)
        return acq
    end

    return resource.RESULT_OK
end

!export("res_sprite_recreate")
function res_sprite_recreate(params : resource.ResourceRecreateParams) : int

    local desc = ddf.from_buffer(reflect.type_of(sprite_ddf.SpriteDesc{ }), params.buffer, params.data_offset, params.data_size) as sprite_ddf.SpriteDesc
    if nil == desc then
        return resource.RESULT_FORMAT_ERROR
    end

    local res = params.resource as SpriteResource
    local new_res:SpriteResource = SpriteResource {
        desc = desc,
        path = params.filename
    }

    local acq:int = acquire_resources(params.factory, new_res)
    if acq == resource.RESULT_OK then
        release_resources(params.factory, res)
        res.desc = new_res.desc
        res.path = new_res.path
        res.texture_set = new_res.texture_set
        res.material = new_res.material
        res.default_animation = new_res.default_animation
        return resource.RESULT_OK
    else
        release_resources(params.factory, new_res)
        return acq
    end

    return resource.RESULT_OK
end

!export("res_sprite_destroy")
function res_sprite_destroy(params : resource.ResourceDestroyParams) : int
    release_resources(params.factory, params.resource as SpriteResource);
end
