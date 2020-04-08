#include "../graphics.h"
#include "graphics_opengl_defines.h"

namespace dmGraphics
{
    const StencilOp STENCIL_OP_KEEP         = GL_KEEP;
    const StencilOp STENCIL_OP_ZERO         = GL_ZERO;
    const StencilOp STENCIL_OP_REPLACE      = GL_REPLACE;
    const StencilOp STENCIL_OP_INCR         = GL_INCR;
    const StencilOp STENCIL_OP_INCR_WRAP    = GL_INCR_WRAP;
    const StencilOp STENCIL_OP_DECR         = GL_DECR;
    const StencilOp STENCIL_OP_DECR_WRAP    = GL_DECR_WRAP;
    const StencilOp STENCIL_OP_INVERT       = GL_INVERT;
}
