#include <string.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "graphics_device.h"
#include "opengl_device.h"
#include <assert.h>

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
extern PFNGLGENPROGRAMARBPROC glGenProgramsARB = NULL;
extern PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
extern PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
extern PFNGLVERTEXPARAMFLOAT4ARBPROC glProgramLocalParameter4fARB = NULL;
extern PFNGLVERTEXATTRIBSETPROC glEnableVertexAttribArray = NULL;
extern PFNGLVERTEXATTRIBSETPROC glDisableVertexAttribArray = NULL;
extern PFNGLVERTEXATTRIBPTRPROC glVertexAttribPointer = NULL;
extern PFNGLTEXPARAM2DPROC glCompressedTexImage2D = NULL;

#else
#error "Platform not supported."
#endif  

using namespace Vectormath::Aos;

SGFXHDevice gdevice;
SGFXHContext gcontext;

GFXHContext GFXGetContext()
{
    return (GFXHContext)&gcontext;
}

GFXHDevice GFXCreateDevice(int* argc, char** argv, GFXSCreateDeviceParams *params )
{
    assert(params);

    int ret = SDL_Init(SDL_INIT_VIDEO);
    assert(ret == 0);

    gdevice.m_SDLscreen = SDL_SetVideoMode(params->m_DisplayWidth, params->m_DisplayHeight, 16, SDL_OPENGL|SDL_RESIZABLE);
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

    return (GFXHDevice)&gdevice;
}

void GFXDestroyDevice()
{
    SDL_Quit();
}

void GFXClear(GFXHContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
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

void GFXFlip()
{
    SDL_GL_SwapBuffers();
}

void GFXDraw(GFXHContext context, GFXPrimitiveType primitive_type, int32_t first, int32_t count )
{
    assert(context);

    glDrawArrays(primitive_type, first, count);
}

void GFXSetVertexStream(GFXHContext context, uint16_t stream, uint16_t size, GFXType type, uint16_t stride, const void* vertex_buffer)
{
    assert(context);
    assert(vertex_buffer);

    glEnableVertexAttribArray(stream);
    glVertexAttribPointer(stream, size, type, false, stride, vertex_buffer);
}

void GFXDisableVertexStream(GFXHContext context, uint16_t stream)
{
    assert(context);

    glDisableVertexAttribArray(stream);
}

void GFXDrawElements(GFXHContext context, GFXPrimitiveType prim_type, uint32_t count, GFXType type, const void* index_buffer)
{
    assert(context);
    assert(index_buffer);

    glDrawElements(prim_type, count, type, index_buffer);
}

void GFXDraw(GFXHContext context, GFXPrimitiveType prim_type, uint32_t first, uint32_t count)
{
    assert(context);

    glDrawArrays(prim_type, first, count);
}

static uint32_t GFXCreateProgram(GLenum type, const void* program, uint32_t program_size)
{
    glEnable(type);
    GLuint shader[1];
    glGenProgramsARB(1, shader);
    glBindProgramARB(type, shader[0]);
    glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, program_size, program);
    glDisable(type);

    return shader[0];
}

HGFXVertexProgram GFXCreateVertexProgram(const void* program, uint32_t program_size)
{
    assert(program);

    return GFXCreateProgram(GL_VERTEX_PROGRAM_ARB, program, program_size);
}

HGFXFragmentProgram GFXCreateFragmentProgram(const void* program, uint32_t program_size)
{
    assert(program);

    return GFXCreateProgram(GL_FRAGMENT_PROGRAM_ARB, program, program_size);
}

void GFXDestroyVertexProgram(HGFXVertexProgram program)
{
    assert(program);
}

void GFXDestroyFragmentProgram(HGFXFragmentProgram program)
{
}

static void GFXSetProgram(GLenum type, uint32_t program)
{
    glEnable(type);
    glBindProgramARB(type, program);
}

void GFXSetVertexProgram(GFXHContext context, HGFXVertexProgram program)
{
    assert(context);

    GFXSetProgram(GL_VERTEX_PROGRAM_ARB, program);
}

void GFXSetFragmentProgram(GFXHContext context, HGFXFragmentProgram program)
{
    assert(context);

    GFXSetProgram(GL_FRAGMENT_PROGRAM_ARB, program);
}

void GFXSetViewport(GFXHContext context, int width, int height)
{
    assert(context);

    glViewport(0, 0, width, height);
}

