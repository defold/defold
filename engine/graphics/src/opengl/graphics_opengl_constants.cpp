#include "../graphics.h"
#include "graphics_opengl_defines.h"

namespace dmGraphics
{
    const TextureType TEXTURE_TYPE_2D       = GL_TEXTURE_2D;
    const TextureType TEXTURE_TYPE_CUBE_MAP = GL_TEXTURE_CUBE_MAP;

    const TextureFilter TEXTURE_FILTER_DEFAULT                = 0;
    const TextureFilter TEXTURE_FILTER_LINEAR                 = GL_LINEAR;
    const TextureFilter TEXTURE_FILTER_NEAREST                = GL_NEAREST;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = GL_NEAREST_MIPMAP_LINEAR;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = GL_LINEAR_MIPMAP_NEAREST;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = GL_LINEAR_MIPMAP_LINEAR;

    const StencilOp STENCIL_OP_KEEP         = GL_KEEP;
    const StencilOp STENCIL_OP_ZERO         = GL_ZERO;
    const StencilOp STENCIL_OP_REPLACE      = GL_REPLACE;
    const StencilOp STENCIL_OP_INCR         = GL_INCR;
    const StencilOp STENCIL_OP_INCR_WRAP    = GL_INCR_WRAP;
    const StencilOp STENCIL_OP_DECR         = GL_DECR;
    const StencilOp STENCIL_OP_DECR_WRAP    = GL_DECR_WRAP;
    const StencilOp STENCIL_OP_INVERT       = GL_INVERT;

#if !defined (GL_ARB_vertex_buffer_object)
    const BufferUsage BUFFER_USAGE_STREAM_DRAW  = 0x88E0;
    const BufferUsage BUFFER_USAGE_STATIC_DRAW  = 0x88E4;
    const BufferUsage BUFFER_USAGE_DYNAMIC_DRAW = 0x88E8;
    const BufferAccess BUFFER_ACCESS_READ_ONLY  = 0x88B8;
    const BufferAccess BUFFER_ACCESS_WRITE_ONLY = 0x88B9;
    const BufferAccess BUFFER_ACCESS_READ_WRITE = 0x88BA;
#else
    const BufferUsage BUFFER_USAGE_STREAM_DRAW  = GL_STREAM_DRAW;
    const BufferUsage BUFFER_USAGE_STATIC_DRAW  = GL_STATIC_DRAW;
    const BufferUsage BUFFER_USAGE_DYNAMIC_DRAW = GL_DYNAMIC_DRAW;
    const BufferAccess BUFFER_ACCESS_READ_ONLY  = GL_READ_ONLY;
    const BufferAccess BUFFER_ACCESS_WRITE_ONLY = GL_WRITE_ONLY;
    const BufferAccess BUFFER_ACCESS_READ_WRITE = GL_READ_WRITE;
#endif
}
