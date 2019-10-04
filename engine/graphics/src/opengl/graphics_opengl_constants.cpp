#include "../graphics.h"
#include "graphics_opengl_defines.h"

namespace dmGraphics
{
    const Type TYPE_BYTE           = GL_BYTE;
    const Type TYPE_UNSIGNED_BYTE  = GL_UNSIGNED_BYTE;
    const Type TYPE_SHORT          = GL_SHORT;
    const Type TYPE_UNSIGNED_SHORT = GL_UNSIGNED_SHORT;
    const Type TYPE_INT            = GL_INT;
    const Type TYPE_UNSIGNED_INT   = GL_UNSIGNED_INT;
    const Type TYPE_FLOAT          = GL_FLOAT;
    const Type TYPE_FLOAT_VEC4     = GL_FLOAT_VEC4;
    const Type TYPE_FLOAT_MAT4     = GL_FLOAT_MAT4;
    const Type TYPE_SAMPLER_2D     = GL_SAMPLER_2D;
    const Type TYPE_SAMPLER_CUBE   = GL_SAMPLER_CUBE;

    const TextureType TEXTURE_TYPE_2D       = GL_TEXTURE_2D;
    const TextureType TEXTURE_TYPE_CUBE_MAP = GL_TEXTURE_CUBE_MAP;

    const TextureFilter TEXTURE_FILTER_DEFAULT                = 0;
    const TextureFilter TEXTURE_FILTER_LINEAR                 = GL_LINEAR;
    const TextureFilter TEXTURE_FILTER_NEAREST                = GL_NEAREST;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST = GL_NEAREST_MIPMAP_NEAREST;
    const TextureFilter TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR  = GL_NEAREST_MIPMAP_LINEAR;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST  = GL_LINEAR_MIPMAP_NEAREST;
    const TextureFilter TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR   = GL_LINEAR_MIPMAP_LINEAR;

#ifndef GL_ARB_multitexture
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_BORDER = 0x812D;
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_EDGE   = 0x812F;
    const TextureWrap TEXTURE_WRAP_MIRRORED_REPEAT = 0x8370;
#else
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_BORDER = GL_CLAMP_TO_BORDER;
    const TextureWrap TEXTURE_WRAP_CLAMP_TO_EDGE   = GL_CLAMP_TO_EDGE;
    const TextureWrap TEXTURE_WRAP_MIRRORED_REPEAT = GL_MIRRORED_REPEAT;
#endif
    const TextureWrap TEXTURE_WRAP_REPEAT          = GL_REPEAT;

    const BlendFactor BLEND_FACTOR_ZERO                     = GL_ZERO;
    const BlendFactor BLEND_FACTOR_ONE                      = GL_ONE;
    const BlendFactor BLEND_FACTOR_SRC_COLOR                = GL_SRC_COLOR;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_SRC_COLOR      = GL_ONE_MINUS_SRC_COLOR;
    const BlendFactor BLEND_FACTOR_DST_COLOR                = GL_DST_COLOR;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_DST_COLOR      = GL_ONE_MINUS_DST_COLOR;
    const BlendFactor BLEND_FACTOR_SRC_ALPHA                = GL_SRC_ALPHA;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      = GL_ONE_MINUS_SRC_ALPHA;
    const BlendFactor BLEND_FACTOR_DST_ALPHA                = GL_DST_ALPHA;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_DST_ALPHA      = GL_ONE_MINUS_DST_ALPHA;
    const BlendFactor BLEND_FACTOR_SRC_ALPHA_SATURATE       = GL_SRC_ALPHA_SATURATE;

#if !defined (GL_ARB_imaging)
    const BlendFactor BLEND_FACTOR_CONSTANT_COLOR           = 0x8001;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = 0x8002;
    const BlendFactor BLEND_FACTOR_CONSTANT_ALPHA           = 0x8003;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = 0x8004;
#else
    const BlendFactor BLEND_FACTOR_CONSTANT_COLOR           = GL_CONSTANT_COLOR;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR = GL_ONE_MINUS_CONSTANT_COLOR;
    const BlendFactor BLEND_FACTOR_CONSTANT_ALPHA           = GL_CONSTANT_ALPHA;
    const BlendFactor BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA = GL_ONE_MINUS_CONSTANT_ALPHA;
#endif

    const State STATE_DEPTH_TEST            = GL_DEPTH_TEST;
    const State STATE_SCISSOR_TEST          = GL_SCISSOR_TEST;
    const State STATE_STENCIL_TEST          = GL_STENCIL_TEST;
#ifndef GL_ES_VERSION_2_0
    const State STATE_ALPHA_TEST            = GL_ALPHA_TEST;
#endif
    const State STATE_BLEND                 = GL_BLEND;
    const State STATE_CULL_FACE             = GL_CULL_FACE;
    const State STATE_POLYGON_OFFSET_FILL   = GL_POLYGON_OFFSET_FILL;

    const CompareFunc COMPARE_FUNC_NEVER    = GL_NEVER;
    const CompareFunc COMPARE_FUNC_LESS     = GL_LESS;
    const CompareFunc COMPARE_FUNC_LEQUAL   = GL_LEQUAL;
    const CompareFunc COMPARE_FUNC_GREATER  = GL_GREATER;
    const CompareFunc COMPARE_FUNC_GEQUAL   = GL_GEQUAL;
    const CompareFunc COMPARE_FUNC_EQUAL    = GL_EQUAL;
    const CompareFunc COMPARE_FUNC_NOTEQUAL = GL_NOTEQUAL;
    const CompareFunc COMPARE_FUNC_ALWAYS   = GL_ALWAYS;

    const StencilOp STENCIL_OP_KEEP         = GL_KEEP;
    const StencilOp STENCIL_OP_ZERO         = GL_ZERO;
    const StencilOp STENCIL_OP_REPLACE      = GL_REPLACE;
    const StencilOp STENCIL_OP_INCR         = GL_INCR;
    const StencilOp STENCIL_OP_INCR_WRAP    = GL_INCR_WRAP;
    const StencilOp STENCIL_OP_DECR         = GL_DECR;
    const StencilOp STENCIL_OP_DECR_WRAP    = GL_DECR_WRAP;
    const StencilOp STENCIL_OP_INVERT       = GL_INVERT;

    const PrimitiveType PRIMITIVE_LINES          = GL_LINES;
    const PrimitiveType PRIMITIVE_TRIANGLES      = GL_TRIANGLES;
    const PrimitiveType PRIMITIVE_TRIANGLE_STRIP = GL_TRIANGLE_STRIP;

    const BufferType BUFFER_TYPE_COLOR_BIT       = GL_COLOR_BUFFER_BIT;
    const BufferType BUFFER_TYPE_DEPTH_BIT       = GL_DEPTH_BUFFER_BIT;
    const BufferType BUFFER_TYPE_STENCIL_BIT     = GL_STENCIL_BUFFER_BIT;

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

    const FaceType FACE_TYPE_FRONT           = GL_FRONT;
    const FaceType FACE_TYPE_BACK            = GL_BACK;
    const FaceType FACE_TYPE_FRONT_AND_BACK  = GL_FRONT_AND_BACK;
}
