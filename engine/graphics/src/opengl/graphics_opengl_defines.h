#ifndef DMGRAPHICS_OPENGL_DEFINES_H
#define DMGRAPHICS_OPENGL_DEFINES_H

#if defined(__linux__) && !defined(ANDROID)

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>

#elif defined (__MACH__)

#if defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR)
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#else
#include <OpenGL/gl.h>
#endif

#elif defined (_WIN32)

#include <dlib/safe_windows.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <win32/glut.h>

#include "win32/glext.h"

#elif defined (ANDROID)
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define GL_BGRA GL_BGRA_EXT
#elif defined (__EMSCRIPTEN__)
#include <GL/gl.h>
#include <GL/glext.h>
#else
#error "Platform not supported."
#endif

// Primitive types
#define DMGRAPHICS_PRIMITIVE_POINTS                         (GL_POINTS)
#define DMGRAPHICS_PRIMITIVE_LINES                          (GL_LINES)
#define DMGRAPHICS_PRIMITIVE_LINE_LOOP                      (GL_LINE_LOOP)
#define DMGRAPHICS_PRIMITIVE_LINE_STRIP                     (GL_LINE_STRIP)
#define DMGRAPHICS_PRIMITIVE_TRIANGLES                      (GL_TRIANGLES)
#define DMGRAPHICS_PRIMITIVE_TRIANGLE_STRIP                 (GL_TRIANGLE_STRIP)
#define DMGRAPHICS_PRIMITIVE_TRIANGLE_FAN                   (GL_TRIANGLE_FAN)

// Buffer type flags
#define DMGRAPHICS_BUFFER_TYPE_COLOR_BIT                    (GL_COLOR_BUFFER_BIT)
#define DMGRAPHICS_BUFFER_TYPE_DEPTH_BIT                    (GL_DEPTH_BUFFER_BIT)
#define DMGRAPHICS_BUFFER_TYPE_STENCIL_BIT                  (GL_STENCIL_BUFFER_BIT)

// Render states
#define DMGRAPHICS_STATE_DEPTH_TEST                         (GL_DEPTH_TEST)
#define DMGRAPHICS_STATE_SCISSOR_TEST                       (GL_SCISSOR_TEST)
#define DMGRAPHICS_STATE_STENCIL_TEST                       (GL_STENCIL_TEST)
#define DMGRAPHICS_STATE_ALPHA_TEST                         (GL_ALPHA_TEST)
#define DMGRAPHICS_STATE_BLEND                              (GL_BLEND)
#define DMGRAPHICS_STATE_CULL_FACE                          (GL_CULL_FACE)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_FILL                (GL_POLYGON_OFFSET_FILL)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_LINE                (GL_POLYGON_OFFSET_LINE)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_POINT               (GL_POLYGON_OFFSET_POINT)

// Types
#define DMGRAPHICS_TYPE_BYTE                                (GL_BYTE)
#define DMGRAPHICS_TYPE_UNSIGNED_BYTE                       (GL_UNSIGNED_BYTE)
#define DMGRAPHICS_TYPE_SHORT                               (GL_SHORT)
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT                      (GL_UNSIGNED_SHORT)
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT_4444                 (GL_UNSIGNED_SHORT_4_4_4_4)
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT_565                  (GL_UNSIGNED_SHORT_5_6_5)
#define DMGRAPHICS_TYPE_INT                                 (GL_INT)
#define DMGRAPHICS_TYPE_UNSIGNED_INT                        (GL_UNSIGNED_INT)
#define DMGRAPHICS_TYPE_FLOAT                               (GL_FLOAT)
#define DMGRAPHICS_TYPE_FLOAT_VEC4                          (GL_FLOAT_VEC4)
#define DMGRAPHICS_TYPE_FLOAT_MAT4                          (GL_FLOAT_MAT4)
#define DMGRAPHICS_TYPE_SAMPLER_2D                          (GL_SAMPLER_2D)
#define DMGRAPHICS_TYPE_SAMPLER_CUBE                        (GL_SAMPLER_CUBE)

#ifdef GL_HALF_FLOAT_OES
#define DMGRAPHICS_TYPE_HALF_FLOAT                          (GL_HALF_FLOAT_OES)
#else
#define DMGRAPHICS_TYPE_HALF_FLOAT                          (GL_HALF_FLOAT)
#endif

