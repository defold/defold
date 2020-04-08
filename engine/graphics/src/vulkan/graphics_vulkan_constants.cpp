#include "../graphics.h"
#include "graphics_vulkan_defines.h"

namespace dmGraphics
{
    const StencilOp STENCIL_OP_KEEP         = 0;
    const StencilOp STENCIL_OP_ZERO         = 1;
    const StencilOp STENCIL_OP_REPLACE      = 2;
    const StencilOp STENCIL_OP_INCR         = 3;
    const StencilOp STENCIL_OP_INCR_WRAP    = 4;
    const StencilOp STENCIL_OP_DECR         = 5;
    const StencilOp STENCIL_OP_DECR_WRAP    = 6;
    const StencilOp STENCIL_OP_INVERT       = 7;
}
