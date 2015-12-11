module graphics

typealias local h:int as gl_int

let GL_BYTE                                 : uint32 = 0x1400
let GL_UNSIGNED_BYTE                        : uint32 = 0x1401
let GL_SHORT                                : uint32 = 0x1402
let GL_UNSIGNED_SHORT                       : uint32 = 0x1403
let GL_INT                                  : uint32 = 0x1404
let GL_UNSIGNED_INT                         : uint32 = 0x1405
let GL_FLOAT                                : uint32 = 0x1406
let GL_DOUBLE                               : uint32 = 0x140A
let GL_2_BYTES                              : uint32 = 0x1407
let GL_3_BYTES                              : uint32 = 0x1408
let GL_4_BYTES                              : uint32 = 0x1409

let BUFFER_USAGE_STREAM_DRAW    : uint32 = 0x88E0
let BUFFER_USAGE_STREAM_READ    : uint32 = 0x88E1
let BUFFER_USAGE_STREAM_COPY    : uint32 = 0x88E2

let BUFFER_USAGE_DYNAMIC_DRAW   : uint32 = 0x88E8
let BUFFER_USAGE_DYNAMIC_READ   : uint32 = 0x88E9
let BUFFER_USAGE_DYNAMIC_COPY   : uint32 = 0x88EA

let BUFFER_USAGE_STATIC_DRAW    : uint32 = 0x88E4
let BUFFER_USAGE_STATIC_READ    : uint32 = 0x88E5
let BUFFER_USAGE_STATIC_COPY    : uint32 = 0x88E6

let PRIMITIVE_LINES          : uint32 = 0x0001
let PRIMITIVE_LINE_LOOP      : uint32 = 0x0002
let PRIMITIVE_LINE_STRIP     : uint32 = 0x0003
let PRIMITIVE_TRIANGLES      : uint32 = 0x0004
let PRIMITIVE_TRIANGLE_STRIP : uint32 = 0x0005
let PRIMITIVE_TRIANGLE_FAN   : uint32 = 0x0006
let PRIMITIVE_QUADS          : uint32 = 0x0007
let PRIMITIVE_QUAD_STRIP     : uint32 = 0x0008
let PRIMITIVE_POLYGON        : uint32 = 0x0009

let BLEND_FACTOR_ZERO        : uint32 = 0x0000
let BLEND_FACTOR_ONE         : uint32 = 0x0001
let BLEND_FACTOR_SRC_ALPHA   : uint32 = 0x0302
let BLEND_FACTOR_DST_ALPHA   : uint32 = 0x0304

let BLEND_FACTOR_ONE_MINUS_SRC_ALPHA : uint32 = 0x0303
let BLEND_FACTOR_ONE_MINUS_DST_ALPHA : uint32 = 0x0305

let BLEND_FACTOR_DST_COLOR : uint32           = 0x0306
let BLEND_FACTOR_ONE_MINUS_DST_COLOR : uint32 = 0x0307

struct VertexElement
    name      : String
    stream    : uint32
    size      : uint32
    type      : uint32
    normalize : uint32
end

struct TextureParams
    local format       : int
    local min_filter   : int
    local mag_filter   : int
    local u_wrap       : int
    local v_wrap       : int
    local data         : handle
    local data_size    : uint32
    local mipmap       : uint16
    local width        : uint16
    local height       : uint16
    local sub_update   : bool
    local x            : uint32
    local y            : uint32
end

struct Texture
    local type            : int
    local texture         : gl_int
    local width           : uint16
    local height          : uint16
    local original_width  : uint16
    local original_height : uint16
    local valid           : uint8
    local params          : @TextureParams
end

struct VertexDeclarationStream
    name           : String
    logical_index  : uint16
    physical_index : int16
    size           : uint16
    offset         : uint16
    type           : int
    normalize      : bool
end

struct VertexDeclaration
    local streams               : @[8:@VertexDeclarationStream]
    local stream_count          : uint16
    local stride                : uint16
    local bound_for_program     : handle
    local modification_version  : uint32
    local valid                 : uint8
end

struct VertexBuffer
    local vb     : gl_int
    local valid  : uint8
end

struct Context
end

struct Program
end

!symbol("SolNewVertexDeclaration")
extern new_vertex_declaration(context:Context, elements:[@VertexElement], count:uint32):VertexDeclaration

!symbol("SolEnableVertexDeclarationCVV")
extern enable_vertex_declaration(context:Context, declaration:VertexDeclaration, vertex_buffer:VertexBuffer)

!symbol("SolEnableVertexDeclarationCVVP")
extern enable_vertex_declaration(context:Context, declaration:VertexDeclaration, vertex_buffer:VertexBuffer, program:Program)

!symbol("SolNewVertexBuffer")
extern new_vertex_buffer(context:Context, size:uint32, data:[float], usage:uint32):VertexBuffer

!symbol("SolDeleteVertexBuffer")
extern delete_vertex_buffer(vb:VertexBuffer)

!symbol("SolSetVertexBufferData")
extern set_vertex_buffer_data(vb:VertexBuffer, data:any, elements:uint32, usage:uint32)
