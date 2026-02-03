// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <dlib/dlib.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/hash.h>
#include <dlib/align.h>
#include <dlib/array.h>
#include <dlib/thread.h>
#include <dlib/time.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/dstrings.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
    #include <emscripten/html5.h>
#endif

#include <platform/platform_window_opengl.h>

#include "graphics_opengl_defines.h"
#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"
#include "graphics_opengl_private.h"

#if defined(DM_PLATFORM_MACOS)
    // Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
    #include <Carbon/Carbon.h>
#endif

/* Include standard OpenGL headers: GLFW uses GL_FALSE/GL_TRUE, and it is
 * convenient for the user to only have to include <GL/glfw.h>. This also
 * solves the problem with Windows <GL/gl.h> and <GL/glu.h> needing some
 * special defines which normally requires the user to include <windows.h>
 * (which is not a nice solution for portable programs).
 */
#if defined(__APPLE_CC__)
    #if defined(DM_PLATFORM_IOS)
        #include <platform/platform_window_ios.h>
        #include <OpenGLES/ES3/gl.h>
    #else
        #include <OpenGL/gl3.h>
        #ifndef GLFW_NO_GLU
            #include <OpenGL/glu.h>
        #endif
    #endif
#elif defined(ANDROID)
    #include <platform/platform_window_android.h>
    #include <EGL/egl.h>
    #include <GLES/gl.h>
#else
    #include <GL/gl.h>
    #ifndef GLFW_NO_GLU
        #include <GL/glu.h>
    #endif
#endif

#if defined (ANDROID)
    #define GL_GLEXT_PROTOTYPES
    #include <GLES2/gl2ext.h>

    #define glDrawArraysInstanced PFN_glDrawArraysInstanced
    #define glDrawElementsInstanced PFN_glDrawElementsInstanced
    #define glVertexAttribDivisor PFN_glVertexAttribDivisor

#elif defined (__MACH__)
    // NOP
#elif defined (__linux__)
    // NOP
#elif defined (_WIN32)
    #ifdef GL_GLEXT_PROTOTYPES
        #undef GL_GLEXT_PROTOTYPES
        #include "win32/glext.h"
        #define GL_GLEXT_PROTOTYPES
    #else
        #include "win32/glext.h"
    #endif

    // VBO Extension for OGL 1.4.1
    typedef void (APIENTRY * PFNGLGENPROGRAMARBPROC) (GLenum, GLuint *);
    typedef void (APIENTRY * PFNGLVERTEXPARAMFLOAT4ARBPROC) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
    typedef void (APIENTRY * PFNGLVERTEXATTRIBSETPROC) (GLuint);
    typedef void (APIENTRY * PFNGLVERTEXATTRIBPTRPROC) (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
    typedef void (APIENTRY * PFNGLTEXPARAM2DPROC) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);
    typedef void (APIENTRY * PFNGLCOMPRTEXSUB2DPROC) (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid *);
    typedef void (APIENTRY * PFNGLBINDBUFFERPROC) (GLenum, GLuint);
    typedef void (APIENTRY * PFNGLBUFFERDATAPROC) (GLenum, GLsizeiptr, const GLvoid*, GLenum);
    typedef void (APIENTRY * PFNGLBINDRENDERBUFFERPROC) (GLenum, GLuint);
    typedef void (APIENTRY * PFNGLRENDERBUFFERSTORAGEPROC) (GLenum, GLenum, GLsizei, GLsizei);
    typedef void (APIENTRY * PFNGLRENDERBUFFERTEXTURE2DPROC) (GLenum, GLenum, GLenum, GLuint, GLint);
    typedef void (APIENTRY * PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum, GLenum, GLenum, GLuint);
    typedef void (APIENTRY * PFNGLBINDFRAMEBUFFERPROC) (GLenum, GLuint);
    typedef void (APIENTRY * PFNGLBUFFERSUBDATAPROC) (GLenum, GLintptr, GLsizeiptr, const GLvoid*);
    typedef void* (APIENTRY * PFNGLMAPBUFFERPROC) (GLenum, GLenum);
    typedef GLboolean (APIENTRY * PFNGLUNMAPBUFFERPROC) (GLenum);
    typedef void (APIENTRY * PFNGLACTIVETEXTUREPROC) (GLenum);
    typedef void (APIENTRY * PFNGLSTENCILFUNCSEPARATEPROC) (GLenum, GLenum, GLint, GLuint);
    typedef void (APIENTRY * PFNGLSTENCILOPSEPARATEPROC) (GLenum, GLenum, GLenum, GLenum);
    typedef void (APIENTRY * PFNGLDRAWBUFFERSPROC) (GLsizei, const GLenum*);
    typedef GLint (APIENTRY * PFNGLGETFRAGDATALOCATIONPROC) (GLuint, const char*);
    typedef void (APIENTRY * PFNGLBINDFRAGDATALOCATIONPROC) (GLuint, GLuint, const char*);

    PFNGLVERTEXATTRIBSETPROC glEnableVertexAttribArray = NULL;
    PFNGLVERTEXATTRIBSETPROC glDisableVertexAttribArray = NULL;
    PFNGLVERTEXATTRIBPTRPROC glVertexAttribPointer = NULL;
    PFNGLTEXPARAM2DPROC glCompressedTexImage2D = NULL;
    PFNGLCOMPRTEXSUB2DPROC glCompressedTexSubImage2D = NULL;
    PFNGLGENBUFFERSPROC glGenBuffersARB = NULL;
    PFNGLDELETEBUFFERSPROC glDeleteBuffersARB = NULL;
    PFNGLBINDBUFFERPROC glBindBufferARB = NULL;
    PFNGLBUFFERDATAPROC glBufferDataARB = NULL;
    PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers = NULL;
    PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer = NULL;
    PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
    PFNGLRENDERBUFFERTEXTURE2DPROC glFramebufferTexture2D = NULL;
    PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer = NULL;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = NULL;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = NULL;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = NULL;
    PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers = NULL;
    PFNGLBUFFERSUBDATAPROC glBufferSubDataARB = NULL;
    PFNGLMAPBUFFERPROC glMapBufferARB = NULL;
    PFNGLUNMAPBUFFERPROC glUnmapBufferARB = NULL;
    PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = NULL;
    PFNGLSTENCILFUNCSEPARATEPROC glStencilFuncSeparate = NULL;
    PFNGLSTENCILOPSEPARATEPROC glStencilOpSeparate = NULL;

    PFNGLGETACTIVEATTRIBPROC glGetActiveAttrib = NULL;
    PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
    PFNGLCREATESHADERPROC glCreateShader = NULL;
    PFNGLSHADERSOURCEPROC glShaderSource = NULL;
    PFNGLCOMPILESHADERPROC glCompileShader = NULL;
    PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
    PFNGLDELETESHADERPROC glDeleteShader = NULL;
    PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
    PFNGLATTACHSHADERPROC glAttachShader = NULL;
    PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
    PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
    PFNGLUSEPROGRAMPROC glUseProgram = NULL;
    PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
    PFNGLGETACTIVEUNIFORMPROC glGetActiveUniform = NULL;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
    PFNGLUNIFORM4FVPROC glUniform4fv = NULL;
    PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv = NULL;
    PFNGLUNIFORM1IPROC glUniform1i = NULL;

    PFNGLTEXSUBIMAGE3DPROC           glTexSubImage3D = NULL;
    PFNGLTEXIMAGE3DPROC              glTexImage3D = NULL;
    PFNGLCOMPRESSEDTEXIMAGE3DPROC    glCompressedTexImage3D = NULL;
    PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D = NULL;

    // Compute
    PFNGLDISPATCHCOMPUTEPROC  glDispatchCompute  = NULL;
    PFNGLMEMORYBARRIERPROC    glMemoryBarrier    = NULL;
    PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = NULL;

    // Uniform buffer objects
    PFNGLBINDBUFFERBASEPROC          glBindBufferBase          = NULL;
    PFNGLBUFFERDATAPROC              glBufferData              = NULL;
    PFNGLGETUNIFORMBLOCKINDEXPROC    glGetUniformBlockIndex    = NULL;
    PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv = NULL;
    PFNGLGETACTIVEUNIFORMSIVPROC     glGetActiveUniformsiv     = NULL;
    PFNGLGENBUFFERSPROC              glGenBuffers              = NULL;
    PFNGLBINDBUFFERPROC              glBindBuffer              = NULL;
    PFNGLUNIFORMBLOCKBINDINGPROC     glUniformBlockBinding     = NULL;

    #if !defined(GL_ES_VERSION_2_0)
        PFNGLGETSTRINGIPROC glGetStringi = NULL;
        PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
        PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
        PFNGLDRAWBUFFERSPROC glDrawBuffers = NULL;
        PFNGLGETFRAGDATALOCATIONPROC glGetFragDataLocation = NULL;
        PFNGLBINDFRAGDATALOCATIONPROC glBindFragDataLocation = NULL;
        PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced = NULL;
        PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced = NULL;
        PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor = NULL;
    #endif
#elif defined(__EMSCRIPTEN__)
    #include <GL/glext.h>
    #if defined GL_ES_VERSION_2_0
        #undef GL_ARRAY_BUFFER_ARB
        #undef GL_ELEMENT_ARRAY_BUFFER_ARB
    #endif
#else
    #error "Platform not supported."
#endif

// OpenGLES compatibility
#if defined(GL_ES_VERSION_2_0)
    #define glClearDepth glClearDepthf
    #define glGenBuffersARB glGenBuffers
    #define glDeleteBuffersARB glDeleteBuffers
    #define glBindBufferARB glBindBuffer
    #define glBufferDataARB glBufferData
    #define glBufferSubDataARB glBufferSubData
    #define glMapBufferARB glMapBufferOES
    #define glUnmapBufferARB glUnmapBufferOES
    #define GL_ARRAY_BUFFER_ARB GL_ARRAY_BUFFER
    #define GL_ELEMENT_ARRAY_BUFFER_ARB GL_ELEMENT_ARRAY_BUFFER
    #define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F

    #ifdef ANDROID
        #define glBindVertexArray glBindVertexArrayOES
        #define glGenVertexArrays glGenVertexArraysOES
    #endif
#endif

DM_PROPERTY_EXTERN(rmtp_DrawCalls);
DM_PROPERTY_EXTERN(rmtp_DispatchCalls);

namespace dmGraphics
{
    using namespace dmVMath;

    #define TO_STR_CASE(x) case x: return #x;
    static const char* GetGLErrorLiteral(GLint err)
    {
        switch(err)
        {
            TO_STR_CASE(GL_NO_ERROR);
            TO_STR_CASE(GL_INVALID_ENUM);
            TO_STR_CASE(GL_INVALID_VALUE);
            TO_STR_CASE(GL_INVALID_OPERATION);
            TO_STR_CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
            TO_STR_CASE(GL_OUT_OF_MEMORY);
            // These are not available by the gl headers we are using currently
            // but left for posterity
            // TO_STR_CASE(GL_STACK_UNDERFLOW);
            // TO_STR_CASE(GL_STACK_OVERFLOW);
        }
        return "<unknown-gl-error>";
    }
    #undef TO_STR_CASE

    static inline void LogGLError(GLint err, const char* fnname, int line)
    {
        dmLogError("%s(%d): gl error %d: %s\n", fnname, line, err, GetGLErrorLiteral(err));
    }

// We use defines here so that we get a callstack from the correct function

#if !defined(ANDROID)

#define CHECK_GL_ERROR \
    { \
        if(g_Context->m_VerifyGraphicsCalls) { \
            GLint err = glGetError(); \
            if (err != 0) \
            { \
                LogGLError(err, __FUNCTION__, __LINE__); \
                assert(0); \
            } \
        } \
    }

#else

// GL_OUT_OF_MEMORY==1285
// Due to the fact that Android can start destroying the surface while we have a frame render in flight,
// we need to not assert on this and instead wait for the proper APP_CMD_* event
#define CHECK_GL_ERROR \
    { \
        if(g_Context->m_VerifyGraphicsCalls) { \
            GLint err = glGetError(); \
            if (err != 0) \
            { \
                LogGLError(err, __FUNCTION__, __LINE__); \
                if (err == GL_OUT_OF_MEMORY) { \
                    dmLogWarning("Signs of surface being destroyed. skipping assert.");\
                    if (dmPlatform::AndroidVerifySurface(g_Context->m_Window)) { \
                        assert(0); \
                    } \
                } else { \
                    assert(0); \
                } \
            } \
        } \
    }

#endif

static bool OpenGLIsTextureFormatSupported(HContext _context, TextureFormat format);

static void OpenGLClearGLError()
{
    GLint err = glGetError();
    while (err != 0)
    {
        err = glGetError();
    }
}

#define CLEAR_GL_ERROR { if(g_Context->m_VerifyGraphicsCalls) OpenGLClearGLError(); }


static void LogFrameBufferError(GLenum status)
{
    switch (status)
    {
#ifdef GL_FRAMEBUFFER_UNDEFINED
        case GL_FRAMEBUFFER_UNDEFINED:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_UNDEFINED, "GL_FRAMEBUFFER_UNDEFINED");
            break;
#endif
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT");
            break;
// glDrawBuffer() not available in ES 2.0
#ifdef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER, "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER");
            break;
#endif

// glReadBuffer() not available in ES 2.0
#ifdef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER, "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER");
            break;
#endif

        case GL_FRAMEBUFFER_UNSUPPORTED:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_UNSUPPORTED, "GL_FRAMEBUFFER_UNSUPPORTED");
            break;

#ifdef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE");
            break;
#endif

#if defined(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE) && !defined(GL_ES_VERSION_3_0)
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE");
            break;
#endif


#ifdef GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT, "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT");
            break;
#endif


#ifdef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
            dmLogError("gl error %d: %s", GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS");
            break;
#endif

        default:
            dmLogError("gl error 0x%08x: <unknown>", status);
            break;
    }
}

#define CHECK_GL_FRAMEBUFFER_ERROR \
    { \
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); \
        if (status != GL_FRAMEBUFFER_COMPLETE) \
        { \
            LogFrameBufferError(status);\
            assert(false);\
        } \
    } \


    #if defined(DM_PLATFORM_IOS)
    struct ChooseEAGLView
    {
        ChooseEAGLView() {
            // Let's us choose the CAEAGLLayer
            // Note: We don't need a valid window here (and we don't have access to one)
            dmPlatform::SetiOSViewTypeOpenGL((dmPlatform::HWindow) 0);
        }
    } g_ChooseEAGLView;
    #endif

    static GraphicsAdapterFunctionTable OpenGLRegisterFunctionTable();
    static bool                         OpenGLIsSupported();
    static HContext                     OpenGLGetContext();

    #if defined(DM_GRAPHICS_USE_OPENGLES)
    static GraphicsAdapter g_opengl_adapter(ADAPTER_FAMILY_OPENGLES);
    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterOpenGLES, &g_opengl_adapter, OpenGLIsSupported, OpenGLRegisterFunctionTable, OpenGLGetContext, ADAPTER_FAMILY_PRIORITY_OPENGLES);
    #else
    static GraphicsAdapter g_opengl_adapter(ADAPTER_FAMILY_OPENGL);
    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterOpenGL, &g_opengl_adapter, OpenGLIsSupported, OpenGLRegisterFunctionTable, OpenGLGetContext, ADAPTER_FAMILY_PRIORITY_OPENGL);
    #endif

    static void PostDeleteTextures(OpenGLContext*, bool);
    static bool OpenGLInitialize(HContext _context, const ContextParams& params);

    extern GLenum TEXTURE_UNIT_NAMES[32];

    // Cross-platform OpenGL/ES extension points. We define our own function pointer typedefs to handle the combination of statically or dynamically linked or core functionality.
    // The alternative is a matrix of conditional typedefs, linked statically/dynamically or core. OpenGL function prototypes does not change, so this is safe.
    typedef void (* DM_PFNGLINVALIDATEFRAMEBUFFERPROC) (GLenum target, GLsizei numAttachments, const GLenum *attachments);
    DM_PFNGLINVALIDATEFRAMEBUFFERPROC PFN_glInvalidateFramebuffer = NULL;

    typedef void (* DM_PFNGLDRAWBUFFERSPROC) (GLsizei n, const GLenum *bufs);
    DM_PFNGLDRAWBUFFERSPROC PFN_glDrawBuffers = NULL;

    // Note: This is necessary for webgl and android to work since we don't load core functions with emsc,
    //       however we might want to do this the other way around perhaps? i.e special case for webgl
    //       and load functions like this for all other platforms.
