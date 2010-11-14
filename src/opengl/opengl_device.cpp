#include <string.h>
#include <assert.h>
#include <dlib/profile.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../graphics_device.h"
#include "opengl_device.h"

#include "../glfw/include/GL/glfw.h"

#ifdef __linux__
#include <GL/glext.h>

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
typedef void (APIENTRY * PFNGLBINDBUFFERPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLBUFFERDATAPROC) (GLenum, GLsizeiptr, const GLvoid*, GLenum);
typedef void (APIENTRY * PFNGLGENRENDERBUFFERPROC) (GLenum, GLuint *);
typedef void (APIENTRY * PFNGLBINDRENDERBUFFERPROC) (GLenum, GLuint);
typedef void (APIENTRY * PFNGLRENDERBUFFERSTORAGEPROC) (GLenum, GLenum, GLsizei, GLsizei);

PFNGLGENPROGRAMARBPROC glGenProgramsARB = NULL;
PFNGLBINDPROGRAMARBPROC glBindProgramARB = NULL;
PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB = NULL;
PFNGLPROGRAMSTRINGARBPROC glProgramStringARB = NULL;
PFNGLVERTEXPARAMFLOAT4ARBPROC glProgramLocalParameter4fARB = NULL;
PFNGLVERTEXATTRIBSETPROC glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBSETPROC glDisableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPTRPROC glVertexAttribPointer = NULL;
PFNGLTEXPARAM2DPROC glCompressedTexImage2D = NULL;
PFNGLGENBUFFERSPROC glGenBuffersARB = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffersARB = NULL;
PFNGLBINDBUFFERPROC glBindBufferARB = NULL;
PFNGLBUFFERDATAPROC glBufferDataARB = NULL;
PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements = NULL;
PFNGLGENRENDERBUFFERPROC glGenRenderBuffers = NULL;
PFNGLBINDRENDERBUFFERPROC glBindRenderBuffer = NULL;
PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage = NULL;
#else
#error "Platform not supported."
#endif

using namespace Vectormath::Aos;

namespace dmGraphics
{
#define CHECK_GL_ERROR \
    { \
        GLint err = glGetError(); \
        if (err != 0) \
        { \
            printf("gl error: %d line: %d\n", err, __LINE__); \
            assert(0); \
        } \
    }\


    Device gdevice;
    Context gcontext;

    HContext GetContext()
    {
        return (HContext)&gcontext;
    }

    HDevice NewDevice(int* argc, char** argv, CreateDeviceParams *params )
    {
        assert(params);

        glfwInit(); // We can do this twice


        if( !glfwOpenWindow( params->m_DisplayWidth, params->m_DisplayHeight, 8,8,8,8, 32,0, GLFW_WINDOW ) )
        {
            glfwTerminate();
            return 0;
        }

        glfwSetWindowTitle(params->m_AppTitle);
        glfwSwapInterval(1);
        CHECK_GL_ERROR



#if 0
        int ret = SDL_InitSubSystem(SDL_INIT_VIDEO);
        assert(ret == 0);

        uint32_t fullscreen = 0;
        if (params->m_Fullscreen)
            fullscreen = SDL_FULLSCREEN;

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 1);
        SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 32 );


        gdevice.m_SDLscreen = SDL_SetVideoMode(params->m_DisplayWidth, params->m_DisplayHeight, 32, SDL_OPENGL|SDL_RESIZABLE|fullscreen|SDL_DOUBLEBUF);
        assert(gdevice.m_SDLscreen);

        SDL_WM_SetCaption(params->m_AppTitle, params->m_AppTitle);
