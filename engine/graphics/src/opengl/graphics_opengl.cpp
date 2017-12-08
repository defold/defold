#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <dlib/dlib.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/hash.h>
#include <dlib/align.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/time.h>

#ifdef __EMSCRIPTEN__
    #include <emscripten/emscripten.h>
#endif

#include "../graphics.h"
#include "../graphics_native.h"
#include "async/job_queue.h"
#include "graphics_opengl.h"

#if defined(__MACH__) && !( defined(__arm__) || defined(__arm64__) )
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>

#if defined(__linux__) && !defined(ANDROID)
#include <GL/glext.h>

#elif defined (ANDROID)
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>

#elif defined (__MACH__)

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
typedef void (APIENTRY * PFNGLBINDPROGRAMARBPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLDELETEPROGRAMSARBPROC) (GLsizei, const GLuint*);
typedef void (APIENTRY * PFNGLPROGRAMSTRINGARBPROC) (GLenum, GLenum, GLsizei, const GLvoid *);
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

PFNGLGENPROGRAMARBPROC glGenProgramsARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLVERTEXPARAMFLOAT4ARBPROC glProgramLocalParameter4fARB = NULL;
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

#elif defined(__EMSCRIPTEN__)
#include <GL/glext.h>
#if defined GL_ES_VERSION_2_0
#undef GL_ARRAY_BUFFER_ARB
#undef GL_ELEMENT_ARRAY_BUFFER_ARB
#endif
#else
#error "Platform not supported."
#endif

using namespace Vectormath::Aos;

// OpenGLES compatibility
#if defined GL_ES_VERSION_2_0
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
#endif

namespace dmGraphics
{
void LogGLError(GLint err)
{
#ifdef GL_ES_VERSION_2_0
    dmLogError("gl error %d\n", err);
#else
    dmLogError("gl error %d: %s\n", err, gluErrorString(err));
#endif
}

#define CHECK_GL_ERROR \
    { \
        if(g_Context->m_VerifyGraphicsCalls) { \
            GLint err = glGetError(); \
            if (err != 0) \
            { \
                LogGLError(err); \
                assert(0); \
            } \
        } \
    }\

#define CLEAR_GL_ERROR \
    { \
        if(g_Context->m_VerifyGraphicsCalls) { \
            glGetError(); \
        } \
    }\


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

#ifdef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE
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
            assert(0);
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

    struct TextureParamsAsync
    {
        HTexture m_Texture;
        TextureParams m_Params;
    };
    dmArray<TextureParamsAsync> g_TextureParamsAsyncArray;
    dmIndexPool16 g_TextureParamsAsyncArrayIndices;
    dmArray<HTexture> g_PostDeleteTexturesArray;
    void PostDeleteTextures(bool);

    extern BufferType BUFFER_TYPES[MAX_BUFFER_TYPE_COUNT];
    extern GLenum TEXTURE_UNIT_NAMES[32];

    Context* g_Context = 0x0;

