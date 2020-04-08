#include "../graphics.h"
#include "graphics_vulkan_defines.h"

namespace dmGraphics
{
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
}