#ifdef ANDROID
    typedef void (* DM_PFNGLTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
    DM_PFNGLTEXSUBIMAGE3DPROC PFN_glTexSubImage3D = NULL;

    typedef void (* DM_PFNGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
    DM_PFNGLTEXIMAGE3DPROC PFN_glTexImage3D = NULL;

    typedef void (* DM_PFNGLCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data);
    DM_PFNGLCOMPRESSEDTEXIMAGE3DPROC PFN_glCompressedTexImage3D = NULL;

    typedef void (* DM_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data);
    DM_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC PFN_glCompressedTexSubImage3D = NULL;

    typedef void (* DM_PFNGLDRAWARRAYSINSTANCEDPROC) (GLenum mode, GLint first, GLsizei count, GLsizei primcount);
    DM_PFNGLDRAWARRAYSINSTANCEDPROC PFN_glDrawArraysInstanced = NULL;

    typedef void (* DM_PFNGLDRAWELEMENTSINSTANCEDPROC) (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei primcount);
    DM_PFNGLDRAWELEMENTSINSTANCEDPROC PFN_glDrawElementsInstanced = NULL;

    typedef void (* DM_PFNGLVERTEXATTRIBDIVISORPROC) (GLuint index, GLuint divisor);
    DM_PFNGLVERTEXATTRIBDIVISORPROC PFN_glVertexAttribDivisor = NULL;

    typedef void (* DM_PFNGLMEMORYBARRIERPROC) (GLbitfield barriers);
    DM_PFNGLMEMORYBARRIERPROC glMemoryBarrier = NULL;

    typedef void (* DM_PFNGLDISPATCHCOMPUTEPROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
    DM_PFNGLDISPATCHCOMPUTEPROC glDispatchCompute = NULL;

    typedef void (* DM_PFNGLBINDIMAGETEXTUREPROC) (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
    DM_PFNGLBINDIMAGETEXTUREPROC glBindImageTexture = NULL;

    typedef void (* DM_PFNGLBINDBUFFERBASEPROC) (GLenum target, GLuint index, GLuint buffer);
    DM_PFNGLBINDBUFFERBASEPROC glBindBufferBase = NULL;

    typedef GLuint (* DM_PFNGLGETUNIFORMBLOCKINDEXPROC) (GLuint program, const GLchar *uniformBlockName);
    DM_PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex = NULL;

    typedef void (* DM_PFNGLGETACTIVEUNIFORMBLOCKIVPROC) (GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
    DM_PFNGLGETACTIVEUNIFORMBLOCKIVPROC glGetActiveUniformBlockiv = NULL;

    typedef void (* DM_PFNGLGETACTIVEUNIFORMSIVPROC) (GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params);
    DM_PFNGLGETACTIVEUNIFORMSIVPROC glGetActiveUniformsiv = NULL;

    typedef void (* DM_PFNGLUNIFORMBLOCKBINDINGPROC) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    DM_PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding = NULL;
#endif

    OpenGLContext* g_Context = 0x0;

    static HOpenglID AddNewGLHandle(HContext _context, GLuint handle)
    {
        OpenGLContext* context = (OpenGLContext*) _context;

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_GLHandlesData.m_Mutex);
        HOpenglID result_idx = context->m_GLHandlesData.m_AllGLHandles.Size();
        if (!context->m_GLHandlesData.m_FreeIndexes.Empty())
        {
            result_idx = context->m_GLHandlesData.m_FreeIndexes.Back();
            context->m_GLHandlesData.m_FreeIndexes.Pop();
        }
        else
        {
            if (context->m_GLHandlesData.m_AllGLHandles.Full())
            {
                context->m_GLHandlesData.m_AllGLHandles.OffsetCapacity(32);
            }
            context->m_GLHandlesData.m_AllGLHandles.Push(0);
        }

        context->m_GLHandlesData.m_AllGLHandles[result_idx] = handle;
        return result_idx;
    }

    static inline GLuint GetGLHandle(OpenGLContext* context, HOpenglID idx)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_GLHandlesData.m_Mutex);
        return context->m_GLHandlesData.m_AllGLHandles[idx];
    }

    static inline GLuint* GetGLHandlePointer(OpenGLContext* context, HOpenglID idx)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_GLHandlesData.m_Mutex);
        return &(context->m_GLHandlesData.m_AllGLHandles[idx]);
    }

    static inline void CleanupGLHandle(OpenGLContext* context, HOpenglID idx)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_GLHandlesData.m_Mutex);
        context->m_GLHandlesData.m_AllGLHandles[idx] = 0;
        if (context->m_GLHandlesData.m_FreeIndexes.Full())
        {
            context->m_GLHandlesData.m_FreeIndexes.OffsetCapacity(32);
        }
        context->m_GLHandlesData.m_FreeIndexes.Push(idx);
    }


    OpenGLContext::OpenGLContext(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_ModificationVersion     = 1;
        m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        m_PrintDeviceInfo         = params.m_PrintDeviceInfo;
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_Width                   = params.m_Width;
        m_Height                  = params.m_Height;
        m_Window                  = params.m_Window;
        m_JobThread               = params.m_JobThread;

        // We need to have some sort of valid default filtering
        if (m_DefaultTextureMinFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR;
        if (m_DefaultTextureMagFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;

        assert(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));

        // Formats supported on all platforms
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
        m_IndexBufferFormatSupport |= 1 << INDEXBUFFER_FORMAT_16;

        m_GLHandlesData.m_AllGLHandles.SetCapacity(1024);
        m_GLHandlesData.m_FreeIndexes.SetCapacity(256);

        DM_STATIC_ASSERT(sizeof(m_TextureFormatSupport) * 8 >= TEXTURE_FORMAT_COUNT, Invalid_Struct_Size );
    }

    static GLenum GetOpenGLPrimitiveType(PrimitiveType prim_type)
    {
        const GLenum primitive_type_lut[] = {
            GL_LINES,
            GL_TRIANGLES,
            GL_TRIANGLE_STRIP
        };
        return primitive_type_lut[prim_type];
    }

    static GLenum GetOpenGLState(State state)
    {
        GLenum state_lut[] = {
            GL_DEPTH_TEST,
            GL_SCISSOR_TEST,
            GL_STENCIL_TEST,
        #if !defined(GL_ES_VERSION_2_0)
            GL_ALPHA_TEST,
        #else
            0x0BC0,
        #endif
            GL_BLEND,
            GL_CULL_FACE,
            GL_POLYGON_OFFSET_FILL,
            // Alpha test enabled
        #if !defined(GL_ES_VERSION_2_0)
            1,
        #else
            0,
        #endif
        };

        return state_lut[state];
    }

    static GLenum GetOpenGLType(Type type)
    {
        const GLenum type_lut[] = {
            GL_BYTE,
            GL_UNSIGNED_BYTE,
            GL_SHORT,
            GL_UNSIGNED_SHORT,
            GL_INT,
            GL_UNSIGNED_INT,
            GL_FLOAT,
            GL_FLOAT_VEC4,
            GL_FLOAT_MAT4,
            GL_SAMPLER_2D,
            GL_SAMPLER_CUBE,
            DMGRAPHICS_SAMPLER_2D_ARRAY,
            DMGRAPHICS_IMAGE_2D,
        };
        return type_lut[type];
    }

    static Type GetGraphicsType(GLenum type)
    {
        switch(type)
        {
            case GL_BYTE:                     return TYPE_BYTE;
            case GL_UNSIGNED_BYTE:            return TYPE_UNSIGNED_BYTE;
            case GL_SHORT:                    return TYPE_SHORT;
            case GL_UNSIGNED_SHORT:           return TYPE_UNSIGNED_SHORT;
            case GL_INT:                      return TYPE_INT;
            case GL_UNSIGNED_INT:             return TYPE_UNSIGNED_INT;
            case GL_FLOAT:                    return TYPE_FLOAT;
            case GL_FLOAT_VEC2:               return TYPE_FLOAT_VEC2;
            case GL_FLOAT_VEC3:               return TYPE_FLOAT_VEC3;
            case GL_FLOAT_VEC4:               return TYPE_FLOAT_VEC4;
            case GL_FLOAT_MAT2:               return TYPE_FLOAT_MAT2;
            case GL_FLOAT_MAT3:               return TYPE_FLOAT_MAT3;
            case GL_FLOAT_MAT4:               return TYPE_FLOAT_MAT4;
            case GL_SAMPLER_2D:               return TYPE_SAMPLER_2D;
            case DMGRAPHICS_SAMPLER_2D_ARRAY: return TYPE_SAMPLER_2D_ARRAY;
            case GL_SAMPLER_CUBE:             return TYPE_SAMPLER_CUBE;
            case DMGRAPHICS_IMAGE_2D:         return TYPE_IMAGE_2D;
            case DMGRAPHICS_SAMPLER_3D:       return TYPE_SAMPLER_3D;
            case DMGRAPHICS_IMAGE_3D:         return TYPE_IMAGE_3D;
            default:break;
        }

        return (Type) -1;
    }

    static GLenum GetOpenGLTextureType(TextureType type)
    {
        switch(type)
        {
            case TEXTURE_TYPE_2D:         return GL_TEXTURE_2D;
            case TEXTURE_TYPE_2D_ARRAY:   return GL_TEXTURE_2D_ARRAY;
            case TEXTURE_TYPE_CUBE_MAP:   return GL_TEXTURE_CUBE_MAP;
            case TEXTURE_TYPE_IMAGE_2D:   return GL_TEXTURE_2D;
            case TEXTURE_TYPE_IMAGE_3D:   return GL_TEXTURE_3D;
            case TEXTURE_TYPE_3D:         return GL_TEXTURE_3D;
            case TEXTURE_TYPE_TEXTURE_3D: return GL_TEXTURE_3D;
            default:break;
        }
        return GL_FALSE;
    }

    static int WorkerAcquireContextRunner(dmJobThread::HContext, dmJobThread::HJob job, void* _context, void* _acquire_flag)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        bool acquire_flag = (uintptr_t) _acquire_flag;
        assert(dmAtomicGet32(&context->m_AuxContextJobPending));

        if (acquire_flag)
        {
            context->m_AuxContext = dmPlatform::AcquireAuxContext(context->m_Window);
        }
        else
        {
            dmPlatform::UnacquireAuxContext(context->m_Window, context->m_AuxContext);
        }

        dmAtomicStore32(&context->m_AuxContextJobPending, 0);
        return 0;
    }

    static void AcquireAuxContextOnThread(OpenGLContext* context, bool acquire_flag)
    {
        if (!context->m_AsyncProcessingSupport)
            return;
        if (!context->m_JobThread)
            return;

        // TODO: If we have multiple workers, we need to either tag one of them as a graphics-only worker,
        //       or create multiple aux contexts and do an acquire for each of them.
        //       But since we only have one worker thread right now, we can leave that for when we have more.
        assert(dmJobThread::GetWorkerCount(context->m_JobThread) == 1);

        dmAtomicStore32(&context->m_AuxContextJobPending, 1);

        dmJobThread::Job job = {0};
        job.m_Process = WorkerAcquireContextRunner;
        job.m_Callback = 0;
        job.m_Context = (void*) context;
        job.m_Data = (void*) (uintptr_t) acquire_flag;

        dmJobThread::HJob hjob = dmJobThread::CreateJob(context->m_JobThread, &job);
        dmJobThread::PushJob(context->m_JobThread, hjob);

        // Block until the job is done
        while(dmAtomicGet32(&context->m_AuxContextJobPending))
        {
            dmTime::Sleep(100);
        }
    }

    static HContext OpenGLNewContext(const ContextParams& params)
    {
        if (g_Context == 0x0)
        {
            g_Context = new OpenGLContext(params);

            if (OpenGLInitialize(g_Context, params))
            {
                return (HContext) g_Context;
            }

            DeleteContext(g_Context);
        }
        return 0x0;
    }

    static void OpenGLDeleteContext(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        if (context != 0x0)
        {
            dmAtomicStore32(&context->m_DeleteContextRequested, 1);
            AcquireAuxContextOnThread(context, false);
            ResetSetTextureAsyncState(context->m_SetTextureAsyncState);

            if (context->m_GLHandlesData.m_Mutex)
            {
                dmMutex::Delete(context->m_GLHandlesData.m_Mutex);
            }

            delete context;
            g_Context = 0x0;
        }
    }

    static HContext OpenGLGetContext()
    {
        return (HContext) g_Context;
    }

    static bool OpenGLIsSupported()
    {
        return true;
    }

    static void OpenGLFinalize()
    {
    }

    static void StoreExtensions(HContext _context, const GLubyte* _extensions)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        context->m_ExtensionsString = strdup((const char*)_extensions);

        char* iter = 0;
        const char* next = dmStrTok(context->m_ExtensionsString, " ", &iter);
        while (next)
        {
            if (context->m_Extensions.Full())
                context->m_Extensions.OffsetCapacity(4);
            context->m_Extensions.Push(next);
            next = dmStrTok(0, " ", &iter);
        }
    }

    static bool OpenGLIsExtensionSupported(HContext _context, const char* extension)
    {
        /* Extension names should not have spaces. */
        const char* where = strchr(extension, ' ');
        if (where || *extension == '\0')
            return false;

        OpenGLContext* context = (OpenGLContext*) _context;

        uint32_t count = context->m_Extensions.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            if (strcmp(extension, context->m_Extensions[i]) == 0)
                return true;
        }
        return false;
    }

    static uint32_t OpenGLGetNumSupportedExtensions(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return context->m_Extensions.Size();
    }

    static const char* OpenGLGetSupportedExtension(HContext _context, uint32_t index)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return context->m_Extensions[index];
    }

    static bool OpenGLIsContextFeatureSupported(HContext _context, ContextFeature feature)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        switch (feature)
        {
            case CONTEXT_FEATURE_MULTI_TARGET_RENDERING: return context->m_MultiTargetRenderingSupport;
            case CONTEXT_FEATURE_TEXTURE_ARRAY:          return context->m_TextureArraySupport;
            case CONTEXT_FEATURE_COMPUTE_SHADER:         return context->m_ComputeSupport;
            case CONTEXT_FEATURE_STORAGE_BUFFER:         return context->m_StorageBufferSupport;
            case CONTEXT_FEATURE_INSTANCING:             return context->m_InstancingSupport;
            case CONTEXT_FEATURE_3D_TEXTURES:            return context->m_3DTextureSupport;
            case CONTEXT_FEATURE_ASTC_ARRAY_TEXTURES:    return context->m_ASTCArrayTextureSupport;
            case CONTEXT_FEATURE_VSYNC:
                break;
        }
        return false;
    }

    static uintptr_t GetExtProcAddress(const char* name, const char* extension_name, const char* core_name, HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        dmPlatform::HWindow window = GetWindow(context);

        /*
            Check in order
            1) ARB - Extensions officially approved by the OpenGL Architecture Review Board
            2) EXT - Extensions agreed upon by multiple OpenGL vendors
            3) OES - Vendor specific code for the OpenGL ES working group
            4) Optionally check as core function (if not GLES and core_name is set)
        */
        uintptr_t func = 0x0;

        if (extension_name)
        {
            static const char* ext_name_prefix_str[] = {"GL_ARB_", "GL_EXT_", "GL_OES_"};
            static const char* proc_name_postfix_str[] = {"ARB", "EXT", "OES"};
            char proc_str[256];
            for(uint32_t i = 0; i < sizeof(ext_name_prefix_str)/sizeof(*ext_name_prefix_str); ++i)
            {
                // Check for extension name string AND process function pointer. Either may be disabled (by vendor) so both must be valid!
                size_t l = dmStrlCpy(proc_str, ext_name_prefix_str[i], 8);
                dmStrlCpy(proc_str + l, extension_name, 256-l);

                if(!OpenGLIsExtensionSupported(context, proc_str))
                {
                    continue;
                }

                l = dmStrlCpy(proc_str, name, 255);
                dmStrlCpy(proc_str + l, proc_name_postfix_str[i], 256-l);
                func = dmPlatform::GetProcAddress(window, proc_str);

                if(func != 0x0)
                {
                    break;
                }
            }
        }
    #if !defined(__EMSCRIPTEN__)
        if(func == 0 && core_name)
        {
            // On OpenGL, optionally check for core driver support if extension wasn't found (i.e extension has become part of core OpenGL)
            func = dmPlatform::GetProcAddress(window, core_name);
        }
    #endif

        return func;
    }

    static bool ValidateAsyncJobProcessing(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;

        // Test async texture access
        {
            TextureCreationParams tcp;
            tcp.m_Width = tcp.m_OriginalWidth = tcp.m_Height = tcp.m_OriginalHeight = 2;
            tcp.m_Type = TEXTURE_TYPE_2D;
            HTexture texture_handle = dmGraphics::NewTexture(context, tcp);

            assert(ASSET_TYPE_TEXTURE == GetAssetType(texture_handle));
            OpenGLTexture* tex = (OpenGLTexture*) context->m_AssetHandleContainer.Get(texture_handle);

            DM_ALIGNED(16) const uint32_t data[] = { 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff };
            TextureParams params;
            params.m_Format = TEXTURE_FORMAT_RGBA;
            params.m_Width = tcp.m_Width;
            params.m_Height = tcp.m_Height;
            params.m_Data = data;
            params.m_DataSize = sizeof(data);
            params.m_MipMap = 0;
            SetTextureAsync(_context, texture_handle, params, 0, 0);

            while(GetTextureStatusFlags(context, texture_handle) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
            {
                dmTime::Sleep(100);
            }

            GLuint tex_handle = GetGLHandle(context, tex->m_TextureIds[0]);
            DM_ALIGNED(16) uint8_t gpu_data[sizeof(data)];
            memset(gpu_data, 0x0, sizeof(gpu_data));
            glBindTexture(GL_TEXTURE_2D, tex_handle);
            CHECK_GL_ERROR;

            GLuint osfb;
            glGenFramebuffers(1, &osfb);
            CHECK_GL_ERROR;
            glBindFramebuffer(GL_FRAMEBUFFER, osfb);
            CHECK_GL_ERROR;

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_handle, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
            {
                GLint vp[4];
                glGetIntegerv( GL_VIEWPORT, vp );
                glViewport(0, 0, tcp.m_Width, tcp.m_Height);
                CHECK_GL_ERROR;
                glReadPixels(0, 0, tcp.m_Width, tcp.m_Height, GL_RGBA, GL_UNSIGNED_BYTE, gpu_data);
                glViewport(vp[0], vp[1], vp[2], vp[3]);
                CHECK_GL_ERROR;
            }
            else
            {
                dmLogDebug("ValidateAsyncJobProcessing glCheckFramebufferStatus failed (%d)", glCheckFramebufferStatus(GL_FRAMEBUFFER));
            }

            glBindTexture(GL_TEXTURE_2D, 0);
            CHECK_GL_ERROR;
            glBindFramebuffer(GL_FRAMEBUFFER, dmPlatform::OpenGLGetDefaultFramebufferId());
            CHECK_GL_ERROR;
            glDeleteFramebuffers(1, &osfb);
            DeleteTexture(_context, texture_handle);

            if(memcmp(data, gpu_data, sizeof(data))!=0)
            {
                dmLogDebug("ValidateAsyncJobProcessing cpu<->gpu data check failed. Unable to verify async texture access integrity.");
                return false;
            }
        }

        return true;
    }

    static void OpenGLPrintDeviceInfo(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        #if defined(DM_GRAPHICS_USE_OPENGLES)
        dmLogInfo("Device: OpenGL ES");
        #else
        dmLogInfo("Device: OpenGL");
        #endif
        dmLogInfo("Renderer: %s", (char *) glGetString(GL_RENDERER));
        dmLogInfo("Version: %s", (char *) glGetString(GL_VERSION));
        dmLogInfo("Vendor: %s", (char *) glGetString(GL_VENDOR));

        dmLogInfo("Extensions:");
        for (uint32_t i = 0; i < OpenGLGetNumSupportedExtensions(context); ++i)
        {
            dmLogInfo("  %s", OpenGLGetSupportedExtension(context, i));
        }

        dmLogInfo("Context features:");
    #define PRINT_FEATURE_IF_SUPPORTED(feature) \
        if (IsContextFeatureSupported(context, feature)) \
            dmLogInfo("  %s", #feature);
        PRINT_FEATURE_IF_SUPPORTED(CONTEXT_FEATURE_MULTI_TARGET_RENDERING);
        PRINT_FEATURE_IF_SUPPORTED(CONTEXT_FEATURE_TEXTURE_ARRAY);
        PRINT_FEATURE_IF_SUPPORTED(CONTEXT_FEATURE_COMPUTE_SHADER);
    #undef PRINT_FEATURE_IF_SUPPORTED
    }

    static bool OpenGLInitialize(HContext _context, const ContextParams& params)
    {
        assert(_context);
        OpenGLContext* context = (OpenGLContext*) _context;

#if defined (_WIN32)
    #define GET_PROC_ADDRESS_OPTIONAL(function, name, type) \
        function = (type)wglGetProcAddress(name);\
        if (function == 0x0)\
        {\
            function = (type)wglGetProcAddress(name "ARB");\
        }\
        if (function == 0x0)\
        {\
            function = (type)wglGetProcAddress(name "EXT");\
        }

    #define GET_PROC_ADDRESS(function, name, type)\
        GET_PROC_ADDRESS_OPTIONAL(function, name, type) \
        if (function == 0x0)\
        {\
            dmLogError("Could not find gl function '%s'.", name);\
            return false;\
        }

        GET_PROC_ADDRESS(glEnableVertexAttribArray, "glEnableVertexAttribArray", PFNGLVERTEXATTRIBSETPROC);
        GET_PROC_ADDRESS(glDisableVertexAttribArray, "glDisableVertexAttribArray", PFNGLVERTEXATTRIBSETPROC);
        GET_PROC_ADDRESS(glVertexAttribPointer, "glVertexAttribPointer", PFNGLVERTEXATTRIBPTRPROC);
        GET_PROC_ADDRESS(glCompressedTexImage2D, "glCompressedTexImage2D", PFNGLTEXPARAM2DPROC);
        GET_PROC_ADDRESS(glCompressedTexSubImage2D, "glCompressedTexSubImage2D", PFNGLCOMPRTEXSUB2DPROC);
        GET_PROC_ADDRESS(glGenBuffersARB, "glGenBuffers", PFNGLGENBUFFERSPROC);
        GET_PROC_ADDRESS(glDeleteBuffersARB, "glDeleteBuffers", PFNGLDELETEBUFFERSPROC);
        GET_PROC_ADDRESS(glBindBufferARB, "glBindBuffer", PFNGLBINDBUFFERPROC);
        GET_PROC_ADDRESS(glBufferDataARB, "glBufferData", PFNGLBUFFERDATAPROC);
        GET_PROC_ADDRESS(glGenRenderbuffers, "glGenRenderbuffers", PFNGLGENRENDERBUFFERSPROC);
        GET_PROC_ADDRESS(glBindRenderbuffer, "glBindRenderbuffer", PFNGLBINDRENDERBUFFERPROC);
        GET_PROC_ADDRESS(glRenderbufferStorage, "glRenderbufferStorage", PFNGLRENDERBUFFERSTORAGEPROC);
        GET_PROC_ADDRESS(glFramebufferTexture2D, "glFramebufferTexture2D", PFNGLRENDERBUFFERTEXTURE2DPROC);
        GET_PROC_ADDRESS(glFramebufferRenderbuffer, "glFramebufferRenderbuffer", PFNGLFRAMEBUFFERRENDERBUFFERPROC);
        GET_PROC_ADDRESS(glGenFramebuffers, "glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC);
        GET_PROC_ADDRESS(glBindFramebuffer, "glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC);
        GET_PROC_ADDRESS(glDeleteFramebuffers, "glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC);
        GET_PROC_ADDRESS(glDeleteRenderbuffers, "glDeleteRenderbuffers", PFNGLDELETERENDERBUFFERSPROC);
        GET_PROC_ADDRESS(glBufferSubDataARB, "glBufferSubData", PFNGLBUFFERSUBDATAPROC);
        GET_PROC_ADDRESS(glMapBufferARB, "glMapBuffer", PFNGLMAPBUFFERPROC); // unused
        GET_PROC_ADDRESS(glUnmapBufferARB, "glUnmapBuffer", PFNGLUNMAPBUFFERPROC); // unused
        GET_PROC_ADDRESS(glActiveTexture, "glActiveTexture", PFNGLACTIVETEXTUREPROC);
        GET_PROC_ADDRESS(glCheckFramebufferStatus, "glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC);
        GET_PROC_ADDRESS(glGetAttribLocation, "glGetAttribLocation", PFNGLGETATTRIBLOCATIONPROC);
        GET_PROC_ADDRESS(glGetActiveAttrib, "glGetActiveAttrib", PFNGLGETACTIVEATTRIBPROC);
        GET_PROC_ADDRESS(glCreateShader, "glCreateShader", PFNGLCREATESHADERPROC);
        GET_PROC_ADDRESS(glShaderSource, "glShaderSource", PFNGLSHADERSOURCEPROC);
        GET_PROC_ADDRESS(glCompileShader, "glCompileShader", PFNGLCOMPILESHADERPROC);
        GET_PROC_ADDRESS(glGetShaderiv, "glGetShaderiv", PFNGLGETSHADERIVPROC);
        GET_PROC_ADDRESS(glGetShaderInfoLog, "glGetShaderInfoLog", PFNGLGETSHADERINFOLOGPROC);
        GET_PROC_ADDRESS(glGetProgramInfoLog, "glGetProgramInfoLog", PFNGLGETPROGRAMINFOLOGPROC);
        GET_PROC_ADDRESS(glDeleteShader, "glDeleteShader", PFNGLDELETESHADERPROC);
        GET_PROC_ADDRESS(glCreateProgram, "glCreateProgram", PFNGLCREATEPROGRAMPROC);
        GET_PROC_ADDRESS(glAttachShader, "glAttachShader", PFNGLATTACHSHADERPROC);
        GET_PROC_ADDRESS(glLinkProgram, "glLinkProgram", PFNGLLINKPROGRAMPROC);
        GET_PROC_ADDRESS(glDeleteProgram, "glDeleteProgram", PFNGLDELETEPROGRAMPROC);
        GET_PROC_ADDRESS(glUseProgram, "glUseProgram", PFNGLUSEPROGRAMPROC);
        GET_PROC_ADDRESS(glGetProgramiv, "glGetProgramiv", PFNGLGETPROGRAMIVPROC);
        GET_PROC_ADDRESS(glGetActiveUniform, "glGetActiveUniform", PFNGLGETACTIVEUNIFORMPROC);
        GET_PROC_ADDRESS(glGetUniformLocation, "glGetUniformLocation", PFNGLGETUNIFORMLOCATIONPROC);
        GET_PROC_ADDRESS(glUniform4fv, "glUniform4fv", PFNGLUNIFORM4FVPROC);
        GET_PROC_ADDRESS(glUniformMatrix4fv, "glUniformMatrix4fv", PFNGLUNIFORMMATRIX4FVPROC);
        GET_PROC_ADDRESS(glUniform1i, "glUniform1i", PFNGLUNIFORM1IPROC);
        GET_PROC_ADDRESS(glStencilOpSeparate, "glStencilOpSeparate", PFNGLSTENCILOPSEPARATEPROC);
        GET_PROC_ADDRESS(glStencilFuncSeparate, "glStencilFuncSeparate", PFNGLSTENCILFUNCSEPARATEPROC);
        GET_PROC_ADDRESS(glTexSubImage3D, "glTexSubImage3D", PFNGLTEXSUBIMAGE3DPROC);
        GET_PROC_ADDRESS(glTexImage3D, "glTexImage3D", PFNGLTEXIMAGE3DPROC);
        GET_PROC_ADDRESS(glCompressedTexImage3D, "glCompressedTexImage3D", PFNGLCOMPRESSEDTEXIMAGE3DPROC);
        GET_PROC_ADDRESS(glCompressedTexSubImage3D, "glCompressedTexSubImage3D", PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC);

        GET_PROC_ADDRESS_OPTIONAL(glDispatchCompute,  "glDispatchCompute",  PFNGLDISPATCHCOMPUTEPROC);
        GET_PROC_ADDRESS_OPTIONAL(glMemoryBarrier,    "glMemoryBarrier",    PFNGLMEMORYBARRIERPROC);
        GET_PROC_ADDRESS_OPTIONAL(glBindImageTexture, "glBindImageTexture", PFNGLBINDIMAGETEXTUREPROC);

        GET_PROC_ADDRESS(glBindBufferBase, "glBindBufferBase", PFNGLBINDBUFFERBASEPROC);
        GET_PROC_ADDRESS(glBufferData, "glBufferData", PFNGLBUFFERDATAPROC);
        GET_PROC_ADDRESS(glGetUniformBlockIndex, "glGetUniformBlockIndex", PFNGLGETUNIFORMBLOCKINDEXPROC);
        GET_PROC_ADDRESS(glGetActiveUniformBlockiv, "glGetActiveUniformBlockiv", PFNGLGETACTIVEUNIFORMBLOCKIVPROC);
        GET_PROC_ADDRESS(glGetActiveUniformsiv, "glGetActiveUniformsiv", PFNGLGETACTIVEUNIFORMSIVPROC);
        GET_PROC_ADDRESS(glGenBuffers, "glGenBuffers", PFNGLGENBUFFERSPROC);
        GET_PROC_ADDRESS(glBindBuffer, "glBindBuffer", PFNGLBINDBUFFERPROC);
        GET_PROC_ADDRESS(glUniformBlockBinding, "glUniformBlockBinding", PFNGLUNIFORMBLOCKBINDINGPROC);

    #if !defined(GL_ES_VERSION_2_0)
        GET_PROC_ADDRESS(glGetStringi,"glGetStringi",PFNGLGETSTRINGIPROC);
        GET_PROC_ADDRESS(glGenVertexArrays, "glGenVertexArrays", PFNGLGENVERTEXARRAYSPROC);
        GET_PROC_ADDRESS(glBindVertexArray, "glBindVertexArray", PFNGLBINDVERTEXARRAYPROC);
        GET_PROC_ADDRESS(glDrawBuffers, "glDrawBuffers", PFNGLDRAWBUFFERSPROC);
        GET_PROC_ADDRESS(glGetFragDataLocation, "glGetFragDataLocation", PFNGLGETFRAGDATALOCATIONPROC);
        GET_PROC_ADDRESS(glBindFragDataLocation, "glBindFragDataLocation", PFNGLBINDFRAGDATALOCATIONPROC);
        GET_PROC_ADDRESS(glDrawArraysInstanced, "glDrawArraysInstanced", PFNGLDRAWARRAYSINSTANCEDPROC);
        GET_PROC_ADDRESS(glDrawElementsInstanced, "glDrawElementsInstanced", PFNGLDRAWELEMENTSINSTANCEDPROC);
        GET_PROC_ADDRESS(glVertexAttribDivisor, "glVertexAttribDivisor", PFNGLVERTEXATTRIBDIVISORPROC);
    #endif

    #undef GET_PROC_ADDRESS
#endif

        context->m_IsGles3Version = 1; // 0 == gles 2, 1 == gles 3
        context->m_PipelineState  = GetDefaultPipelineState();

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__) || defined(DM_GRAPHICS_USE_OPENGLES)
        context->m_IsShaderLanguageGles = 1;

        const char* version = (char *) glGetString(GL_VERSION);
        if (strstr(version, "OpenGL ES 2.") != 0) {
            context->m_IsGles3Version = 0;
        } else {
            context->m_IsGles3Version = 1;
        }
#else
    #if defined(DM_PLATFORM_IOS)
        // iOS
        context->m_IsGles3Version = 1;
        context->m_IsShaderLanguageGles = 1;
    #else
        context->m_IsGles3Version = 1;
        context->m_IsShaderLanguageGles = 0;
    #endif
#endif

#if defined(__EMSCRIPTEN__)
        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE emscripten_ctx = emscripten_webgl_get_current_context();
        assert(emscripten_ctx != 0 && "Unable to get GL context from emscripten.");

        // These are all the available official webgl extensions, taken from this list:
        // https://developer.mozilla.org/en-US/docs/Web/API/WebGL_API/Using_Extensions
        emscripten_webgl_enable_extension(emscripten_ctx, "ANGLE_instanced_arrays");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_blend_minmax");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_color_buffer_float");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_color_buffer_half_float");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_disjoint_timer_query");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_float_blend");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_frag_depth");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_shader_texture_lod");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_sRGB");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_texture_compression_bptc");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_texture_compression_rgtc");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_texture_filter_anisotropic");
        emscripten_webgl_enable_extension(emscripten_ctx, "EXT_texture_norm16");
        emscripten_webgl_enable_extension(emscripten_ctx, "KHR_parallel_shader_compile");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_element_index_uint");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_fbo_render_mipmap");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_standard_derivatives");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_texture_float");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_texture_float_linear");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_texture_half_float");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_texture_half_float_linear");
        emscripten_webgl_enable_extension(emscripten_ctx, "OES_vertex_array_object");
        emscripten_webgl_enable_extension(emscripten_ctx, "OVR_multiview2");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_color_buffer_float");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_compressed_texture_astc");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_compressed_texture_etc");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_compressed_texture_etc1");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_compressed_texture_pvrtc");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_compressed_texture_s3tc");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_compressed_texture_s3tc_srgb");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_debug_renderer_info");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_debug_shaders");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_depth_texture");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_draw_buffers");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_lose_context");
        emscripten_webgl_enable_extension(emscripten_ctx, "WEBGL_multi_draw");