    Context::Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_ModificationVersion = 1;
        m_VerifyGraphicsCalls = params.m_VerifyGraphicsCalls;
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        // Formats supported on all platforms
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
    }

    HContext NewContext(const ContextParams& params)
    {
        if (g_Context == 0x0)
        {
            if (glfwInit() == GL_FALSE)
            {
                dmLogError("Could not initialize glfw.");
                return 0x0;
            }
            g_Context = new Context(params);
            g_Context->m_AsyncMutex = dmMutex::New();
            return g_Context;
        }
        return 0x0;
    }

    void DeleteContext(HContext context)
    {
        if (context != 0x0)
        {
            if(g_Context->m_AsyncMutex)
            {
                dmMutex::Delete(g_Context->m_AsyncMutex);
            }
            delete context;
            g_Context = 0x0;
        }
    }

    bool Initialize()
    {
        // NOTE: We do glfwInit as glfw doesn't cleanup menus properly on OSX.
        return (glfwInit() == GL_TRUE);
    }

    void Finalize()
    {
        glfwTerminate();
    }

    void OnWindowResize(int width, int height)
    {
        assert(g_Context);
        g_Context->m_WindowWidth = (uint32_t)width;
        g_Context->m_WindowHeight = (uint32_t)height;
        if (g_Context->m_WindowResizeCallback != 0x0)
            g_Context->m_WindowResizeCallback(g_Context->m_WindowResizeCallbackUserData, (uint32_t)width, (uint32_t)height);
    }

    int OnWindowClose()
    {
        assert(g_Context);
        if (g_Context->m_WindowCloseCallback != 0x0)
            return g_Context->m_WindowCloseCallback(g_Context->m_WindowCloseCallbackUserData);
        // Close by default
        return 1;
    }

    static void OnWindowFocus(int focus)
    {
        assert(g_Context);
        if (g_Context->m_WindowFocusCallback != 0x0)
            g_Context->m_WindowFocusCallback(g_Context->m_WindowFocusCallbackUserData, focus);
    }

    static bool IsExtensionSupported(const char* extension, const GLubyte* extensions)
    {
        // Copied from http://www.opengl.org/archives/resources/features/OGLextensions/
        const GLubyte *start;

        /* Extension names should not have spaces. */
        GLubyte* where = (GLubyte *) strchr(extension, ' ');
        if (where || *extension == '\0')
            return false;

        /* It takes a bit of care to be fool-proof about parsing the
           OpenGL extensions string. Don't be fooled by sub-strings,
           etc. */

        start = extensions;
        GLubyte* terminator;
        for (;;) {
            where = (GLubyte *) strstr((const char *) start, extension);
            if (!where)
                break;
            terminator = where + strlen(extension);
            if (where == start || *(where - 1) == ' ')
                if (*terminator == ' ' || *terminator == '\0')
                    return true;
            start = terminator;
        }

        return false;
    }

    static bool ValidateAsyncJobProcessing(HContext context)
    {
        // Test async texture access
        {
            TextureCreationParams tcp;
            tcp.m_Width = tcp.m_OriginalWidth = tcp.m_Height = tcp.m_OriginalHeight = 2;
            tcp.m_Type = TEXTURE_TYPE_2D;
            HTexture texture = dmGraphics::NewTexture(context, tcp);

            DM_ALIGNED(16) const uint32_t data[] = { 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff };
            TextureParams params;
            params.m_Format = TEXTURE_FORMAT_RGBA;
            params.m_Width = tcp.m_Width;
            params.m_Height = tcp.m_Height;
            params.m_Data = data;
            params.m_DataSize = sizeof(data);
            params.m_MipMap = 0;
            SetTextureAsync(texture, params);

            while(GetTextureStatusFlags(texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
                dmTime::Sleep(100);

            DM_ALIGNED(16) uint8_t gpu_data[sizeof(data)];
            memset(gpu_data, 0x0, sizeof(gpu_data));
            glBindTexture(GL_TEXTURE_2D, texture->m_Texture);
            CHECK_GL_ERROR

            GLuint osfb;
            glGenFramebuffers(1, &osfb);
            CHECK_GL_ERROR
            glBindFramebuffer(GL_FRAMEBUFFER, osfb);
            CHECK_GL_ERROR;

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->m_Texture, 0);
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
            glBindFramebuffer(GL_FRAMEBUFFER, glfwGetDefaultFramebuffer());
            CHECK_GL_ERROR;
            glDeleteFramebuffers(1, &osfb);
            DeleteTexture(texture);

            if(memcmp(data, gpu_data, sizeof(data))!=0)
            {
                dmLogDebug("ValidateAsyncJobProcessing cpu<->gpu data check failed. Unable to verify async texture access integrity.");
                return false;
            }
        }

        return true;
    }

    WindowResult OpenWindow(HContext context, WindowParams *params)
    {
        assert(context);
        assert(params);

        if (context->m_WindowOpened) return WINDOW_RESULT_ALREADY_OPENED;

        if (params->m_HighDPI) {
            glfwOpenWindowHint(GLFW_WINDOW_HIGH_DPI, 1);
        }


        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params->m_Samples);
        int mode = GLFW_WINDOW;
        if (params->m_Fullscreen)
            mode = GLFW_FULLSCREEN;
        if (!glfwOpenWindow(params->m_Width, params->m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

#if defined (_WIN32)
#define GET_PROC_ADDRESS(function, name, type)\
        function = (type)wglGetProcAddress(name);\
        if (function == 0x0)\
        {\
            function = (type)wglGetProcAddress(name "ARB");\
        }\
        if (function == 0x0)\
        {\
            function = (type)wglGetProcAddress(name "EXT");\
        }\
        if (function == 0x0)\
        {\
            dmLogError("Could not find gl function '%s'.", name);\
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;\
        }

        GET_PROC_ADDRESS(glGenProgramsARB, "glGenPrograms", PFNGLGENPROGRAMARBPROC);
        GET_PROC_ADDRESS(glBindProgramARB, "glBindProgram", PFNGLBINDPROGRAMARBPROC);
        GET_PROC_ADDRESS(glDeleteProgramsARB, "glDeletePrograms", PFNGLDELETEPROGRAMSARBPROC);
        GET_PROC_ADDRESS(glProgramStringARB, "glProgramString", PFNGLPROGRAMSTRINGARBPROC);
        GET_PROC_ADDRESS(glProgramLocalParameter4fARB, "glProgramLocalParameter4f", PFNGLVERTEXPARAMFLOAT4ARBPROC);
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
        GET_PROC_ADDRESS(glMapBufferARB, "glMapBuffer", PFNGLMAPBUFFERPROC);
        GET_PROC_ADDRESS(glUnmapBufferARB, "glUnmapBuffer", PFNGLUNMAPBUFFERPROC);
        GET_PROC_ADDRESS(glActiveTexture, "glActiveTexture", PFNGLACTIVETEXTUREPROC);
        GET_PROC_ADDRESS(glCheckFramebufferStatus, "glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC);
        GET_PROC_ADDRESS(glGetAttribLocation, "glGetAttribLocation", PFNGLGETATTRIBLOCATIONPROC);
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

#undef GET_PROC_ADDRESS
#endif

        glfwSetWindowTitle(params->m_Title);
        glfwSetWindowSizeCallback(OnWindowResize);
        glfwSetWindowCloseCallback(OnWindowClose);
        glfwSetWindowFocusCallback(OnWindowFocus);
        glfwSwapInterval(1);
        CHECK_GL_ERROR

        context->m_WindowResizeCallback         = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback          = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData  = params->m_CloseCallbackUserData;
        context->m_WindowFocusCallback          = params->m_FocusCallback;
        context->m_WindowFocusCallbackUserData  = params->m_FocusCallbackUserData;
        context->m_WindowOpened = 1;
        context->m_Width = params->m_Width;
        context->m_Height = params->m_Height;
        // read back actual window size
        int width, height;
        glfwGetWindowSize(&width, &height);
        context->m_WindowWidth = (uint32_t)width;
        context->m_WindowHeight = (uint32_t)height;
        context->m_Dpi = 0;

        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: OpenGL");
            dmLogInfo("Renderer: %s\n", (char *) glGetString(GL_RENDERER));
            dmLogInfo("Version: %s\n", (char *) glGetString(GL_VERSION));
            dmLogInfo("Vendor: %s\n", (char *) glGetString(GL_VENDOR));
            dmLogInfo("Extensions: %s\n", (char *) glGetString(GL_EXTENSIONS));
        }

#if defined(__MACH__) && !( defined(__arm__) || defined(__arm64__) )
        ProcessSerialNumber psn;
        OSErr err;

        // Move window to front. Required if running without application bundle.
        err = GetCurrentProcess( &psn );
        if (err == noErr)
            (void) SetFrontProcess( &psn );
#endif

        // Check texture format support
        const GLubyte* extensions = glGetString(GL_EXTENSIONS);
        if (IsExtensionSupported("GL_IMG_texture_compression_pvrtc", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        }
        if (IsExtensionSupported("GL_EXT_texture_compression_dxt1", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_DXT1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_DXT1;
        }
        if (IsExtensionSupported("GL_EXT_texture_compression_dxt3", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_DXT3;
        }
        if (IsExtensionSupported("GL_EXT_texture_compression_dxt5", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_DXT5;
        }
        if (IsExtensionSupported("GL_OES_compressed_ETC1_RGB8_texture", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
        }

#if defined (__EMSCRIPTEN__)
        // webgl GL_DEPTH_STENCIL_ATTACHMENT for stenciling and GL_DEPTH_COMPONENT16 for depth only by specifications, even though it reports 24-bit depth and no packed depth stencil extensions.
        context->m_PackedDepthStencil = 1;
        context->m_DepthBufferBits = 16;
#else

        if ((IsExtensionSupported("GL_OES_packed_depth_stencil", extensions)) || (IsExtensionSupported("GL_EXT_packed_depth_stencil", extensions)))
        {
            context->m_PackedDepthStencil = 1;
        }
        GLint depth_buffer_bits;
        glGetIntegerv( GL_DEPTH_BITS, &depth_buffer_bits );
        context->m_DepthBufferBits = (uint32_t) depth_buffer_bits;
#endif

        GLint max_texture_size;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        context->m_MaxTextureSize = max_texture_size;

        JobQueueInitialize();
        if(JobQueueIsAsync())
        {
            if(!ValidateAsyncJobProcessing(context))
            {
                dmLogDebug("AsyncInitialize: Failed to verify async job processing. Fallback to single thread processing.");
                JobQueueFinalize();
            }
        }
        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            JobQueueFinalize();
            PostDeleteTextures(true);
            glfwCloseWindow();
            context->m_WindowResizeCallback = 0x0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
            context->m_WindowOpened = 0;
        }
    }

    void IconifyWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            glfwIconifyWindow();
        }
    }

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
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

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        assert(context);
        if (context->m_WindowOpened)
            return glfwGetWindowParam(state);
        else
            return 0;
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        assert(context);
        return context->m_Dpi;
    }

    uint32_t GetWidth(HContext context)
    {
        assert(context);
        return context->m_Width;
    }

    uint32_t GetHeight(HContext context)
    {
        assert(context);
        return context->m_Height;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        assert(context);
        return context->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        assert(context);
        return context->m_WindowHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_Width = width;
            context->m_Height = height;
            glfwSetWindowSize((int)width, (int)height);
            int window_width, window_height;
            glfwGetWindowSize(&window_width, &window_height);
            context->m_WindowWidth = window_width;
            context->m_WindowHeight = window_height;
            // The callback is not called from glfw when the size is set manually
            if (context->m_WindowResizeCallback)
            {
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, window_width, window_height);
            }
        }
    }

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);
        DM_PROFILE(Graphics, "Clear");

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;
        glClearColor(r, g, b, a);
        CHECK_GL_ERROR

        glClearDepth(depth);
        CHECK_GL_ERROR

        glClearStencil(stencil);
        CHECK_GL_ERROR

        glClear(flags);
        CHECK_GL_ERROR
    }

    void Flip(HContext context)
    {
        DM_PROFILE(VSync, "Wait");
        PostDeleteTextures(false);
        glfwSwapBuffers();
        CHECK_GL_ERROR
    }

    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {
        glfwSwapInterval(swap_interval);
    }

    #define WRAP_GLFW_NATIVE_HANDLE_CALL(return_type, func_name) return_type GetNative##func_name() { return glfwGet##func_name(); }

    WRAP_GLFW_NATIVE_HANDLE_CALL(id, iOSUIWindow);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, iOSUIView);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, iOSEAGLContext);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, OSXNSWindow);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, OSXNSView);
    WRAP_GLFW_NATIVE_HANDLE_CALL(id, OSXNSOpenGLContext);
    WRAP_GLFW_NATIVE_HANDLE_CALL(HWND, WindowsHWND);
    WRAP_GLFW_NATIVE_HANDLE_CALL(HGLRC, WindowsHGLRC);
    WRAP_GLFW_NATIVE_HANDLE_CALL(EGLContext, AndroidEGLContext);
    WRAP_GLFW_NATIVE_HANDLE_CALL(EGLSurface, AndroidEGLSurface);
    WRAP_GLFW_NATIVE_HANDLE_CALL(JavaVM*, AndroidJavaVM);
    WRAP_GLFW_NATIVE_HANDLE_CALL(jobject, AndroidActivity);
    WRAP_GLFW_NATIVE_HANDLE_CALL(Window, X11Window);
    WRAP_GLFW_NATIVE_HANDLE_CALL(GLXContext, X11GLXContext);

    #undef WRAP_GLFW_NATIVE_HANDLE_CALL

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        uint32_t buffer = 0;
        glGenBuffersARB(1, &buffer);
        CHECK_GL_ERROR
        SetVertexBufferData(buffer, size, data, buffer_usage);
        return buffer;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        GLuint b = (GLuint) buffer;
        glDeleteBuffersARB(1, &b);
        CHECK_GL_ERROR
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(Graphics, "SetVertexBufferData");
        // NOTE: Android doesn't seem to like zero-sized vertex buffers
        if (size == 0) {
            return;
        }
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, size, data, buffer_usage);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(Graphics, "SetVertexBufferSubData");
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferSubDataARB(GL_ARRAY_BUFFER_ARB, offset, size, data);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        uint32_t buffer = 0;
        glGenBuffersARB(1, &buffer);
        CHECK_GL_ERROR
        SetIndexBufferData(buffer, size, data, buffer_usage);
        return buffer;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        GLuint b = (GLuint) buffer;
        glDeleteBuffersARB(1, &b);
        CHECK_GL_ERROR
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(Graphics, "SetIndexBufferData");
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, size, data, buffer_usage);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(Graphics, "SetIndexBufferSubData");
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer);
        CHECK_GL_ERROR
        glBufferSubDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, offset, size, data);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    static uint32_t GetTypeSize(Type type)
    {
        uint32_t size = 0;
        switch (type)
        {
            case TYPE_BYTE:
            case TYPE_UNSIGNED_BYTE:
                size = 1;
                break;

            case TYPE_SHORT:
            case TYPE_UNSIGNED_SHORT:
                size = 2;
                break;

            case TYPE_INT:
            case TYPE_UNSIGNED_INT:
            case TYPE_FLOAT:
                size = 4;
                break;

            default:
                assert(0);
                break;
        }
        return size;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        HVertexDeclaration vd = NewVertexDeclaration(context, element, count);
        vd->m_Stride = stride;
        return vd;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration;
        memset(vd, 0, sizeof(VertexDeclaration));

        vd->m_Stride = 0;
        assert(count < (sizeof(vd->m_Streams) / sizeof(vd->m_Streams[0]) ) );

        for (uint32_t i=0; i<count; i++)
        {
            vd->m_Streams[i].m_Name = element[i].m_Name;
            vd->m_Streams[i].m_LogicalIndex = i;
            vd->m_Streams[i].m_PhysicalIndex = -1;
            vd->m_Streams[i].m_Size = element[i].m_Size;
            vd->m_Streams[i].m_Type = element[i].m_Type;
            vd->m_Streams[i].m_Normalize = element[i].m_Normalize;
            vd->m_Streams[i].m_Offset = vd->m_Stride;

            vd->m_Stride += element[i].m_Size * GetTypeSize(element[i].m_Type);
        }
        vd->m_StreamCount = count;
        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        assert(vertex_declaration);
        #define BUFFER_OFFSET(i) ((char*)0x0 + (i))

        glBindBufferARB(GL_ARRAY_BUFFER, vertex_buffer);
        CHECK_GL_ERROR

        for (uint32_t i=0; i<vertex_declaration->m_StreamCount; i++)
        {
            glEnableVertexAttribArray(vertex_declaration->m_Streams[i].m_LogicalIndex);
            CHECK_GL_ERROR
            glVertexAttribPointer(
                    vertex_declaration->m_Streams[i].m_LogicalIndex,
                    vertex_declaration->m_Streams[i].m_Size,
                    vertex_declaration->m_Streams[i].m_Type,
                    vertex_declaration->m_Streams[i].m_Normalize,
                    vertex_declaration->m_Stride,
            BUFFER_OFFSET(vertex_declaration->m_Streams[i].m_Offset) );   //The starting point of the VBO, for the vertices

            CHECK_GL_ERROR
        }

        #undef BUFFER_OFFSET
    }

    static void BindVertexDeclarationProgram(HContext context, HVertexDeclaration vertex_declaration, HProgram program)
    {

        uint32_t n = vertex_declaration->m_StreamCount;
        VertexDeclaration::Stream* streams = &vertex_declaration->m_Streams[0];
        for (uint32_t i=0; i < n; i++)
        {
            GLint location = glGetAttribLocation(program, streams[i].m_Name);
            if (location != -1)
            {
                streams[i].m_PhysicalIndex = location;
            }
            else
            {
                CLEAR_GL_ERROR
                // TODO: Disabled irritating warning? Should we care about not used streams?
                //dmLogWarning("Vertex attribute %s is not active or defined", streams[i].m_Name);
                streams[i].m_PhysicalIndex = -1;
            }
        }

        vertex_declaration->m_BoundForProgram = program;
        vertex_declaration->m_ModificationVersion = context->m_ModificationVersion;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        assert(context);
        assert(vertex_buffer);
        assert(vertex_declaration);

        if (!(context->m_ModificationVersion == vertex_declaration->m_ModificationVersion && vertex_declaration->m_BoundForProgram == program))
        {
            BindVertexDeclarationProgram(context, vertex_declaration, program);
        }

        #define BUFFER_OFFSET(i) ((char*)0x0 + (i))

        glBindBufferARB(GL_ARRAY_BUFFER, vertex_buffer);
        CHECK_GL_ERROR

        for (uint32_t i=0; i<vertex_declaration->m_StreamCount; i++)
        {
            if (vertex_declaration->m_Streams[i].m_PhysicalIndex != -1)
            {
                glEnableVertexAttribArray(vertex_declaration->m_Streams[i].m_PhysicalIndex);
                CHECK_GL_ERROR
                glVertexAttribPointer(
                        vertex_declaration->m_Streams[i].m_PhysicalIndex,
                        vertex_declaration->m_Streams[i].m_Size,
                        vertex_declaration->m_Streams[i].m_Type,
                        vertex_declaration->m_Streams[i].m_Normalize,
                        vertex_declaration->m_Stride,
                BUFFER_OFFSET(vertex_declaration->m_Streams[i].m_Offset) );   //The starting point of the VBO, for the vertices

                CHECK_GL_ERROR
            }
        }

        #undef BUFFER_OFFSET
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);

        for (uint32_t i=0; i<vertex_declaration->m_StreamCount; i++)
        {
            glDisableVertexAttribArray(i);
            CHECK_GL_ERROR
        }

        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR
    }

    uint32_t g_DrawCallsHash = dmHashString32("DrawCalls");

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
        assert(index_buffer);
        DM_PROFILE(Graphics, "DrawElements");
        DM_COUNTER_HASH("DrawCalls", g_DrawCallsHash, 1);

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
        CHECK_GL_ERROR

        glDrawElements(prim_type, count, type, 0);
        CHECK_GL_ERROR
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);
        DM_PROFILE(Graphics, "Draw");
        DM_COUNTER_HASH("DrawCalls", g_DrawCallsHash, 1);
        glDrawArrays(prim_type, first, count);
        CHECK_GL_ERROR
    }

    static uint32_t CreateShader(GLenum type, const void* program, uint32_t program_size)
    {
        GLuint s = glCreateShader(type);
        CHECK_GL_ERROR
        GLint size = program_size;
        glShaderSource(s, 1, (const GLchar**) &program, &size);
        CHECK_GL_ERROR
        glCompileShader(s);
        CHECK_GL_ERROR

        GLint status;
        glGetShaderiv(s, GL_COMPILE_STATUS, &status);
        if (status == 0)
        {
#ifndef NDEBUG
            GLint logLength;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar *)malloc(logLength);
                glGetShaderInfoLog(s, logLength, &logLength, log);
                dmLogWarning("%s\n", log);
                free(log);
            }
