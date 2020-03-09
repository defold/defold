#include "../graphics.h"
#include "graphics_vulkan_defines.h"

namespace dmGraphics
{
    const TextureWrap TEXTURE_WRAP_REPEAT          = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_BORDER = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_EDGE   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const TextureWrap TEXTURE_WRAP_MIRRORED_REPEAT = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

    const TextureType TEXTURE_TYPE_2D            = 11;
    const TextureType TEXTURE_TYPE_CUBE_MAP      = 12;

    const TextureFilter TEXTURE_FILTER_DEFAULT                = 0;
    const TextureFilter TEXTURE_FILTER_NEAREST                = 1;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 2;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 3;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 4;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 5;
    const TextureFilter TEXTURE_FILTER_LINEAR                 = 6;

    // Lookup table types
    const BlendFactor BLEND_FACTOR_ZERO                     = 0;
    const BlendFactor BLEND_FACTOR_ONE                      = 1;
    const BlendFactor BLEND_FACTOR_SRC_COLOR                = 2;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_SRC_COLOR      = 3;
    const BlendFactor BLEND_FACTOR_DST_COLOR                = 4;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_DST_COLOR      = 5;
    const BlendFactor BLEND_FACTOR_SRC_ALPHA                = 6;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      = 7;
    const BlendFactor BLEND_FACTOR_DST_ALPHA                = 8;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_DST_ALPHA      = 9;
    const BlendFactor BLEND_FACTOR_SRC_ALPHA_SATURATE       = 10;
    const BlendFactor BLEND_FACTOR_CONSTANT_COLOR           = 11;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 12;
    const BlendFactor BLEND_FACTOR_CONSTANT_ALPHA           = 13;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 14;

    const CompareFunc COMPARE_FUNC_NEVER    = 0;
    const CompareFunc COMPARE_FUNC_LESS     = 1;
    const CompareFunc COMPARE_FUNC_LEQUAL   = 2;
    const CompareFunc COMPARE_FUNC_GREATER  = 3;
    const CompareFunc COMPARE_FUNC_GEQUAL   = 4;
    const CompareFunc COMPARE_FUNC_EQUAL    = 5;
    const CompareFunc COMPARE_FUNC_NOTEQUAL = 6;
    const CompareFunc COMPARE_FUNC_ALWAYS   = 7;

    const StencilOp STENCIL_OP_KEEP         = 0;
    const StencilOp STENCIL_OP_ZERO         = 1;
    const StencilOp STENCIL_OP_REPLACE      = 2;
    const StencilOp STENCIL_OP_INCR         = 3;
    const StencilOp STENCIL_OP_INCR_WRAP    = 4;
    const StencilOp STENCIL_OP_DECR         = 5;
    const StencilOp STENCIL_OP_DECR_WRAP    = 6;
    const StencilOp STENCIL_OP_INVERT       = 7;

    const BufferUsage BUFFER_USAGE_STREAM_DRAW  = 0;
    const BufferUsage BUFFER_USAGE_STATIC_DRAW  = 1;
    const BufferUsage BUFFER_USAGE_DYNAMIC_DRAW = 2;
    const BufferAccess BUFFER_ACCESS_READ_ONLY  = 3;
    const BufferAccess BUFFER_ACCESS_WRITE_ONLY = 4;
    const BufferAccess BUFFER_ACCESS_READ_WRITE = 5;

    const FaceType FACE_TYPE_FRONT           = 0;
    const FaceType FACE_TYPE_BACK            = 1;
    const FaceType FACE_TYPE_FRONT_AND_BACK  = 2;
}