#endif

#if defined(DM_PLATFORM_MACOS)
        ProcessSerialNumber psn;
        OSErr err;

        // Move window to front. Required if running without application bundle.
        err = GetCurrentProcess( &psn );
        if (err == noErr)
            (void) SetFrontProcess( &psn );
#endif


    #if !(defined(__EMSCRIPTEN__) || defined(GL_ES_VERSION_2_0))
        GLint n;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        CHECK_GL_ERROR;

        if (n > 0)
        {
            int max_len = 0;
            int cursor = 0;

            for (GLint i = 0; i < n; i++)
            {
                char* ext = (char*) glGetStringi(GL_EXTENSIONS,i);
                max_len += (int) strlen((const char*)ext) + 1; // name + space
            }

            char* extensions_ptr = (char*) malloc(max_len);

            for (GLint i = 0; i < n; i++)
            {
                char* ext = (char*) glGetStringi(GL_EXTENSIONS,i);
                int str_len = (int) strlen((const char*)ext);

                strcpy(extensions_ptr + cursor, ext);

                cursor += str_len;
                extensions_ptr[cursor] = ' ';

                cursor += 1;
            }

            extensions_ptr[max_len-1] = 0;

            const GLubyte* extensions = (const GLubyte*) extensions_ptr;
            StoreExtensions(context, extensions);
            free(extensions_ptr);
        }
    #else
        const GLubyte* extensions = glGetString(GL_EXTENSIONS);
        assert(extensions);
        StoreExtensions(context, extensions);
    #endif

    #define DMGRAPHICS_GET_PROC_ADDRESS_EXT(function, name, extension_name, core_name, type, context)\
        if (function == 0x0)\
            function = (type) GetExtProcAddress(name, extension_name, core_name, context);

        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glInvalidateFramebuffer,   "glDiscardFramebuffer", "discard_framebuffer", "glInvalidateFramebuffer", DM_PFNGLINVALIDATEFRAMEBUFFERPROC, context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glDrawBuffers,             "glDrawBuffers",        "draw_buffers",        "glDrawBuffers",           DM_PFNGLDRAWBUFFERSPROC, context);
    #ifdef ANDROID
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glTexSubImage3D,           "glTexSubImage3D",           "texture_array",           "glTexSubImage3D",           DM_PFNGLTEXSUBIMAGE3DPROC,           context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glTexImage3D,              "glTexImage3D",              "texture_array",           "glTexImage3D",              DM_PFNGLTEXIMAGE3DPROC,              context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glCompressedTexSubImage3D, "glCompressedTexSubImage3D", "texture_array",           "glCompressedTexSubImage3D", DM_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC, context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glCompressedTexImage3D,    "glCompressedTexImage3D",    "texture_array",           "glCompressedTexImage3D",    DM_PFNGLCOMPRESSEDTEXIMAGE3DPROC,    context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glDrawArraysInstanced,     "glDrawArraysInstanced",     NULL,                      "glDrawArraysInstanced",     DM_PFNGLDRAWARRAYSINSTANCEDPROC,     context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glDrawElementsInstanced,   "glDrawElementsInstanced",   NULL,                      "glDrawElementsInstanced",   DM_PFNGLDRAWELEMENTSINSTANCEDPROC,   context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glVertexAttribDivisor,     "glVertexAttribDivisor",     NULL,                      "glVertexAttribDivisor",     DM_PFNGLVERTEXATTRIBDIVISORPROC,     context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glMemoryBarrier,               "glMemoryBarrier",           "shader_image_load_store", "glMemoryBarrier",           DM_PFNGLMEMORYBARRIERPROC,           context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glBindImageTexture,            "glBindImageTexture",        "shader_image_load_store", "glBindImageTexture",        DM_PFNGLBINDIMAGETEXTUREPROC,        context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glDispatchCompute,             "glDispatchCompute",         "compute_shader",          "glDispatchCompute",         DM_PFNGLDISPATCHCOMPUTEPROC,         context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glBindBufferBase,              "glBindBufferBase",           "",                       "glBindBufferBase",          DM_PFNGLBINDBUFFERBASEPROC,          context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glGetUniformBlockIndex,        "glGetUniformBlockIndex",     "",                       "glGetUniformBlockIndex",    DM_PFNGLGETUNIFORMBLOCKINDEXPROC,    context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glGetActiveUniformBlockiv,     "glGetActiveUniformBlockiv",  "",                       "glGetActiveUniformBlockiv", DM_PFNGLGETACTIVEUNIFORMBLOCKIVPROC, context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glGetActiveUniformsiv,         "glGetActiveUniformsiv",      "",                       "glGetActiveUniformsiv",     DM_PFNGLGETACTIVEUNIFORMSIVPROC,     context);
        DMGRAPHICS_GET_PROC_ADDRESS_EXT(glUniformBlockBinding,         "glUniformBlockBinding",      "",                       "glUniformBlockBinding",     DM_PFNGLUNIFORMBLOCKBINDINGPROC,     context);
    #endif

    #undef DMGRAPHICS_GET_PROC_ADDRESS_EXT

        if (OpenGLIsExtensionSupported(context, "GL_IMG_texture_compression_pvrtc") ||
            OpenGLIsExtensionSupported(context, "WEBGL_compressed_texture_pvrtc"))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        }

        if (OpenGLIsExtensionSupported(context, "GL_OES_compressed_ETC1_RGB8_texture") ||
            OpenGLIsExtensionSupported(context, "WEBGL_compressed_texture_etc") ||
            OpenGLIsExtensionSupported(context, "WEBGL_compressed_texture_etc1"))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
        }

        // https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt
        if (OpenGLIsExtensionSupported(context, "GL_EXT_texture_compression_s3tc") ||
            OpenGLIsExtensionSupported(context, "WEBGL_compressed_texture_s3tc"))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_BC1; // DXT1
            // We'll use BC3 for this
            //context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_BC2; // DXT3
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_BC3; // DXT5
        }

        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_compression_rgtc.txt
        if (OpenGLIsExtensionSupported(context, "GL_ARB_texture_compression_rgtc") ||
            OpenGLIsExtensionSupported(context, "GL_EXT_texture_compression_rgtc") ||
            OpenGLIsExtensionSupported(context, "EXT_texture_compression_rgtc"))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_R_BC4;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RG_BC5;
        }

        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_texture_compression_bptc.txt
        if (OpenGLIsExtensionSupported(context, "GL_ARB_texture_compression_bptc") ||
            OpenGLIsExtensionSupported(context, "GL_EXT_texture_compression_bptc") ||
            OpenGLIsExtensionSupported(context, "EXT_texture_compression_bptc") )
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_BC7;
        }

        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_ES3_compatibility.txt
        if (OpenGLIsExtensionSupported(context, "GL_ARB_ES3_compatibility"))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_ETC2;
        }

        // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_ES3_compatibility.txt
        if (OpenGLIsExtensionSupported(context, "GL_KHR_texture_compression_astc_ldr") ||
            OpenGLIsExtensionSupported(context, "GL_OES_texture_compression_astc") ||
            OpenGLIsExtensionSupported(context, "OES_texture_compression_astc") ||
            OpenGLIsExtensionSupported(context, "WEBGL_compressed_texture_astc"))
        {
            context->m_ASTCSupport = 1;
            context->m_ASTCArrayTextureSupport = 1;
        }

        // Check if we're using a recent enough OpenGL version
        if (context->m_IsGles3Version)
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB16F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB32F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA16F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA32F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_R16F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RG16F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_R32F;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RG32F;

            context->m_InstancingSupport = 1;

        #ifdef ANDROID
            context->m_InstancingSupport &= PFN_glDrawArraysInstanced   != 0;
            context->m_InstancingSupport &= PFN_glDrawElementsInstanced != 0;
            context->m_InstancingSupport &= PFN_glVertexAttribDivisor   != 0;
        #endif
        }
        else
        {
            // https://registry.khronos.org/OpenGL/extensions/EXT/EXT_color_buffer_half_float.txt
            if (OpenGLIsExtensionSupported(context, "EXT_color_buffer_half_float"))
            {
                context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB16F;
                context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA16F;
            }

            // https://registry.khronos.org/webgl/extensions/WEBGL_color_buffer_float/
            if (OpenGLIsExtensionSupported(context, "WEBGL_color_buffer_float"))
            {
                context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB32F;
                context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA32F;
            }

            // https://registry.khronos.org/webgl/extensions/ANGLE_instanced_arrays/
            if (OpenGLIsExtensionSupported(context, "ANGLE_instanced_arrays"))
            {
                context->m_InstancingSupport = 1;
            }
        }

        // GL_NUM_COMPRESSED_TEXTURE_FORMATS is deprecated in newer OpenGL Versions
        GLint iNumCompressedFormats = 0;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iNumCompressedFormats);
        if (iNumCompressedFormats > 0)
        {
            GLint *pCompressedFormats = new GLint[iNumCompressedFormats];
            glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, pCompressedFormats);
            bool isPagedASTCSupported = true;
            #if defined (__EMSCRIPTEN__)
            // Workaround for some old phones which don't work with ASTC in glCompressedTexImage3D
            // see https://github.com/defold/defold/issues/8030
            // and https://github.com/defold/defold/issues/11009
            if (context->m_IsGles3Version && OpenGLIsTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_4X4)) {
                unsigned char fakeZeroBuffer[] = {
                    0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    
                    0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF

                };
                GLuint texture;
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
                DMGRAPHICS_COMPRESSED_TEX_IMAGE_3D(GL_TEXTURE_2D_ARRAY, 0, DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_4x4_KHR, 4, 4, 2, 0, 32, &fakeZeroBuffer);
                GLint err = glGetError();
                if (err != 0)
                {
                    // Only disable ASTC for array textures; keep 2D ASTC enabled
                    context->m_ASTCArrayTextureSupport = 0;
                    isPagedASTCSupported = false;
                }
                glDeleteTextures(1, &texture);
            }
            #endif
            for (int i = 0; i < iNumCompressedFormats; i++)
            {
                // If 4x4 is supported, all ASTC formats should be supported.
                if (pCompressedFormats[i] == DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_4x4_KHR)
                {
                    context->m_ASTCSupport = 1;
                    if (isPagedASTCSupported)
                        context->m_ASTCArrayTextureSupport = 1;
                }
                else
                {
                    switch (pCompressedFormats[i])
                    {
                        #define CASE(_NAME1,_NAME2) case _NAME1 : context->m_TextureFormatSupport |= 1 << _NAME2; break;
                        CASE(DMGRAPHICS_TEXTURE_FORMAT_RGBA8_ETC2_EAC, TEXTURE_FORMAT_RGBA_ETC2);
                        CASE(DMGRAPHICS_TEXTURE_FORMAT_R11_EAC, TEXTURE_FORMAT_R_ETC2);
                        CASE(DMGRAPHICS_TEXTURE_FORMAT_RG11_EAC, TEXTURE_FORMAT_RG_ETC2);
                        #undef CASE
                    default: break;
                    }
                }
            }
            delete[] pCompressedFormats;
        }


#if defined (__EMSCRIPTEN__)
        // webgl GL_DEPTH_STENCIL_ATTACHMENT for stenciling and GL_DEPTH_COMPONENT16 for depth only by specifications, even though it reports 24-bit depth and no packed depth stencil extensions.
        context->m_PackedDepthStencilSupport = 1;
        context->m_DepthBufferBits = 16;
#else

    #if defined(__MACH__)
        context->m_PackedDepthStencilSupport = 1;
    #endif

        if ((OpenGLIsExtensionSupported(context, "GL_OES_packed_depth_stencil")) || (OpenGLIsExtensionSupported(context, "GL_EXT_packed_depth_stencil")))
        {
            context->m_PackedDepthStencilSupport = 1;
        }
        GLint depth_buffer_bits;
        glGetIntegerv( GL_DEPTH_BITS, &depth_buffer_bits );
        if (GL_INVALID_ENUM == glGetError())
        {
            depth_buffer_bits = 24;
        }

        context->m_DepthBufferBits = (uint32_t) depth_buffer_bits;
#endif

        GLint gl_max_texture_size = 1024;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);
        context->m_MaxTextureSize = gl_max_texture_size;
        CLEAR_GL_ERROR;

#if defined(DM_PLATFORM_IOS) || defined(ANDROID)
        // Hardcoded values for iOS and Android for now. The value is a hint, max number of vertices will still work with performance penalty
        // The below seems to be the reported sweet spot for modern or semi-modern hardware
        context->m_MaxElementVertices = 1024*1024;
#else
        // We don't accept values lower than 65k. It's a trade-off on drawcalls vs bufferdata upload
        GLint gl_max_elem_verts = 65536;
        bool legacy = context->m_IsGles3Version == 0;
        if (!legacy) {
            glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_max_elem_verts);
        }
        context->m_MaxElementVertices = dmMath::Max(65536, gl_max_elem_verts);
        CLEAR_GL_ERROR;

        GLint gl_max_elem_indices = 65536;
        if (!legacy) {
            glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_max_elem_indices);
        }
        CLEAR_GL_ERROR;
#endif

        if (OpenGLIsExtensionSupported(context, "GL_OES_compressed_ETC1_RGB8_texture"))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
        }

        if (OpenGLIsExtensionSupported(context, "GL_EXT_texture_filter_anisotropic"))
        {
            context->m_AnisotropySupport = 1;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &context->m_MaxAnisotropy);
        }

        if (context->m_IsGles3Version || OpenGLIsExtensionSupported(context, "GL_EXT_texture_array"))
        {
            context->m_TextureArraySupport         = 1;
            context->m_MultiTargetRenderingSupport = 1;

        #ifdef ANDROID
            context->m_TextureArraySupport &= PFN_glTexSubImage3D           != 0;
            context->m_TextureArraySupport &= PFN_glTexImage3D              != 0;
            context->m_TextureArraySupport &= PFN_glCompressedTexSubImage3D != 0;
            context->m_TextureArraySupport &= PFN_glCompressedTexImage3D    != 0;
        #endif

            // 3D Textures uses the same functions as array textures
            context->m_3DTextureSupport = context->m_TextureArraySupport;
        }

#if defined(__ANDROID__) || defined(__arm__) || defined(__arm64__) || defined(__EMSCRIPTEN__) || defined(DM_GRAPHICS_USE_OPENGLES)
        if (OpenGLIsExtensionSupported(context, "GL_OES_element_index_uint") ||
            OpenGLIsExtensionSupported(context, "OES_element_index_uint"))
        {
            context->m_IndexBufferFormatSupport |= 1 << INDEXBUFFER_FORMAT_32;
        }

    #if !defined(__EMSCRIPTEN__)
        // Note: This is enabled automatically for WebGL2, and the defined value is not available **at all** on WebGL1,
        //       so if we don't want to do crazy workarounds for WebGL let's just ignore it and move on.
        if (!context->m_IsGles3Version && OpenGLIsExtensionSupported(context, "GL_ARB_seamless_cube_map"))
        {
            glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
            CLEAR_GL_ERROR;
        }
    #endif
#else
        context->m_IndexBufferFormatSupport |= 1 << INDEXBUFFER_FORMAT_32;

        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
        CLEAR_GL_ERROR;