#endif
            glDeleteShader(s);
            return 0;
        }

        return s;
    }

    HVertexProgram NewVertexProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateShader(GL_VERTEX_SHADER, program, program_size);
    }

    HFragmentProgram NewFragmentProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateShader(GL_FRAGMENT_SHADER, program, program_size);
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        IncreaseModificationVersion(context);

        (void) context;
        GLuint p = glCreateProgram();
        CHECK_GL_ERROR
        glAttachShader(p, vertex_program);
        CHECK_GL_ERROR
        glAttachShader(p, fragment_program);
        CHECK_GL_ERROR
        glLinkProgram(p);

        GLint status;
        glGetProgramiv(p, GL_LINK_STATUS, &status);
        if (status == 0)
        {
#ifndef NDEBUG
            GLint logLength;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar *)malloc(logLength);
                glGetProgramInfoLog(p, logLength, &logLength, log);
                dmLogWarning("%s\n", log);
                free(log);
            }
#endif
            glDeleteProgram(p);
            CHECK_GL_ERROR
            return 0;
        }

        CHECK_GL_ERROR
        return p;
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        (void) context;
        glDeleteProgram(program);
    }

    // Tries to compile a shader (either a vertex or fragment) program.
    // We use this together with a temporary GLuint program to see if we it's
    // possible to compile a reloaded program.
    //
    // In case the compile fails, it also prints the compile errors with dmLogWarning.
    static bool TryCompileShader(GLuint prog, const void* program, GLint size)
    {
        glShaderSource(prog, 1, (const GLchar**) &program, &size);
        CHECK_GL_ERROR
        glCompileShader(prog);
        CHECK_GL_ERROR

        GLint status;
        glGetShaderiv(prog, GL_COMPILE_STATUS, &status);
        if (status == 0)
        {
            GLint logLength;
            glGetShaderiv(prog, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar *)malloc(logLength);
                glGetShaderInfoLog(prog, logLength, &logLength, log);
                dmLogWarning("%s\n", log);
                free(log);
            }
            CHECK_GL_ERROR
            return false;
        }

        return true;
    }

    bool ReloadVertexProgram(HVertexProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        GLint size = program_size;

        GLuint tmp_shader = glCreateShader(GL_VERTEX_SHADER);
        bool success = TryCompileShader(tmp_shader, program, size);
        glDeleteShader(tmp_shader);
        CHECK_GL_ERROR

        if (success)
        {
            glShaderSource(prog, 1, (const GLchar**) &program, &size);
            CHECK_GL_ERROR
            glCompileShader(prog);
            CHECK_GL_ERROR
        }

        return success;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        GLint size = program_size;

        GLuint tmp_shader = glCreateShader(GL_FRAGMENT_SHADER);
        bool success = TryCompileShader(tmp_shader, program, size);
        glDeleteShader(tmp_shader);
        CHECK_GL_ERROR

        if (success)
        {
            glShaderSource(prog, 1, (const GLchar**) &program, &size);
            CHECK_GL_ERROR
            glCompileShader(prog);
            CHECK_GL_ERROR
        }

        return success;
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        glDeleteShader(program);
        CHECK_GL_ERROR
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        glDeleteShader(program);
        CHECK_GL_ERROR
    }

    void EnableProgram(HContext context, HProgram program)
    {
        (void) context;
        glUseProgram(program);
        CHECK_GL_ERROR
    }

    void DisableProgram(HContext context)
    {
        (void) context;
        glUseProgram(0);
    }

    static bool TryLinkProgram(HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        GLuint tmp_program = glCreateProgram();
        CHECK_GL_ERROR
        glAttachShader(tmp_program, vert_program);
        CHECK_GL_ERROR
        glAttachShader(tmp_program, frag_program);
        CHECK_GL_ERROR
        glLinkProgram(tmp_program);

        bool success = true;
        GLint status;
        glGetProgramiv(tmp_program, GL_LINK_STATUS, &status);
        if (status == 0)
        {
            GLint logLength;
            glGetProgramiv(tmp_program, GL_INFO_LOG_LENGTH, &logLength);
            if (logLength > 0)
            {
                GLchar *log = (GLchar *)malloc(logLength);
                glGetProgramInfoLog(tmp_program, logLength, &logLength, log);
                dmLogWarning("%s\n", log);
                free(log);
            }
            success = false;
        }
        glDeleteProgram(tmp_program);

        return success;
    }

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        if (!TryLinkProgram(vert_program, frag_program))
        {
            return false;
        }

        glLinkProgram(program);
        CHECK_GL_ERROR
        return true;
    }

    uint32_t GetUniformCount(HProgram prog)
    {
        GLint count;
        glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &count);
        CHECK_GL_ERROR
        return count;
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {
        GLint uniform_size;
        GLenum uniform_type;
        glGetActiveUniform(prog, index, buffer_size, 0, &uniform_size, &uniform_type, buffer);
        *type = (Type) uniform_type;
        CHECK_GL_ERROR
    }

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        GLint location = glGetUniformLocation(prog, name);
        if (location == -1)
        {
            // Clear error if uniform isn't found
            CLEAR_GL_ERROR
        }
        return (uint32_t) location;
    }

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);

        glViewport(x, y, width, height);
        CHECK_GL_ERROR
    }

    void SetConstantV4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);

        glUniform4fv(base_register,  1, (const GLfloat*) data);
        CHECK_GL_ERROR
    }

    void SetConstantM4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        glUniformMatrix4fv(base_register, 1, 0, (const GLfloat*) data);
        CHECK_GL_ERROR
    }

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {
        assert(context);
        glUniform1i(location, unit);
        CHECK_GL_ERROR
    }

    void SetDepthStencilRenderBuffer(RenderTarget* rt, bool update_current = false)
    {
        uint32_t param_buffer_index = rt->m_BufferTypeFlags & dmGraphics::BUFFER_TYPE_DEPTH_BIT ?  GetBufferTypeIndex(BUFFER_TYPE_DEPTH_BIT) :  GetBufferTypeIndex(BUFFER_TYPE_STENCIL_BIT);

        if(rt->m_DepthStencilBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, rt->m_DepthStencilBuffer);
#ifdef GL_DEPTH_STENCIL_ATTACHMENT
            // if we have the capability of GL_DEPTH_STENCIL_ATTACHMENT, create a single combined depth-stencil buffer
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, rt->m_BufferTextureParams[param_buffer_index].m_Width, rt->m_BufferTextureParams[param_buffer_index].m_Height);
            CHECK_GL_ERROR
            if(!update_current)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->m_DepthStencilBuffer);
                CHECK_GL_ERROR
            }