static void GFXSetProgramConstantBlock(GFXHContext context, GLenum type, const Vector4* data, int base_register, int num_vectors)
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

void GFXSetFragmentConstant(GFXHContext context, const Vector4* data, int base_register)
{
    assert(context);

    GFXSetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, 1);
}

void GFXSetVertexConstantBlock(GFXHContext context, const Vector4* data, int base_register, int num_vectors)
{
    assert(context);

    GFXSetProgramConstantBlock(context, GL_VERTEX_PROGRAM_ARB, data, base_register, num_vectors);
}

void GFXSetFragmentConstantBlock(GFXHContext context, const Vector4* data, int base_register, int num_vectors)
{
    assert(context);

    GFXSetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, num_vectors);
}

void GFXSetTexture(GFXHContext context, GFXHTexture t)
{
    assert(context);
    assert(t);

    SGFXHTexture* tex_h = (SGFXHTexture*)t;
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, tex_h->m_Texture);

    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );


}

GFXHTexture GFXCreateTexture(uint32_t width, uint32_t height, GFXTextureFormat texture_format)
{
    GLuint t;
    glGenTextures( 1, &t );
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

    SGFXHTexture* tex = new SGFXHTexture;
    tex->m_Texture = t;
    return (GFXHTexture) tex;
}

void GFXSetTextureData(GFXHTexture texture,
                       uint16_t mip_map,
                       uint16_t width, uint16_t height, uint16_t border,
                       GFXTextureFormat texture_format, const void* data, uint32_t data_size)
{
    GLenum gl_format;
    GLenum gl_type = GL_UNSIGNED_BYTE;
    GLint internal_format;

    switch (texture_format)
    {
    case GFX_TEXTURE_FORMAT_LUMINANCE:
        gl_format = GL_LUMINANCE;
        internal_format = 1;
        break;
    case GFX_TEXTURE_FORMAT_RGB:
        gl_format = GL_RGB;
        internal_format = 3;
        break;
    case GFX_TEXTURE_FORMAT_RGBA:
        gl_format = GL_RGBA;
        internal_format = 4;
        break;
    case GFX_TEXTURE_FORMAT_RGB_DXT1:
        gl_format = GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
        break;
    case GFX_TEXTURE_FORMAT_RGBA_DXT1:
        gl_format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        break;
    case GFX_TEXTURE_FORMAT_RGBA_DXT3:
        gl_format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        break;
    case GFX_TEXTURE_FORMAT_RGBA_DXT5:
        gl_format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        glCompressedTexImage2D(GL_TEXTURE_2D, mip_map, texture_format, width, height, border, data_size, data);
        break;
    default:
        assert(0);
    }
    switch (texture_format)
    {
    case GFX_TEXTURE_FORMAT_LUMINANCE:
    case GFX_TEXTURE_FORMAT_RGB:
    case GFX_TEXTURE_FORMAT_RGBA:
        glTexImage2D(GL_TEXTURE_2D, mip_map, internal_format, width, height, border, gl_format, gl_type, data);
        break;

    case GFX_TEXTURE_FORMAT_RGB_DXT1:
    case GFX_TEXTURE_FORMAT_RGBA_DXT1:
    case GFX_TEXTURE_FORMAT_RGBA_DXT3:
    case GFX_TEXTURE_FORMAT_RGBA_DXT5:
        glCompressedTexImage2D(GL_TEXTURE_2D, mip_map, gl_format, width, height, border, data_size, data);
        break;
    default:
        assert(0);
    }
}

void GFXDestroyTexture(GFXHTexture t)
{
    assert(t);

    SGFXHTexture* tex = (SGFXHTexture*)t;

    delete tex;
}

void GFXEnableState(GFXHContext context, GFXRenderState state)
{
    assert(context);

    glEnable(state);
}

void GFXDisableState(GFXHContext context, GFXRenderState state)
{
    assert(context);

    glDisable(state);
}

void GFXSetBlendFunc(GFXHContext context, GFXBlendFactor source_factor, GFXBlendFactor destinaton_factor)
{
    assert(context);

    glBlendFunc((GLenum) source_factor, (GLenum) destinaton_factor);
}

void GFXSetDepthMask(GFXHContext context, bool mask)
{
    assert(context);

    glDepthMask(mask);
}