#endif

    #ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT
        int32_t version_major = 0, version_minor = 0;
        glGetIntegerv(DMGRAPHICS_MAJOR_VERSION, &version_major);
        glGetIntegerv(DMGRAPHICS_MINOR_VERSION, &version_minor);

        #define COMPUTE_VERSION_NEEDED(MAJOR, MINOR) (MAJOR > version_major || (version_major ==  MAJOR && version_minor >= MINOR))

        #if defined(GL_ES_VERSION_3_0) || defined(GL_ES_VERSION_2_0)
            context->m_ComputeSupport = COMPUTE_VERSION_NEEDED(3,1);
        #else
            context->m_ComputeSupport = COMPUTE_VERSION_NEEDED(4,3);
        #endif

        context->m_ComputeSupport &= glDispatchCompute  != 0;
        context->m_ComputeSupport &= glMemoryBarrier    != 0;
        context->m_ComputeSupport &= glBindImageTexture != 0;

        #undef COMPUTE_VERSION_NEEDED
    #endif

        if (context->m_PrintDeviceInfo)
        {
            OpenGLPrintDeviceInfo(context);
        }

        context->m_AsyncProcessingSupport = dmThread::PlatformHasThreadSupport() && dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_AUX_CONTEXT);
        if (context->m_AsyncProcessingSupport)
        {
            AcquireAuxContextOnThread(context, true);

            InitializeSetTextureAsyncState(context->m_SetTextureAsyncState);

            context->m_GLHandlesData.m_Mutex = dmMutex::New();
            context->m_AssetHandleContainerMutex = dmMutex::New();

            if (context->m_JobThread == 0x0)
            {
                dmLogError("AsyncInitialize: Platform has async support but no job thread. Fallback to single thread processing.");
                context->m_AsyncProcessingSupport = 0;
            }
            else if(!ValidateAsyncJobProcessing(context))
            {
                dmLogDebug("AsyncInitialize: Failed to verify async job processing. Fallback to single thread processing.");
                context->m_AsyncProcessingSupport = 0;
            }

            if (!context->m_AsyncProcessingSupport)
            {
                dmMutex::Delete(context->m_GLHandlesData.m_Mutex);
                context->m_GLHandlesData.m_Mutex = 0;

                dmMutex::Delete(context->m_AssetHandleContainerMutex);
                context->m_AssetHandleContainerMutex = 0;
            }
        }

        if (glGenVertexArrays)
        {
            GLuint handle;
            glGenVertexArrays(1, &handle);
            context->m_GlobalVAO = AddNewGLHandle(_context, handle);
        }

        SetSwapInterval(_context, params.m_SwapInterval);

        return true;
    }

    static dmPlatform::HWindow OpenGLGetWindow(HContext _context)
    {
        assert(_context);
        OpenGLContext* context = (OpenGLContext*) _context;
        return context->m_Window;
    }

    static void OpenGLCloseWindow(HContext _context)
    {
        assert(_context);
        OpenGLContext* context = (OpenGLContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            PostDeleteTextures(context, true);

            context->m_Width = 0;
            context->m_Height = 0;
            context->m_Extensions.SetSize(0);
            free(context->m_ExtensionsString);
            context->m_ExtensionsString = 0;
        }
    }

    static void OpenGLRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        #ifdef __EMSCRIPTEN__
        while (0 != is_running(user_data))
        {
            // N.B. Beyond the first test, the above statement is essentially formal since set_main_loop will throw an exception.
            emscripten_set_main_loop_arg(step_method, user_data, 0, 1);
        }
        #else
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
        #endif
    }

    static PipelineState OpenGLGetPipelineState(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return context->m_PipelineState;
    }

    static uint32_t OpenGLGetDisplayDpi(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        return 0;
    }

    static uint32_t OpenGLGetWidth(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        return context->m_Width;
    }

    static uint32_t OpenGLGetHeight(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        return context->m_Height;
    }

    static void OpenGLSetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        OpenGLContext* context = (OpenGLContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            context->m_Width  = width;
            context->m_Height = height;
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void OpenGLResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        OpenGLContext* context = (OpenGLContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void OpenGLGetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void OpenGLClear(HContext _context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        DM_PROFILE(__FUNCTION__);

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;
        glClearColor(r, g, b, a);
        CHECK_GL_ERROR;

        glClearDepth(depth);
        CHECK_GL_ERROR;

        glClearStencil(stencil);
        CHECK_GL_ERROR;

        GLbitfield gl_flags = (flags & BUFFER_TYPE_COLOR0_BIT)  ? GL_COLOR_BUFFER_BIT   : 0;
        gl_flags           |= (flags & BUFFER_TYPE_DEPTH_BIT)   ? GL_DEPTH_BUFFER_BIT   : 0;
        gl_flags           |= (flags & BUFFER_TYPE_STENCIL_BIT) ? GL_STENCIL_BUFFER_BIT : 0;

        glClear(gl_flags);
        CHECK_GL_ERROR
    }

    static void OpenGLBeginFrame(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
#if defined(ANDROID)
        dmPlatform::AndroidBeginFrame(context->m_Window);
#endif
        glBindFramebuffer(GL_FRAMEBUFFER, dmPlatform::OpenGLGetDefaultFramebufferId());
    }

    static void OpenGLFlip(HContext _context)
    {
        DM_PROFILE(__FUNCTION__);
        OpenGLContext* context = (OpenGLContext*) _context;
        PostDeleteTextures(context, false);
        dmPlatform::SwapBuffers(context->m_Window);
        CHECK_GL_ERROR;
    }

    static GLenum GetOpenGLBufferUsage(BufferUsage buffer_usage)
    {
        const GLenum buffer_usage_lut[] = {
        #if !defined (GL_ARB_vertex_buffer_object)
            0x88E0,
            0x88E4,
            0x88E8,
        #else
            GL_STREAM_DRAW,
            GL_STATIC_DRAW,
            GL_DYNAMIC_DRAW,
        #endif
        };

        return buffer_usage_lut[buffer_usage];
    }

    static HUniformBuffer OpenGLNewUniformBuffer(HContext _context, const UniformBufferLayout& layout)
    {
        OpenGLContext* context = (OpenGLContext*) _context;

        if (!context->m_IsGles3Version)
        {
            dmLogWarning("Uniform buffers are not supported on this OpenGL version.");
            return 0;
        }

        OpenGLUniformBuffer* ubo = new OpenGLUniformBuffer();
        ubo->m_BaseUniformBuffer.m_Layout       = layout;
        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;

        GLuint buffer_handle = 0;
        glGenBuffers(1, &buffer_handle);
        ubo->m_Id = AddNewGLHandle(context, buffer_handle);
        CHECK_GL_ERROR;

        return (HUniformBuffer) ubo;
    }

    static void OpenGLSetUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t offset, uint32_t size, const void* data)
    {
        OpenGLContext* context = (OpenGLContext*)_context;
        OpenGLUniformBuffer* ubo = (OpenGLUniformBuffer*) uniform_buffer;
        assert(offset + size <= ubo->m_BaseUniformBuffer.m_Layout.m_Size);

        GLuint handle = GetGLHandle(context, ubo->m_Id);

        glBindBuffer(GL_UNIFORM_BUFFER, handle);
        if (offset != 0)
        {
            glBufferSubDataARB(GL_UNIFORM_BUFFER, offset, size, data);
        }
        glBufferData(GL_UNIFORM_BUFFER, size, data, GL_STATIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    static void OpenGLDisableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        OpenGLContext* context = (OpenGLContext*)_context;
        OpenGLUniformBuffer* ubo = (OpenGLUniformBuffer*) uniform_buffer;

        if (ubo->m_BaseUniformBuffer.m_BoundSet == UNUSED_BINDING_OR_SET || ubo->m_BaseUniformBuffer.m_BoundBinding == UNUSED_BINDING_OR_SET)
        {
            return;
        }

        if (context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] == ubo)
        {
            context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] = 0;
        }

        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;
    }

    static void OpenGLEnableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t binding, uint32_t set)
    {
        OpenGLContext* context = (OpenGLContext*)_context;
        OpenGLUniformBuffer* ubo = (OpenGLUniformBuffer*) uniform_buffer;

        ubo->m_BaseUniformBuffer.m_BoundBinding = binding;
        ubo->m_BaseUniformBuffer.m_BoundSet     = set;

        if (context->m_CurrentUniformBuffers[set][binding])
        {
            OpenGLDisableUniformBuffer(context, (HUniformBuffer) context->m_CurrentUniformBuffers[set][binding]);
        }

        context->m_CurrentUniformBuffers[set][binding] = ubo;
    }

    static void OpenGLDeleteUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        OpenGLContext* context = (OpenGLContext*)_context;
        OpenGLUniformBuffer* ubo = (OpenGLUniformBuffer*) uniform_buffer;

        OpenGLDisableUniformBuffer(_context, uniform_buffer);
        delete ubo;
    }

    static HVertexBuffer OpenGLNewVertexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLBuffer* vertex_buffer = new OpenGLBuffer();
        vertex_buffer->m_MemorySize = size;
        GLuint handle = 0;
        glGenBuffersARB(1, &handle);
        vertex_buffer->m_Id = AddNewGLHandle(context, handle);
        CHECK_GL_ERROR;
        SetVertexBufferData((HVertexBuffer) vertex_buffer, size, data, buffer_usage);
        return (HVertexBuffer) vertex_buffer;
    }

    static void OpenGLDeleteVertexBuffer(HVertexBuffer buffer)
    {
        if (!buffer)
        {
            return;
        }
        OpenGLBuffer* vertex_buffer = (OpenGLBuffer*) buffer;
        GLuint handle = GetGLHandle(g_Context, vertex_buffer->m_Id);
        glDeleteBuffersARB(1, &handle);
        CleanupGLHandle(g_Context, vertex_buffer->m_Id);
        CHECK_GL_ERROR;
        delete vertex_buffer;
    }

    static void OpenGLSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);
        // NOTE: Android doesn't seem to like zero-sized vertex buffers
        if (size == 0)
        {
            return;
        }
        OpenGLBuffer* vertex_buffer = (OpenGLBuffer*) buffer;
        vertex_buffer->m_MemorySize = size;
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, GetGLHandle(g_Context, vertex_buffer->m_Id));
        CHECK_GL_ERROR
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, size, data, GetOpenGLBufferUsage(buffer_usage));
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR;
    }

    static void OpenGLSetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        if (!buffer)
        {
            return;
        }
        OpenGLBuffer* vertex_buffer = (OpenGLBuffer*) buffer;
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, GetGLHandle(g_Context, vertex_buffer->m_Id));
        CHECK_GL_ERROR;
        glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, offset, size, data);
        CHECK_GL_ERROR;
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR;
    }

    static uint32_t OpenGLGetVertexBufferSize(HVertexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        OpenGLBuffer* vertex_buffer = (OpenGLBuffer*) buffer;
        return vertex_buffer->m_MemorySize;
    }

    static uint32_t OpenGLGetMaxElementsVertices(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return context->m_MaxElementVertices;
    }

    static void OpenGLSetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);
        // NOTE: WebGl doesn't seem to like zero-sized vertex buffers (very poor performance)
        if (size == 0 || !buffer)
        {
            return;
        }

        OpenGLBuffer* index_buffer = (OpenGLBuffer*) buffer;
        index_buffer->m_MemorySize = size;

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GetGLHandle(g_Context, index_buffer->m_Id));
        CHECK_GL_ERROR
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, size, data, GetOpenGLBufferUsage(buffer_usage));
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    static HIndexBuffer OpenGLNewIndexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLBuffer* index_buffer = new OpenGLBuffer;
        GLuint handle = 0;
        glGenBuffersARB(1, &handle);
        index_buffer->m_Id = AddNewGLHandle(context, handle);
        CHECK_GL_ERROR
        OpenGLSetIndexBufferData((HIndexBuffer) index_buffer, size, data, buffer_usage);
        index_buffer->m_MemorySize = size;
        return (HIndexBuffer) index_buffer;
    }

    static void OpenGLDeleteIndexBuffer(HIndexBuffer buffer)
    {
        if (!buffer)
        {
            return;
        }

        OpenGLBuffer* index_buffer = (OpenGLBuffer*) buffer;
        GLuint handle = GetGLHandle(g_Context, index_buffer->m_Id);
        glDeleteBuffersARB(1, &handle);
        CleanupGLHandle(g_Context, index_buffer->m_Id);
        CHECK_GL_ERROR;
        delete index_buffer;
    }

    static void OpenGLSetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        if (!buffer)
        {
            return;
        }
        DM_PROFILE(__FUNCTION__);
        OpenGLBuffer* index_buffer = (OpenGLBuffer*) buffer;
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GetGLHandle(g_Context, index_buffer->m_Id));
        CHECK_GL_ERROR;
        glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, size, data);
        CHECK_GL_ERROR;
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR;
    }

    static uint32_t OpenGLGetIndexBufferSize(HIndexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        OpenGLBuffer* index_buffer = (OpenGLBuffer*) buffer;
        return index_buffer->m_MemorySize;
    }

    static bool OpenGLIsIndexBufferFormatSupported(HContext _context, IndexBufferFormat format)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return (context->m_IndexBufferFormatSupport & (1 << format)) != 0;
    }

    // NOTE: This function doesn't seem to be used anywhere?
    static uint32_t OpenGLGetMaxElementsIndices(HContext _context)
    {
        return 0;
    }

    static HVertexDeclaration OpenGLNewVertexDeclarationStride(HContext _context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        HVertexDeclaration vd = NewVertexDeclaration(context, stream_declaration);
        vd->m_Stride = stride;
        return vd;
    }

    static HVertexDeclaration OpenGLNewVertexDeclaration(HContext _context, HVertexStreamDeclaration stream_declaration)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        VertexDeclaration* vd = new VertexDeclaration;
        memset(vd, 0, sizeof(VertexDeclaration));

        vd->m_Stride = 0;
        for (uint32_t i = 0; i < stream_declaration->m_StreamCount; i++)
        {
            vd->m_Streams[i].m_NameHash  = stream_declaration->m_Streams[i].m_NameHash;
            vd->m_Streams[i].m_Location  = -1;
            vd->m_Streams[i].m_Size      = stream_declaration->m_Streams[i].m_Size;
            vd->m_Streams[i].m_Type      = stream_declaration->m_Streams[i].m_Type;
            vd->m_Streams[i].m_Normalize = stream_declaration->m_Streams[i].m_Normalize;
            vd->m_Streams[i].m_Offset    = vd->m_Stride;

            vd->m_Stride += stream_declaration->m_Streams[i].m_Size * GetTypeSize(stream_declaration->m_Streams[i].m_Type);
        }
        vd->m_StreamCount = stream_declaration->m_StreamCount;
        vd->m_StepFunction = stream_declaration->m_StepFunction;
        return vd;
    }

    static void BindVertexDeclarationProgram(HContext _context, HVertexDeclaration vertex_declaration, HProgram program)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLProgram* program_ptr = (OpenGLProgram*) program;
        uint32_t n = vertex_declaration->m_StreamCount;
        VertexDeclaration::Stream* streams = &vertex_declaration->m_Streams[0];
        for (uint32_t i = 0; i < n; i++)
        {
            int32_t location = -1;
            for (int j = 0; j < program_ptr->m_Attributes.Size(); ++j)
            {
                if (program_ptr->m_Attributes[j].m_NameHash == streams[i].m_NameHash)
                {
                    location = program_ptr->m_Attributes[j].m_Location;
                    break;
                }
            }

            if (location != -1)
            {
                streams[i].m_Location = location;
            }
            else
            {
                CLEAR_GL_ERROR
                // TODO: Disabled irritating warning? Should we care about not used streams?
                //dmLogWarning("Vertex attribute %s is not active or defined", streams[i].m_Name);
                streams[i].m_Location = -1;
            }
        }

        vertex_declaration->m_BoundForProgram     = program;
        vertex_declaration->m_ModificationVersion = context->m_ModificationVersion;
    }

    static void OpenGLEnableVertexBuffer(HContext _context, HVertexBuffer buffer, uint32_t binding_index)
    {
        OpenGLBuffer* vertex_buffer = (OpenGLBuffer*) buffer;
        OpenGLContext* context = (OpenGLContext*) _context;
        glBindBufferARB(GL_ARRAY_BUFFER, GetGLHandle(context, vertex_buffer->m_Id));
        CHECK_GL_ERROR;
    }

    static void OpenGLDisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
        // NOP
    }

    static inline uint32_t GetSubVectorBindCount(uint32_t size)
    {
        // Not sure if this supports mat2?
        if (size == 9)       return 3;
        else if (size == 16) return 4;
        return 1;
    }

    static inline void SetVertexAttribute(OpenGLContext* context, int32_t loc, uint32_t component_count, GLenum opengl_type, bool normalize, uint32_t stride, uint32_t offset, VertexStepFunction step_function)
    {
    #if 0
        dmLogInfo("Attribute: %d, %d, %d, %d, %d, %d", loc, component_count, opengl_type, normalize, stride, offset);
    #endif

        // TODO: We should cache these bindings and skip applying them if they haven't changed
        glEnableVertexAttribArray(loc);
        CHECK_GL_ERROR;

        glVertexAttribPointer(
            loc,
            component_count,
            opengl_type,
            normalize,
            stride,
            (GLvoid*) (size_t) offset); // The starting point of the VBO, for the vertices
        CHECK_GL_ERROR;

        if (context->m_InstancingSupport)
        {
            glVertexAttribDivisor(loc, step_function);
            CHECK_GL_ERROR;
        }
    }

    static void OpenGLEnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        assert(_context);
        assert(vertex_declaration);

        OpenGLContext* context = (OpenGLContext*) _context;

        if (glBindVertexArray)
            glBindVertexArray(GetGLHandle(context, context->m_GlobalVAO));

        if (context->m_ModificationVersion != vertex_declaration->m_ModificationVersion || vertex_declaration->m_BoundForProgram != program)
        {
            BindVertexDeclarationProgram(context, vertex_declaration, program);
        }

        for (uint32_t i = 0; i < vertex_declaration->m_StreamCount; i++)
        {
            if (vertex_declaration->m_Streams[i].m_Location != -1)
            {
                uint32_t sub_vector_bind_count = GetSubVectorBindCount(vertex_declaration->m_Streams[i].m_Size);
                uint32_t base_location = vertex_declaration->m_Streams[i].m_Location;
                uint32_t stream_offset = base_offset + vertex_declaration->m_Streams[i].m_Offset;
                uint32_t sub_vector_component_count = sub_vector_bind_count > 1 ? sub_vector_bind_count : vertex_declaration->m_Streams[i].m_Size;

                for (uint32_t j = 0; j < sub_vector_bind_count; ++j)
                {
                    SetVertexAttribute(
                        context,
                        base_location + j,
                        sub_vector_component_count,
                        GetOpenGLType(vertex_declaration->m_Streams[i].m_Type),
                                      vertex_declaration->m_Streams[i].m_Normalize,
                                      vertex_declaration->m_Stride,
                                      stream_offset + j * GetTypeSize(vertex_declaration->m_Streams[i].m_Type) * sub_vector_component_count,
                                      vertex_declaration->m_StepFunction);
                }
            }
        }
    }

    static void OpenGLDisableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        assert(vertex_declaration);

        for (uint32_t i = 0; i < vertex_declaration->m_StreamCount; i++)
        {
            if (vertex_declaration->m_Streams[i].m_Location != -1)
            {
                uint32_t sub_vector_count = GetSubVectorBindCount(vertex_declaration->m_Streams[i].m_Size);
                int32_t base_location = vertex_declaration->m_Streams[i].m_Location;

                for (int j = 0; j < sub_vector_count; ++j)
                {
                    glDisableVertexAttribArray(base_location + j);
                    CHECK_GL_ERROR;
                }
            }
        }

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR;

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR;
    }

    static void DrawSetup(OpenGLContext* context)
    {
        OpenGLProgram* program = context->m_CurrentProgram;

        if (!context->m_IsGles3Version)
            return;

        for (int i = 0; i < program->m_UniformBuffers.Size(); ++i)
        {
            OpenGLScratchUniformBuffer& ubo = program->m_UniformBuffers[i];
            OpenGLUniformBuffer* bound_ubo = context->m_CurrentUniformBuffers[ubo.m_ResourceSet][ubo.m_ResourceBinding];

            if (bound_ubo)
            {
                ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[ubo.m_ResourceSet][ubo.m_ResourceBinding];
                UniformBufferLayout* pgm_layout = (UniformBufferLayout*) pgm_res.m_BindingUserData;

                if (bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash != pgm_layout->m_Hash)
                {
                    dmLogWarning("Uniform buffer with hash %d has an incompatible layout with the currently bound program at the shader binding '%s' (hash=%d)",
                        bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash,
                        pgm_res.m_Res->m_Name,
                        pgm_layout->m_Hash);

                    // Fallback to the scratch buffer uniform setup
                    bound_ubo = 0;
                }
            }

            if (bound_ubo)
            {
                glBindBufferBase(GL_UNIFORM_BUFFER, ubo.m_BindPoint, GetGLHandle(context, bound_ubo->m_Id));
                CHECK_GL_ERROR;
            }
            else
            {
                if (ubo.m_ActiveUniforms > 0)
                {
                    glBindBufferBase(GL_UNIFORM_BUFFER, ubo.m_BindPoint, GetGLHandle(context, ubo.m_Id));
                    CHECK_GL_ERROR;

                    if (ubo.m_Dirty > 0)
                    {
                        glBindBuffer(GL_UNIFORM_BUFFER, GetGLHandle(context, ubo.m_Id));
                        CHECK_GL_ERROR;
                        glBufferData(GL_UNIFORM_BUFFER, ubo.m_BlockSize, ubo.m_BlockMemory, GL_STATIC_DRAW);
                        CHECK_GL_ERROR;
                        ubo.m_Dirty = false;
                    }
                }
            }
        }
    }

    static void OpenGLDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer buffer, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        assert(buffer);

        OpenGLBuffer* index_buffer = (OpenGLBuffer*) buffer;
        OpenGLContext* context = (OpenGLContext*) _context;

        DrawSetup(context);

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, GetGLHandle(context, index_buffer->m_Id));
        CHECK_GL_ERROR;

        if (context->m_InstancingSupport)
        {
            glDrawElementsInstanced(GetOpenGLPrimitiveType(prim_type), count, GetOpenGLType(type), (GLvoid*)(uintptr_t) first, dmMath::Max((uint32_t) 1, instance_count));
            CHECK_GL_ERROR;
        }
        else
        {
            glDrawElements(GetOpenGLPrimitiveType(prim_type), count, GetOpenGLType(type), (GLvoid*)(uintptr_t) first);
            CHECK_GL_ERROR
        }
    }

    static void OpenGLDraw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);
        assert(context);

        DrawSetup(context);

        if (context->m_InstancingSupport)
        {
            glDrawArraysInstanced(GetOpenGLPrimitiveType(prim_type), first, count, dmMath::Max((uint32_t) 1, instance_count));
            CHECK_GL_ERROR;
        }
        else
        {
            glDrawArrays(GetOpenGLPrimitiveType(prim_type), first, count);
            CHECK_GL_ERROR
        }
    }

    static void OpenGLDispatchCompute(HContext _context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
    #ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT
        OpenGLContext* context = (OpenGLContext*) _context;
        if (context->m_ComputeSupport)
        {
            DM_PROFILE(__FUNCTION__);
            DM_PROPERTY_ADD_U32(rmtp_DispatchCalls, 1);

            DrawSetup(context);

            glDispatchCompute(group_count_x, group_count_y, group_count_z);
            CHECK_GL_ERROR;

            glMemoryBarrier(DMGRAPHICS_BARRIER_BIT_SHADER_IMAGE_ACCESS);
            CHECK_GL_ERROR;
        }
    #endif
    }

    static GLuint DoCreateShader(HContext _context, GLenum type, const void* program, uint32_t program_size, char* error_buffer, uint32_t error_buffer_size)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        GLuint shader_id = glCreateShader(type);
        CHECK_GL_ERROR;
        GLint size = program_size;
        glShaderSource(shader_id, 1, (const GLchar**) &program, &size);
        CHECK_GL_ERROR;
        glCompileShader(shader_id);
        CHECK_GL_ERROR;

        GLint status;
        glGetShaderiv(shader_id, GL_COMPILE_STATUS, &status);
        if (status == 0)
        {
            const char* type_str = "";
            switch(type)
            {
                case GL_VERTEX_SHADER:
                    type_str = "vertex";
                    break;
                case GL_FRAGMENT_SHADER:
                    type_str = "fragment";
                    break;
                case DMGRAPHICS_TYPE_COMPUTE_SHADER:
                    type_str = "compute";
                    break;
                default:
                    break;
            }

            char* log_str = 0;

#ifndef NDEBUG
            GLint logLength;
            glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                log_str = (GLchar *)malloc(logLength);
                glGetShaderInfoLog(shader_id, logLength, &logLength, log_str);
            }
