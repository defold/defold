module comp_sprite

require io
require reflect
require C
require interop
require ddf
require vmath
require gameobject
require gamesys
require graphics
require indexpool
require render
require array
require hash
require sprite_ddf
require tile_ddf
require texture_set_ddf
require gamesys_ddf
require model_ddf
require res_sprite
require res_textureset

-------------------------------------------------------
-- Render object cache to minimize allocation, allocate
-- in blocks of 16 objects at the time
-------------------------------------------------------

struct ROCacheEntry
    render_objects     : @[16:@render.RenderObject]
    next               : ROCacheEntry
end

struct ROCache
    first              : @ROCacheEntry
    capacity           : int
    allocated          : int

    fn expand()
        local blk:ROCacheEntry = self.first
        while not nil(blk.next) do
            blk = blk.next
        end
        blk.next = ROCacheEntry { }
        self.capacity = self.capacity + 16
    end

    fn clear()
        self.allocated = 0
    end

    fn alloc():render.RenderObject
        if self.capacity == 0 then
            self.capacity = 16
        end
        if self.allocated == self.capacity then
            self.expand()
            return self.alloc()
        end
        local idx = self.allocated
        local blk:ROCacheEntry = self.first
        local bi = 0
        while idx >= 16 do
            blk = blk.next
            idx = idx - 16
            bi = bi + 1
        end
        self.allocated = self.allocated + 1
        return blk.render_objects[idx]
    end
end


-------------------------------------------------------
-- Sprite component                                  --
-------------------------------------------------------

struct SpriteVertex
    x : float
    y : float
    z : float
    u : float
    v : float
end

let FLAG_ENABLED:byte         = 1 as byte
let FLAG_ALLOCATED:byte       = 2 as byte
let FLAG_ADDED_TO_UPDATE:byte = 4 as byte

struct SpriteComponent
    index              : int32
    instance           : gameobject.Instance
    position           : @vmath.Point3
    rotation           : @vmath.Quat
    scale              : @vmath.Vector3
    world              : @vmath.Matrix4
    resource           : res_sprite.SpriteResource
    render_constants   : @[4:@render.Constant]
    constant_count     : int
    current_animation  : uint64
    anim_inv_duration  : float
    anim_timer         : float
    playing            : bool
    flip_horizontal    : uint32
    flip_vertical      : uint32
    mixed_hash         : uint32
    component_index    : uint8
    listener           : @gameobject.ComponentMessagePath
    has_listener       : bool
end

struct SpriteWorld
    components         : [@SpriteComponent]
    components_flags   : [byte]
    pool               : @indexpool.IndexPool
    vertex_declaration : graphics.VertexDeclaration
    vertex_buffer      : graphics.VertexBuffer
    vertex_data        : [@SpriteVertex]
    vertex_write_ptr   : uint32
    ro_cache           : @ROCache
end

!export("comp_sprite_new_world")
function new_world(params : gameobject.ComponentNewWorldParams) : int
    local context = params.context as gamesys.SpriteContext
    local world = SpriteWorld { }

    world.components = [context.max_sprite_count:@SpriteComponent]
    world.components_flags = [context.max_sprite_count:byte]
    world.pool = indexpool.make(context.max_sprite_count)

    local ve = [2:@graphics.VertexElement]
    ve[0] = graphics.VertexElement { name = "position", stream = 0x0, size = 0x3, type = graphics.GL_FLOAT, normalize = 0x0 }
    ve[1] = graphics.VertexElement { name = "texcoord0", stream = 0x1, size = 0x2, type = graphics.GL_FLOAT, normalize = 0x0 }

    local graphics_context = render.get_graphics_context(context.render_context)
    world.vertex_declaration = graphics.new_vertex_declaration(graphics_context, ve, 0x2)

    local buf:[float] = nil;
    world.vertex_buffer = graphics.new_vertex_buffer(graphics_context, 0x0, buf, graphics.BUFFER_USAGE_STREAM_DRAW)
    world.vertex_data = [6 * context.max_sprite_count:@SpriteVertex]
    params.world = world

    return gameobject.CREATE_RESULT_OK
