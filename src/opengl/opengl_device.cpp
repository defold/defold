#include <string.h>
#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../graphics_device.h"
#include "opengl_device.h"

#ifdef __linux__
#include <GL/glext.h>

#elif defined (__MACH__)
#include <sdl/SDL_opengl.h>

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
typedef void (APIENTRY * PFNGLPROGRAMSTRINGARBPROC) (GLenum, GLenum, GLsizei, const GLvoid *);
typedef void (APIENTRY * PFNGLVERTEXPARAMFLOAT4ARBPROC) (GLenum, GLuint, GLfloat, GLfloat, GLfloat, GLfloat);
typedef void (APIENTRY * PFNGLVERTEXATTRIBSETPROC) (GLuint);
typedef void (APIENTRY * PFNGLVERTEXATTRIBPTRPROC) (GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
typedef void (APIENTRY * PFNGLTEXPARAM2DPROC) (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);
PFNGLGENPROGRAMARBPROC glGenProgramsARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLVERTEXPARAMFLOAT4ARBPROC glProgramLocalParameter4fARB = NULL;
PFNGLVERTEXATTRIBSETPROC glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBSETPROC glDisableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPTRPROC glVertexAttribPointer = NULL;
PFNGLTEXPARAM2DPROC glCompressedTexImage2D = NULL;

#else
#error "Platform not supported."
#endif

using namespace Vectormath::Aos;

namespace dmGraphics
{

    Device gdevice;
    Context gcontext;

    HContext GetContext()
    {
        return (HContext)&gcontext;
    }

    HDevice CreateDevice(int* argc, char** argv, CreateDeviceParams *params )
    {
        assert(params);

        int ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
        assert(ret == 0);

        uint32_t fullscreen = 0;
        if (params->m_Fullscreen)
            fullscreen = SDL_FULLSCREEN;

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);

        gdevice.m_SDLscreen = SDL_SetVideoMode(params->m_DisplayWidth, params->m_DisplayHeight, 16, SDL_OPENGL|SDL_RESIZABLE|fullscreen|SDL_DOUBLEBUF);
        assert(gdevice.m_SDLscreen);

        SDL_WM_SetCaption(params->m_AppTitle, params->m_AppTitle);

        gdevice.m_DisplayWidth = params->m_DisplayWidth;
        gdevice.m_DisplayHeight = params->m_DisplayHeight;

        if (params->m_PrintDeviceInfo)
        {
            printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
            printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
            printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
            printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
        }


    #if defined (_WIN32)
        glGenProgramsARB = (PFNGLGENPROGRAMARBPROC) wglGetProcAddress("glGenProgramsARB");
        glBindProgramARB = (PFNGLBINDPROGRAMARBPROC) wglGetProcAddress("glBindProgramARB");
        glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) wglGetProcAddress("glProgramStringARB");
        glProgramLocalParameter4fARB = (PFNGLVERTEXPARAMFLOAT4ARBPROC) wglGetProcAddress("glProgramLocalParameter4fARB");
        glEnableVertexAttribArray = (PFNGLVERTEXATTRIBSETPROC) wglGetProcAddress("glEnableVertexAttribArray");
        glDisableVertexAttribArray = (PFNGLVERTEXATTRIBSETPROC) wglGetProcAddress("glDisableVertexAttribArray");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPTRPROC) wglGetProcAddress("glVertexAttribPointer");
        glCompressedTexImage2D = (PFNGLTEXPARAM2DPROC) wglGetProcAddress("glCompressedTexImage2D");
    #endif

        return (HDevice)&gdevice;
    }

    void DestroyDevice()
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;
        glClearColor(r, g, b, a);

        glClearDepth(depth);
        glClearStencil(stencil);
        glClear(flags);
    }

    void Flip()
    {
        SDL_GL_SwapBuffers();
    }

    void Draw(HContext context, PrimitiveType primitive_type, int32_t first, int32_t count )
    {
        assert(context);

        glDrawArrays(primitive_type, first, count);
    }

    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);

        glEnableVertexAttribArray(stream);
        glVertexAttribPointer(stream, size, type, false, stride, vertex_buffer);
    }

    void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);

        glDisableVertexAttribArray(stream);
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer)
    {
        assert(context);
        assert(index_buffer);

        glDrawElements(prim_type, count, type, index_buffer);
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);

        glDrawArrays(prim_type, first, count);
    }

    static uint32_t CreateProgram(GLenum type, const void* program, uint32_t program_size)
    {
        glEnable(type);
        GLuint shader[1];
        glGenProgramsARB(1, shader);
        glBindProgramARB(type, shader[0]);
        glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, program_size, program);
        glDisable(type);

        return shader[0];
    }

    HVertexProgram CreateVertexProgram(const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateProgram(GL_VERTEX_PROGRAM_ARB, program, program_size);
    }

    HFragmentProgram CreateFragmentProgram(const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateProgram(GL_FRAGMENT_PROGRAM_ARB, program, program_size);
    }

    void DestroyVertexProgram(HVertexProgram program)
    {
        assert(program);
    }

    void DestroyFragmentProgram(HFragmentProgram program)
    {
    }

    static void SetProgram(GLenum type, uint32_t program)
    {
        glEnable(type);
        glBindProgramARB(type, program);
    }

    void SetVertexProgram(HContext context, HVertexProgram program)
    {
        assert(context);

        SetProgram(GL_VERTEX_PROGRAM_ARB, program);
    }

    void SetFragmentProgram(HContext context, HFragmentProgram program)
    {
        assert(context);

        SetProgram(GL_FRAGMENT_PROGRAM_ARB, program);
    }

    void SetViewport(HContext context, int width, int height)
    {
        assert(context);

        glViewport(0, 0, width, height);
    }

    static void SetProgramConstantBlock(HContext context, GLenum type, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);
        assert(data);
        assert(base_register >= 0);
        assert(num_vectors >= 0);

        const float *f = (const float*)data;
        int reg = 0;
        for (int i=0; i<num_vectors; i++, reg+=4)
        {
            glProgramLocalParameter4fARB(type, base_register+i,  f[0+reg], f[1+reg], f[2+reg], f[3+reg]);
        }

    }

    void SetFragmentConstant(HContext context, const Vector4* data, int base_register)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, 1);
    }

    void SetVertexConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_VERTEX_PROGRAM_ARB, data, base_register, num_vectors);
    }

    void SetFragmentConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, num_vectors);
    }

    void SetTexture(HContext context, HTexture t)
    {
        assert(context);
        assert(t);

        Texture* tex_h = (Texture*)t;
        glEnable(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, tex_h->m_Texture);

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );


    }

    HTexture CreateTexture(uint32_t width, uint32_t height, TextureFormat texture_format)
    {
        GLuint t;
        glGenTextures( 1, &t );
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        Texture* tex = new Texture;
        tex->m_Texture = t;
        return (HTexture) tex;
    }

    void SetTextureData(HTexture texture,
                           uint16_t mip_map,
                           uint16_t width, uint16_t height, uint16_t border,
                           TextureFormat texture_format, const void* data, uint32_t data_size)
    {
        GLenum gl_format;
        GLenum gl_type = GL_UNSIGNED_BYTE;
        GLint internal_format;

        switch (texture_format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
            gl_format = GL_LUMINANCE;
            internal_format = 1;
            break;
        case TEXTURE_FORMAT_RGB:
            gl_format = GL_RGB;
            internal_format = 3;
            break;
        case TEXTURE_FORMAT_RGBA:
            gl_format = GL_RGBA;
            internal_format = 4;
            break;
        case TEXTURE_FORMAT_RGB_DXT1:
            gl_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
            break;
        case TEXTURE_FORMAT_RGBA_DXT1:
            gl_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            break;
        case TEXTURE_FORMAT_RGBA_DXT3:
            gl_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            break;
        case TEXTURE_FORMAT_RGBA_DXT5:
            gl_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            glCompressedTexImage2D(GL_TEXTURE_2D, mip_map, texture_format, width, height, border, data_size, data);
            break;
        default:
            assert(0);
        }
        switch (texture_format)
        {
        case TEXTURE_FORMAT_LUMINANCE:
        case TEXTURE_FORMAT_RGB:
        case TEXTURE_FORMAT_RGBA:
            glTexImage2D(GL_TEXTURE_2D, mip_map, internal_format, width, height, border, gl_format, gl_type, data);
            break;

        case TEXTURE_FORMAT_RGB_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT3:
        case TEXTURE_FORMAT_RGBA_DXT5:
            glCompressedTexImage2D(GL_TEXTURE_2D, mip_map, gl_format, width, height, border, data_size, data);
            break;
        default:
            assert(0);
        }
    }

    void DestroyTexture(HTexture t)
    {
        assert(t);

        Texture* tex = (Texture*)t;

        delete tex;
    }

    void EnableState(HContext context, RenderState state)
    {
        assert(context);

        glEnable(state);
    }

    void DisableState(HContext context, RenderState state)
    {
        assert(context);

        glDisable(state);
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);

        glBlendFunc((GLenum) source_factor, (GLenum) destinaton_factor);
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);

        glDepthMask(mask);
    }

}