#endif
            if (error_buffer)
            {
                dmSnPrintf(error_buffer, error_buffer_size, "Unable to compile %s shader.\nError: %s", type_str, log_str == 0 ? "Unknown" : log_str);
            }
            if (log_str)
            {
                free(log_str);
            }
            glDeleteShader(shader_id);
            return 0;
        }

        return shader_id;
    }

    static OpenGLShader* CreateShader(HContext _context, GLenum type, ShaderDesc::Shader* ddf_shader, char* error_buffer, uint32_t error_buffer_size)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        GLuint shader_id = DoCreateShader(context, type, ddf_shader->m_Source.m_Data, ddf_shader->m_Source.m_Count, error_buffer, error_buffer_size);
        if (!shader_id)
        {
            return 0;
        }
        OpenGLShader* shader = new OpenGLShader();
        shader->m_Id         = AddNewGLHandle(context, shader_id);
        shader->m_Language   = ddf_shader->m_Language;

        switch(type)
        {
            case GL_VERTEX_SHADER:
                shader->m_Stage = SHADER_STAGE_FLAG_VERTEX;
                break;
            case GL_FRAGMENT_SHADER:
                shader->m_Stage = SHADER_STAGE_FLAG_FRAGMENT;
                break;
            case DMGRAPHICS_TYPE_COMPUTE_SHADER:
                shader->m_Stage = SHADER_STAGE_FLAG_COMPUTE;
                break;
            default:
                break;
        }

        return shader;
    }

    static void BuildAttributes(OpenGLProgram* program_ptr)
    {
        GLint num_attributes;
        GLuint program_handle = GetGLHandle(g_Context, program_ptr->m_Id);
        glGetProgramiv(program_handle, GL_ACTIVE_ATTRIBUTES, &num_attributes);
        CHECK_GL_ERROR;

        program_ptr->m_Attributes.SetCapacity(num_attributes);
        program_ptr->m_Attributes.SetSize(num_attributes);

        char attribute_name[256];
        for (int i = 0; i < num_attributes; ++i)
        {
            OpenGLVertexAttribute& attr = program_ptr->m_Attributes[i];
            GLsizei attr_len;
            GLint   attr_size;
            GLenum  attr_type;
            glGetActiveAttrib(program_handle, i,
                sizeof(attribute_name),
                &attr_len,
                &attr_size,
                &attr_type,
                attribute_name);
            CHECK_GL_ERROR;

            attr.m_Location = glGetAttribLocation(program_handle, attribute_name);
            attr.m_NameHash = dmHashString64(attribute_name);
            attr.m_Count    = attr_size;
            attr.m_Type     = attr_type;
            CHECK_GL_ERROR;
        }
    }

    static inline char* GetBaseUniformName(char* str, uint32_t len)
    {
        char* ptr = str;
        for (int i = len - 1; i >= 0; i--)
        {
            // For arrays, OpenGL returns the name as `name[0]`
            if (ptr[i] == '[')
            {
                ptr[i] = 0;
            }
            else if (ptr[i] == '.')
            {
                return &ptr[i+1];
            }
        }

        return str;
    }

    static inline int FindNamespace(char* str, uint32_t len)
    {
        // Look for the first dot in the string
        for (int i = 0; i < len; ++i)
        {
            if (str[i] == '.')
            {
                return i;
            }
        }
        return -1;
    }

    static void BuildUniformBuffers(OpenGLContext* context, OpenGLProgram* program)
    {
        uint32_t num_ubos = program->m_BaseProgram.m_ShaderMeta.m_UniformBuffers.Size();
        uint32_t ubo_binding = 0;
        program->m_UniformBuffers.SetCapacity(num_ubos);
        program->m_UniformBuffers.SetSize(num_ubos);

        memset(program->m_UniformBuffers.Begin(), 0, sizeof(OpenGLScratchUniformBuffer) * num_ubos);

        uint32_t num_uniform_buffers = program->m_BaseProgram.m_ShaderMeta.m_UniformBuffers.Size();
        for (uint32_t j = 0; j < num_uniform_buffers; ++j)
        {
            ShaderResourceBinding& res = program->m_BaseProgram.m_ShaderMeta.m_UniformBuffers[j];

            GLuint program_handle = GetGLHandle(context, program->m_Id);
            GLuint blockIndex = glGetUniformBlockIndex(program_handle, res.m_Name);
            CHECK_GL_ERROR;

            if (blockIndex == GL_INVALID_INDEX)
            {
                continue;
            }

            ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[res.m_Set][res.m_Binding];

            GLint blockSize;
            glGetActiveUniformBlockiv(program_handle, blockIndex, GL_UNIFORM_BLOCK_DATA_SIZE, &blockSize);
            CHECK_GL_ERROR;

            GLint activeUniforms;
            glGetActiveUniformBlockiv(program_handle, blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &activeUniforms);
            CHECK_GL_ERROR;

            OpenGLScratchUniformBuffer& ubo = program->m_UniformBuffers[blockIndex];
            ubo.m_Indices.SetCapacity(activeUniforms);
            ubo.m_Indices.SetSize(activeUniforms);
            ubo.m_Offsets.SetCapacity(activeUniforms);
            ubo.m_Offsets.SetSize(activeUniforms);
            ubo.m_Layout          = (UniformBufferLayout*) pgm_res.m_BindingUserData;
            ubo.m_BindPoint       = ubo_binding++;
            ubo.m_BlockSize       = blockSize;
            ubo.m_ActiveUniforms  = activeUniforms;
            ubo.m_ResourceBinding = res.m_Binding;
            ubo.m_ResourceSet     = res.m_Set;
            ubo.m_BlockMemory     = new uint8_t[ubo.m_BlockSize];
            memset(ubo.m_BlockMemory, 0, ubo.m_BlockSize);

            glGetActiveUniformBlockiv(program_handle, blockIndex, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, ubo.m_Indices.Begin());
            CHECK_GL_ERROR;
            glGetActiveUniformsiv(program_handle, activeUniforms, (GLuint*) ubo.m_Indices.Begin(), GL_UNIFORM_OFFSET, ubo.m_Offsets.Begin());
            CHECK_GL_ERROR;

            // Create a handle for the UBO and link it to the program
            GLuint buffer_handle = 0;
            glGenBuffers(1, &buffer_handle);
            ubo.m_Id = AddNewGLHandle(context, buffer_handle);
            CHECK_GL_ERROR;
            glBindBuffer(GL_UNIFORM_BUFFER, buffer_handle);
            CHECK_GL_ERROR;

            glBufferData(GL_UNIFORM_BUFFER, blockSize, ubo.m_BlockMemory, GL_STATIC_DRAW);
            CHECK_GL_ERROR;

            glBindBufferBase(GL_UNIFORM_BUFFER, ubo.m_BindPoint, buffer_handle);
            CHECK_GL_ERROR;
            glUniformBlockBinding(program_handle, blockIndex, ubo.m_BindPoint);
            CHECK_GL_ERROR;
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            CHECK_GL_ERROR;
        }
    }

    struct BuildCanonicalPathsCallbackContext
    {
        struct PathPointers
        {
            uint32_t m_OffsetCanonicalName     : 31;
            uint32_t m_OffsetCanonicalNameUsed : 1;
            uint32_t m_OffsetNamespace         : 31;
            uint32_t m_OffsetNamespaceUsed     : 1;
            uint32_t m_OffsetInstanceName      : 31;
            uint32_t m_OffsetInstanceNameUsed  : 1;
        };

        char* CanonicalName(uint32_t i)
        {
            if (!m_Paths[i].m_OffsetCanonicalNameUsed)
            {
                return 0;
            }
            return m_Buffer.Begin() + m_Paths[i].m_OffsetCanonicalName;
        }
        char* Namespace(uint32_t i)
        {
            if (!m_Paths[i].m_OffsetNamespaceUsed)
            {
                return 0;
            }
            return m_Buffer.Begin() + m_Paths[i].m_OffsetNamespace;
        }
        char* InstanceName(uint32_t i)
        {
            if (!m_Paths[i].m_OffsetInstanceNameUsed)
            {
                return 0;
            }
            return m_Buffer.Begin() + m_Paths[i].m_OffsetInstanceName;
        }

        dmArray<char>         m_Buffer;
        dmArray<PathPointers> m_Paths;
    };

    static void BuildCanonicalPathsCallback(const CreateUniformLeafMembersCallbackParams& params, void* user_data)
    {
        BuildCanonicalPathsCallbackContext* context = (BuildCanonicalPathsCallbackContext*) user_data;

        BuildCanonicalPathsCallbackContext::PathPointers path_pointers = {};

        uint32_t increase_bytes        = 0;
        uint32_t canonical_name_length = 0;
        uint32_t namespace_length      = 0;
        uint32_t instance_name_length  = 0;

        if (params.m_CanonicalName)
        {
            canonical_name_length = strlen(params.m_CanonicalName);
            increase_bytes += canonical_name_length + 1;
        }
        if (params.m_Namespace)
        {
            namespace_length = strlen(params.m_Namespace);
            increase_bytes += namespace_length + 1;
        }
        if (params.m_InstanceName)
        {
            instance_name_length = strlen(params.m_InstanceName);
            increase_bytes += instance_name_length + 1;
        }

        context->m_Buffer.OffsetCapacity(increase_bytes);
        uint32_t buffer_offset = context->m_Buffer.Size();

        char* write_ptr   = context->m_Buffer.Begin() + buffer_offset;
        char* write_begin = write_ptr;

        if (params.m_CanonicalName)
        {
            memcpy(write_ptr, params.m_CanonicalName, canonical_name_length);
            path_pointers.m_OffsetCanonicalName     = buffer_offset + write_ptr - write_begin;
            path_pointers.m_OffsetCanonicalNameUsed = 1;
            write_ptr[canonical_name_length]        = 0;
            write_ptr                              += canonical_name_length + 1;
        }
        if (params.m_Namespace)
        {
            memcpy(write_ptr, params.m_Namespace, namespace_length);
            path_pointers.m_OffsetNamespace     = buffer_offset + write_ptr - write_begin;
            path_pointers.m_OffsetNamespaceUsed = 1;
            write_ptr[namespace_length]         = 0;
            write_ptr                          += namespace_length + 1;
        }
        if (params.m_InstanceName)
        {
            memcpy(write_ptr, params.m_InstanceName, instance_name_length);
            path_pointers.m_OffsetInstanceName     = buffer_offset + write_ptr - write_begin;
            path_pointers.m_OffsetInstanceNameUsed = 1;
            write_ptr[instance_name_length]        = 0;
            write_ptr                             += instance_name_length + 1;
        }

        context->m_Buffer.SetSize(context->m_Buffer.Capacity());

        context->m_Paths.OffsetCapacity(1);
        context->m_Paths.Push(path_pointers);
    }

    // Create a canonical path for all leaf members in uniform buffers, e.g:
    //
    // Case 1:
    // struct PBRMaterial
    // {
    //     float metallic;
    //     float roughness;
    //     ...
    // };
    // uniform fs_uniforms {
    //     PBRMaterial material;
    // };
    // member 0: fs_uniforms.material.metallic;
    // member 1: fs_uniforms.material.roughness;
    //
    // Case 2:
    // We have the same, but we also use an "instance member" to rename the uniform (will also be useful later for arrayed uniforms):
    // uniform fs_uniforms {
    //     PBRMaterial material;
    // } my_material;
    // member 0: my_material.material.metallic;
    // member 1: my_material.material.roughness;
    static char* GetConstructedCanonicalName(BuildCanonicalPathsCallbackContext& context, char* str, uint32_t str_len, char* canonical_name_buffer, uint32_t canonical_name_buffer_len)
    {
        int namespace_end = FindNamespace(str, str_len);
        if (namespace_end == -1)
        {
            // Texture uniforms doesn't have namespaces
            return str;
        }

        memcpy(canonical_name_buffer, str, namespace_end);
        canonical_name_buffer[namespace_end] = 0;

        for (int i = 0; i < context.m_Paths.Size(); ++i)
        {
            // These should be null-terminated
            char* namespace_path = context.Namespace(i);

            if (namespace_path && strcmp(namespace_path, canonical_name_buffer) == 0)
            {
                // Matching namespace found
                char* canonical_name = context.CanonicalName(i);
                assert(canonical_name);

                char* str_without_namespace = str + namespace_end + 1;

                // Found a match, we now write either the name without the namespace,
                // or the instance + name without namespace
                if (strcmp(canonical_name, str_without_namespace) == 0)
                {
                    char* instance_name = context.InstanceName(i);
                    char* write_ptr = canonical_name_buffer;

                    if (instance_name && instance_name[0] != 0)
                    {
                        uint32_t instance_name_len = strlen(instance_name);
                        memcpy(write_ptr, instance_name, instance_name_len);
                        write_ptr[instance_name_len] = '.';
                        write_ptr += instance_name_len + 1;
                    }

                    uint32_t canonical_name_len = strlen(canonical_name);
                    memcpy(write_ptr, canonical_name, canonical_name_len);
                    write_ptr[canonical_name_len] = 0;
                    return canonical_name_buffer;
                }
            }
        }

        return str;
    }

    static void OpenGLBuildUniforms(OpenGLContext* context, OpenGLProgram* program, OpenGLShader** shaders, uint32_t num_shaders)
    {
        if (context->m_IsGles3Version)
        {
            BuildUniformBuffers(context, program);
        }

        char uniform_name_buffer[256];
        char canonical_name_buffer[256];

        GLint num_uniforms;
        GLuint program_handle = GetGLHandle(context, program->m_Id);
        glGetProgramiv(program_handle, GL_ACTIVE_UNIFORMS, &num_uniforms);
        CHECK_GL_ERROR;

        // The OpenGL reflect will not return the instance name of a uniform block,
        // so we need to look at the reflection data to get the actual canonical path for a uniform.
        // I.e, if a uniform is specified as:
        //
        // uniform fs_uniforms {
        //     PBRMaterial material;
        // } my_material;
        //
        // OpenGL will yield the following binding:
        // fs_uniforms.material
        //
        // But what we actually want is:
        // my_material.material

        const bool prepend_instance_name = false;

        // Create a list of all canonical leaf paths from reflected uniforms
        BuildCanonicalPathsCallbackContext canonical_paths_ctx;
        IterateUniforms(&program->m_BaseProgram, prepend_instance_name, BuildCanonicalPathsCallback, &canonical_paths_ctx);

        program->m_BaseProgram.m_Uniforms.SetCapacity(num_uniforms);
        program->m_BaseProgram.m_Uniforms.SetSize(num_uniforms);

        for (int i = 0; i < num_uniforms; ++i)
        {
            GLint uniform_size;
            GLenum uniform_type;
            GLsizei uniform_name_length;
            glGetActiveUniform(program_handle, i,
                sizeof(uniform_name_buffer),
                &uniform_name_length,
                &uniform_size,
                &uniform_type,
                uniform_name_buffer);
            CHECK_GL_ERROR;

            GLint uniform_block_index = -1;
            if (context->m_IsGles3Version)
            {
                glGetActiveUniformsiv(program_handle, 1, (GLuint*)&i, GL_UNIFORM_BLOCK_INDEX, &uniform_block_index);
            }

            HUniformLocation uniform_location = INVALID_UNIFORM_LOCATION;

            if (uniform_block_index != -1)
            {
                OpenGLScratchUniformBuffer& ubo = program->m_UniformBuffers[uniform_block_index];
                uint32_t uniform_member_index = 0;

                for (int j = 0; j < ubo.m_Indices.Size(); ++j)
                {
                    if (ubo.m_Indices[j] == i)
                    {
                        uniform_member_index = j;
                        break;
                    }
                }
                uniform_location = ((uint64_t) 1) << 32 | uniform_member_index << 16 | uniform_block_index;
            }
            else
            {
                uniform_location = (HUniformLocation) glGetUniformLocation(program_handle, uniform_name_buffer);
            }

            char* uniform_name = GetBaseUniformName(uniform_name_buffer, uniform_name_length);
            uniform_name_length = strlen(uniform_name);

            // These are temporary strings, we need copies of them.
            char* canonical_name = GetConstructedCanonicalName(canonical_paths_ctx, uniform_name, uniform_name_length, canonical_name_buffer, sizeof(canonical_name_buffer));
            assert(canonical_name != 0);

            Uniform& uniform   = program->m_BaseProgram.m_Uniforms[i];
            uniform.m_Name     = strdup(canonical_name);
            uniform.m_NameHash = dmHashString64(canonical_name);
            uniform.m_Location = uniform_location;
            uniform.m_Count    = uniform_size;
            uniform.m_Type     = GetGraphicsType(uniform_type);

        #if 0
            dmLogInfo("  Uniform[%d]: full-name: %s, canonical-name: %s", i, uniform_name_buffer, canonical_name);
        #endif

            // JG: Original code did this, but I'm not sure why.
            if (uniform.m_Location == -1)
            {
                // Clear error if uniform isn't found
                CLEAR_GL_ERROR
            }
        }
    }

    static inline void IncreaseModificationVersion(OpenGLContext* context)
    {
        ++context->m_ModificationVersion;
        context->m_ModificationVersion = dmMath::Max(0U, context->m_ModificationVersion);
    }

    static bool LinkProgram(GLuint program)
    {
        glLinkProgram(program);

        GLint status;
        glGetProgramiv(program, GL_LINK_STATUS, &status);

        if (status == 0)
        {
            dmLogError("Unable to link program.");
#ifndef NDEBUG
            GLint logLength;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar*) malloc(logLength);
                glGetProgramInfoLog(program, logLength, &logLength, log);
                dmLogWarning("%s\n", log);
                free(log);
            }
#endif
            return false;
        }
        return true;
    }

    static void CreateProgramResourceBindings(OpenGLProgram* program, ResourceBindingDesc bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT], OpenGLShader** shaders, uint32_t num_shaders)
    {
        ProgramResourceBindingsInfo binding_info = {};
        for (int i = 0; i < num_shaders; ++i)
        {
            FillProgramResourceBindings(&program->m_BaseProgram, bindings, 0, 0, binding_info);
        }
        program->m_BaseProgram.m_MaxSet     = binding_info.m_MaxSet;
        program->m_BaseProgram.m_MaxBinding = binding_info.m_MaxBinding;
    }

    static bool SetupGraphicsProgram(OpenGLContext* context, OpenGLProgram* program, OpenGLShader* vertex_shader, OpenGLShader* fragment_shader)
    {
        IncreaseModificationVersion(context);

        GLuint p = glCreateProgram();
        CHECK_GL_ERROR;

        GLuint vertex_id   = GetGLHandle(context, vertex_shader->m_Id);
        GLuint fragment_id = GetGLHandle(context, fragment_shader->m_Id);

        glAttachShader(p, vertex_id);
        CHECK_GL_ERROR;
        glAttachShader(p, fragment_id);
        CHECK_GL_ERROR;

        // For MRT bindings to work correctly on all platforms,
        // we need to specify output locations manually
#ifndef GL_ES_VERSION_2_0
        const char* base_output_name = "_DMENGINE_GENERATED_gl_FragColor";
        char buf[64] = {0};
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            snprintf(buf, sizeof(buf), "%s_%d", base_output_name, i);
            glBindFragDataLocation(p, i, buf);
        }
#endif

        if (!LinkProgram(p))
        {
            delete program;
            glDeleteProgram(p);
            CHECK_GL_ERROR;
            return false;
        }

        program->m_Id       = AddNewGLHandle(context, p);
        program->m_Language = vertex_shader->m_Language;

        OpenGLShader* shaders[] = { vertex_shader, fragment_shader };

        ResourceBindingDesc bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};
        CreateProgramResourceBindings(program, bindings, shaders, DM_ARRAY_SIZE(shaders));

        OpenGLBuildUniforms(context, program, shaders, DM_ARRAY_SIZE(shaders));
        BuildAttributes(program);
        return true;
    }

    static bool SetupComputeProgram(OpenGLContext* context, OpenGLProgram* program, OpenGLShader* shader)
    {
    #ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT
        IncreaseModificationVersion(context);

        GLuint p = glCreateProgram();
        CHECK_GL_ERROR;

        GLuint compute_shader_id = GetGLHandle(context, shader->m_Id);

        glAttachShader(p, compute_shader_id);
        CHECK_GL_ERROR;

        if (!LinkProgram(p))
        {
            delete program;
            glDeleteProgram(p);
            CHECK_GL_ERROR;
            return false;
        }

        program->m_Id            = AddNewGLHandle(context, p);
        program->m_Language      = shader->m_Language;
        program->m_ComputeShader = shader;

        OpenGLBuildUniforms(context, program, &shader, 1);
        return true;
    #else
        dmLogInfo("Compute Shaders are not supported for OpenGL on this platform.");
        return false;
    #endif
    }

    static void DeleteShader(OpenGLContext* context, OpenGLShader* shader)
    {
        if (shader)
        {
            glDeleteShader(GetGLHandle(context, shader->m_Id));
            CHECK_GL_ERROR;
            CleanupGLHandle(context, shader->m_Id);
            delete shader;
        }
    }

    static HProgram OpenGLNewProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLProgram* program = new OpenGLProgram();

        CreateShaderMeta(&ddf->m_Reflection, &program->m_BaseProgram.m_ShaderMeta);

        if (ddf_cp)
        {
        #ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT
            OpenGLShader* compute_shader = CreateShader(_context, DMGRAPHICS_TYPE_COMPUTE_SHADER, ddf_cp, error_buffer, error_buffer_size);
            if (!SetupComputeProgram(context, program, compute_shader))
            {
                DeleteShader(context, compute_shader);
                return 0;
            }
        #else
            dmSnPrintf(error_buffer, error_buffer_size, "Compute Shaders are not supported for OpenGL on this platform.");
            return 0;
        #endif
        }
        else
        {
            OpenGLShader* vertex_shader = CreateShader(_context, GL_VERTEX_SHADER, ddf_vp, error_buffer, error_buffer_size);
            OpenGLShader* fragment_shader = CreateShader(_context, GL_FRAGMENT_SHADER, ddf_fp, error_buffer, error_buffer_size);
            if (!SetupGraphicsProgram(context, program, vertex_shader, fragment_shader))
            {
                DeleteShader(context, vertex_shader);
                DeleteShader(context, fragment_shader);
                return 0;
            }
        }

        return (HProgram) program;
    }

    // Tries to compile a shader (either a vertex or fragment) program.
    // We use this together with a temporary GLuint program to see if we it's
    // possible to compile a reloaded program.
    //
    // In case the compile fails, it also prints the compile errors with dmLogWarning.
    static bool TryCompileShader(GLuint prog, const void* program, GLint size)
    {
        glShaderSource(prog, 1, (const GLchar**) &program, &size);
        CHECK_GL_ERROR;
        glCompileShader(prog);
        CHECK_GL_ERROR;

        GLint status;
        glGetShaderiv(prog, GL_COMPILE_STATUS, &status);
        if (status == 0)
        {
            dmLogError("Unable to compile shader.");
            GLint logLength;
            glGetShaderiv(prog, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar *)malloc(logLength);
                glGetShaderInfoLog(prog, logLength, &logLength, log);
                dmLogError("%s", log);
                free(log);
            }
            CHECK_GL_ERROR;
            return false;
        }

        return true;
    }

    static bool TryLinkProgram(GLuint* ids, int num_ids)
    {
        GLuint tmp_program = glCreateProgram();
        CHECK_GL_ERROR;

        for (int i = 0; i < num_ids; ++i)
        {
            glAttachShader(tmp_program, ids[i]);
            CHECK_GL_ERROR;
        }

        glLinkProgram(tmp_program);

        bool success = true;
        GLint status;
        glGetProgramiv(tmp_program, GL_LINK_STATUS, &status);
        if (status == 0)
        {
            dmLogError("Unable to link program.");
            GLint logLength;
            glGetProgramiv(tmp_program, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar *)malloc(logLength);
                glGetProgramInfoLog(tmp_program, logLength, &logLength, log);
                dmLogError("%s", log);
                free(log);
            }
            success = false;
        }
        glDeleteProgram(tmp_program);

        return success;
    }

    static bool ReloadShader(OpenGLContext* context, OpenGLShader* shader, ShaderDesc::Shader* ddf, GLenum type)
    {
        GLuint tmp_shader = glCreateShader(type);
        bool success = TryCompileShader(tmp_shader, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        glDeleteShader(tmp_shader);
        CHECK_GL_ERROR;

        if (success)
        {
            GLuint id = GetGLHandle(context, shader->m_Id);
            glShaderSource(id, 1, (const GLchar**) &ddf->m_Source.m_Data, (GLint*) &ddf->m_Source.m_Count);
            CHECK_GL_ERROR;
            glCompileShader(id);
            CHECK_GL_ERROR;
        }
        return success;
    }

    static bool OpenGLReloadProgram(HContext _context, HProgram _program, ShaderDesc* ddf)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLProgram* program = (OpenGLProgram*) _program;

        if (ddf_cp)
        {
            if (!ReloadShader(context, program->m_ComputeShader, ddf_cp, DMGRAPHICS_TYPE_COMPUTE_SHADER))
            {
                return false;
            }

            GLuint id = GetGLHandle(context, program->m_ComputeShader->m_Id);

            if (!TryLinkProgram(&id, 1))
            {
                return false;
            }

            glLinkProgram(GetGLHandle(context, program->m_Id));
            CHECK_GL_ERROR;
        }
        else
        {
            if (!ReloadShader(context, program->m_VertexShader, ddf_vp, GL_VERTEX_SHADER))
            {
                return false;
            }
            if (!ReloadShader(context, program->m_FragmentShader, ddf_fp, GL_FRAGMENT_SHADER))
            {
                return false;
            }

            GLuint ids[] = {
                GetGLHandle(context, program->m_VertexShader->m_Id),
                GetGLHandle(context, program->m_FragmentShader->m_Id)
            };

            if (!TryLinkProgram(ids, 2))
            {
                return false;
            }

            glLinkProgram(GetGLHandle(context, program->m_Id));
            CHECK_GL_ERROR;

            BuildAttributes(program);
        }

        return true;
    }

    static void OpenGLDeleteProgram(HContext _context, HProgram program)
    {
        OpenGLProgram* program_ptr = (OpenGLProgram*) program;
        OpenGLContext* context = (OpenGLContext*) _context;

        glDeleteProgram(GetGLHandle(context, program_ptr->m_Id));
        CleanupGLHandle(context, program_ptr->m_Id);

        DeleteShader(context, program_ptr->m_VertexShader);
        DeleteShader(context, program_ptr->m_FragmentShader);
        DeleteShader(context, program_ptr->m_ComputeShader);

        for (int i = 0; i < program_ptr->m_UniformBuffers.Size(); ++i)
        {
            delete program_ptr->m_UniformBuffers[i].m_BlockMemory;
        }

        delete program_ptr;
    }

    static ShaderDesc::Language OpenGLGetProgramLanguage(HProgram program)
    {
        return ((OpenGLProgram*) program)->m_Language;
    }

    static bool OpenGLIsShaderLanguageSupported(HContext _context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        OpenGLContext* context = (OpenGLContext*) _context;

        if (context->m_IsShaderLanguageGles) // 0 == glsl, 1 == gles
        {
            if (context->m_IsGles3Version)
            {
                return language == ShaderDesc::LANGUAGE_GLES_SM300;
            }
            return language == ShaderDesc::LANGUAGE_GLES_SM100;
        }
        else if (shader_type == ShaderDesc::SHADER_TYPE_COMPUTE)
        {
            return language == ShaderDesc::LANGUAGE_GLSL_SM430;
        }
        return language == ShaderDesc::LANGUAGE_GLSL_SM330;
    }

    static void OpenGLEnableProgram(HContext _context, HProgram _program)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLProgram* program = (OpenGLProgram*) _program;
        context->m_CurrentProgram = program;
        glUseProgram(GetGLHandle(context, program->m_Id));
        CHECK_GL_ERROR;
    }

    static void OpenGLDisableProgram(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        context->m_CurrentProgram = 0;
        glUseProgram(0);
        CHECK_GL_ERROR;
    }

    static uint32_t OpenGLGetAttributeCount(HProgram prog)
    {
        assert(prog);
        OpenGLProgram* program_ptr = (OpenGLProgram*) prog;
        return program_ptr->m_Attributes.Size();
    }

    static uint32_t GetElementCount(GLenum type)
    {
        switch(type)
        {
            case GL_FLOAT:        return 1;
            case GL_FLOAT_VEC2:   return 2;
            case GL_FLOAT_VEC3:   return 3;
            case GL_FLOAT_VEC4:   return 4;
            case GL_FLOAT_MAT2:   return 4;
            case GL_FLOAT_MAT3:   return 9;
            case GL_FLOAT_MAT4:   return 16;
            case GL_INT:          return 1;
            case GL_INT_VEC2:     return 2;
            case GL_INT_VEC3:     return 3;
            case GL_INT_VEC4:     return 4;
            case GL_UNSIGNED_INT: return 1;
        }
        assert(0 && "Unsupported type");
        return 0;
    }

    static void OpenGLGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        assert(prog);
        OpenGLProgram* program_ptr = (OpenGLProgram*) prog;
        if (index >= program_ptr->m_Attributes.Size())
        {
            return;
        }

        OpenGLVertexAttribute& attr = program_ptr->m_Attributes[index];
        *name_hash                  = attr.m_NameHash;
        *type                       = GetGraphicsType(attr.m_Type);
        *num_values                 = attr.m_Count;
        *location                   = attr.m_Location;
        *element_count              = GetElementCount(attr.m_Type);
    }

    static void OpenGLSetViewport(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);

        glViewport(x, y, width, height);
        CHECK_GL_ERROR;
    }

    static void OpenGLSetConstantV4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        uint32_t block_member = UNIFORM_LOCATION_GET_OP2(base_location);

        if (block_member)
        {
            uint32_t block_index = UNIFORM_LOCATION_GET_OP0(base_location);
            uint32_t member_index = UNIFORM_LOCATION_GET_OP1(base_location);
            OpenGLScratchUniformBuffer& ubo = context->m_CurrentProgram->m_UniformBuffers[block_index];

            uint8_t* data_ptr = ubo.m_BlockMemory + ubo.m_Offsets[member_index];
            memcpy(data_ptr, data, sizeof(Vector4) * count);
            ubo.m_Dirty = true;
        }
        else
        {
            glUniform4fv(base_location, count, (const GLfloat*) data);
            CHECK_GL_ERROR;
        }
    }

    static void OpenGLSetConstantM4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        uint32_t block_member = UNIFORM_LOCATION_GET_OP2(base_location);

        if (block_member)
        {
            uint32_t block_index = UNIFORM_LOCATION_GET_OP0(base_location);
            uint32_t member_index = UNIFORM_LOCATION_GET_OP1(base_location);
            OpenGLScratchUniformBuffer& ubo = context->m_CurrentProgram->m_UniformBuffers[block_index];

            uint8_t* data_ptr = ubo.m_BlockMemory + ubo.m_Offsets[member_index];
            memcpy(data_ptr, data, sizeof(Vector4) * count * 4);
            ubo.m_Dirty = true;
        }
        else
        {
            glUniformMatrix4fv(base_location, count, 0, (const GLfloat*) data);
            CHECK_GL_ERROR;
        }
    }

    static void OpenGLSetSampler(HContext _context, HUniformLocation location, int32_t unit)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glUniform1i(location, unit);
        CHECK_GL_ERROR;
    }

    static inline GLint GetDepthBufferFormat(OpenGLContext* context)
    {
         return context->m_DepthBufferBits == 16 ?
            DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH16 :
            DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH24;
    }

