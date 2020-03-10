#include "../graphics.h"
#include "graphics_vulkan_defines.h"

namespace dmGraphics
{
    const TextureType TEXTURE_TYPE_2D            = 11;
    const TextureType TEXTURE_TYPE_CUBE_MAP      = 12;

    const TextureFilter TEXTURE_FILTER_DEFAULT                = 0;
    const TextureFilter TEXTURE_FILTER_NEAREST                = 1;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 2;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 3;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 4;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 5;
    const TextureFilter TEXTURE_FILTER_LINEAR                 = 6;

    const StencilOp STENCIL_OP_KEEP         = 0;
    const StencilOp STENCIL_OP_ZERO         = 1;
    const StencilOp STENCIL_OP_REPLACE      = 2;
    const StencilOp STENCIL_OP_INCR         = 3;
    const StencilOp STENCIL_OP_INCR_WRAP    = 4;
    const StencilOp STENCIL_OP_DECR         = 5;
    const StencilOp STENCIL_OP_DECR_WRAP    = 6;
    const StencilOp STENCIL_OP_INVERT       = 7;

    const BufferAccess BUFFER_ACCESS_READ_ONLY  = 3;
    const BufferAccess BUFFER_ACCESS_WRITE_ONLY = 4;
    const BufferAccess BUFFER_ACCESS_READ_WRITE = 5;
}
