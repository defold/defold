#ifndef OPENGL_DEVICE_DEFINES_H
#define OPENGL_DEVICE_DEFINES_H

#ifdef __linux__

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#elif defined (__MACH__)

#include <OpenGL/gl.h>

#elif defined (_WIN32)

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <win32/glut.h>

#else
#error "Platform not supported."
#endif

namespace GFX
{
    typedef uint32_t            HVertexProgram;
    typedef uint32_t            HFragmentProgram;
    typedef struct Context*     HContext;
    typedef struct Device*      HDevice;
    typedef struct Texture*     HTexture;
}

// Primitive types
#define GFXDEVICE_PRIMITIVE_POINTLIST                   (GL_POINTS)
#define GFXDEVICE_PRIMITIVE_LINES                       (GL_LINES)
#define GFXDEVICE_PRIMITIVE_LINE_LOOP                   (GL_LINE_LOOP)
#define GFXDEVICE_PRIMITIVE_LINE_STRIP                  (-1ul)
#define GFXDEVICE_PRIMITIVE_TRIANGLES                   (GL_TRIANGLES)
#define GFXDEVICE_PRIMITIVE_TRIANGLE_STRIP              (GL_TRIANGLE_STRIP)
#define GFXDEVICE_PRIMITIVE_TRIANGLE_FAN                (GL_TRIANGLE_FAN)

// Clear flags
#define GFXDEVICE_CLEAR_COLOURUFFER                     (GL_COLOR_BUFFER_BIT)
#define GFXDEVICE_CLEAR_DEPTHBUFFER                     (GL_DEPTH_BUFFER_BIT)
#define GFXDEVICE_CLEAR_STENCILBUFFER                   (GL_STENCIL_BUFFER_BIT)

#define GFXDEVICE_MATRIX_TYPE_WORLD                     (123ul)
#define GFXDEVICE_MATRIX_TYPE_VIEW                      (321ul)
#define GFXDEVICE_MATRIX_TYPE_PROJECTION                (GL_PROJECTION)

// Render states
#define GFXDEVICE_STATE_DEPTH_TEST                      (GL_DEPTH_TEST)
#define GFXDEVICE_STATE_ALPHA_TEST                      (GL_ALPHA_TEST)
#define GFXDEVICE_STATE_BLEND                           (GL_BLEND)

// Types
#define GFXDEVICE_TYPE_BYTE                             (GL_BYTE)
#define GFXDEVICE_TYPE_UNSIGNED_BYTE                    (GL_UNSIGNED_BYTE)
#define GFXDEVICE_TYPE_SHORT                            (GL_SHORT)
#define GFXDEVICE_TYPE_UNSIGNED_SHORT                   (GL_UNSIGNED_SHORT)
#define GFXDEVICE_TYPE_INT                              (GL_INT)
#define GFXDEVICE_TYPE_UNSIGNED_INT                     (GL_UNSIGNED_INT)
#define GFXDEVICE_TYPE_FLOAT                            (GL_FLOAT)

// Texture format
#define GFXDEVICE_TEXTURE_FORMAT_ALPHA                  (GL_ALPHA)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA                   (GL_RGBA)
#define GFXDEVICE_TEXTURE_FORMAT_RGB_DXT1               (GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT1              (GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT1              (GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT3              (GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
#define GFXDEVICE_TEXTURE_FORMAT_RGBA_DXT5              (GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)

// Blend factors
#define GFXDEVICE_BLEND_FACTOR_ZERO                     (GL_ZERO)
#define GFXDEVICE_BLEND_FACTOR_ONE                      (GL_ONE)
#define GFXDEVICE_BLEND_FACTOR_SRC_COLOR                (GL_SRC_COLOR)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_SRC_COLOR      (GL_ONE_MINUS_SRC_COLOR)
#define GFXDEVICE_BLEND_FACTOR_DST_COLOR                (GL_DST_COLOR)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_DST_COLOR      (GL_ONE_MINUS_DST_COLOR)
#define GFXDEVICE_BLEND_FACTOR_SRC_ALPHA                (GL_SRC_ALPHA)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA      (GL_ONE_MINUS_SRC_ALPHA)
#define GFXDEVICE_BLEND_FACTOR_DST_ALPHA                (GL_DST_ALPHA)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_DST_ALPHA      (GL_ONE_MINUS_DST_ALPHA)
#define GFXDEVICE_BLEND_FACTOR_SRC_ALPHA_SATURATE       (GL_SRC_ALPHA_SATURATE)

#if !defined (GL_ARB_imaging)
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_COLOR           (0x8001)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR (0x8002)
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_ALPHA           (0x8003)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA (0x8004)
#else
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_COLOR           (GL_CONSTANT_COLOR)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR (GL_ONE_MINUS_CONSTANT_COLOR)
#define GFXDEVICE_BLEND_FACTOR_CONSTANT_ALPHA           (GL_CONSTANT_ALPHA)
#define GFXDEVICE_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA (GL_ONE_MINUS_CONSTANT_ALPHA)
#endif


#endif // OPENGL_DEVICE_DEFINES_H