#if __EMSCRIPTEN__
    static bool WebGLValidateFramebufferAttachmentDimensions(OpenGLContext* context, uint32_t buffer_type_flags, BufferType* color_buffer_flags, const RenderTargetCreationParams params)
    {
        uint32_t attachment_width      = -1;
        uint32_t attachment_height     = -1;
        uint32_t max_color_attachments = context->m_MultiTargetRenderingSupport ? MAX_BUFFER_COLOR_ATTACHMENTS : 1;

        for (int i = 0; i < max_color_attachments; ++i)
        {
            if (buffer_type_flags & color_buffer_flags[i])
            {
                const TextureCreationParams color_params = params.m_ColorBufferCreationParams[i];

                if (attachment_width == -1)
                {
                    attachment_width  = color_params.m_Width;
                    attachment_height = color_params.m_Height;
                }
                else if (attachment_width != color_params.m_Width || attachment_height != color_params.m_Height)
                {
                    return false;
                }
            }
        }

        if(buffer_type_flags & (BUFFER_TYPE_STENCIL_BIT | BUFFER_TYPE_DEPTH_BIT))
        {
            const TextureCreationParams depth_params = params.m_DepthBufferCreationParams;
            const TextureCreationParams stencil_params = params.m_StencilBufferCreationParams;

            if(!(buffer_type_flags & BUFFER_TYPE_STENCIL_BIT))
            {
                if (attachment_width != -1)
                {
                    return attachment_width  == depth_params.m_Width &&
                           attachment_height == depth_params.m_Height;
                }
            }
            else if(!(buffer_type_flags & BUFFER_TYPE_DEPTH_BIT))
            {
                if (attachment_width != -1)
                {
                    return attachment_width  == stencil_params.m_Width &&
                           attachment_height == stencil_params.m_Height;
                }
            }
            else
            {
                if (attachment_width == -1)
                {
                    return depth_params.m_Width == stencil_params.m_Width &&
                           depth_params.m_Height == stencil_params.m_Height;
                }

                return attachment_width  == depth_params.m_Width &&
                       attachment_height == depth_params.m_Height &&
                       attachment_width  == stencil_params.m_Width &&
                       attachment_height == stencil_params.m_Height;
            }
        }

        return true;
    }
