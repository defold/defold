#ifndef DMGRAPHICS_OPENGL_DEFINES_H
#define DMGRAPHICS_OPENGL_DEFINES_H

#if defined(__linux__) && !defined(ANDROID)

#define GL_HAS_RENDERDOC_SUPPORT
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

#define GL_HAS_RENDERDOC_SUPPORT
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

// Types
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT_4444                 (GL_UNSIGNED_SHORT_4_4_4_4)
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT_565                  (GL_UNSIGNED_SHORT_5_6_5)

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
#ifdef GL_COLOR_EXT
#define DMGRAPHICS_RENDER_BUFFER_COLOR                      (GL_COLOR_EXT)
#else
#define DMGRAPHICS_RENDER_BUFFER_COLOR                      (0x1800)
#endif
#ifdef GL_DEPTH_EXT
#define DMGRAPHICS_RENDER_BUFFER_DEPTH                      (GL_DEPTH_EXT)
#else
#define DMGRAPHICS_RENDER_BUFFER_DEPTH                      (0x1801)
#endif
#ifdef GL_STENCIL_EXT
#define DMGRAPHICS_RENDER_BUFFER_STENCIL                    (GL_STENCIL_EXT)
#else
#define DMGRAPHICS_RENDER_BUFFER_STENCIL                    (0x1802)
#endif
#define DMGRAPHICS_RENDER_BUFFER_COLOR_ATTACHMENT           (GL_COLOR_ATTACHMENT0)
#define DMGRAPHICS_RENDER_BUFFER_DEPTH_ATTACHMENT           (GL_DEPTH_ATTACHMENT)
#define DMGRAPHICS_RENDER_BUFFER_STENCIL_ATTACHMENT         (GL_STENCIL_ATTACHMENT)

// Texture formats
// Some platforms (e.g Android) supports texture formats even when undefined
// We check this at runtime through extensions supported
#if defined(GL_HAS_RENDERDOC_SUPPORT)
#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE                 (GL_RED)
#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA           (GL_RG)
#else
#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE                 (GL_LUMINANCE)
#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA           (GL_LUMINANCE_ALPHA)
#endif
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

#endif // DMGRAPHICS_OPENGL_DEFINES_H
