module res_textureset

require graphics
require ddf
require hash
require interop

require texture_set_ddf

struct TextureSetResource
    -- these are sol allocated from C and can be used freely
    texture                      : graphics.Texture
    texture_set                  : texture_set_ddf.TextureSet
    -- no touching
    local hull_collision_groups  : @interop.Array
    local animation_ids          : @interop.HashTable
    local hull_set               : handle
    local valid                  : uint8
end

!export("sol_res_textureset_alloc")
function sol_res_textureset_alloc():any
    return TextureSetResource { }
end

!symbol("SolTextureSetResourceGetAnimationById")
extern get_animation_by_id(res:TextureSetResource, id:uint64):int