#endif

    static void CreateRenderTargetAttachment(OpenGLContext* context, OpenGLRenderTargetAttachment& attachment, AttachmentType type, const TextureParams params, const TextureCreationParams creation_params)
    {
        attachment.m_Params = params;
        attachment.m_Type   = type;

        switch(type)
        {
            case ATTACHMENT_TYPE_BUFFER:
            {
                GLuint handle = 0;
                glGenRenderbuffers(1, &handle);
                attachment.m_Buffer = AddNewGLHandle((HContext)context, handle);
                CHECK_GL_ERROR;
                break;
            }
            case ATTACHMENT_TYPE_TEXTURE:
                attachment.m_Texture = NewTexture(context, creation_params);
                break;
            default: assert(0);
        }

        ClearTextureParamsData(attachment.m_Params);
    }

    static inline void AttachRenderTargetAttachment(OpenGLContext* context, OpenGLRenderTargetAttachment& attachment, GLenum* attachment_targets, uint32_t num_attachment_targets)
    {
        if (attachment.m_Attached)
        {
            return;
        }

        if (attachment.m_Type == ATTACHMENT_TYPE_BUFFER)
        {
            for (int i = 0; i < num_attachment_targets; ++i)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment_targets[i], GL_RENDERBUFFER, GetGLHandle(context, attachment.m_Buffer));
                CHECK_GL_ERROR;
                CHECK_GL_FRAMEBUFFER_ERROR;
            }
        }
        else if (attachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
        {
            OpenGLTexture* attachment_tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, attachment.m_Texture);
            for (int i = 0; i < num_attachment_targets; ++i)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_targets[i], GL_TEXTURE_2D, GetGLHandle(context, attachment_tex->m_TextureIds[0]), 0);
                CHECK_GL_ERROR;
                CHECK_GL_FRAMEBUFFER_ERROR;
            }
        }
        else assert(0);

        attachment.m_Attached = true;
    }

    static void ApplyRenderTargetAttachments(HContext _context, OpenGLRenderTarget* rt, bool update_current)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_ColorAttachments[i].m_Type == ATTACHMENT_TYPE_TEXTURE)
            {
                SetTexture(context, rt->m_ColorAttachments[i].m_Texture, rt->m_ColorAttachments[i].m_Params);

                GLenum attachments[] = { (GLenum) GL_COLOR_ATTACHMENT0 + i };
                AttachRenderTargetAttachment(context, rt->m_ColorAttachments[i], attachments, DM_ARRAY_SIZE(attachments));
            }
        }

        if (rt->m_DepthStencilAttachment.m_Type != ATTACHMENT_TYPE_UNUSED)
        {
            if (rt->m_DepthStencilAttachment.m_Type == ATTACHMENT_TYPE_BUFFER)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, GetGLHandle(context, rt->m_DepthStencilAttachment.m_Buffer));
                glRenderbufferStorage(GL_RENDERBUFFER, DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH_STENCIL, rt->m_DepthStencilAttachment.m_Params.m_Width, rt->m_DepthStencilAttachment.m_Params.m_Height);
                CHECK_GL_ERROR;
    #ifdef GL_DEPTH_STENCIL_ATTACHMENT
                // if we have the capability of GL_DEPTH_STENCIL_ATTACHMENT, create a single combined depth-stencil buffer
                GLenum attachments[] = { GL_DEPTH_STENCIL_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_DepthStencilAttachment, attachments, DM_ARRAY_SIZE(attachments));
    #else
                // create a depth-stencil that has the same buffer attached to both GL_DEPTH_ATTACHMENT and GL_STENCIL_ATTACHMENT (typical ES <= 2.0)
                GLenum attachments[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_DepthStencilAttachment, attachments, DM_ARRAY_SIZE(attachments));
    #endif
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            else if (rt->m_DepthStencilAttachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
            {
                OpenGLTexture* attachment_tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, rt->m_DepthStencilAttachment.m_Texture);

                // JG: This is a workaround! We can't use SetTexture here since there is no compound format for depth+stencil, and I don't want to introduce one *right now* just for OpenGL..

                glBindTexture(GL_TEXTURE_2D, GetGLHandle(context, attachment_tex->m_TextureIds[0]));
                CHECK_GL_ERROR;

                 // The data type (DMGRAPHICS_TYPE_UNSIGNED_INT_24_8) might change later when we introduce 32f depth formats
                glTexImage2D(GL_TEXTURE_2D, 0,
                    DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH24_STENCIL8,
                    rt->m_DepthStencilAttachment.m_Params.m_Width,
                    rt->m_DepthStencilAttachment.m_Params.m_Height,
                    0, DMGRAPHICS_FORMAT_DEPTH_STENCIL, DMGRAPHICS_TYPE_UNSIGNED_INT_24_8, 0);
                CHECK_GL_ERROR;

                glBindTexture(GL_TEXTURE_2D, 0);
            #ifdef GL_DEPTH_STENCIL_ATTACHMENT
                GLenum attachments[] = { GL_DEPTH_STENCIL_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_DepthStencilAttachment, attachments, DM_ARRAY_SIZE(attachments));
            #else
                GLenum attachments[] = { GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_DepthStencilAttachment, attachments, DM_ARRAY_SIZE(attachments));
            #endif
            }
            else
            {
                assert(0);
            }
        }
        else
        {
            if (rt->m_DepthAttachment.m_Type == ATTACHMENT_TYPE_BUFFER)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, GetGLHandle(context, rt->m_DepthAttachment.m_Buffer));
                glRenderbufferStorage(GL_RENDERBUFFER, GetDepthBufferFormat(context), rt->m_DepthAttachment.m_Params.m_Width, rt->m_DepthAttachment.m_Params.m_Height);
                CHECK_GL_ERROR;

                GLenum attachments[] = { GL_DEPTH_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_DepthAttachment, attachments, DM_ARRAY_SIZE(attachments));

                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            else if (rt->m_DepthAttachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
            {
                SetTexture(context, rt->m_DepthAttachment.m_Texture, rt->m_DepthAttachment.m_Params);

                GLenum attachments[] = { GL_DEPTH_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_DepthAttachment, attachments, DM_ARRAY_SIZE(attachments));
            }

            if (rt->m_StencilAttachment.m_Type == ATTACHMENT_TYPE_BUFFER)
            {
                glBindRenderbuffer(GL_RENDERBUFFER, GetGLHandle(context, rt->m_StencilAttachment.m_Buffer));
                glRenderbufferStorage(GL_RENDERBUFFER, DMGRAPHICS_RENDER_BUFFER_FORMAT_STENCIL8, rt->m_StencilAttachment.m_Params.m_Width, rt->m_StencilAttachment.m_Params.m_Height);
                CHECK_GL_ERROR;

                GLenum attachments[] = { GL_STENCIL_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_StencilAttachment, attachments, DM_ARRAY_SIZE(attachments));

                glBindRenderbuffer(GL_RENDERBUFFER, 0);
            }
            else if (rt->m_StencilAttachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
            {
                SetTexture(context, rt->m_StencilAttachment.m_Texture, rt->m_StencilAttachment.m_Params);

                GLenum attachments[] = { GL_STENCIL_ATTACHMENT };
                AttachRenderTargetAttachment(context, rt->m_StencilAttachment, attachments, DM_ARRAY_SIZE(attachments));
            }
        }
    }

    static HRenderTarget OpenGLNewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        OpenGLContext* context        = (OpenGLContext*) _context;
        bool any_color_attachment_set = false;
        bool use_depth_attachment     = buffer_type_flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT;
        bool use_stencil_attachment   = buffer_type_flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT;
        bool depth_texture            = params.m_DepthTexture;
        bool stencil_texture          = params.m_StencilTexture;

        BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        // NOTE: Regarding this, the OpenGL context we create is to low on all current desktop platforms to use, but it should work for the other adapters.
        if (stencil_texture)
        {
            dmLogWarning("Stencil textures are not supported on the OpenGL adapter, defaulting to render buffer.");
        }

        if (use_depth_attachment && use_stencil_attachment && stencil_texture != depth_texture)
        {
            dmLogWarning("Creating a RenderTarget with different backing storage (depth: %s != stencil: %s), defaulting to the depth buffer type for both.",
                (depth_texture ? "texture" : "buffer"),
                (stencil_texture ? "texture" : "buffer"));
            stencil_texture = depth_texture;
        }

        // Emscripten: WebGL 1 requires the "WEBGL_draw_buffers" extension to load the PFN_glDrawBuffers pointer,
        //             but for WebGL 2 we have support natively and no way of loading the pointer via glfwGetprocAddress
    #if __EMSCRIPTEN__
        if (!WebGLValidateFramebufferAttachmentDimensions(context, buffer_type_flags, color_buffer_flags, params))
        {
            dmLogError("All attachments must have the same size when running on WebGL!");
            return 0;
        }
    #endif

        OpenGLRenderTarget* rt = new OpenGLRenderTarget();
        rt->m_BufferTypeFlags  = buffer_type_flags;

        GLuint handle = 0;
        glGenFramebuffers(1, &handle);
        rt->m_Id = AddNewGLHandle(_context, handle);
        CHECK_GL_ERROR;
        glBindFramebuffer(GL_FRAMEBUFFER, handle);
        CHECK_GL_ERROR;

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (buffer_type_flags & color_buffer_flags[i])
            {
                uint32_t color_buffer_index = GetBufferTypeIndex(color_buffer_flags[i]);
                CreateRenderTargetAttachment(context, rt->m_ColorAttachments[i], ATTACHMENT_TYPE_TEXTURE, params.m_ColorBufferParams[color_buffer_index], params.m_ColorBufferCreationParams[color_buffer_index]);
                any_color_attachment_set = true;
            }
        }

        if (use_depth_attachment || use_stencil_attachment)
        {
            if (use_depth_attachment && use_stencil_attachment)
            {
                // If both depth and stencil attachments are requested, we create a shared texture for both attachments since we cannot mix and match buffers and textures as attachments in OpenGL.
                if (depth_texture)
                {
                    CreateRenderTargetAttachment(context, rt->m_DepthStencilAttachment, ATTACHMENT_TYPE_TEXTURE, params.m_DepthBufferParams, params.m_DepthBufferCreationParams);
                }
                else if (context->m_PackedDepthStencilSupport)
                {
                    CreateRenderTargetAttachment(context, rt->m_DepthStencilAttachment, ATTACHMENT_TYPE_BUFFER, params.m_DepthBufferParams, params.m_DepthBufferCreationParams);
                }
                else
                {
                    CreateRenderTargetAttachment(context, rt->m_DepthAttachment, ATTACHMENT_TYPE_BUFFER, params.m_DepthBufferParams, params.m_DepthBufferCreationParams);
                    CreateRenderTargetAttachment(context, rt->m_DepthAttachment, ATTACHMENT_TYPE_BUFFER, params.m_StencilBufferParams, params.m_StencilBufferCreationParams);
                }
            }
            else if (use_depth_attachment)
            {
                CreateRenderTargetAttachment(context, rt->m_DepthAttachment, depth_texture ? ATTACHMENT_TYPE_TEXTURE : ATTACHMENT_TYPE_BUFFER, params.m_DepthBufferParams, params.m_DepthBufferCreationParams);
            }
            else if (use_stencil_attachment)
            {
                CreateRenderTargetAttachment(context, rt->m_StencilAttachment, ATTACHMENT_TYPE_BUFFER, params.m_StencilBufferParams, params.m_StencilBufferCreationParams);
            }
        }

        ApplyRenderTargetAttachments(_context, rt, false);

        // Disable color buffer
        if (!any_color_attachment_set)
        {
        #if !defined(GL_ES_VERSION_2_0)
            // TODO: Not available in OpenGL ES.
            // According to this thread it should not be required but honestly I don't quite understand
            // https://devforums.apple.com/message/495216#495216
            glDrawBuffer(GL_NONE);
            CHECK_GL_ERROR;
            glReadBuffer(GL_NONE);
            CHECK_GL_ERROR;
        #endif
        }

        CHECK_GL_FRAMEBUFFER_ERROR;
        glBindFramebuffer(GL_FRAMEBUFFER, dmPlatform::OpenGLGetDefaultFramebufferId());
        CHECK_GL_ERROR;

        return StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
    }

    static void DeleteRenderTargetAttachment(OpenGLRenderTargetAttachment& attachment)
    {
        if (attachment.m_Type == ATTACHMENT_TYPE_BUFFER && attachment.m_Buffer)
        {
            GLuint handle = GetGLHandle(g_Context, attachment.m_Buffer);
            glDeleteRenderbuffers(1, &handle);
            CleanupGLHandle(g_Context, attachment.m_Buffer);
            attachment.m_Buffer = 0;
        }
        else if (attachment.m_Type == ATTACHMENT_TYPE_TEXTURE && attachment.m_Texture)
        {
            DeleteTexture((HContext)g_Context, attachment.m_Texture);
            attachment.m_Texture = 0;
        }
    }

    static void OpenGLDeleteRenderTarget(HContext _context, HRenderTarget render_target)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLRenderTarget* rt = GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, render_target);

        GLuint handle = GetGLHandle(context, rt->m_Id);
        glDeleteFramebuffers(1, &handle);
        CleanupGLHandle(context, rt->m_Id);

        for (uint8_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; i++)
        {
            DeleteRenderTargetAttachment(rt->m_ColorAttachments[i]);
        }

        DeleteRenderTargetAttachment(rt->m_DepthStencilAttachment);
        DeleteRenderTargetAttachment(rt->m_DepthAttachment);
        DeleteRenderTargetAttachment(rt->m_StencilAttachment);

        context->m_AssetHandleContainer.Release(render_target);

        delete rt;
    }

    uint32_t OpenGLGetDefaultFramebufferId(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return dmPlatform::OpenGLGetDefaultFramebufferId();
    }

    static void OpenGLSetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLRenderTarget* rt = 0;

        if (render_target != 0)
        {
            rt = GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, render_target);
        }

        if(PFN_glInvalidateFramebuffer != NULL)
        {
            if(context->m_FrameBufferInvalidateBits)
            {
                uint32_t invalidate_bits = context->m_FrameBufferInvalidateBits;
                if((invalidate_bits & (BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT)) && (context->m_PackedDepthStencilSupport))
                {
                    // if packed depth/stencil buffer is used and either is set as transient, force both non-transient (as both will otherwise be transient).
                    invalidate_bits &= ~(BUFFER_TYPE_DEPTH_BIT | BUFFER_TYPE_STENCIL_BIT);
                }
                GLenum types[MAX_BUFFER_TYPE_COUNT];
                uint32_t types_count = 0;
                if(invalidate_bits & BUFFER_TYPE_COLOR0_BIT)
                {
                    types[types_count++] = context->m_FrameBufferInvalidateAttachments ? DMGRAPHICS_RENDER_BUFFER_COLOR_ATTACHMENT : DMGRAPHICS_RENDER_BUFFER_COLOR;
                }
                if(invalidate_bits & BUFFER_TYPE_DEPTH_BIT)
                {
                    types[types_count++] = context->m_FrameBufferInvalidateAttachments ? DMGRAPHICS_RENDER_BUFFER_DEPTH_ATTACHMENT : DMGRAPHICS_RENDER_BUFFER_DEPTH;
                }
                if(invalidate_bits & BUFFER_TYPE_STENCIL_BIT)
                {
                    types[types_count++] = context->m_FrameBufferInvalidateAttachments ? DMGRAPHICS_RENDER_BUFFER_STENCIL_ATTACHMENT : DMGRAPHICS_RENDER_BUFFER_STENCIL;
                }
                PFN_glInvalidateFramebuffer( GL_FRAMEBUFFER, types_count, &types[0] );
            }
            context->m_FrameBufferInvalidateBits = transient_buffer_types;
#if defined(DM_PLATFORM_IOS)
            context->m_FrameBufferInvalidateAttachments = 1; // always attachments on iOS
#else
            context->m_FrameBufferInvalidateAttachments = rt != NULL;
#endif
        }
        glBindFramebuffer(GL_FRAMEBUFFER, rt == NULL ? dmPlatform::OpenGLGetDefaultFramebufferId() : GetGLHandle(context, rt->m_Id));
        CHECK_GL_ERROR;

    #if __EMSCRIPTEN__
        #define DRAW_BUFFERS_FN glDrawBuffers
    #else
        #define DRAW_BUFFERS_FN PFN_glDrawBuffers
    #endif

        if (rt != NULL && DRAW_BUFFERS_FN != 0x0)
        {
            uint32_t num_buffers = 0;
            GLuint buffers[MAX_BUFFER_COLOR_ATTACHMENTS] = {};

            for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; i++)
            {
                if (rt->m_ColorAttachments[i].m_Texture)
                {
                    buffers[i] = GL_COLOR_ATTACHMENT0 + i;
                    num_buffers++;
                }
                else
                {
                    buffers[i] = 0;
                }
            }

            if (num_buffers > 1)
            {
                DRAW_BUFFERS_FN(num_buffers, buffers);
            }
        }

    #undef DRAW_BUFFERS_FN

        CHECK_GL_FRAMEBUFFER_ERROR;
    }

    static inline HTexture GetAttachmentTexture(OpenGLRenderTargetAttachment& attachment)
    {
        if (attachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
            return attachment.m_Texture;
        return 0;
    }

    static HTexture OpenGLGetRenderTargetTexture(HContext _context, HRenderTarget render_target, BufferType buffer_type)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLRenderTarget* rt = GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, render_target);

        if (IsColorBufferType(buffer_type))
        {
            return GetAttachmentTexture(rt->m_ColorAttachments[GetBufferTypeIndex(buffer_type)]);
        }
        else if (rt->m_DepthStencilAttachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
        {
            return rt->m_DepthStencilAttachment.m_Texture;
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT && rt->m_DepthAttachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
        {
            return GetAttachmentTexture(rt->m_DepthAttachment);
        }
        else if (buffer_type == BUFFER_TYPE_STENCIL_BIT && rt->m_StencilAttachment.m_Type == ATTACHMENT_TYPE_TEXTURE)
        {
            return GetAttachmentTexture(rt->m_StencilAttachment);
        }
        return 0;
    }

    static void OpenGLGetRenderTargetSize(HContext _context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLRenderTarget* rt = GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, render_target);
        TextureParams* params = 0;

        if (IsColorBufferType(buffer_type))
        {
            uint32_t i = GetBufferTypeIndex(buffer_type);
            assert(i < MAX_BUFFER_COLOR_ATTACHMENTS);
            params = &rt->m_ColorAttachments[i].m_Params;
        }
        else if (rt->m_DepthStencilAttachment.m_Type != ATTACHMENT_TYPE_UNUSED)
        {
            params = &rt->m_DepthStencilAttachment.m_Params;
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT)
        {
            params = &rt->m_DepthAttachment.m_Params;
        }
        else if (buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            params = &rt->m_StencilAttachment.m_Params;
        }
        else
        {
            assert(0);
        }

        width  = params->m_Width;
        height = params->m_Height;
    }

    static void OpenGLSetRenderTargetSize(HContext _context, HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLRenderTarget* rt = GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, render_target);

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            rt->m_ColorAttachments[i].m_Params.m_Width  = width;
            rt->m_ColorAttachments[i].m_Params.m_Height = height;
        }

        rt->m_DepthStencilAttachment.m_Params.m_Width  = width;
        rt->m_DepthStencilAttachment.m_Params.m_Height = height;
        rt->m_DepthAttachment.m_Params.m_Width         = width;
        rt->m_DepthAttachment.m_Params.m_Height        = height;
        rt->m_StencilAttachment.m_Params.m_Width       = width;
        rt->m_StencilAttachment.m_Params.m_Height      = height;

        ApplyRenderTargetAttachments(context, rt, true);
    }

    static bool OpenGLIsTextureFormatSupported(HContext _context, TextureFormat format)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return (context->m_TextureFormatSupport & (1 << format)) != 0 || (context->m_ASTCSupport && IsTextureFormatASTC(format));
    }

    static uint32_t OpenGLGetMaxTextureSize(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        return context->m_MaxTextureSize;
    }

    static HTexture OpenGLNewTexture(HContext _context, const TextureCreationParams& params)
    {
        uint16_t num_texture_ids = 1;
        TextureType texture_type = params.m_Type;

        OpenGLContext* context = (OpenGLContext*) _context;

        // If an array texture was requested but we cannot create such textures,
        // we need to fallback to separate textures instead
        if (params.m_Type == TEXTURE_TYPE_2D_ARRAY && !context->m_TextureArraySupport)
        {
            num_texture_ids = params.m_Depth;
            texture_type    = TEXTURE_TYPE_2D;
        }

        HOpenglID* static_ids = (uint32_t*) malloc(num_texture_ids * sizeof(HOpenglID));
        GLuint* gl_texture_ids = (GLuint*) malloc(num_texture_ids * sizeof(GLuint));
        glGenTextures(num_texture_ids, gl_texture_ids);
        CHECK_GL_ERROR;
        for (uint16_t idx = 0; idx < num_texture_ids; ++idx)
        {
            static_ids[idx] = AddNewGLHandle(_context, gl_texture_ids[idx]);
        }
        free(gl_texture_ids);

        OpenGLTexture* tex    = new OpenGLTexture();
        tex->m_Type           = texture_type;
        tex->m_TextureIds     = static_ids;
        tex->m_Width          = params.m_Width;
        tex->m_Height         = params.m_Height;
        tex->m_Depth          = params.m_Depth;
        tex->m_NumTextureIds  = num_texture_ids;
        tex->m_UsageHintFlags = params.m_UsageHintBits;
        tex->m_PageCount      = params.m_LayerCount;

        if (params.m_OriginalWidth == 0)
        {
            tex->m_OriginalWidth  = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        }
        else
        {
            tex->m_OriginalWidth  = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        tex->m_MipMapCount = 0;
        tex->m_DataState = 0;
        tex->m_ResourceSize = 0;

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        return StoreAssetInContainer(context->m_AssetHandleContainer, tex, ASSET_TYPE_TEXTURE);
    }

    static void DoDeleteTexture(OpenGLContext* context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);

        // Even if we check for validity when the texture was flagged for async deletion,
        // we can still end up in this state in very specific cases.
        if (tex != 0x0)
        {
            GLuint* handles = (GLuint*) malloc(tex->m_NumTextureIds * sizeof(GLuint));
            for (uint16_t idx = 0; idx < tex->m_NumTextureIds; ++idx)
            {
                handles[idx] = GetGLHandle(context, tex->m_TextureIds[idx]);
                CleanupGLHandle(context, tex->m_TextureIds[idx]);
            }
            glDeleteTextures(tex->m_NumTextureIds, handles);
            CHECK_GL_ERROR;
            free(handles);
            free(tex->m_TextureIds);
        }

        context->m_AssetHandleContainer.Release(texture);
        delete tex;
    }

    static int AsyncDeleteTextureProcess(dmJobThread::HContext, dmJobThread::HJob job, void* _context, void* data)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DoDeleteTexture(context, (HTexture) data);
        return 0;
    }

    static void OpenGLDeleteTextureAsync(OpenGLContext* context, HTexture texture)
    {
        if (context->m_AsyncProcessingSupport)
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);

            dmJobThread::Job job = {0};
            job.m_Process = AsyncDeleteTextureProcess;
            job.m_Callback = 0;
            job.m_Context = (void*) context;
            job.m_Data = (void*) (uintptr_t) texture;

            dmJobThread::HJob hjob = dmJobThread::CreateJob(context->m_JobThread, &job);
            dmJobThread::PushJob(context->m_JobThread, hjob);
        }
        else
        {
            DoDeleteTexture(context, texture);
        }
    }

    static void PostDeleteTextures(OpenGLContext* context, bool force_delete)
    {
        DM_PROFILE("OpenGLPostDeleteTextures");

        if (force_delete)
        {
            uint32_t size = context->m_SetTextureAsyncState.m_PostDeleteTextures.Size();
            for (uint32_t i = 0; i < size; ++i)
            {
                DoDeleteTexture(context, context->m_SetTextureAsyncState.m_PostDeleteTextures[i]);
            }
            return;
        }

        uint32_t i = 0;
        while(i < context->m_SetTextureAsyncState.m_PostDeleteTextures.Size())
        {
            HTexture texture = context->m_SetTextureAsyncState.m_PostDeleteTextures[i];
            if(!(dmGraphics::GetTextureStatusFlags(context, texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING))
            {
                OpenGLDeleteTextureAsync(context, texture);
                context->m_SetTextureAsyncState.m_PostDeleteTextures.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
    }

    static void OpenGLDeleteTexture(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(texture);
        // We can only delete valid textures
        if (!IsAssetHandleValid(context, texture))
        {
            return;
        }
        // If they're not uploaded yet, we cannot delete them
        if(dmGraphics::GetTextureStatusFlags(context, texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
        {
            PushSetTextureAsyncDeleteTexture(context->m_SetTextureAsyncState, texture);
        }
        else
        {
            OpenGLDeleteTextureAsync(context, texture);
        }
    }

    static GLenum GetOpenGLTextureWrap(TextureWrap wrap)
    {
        GLenum texture_wrap_lut[] = {
        #ifndef GL_ARB_multitexture
            0x812D,
            0x812F,
            0x8370,
        #else
            GL_CLAMP_TO_BORDER,
            GL_CLAMP_TO_EDGE,
            GL_MIRRORED_REPEAT,
        #endif
            GL_REPEAT,
        };
        return texture_wrap_lut[wrap];
    }

    static GLenum GetOpenGLTextureFilter(TextureFilter texture_filter)
    {
        const GLenum texture_filter_lut[] = {
            0,
            GL_NEAREST,
            GL_LINEAR,
            GL_NEAREST_MIPMAP_NEAREST,
            GL_NEAREST_MIPMAP_LINEAR,
            GL_LINEAR_MIPMAP_NEAREST,
            GL_LINEAR_MIPMAP_LINEAR,
        };

        return texture_filter_lut[texture_filter];
    }

    static inline GLenum GetNonMipMapVersionOfFilter(GLenum filter)
    {
        switch (filter)
        {
            case GL_NEAREST:
            case GL_NEAREST_MIPMAP_NEAREST:
            case GL_NEAREST_MIPMAP_LINEAR:
                return GL_NEAREST;
            default:
                return GL_LINEAR;
        }
    }


    static void OpenGLSetTextureParams(HContext _context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        if (tex == 0x0)
        {
            return;
        }

        GLenum gl_type       = GetOpenGLTextureType(tex->m_Type);
        GLenum gl_min_filter = GetOpenGLTextureFilter(minfilter == TEXTURE_FILTER_DEFAULT ? context->m_DefaultTextureMinFilter : minfilter);
        GLenum gl_mag_filter = GetOpenGLTextureFilter(magfilter == TEXTURE_FILTER_DEFAULT ? context->m_DefaultTextureMagFilter : magfilter);

        // Using a mipmapped min filter without any mipmaps will break the sampler
        if (tex->m_MipMapCount <= 1)
        {
            gl_min_filter = GetNonMipMapVersionOfFilter(gl_min_filter);
        }

        glTexParameteri(gl_type, GL_TEXTURE_MIN_FILTER, gl_min_filter);
        CHECK_GL_ERROR;

        glTexParameteri(gl_type, GL_TEXTURE_MAG_FILTER, gl_mag_filter);
        CHECK_GL_ERROR;

        glTexParameteri(gl_type, GL_TEXTURE_WRAP_S, GetOpenGLTextureWrap(uwrap));
        CHECK_GL_ERROR

        glTexParameteri(gl_type, GL_TEXTURE_WRAP_T, GetOpenGLTextureWrap(vwrap));
        CHECK_GL_ERROR

        if (context->m_AnisotropySupport && max_anisotropy > 1.0f)
        {
            glTexParameterf(gl_type, GL_TEXTURE_MAX_ANISOTROPY_EXT, dmMath::Min(max_anisotropy, context->m_MaxAnisotropy));
            CHECK_GL_ERROR
        }
    }

    static uint8_t OpenGLGetNumTextureHandles(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_NumTextureIds : 0;
    }

    static uint32_t OpenGLGetTextureUsageHintFlags(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_UsageHintFlags : 0;
    }

    static uint8_t OpenGLGetTexturePageCount(HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_Context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(g_Context->m_AssetHandleContainer, texture);
        return tex ? tex->m_PageCount : 0;
    }

    static uint32_t OpenGLGetTextureStatusFlags(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        uint32_t flags     = TEXTURE_STATUS_OK;
        if(tex && dmAtomicGet32(&tex->m_DataState))
        {
            flags |= TEXTURE_STATUS_DATA_PENDING;
        }
        return flags;
    }

    // Called on worker thread
    static int AsyncProcessCallback(dmJobThread::HContext, dmJobThread::HJob job, void* _context, void* data)
    {
        OpenGLContext* context     = (OpenGLContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        if (dmAtomicGet32(&context->m_DeleteContextRequested))
        {
            return 0;
        }

        // TODO: If we use multiple workers, we either need more secondary contexts,
        //       or we need to guard this call with a mutex.
        //       The window handle (pointer) isn't protected by a mutex either,
        //       but it is currently not used with our GLFW version (yet) so
        //       we don't necessarily need to guard it right now.
        SetTexture(_context, ap.m_Texture, ap.m_Params);
        glFlush();

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, ap.m_Texture);
        int32_t data_state = dmAtomicGet32(&tex->m_DataState);
        data_state &= ~(1<<ap.m_Params.m_MipMap);
        dmAtomicStore32(&tex->m_DataState, data_state);
        return 0;
    }

    // Called on thread where we update (which should be the main thread)
    static void AsyncCompleteCallback(dmJobThread::HContext, dmJobThread::HJob job, dmJobThread::JobStatus status, void* _context, void* data, int result)
    {
        OpenGLContext* context     = (OpenGLContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        if (ap.m_Callback)
        {
            ap.m_Callback(ap.m_Texture, ap.m_UserData);
        }

        ReturnSetTextureAsyncIndex(context->m_SetTextureAsyncState, param_array_index);
    }

    static void OpenGLSetTextureAsync(HContext _context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        if (context->m_AsyncProcessingSupport)
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
            OpenGLTexture* tex         = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
            tex->m_DataState          |= 1<<params.m_MipMap;
            uint16_t param_array_index = PushSetTextureAsyncState(context->m_SetTextureAsyncState, texture, params, callback, user_data);

            dmJobThread::Job job = {0};
            job.m_Process = AsyncProcessCallback;
            job.m_Callback = AsyncCompleteCallback;
            job.m_Context = (void*) context;
            job.m_Data = (void*) (uintptr_t) param_array_index;

            dmJobThread::HJob hjob = dmJobThread::CreateJob(context->m_JobThread, &job);
            dmJobThread::PushJob(g_Context->m_JobThread, hjob);
        }
        else
        {
            SetTexture(context, texture, params);
        }
    }

    // JG: We should deprecate / remove this function. It's not safe to return pointers like this.
    //     Instead, we should add functions (like we have for other APIs), that returns the actual underlying API object.
    //     Which in this case would be just an int. And perhaps functions that check for validity of the high-level texture.
    static HandleResult OpenGLGetTextureHandle(HTexture texture, void** out_handle)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_Context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(g_Context->m_AssetHandleContainer, texture);
        *out_handle = 0x0;

        if (!texture)
        {
            return HANDLE_RESULT_ERROR;
        }

        *out_handle = GetGLHandlePointer(g_Context, tex->m_TextureIds[0]);

        return HANDLE_RESULT_OK;
    }

    static inline void GetOpenGLSetTextureParams(OpenGLContext* context, TextureFormat format, GLint& gl_internal_format, GLenum& gl_format, GLenum& gl_type)
    {
        #define ES2_ENUM_WORKAROUND(var, value) if (!context->m_IsGles3Version) var = value

    #ifdef __EMSCRIPTEN__
        #define EMSCRIPTEN_ES2_BACKWARDS_COMPAT(var, value) ES2_ENUM_WORKAROUND(var, value)
    #else
        #define EMSCRIPTEN_ES2_BACKWARDS_COMPAT(var, value)
    #endif
    #ifdef __ANDROID__
        #define ANDROID_ES2_BACKWARDS_COMPAT(var, value) ES2_ENUM_WORKAROUND(var, value)
    #else
        #define ANDROID_ES2_BACKWARDS_COMPAT(var, value)
    #endif

        switch (format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE;
            break;
        case TEXTURE_FORMAT_LUMINANCE_ALPHA:
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA;
            break;
        case TEXTURE_FORMAT_RGB:
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            break;
        case TEXTURE_FORMAT_RGBA:
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            break;
        case TEXTURE_FORMAT_RGB_16BPP:
            gl_type            = DMGRAPHICS_TYPE_UNSIGNED_SHORT_565;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            break;
        case TEXTURE_FORMAT_RGBA_16BPP:
            gl_type            = DMGRAPHICS_TYPE_UNSIGNED_SHORT_4444;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            break;

        case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:   gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_2BPPV1; break;
        case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:   gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_4BPPV1; break;
        case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:  gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1; break;
        case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:  gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1; break;
        case TEXTURE_FORMAT_RGB_ETC1:           gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_ETC1; break;
        case TEXTURE_FORMAT_R_ETC2:             gl_format = DMGRAPHICS_TEXTURE_FORMAT_R11_EAC; break;
        case TEXTURE_FORMAT_RG_ETC2:            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RG11_EAC; break;
        case TEXTURE_FORMAT_RGBA_ETC2:          gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA8_ETC2_EAC; break;

        case TEXTURE_FORMAT_RGBA_ASTC_4X4:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_4x4_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_5X4:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_5x4_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_5X5:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_5x5_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_6X5:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_6x5_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_6X6:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_6x6_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_8X5:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_8x5_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_8X6:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_8x6_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_8X8:      gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_8x8_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_10X5:     gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_10x5_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_10X6:     gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_10x6_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_10X8:     gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_10x8_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_10X10:    gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_10x10_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_12X10:    gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_12x10_KHR; break;
        case TEXTURE_FORMAT_RGBA_ASTC_12X12:    gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_ASTC_12x12_KHR; break;

        case TEXTURE_FORMAT_RGB_BC1:            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_DXT1; break;
        case TEXTURE_FORMAT_RGBA_BC3:           gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT5; break;
        case TEXTURE_FORMAT_R_BC4:              gl_format = DMGRAPHICS_TEXTURE_FORMAT_RED_RGTC1; break;
        case TEXTURE_FORMAT_RG_BC5:             gl_format = DMGRAPHICS_TEXTURE_FORMAT_RG_RGTC2; break;
        case TEXTURE_FORMAT_RGBA_BC7:           gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_BPTC_UNORM; break;

        // Float formats
        case TEXTURE_FORMAT_RGB16F:
            gl_type            = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB16F;
            EMSCRIPTEN_ES2_BACKWARDS_COMPAT(gl_internal_format, DMGRAPHICS_TEXTURE_FORMAT_RGB);
            EMSCRIPTEN_ES2_BACKWARDS_COMPAT(gl_type, DMGRAPHICS_TYPE_HALF_FLOAT_OES);
            ANDROID_ES2_BACKWARDS_COMPAT(gl_type, DMGRAPHICS_TYPE_HALF_FLOAT_OES);
            break;
        case TEXTURE_FORMAT_RGB32F:
            gl_type            = GL_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB32F;
            EMSCRIPTEN_ES2_BACKWARDS_COMPAT(gl_internal_format, DMGRAPHICS_TEXTURE_FORMAT_RGB);
            break;
        case TEXTURE_FORMAT_RGBA16F:
            gl_type            = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA16F;
            EMSCRIPTEN_ES2_BACKWARDS_COMPAT(gl_internal_format, DMGRAPHICS_TEXTURE_FORMAT_RGBA);
            EMSCRIPTEN_ES2_BACKWARDS_COMPAT(gl_type, DMGRAPHICS_TYPE_HALF_FLOAT_OES);
            ANDROID_ES2_BACKWARDS_COMPAT(gl_type, DMGRAPHICS_TYPE_HALF_FLOAT_OES);
            break;
        case TEXTURE_FORMAT_RGBA32F:
            gl_type            = GL_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA32F;
            EMSCRIPTEN_ES2_BACKWARDS_COMPAT(gl_internal_format, DMGRAPHICS_TEXTURE_FORMAT_RGBA);
            break;
        case TEXTURE_FORMAT_R16F:
            gl_type            = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RED;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_R16F;
            ANDROID_ES2_BACKWARDS_COMPAT(gl_type, DMGRAPHICS_TYPE_HALF_FLOAT_OES);
            break;
        case TEXTURE_FORMAT_R32F:
            gl_type            = GL_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RED;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_R32F;
            break;
        case TEXTURE_FORMAT_RG16F:
            gl_type            = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RG;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RG16F;
            ANDROID_ES2_BACKWARDS_COMPAT(gl_type, DMGRAPHICS_TYPE_HALF_FLOAT_OES);
            break;
        case TEXTURE_FORMAT_RG32F:
            gl_type            = GL_FLOAT;
            gl_format          = DMGRAPHICS_TEXTURE_FORMAT_RG;
            gl_internal_format = DMGRAPHICS_TEXTURE_FORMAT_RG32F;
            break;
        case TEXTURE_FORMAT_DEPTH:
            gl_type            = GL_FLOAT;
            gl_format          = GL_DEPTH_COMPONENT;
            gl_internal_format = GetDepthBufferFormat(context);
        #ifdef __EMSCRIPTEN__
            gl_type            = GL_UNSIGNED_INT;
            gl_internal_format = context->m_IsGles3Version ? GL_DEPTH_COMPONENT24 : DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH16;
        #endif
            break;

        default:
            assert(0);
            dmLogError("Texture format %s is not a valid format.", GetTextureFormatLiteral(format));
            break;
        }

    #undef ES2_ENUM_WORKAROUND
    #undef EMSCRIPTEN_ES2_BACKWARDS_COMPAT
    #undef ANDROID_ES2_BACKWARDS_COMPAT
    }

    static void OpenGLSetTexture(HContext _context, HTexture texture, const TextureParams& params)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_PROFILE(__FUNCTION__);

        // Stencil textures are not supported
        assert(params.m_Format != TEXTURE_FORMAT_STENCIL);

        // Responsibility is on caller to not send in too big textures.
        assert(params.m_Width <= context->m_MaxTextureSize);
        assert(params.m_Height <= context->m_MaxTextureSize);

        int unpackAlignment = 4;
        /*
         * For RGB-textures the row-alignment may not be a multiple of 4.
         * OpenGL doesn't like this by default
         */
        if (params.m_Format != TEXTURE_FORMAT_RGBA && !IsTextureFormatCompressed(params.m_Format))
        {
            uint32_t bytes_per_row = params.m_Width * dmMath::Max(1U, GetTextureFormatBitsPerPixel(params.m_Format)/8);
            if (bytes_per_row % 4 == 0) {
                // Ok
            } else if (bytes_per_row % 2 == 0) {
                unpackAlignment = 2;
            } else {
                unpackAlignment = 1;
            }
        }

        if (unpackAlignment != 4)
        {
            glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
            CHECK_GL_ERROR;
        }

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);

        tex->m_MipMapCount = dmMath::Max(tex->m_MipMapCount, (uint16_t)(params.m_MipMap+1));

        GLenum type              = GetOpenGLTextureType(tex->m_Type);
        GLenum gl_format         = 0;
        GLenum gl_type           = GL_UNSIGNED_BYTE; // only used of uncompressed formats
        GLint gl_internal_format = -1;               // only used for uncompressed formats

        GetOpenGLSetTextureParams(context, params.m_Format, gl_internal_format, gl_format, gl_type);

        tex->m_Params = params;

        if (!params.m_SubUpdate)
        {
            if (params.m_MipMap == 0)
            {
                tex->m_Width  = params.m_Width;
                tex->m_Height = params.m_Height;
                tex->m_Depth  = params.m_Depth;
            }

            if (params.m_MipMap == 0)
            {
                tex->m_ResourceSize = params.m_DataSize;
            }
        }

        for (int i = 0; i < tex->m_NumTextureIds; ++i)
        {
            glBindTexture(type, GetGLHandle(context, tex->m_TextureIds[i]));
            CHECK_GL_ERROR;

            if (!params.m_SubUpdate)
            {
                SetTextureParams(context, texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);
            }

            switch (params.m_Format)
            {
            case TEXTURE_FORMAT_LUMINANCE:
            case TEXTURE_FORMAT_LUMINANCE_ALPHA:
            case TEXTURE_FORMAT_DEPTH:
            case TEXTURE_FORMAT_RGB:
            case TEXTURE_FORMAT_RGBA:
            case TEXTURE_FORMAT_RGB_16BPP:
            case TEXTURE_FORMAT_RGBA_16BPP:
            case TEXTURE_FORMAT_RGB16F:
            case TEXTURE_FORMAT_RGB32F:
            case TEXTURE_FORMAT_RGBA16F:
            case TEXTURE_FORMAT_RGBA32F:
            case TEXTURE_FORMAT_R16F:
            case TEXTURE_FORMAT_R32F:
            case TEXTURE_FORMAT_RG16F:
            case TEXTURE_FORMAT_RG32F:
                if (tex->m_Type == TEXTURE_TYPE_2D || tex->m_Type == TEXTURE_TYPE_IMAGE_2D)
                {
                    const char* p = (const char*) params.m_Data;
                    if (params.m_SubUpdate)
                    {
                        glTexSubImage2D(GL_TEXTURE_2D, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * i);
                    }
                    else
                    {
                        glTexImage2D(GL_TEXTURE_2D, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * i);
                    }
                    CHECK_GL_ERROR;
                }
                else if (tex->m_Type == TEXTURE_TYPE_2D_ARRAY)
                {
                    assert(context->m_TextureArraySupport);
                    if (params.m_SubUpdate)
                    {
                        DMGRAPHICS_TEX_SUB_IMAGE_3D(GL_TEXTURE_2D_ARRAY, params.m_MipMap, params.m_X, params.m_Y, params.m_Slice, params.m_Width, params.m_Height, params.m_LayerCount, gl_format, gl_type, params.m_Data);
                    }
                    else
                    {
                        DMGRAPHICS_TEX_IMAGE_3D(GL_TEXTURE_2D_ARRAY, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, params.m_LayerCount, 0, gl_format, gl_type, params.m_Data);
                    }
                    CHECK_GL_ERROR;
                }
                else if (tex->m_Type == TEXTURE_TYPE_3D || tex->m_Type == TEXTURE_TYPE_IMAGE_3D)
                {
                    assert(context->m_3DTextureSupport);
                    if (params.m_SubUpdate)
                    {
                        DMGRAPHICS_TEX_SUB_IMAGE_3D(GL_TEXTURE_3D, params.m_MipMap, params.m_X, params.m_Y, params.m_Z, params.m_Width, params.m_Height, params.m_Depth, gl_format, gl_type, params.m_Data);
                    }
                    else
                    {
                        DMGRAPHICS_TEX_IMAGE_3D(GL_TEXTURE_3D, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, params.m_Depth, 0, gl_format, gl_type, params.m_Data);
                    }
                    CHECK_GL_ERROR;
                }
                else if (tex->m_Type == TEXTURE_TYPE_CUBE_MAP)
                {
                    assert(tex->m_NumTextureIds == 1);
                    const char* p = (const char*) params.m_Data;
                    if (params.m_SubUpdate)
                    {
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 0);
                        CHECK_GL_ERROR;
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 1);
                        CHECK_GL_ERROR;
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 2);
                        CHECK_GL_ERROR;
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 3);
                        CHECK_GL_ERROR;
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 4);
                        CHECK_GL_ERROR;
                        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 5);
                        CHECK_GL_ERROR;
                    }
                    else
                    {
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 0);
                        CHECK_GL_ERROR;
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 1);
                        CHECK_GL_ERROR;
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 2);
                        CHECK_GL_ERROR;
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 3);
                        CHECK_GL_ERROR;
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 4);
                        CHECK_GL_ERROR;
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, gl_internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 5);
                        CHECK_GL_ERROR;
                    }
                }
                else
                {
                    assert(0);
                }
                break;

            case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            case TEXTURE_FORMAT_RGB_ETC1:
            case TEXTURE_FORMAT_R_ETC2:
            case TEXTURE_FORMAT_RG_ETC2:
            case TEXTURE_FORMAT_RGBA_ETC2:
            case TEXTURE_FORMAT_RGBA_ASTC_4X4:
            case TEXTURE_FORMAT_RGBA_ASTC_5X4:
            case TEXTURE_FORMAT_RGBA_ASTC_5X5:
            case TEXTURE_FORMAT_RGBA_ASTC_6X5:
            case TEXTURE_FORMAT_RGBA_ASTC_6X6:
            case TEXTURE_FORMAT_RGBA_ASTC_8X5:
            case TEXTURE_FORMAT_RGBA_ASTC_8X6:
            case TEXTURE_FORMAT_RGBA_ASTC_8X8:
            case TEXTURE_FORMAT_RGBA_ASTC_10X5:
            case TEXTURE_FORMAT_RGBA_ASTC_10X6:
            case TEXTURE_FORMAT_RGBA_ASTC_10X8:
            case TEXTURE_FORMAT_RGBA_ASTC_10X10:
            case TEXTURE_FORMAT_RGBA_ASTC_12X10:
            case TEXTURE_FORMAT_RGBA_ASTC_12X12:
            case TEXTURE_FORMAT_RGB_BC1:
            case TEXTURE_FORMAT_RGBA_BC3:
            case TEXTURE_FORMAT_R_BC4:
            case TEXTURE_FORMAT_RG_BC5:
            case TEXTURE_FORMAT_RGBA_BC7:
                if (params.m_DataSize > 0)
                {
                    if (tex->m_Type == TEXTURE_TYPE_2D)
                    {
                        if (params.m_SubUpdate)
                        {
                            glCompressedTexSubImage2D(GL_TEXTURE_2D, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, params.m_Data);
                        }
                        else
                        {
                            glCompressedTexImage2D(GL_TEXTURE_2D, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, params.m_Data);
                        }
                        CHECK_GL_ERROR;
                    }
                    else if (tex->m_Type == TEXTURE_TYPE_2D_ARRAY)
                    {
                        if (params.m_SubUpdate)
                        {
                            DMGRAPHICS_COMPRESSED_TEX_SUB_IMAGE_3D(GL_TEXTURE_2D_ARRAY, params.m_MipMap, params.m_X, params.m_Y, params.m_Slice, params.m_Width, params.m_Height, params.m_LayerCount, gl_format, params.m_DataSize, params.m_Data);
                        }
                        else
                        {
                            DMGRAPHICS_COMPRESSED_TEX_IMAGE_3D(GL_TEXTURE_2D_ARRAY, params.m_MipMap, gl_format, params.m_Width, params.m_Height, params.m_LayerCount, 0, params.m_DataSize * params.m_LayerCount, params.m_Data);
                        }
                        CHECK_GL_ERROR;
                    }
                    else if (tex->m_Type == TEXTURE_TYPE_3D)
                    {
                        if (params.m_SubUpdate)
                        {
                            DMGRAPHICS_COMPRESSED_TEX_SUB_IMAGE_3D(GL_TEXTURE_3D, params.m_MipMap, params.m_X, params.m_Y, params.m_Z, params.m_Width, params.m_Height, params.m_Depth, gl_format, params.m_DataSize, params.m_Data);
                        }
                        else
                        {
                            DMGRAPHICS_COMPRESSED_TEX_IMAGE_3D(GL_TEXTURE_3D, params.m_MipMap, gl_format, params.m_Width, params.m_Height, params.m_Depth, 0, params.m_DataSize * params.m_Depth, params.m_Data);
                        }
                        CHECK_GL_ERROR;
                    }
                    else if (tex->m_Type == TEXTURE_TYPE_CUBE_MAP)
                    {
                        const char* p = (const char*) params.m_Data;
                        if (params.m_SubUpdate)
                        {
                            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 0);
                            CHECK_GL_ERROR;
                            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 1);
                            CHECK_GL_ERROR;
                            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 2);
                            CHECK_GL_ERROR;
                            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 3);
                            CHECK_GL_ERROR;
                            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 4);
                            CHECK_GL_ERROR;
                            glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 5);
                            CHECK_GL_ERROR;
                        }
                        else
                        {
                            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 0);
                            CHECK_GL_ERROR;
                            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 1);
                            CHECK_GL_ERROR;
                            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 2);
                            CHECK_GL_ERROR;
                            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 3);
                            CHECK_GL_ERROR;
                            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 4);
                            CHECK_GL_ERROR;
                            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 5);
                            CHECK_GL_ERROR;
                        }
                    }
                    else
                    {
                        assert(0);
                    }
                }
                break;
            default:
                assert(0);
                break;
            }
        }

        glBindTexture(type, 0);
        CHECK_GL_ERROR;

        if (unpackAlignment != 4)
        {
            // Restore to default
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            CHECK_GL_ERROR;
        }
    }

    // NOTE: This is an approximation
    static uint32_t OpenGLGetTextureResourceSize(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        if (!tex)
        {
            return 0;
        }

        uint32_t size_total = 0;
        uint32_t size = tex->m_ResourceSize; // Size for mip 0
        for(uint32_t i = 0; i < tex->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        size_total *= dmMath::Max((uint16_t) 1, tex->m_Depth);
        return size_total + sizeof(OpenGLTexture);
    }

    static uint16_t OpenGLGetTextureWidth(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Width : 0;
    }

    static uint16_t OpenGLGetTextureHeight(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Height : 0;
    }

    static uint16_t OpenGLGetOriginalTextureWidth(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_OriginalWidth : 0;
    }

    static uint16_t OpenGLGetOriginalTextureHeight(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_OriginalHeight : 0;
    }

    static TextureType OpenGLGetTextureType(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Type : TEXTURE_TYPE_2D;
    }

    static uint16_t OpenGLGetTextureDepth(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_Depth : 0;
    }

    static uint8_t OpenGLGetTextureMipmapCount(HContext _context, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        return tex ? tex->m_MipMapCount : 0;
    }

#ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT
    static bool GetTextureUniform(OpenGLContext* context, uint32_t unit, int32_t* index, Type* type)
    {
        uint32_t num_uniforms = context->m_CurrentProgram->m_BaseProgram.m_Uniforms.Size();
        uint32_t texture_unit = 0;
        for (int i = 0; i < num_uniforms; ++i)
        {
            if (IsTypeTextureType(context->m_CurrentProgram->m_BaseProgram.m_Uniforms[i].m_Type))
            {
                if (texture_unit == unit)
                {
                    *index = i;
                    *type = context->m_CurrentProgram->m_BaseProgram.m_Uniforms[i].m_Type;
                    return true;
                }
                texture_unit++;
            }
        }
        return false;
    }
#endif

    static bool BindComputeImage(OpenGLContext* context, OpenGLTexture* tex, uint32_t unit, uint32_t id_index, bool do_unbind = false)
    {
    #ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT
        if (!context->m_ComputeSupport)
            return false;

        int32_t uniform_index;
        Type type;

        if (GetTextureUniform(context, unit, &uniform_index, &type))
        {
            // Binding a image texture to a imagexd slot, otherwise we'll bind it as a combined sampler
            if (type == TYPE_IMAGE_2D || type == TYPE_IMAGE_3D)
            {
                GLenum access            = DMGRAPHICS_READ_ONLY;
                GLenum gl_format         = 0;
                GLenum gl_type           = GL_UNSIGNED_BYTE;
                GLint gl_internal_format = 0;
                GLuint id                = 0;
                GetOpenGLSetTextureParams(context, tex->m_Params.m_Format, gl_internal_format, gl_format, gl_type);

                // We need a valid texture regardless of bind/unbind
                if (!do_unbind)
                {
                    id     = GetGLHandle(context, tex->m_TextureIds[id_index]);
                    access = tex->m_UsageHintFlags & TEXTURE_USAGE_FLAG_STORAGE ? DMGRAPHICS_READ_WRITE : DMGRAPHICS_READ_ONLY;
                }
                glBindImageTexture(unit, id, 0, GL_FALSE, 0, access, gl_internal_format);
                CHECK_GL_ERROR;

                return true;
            }
        }
    #endif
        return false;
    }

    static void OpenGLEnableTexture(HContext _context, uint32_t unit, uint8_t id_index, HTexture texture)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(GetAssetType(texture) == ASSET_TYPE_TEXTURE);

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);
        if (!tex)
        {
            return;
        }

        assert(id_index < tex->m_NumTextureIds);