#else
            // create a depth-stencil that has the same buffer attached to both GL_DEPTH_ATTACHMENT and GL_STENCIL_ATTACHMENT (typical ES <= 2.0)
            glRenderbufferStorage(GL_RENDERBUFFER, DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH_STENCIL, rt->m_BufferTextureParams[param_buffer_index].m_Width, rt->m_BufferTextureParams[param_buffer_index].m_Height);
            CHECK_GL_ERROR
            if(!update_current)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->m_DepthStencilBuffer);
                CHECK_GL_ERROR
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->m_DepthStencilBuffer);
                CHECK_GL_ERROR
            }
#endif
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
            return;
        }

        if(rt->m_DepthBuffer)
        {
            // create depth buffer with best possible bitdepth, as some hardware supports both 16 and 24-bit quality.
            GLenum format = rt->m_DepthBufferBits == 16 ? DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH16 : DMGRAPHICS_RENDER_BUFFER_FORMAT_DEPTH24;
            glBindRenderbuffer(GL_RENDERBUFFER, rt->m_DepthBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, format, rt->m_BufferTextureParams[param_buffer_index].m_Width, rt->m_BufferTextureParams[param_buffer_index].m_Height);
            CHECK_GL_ERROR
            if(!update_current)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->m_DepthBuffer);
                CHECK_GL_ERROR
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        if(rt->m_StencilBuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, rt->m_StencilBuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, DMGRAPHICS_RENDER_BUFFER_FORMAT_STENCIL, rt->m_BufferTextureParams[param_buffer_index].m_Width, rt->m_BufferTextureParams[param_buffer_index].m_Height);
            CHECK_GL_ERROR
            if(!update_current)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rt->m_StencilBuffer);
                CHECK_GL_ERROR
            }
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
    }


    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget;
        memset(rt, 0, sizeof(RenderTarget));

        rt->m_BufferTypeFlags = buffer_type_flags;
        rt->m_DepthBufferBits = context->m_DepthBufferBits;

        glGenFramebuffers(1, &rt->m_Id);
        CHECK_GL_ERROR
        glBindFramebuffer(GL_FRAMEBUFFER, rt->m_Id);
        CHECK_GL_ERROR

        memcpy(rt->m_BufferTextureParams, params, sizeof(TextureParams) * MAX_BUFFER_TYPE_COUNT);
        // don't save the data
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            rt->m_BufferTextureParams[i].m_Data = 0x0;
            rt->m_BufferTextureParams[i].m_DataSize = 0;
        }
        if(buffer_type_flags & dmGraphics::BUFFER_TYPE_COLOR_BIT)
        {
            uint32_t color_buffer_index = GetBufferTypeIndex(BUFFER_TYPE_COLOR_BIT);
            rt->m_ColorBufferTexture = NewTexture(context, creation_params[color_buffer_index]);
            SetTexture(rt->m_ColorBufferTexture, params[color_buffer_index]);
            // attach the texture to FBO color attachment point
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->m_ColorBufferTexture->m_Texture, 0);
            CHECK_GL_ERROR
        }

        if(buffer_type_flags & (dmGraphics::BUFFER_TYPE_STENCIL_BIT | dmGraphics::BUFFER_TYPE_DEPTH_BIT))
        {
            if(!(buffer_type_flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT))
            {
                glGenRenderbuffers(1, &rt->m_DepthBuffer);
                CHECK_GL_ERROR
            }
            else
            {
                if(context->m_PackedDepthStencil)
                {
                    glGenRenderbuffers(1, &rt->m_DepthStencilBuffer);
                    CHECK_GL_ERROR
                }
                else
                {
                    glGenRenderbuffers(1, &rt->m_DepthBuffer);
                    CHECK_GL_ERROR
                    glGenRenderbuffers(1, &rt->m_StencilBuffer);
                    CHECK_GL_ERROR
                }
            }
            SetDepthStencilRenderBuffer(rt);
        }

        // Disable color buffer
        if ((buffer_type_flags & BUFFER_TYPE_COLOR_BIT) == 0)
        {
#ifndef GL_ES_VERSION_2_0
            // TODO: Not available in OpenGL ES.
            // According to this thread it should not be required but honestly I don't quite understand
            // https://devforums.apple.com/message/495216#495216
            glDrawBuffer(GL_NONE);
            CHECK_GL_ERROR
            glReadBuffer(GL_NONE);
            CHECK_GL_ERROR
#endif
        }

        CHECK_GL_FRAMEBUFFER_ERROR
        glBindFramebuffer(GL_FRAMEBUFFER, glfwGetDefaultFramebuffer());
        CHECK_GL_ERROR

        return rt;
    }


    void DeleteRenderTarget(HRenderTarget render_target)
    {
        glDeleteFramebuffers(1, &render_target->m_Id);
        if (render_target->m_ColorBufferTexture)
            DeleteTexture(render_target->m_ColorBufferTexture);
        if (render_target->m_DepthStencilBuffer)
            glDeleteRenderbuffers(1, &render_target->m_DepthStencilBuffer);
        if (render_target->m_DepthBuffer)
            glDeleteRenderbuffers(1, &render_target->m_DepthBuffer);
        if (render_target->m_StencilBuffer)
            glDeleteRenderbuffers(1, &render_target->m_StencilBuffer);

        delete render_target;
    }

    void EnableRenderTarget(HContext context, HRenderTarget render_target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, render_target->m_Id);
        CHECK_GL_ERROR
        CHECK_GL_FRAMEBUFFER_ERROR
    }

    void DisableRenderTarget(HContext context, HRenderTarget render_target)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, glfwGetDefaultFramebuffer());
        CHECK_GL_ERROR
        CHECK_GL_FRAMEBUFFER_ERROR
    }

    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        if(buffer_type != BUFFER_TYPE_COLOR_BIT)
            return 0;
        return render_target->m_ColorBufferTexture;
    }

    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        assert(render_target);
        uint32_t i = GetBufferTypeIndex(buffer_type);
        assert(i < MAX_BUFFER_TYPE_COUNT);
        width = render_target->m_BufferTextureParams[i].m_Width;
        height = render_target->m_BufferTextureParams[i].m_Height;
    }

    void SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        assert(render_target);
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            render_target->m_BufferTextureParams[i].m_Width = width;
            render_target->m_BufferTextureParams[i].m_Height = height;
            if(i == GetBufferTypeIndex(BUFFER_TYPE_COLOR_BIT))
            {
                if(render_target->m_ColorBufferTexture)
                    SetTexture(render_target->m_ColorBufferTexture, render_target->m_BufferTextureParams[i]);
            }
        }
        SetDepthStencilRenderBuffer(render_target, true);
    }

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    uint32_t GetMaxTextureSize(HContext context)
    {
        return context->m_MaxTextureSize;
    }

    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        GLuint t;
        glGenTextures( 1, &t );
        CHECK_GL_ERROR

        Texture* tex = new Texture;
        tex->m_Type = params.m_Type;
        tex->m_Texture = t;

        tex->m_Width = params.m_Width;
        tex->m_Height = params.m_Height;

        if (params.m_OriginalWidth == 0){
            tex->m_OriginalWidth = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        } else {
            tex->m_OriginalWidth = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        tex->m_DataState = 0;
        return (HTexture) tex;
    }

    void PostDeleteTextures(bool force_delete)
    {
        uint32_t i = 0;
        while(i < g_PostDeleteTexturesArray.Size())
        {
            HTexture texture = g_PostDeleteTexturesArray[i];
            if((!(dmGraphics::GetTextureStatusFlags(texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)) || (force_delete))
            {
                glDeleteTextures(1, &texture->m_Texture);
                CHECK_GL_ERROR
                delete texture;
                g_PostDeleteTexturesArray.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
    }

    void DeleteTexture(HTexture texture)
    {
        assert(texture);
        if(dmGraphics::GetTextureStatusFlags(texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
        {
            if (g_PostDeleteTexturesArray.Full())
            {
                g_PostDeleteTexturesArray.OffsetCapacity(64);
            }
            g_PostDeleteTexturesArray.Push(texture);
            return;
        }

        glDeleteTextures(1, &texture->m_Texture);
        CHECK_GL_ERROR

        delete texture;
    }

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap)
    {
        GLenum type = (GLenum) texture->m_Type;

        glTexParameteri(type, GL_TEXTURE_MIN_FILTER, minfilter);
        CHECK_GL_ERROR

        glTexParameteri(type, GL_TEXTURE_MAG_FILTER, magfilter);
        CHECK_GL_ERROR

        glTexParameteri(type, GL_TEXTURE_WRAP_S, uwrap);
        CHECK_GL_ERROR

        glTexParameteri(type, GL_TEXTURE_WRAP_T, vwrap);
        CHECK_GL_ERROR
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        uint32_t flags = TEXTURE_STATUS_OK;
        if(texture->m_DataState)
            flags |= TEXTURE_STATUS_DATA_PENDING;
        return flags;
    }

    void DoSetTextureAsync(void* context)
    {
        uint16_t param_array_index = (uint16_t) (size_t) context;
        TextureParamsAsync ap;
        {
            dmMutex::ScopedLock lk(g_Context->m_AsyncMutex);
            ap = g_TextureParamsAsyncArray[param_array_index];
            g_TextureParamsAsyncArrayIndices.Push(param_array_index);
        }
        SetTexture(ap.m_Texture, ap.m_Params);
        glFlush();
        ap.m_Texture->m_DataState &= ~(1<<ap.m_Params.m_MipMap);
    }

    void SetTextureAsync(HTexture texture, const TextureParams& params)
    {
        texture->m_DataState |= 1<<params.m_MipMap;
        uint16_t param_array_index;
        {
            dmMutex::ScopedLock lk(g_Context->m_AsyncMutex);
            if (g_TextureParamsAsyncArrayIndices.Remaining() == 0)
            {
                g_TextureParamsAsyncArrayIndices.SetCapacity(g_TextureParamsAsyncArrayIndices.Capacity()+64);
                g_TextureParamsAsyncArray.SetCapacity(g_TextureParamsAsyncArrayIndices.Capacity());
                g_TextureParamsAsyncArray.SetSize(g_TextureParamsAsyncArray.Capacity());
            }
            param_array_index = g_TextureParamsAsyncArrayIndices.Pop();
            TextureParamsAsync& ap = g_TextureParamsAsyncArray[param_array_index];
            ap.m_Texture = texture;
            ap.m_Params = params;
        }

        JobDesc j;
        j.m_Context = (void*)(size_t)param_array_index;
        j.m_Func = DoSetTextureAsync;
        j.m_FuncComplete = 0;
        JobQueuePush(j);
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {
        // validate write accessibility for format. Some format are not garuanteed to be writeable
        switch (params.m_Format)
        {
            case TEXTURE_FORMAT_DEPTH:
                dmLogError("TEXTURE_FORMAT_DEPTH is not a valid argument for SetTexture");
                return;
            case TEXTURE_FORMAT_STENCIL:
                dmLogError("TEXTURE_FORMAT_STENCIL is not a valid argument for SetTexture");
                return;
            default:
                break;
        }

        // Responsibility is on caller to not send in too big textures.
        assert(params.m_Width <= g_Context->m_MaxTextureSize);
        assert(params.m_Height <= g_Context->m_MaxTextureSize);

        int unpackAlignment = 4;
        /*
         * For RGA-textures the row-alignment may not be a multiple of 4.
         * OpenGL doesn't like this by default
         */
        if (params.m_Format != TEXTURE_FORMAT_RGBA)
        {
            uint32_t bytes_per_row = 1;

            switch(params.m_Format)
            {
                case TEXTURE_FORMAT_RGB:
                    bytes_per_row = params.m_Width * 3;
                    break;

                case TEXTURE_FORMAT_LUMINANCE_ALPHA:
                case TEXTURE_FORMAT_RGB_16BPP:
                case TEXTURE_FORMAT_RGBA_16BPP:
                    bytes_per_row = params.m_Width * 2;
                    break;

                case TEXTURE_FORMAT_RGB16F:
                    bytes_per_row = params.m_Width * 6;
                    break;

                case TEXTURE_FORMAT_RGB32F:
                    bytes_per_row = params.m_Width * 12;
                    break;

                case TEXTURE_FORMAT_RGBA16F:
                    bytes_per_row = params.m_Width * 8;
                    break;

                case TEXTURE_FORMAT_RGBA32F:
                    bytes_per_row = params.m_Width * 16;
                    break;

                case TEXTURE_FORMAT_R16F:
                    bytes_per_row = params.m_Width * 2;
                    break;

                case TEXTURE_FORMAT_R32F:
                    bytes_per_row = params.m_Width * 4;
                    break;

                case TEXTURE_FORMAT_RG16F:
                    bytes_per_row = params.m_Width * 4;
                    break;

                case TEXTURE_FORMAT_RG32F:
                    bytes_per_row = params.m_Width * 8;
                    break;

                default:
                    break;
            }

            if (bytes_per_row % 4 == 0) {
                // Ok
            } else if (bytes_per_row % 2 == 0) {
                unpackAlignment = 2;
            } else {
                unpackAlignment = 1;
            }
        }

        if (unpackAlignment != 4) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
            CHECK_GL_ERROR
        }

        GLenum type = (GLenum) texture->m_Type;
        glBindTexture(type, texture->m_Texture);
        CHECK_GL_ERROR

        texture->m_Params = params;
        if (!params.m_SubUpdate) {
            SetTextureParams(texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap);
        }



        GLenum gl_format;
        GLenum gl_type = DMGRAPHICS_TYPE_UNSIGNED_BYTE;
        // Only used for uncompressed formats
        GLint internal_format;
        switch (params.m_Format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE;
            break;
        case TEXTURE_FORMAT_LUMINANCE_ALPHA:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE_ALPHA;
            break;
        case TEXTURE_FORMAT_RGB:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            break;
        case TEXTURE_FORMAT_RGBA:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            break;
        case TEXTURE_FORMAT_RGB_16BPP:
            gl_type = DMGRAPHICS_TYPE_UNSIGNED_SHORT_565;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            break;
        case TEXTURE_FORMAT_RGBA_16BPP:
            gl_type = DMGRAPHICS_TYPE_UNSIGNED_SHORT_4444;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            break;
        case TEXTURE_FORMAT_RGB_DXT1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_DXT1;
            break;
        case TEXTURE_FORMAT_RGBA_DXT1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT1;
            break;
        case TEXTURE_FORMAT_RGBA_DXT3:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT3;
            break;
        case TEXTURE_FORMAT_RGBA_DXT5:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT3;
            CHECK_GL_ERROR
            break;
        case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
            break;
        case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
            break;
        case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            break;
        case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
            break;
        case TEXTURE_FORMAT_RGB_ETC1:
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB_ETC1;
            break;
        case TEXTURE_FORMAT_RGB16F:
            gl_type = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB16F;
            break;
        case TEXTURE_FORMAT_RGB32F:
            gl_type = DMGRAPHICS_TYPE_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGB;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGB32F;
            break;
        case TEXTURE_FORMAT_RGBA16F:
            gl_type = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA16F;
            break;
        case TEXTURE_FORMAT_RGBA32F:
            gl_type = DMGRAPHICS_TYPE_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RGBA32F;
            break;
        case TEXTURE_FORMAT_R16F:
            gl_type = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RED;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_R16F;
            break;
        case TEXTURE_FORMAT_R32F:
            gl_type = DMGRAPHICS_TYPE_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RED;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_R32F;
            break;
        case TEXTURE_FORMAT_RG16F:
            gl_type = DMGRAPHICS_TYPE_HALF_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RG;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RG16F;
            break;
        case TEXTURE_FORMAT_RG32F:
            gl_type = DMGRAPHICS_TYPE_FLOAT;
            gl_format = DMGRAPHICS_TEXTURE_FORMAT_RG;
            internal_format = DMGRAPHICS_TEXTURE_FORMAT_RG32F;
            break;

        default:
            gl_format = 0;
            assert(0);
            break;
        }

        switch (params.m_Format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
        case TEXTURE_FORMAT_LUMINANCE_ALPHA:
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
            if (texture->m_Type == TEXTURE_TYPE_2D) {
                if (params.m_SubUpdate) {
                    glTexSubImage2D(GL_TEXTURE_2D, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, params.m_Data);
                } else {
                    glTexImage2D(GL_TEXTURE_2D, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, params.m_Data);
                }
                CHECK_GL_ERROR
            } else if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP) {
                const char* p = (const char*) params.m_Data;
                if (params.m_SubUpdate) {
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 0);
                    CHECK_GL_ERROR
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 1);
                    CHECK_GL_ERROR
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 2);
                    CHECK_GL_ERROR
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 3);
                    CHECK_GL_ERROR
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 4);
                    CHECK_GL_ERROR
                    glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, gl_type, p + params.m_DataSize * 5);
                    CHECK_GL_ERROR
                } else {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 0);
                    CHECK_GL_ERROR
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 1);
                    CHECK_GL_ERROR
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 2);
                    CHECK_GL_ERROR
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 3);
                    CHECK_GL_ERROR
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 4);
                    CHECK_GL_ERROR
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, internal_format, params.m_Width, params.m_Height, 0, gl_format, gl_type, p + params.m_DataSize * 5);
                    CHECK_GL_ERROR
                }

            } else {
                assert(0);
            }
            break;

        case TEXTURE_FORMAT_RGB_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT3:
        case TEXTURE_FORMAT_RGBA_DXT5:
        case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
        case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
        case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
        case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
        case TEXTURE_FORMAT_RGB_ETC1:
            if (params.m_DataSize > 0) {
                if (texture->m_Type == TEXTURE_TYPE_2D) {
                    if (params.m_SubUpdate) {
                        glCompressedTexSubImage2D(GL_TEXTURE_2D, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, params.m_Data);
                    } else {
                        glCompressedTexImage2D(GL_TEXTURE_2D, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, params.m_Data);
                    }
                    CHECK_GL_ERROR
                } else if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP) {
                    const char* p = (const char*) params.m_Data;
                    if (params.m_SubUpdate) {
                        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 0);
                        CHECK_GL_ERROR
                        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 1);
                        CHECK_GL_ERROR
                        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 2);
                        CHECK_GL_ERROR
                        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 3);
                        CHECK_GL_ERROR
                        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 4);
                        CHECK_GL_ERROR
                        glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, params.m_X, params.m_Y, params.m_Width, params.m_Height, gl_format, params.m_DataSize, p + params.m_DataSize * 5);
                        CHECK_GL_ERROR
                    } else {
                        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 0);
                        CHECK_GL_ERROR
                        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 1);
                        CHECK_GL_ERROR
                        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 2);
                        CHECK_GL_ERROR
                        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 3);
                        CHECK_GL_ERROR
                        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 4);
                        CHECK_GL_ERROR
                        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, params.m_MipMap, gl_format, params.m_Width, params.m_Height, 0, params.m_DataSize, p + params.m_DataSize * 5);
                        CHECK_GL_ERROR
                    }
                } else {
                    assert(0);
                }
            }

            break;
        default:
            assert(0);
            break;
        }

        glBindTexture(type, 0);
        CHECK_GL_ERROR

        if (unpackAlignment != 4) {
            // Restore to default
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            CHECK_GL_ERROR
        }
    }

    uint16_t GetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(texture);

