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
}