// Render buffer storage formats
#ifdef GL_DEPTH_STENCIL_OES
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH_STENCIL       (GL_DEPTH24_STENCIL8_OES)
#else
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH_STENCIL       (GL_DEPTH_STENCIL)
#endif
#ifdef GL_DEPTH_COMPONENT24_OES
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH24             (GL_DEPTH_COMPONENT24_OES)
#else
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH24             (GL_DEPTH_COMPONENT)
#endif
#ifdef GL_DEPTH_COMPONENT16_OES
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH16             (GL_DEPTH_COMPONENT16_OES)
#else
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH16             (GL_DEPTH_COMPONENT16)
#endif
#ifdef GL_STENCIL_INDEX8_OES
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_STENCIL            (GL_STENCIL_INDEX8_OES)
#else
#define DMGRAPHICS_RENDER_BUFFER_FORMAT_STENCIL            (GL_STENCIL_INDEX8)
#endif

// Texture formats
// Some platforms (e.g Android) supports texture formats even when undefined
// We check this at runtime through extensions supported
#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE                 (GL_LUMINANCE)
#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA           (GL_LUMINANCE_ALPHA)
#define DMGRAPHICS_TEXTURE_FORMAT_RGB                       (GL_RGB)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA                      (GL_RGBA)

// Texture formats related to floating point textures and render targets
// There is a bit of difference on format and internal format enums
// should be used for OGL ES 2.0, WebGL and desktop contexts, hence the
// ifdefs below.
#if defined(GL_RED_EXT)
#define DMGRAPHICS_TEXTURE_FORMAT_RED                       (GL_RED_EXT)
#elif defined(GL_RED)
#define DMGRAPHICS_TEXTURE_FORMAT_RED                       (GL_RED)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RED                       (GL_LUMINANCE)
#endif

#if defined(GL_RG_EXT)
#define DMGRAPHICS_TEXTURE_FORMAT_RG                        (GL_RG_EXT)
#elif defined(GL_RG)
#define DMGRAPHICS_TEXTURE_FORMAT_RG                        (GL_RG)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RG                        (GL_LUMINANCE_ALPHA)
#endif

#if defined(GL_RGB32F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_RGB32F                    (GL_RGB32F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGB32F                    (GL_RGB)
#endif

#if defined(GL_RGBA32F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA32F                   (GL_RGBA32F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA32F                   (GL_RGBA)
#endif

#if defined(GL_RGB16F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_RGB16F                    (GL_RGB16F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGB16F                    (GL_RGB)
#endif

#if defined(GL_RGBA16F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA16F                   (GL_RGBA16F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA16F                   (GL_RGBA)
#endif

#if defined(GL_R16F_EXT)
#define DMGRAPHICS_TEXTURE_FORMAT_R16F                      (GL_R16F_EXT)
#elif defined(GL_R16F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_R16F                      (GL_R16F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_R16F                      (0x822D)
#endif

#if defined(GL_R32F_EXT)
#define DMGRAPHICS_TEXTURE_FORMAT_R32F                      (GL_R32F_EXT)
#elif defined(GL_R32F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_R32F                      (GL_R32F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_R32F                      (0x822E)
#endif

#if defined(GL_RG16F_EXT)
#define DMGRAPHICS_TEXTURE_FORMAT_RG16F                     (GL_RG16F_EXT)
#elif defined(GL_RG16F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_RG16F                     (GL_RG16F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RG16F                     (0x822F)
#endif

#if defined(GL_RG32F_EXT)
#define DMGRAPHICS_TEXTURE_FORMAT_RG32F                     (GL_RG32F_EXT)
#elif defined(GL_RG32F) && !defined (__EMSCRIPTEN__)
#define DMGRAPHICS_TEXTURE_FORMAT_RG32F                     (GL_RG32F)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RG32F                     (0x8230)
#endif

// Compressed texture formats
#ifdef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_DXT1                  (GL_COMPRESSED_RGB_S3TC_DXT1_EXT)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_DXT1                  0x83F0
#endif
#ifdef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT1                 (GL_COMPRESSED_RGBA_S3TC_DXT1_EXT)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT1                 0x83F1
#endif
#ifdef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT3                 (GL_COMPRESSED_RGBA_S3TC_DXT3_EXT)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT3                 0x83F2
#endif
#ifdef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT5                 (GL_COMPRESSED_RGBA_S3TC_DXT5_EXT)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT5                 0x83F3
#endif
#ifdef GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_2BPPV1          (GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_2BPPV1          0x8C01
#endif
#ifdef GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_4BPPV1          (GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_4BPPV1          0x8C00
#endif
#ifdef GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1         (GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1         0x8C03
#endif
#ifdef GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1         (GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1         0x8C02
#endif
#ifdef ETC1_RGB8_OES
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_ETC1                  (ETC1_RGB8_OES)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_ETC1                  0x8D64
#endif

