module graphics

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

struct VertexBuffer
    buffer    : handle
    copy      : handle
    size      : uint32
end

struct TextureParams
end

struct Texture
end

struct VertexDeclarationStream
end

struct VertexDeclaration
    declarations : @[8:@VertexElement]    
end

struct Context
end

struct Program
end

function new_vertex_declaration(context:Context, elements:[@VertexElement], count:uint32):VertexDeclaration
    return VertexDeclaration { }
end

function new_vertex_buffer(context:Context, size:uint32, data:[float], usage:uint32):VertexBuffer
    return VertexBuffer { } 
end

function delete_vertex_buffer(vb:VertexBuffer)
end

function enable_vertex_declaration(context:Context, declaration:VertexDeclaration, vertex_buffer:VertexBuffer)
end

function enable_vertex_declaration(context:Context, declaration:VertexDeclaration, vertex_buffer:VertexBuffer, program:Program)
end

function set_vertex_buffer_data(vb:VertexBuffer, data:any, elements:uint32, usage:uint32)
end