#endif

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
        glDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC) wglGetProcAddress("glDeleteProgramsARB");
        glProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC) wglGetProcAddress("glProgramStringARB");
        glProgramLocalParameter4fARB = (PFNGLVERTEXPARAMFLOAT4ARBPROC) wglGetProcAddress("glProgramLocalParameter4fARB");
        glEnableVertexAttribArray = (PFNGLVERTEXATTRIBSETPROC) wglGetProcAddress("glEnableVertexAttribArray");
        glDisableVertexAttribArray = (PFNGLVERTEXATTRIBSETPROC) wglGetProcAddress("glDisableVertexAttribArray");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPTRPROC) wglGetProcAddress("glVertexAttribPointer");
        glCompressedTexImage2D = (PFNGLTEXPARAM2DPROC) wglGetProcAddress("glCompressedTexImage2D");
        glGenBuffersARB = (PFNGLGENBUFFERSPROC) wglGetProcAddress("glGenBuffersARB");
        glDeleteBuffersARB = (PFNGLDELETEBUFFERSPROC) wglGetProcAddress("glDeleteBuffersARB");
        glBindBufferARB = (PFNGLBINDBUFFERPROC) wglGetProcAddress("glBindBufferARB");
        glBufferDataARB = (PFNGLBUFFERDATAPROC) wglGetProcAddress("glBufferDataARB");
        glDrawRangeElements = (PFNGLDRAWRANGEELEMENTSPROC) wglGetProcAddress("glDrawRangeElements");
        glGenRenderBuffers = (PFNGLGENRENDERBUFFERPROC) wglGetProcAddress("glGenRenderBuffers");
        glBindRenderBuffer = (PFNGLBINDRENDERBUFFERPROC) wglGetProcAddress("glBindRenderBuffer");
        glRenderBufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) wglGetProcAddress("glRenderBufferStorage");
    #endif

        return (HDevice)&gdevice;
    }

    void DeleteDevice(HDevice device)
    {
        glfwTerminate();
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);

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

    void Flip()
    {
        glfwSwapBuffers();
        CHECK_GL_ERROR
    }

    HVertexBuffer NewVertexbuffer(uint32_t element_size, uint32_t element_count, BufferType buffer_type, MemoryType memory_type, uint32_t buffer_count, const void* data)
    {
        assert(buffer_count < 4);

        VertexBuffer* buffer = new VertexBuffer;

        GLenum vbo_type;
        if (buffer_type == BUFFER_TYPE_DYNAMIC)
        {
            vbo_type = GL_STREAM_DRAW_ARB;
        }
        else if (buffer_type == BUFFER_TYPE_STATIC)
        {
            vbo_type = GL_STATIC_DRAW_ARB;
        }

        glGenBuffersARB(1, &buffer->m_VboId);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffer->m_VboId);
        CHECK_GL_ERROR
        glBufferDataARB(GL_ARRAY_BUFFER_ARB, element_size*element_count, data, vbo_type);
        CHECK_GL_ERROR

        // TODO: removed when theres full vbo-support
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR


        buffer->m_BufferCount = buffer_count;
        buffer->m_ElementCount = element_count;
        buffer->m_ElementSize = element_size;
        buffer->m_BufferType = buffer_type;
        buffer->m_MemoryType = memory_type;

        for (uint32_t i=0; i<buffer_count; i++)
        {
            buffer->m_Data[i] = malloc(element_size*element_count);
            if (data)
                memcpy(buffer->m_Data[i], data, element_size*element_count);
        }

        return buffer;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        assert(buffer->m_BufferCount < 4);

        for (uint32_t i=0; i<buffer->m_BufferCount; i++)
        {
            free(buffer->m_Data[i]);
        }

        glDeleteBuffersARB(1, &buffer->m_VboId);
        CHECK_GL_ERROR

        delete buffer;
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
        }
        return size;
    }

    HVertexDeclaration NewVertexDeclaration(VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration;


        vd->m_Stride = 0;
        assert(count < (sizeof(vd->m_Streams) / sizeof(vd->m_Streams[0]) ) );

        for (uint32_t i=0; i<count; i++)
        {
            vd->m_Streams[i].m_Index = i;
            vd->m_Streams[i].m_Size = element[i].m_Size;
            vd->m_Streams[i].m_Usage = element[i].m_Usage;
            vd->m_Streams[i].m_Type = element[i].m_Type;
            vd->m_Streams[i].m_UsageIndex = element[i].m_UsageIndex;
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


    HIndexBuffer NewIndexBuffer(uint32_t element_count, BufferType buffer_type, MemoryType memory_type, const void* data)
    {
        IndexBuffer* buffer = new IndexBuffer;
        uint32_t element_size = sizeof(int);

        buffer->m_ElementCount = element_count;
        buffer->m_BufferType = buffer_type;
        buffer->m_MemoryType = memory_type;
        buffer->m_Data = malloc(element_count*element_size);
        memcpy(buffer->m_Data, data, element_count*element_size);

        glGenBuffersARB(1, &buffer->m_VboId);
        CHECK_GL_ERROR
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffer->m_VboId);
        CHECK_GL_ERROR
        glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB, element_size*element_count, data, GL_STATIC_DRAW);
        CHECK_GL_ERROR

        // TODO: removed when theres full vbo-support
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CHECK_GL_ERROR

        return buffer;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        free(buffer->m_Data);
        glDeleteBuffersARB(1, &buffer->m_VboId);
        CHECK_GL_ERROR
        delete buffer;
    }


    void SetVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        assert(vertex_declaration);
        #define BUFFER_OFFSET(i) ((char*)0x0 + (i))

        glBindBufferARB(GL_ARRAY_BUFFER, vertex_buffer->m_VboId);
        CHECK_GL_ERROR


        for (uint32_t i=0; i<vertex_declaration->m_StreamCount; i++)
        {
            glEnableVertexAttribArray(vertex_declaration->m_Streams[i].m_Index);
            CHECK_GL_ERROR
            glVertexAttribPointer(
                    vertex_declaration->m_Streams[i].m_Index,
                    vertex_declaration->m_Streams[i].m_Size,
                    vertex_declaration->m_Streams[i].m_Type,
                    false,
                    vertex_declaration->m_Stride,
            BUFFER_OFFSET(vertex_declaration->m_Streams[i].m_Offset) );   //The starting point of the VBO, for the vertices

            CHECK_GL_ERROR
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



    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);

        glEnableVertexAttribArray(stream);
        CHECK_GL_ERROR
        glVertexAttribPointer(stream, size, type, false, stride, vertex_buffer);
        CHECK_GL_ERROR
    }

    void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);

        glDisableVertexAttribArray(stream);
        CHECK_GL_ERROR

    }

    void DrawRangeElements(HContext context, PrimitiveType prim_type, uint32_t start, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
        assert(index_buffer);
        DM_PROFILE(Graphics, "DrawElements");

        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, index_buffer->m_VboId);
        CHECK_GL_ERROR

        glDrawRangeElements(prim_type, start, 100000, count*3, type, 0);
        CHECK_GL_ERROR


    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer)
    {
        assert(context);
        assert(index_buffer);
        DM_PROFILE(Graphics, "DrawElements");

        glDrawElements(prim_type, count, type, index_buffer);
        CHECK_GL_ERROR
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);
        DM_PROFILE(Graphics, "Draw");
        glDrawArrays(prim_type, first, count);
        CHECK_GL_ERROR
    }

    static uint32_t CreateProgram(GLenum type, const void* program, uint32_t program_size)
    {
        glEnable(type);
        GLuint shader[1];
        glGenProgramsARB(1, shader);
        CHECK_GL_ERROR

        glBindProgramARB(type, shader[0]);
        CHECK_GL_ERROR

        glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, program_size, program);
        CHECK_GL_ERROR

        glDisable(type);
        CHECK_GL_ERROR

        return shader[0];
    }

    HVertexProgram NewVertexProgram(const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateProgram(GL_VERTEX_PROGRAM_ARB, program, program_size);
    }

    HFragmentProgram NewFragmentProgram(const void* program, uint32_t program_size)
    {
        assert(program);

        return CreateProgram(GL_FRAGMENT_PROGRAM_ARB, program, program_size);
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        glDeleteProgramsARB(1, &program);
        CHECK_GL_ERROR
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        glDeleteProgramsARB(1, &program);
        CHECK_GL_ERROR
    }

    static void SetProgram(GLenum type, uint32_t program)
    {
        glEnable(type);
        CHECK_GL_ERROR
        glBindProgramARB(type, program);
        CHECK_GL_ERROR
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
        CHECK_GL_ERROR
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
            CHECK_GL_ERROR
        }

    }

    void SetFragmentConstant(HContext context, const Vector4* data, int base_register)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, 1);
        CHECK_GL_ERROR
    }

    void SetVertexConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_VERTEX_PROGRAM_ARB, data, base_register, num_vectors);
        CHECK_GL_ERROR
    }

    void SetFragmentConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);

        SetProgramConstantBlock(context, GL_FRAGMENT_PROGRAM_ARB, data, base_register, num_vectors);
        CHECK_GL_ERROR
    }

    HRenderTarget NewRenderBuffer(uint32_t width, uint32_t height, TextureFormat format)
    {
        RenderTarget* rt = new RenderTarget;

        rt->m_Texture = NewTexture(width, height, TEXTURE_FORMAT_RGBA);

        glGenRenderbuffers(1, &rt->m_RboId);
        CHECK_GL_ERROR
        glBindRenderbuffer(GL_RENDERBUFFER_EXT, rt->m_RboId);
        CHECK_GL_ERROR
        glRenderbufferStorage(GL_RENDERBUFFER_EXT, GL_RGBA, width, height);
        CHECK_GL_ERROR
        glBindRenderbuffer(GL_RENDERBUFFER_EXT, 0);
        CHECK_GL_ERROR


        glGenFramebuffers(1, &rt->m_FboId);
        CHECK_GL_ERROR
        glBindFramebuffer(GL_FRAMEBUFFER_EXT, rt->m_FboId);
        CHECK_GL_ERROR

        // attach the texture to FBO color attachment point
        glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, rt->m_Texture->m_Texture, 0);
        CHECK_GL_ERROR

        // attach the renderbuffer to depth attachment point
        glFramebufferRenderbuffer(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, rt->m_RboId);
        CHECK_GL_ERROR

        return rt;
    }

    void DeleteRenderBuffer(HRenderTarget renderbuffer)
    {
        DeleteTexture(renderbuffer->m_Texture);
        delete renderbuffer;
    }

    void SetTexture(HContext context, HTexture texture)
    {
        assert(context);
        assert(texture);

        glEnable(GL_TEXTURE_2D);
        CHECK_GL_ERROR

        glBindTexture(GL_TEXTURE_2D, texture->m_Texture);
        CHECK_GL_ERROR

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        CHECK_GL_ERROR
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        CHECK_GL_ERROR
    }

    HTexture NewTexture(uint32_t width, uint32_t height, TextureFormat texture_format)
    {
        GLuint t;
        glGenTextures( 1, &t );
        CHECK_GL_ERROR

        glBindTexture(GL_TEXTURE_2D, t);
        CHECK_GL_ERROR

        glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE );
        CHECK_GL_ERROR

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        CHECK_GL_ERROR

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        CHECK_GL_ERROR

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
            CHECK_GL_ERROR
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
            CHECK_GL_ERROR
            break;

        case TEXTURE_FORMAT_RGB_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT1:
        case TEXTURE_FORMAT_RGBA_DXT3:
        case TEXTURE_FORMAT_RGBA_DXT5:
            glCompressedTexImage2D(GL_TEXTURE_2D, mip_map, gl_format, width, height, border, data_size, data);
            CHECK_GL_ERROR
            break;
        default:
            assert(0);
        }
    }

    void DeleteTexture(HTexture texture)
    {
        assert(texture);

        glDeleteTextures(1, &texture->m_Texture);

        delete texture;
    }

    void EnableState(HContext context, RenderState state)
    {
        assert(context);
        glEnable(state);
        CHECK_GL_ERROR
    }

    void DisableState(HContext context, RenderState state)
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

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        glDepthMask(mask);
        CHECK_GL_ERROR
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
        glCullFace(face_type);
        CHECK_GL_ERROR
    }

    uint32_t GetWindowParam(WindowParam param)
    {
        return glfwGetWindowParam(param);
    }

    uint32_t GetWindowWidth()
    {
        return gdevice.m_DisplayWidth;
    }

    uint32_t GetWindowHeight()
    {
        return gdevice.m_DisplayHeight;
    }
}
