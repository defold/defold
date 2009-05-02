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
#include "win32/glext.h"

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
    glEnableVertexAttribArray(stream);
    glVertexAttribPointer(stream, size, type, false, stride, vertex_buffer);
}

void GFXDisableVertexStream(GFXHContext context, uint16_t stream)
{
    glDisableVertexAttribArray(stream);
}

void GFXDrawElements(GFXHContext context, GFXPrimitiveType prim_type, uint32_t count, GFXType type, const void* index_buffer)
{
    glDrawElements(prim_type, count, type, index_buffer);
}

void GFXDraw(GFXHContext context, GFXPrimitiveType prim_type, uint32_t first, uint32_t count)
{
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
    return GFXCreateProgram(GL_VERTEX_PROGRAM_ARB, program, program_size);
}

HGFXFragmentProgram GFXCreateFragmentProgram(const void* program, uint32_t program_size)
{
    return GFXCreateProgram(GL_FRAGMENT_PROGRAM_ARB, program, program_size);
}

static void GFXSetProgram(GLenum type, uint32_t program)
{
    glEnable(type);
    glBindProgramARB(type, program);
}

void GFXSetVertexProgram(GFXHContext context, HGFXVertexProgram program)
{
    GFXSetProgram(GL_VERTEX_PROGRAM_ARB, program);
}

void GFXSetFragmentProgram(GFXHContext context, HGFXFragmentProgram program)
{
    GFXSetProgram(GL_FRAGMENT_PROGRAM_ARB, program);
}

void GFXSetViewport(GFXHContext context, int width, int height)
{
    assert(context);
    glViewport(0, 0, width, height);
}

void GFXSetMatrix(GFXHContext context, GFXMatrixMode matrix_mode, const Matrix4* matrix)
{
    assert(context);
    assert(matrix);

    SGFXHContext* context_t = (SGFXHContext*)context;

    if (matrix_mode == GFX_DEVICE_MATRIX_TYPE_PROJECTION)
    {
        glMatrixMode(matrix_mode);
        glLoadMatrixf((float*)matrix);
    }
    else if (matrix_mode == GFX_DEVICE_MATRIX_TYPE_VIEW)
    {
        context_t->m_ViewMatrix = *matrix;
    }
    else if (matrix_mode == GFX_DEVICE_MATRIX_TYPE_WORLD)
    {
        glMatrixMode(GL_MODELVIEW);
        Matrix4 res = context_t->m_ViewMatrix * (*matrix);
        glLoadMatrixf((float*)&res);
    }
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

GFXHTexture GFXCreateTexture(uint32 width, uint32 height, GFXTextureFormat texture_format)
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
                       uint16 mip_map,
                       uint16_t width, uint16_t height, uint16_t border,
                       GFXTextureFormat texture_format, const void* data, uint32_t data_size)
{
    GLenum gl_format;
    GLenum gl_type = GL_UNSIGNED_BYTE;
    GLint internal_format;

    switch (texture_format)
    {
    case GFX_TEXTURE_FORMAT_ALPHA:
        gl_format = GL_ALPHA;
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
    case GFX_TEXTURE_FORMAT_ALPHA:
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