end

!export("comp_sprite_delete_world")
function delete_world(params:gameobject.ComponentDeleteWorldParams):int
    local world = params.world as SpriteWorld
    graphics.delete_vertex_buffer(world.vertex_buffer)
    return gameobject.CREATE_RESULT_OK
end

local tmp_hash:hash.HashState32 = hash.HashState32 { }

function rehash(comp:SpriteComponent)
    local state = tmp_hash
    hash.init(state)
    hash.update(state, comp.resource.texture_set)
    hash.update(state, comp.resource.material)
    hash.update(state, comp.resource.desc.blend_mode)
    let size = comp.constant_count
    for i=0, size do
        hash.update(state, comp.render_constants[i])
    end
    comp.mixed_hash = hash.final(state)
end

function get_size(comp:SpriteComponent):vmath.Vector3
    local tmp = vmath.Vector3 { }
    if nil(comp.resource) or nil(comp.resource.texture_set) then
        return tmp
    end
    local ts = comp.resource.texture_set
    local anim_id = res_textureset.get_animation_by_id(ts, comp.current_animation)
    if anim_id >=0 then
        local anims = ts.texture_set.animations
        tmp.x = anims[anim_id].width as float
        tmp.y = anims[anim_id].height as float
        tmp.z = 1.0f
    end
    return tmp
end

function compute_scaled_size(comp:SpriteComponent):vmath.Vector3
    local r = get_size(comp)
    r.x = r.x * comp.scale.x
    r.y = r.y * comp.scale.y
    return r
end

function get_current_tile(comp:SpriteComponent, anim:texture_set_ddf.TextureSetAnimation):uint32
    local t = comp.anim_timer
    if (anim.playback == tile_ddf.PLAYBACK_ONCE_BACKWARD or anim.playback == tile_ddf.PLAYBACK_LOOP_BACKWARD) then
        t = 1.0f - t
    end
    let interval = anim.end_ - anim.start
    local frame_count = interval
    if anim.playback == tile_ddf.PLAYBACK_ONCE_PINGPONG or anim.playback == tile_ddf.PLAYBACK_LOOP_PINGPONG then
        let tmp = 2 * frame_count - 2
        if tmp > 1 then
            frame_count = tmp as uint32
        else
            frame_count = 1u32
        end
    end
    local frame = (t * (frame_count as float)) as uint32
    if frame >= interval then
        frame = (2 * (interval - 1) - frame) as uint32
    end
    return anim.start + frame
end

function animate(world:SpriteWorld, dt:float)
    let flags_mask = FLAG_ALLOCATED | FLAG_ADDED_TO_UPDATE | FLAG_ENABLED
    local count:uint32 = 0u32
    for i,comp in world.components do
        local flags = world.components_flags[i]
        if ((flags & flags_mask) == flags_mask) and comp.playing then
            if nil(comp.resource) then
                io.println("resource nil")
            end
            if nil(comp.resource.texture_set) then
                io.println("texture set nil")
            end
            let tex_set = comp.resource.texture_set
            let anim = res_textureset.get_animation_by_id(tex_set, comp.current_animation)
            if anim >= 0 then
                let ddf = tex_set.texture_set
                let anim = ddf.animations[anim]
                comp.anim_timer = comp.anim_timer + dt * comp.anim_inv_duration
                if comp.anim_timer >= 1.0f then
                    comp.anim_timer = 0.0f
                    if anim.playback == tile_ddf.PLAYBACK_ONCE_FORWARD or
                       anim.playback == tile_ddf.PLAYBACK_ONCE_BACKWARD or
                       anim.playback == tile_ddf.PLAYBACK_ONCE_PINGPONG then
                        comp.anim_timer = 1.0f
                    else
                        comp.anim_timer = comp.anim_timer - (comp.anim_timer as int32) as float
                    end
                end
            end
        end
    end
end