#if !defined(GL_ES_VERSION_3_0) && defined(GL_ES_VERSION_2_0) && !defined(__EMSCRIPTEN__)  && !defined(ANDROID)
        glEnable(GL_TEXTURE_2D);
        CHECK_GL_ERROR;
#endif

        glActiveTexture(TEXTURE_UNIT_NAMES[unit]);
        CHECK_GL_ERROR;

        bool bind_as_texture = true;
        if (tex->m_Type == TEXTURE_TYPE_IMAGE_2D || tex->m_Type == TEXTURE_TYPE_IMAGE_3D)
        {
            bind_as_texture = !BindComputeImage(context, tex, unit, id_index);
        }

        if (bind_as_texture)
        {
            glBindTexture(GetOpenGLTextureType(tex->m_Type), GetGLHandle(context, tex->m_TextureIds[id_index]));
            CHECK_GL_ERROR;
            OpenGLSetTextureParams(_context, texture, tex->m_Params.m_MinFilter, tex->m_Params.m_MagFilter, tex->m_Params.m_UWrap, tex->m_Params.m_VWrap, 1.0f);
        }
    }

    static void OpenGLDisableTexture(HContext _context, uint32_t unit, HTexture texture)
    {
#if !defined(GL_ES_VERSION_3_0) && defined(GL_ES_VERSION_2_0) && !defined(__EMSCRIPTEN__)  && !defined(ANDROID)
        glEnable(GL_TEXTURE_2D);
        CHECK_GL_ERROR;
#endif

        OpenGLContext* context = (OpenGLContext*) _context;

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        OpenGLTexture* tex = GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, texture);

        if (!tex)
        {
            return;
        }

        glActiveTexture(TEXTURE_UNIT_NAMES[unit]);
        CHECK_GL_ERROR;

        bool unbind_as_texture = true;
        if (tex->m_Type == TEXTURE_TYPE_IMAGE_2D || tex->m_Type == TEXTURE_TYPE_IMAGE_3D)
        {
            unbind_as_texture = !BindComputeImage(context, tex, unit, 0, true);
        }

        if (unbind_as_texture)
        {
            glBindTexture(GetOpenGLTextureType(tex->m_Type), 0);
            CHECK_GL_ERROR;
        }
    }

    static void OpenGLReadPixels(HContext _context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(buffer_size >= width * height * 4);
        glReadPixels(x, y, width, height,
                     GL_BGRA,
                     GL_UNSIGNED_BYTE,
                     buffer);
        CHECK_GL_ERROR;
        unsigned int *pixels = (unsigned int*)buffer;
        // flip vertically
        for (uint32_t yi = 0; yi < (height / 2); ++yi)
        {
            for (uint32_t xi = 0; xi < width; ++xi)
            {
                unsigned int offset1 = xi + (yi * width);
                unsigned int offset2 = xi + ((height - 1 - yi) * width);
                unsigned int pixel1 = pixels[offset1];
                unsigned int pixel2 = pixels[offset2];
                pixels[offset1] = pixel2;
                pixels[offset2] = pixel1;
            }
        }
    }

    static void OpenGLEnableState(HContext _context, State state)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
    #if !defined(GL_ES_VERSION_2_0)
        if (state == STATE_ALPHA_TEST)
        {
            dmLogOnceWarning("Enabling the graphics.STATE_ALPHA_TEST state is not supported in this OpenGL version.");
            return;
        }
    #endif
        glEnable(GetOpenGLState(state));
        CHECK_GL_ERROR

        SetPipelineStateValue(context->m_PipelineState, state, 1);
    }

    static void OpenGLDisableState(HContext _context, State state)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
    #if !defined(GL_ES_VERSION_2_0)
        if (state == STATE_ALPHA_TEST)
        {
            dmLogOnceWarning("Disabling the graphics.STATE_ALPHA_TEST state is not supported in this OpenGL version.");
            return;
        }
    #endif
        glDisable(GetOpenGLState(state));
        CHECK_GL_ERROR

        SetPipelineStateValue(context->m_PipelineState, state, 0);
    }

    static void OpenGLSetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(_context);
        GLenum blend_factor_lut[] = {
            GL_ZERO,
            GL_ONE,
            GL_SRC_COLOR,
            GL_ONE_MINUS_SRC_COLOR,
            GL_DST_COLOR,
            GL_ONE_MINUS_DST_COLOR,
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA,
            GL_DST_ALPHA,
            GL_ONE_MINUS_DST_ALPHA,
            GL_SRC_ALPHA_SATURATE,
        #if !defined (GL_ARB_imaging)
            0x8001,
            0x8002,
            0x8003,
            0x8004,
        #else
            GL_CONSTANT_COLOR,
            GL_ONE_MINUS_CONSTANT_COLOR,
            GL_CONSTANT_ALPHA,
            GL_ONE_MINUS_CONSTANT_ALPHA,
        #endif
        };

        glBlendFunc(blend_factor_lut[source_factor], blend_factor_lut[destinaton_factor]);
        CHECK_GL_ERROR

        OpenGLContext* context = (OpenGLContext*) _context;

        context->m_PipelineState.m_BlendSrcFactor = source_factor;
        context->m_PipelineState.m_BlendDstFactor = destinaton_factor;
    }

    static void OpenGLSetColorMask(HContext _context, bool red, bool green, bool blue, bool alpha)
    {
        assert(_context);
        glColorMask(red, green, blue, alpha);
        CHECK_GL_ERROR;

        OpenGLContext* context = (OpenGLContext*) _context;

        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;
        context->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void OpenGLSetDepthMask(HContext _context, bool enable_mask)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glDepthMask(enable_mask);
        CHECK_GL_ERROR;

        context->m_PipelineState.m_WriteDepth = enable_mask;
    }

    static GLenum GetOpenGLCompareFunc(CompareFunc func)
    {
        GLenum func_lut[] = {
            GL_NEVER,
            GL_LESS,
            GL_LEQUAL,
            GL_GREATER,
            GL_GEQUAL,
            GL_EQUAL,
            GL_NOTEQUAL,
            GL_ALWAYS,
        };

        return func_lut[func];
    }

    static GLenum GetOpenGLFaceTypeFunc(FaceType face_type)
    {
        const GLenum face_type_lut[] = {
            GL_FRONT,
            GL_BACK,
            GL_FRONT_AND_BACK,
        };

        return face_type_lut[face_type];
    }

    static void OpenGLSetDepthFunc(HContext _context, CompareFunc func)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glDepthFunc(GetOpenGLCompareFunc(func));
        CHECK_GL_ERROR
        context->m_PipelineState.m_DepthTestFunc = func;
    }

    static void OpenGLSetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glScissor((GLint)x, (GLint)y, (GLint)width, (GLint)height);
        CHECK_GL_ERROR;
    }

    static void OpenGLSetStencilMask(HContext _context, uint32_t mask)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glStencilMask(mask);
        CHECK_GL_ERROR;
        context->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void OpenGLSetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(_context);
        glStencilFunc(GetOpenGLCompareFunc(func), ref, mask);
        CHECK_GL_ERROR

        OpenGLContext* context = (OpenGLContext*) _context;
        context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void OpenGLSetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(_context);
        glStencilFuncSeparate(GetOpenGLFaceTypeFunc(face_type), GetOpenGLCompareFunc(func), ref, mask);
        CHECK_GL_ERROR

        OpenGLContext* context = (OpenGLContext*) _context;
        if (face_type == FACE_TYPE_BACK)
        {
            context->m_PipelineState.m_StencilBackTestFunc = (uint8_t) func;
        }
        else
        {
            context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        }
        context->m_PipelineState.m_StencilReference   = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask = (uint8_t) mask;
    }

    static void OpenGLSetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(_context);
        const GLenum stencil_op_lut[] = {
            GL_KEEP,
            GL_ZERO,
            GL_REPLACE,
            GL_INCR,
            GL_INCR_WRAP,
            GL_DECR,
            GL_DECR_WRAP,
            GL_INVERT,
        };

        glStencilOp(stencil_op_lut[sfail], stencil_op_lut[dpfail], stencil_op_lut[dppass]);
        CHECK_GL_ERROR;

        OpenGLContext* context = (OpenGLContext*) _context;
        context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        context->m_PipelineState.m_StencilBackOpFail       = sfail;
        context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void OpenGLSetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(_context);
        const GLenum stencil_op_lut[] = {
            GL_KEEP,
            GL_ZERO,
            GL_REPLACE,
            GL_INCR,
            GL_INCR_WRAP,
            GL_DECR,
            GL_DECR_WRAP,
            GL_INVERT,
        };

        glStencilOpSeparate(GetOpenGLFaceTypeFunc(face_type), stencil_op_lut[sfail], stencil_op_lut[dpfail], stencil_op_lut[dppass]);
        CHECK_GL_ERROR;

        OpenGLContext* context = (OpenGLContext*) _context;
        if (face_type == FACE_TYPE_BACK)
        {
            context->m_PipelineState.m_StencilBackOpFail       = sfail;
            context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
            context->m_PipelineState.m_StencilBackOpPass       = dppass;
        }
        else
        {
            context->m_PipelineState.m_StencilFrontOpFail      = sfail;
            context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
            context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        }
    }

    static void OpenGLSetCullFace(HContext _context, FaceType face_type)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glCullFace(GetOpenGLFaceTypeFunc(face_type));
        CHECK_GL_ERROR

        context->m_PipelineState.m_CullFaceType = face_type;
    }

    static void OpenGLSetFaceWinding(HContext _context, FaceWinding face_winding)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);

        const GLenum face_winding_lut[] = {
            GL_CCW,
            GL_CW,
        };

        glFrontFace(face_winding_lut[face_winding]);

        context->m_PipelineState.m_FaceWinding = face_winding;
    }

    static void OpenGLSetPolygonOffset(HContext _context, float factor, float units)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        assert(context);
        glPolygonOffset(factor, units);
        CHECK_GL_ERROR;
    }

    static bool OpenGLIsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
    {
        if (asset_handle == 0)
        {
            return false;
        }

        OpenGLContext* context = (OpenGLContext*) _context;
        AssetType type         = GetAssetType(asset_handle);

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetHandleContainerMutex);
        if (type == ASSET_TYPE_TEXTURE)
        {
            return GetAssetFromContainer<OpenGLTexture>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            return GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    // DMSDK
    uint32_t OpenGLGetRenderTargetId(HContext _context, HRenderTarget render_target)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        OpenGLRenderTarget* rt = GetAssetFromContainer<OpenGLRenderTarget>(context->m_AssetHandleContainer, render_target);
        return GetGLHandle(context, rt->m_Id);
    }

    static void OpenGLInvalidateGraphicsHandles(HContext _context)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_GLHandlesData.m_Mutex);
        // Set all handles to 0. It indicates that handles not valid.
        memset(context->m_GLHandlesData.m_AllGLHandles.Begin(), 0, (context->m_GLHandlesData.m_AllGLHandles.End() - context->m_GLHandlesData.m_AllGLHandles.Begin()) * sizeof(uint32_t));
    }

    static void OpenGLGetViewport(HContext _context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
    {
        OpenGLContext* context = (OpenGLContext*) _context;
        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        *x = vp[0], *y = vp[1], *width = vp[2], *height = vp[3];
    }

    GLenum TEXTURE_UNIT_NAMES[32] =
    {
        GL_TEXTURE0,
        GL_TEXTURE1,
        GL_TEXTURE2,
        GL_TEXTURE3,
        GL_TEXTURE4,
        GL_TEXTURE5,
        GL_TEXTURE6,
        GL_TEXTURE7,
        GL_TEXTURE8,
        GL_TEXTURE9,
        GL_TEXTURE10,
        GL_TEXTURE11,
        GL_TEXTURE12,
        GL_TEXTURE13,
        GL_TEXTURE14,
        GL_TEXTURE15,
        GL_TEXTURE16,
        GL_TEXTURE17,
        GL_TEXTURE18,
        GL_TEXTURE19,
        GL_TEXTURE20,
        GL_TEXTURE21,
        GL_TEXTURE22,
        GL_TEXTURE23,
        GL_TEXTURE24,
        GL_TEXTURE25,
        GL_TEXTURE26,
        GL_TEXTURE27,
        GL_TEXTURE28,
        GL_TEXTURE29,
        GL_TEXTURE30,
        GL_TEXTURE31
    };

    static GraphicsAdapterFunctionTable OpenGLRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, OpenGL);
        return fn_table;
    }
}
