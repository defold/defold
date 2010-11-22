#ifndef OPENGL_DEVICE_DEFINES_H
#define OPENGL_DEVICE_DEFINES_H


// Primitive types
#define GFXDEVICE_PRIMITIVE_POINTLIST           (-1ul)
#define GFXDEVICE_PRIMITIVE_LINES               (-1ul)
#define GFXDEVICE_PRIMITIVE_LINE_LOOP           (-1ul)
#define GFXDEVICE_PRIMITIVE_LINE_STRIP          (-1ul)
#define GFXDEVICE_PRIMITIVE_TRIANGLES           (-1ul)
#define GFXDEVICE_PRIMITIVE_TRIANGLE_STRIP      (-1ul)
#define GFXDEVICE_PRIMITIVE_TRIANGLE_FAN        (-1ul)
#define GFXDEVICE_PRIMITIVE_QUADS               (-1ul)
#define GFXDEVICE_PRIMITIVE_QUAD_STRIP          (-1ul)

// Clear flags
#define GFXDEVICE_CLEAR_COLOURUFFER            (-1ul)
#define GFXDEVICE_CLEAR_DEPTHBUFFER            (-1ul)
#define GFXDEVICE_CLEAR_STENCILBUFFER          (-1ul)

// Render states
#define GFXDEVICE_STATE_DEPTH_TEST             (-1ul)
#define GFXDEVICE_STATE_ALPHA_TEST             (-1ul)
#define GFXDEVICE_STATE_BLEND                  (-1ul)

// Types
#define GFXDEVICE_TYPE_BYTE                    (-1ul)
#define GFXDEVICE_TYPE_UNSIGNED_BYTE           (-1ul)
#define GFXDEVICE_TYPE_SHORT                   (-1ul)
#define GFXDEVICE_TYPE_UNSIGNED_SHORT          (-1ul)
#define GFXDEVICE_TYPE_INT                     (-1ul)
#define GFXDEVICE_TYPE_UNSIGNED_INT            (-1ul)
#define GFXDEVICE_TYPE_FLOAT                   (-1ul)

// Texture format
#define GFXDEVICE_TEXTURE_FORMAT_ALPHA         (-1ul)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA          (-1ul)
#define GFXDEVICE_TEXTURE_FORMAT_RGB_DXT1      (-1ul)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT1     (-1ul)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT1     (-1ul)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT3     (-1ul)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT5     (-1ul)

// Blend factors
#define GFXDEVICE_BLEND_FACTOR_ZERO                     (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE                      (-1ul)
#define GFXDEVICE_BLEND_FACTOR_SRC_COLOR                (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_SRC_COLOR      (-1ul)
#define GFXDEVICE_BLEND_FACTOR_DST_COLOR                (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_DST_COLOR      (-1ul)
#define GFXDEVICE_BLEND_FACTOR_SRC_ALPHA                (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      (-1ul)
#define GFXDEVICE_BLEND_FACTOR_DST_ALPHA                (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_DST_ALPHA      (-1ul)
#define GFXDEVICE_BLEND_FACTOR_SRC_ALPHA_SATURATE       (-1ul)

#if !defined (GL_ARB_imaging)
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_COLOR           (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR (-1ul)
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_ALPHA           (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA (-1ul)
#else
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_COLOR           (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR (-1ul)
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_ALPHA           (-1ul)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA (-1ul)
#endif


#endif // OPENGL_DEVICE_DEFINES_H
