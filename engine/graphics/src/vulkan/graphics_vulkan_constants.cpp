#include "../graphics.h"
#include "graphics_vulkan_defines.h"

namespace dmGraphics
{
	// Matching VK_* Types
    const Type TYPE_BYTE                           = VK_FORMAT_R8_SINT;
    const Type TYPE_UNSIGNED_BYTE                  = VK_FORMAT_R8_UINT;
    const Type TYPE_SHORT                          = VK_FORMAT_R16_SINT;
    const Type TYPE_UNSIGNED_SHORT                 = VK_FORMAT_R16_UINT;
    const Type TYPE_INT                            = VK_FORMAT_R32_SINT;
    const Type TYPE_UNSIGNED_INT                   = VK_FORMAT_R32_UINT;
    const Type TYPE_FLOAT                          = VK_FORMAT_R32_SFLOAT;
    const Type TYPE_FLOAT_VEC4                     = VK_FORMAT_R32G32B32A32_SFLOAT;
    const Type TYPE_FLOAT_MAT4                     = VK_FORMAT_R32G32B32A32_SFLOAT;

    const TextureWrap TEXTURE_WRAP_REPEAT          = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_BORDER = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_EDGE   = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const TextureWrap TEXTURE_WRAP_MIRRORED_REPEAT = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;

	// Missing VK equivalent (use increasing number)
    const BufferType BUFFER_TYPE_COLOR_BIT       = 1;
    const BufferType BUFFER_TYPE_DEPTH_BIT       = 2;
    const BufferType BUFFER_TYPE_STENCIL_BIT     = 3;

    const State STATE_DEPTH_TEST                 = 4;
    const State STATE_SCISSOR_TEST               = 5;
    const State STATE_STENCIL_TEST               = 6;
    const State STATE_ALPHA_TEST                 = 7;
    const State STATE_BLEND                      = 8;
    const State STATE_CULL_FACE                  = 9;
    const State STATE_POLYGON_OFFSET_FILL        = 10;

    const TextureType TEXTURE_TYPE_2D            = 11;
    const TextureType TEXTURE_TYPE_CUBE_MAP      = 12;

    const Type TYPE_SAMPLER_2D                   = 13;
    const Type TYPE_SAMPLER_CUBE                 = 14;

    const TextureFilter TEXTURE_FILTER_DEFAULT                = 15;
    const TextureFilter TEXTURE_FILTER_LINEAR                 = 16;
    const TextureFilter TEXTURE_FILTER_NEAREST                = 17;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = 18;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = 19;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = 20;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = 21;

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

    const PrimitiveType PRIMITIVE_LINES          = 0;
    const PrimitiveType PRIMITIVE_TRIANGLES      = 1;
    const PrimitiveType PRIMITIVE_TRIANGLE_STRIP = 2;

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