#if !defined(GL_ES_VERSION_2_0) and !defined(__EMSCRIPTEN__)
        glEnable(GL_TEXTURE_2D);
        CHECK_GL_ERROR
#endif

        glActiveTexture(TEXTURE_UNIT_NAMES[unit]);
        CHECK_GL_ERROR
        glBindTexture((GLenum) texture->m_Type, texture->m_Texture);
        CHECK_GL_ERROR

        SetTextureParams(texture, texture->m_Params.m_MinFilter, texture->m_Params.m_MagFilter, texture->m_Params.m_UWrap, texture->m_Params.m_VWrap);
    }

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);

#if !defined(GL_ES_VERSION_2_0) and !defined(__EMSCRIPTEN__)
        glEnable(GL_TEXTURE_2D);
        CHECK_GL_ERROR
#endif

        glActiveTexture(TEXTURE_UNIT_NAMES[unit]);
        CHECK_GL_ERROR
        glBindTexture((GLenum) texture->m_Type, 0);
        CHECK_GL_ERROR
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        uint32_t w = dmGraphics::GetWidth(context);
        uint32_t h = dmGraphics::GetHeight(context);
        assert (buffer_size >= w * h * 4);
        glReadPixels(0, 0, w, h,
                     GL_BGRA,
                     GL_UNSIGNED_BYTE,
                     buffer);
    }

    void EnableState(HContext context, State state)
    {
        assert(context);
        glEnable(state);
        CHECK_GL_ERROR
    }

    void DisableState(HContext context, State state)
    {
        assert(context);
        glDisable(state);
        CHECK_GL_ERROR
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
        glBlendFunc((GLenum) source_factor, (GLenum) destinaton_factor);
        CHECK_GL_ERROR
    }

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        glColorMask(red, green, blue, alpha);
        CHECK_GL_ERROR
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        glDepthMask(mask);
        CHECK_GL_ERROR
    }

    void SetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        glDepthFunc((GLenum) func);
        CHECK_GL_ERROR
    }

    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
        glScissor((GLint)x, (GLint)y, (GLint)width, (GLint)height);
        CHECK_GL_ERROR
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        glStencilMask(mask);
        CHECK_GL_ERROR
    }

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        glStencilFunc((GLenum) func, ref, mask);
        CHECK_GL_ERROR
    }

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        glStencilOp((GLenum) sfail, (GLenum) dpfail, (GLenum) dppass);
        CHECK_GL_ERROR
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
        glCullFace(face_type);
        CHECK_GL_ERROR
    }

    void SetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
        glPolygonOffset(factor, units);
        CHECK_GL_ERROR
    }

    BufferType BUFFER_TYPES[MAX_BUFFER_TYPE_COUNT] = {BUFFER_TYPE_COLOR_BIT, BUFFER_TYPE_DEPTH_BIT, BUFFER_TYPE_STENCIL_BIT};

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

    // Nop functions, exist in graphics_private.h but only used for tests.
    void SetForceFragmentReloadFail(bool should_fail)
    {
        // nop
    }

    void SetForceVertexReloadFail(bool should_fail)
    {
        // nop
    }

}