function play_animation(comp:SpriteComponent, animation_id:uint64):bool
    local texture_set = comp.resource.texture_set
    local anim_id = res_textureset.get_animation_by_id(texture_set, animation_id)
    if anim_id >= 0 then
        let tex_set = texture_set.texture_set
        let anim = tex_set.animations[anim_id]
        local frame_count = (anim.end_ - anim.start)
        if (anim.playback == tile_ddf.PLAYBACK_ONCE_PINGPONG) or (anim.playback == tile_ddf.PLAYBACK_LOOP_PINGPONG) then
            let tmp = 2 * frame_count - 2
            if tmp > 1 then
                frame_count = tmp as uint32
            else
                frame_count = 1u32
            end
        end
        comp.current_animation = animation_id
        comp.anim_inv_duration = (anim.fps as float) / (frame_count as float)
        comp.anim_timer = 0f
        comp.playing = (anim.playback ~= tile_ddf.PLAYBACK_NONE)
        return true
    else
        io.println("Unable to play animation [" .. animation_id .. "] because could not be found")
        return false
    end
end

function play_animation(comp:SpriteComponent, animation_id:String):bool
    return play_animation(comp, hash.hash_string64(animation_id))
end

-- Until sol supports annotating external functions with no-escape,
-- use these to pass into native functions to avoid heap allocations.
local tmp_matrices:[@vmath.Matrix4] = [2:@vmath.Matrix4]

function update_transforms(world:SpriteWorld, sub_pixels:bool)
    let flags_mask = FLAG_ALLOCATED | FLAG_ADDED_TO_UPDATE | FLAG_ENABLED
    for i:int=0,#world.components do
        local flags = world.components_flags[i]
        local comp = world.components[i]
        if (flags & flags_mask) == flags_mask then
            local texture_set = comp.resource.texture_set
            local anim_id = res_textureset.get_animation_by_id(texture_set, comp.current_animation)
            if anim_id >= 0 then
                -- compute new world matrix
                local tr = vmath.Transform {
                        rotation = comp.rotation,
                        translation = comp.position,
                        scale = vmath.Vector3 { x=1f, y=1f, z=1f }
                }

                local m_local = tmp_matrices[0]
                local m_world = tmp_matrices[1]
                vmath.transform_to_matrix(m_local, tr)
                gameobject.get_world_matrix(comp.instance, m_world)

                local w:vmath.Matrix4 = vmath.Matrix4 { }
                if gameobject.get_scale_along_z(comp.instance) then
                    vmath.multiply(w, m_world, m_local)
                else
                    vmath.multiply_no_z_scale(w, m_world, m_local)
                end

                vmath.append_scale(w, w, compute_scaled_size(comp))
                if sub_pixels then
                        w.d[12] = ((w.d[12] as int) as float)
                        w.d[13] = ((w.d[13] as int) as float)
                        w.d[14] = ((w.d[14] as int) as float)
                end

                comp.world = w
            end
        end
    end
end

function post_messages(world:SpriteWorld)
    let flags_mask = FLAG_ALLOCATED | FLAG_ADDED_TO_UPDATE | FLAG_ENABLED
    for i:int=0,#world.components do
        local flags = world.components_flags[i]
        local comp = world.components[i]
        if (flags & flags_mask == flags_mask) and comp.playing then
            let texture_set = comp.resource.texture_set
            let anim_id = res_textureset.get_animation_by_id(texture_set, comp.current_animation)
            if anim_id < 0 then
                comp.playing = false
            else
                let ddf = texture_set.texture_set
                let anim = ddf.animations[anim_id]
                if anim.playback == tile_ddf.PLAYBACK_ONCE_FORWARD or anim.playback == tile_ddf.PLAYBACK_ONCE_BACKWARD or
                   anim.playback == tile_ddf.PLAYBACK_ONCE_PINGPONG then
                    --
                    comp.playing = false
                    if comp.has_listener then
                        -- post
                        local msg = sprite_ddf.AnimationDone {
                            current_tile = (get_current_tile(comp, anim) - anim.start + 1) as uint32,
                            id = comp.current_animation
                        };
                        gameobject.post_message(comp.instance, comp.component_index, comp.listener, msg)
                    end
                end
            end
        end
    end
