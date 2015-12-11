module render

require C
require io
require graphics
require vmath
require resource
require interop

struct Material
    local render_context        : handle
    local program               : handle
    local vertex_program        : handle
    local fragment_program      : handle
    local name_hash_to_location : @interop.HashTable
    local samplers              : @interop.Array
    local constants             : @interop.Array
    local tag_mask              : uint32
    local user_data_1           : uint64
    local user_data_2           : uint64
    local valid                 : uint8
end

struct MaterialProgramConstantInfo
    constant_id   : uint64
    element_ids   : @[4:uint64]
    element_index : int32
end

typealias local h:handle as Context

let RENDER_ORDER_BEFORE_WORLD:uint8  = 0u8
let RENDER_ORDER_WORLD:uint8         = 1u8
let RENDER_ORDER_AFTER_WORLD:uint8   = 2u8

let RENDER_LIST_OPERATION_BEGIN:uint32 = 0u32
let RENDER_LIST_OPERATION_BATCH:uint32 = 1u32
let RENDER_LIST_OPERATION_END:uint32   = 2u32

typealias local v:uint8 as RenderListDispatch

struct RenderListEntry
    world_position  : @vmath.Point3
    order           : uint32
    batch_key       : uint32
    tag_mask        : uint32
    user_data       : any
    major_order     : uint8
    dispatch        : RenderListDispatch
end

struct RenderListAllocation
    entries     : [@RenderListEntry]
    range_begin : uint32
    range_end   : uint32
end

struct Index
    value       : uint32
end

let MAX_TEXTURE_COUNT:uint32   = 32u32
let MAX_CONSTANT_COUNT:uint32  = 4u32

struct Constant
    value       : @vmath.Vector4
    name_hash   : uint64
    type        : int
    location    : int32
end

struct StencilTestParams
    func        : uint32
    op_s_fail   : uint32
    op_d_fail   : uint32
    op_p_pass   : uint32
    flags       : uint32
end

struct RenderObject
    constant            : @[4:@Constant]
    world_transform     : @vmath.Matrix4
    texture_transform   : @vmath.Matrix4
    vertex_buffer       : graphics.VertexBuffer
    vertex_declaration  : graphics.VertexDeclaration
    index_buffer        : handle
    material            : Material
    texture             : @[32:graphics.Texture]
    primitive_type      : uint32
    index_type          : uint32
    source_blend        : uint32
    dest_blend          : uint32
    stencil_test_param  : @StencilTestParams
    vertex_start        : uint32
    vertex_count        : uint32
    vertex_constant_mask       : uint8
    fragment_constant_mask     : uint8
    local flags         : uint8

    function set_flags(set_blend_factors:bool, set_stencil_tests:bool)
        self.flags = 0u8
        if set_blend_factors then
          self.flags = 1u8
        end
        if set_stencil_tests then
          self.flags = self.flags | 2u8
        end
    end
end

struct RenderListDispatchParams
    context             : Context
    user_data           : any
    operation           : int
    entries             : [@RenderListEntry]
    indices             : [uint32]
    range_begin         : uint32
    range_end           : uint32
end

!symbol("SolGetGraphicsContext")
extern get_graphics_context(context:Context):graphics.Context

!symbol("SolRenderListAlloc")
extern render_list_alloc(context:Context, entries:uint32, alloc_out:RenderListAllocation)

!symbol("SolRenderListSubmit")
extern render_list_submit(context:Context, allocation:RenderListAllocation, end_index:uint32)

!symbol("SolRenderListEnd")
extern render_list_end(context:Context)

!symbol("SolRenderCreateDispatch")
extern render_list_make_dispatch(context:Context, *(RenderListDispatchParams)->(), user_data:any):RenderListDispatch

!symbol("SolRenderGetMaterialTagMask")
extern get_material_tag_mask(material:Material):uint32

!symbol("SolRenderAddToRender")
extern add_to_render(context:Context, ro:RenderObject)

!symbol("SolEnableRenderObjectConstant")
extern enable_render_object_constant(ro:RenderObject, name_hash:uint64, value:vmath.Vector4)

!symbol("SolDisableRenderObjectConstant")
extern disable_render_object_constant(ro:RenderObject, name_hash:uint64, value:vmath.Vector4)

!symbol("SolGetMaterialConstantLocation")
extern get_material_constant_location(material:Material, name_hash:uint64):int

!symbol("SolGetMaterialProgramConstant")
extern get_material_program_constant(material:Material, name_hash:uint64, out_value:Constant):bool

!symbol("SolGetMaterialProgramConstantInfo")
extern get_material_program_constant_info(material:Material, name_hash:uint64, info_out:MaterialProgramConstantInfo):bool