// Texture filter
#define DMGRAPHICS_TEXTURE_FILTER_LINEAR                    (GL_LINEAR)
#define DMGRAPHICS_TEXTURE_FILTER_NEAREST                   (GL_NEAREST)
#define DMGRAPHICS_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST    (GL_NEAREST_MIPMAP_NEAREST)
#define DMGRAPHICS_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR     (GL_NEAREST_MIPMAP_LINEAR)
#define DMGRAPHICS_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST     (GL_LINEAR_MIPMAP_NEAREST)
#define DMGRAPHICS_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR      (GL_LINEAR_MIPMAP_LINEAR)

// Texture type
#define DMGRAPHICS_TEXTURE_TYPE_2D                          (GL_TEXTURE_2D)
#define DMGRAPHICS_TEXTURE_TYPE_CUBE_MAP                    (GL_TEXTURE_CUBE_MAP)

// Texture wrap
#define DMGRAPHICS_TEXTURE_WRAP_REPEAT                      (GL_REPEAT)
#ifndef GL_ARB_multitexture
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_BORDER             (0x812D)
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE               (0x812F)
#define DMGRAPHICS_TEXTURE_WRAP_MIRRORED_REPEAT             (0x8370)
#else
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_BORDER             (GL_CLAMP_TO_BORDER)
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE               (GL_CLAMP_TO_EDGE)
#define DMGRAPHICS_TEXTURE_WRAP_MIRRORED_REPEAT             (GL_MIRRORED_REPEAT)
#endif

// Blend factors
#define DMGRAPHICS_BLEND_FACTOR_ZERO                        (GL_ZERO)
#define DMGRAPHICS_BLEND_FACTOR_ONE                         (GL_ONE)
#define DMGRAPHICS_BLEND_FACTOR_SRC_COLOR                   (GL_SRC_COLOR)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_SRC_COLOR         (GL_ONE_MINUS_SRC_COLOR)
#define DMGRAPHICS_BLEND_FACTOR_DST_COLOR                   (GL_DST_COLOR)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_DST_COLOR         (GL_ONE_MINUS_DST_COLOR)
#define DMGRAPHICS_BLEND_FACTOR_SRC_ALPHA                   (GL_SRC_ALPHA)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA         (GL_ONE_MINUS_SRC_ALPHA)
#define DMGRAPHICS_BLEND_FACTOR_DST_ALPHA                   (GL_DST_ALPHA)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_DST_ALPHA         (GL_ONE_MINUS_DST_ALPHA)
#define DMGRAPHICS_BLEND_FACTOR_SRC_ALPHA_SATURATE          (GL_SRC_ALPHA_SATURATE)

// Compare functions
#define DMGRAPHICS_COMPARE_FUNC_NEVER                       (GL_NEVER)
#define DMGRAPHICS_COMPARE_FUNC_LESS                        (GL_LESS)
#define DMGRAPHICS_COMPARE_FUNC_LEQUAL                      (GL_LEQUAL)
#define DMGRAPHICS_COMPARE_FUNC_GREATER                     (GL_GREATER)
#define DMGRAPHICS_COMPARE_FUNC_GEQUAL                      (GL_GEQUAL)
#define DMGRAPHICS_COMPARE_FUNC_EQUAL                       (GL_EQUAL)
#define DMGRAPHICS_COMPARE_FUNC_NOTEQUAL                    (GL_NOTEQUAL)
#define DMGRAPHICS_COMPARE_FUNC_ALWAYS                      (GL_ALWAYS)

// Stencil operation
#define DMGRAPHICS_STENCIL_OP_KEEP                          (GL_KEEP)
#define DMGRAPHICS_STENCIL_OP_ZERO                          (GL_ZERO)
#define DMGRAPHICS_STENCIL_OP_REPLACE                       (GL_REPLACE)
#define DMGRAPHICS_STENCIL_OP_INCR                          (GL_INCR)
#define DMGRAPHICS_STENCIL_OP_INCR_WRAP                     (GL_INCR_WRAP)
#define DMGRAPHICS_STENCIL_OP_DECR                          (GL_DECR)
#define DMGRAPHICS_STENCIL_OP_DECR_WRAP                     (GL_DECR_WRAP)
#define DMGRAPHICS_STENCIL_OP_INVERT                        (GL_INVERT)