end

-- For zero-allocation render these are here until more sol complier optimizations
-- can be used.
local tmp_vec4:[@vmath.Vector4] = [4:@vmath.Vector4]
local pick_order:[int] = [0,1,2,2,3,0]
local tex_coord_order:[int32] = [
    0,1,2,2,3,0,
    3,2,1,1,0,3,
    1,0,3,3,2,1,
    2,3,0,0,1,2
]

function render_batch(world:SpriteWorld, params:render.RenderListDispatchParams)

    local first:SpriteComponent = nil
    local start = world.vertex_write_ptr

    for i=params.range_begin,params.range_end do

        local entry_idx = params.indices[i]
        local entry = params.entries[entry_idx]
        local comp = entry.user_data as SpriteComponent

        if i == params.range_begin then
            first = comp
        end

        local tex_set = first.resource.texture_set
        local tex_set_ddf = tex_set.texture_set
        local anim_id = res_textureset.get_animation_by_id(tex_set, comp.current_animation)

        if anim_id >= 0 then
            local animation = tex_set_ddf.animations[anim_id]
            local flip_flag = 0
            local tile = get_current_tile(comp, animation)

            if animation.flip_horizontal ~= comp.flip_horizontal then
                flip_flag = 1
            end
            if animation.flip_vertical ~= comp.flip_vertical then
                flip_flag = flip_flag | 2
            end

            local tc = first.resource.unpacked_uvs
            local uv_base = 8 * tile
            local index_base = 6 * flip_flag
            local vtx0 = world.vertex_data[world.vertex_write_ptr+0]

            vmath.multiply(tmp_vec4[0], comp.world, vmath.Vector4 { x=-0.5f, y=-0.5f, z=0.0f, w=1.0f })
            vmath.multiply(tmp_vec4[1], comp.world, vmath.Vector4 { x=-0.5f, y= 0.5f, z=0.0f, w=1.0f })
            vmath.multiply(tmp_vec4[2], comp.world, vmath.Vector4 { x= 0.5f, y= 0.5f, z=0.0f, w=1.0f })
            vmath.multiply(tmp_vec4[3], comp.world, vmath.Vector4 { x= 0.5f, y=-0.5f, z=0.0f, w=1.0f })

            for pi,k in pick_order do
                local v = world.vertex_data[world.vertex_write_ptr + pi]
                local tv = tmp_vec4[k]
                v.x = tv.x
                v.y = tv.y
                v.z = tv.z
                v.u = tc[uv_base + 2*tex_coord_order[index_base+pi]+0]
                v.v = tc[uv_base + 2*tex_coord_order[index_base+pi]+1]
            end

            world.vertex_write_ptr = world.vertex_write_ptr + (#pick_order) as uint32
        end
    end

    if nil(first) then
        return
    end

    -- fill
    local ro = world.ro_cache.alloc()
    ro.constant[0].location = -1
    ro.constant[1].location = -1
    ro.constant[2].location = -1
    ro.constant[3].location = -1
    ro.vertex_declaration = world.vertex_declaration
    ro.vertex_buffer = world.vertex_buffer
    ro.primitive_type = graphics.PRIMITIVE_TRIANGLES
    ro.vertex_start = start
    ro.vertex_count = world.vertex_write_ptr - start
    ro.material = first.resource.material
    vmath.identity(ro.world_transform)
    vmath.identity(ro.texture_transform)
    ro.source_blend = graphics.BLEND_FACTOR_ONE
    ro.dest_blend = graphics.BLEND_FACTOR_ZERO

    local tex_set = first.resource.texture_set
    ro.texture[0] = tex_set.texture

    for i=0,first.constant_count do
        render.enable_render_object_constant(ro, first.render_constants[i].name_hash, first.render_constants[i].value)
    end

    local blend_mode = first.resource.desc.blend_mode

    if blend_mode == sprite_ddf.SPRITE_DESC_BLEND_MODE_ALPHA then
        ro.source_blend = graphics.BLEND_FACTOR_ONE
        ro.dest_blend = graphics.BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
    elseif blend_mode == sprite_ddf.SPRITE_DESC_BLEND_MODE_ADD or blend_mode == sprite_ddf.SPRITE_DESC_BLEND_MODE_ADD_ALPHA then
        ro.source_blend = graphics.BLEND_FACTOR_ONE
        ro.dest_blend = graphics.BLEND_FACTOR_ONE
    elseif blend_mode == sprite_ddf.SPRITE_DESC_BLEND_MODE_MULT then
        ro.source_blend = graphics.BLEND_FACTOR_DST_COLOR
        ro.dest_blend = graphics.BLEND_FACTOR_ONE_MINUS_SRC_ALPHA
    else
        io.println("comp_sprite_sprite: Unknown blend mode " .. blend_mode)
    end

    ro.set_flags(true, false)
    render.add_to_render(params.context, ro)
end

!export("comp_sprite_create")
function create(params : gameobject.ComponentCreateParams) : int

    local world = params.world as SpriteWorld
    local idx = indexpool.alloc(world.pool)
    local resource = params.resource as res_sprite.SpriteResource

    if idx < 0 then
        io.println("Failed to allocate")
        return gameobject.CREATE_RESULT_UNKNOWN_ERROR
    end

    local comp = world.components[idx]
    comp.index = idx
    comp.instance = params.instance
    comp.position = params.position
    comp.rotation = params.rotation
    comp.scale = vmath.Vector3 { x=1.0f, y=1.0f, z=1.0f }
    comp.resource = resource
    comp.playing = false
    comp.component_index = params.component_index
    world.components_flags[idx] = FLAG_ENABLED | FLAG_ALLOCATED
    rehash(comp)

    play_animation(comp, comp.resource.desc.default_animation)

    params.user_data = idx
    return gameobject.CREATE_RESULT_OK
end

!export("comp_sprite_destroy")
function destroy(params : gameobject.ComponentDestroyParams) : int
    local world = params.world as SpriteWorld
    local idx = params.user_data as int
    world.components_flags[idx] = 0 as byte
    indexpool.free(world.pool, idx)
    return gameobject.CREATE_RESULT_OK
end

!export("comp_sprite_init")
function init(params : gameobject.ComponentInitParams) : int
    return gameobject.CREATE_RESULT_OK
end

!export("comp_sprite_final")
function final(params : gameobject.ComponentFinalParams) : int
    return gameobject.CREATE_RESULT_OK
end

!export("comp_sprite_add_to_update")
function add_to_update(params : gameobject.ComponentAddToUpdateParams) : int
    local world = params.world as SpriteWorld
    local idx = params.user_data as int
    world.components_flags[idx] = world.components_flags[idx] | FLAG_ADDED_TO_UPDATE
    return gameobject.UPDATE_RESULT_OK
end

!export("comp_sprite_update")
function update(params : gameobject.ComponentsUpdateParams) : int
    local world = params.world as SpriteWorld
    animate(world, params.dt)
    post_messages(world)
    update_transforms(world, false)
    return gameobject.UPDATE_RESULT_OK
end

!export("comp_sprite_render_dispatch")
function render_dispatch(params:render.RenderListDispatchParams)
    local world = params.user_data as SpriteWorld
    if params.operation == render.RENDER_LIST_OPERATION_BEGIN then
        -- set up for producing vertices
        world.vertex_write_ptr = 0u32
        world.ro_cache.clear()
    elseif params.operation == render.RENDER_LIST_OPERATION_END then
        -- flush them out
        graphics.set_vertex_buffer_data(world.vertex_buffer, world.vertex_data as any, 0u32, graphics.BUFFER_USAGE_STATIC_DRAW)
        graphics.set_vertex_buffer_data(world.vertex_buffer, world.vertex_data as any, world.vertex_write_ptr, graphics.BUFFER_USAGE_STATIC_DRAW)
    elseif params.operation == render.RENDER_LIST_OPERATION_BATCH then
        -- generate
        render_batch(world, params)
    end
end

struct RenderStatic
    alloc : @render.RenderListAllocation -- inline for ease of clearing
end

local render_static:RenderStatic = RenderStatic { }

!export("comp_sprite_render")
function render(params : gameobject.ComponentsRenderParams) : int
--    return gameobject.UPDATE_RESULT_OK
    local context = params.context as gamesys.SpriteContext
    local world = params.world as SpriteWorld
    local render_context = context.render_context

    let flags_mask = FLAG_ALLOCATED | FLAG_ADDED_TO_UPDATE | FLAG_ENABLED
    local count:uint32 = 0u32
    for i,_ in world.components do
        local flags = world.components_flags[i]
        if (flags & flags_mask) == flags_mask then
            count = count + 1u32
        end
    end

    render.render_list_alloc(render_context, count, render_static.alloc)

    local ptr:uint32 = render_static.alloc.range_begin
    let entries = render_static.alloc.entries

    let dispatch:render.RenderListDispatch = render.render_list_make_dispatch(render_context, *render_dispatch, world)

    for i,_ in world.components do
        local flags = world.components_flags[i]
        if (flags & flags_mask) == flags_mask then
            let comp = world.components[i]
            let entry = entries[ptr]
            entry.world_position = vmath.Point3 {
                    x = comp.world.d[12],
                    y = comp.world.d[13],
                    z = comp.world.d[14]
            }
            entry.batch_key = comp.mixed_hash
            entry.tag_mask = render.get_material_tag_mask(comp.resource.material)
            entry.user_data = comp
            entry.major_order = render.RENDER_ORDER_WORLD
            entry.dispatch = dispatch
            ptr = ptr + 1u32
        end
    end

    render.render_list_submit(render_context, render_static.alloc, ptr)
    render_static.alloc = render.RenderListAllocation { } -- don't leave any refs
    return gameobject.UPDATE_RESULT_OK
end

!export("comp_sprite_post_update")
function post_update(params : gameobject.ComponentsPostUpdateParams) : int
    return gameobject.UPDATE_RESULT_OK
end

let HASH_ENABLE:uint64 = hash.hash_string64("enable")
let HASH_DISABLE:uint64 = hash.hash_string64("disable")
let HASH_PLAY_ANIMATION:uint64 = hash.hash_string64("play_animation")
let HASH_SET_FLIP_H:uint64 = hash.hash_string64("set_flip_horizontal")
let HASH_SET_FLIP_V:uint64 = hash.hash_string64("set_flip_vertical")
let HASH_SET_CONSTANT:uint64 = hash.hash_string64("set_constant")
let HASH_RESET_CONSTANT:uint64 = hash.hash_string64("reset_constant")
let HASH_SET_SCALE:uint64 = hash.hash_string64("set_scale")

struct OnMessageStatic
    play_animation   : @sprite_ddf.PlayAnimation
    set_flip_h       : @sprite_ddf.SetFlipHorizontal
    set_flip_v       : @sprite_ddf.SetFlipVertical
    set_constant     : @model_ddf.SetConstant
    reset_constant   : @model_ddf.ResetConstant
    set_scale        : @sprite_ddf.SetScale
end

local on_message_static:OnMessageStatic = OnMessageStatic { }

!export("comp_sprite_on_message")
function on_message(params : gameobject.ComponentOnMessageParams) : int
    local world = params.world as SpriteWorld
    local idx = params.user_data as int
    local comp = world.components[idx]
    if params.message_id == HASH_PLAY_ANIMATION then
        -- can also do the following to the cost of one allocation
        -- let msg = gameobject.get_message_data(params.message) as sprite_ddf.ResetConstant
        let msg = on_message_static.play_animation
        if gameobject.get_message_data(params.message, msg) then
            if play_animation(comp, msg.id) then
                comp.has_listener = gameobject.get_message_sender_component_path(params.instance, params.message, comp.listener)
            end
        end
    elseif params.message_id == HASH_SET_FLIP_H then
        let msg = on_message_static.set_flip_h
        if gameobject.get_message_data(params.message, msg) then
            comp.flip_horizontal = msg.flip
        end
    elseif params.message_id == HASH_SET_FLIP_V then
        let msg = on_message_static.set_flip_v
        if gameobject.get_message_data(params.message, msg) then
            comp.flip_vertical = msg.flip
        end
    elseif params.message_id == HASH_ENABLE then
        world.components_flags[idx] = world.components_flags[idx] | FLAG_ENABLED
    elseif params.message_id == HASH_DISABLE then
        world.components_flags[idx] = world.components_flags[idx] & (FLAG_ALLOCATED | FLAG_ADDED_TO_UPDATE)
    elseif params.message_id == HASH_SET_CONSTANT then
        let msg = on_message_static.set_constant
        if gameobject.get_message_data(params.message, msg) then
            -- would use stack vec here although the fn call will make it heap alloc
            local tmp = tmp_vec4[0]
            tmp.x = msg.value.x
            tmp.y = msg.value.y
            tmp.z = msg.value.z
            tmp.w = msg.value.w
            let res = gamesys.set_material_constant(comp.resource.material, msg.name_hash, tmp, *set_constant_callback, comp)
            if res == gameobject.PROPERTY_RESULT_NOT_FOUND then
                io.println("Attempt to set constant that does not exist!");
            end
        end
    elseif params.message_id == HASH_RESET_CONSTANT then
        let msg = on_message_static.reset_constant
        if gameobject.get_message_data(params.message, msg) then
            let size = comp.constant_count
            for i=0, size do
                if comp.render_constants[i].name_hash == msg.name_hash then
                    comp.render_constants[i] = comp.render_constants[size-1]
                    comp.constant_count = comp.constant_count - 1
                    rehash(comp)
                end
            end
        end
    elseif params.message_id == HASH_SET_SCALE then
        let msg = on_message_static.set_scale
        if gameobject.get_message_data(params.message, msg) then
            comp.scale.x = msg.scale.x
            comp.scale.y = msg.scale.y
            comp.scale.z = msg.scale.z
        end
    end
    return gameobject.UPDATE_RESULT_OK
end

let prop_size:uint64 = hash.hash_string64("size")
let prop_size_x:uint64 = hash.hash_string64("size.x")
let prop_size_y:uint64 = hash.hash_string64("size.y")
let prop_size_z:uint64 = hash.hash_string64("size.z")
let prop_scale:uint64 = hash.hash_string64("scale")
let prop_scale_x:uint64 = hash.hash_string64("scale.x")
let prop_scale_y:uint64 = hash.hash_string64("scale.y")
let prop_scale_z:uint64 = hash.hash_string64("scale.z")

!export("comp_sprite_get_property")
function get_property(params : gameobject.ComponentGetPropertyParams) : int
    local world = params.world as SpriteWorld
    local idx = params.user_data as int
    local comp = world.components[idx]
    -- size
    if params.property_id == prop_size then
        params.value = get_size(comp)
        params.element_ids[0] = prop_size_x
        params.element_ids[1] = prop_size_y
        params.element_ids[2] = prop_size_z
    elseif params.property_id == prop_size_x then
        params.value = get_size(comp).x as double
    elseif params.property_id == prop_size_y then
        params.value = get_size(comp).y as double
    elseif params.property_id == prop_size_z then
        params.value = get_size(comp).z as double
    elseif params.property_id == prop_scale then
        params.value = comp.scale
        params.element_ids[0] = prop_scale_x
        params.element_ids[1] = prop_scale_y
        params.element_ids[2] = prop_scale_z
    elseif params.property_id == prop_scale_x then
        params.value = comp.scale.x as double
    elseif params.property_id == prop_scale_y then
        params.value = comp.scale.y as double
    elseif params.property_id == prop_scale_z then
        params.value = comp.scale.z as double
    else
        local gcr:gamesys.GetConstantResult = gamesys.GetConstantResult { }
        local ret = gamesys.get_material_constant(comp.resource.material, params.property_id, gcr, *get_constant_callback, comp)
        if ret == gameobject.PROPERTY_RESULT_OK then
            if gcr.element_index == 0 then
                params.value = gcr.value.x as double
            elseif gcr.element_index == 1 then
                params.value = gcr.value.y as double
            elseif gcr.element_index == 2 then
                params.value = gcr.value.z as double
            elseif gcr.element_index == 3 then
                params.value = gcr.value.w as double
            else
                params.value = gcr.value
                for i=0,4 do
                    params.element_ids[i] = gcr.element_ids[i]
                end
            end
            return gameobject.PROPERTY_RESULT_OK
        end
        return gameobject.PROPERTY_RESULT_NOT_FOUND
    end

    return gameobject.PROPERTY_RESULT_OK
end

!export("comp_sprite_get_constant_callback")
function get_constant_callback(user_data:any, name_hash:uint64, output:render.Constant):bool
    local comp = user_data as SpriteComponent
    for i=0, comp.constant_count do
        local c = comp.render_constants[i]
        if c.name_hash == name_hash then
            output.value = c.value
            return true
        end
    end
    return false
end

!export("comp_sprite_set_constant_callback")
function set_constant_callback(user_data:any, name_hash:uint64, element_index:int, value:vmath.Vector4):bool

    local comp = user_data as SpriteComponent
    local target:render.Constant = nil
    for i=0, comp.constant_count do
        if comp.render_constants[i].name_hash == name_hash then
            target = comp.render_constants[i]
        end
    end

    if nil(target) then
        local max = 4
        if comp.constant_count == max then
            io.println("Out of sprint constants! (" .. max .. ")")
            return false
        end

        target = comp.render_constants[comp.constant_count]
        render.get_material_program_constant(comp.resource.material, name_hash, target)
        comp.constant_count = comp.constant_count + 1
    end

    if element_index == -1 then
        target.value.x = value.x
        target.value.y = value.y
        target.value.z = value.z
        target.value.w = value.w
    elseif element_index == 0 then
        target.value.x = value.x
    elseif element_index == 1 then
        target.value.y = value.x
    elseif element_index == 2 then
        target.value.z = value.x
    elseif element_index == 3 then
        target.value.w = value.x
    end

    rehash(comp)
    return true
end

!export("comp_sprite_set_property")
function set_property(params : gameobject.ComponentSetPropertyParams) : int
    local world = params.world as SpriteWorld
    local idx = params.user_data as int
    local comp = world.components[idx]

    if params.property_id == prop_scale then
        -- compile error if assigning directly
        local val = params.value as vmath.Vector3
        comp.scale.x = val.x
        comp.scale.y = val.y
        comp.scale.z = val.z
    elseif params.property_id == prop_scale_x then
        comp.scale.x = (params.value as double) as float
    elseif params.property_id == prop_scale_y then
        comp.scale.y = (params.value as double) as float
    elseif params.property_id == prop_scale_z then
        comp.scale.z = (params.value as double) as float
    elseif params.property_id == prop_size or params.property_id == prop_size_x or params.property_id == prop_size_y or params.property_id == prop_size_z then
        return gameobject.PROPERTY_RESULT_UNSUPPORTED_OPERATION
    else
        if reflect.type_of(params.value) == reflect.type_of(vmath.Vector4 { }) then
            return gamesys.set_material_constant(comp.resource.material, params.property_id, params.value as vmath.Vector4, *set_constant_callback, comp)
        elseif reflect.type_of(params.value) == reflect.type_of(0d) then
            tmp_vec4[0].x = (params.value as double) as float
            return gamesys.set_material_constant(comp.resource.material, params.property_id, tmp_vec4[0], *set_constant_callback, comp)
        end
        return gameobject.PROPERTY_RESULT_NOT_FOUND
    end

    return gameobject.PROPERTY_RESULT_OK
end