#if !defined (GL_ARB_imaging)
#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_COLOR              (0x8001)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR    (0x8002)
#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_ALPHA              (0x8003)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA    (0x8004)
#else
#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_COLOR              (GL_CONSTANT_COLOR)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR    (GL_ONE_MINUS_CONSTANT_COLOR)
#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_ALPHA              (GL_CONSTANT_ALPHA)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA    (GL_ONE_MINUS_CONSTANT_ALPHA)
#endif

#if !defined (GL_ARB_vertex_buffer_object)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_DRAW                 (0x88E0)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_READ                 (0x88E1)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_COPY                 (0x88E2)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_DRAW                (0x88E8)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_READ                (0x88E9)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_COPY                (0x88EA)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_DRAW                 (0x88E4)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_READ                 (0x88E5)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_COPY                 (0x88E6)
#define DMGRAPHICS_BUFFER_ACCESS_READ_ONLY                  (0x88B8)
#define DMGRAPHICS_BUFFER_ACCESS_WRITE_ONLY                 (0x88B9)
#define DMGRAPHICS_BUFFER_ACCESS_READ_WRITE                 (0x88BA)
#else
#define DMGRAPHICS_BUFFER_USAGE_STREAM_DRAW                 (GL_STREAM_DRAW)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_READ                 (GL_STREAM_READ)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_COPY                 (GL_STREAM_COPY)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_DRAW                (GL_DYNAMIC_DRAW)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_READ                (GL_DYNAMIC_READ)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_COPY                (GL_DYNAMIC_COPY)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_DRAW                 (GL_STATIC_DRAW)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_READ                 (GL_STATIC_READ)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_COPY                 (GL_STATIC_COPY)
#define DMGRAPHICS_BUFFER_ACCESS_READ_ONLY                  (GL_READ_ONLY)
#define DMGRAPHICS_BUFFER_ACCESS_WRITE_ONLY                 (GL_WRITE_ONLY)
#define DMGRAPHICS_BUFFER_ACCESS_READ_WRITE                 (GL_READ_WRITE)
#endif

#define DMGRAPHICS_WINDOW_STATE_OPENED                      0x00020001
#define DMGRAPHICS_WINDOW_STATE_ACTIVE                      0x00020002
#define DMGRAPHICS_WINDOW_STATE_ICONIFIED                   0x00020003
#define DMGRAPHICS_WINDOW_STATE_ACCELERATED                 0x00020004
#define DMGRAPHICS_WINDOW_STATE_RED_BITS                    0x00020005
#define DMGRAPHICS_WINDOW_STATE_GREEN_BITS                  0x00020006
#define DMGRAPHICS_WINDOW_STATE_BLUE_BITS                   0x00020007
#define DMGRAPHICS_WINDOW_STATE_ALPHA_BITS                  0x00020008
#define DMGRAPHICS_WINDOW_STATE_DEPTH_BITS                  0x00020009
#define DMGRAPHICS_WINDOW_STATE_STENCIL_BITS                0x0002000A
#define DMGRAPHICS_WINDOW_STATE_REFRESH_RATE                0x0002000B
#define DMGRAPHICS_WINDOW_STATE_ACCUM_RED_BITS              0x0002000C
#define DMGRAPHICS_WINDOW_STATE_ACCUM_GREEN_BITS            0x0002000D
#define DMGRAPHICS_WINDOW_STATE_ACCUM_BLUE_BITS             0x0002000E
#define DMGRAPHICS_WINDOW_STATE_ACCUM_ALPHA_BITS            0x0002000F
#define DMGRAPHICS_WINDOW_STATE_AUX_BUFFERS                 0x00020010
#define DMGRAPHICS_WINDOW_STATE_STEREO                      0x00020011
#define DMGRAPHICS_WINDOW_STATE_WINDOW_NO_RESIZE            0x00020012
#define DMGRAPHICS_WINDOW_STATE_FSAA_SAMPLES                0x00020013

#define DMGRAPHICS_FACE_TYPE_FRONT                          (GL_FRONT)
#define DMGRAPHICS_FACE_TYPE_BACK                           (GL_BACK)
#define DMGRAPHICS_FACE_TYPE_FRONT_AND_BACK                 (GL_FRONT_AND_BACK)

#endif // DMGRAPHICS_OPENGL_DEFINES_H
