#include <string.h>
#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "../graphics.h"
#include "../graphics_native.h"
#include "graphics_null.h"
#include "glsl_uniform_parser.h"

using namespace Vectormath::Aos;

uint64_t g_DrawCount = 0;

// Used only for tests
bool g_ForceFragmentReloadFail = false;
bool g_ForceVertexReloadFail = false;

namespace dmGraphics
{
    uint16_t TYPE_SIZE[] =
    {
        sizeof(char), // TYPE_BYTE
        sizeof(unsigned char), // TYPE_UNSIGNED_BYTE
        sizeof(short), // TYPE_SHORT
        sizeof(unsigned short), // TYPE_UNSIGNED_SHORT
        sizeof(int), // TYPE_INT
        sizeof(unsigned int), // TYPE_UNSIGNED_INT
        sizeof(float) // TYPE_FLOAT
    };

    bool g_ContextCreated = false;

    bool Initialize()
    {
        return true;
    }

    void Finalize()
    {
        // nop
    }

    Context::Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
    }

    HContext NewContext(const ContextParams& params)
    {
        if (!g_ContextCreated)
        {
            g_ContextCreated = true;
            return new Context(params);
        }
        else
        {
            return 0x0;
        }
    }

    void DeleteContext(HContext context)
    {
        assert(context);
        if (g_ContextCreated)
        {
            delete context;
            g_ContextCreated = false;
        }
    }

    WindowResult OpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(params);
        if (context->m_WindowOpened)
            return WINDOW_RESULT_ALREADY_OPENED;
        context->m_WindowResizeCallback = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData = params->m_CloseCallbackUserData;
        context->m_Width = params->m_Width;
        context->m_Height = params->m_Height;
        context->m_WindowWidth = params->m_Width;
        context->m_WindowHeight = params->m_Height;
        context->m_Dpi = 0;
        context->m_WindowOpened = 1;
        uint32_t buffer_size = 4 * context->m_WindowWidth * context->m_WindowHeight;
        context->m_MainFrameBuffer.m_ColorBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_DepthBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_StencilBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_ColorBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_DepthBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_StencilBufferSize = buffer_size;
        context->m_CurrentFrameBuffer = &context->m_MainFrameBuffer;
        context->m_Program = 0x0;
        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: null");
        }
        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer;
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_WindowOpened = 0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
        }
    }

    void IconifyWindow(HContext context)
    {
        assert(context);
    }

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
                return context->m_WindowOpened;
            default:
                return 0;
        }
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        assert(context);
        return context->m_Dpi;
    }

    uint32_t GetWidth(HContext context)
    {
        return context->m_Width;
    }

    uint32_t GetHeight(HContext context)
    {
        return context->m_Height;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        return context->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        return context->m_WindowHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer;
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_Width = width;
            context->m_Height = height;
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;
            uint32_t buffer_size = 4 * width * height;
            main.m_ColorBuffer = new char[buffer_size];
            main.m_ColorBufferSize = buffer_size;
            main.m_DepthBuffer = new char[buffer_size];
            main.m_DepthBufferSize = buffer_size;
            main.m_StencilBuffer = new char[buffer_size];
            main.m_StencilBufferSize = buffer_size;
            if (context->m_WindowResizeCallback)
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
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
        if (flags & dmGraphics::BUFFER_TYPE_COLOR_BIT)
        {
            uint32_t colour = (red << 24) | (green << 16) | (blue << 8) | alpha;
            uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_ColorBuffer;
            uint32_t count = context->m_CurrentFrameBuffer->m_ColorBufferSize / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
                buffer[i] = colour;
        }
        if (flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT)
        {
            float* buffer = (float*)context->m_CurrentFrameBuffer->m_DepthBuffer;
            uint32_t count = context->m_CurrentFrameBuffer->m_DepthBufferSize / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
                buffer[i] = depth;
        }
        if (flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_StencilBuffer;
            uint32_t count = context->m_CurrentFrameBuffer->m_StencilBufferSize / sizeof(uint32_t);
            for (uint32_t i = 0; i < count; ++i)
                buffer[i] = stencil;
        }
    }

    void Flip(HContext context)
    {
        // Mimick glfw
        if (context->m_RequestWindowClose)
        {
            if (context->m_WindowCloseCallback != 0x0 && context->m_WindowCloseCallback(context->m_WindowCloseCallbackUserData))
            {
                CloseWindow(context);
            }
        }

        g_DrawCount = 0;
    }

    void SetSwapInterval(HContext /*context*/, uint32_t /*swap_interval*/)
    {
        // NOP
    }

    #define NATIVE_HANDLE_IMPL(return_type, func_name) return_type GetNative##func_name() { return NULL; }

    NATIVE_HANDLE_IMPL(id, iOSUIWindow);
    NATIVE_HANDLE_IMPL(id, iOSUIView);
    NATIVE_HANDLE_IMPL(id, iOSEAGLContext);
    NATIVE_HANDLE_IMPL(id, OSXNSWindow);
    NATIVE_HANDLE_IMPL(id, OSXNSView);
    NATIVE_HANDLE_IMPL(id, OSXNSOpenGLContext);
    NATIVE_HANDLE_IMPL(HWND, WindowsHWND);
    NATIVE_HANDLE_IMPL(HGLRC, WindowsHGLRC);
    NATIVE_HANDLE_IMPL(EGLContext, AndroidEGLContext);
    NATIVE_HANDLE_IMPL(EGLSurface, AndroidEGLSurface);
    NATIVE_HANDLE_IMPL(JavaVM*, AndroidJavaVM);
    NATIVE_HANDLE_IMPL(jobject, AndroidActivity);
    NATIVE_HANDLE_IMPL(Window, X11Window);
    NATIVE_HANDLE_IMPL(GLXContext, X11GLXContext);

    #undef NATIVE_HANDLE_IMPL

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = new VertexBuffer();
        vb->m_Buffer = new char[size];
        vb->m_Copy = 0x0;
        vb->m_Size = size;
        if (size > 0 && data != 0x0)
            memcpy(vb->m_Buffer, data, size);
        return (uintptr_t)vb;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        assert(vb->m_Copy == 0x0);
        delete [] vb->m_Buffer;
        delete vb;
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        assert(vb->m_Copy == 0x0);
        delete [] vb->m_Buffer;
        vb->m_Buffer = new char[size];
        vb->m_Size = size;
        if (data != 0x0)
            memcpy(vb->m_Buffer, data, size);
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        if (offset + size <= vb->m_Size && data != 0x0)
            memcpy(&(vb->m_Buffer)[offset], data, size);
    }

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        vb->m_Copy = new char[vb->m_Size];
        memcpy(vb->m_Copy, vb->m_Buffer, vb->m_Size);
        return vb->m_Copy;
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        memcpy(vb->m_Buffer, vb->m_Copy, vb->m_Size);
        delete [] vb->m_Copy;
        vb->m_Copy = 0x0;
        return true;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = new IndexBuffer();
        ib->m_Buffer = new char[size];
        ib->m_Copy = 0x0;
        ib->m_Size = size;
        memcpy(ib->m_Buffer, data, size);
        return (uintptr_t)ib;
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        assert(ib->m_Copy == 0x0);
        delete [] ib->m_Buffer;
        delete ib;
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        assert(ib->m_Copy == 0x0);
        delete [] ib->m_Buffer;
        ib->m_Buffer = new char[size];
        ib->m_Size = size;
        if (data != 0x0)
            memcpy(ib->m_Buffer, data, size);
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        if (offset + size <= ib->m_Size && data != 0x0)
            memcpy(&(ib->m_Buffer)[offset], data, size);
    }

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        ib->m_Copy = new char[ib->m_Size];
        memcpy(ib->m_Copy, ib->m_Buffer, ib->m_Size);
        return ib->m_Copy;
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        memcpy(ib->m_Buffer, ib->m_Copy, ib->m_Size);
        delete [] ib->m_Copy;
        ib->m_Copy = 0x0;
        return true;
    }

    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        return NewVertexDeclaration(context, element, count);
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(*vd));
        vd->m_Count = count;
        for (uint32_t i = 0; i < count; ++i)
        {
            assert(vd->m_Elements[element[i].m_Stream].m_Size == 0);
            vd->m_Elements[element[i].m_Stream] = element[i];
        }
        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    static void EnableVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        VertexStream& s = context->m_VertexStreams[stream];
        assert(s.m_Source == 0x0);
        assert(s.m_Buffer == 0x0);
        s.m_Source = vertex_buffer;
        s.m_Size = size * TYPE_SIZE[type - dmGraphics::TYPE_BYTE];
        s.m_Stride = stride;
    }

    static void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);
        VertexStream& s = context->m_VertexStreams[stream];
        s.m_Size = 0;
        if (s.m_Buffer != 0x0)
        {
            delete [] (char*)s.m_Buffer;
            s.m_Buffer = 0x0;
        }
        s.m_Source = 0x0;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        assert(context);
        assert(vertex_declaration);
        assert(vertex_buffer);
        VertexBuffer* vb = (VertexBuffer*)vertex_buffer;
        uint16_t stride = 0;
        for (uint32_t i = 0; i < vertex_declaration->m_Count; ++i)
            stride += vertex_declaration->m_Elements[i].m_Size * TYPE_SIZE[vertex_declaration->m_Elements[i].m_Type - dmGraphics::TYPE_BYTE];
        uint32_t offset = 0;
        for (uint16_t i = 0; i < vertex_declaration->m_Count; ++i)
        {
            VertexElement& ve = vertex_declaration->m_Elements[i];
            if (ve.m_Size > 0)
            {
                EnableVertexStream(context, i, ve.m_Size, ve.m_Type, stride, &vb->m_Buffer[offset]);
                offset += ve.m_Size * TYPE_SIZE[ve.m_Type - dmGraphics::TYPE_BYTE];
            }
        }
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);
        for (uint32_t i = 0; i < vertex_declaration->m_Count; ++i)
            if (vertex_declaration->m_Elements[i].m_Size > 0)
                DisableVertexStream(context, i);
    }

    static uint32_t GetIndex(Type type, HIndexBuffer ib, uint32_t index)
    {
        const void* index_buffer = ((IndexBuffer*) ib)->m_Buffer;
        uint32_t result = ~0;
        switch (type)
        {
            case dmGraphics::TYPE_BYTE:
                result = ((char*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_UNSIGNED_BYTE:
                result = ((unsigned char*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_SHORT:
                result = ((short*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_UNSIGNED_SHORT:
                result = ((unsigned short*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_INT:
                result = ((int*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_UNSIGNED_INT:
                result = ((unsigned int*)index_buffer)[index];
                break;
            case dmGraphics::TYPE_FLOAT:
                result = ((float*)index_buffer)[index];
                break;
            default:
                assert(0);
                break;
        }
        return result;
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
        assert(index_buffer);
        for (uint32_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
        {
            VertexStream& vs = context->m_VertexStreams[i];
            if (vs.m_Size > 0)
            {
                vs.m_Buffer = new char[vs.m_Size * count];
            }
        }
        for (uint32_t i = 0; i < count; ++i)
        {
            uint32_t index = GetIndex(type, index_buffer, i + first);
            for (uint32_t j = 0; j < MAX_VERTEX_STREAM_COUNT; ++j)
            {
                VertexStream& vs = context->m_VertexStreams[j];
                if (vs.m_Size > 0)
                    memcpy(&((char*)vs.m_Buffer)[i * vs.m_Size], &((char*)vs.m_Source)[index * vs.m_Stride], vs.m_Size);
            }
        }

        g_DrawCount++;
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);
        g_DrawCount++;
    }

    uint64_t GetDrawCount()
    {
        return g_DrawCount;
    }

    struct VertexProgram
    {
        char* m_Data;
    };

    struct FragmentProgram
    {
        char* m_Data;
    };

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata);

    struct Uniform
    {
        Uniform() : m_Name(0) {};
        char* m_Name;
        uint32_t m_Index;
        Type m_Type;
    };

    struct Program
    {
        Program(VertexProgram* vp, FragmentProgram* fp)
        {
            m_Uniforms.SetCapacity(16);
            m_VP = vp;
            m_FP = fp;
            if (m_VP != 0x0)
                GLSLUniformParse(m_VP->m_Data, NullUniformCallback, (uintptr_t)this);
            if (m_FP != 0x0)
                GLSLUniformParse(m_FP->m_Data, NullUniformCallback, (uintptr_t)this);
        }

        ~Program()
        {
            for(uint32_t i = 0; i < m_Uniforms.Size(); ++i)
                delete[] m_Uniforms[i].m_Name;
        }

        VertexProgram* m_VP;
        FragmentProgram* m_FP;
        dmArray<Uniform> m_Uniforms;
    };

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata)
    {
        Program* program = (Program*) userdata;
        if(program->m_Uniforms.Full())
            program->m_Uniforms.OffsetCapacity(16);
        Uniform uniform;
        name_length++;
        uniform.m_Name = new char[name_length];
        dmStrlCpy(uniform.m_Name, name, name_length);
        uniform.m_Index = program->m_Uniforms.Size();
        uniform.m_Type = type;
        program->m_Uniforms.Push(uniform);
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        VertexProgram* vertex = 0x0;
        FragmentProgram* fragment = 0x0;
        if (vertex_program != INVALID_VERTEX_PROGRAM_HANDLE)
            vertex = (VertexProgram*) vertex_program;
        if (fragment_program != INVALID_FRAGMENT_PROGRAM_HANDLE)
            fragment = (FragmentProgram*) fragment_program;
        return (HProgram) new Program(vertex, fragment);
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        delete (Program*) program;
    }

    HVertexProgram NewVertexProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = new VertexProgram();
        p->m_Data = new char[program_size+1];
        memcpy(p->m_Data, program, program_size);
        p->m_Data[program_size] = '\0';
        return (uintptr_t)p;
    }

    HFragmentProgram NewFragmentProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = new FragmentProgram();
        p->m_Data = new char[program_size+1];
        memcpy(p->m_Data, program, program_size);
        p->m_Data[program_size] = '\0';
        return (uintptr_t)p;
    }

    bool ReloadVertexProgram(HVertexProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
        return !g_ForceVertexReloadFail;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
        return !g_ForceFragmentReloadFail;
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void EnableProgram(HContext context, HProgram program)
    {
        assert(context);
        context->m_Program = (void*)program;
    }

    void DisableProgram(HContext context)
    {
        assert(context);
        context->m_Program = 0x0;
    }

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        (void) context;
        (void) program;

        return true;
    }

    uint32_t GetUniformCount(HProgram prog)
    {
        return ((Program*)prog)->m_Uniforms.Size();
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {
        Program* program = (Program*)prog;
        assert(index < program->m_Uniforms.Size());
        Uniform& uniform = program->m_Uniforms[index];
        *buffer = '\0';
        dmStrlCat(buffer, uniform.m_Name, buffer_size);
        *type = uniform.m_Type;
    }

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        Program* program = (Program*)prog;
        uint32_t count = program->m_Uniforms.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Uniform& uniform = program->m_Uniforms[i];
            if (dmStrCaseCmp(uniform.m_Name, name)==0)
            {
                return (int32_t)uniform.m_Index;
            }
        }
        return -1;
    }

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
    }

    const Vector4& GetConstantV4Ptr(HContext context, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        return context->m_ProgramRegisters[base_register];
    }

    void SetConstantV4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4));
    }

    void SetConstantM4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4) * 4);
    }

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {
    }

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget();
        memset(rt, 0, sizeof(RenderTarget));

        void** buffers[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBuffer, &rt->m_FrameBuffer.m_DepthBuffer, &rt->m_FrameBuffer.m_StencilBuffer};
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBufferSize, &rt->m_FrameBuffer.m_DepthBufferSize, &rt->m_FrameBuffer.m_StencilBufferSize};
        BufferType buffer_types[MAX_BUFFER_TYPE_COUNT] = {BUFFER_TYPE_COLOR_BIT, BUFFER_TYPE_DEPTH_BIT, BUFFER_TYPE_STENCIL_BIT};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            assert(GetBufferTypeIndex(buffer_types[i]) == i);
            if (buffer_type_flags & buffer_types[i])
            {
                uint32_t buffer_size = sizeof(uint32_t) * params[i].m_Width * params[i].m_Height;
                *(buffers[i]) = new char[buffer_size];
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i] = params[i];
                rt->m_BufferTextureParams[i].m_Data = 0x0;
                rt->m_BufferTextureParams[i].m_DataSize = 0;

                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR_BIT))
                {
                    rt->m_ColorBufferTexture = NewTexture(context, creation_params[i]);
                    SetTexture(rt->m_ColorBufferTexture, rt->m_BufferTextureParams[i]);
                }
            }
        }

        return rt;
    }

    void DeleteRenderTarget(HRenderTarget rt)
    {
        if (rt->m_ColorBufferTexture)
            DeleteTexture(rt->m_ColorBufferTexture);
        delete [] (char*)rt->m_FrameBuffer.m_ColorBuffer;
        delete [] (char*)rt->m_FrameBuffer.m_DepthBuffer;
        delete [] (char*)rt->m_FrameBuffer.m_StencilBuffer;
        delete rt;
    }

    void EnableRenderTarget(HContext context, HRenderTarget rendertarget)
    {
        assert(context);
        assert(rendertarget);
        context->m_CurrentFrameBuffer = &rendertarget->m_FrameBuffer;
    }

    void DisableRenderTarget(HContext context, HRenderTarget rendertarget)
    {
        assert(context);
        assert(rendertarget);
        context->m_CurrentFrameBuffer = &context->m_MainFrameBuffer;
    }

    HTexture GetRenderTargetTexture(HRenderTarget rendertarget, BufferType buffer_type)
    {
        if(buffer_type != BUFFER_TYPE_COLOR_BIT)
            return 0;
        return rendertarget->m_ColorBufferTexture;
    }

    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        assert(render_target);
        uint32_t i = GetBufferTypeIndex(buffer_type);
        assert(i < MAX_BUFFER_TYPE_COUNT);
        width = render_target->m_BufferTextureParams[i].m_Width;
        height = render_target->m_BufferTextureParams[i].m_Height;
    }

    void SetRenderTargetSize(HRenderTarget rt, uint32_t width, uint32_t height)
    {
        void** buffers[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBuffer, &rt->m_FrameBuffer.m_DepthBuffer, &rt->m_FrameBuffer.m_StencilBuffer};
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBufferSize, &rt->m_FrameBuffer.m_DepthBufferSize, &rt->m_FrameBuffer.m_StencilBufferSize};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            if (buffers[i])
            {
                delete [] (char*)*(buffers[i]);
                uint32_t buffer_size = sizeof(uint32_t) * width * height;
                *(buffers[i]) = new char[buffer_size];
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i].m_Width = width;
                rt->m_BufferTextureParams[i].m_Height = height;
                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR_BIT))
                {
                    SetTexture(rt->m_ColorBufferTexture, rt->m_BufferTextureParams[i]);
                }
            }
        }
    }

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    uint32_t GetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        Texture* tex = new Texture();

        tex->m_Width = params.m_Width;
        tex->m_Height = params.m_Height;
        tex->m_Data = 0;

        if (params.m_OriginalWidth == 0) {
            tex->m_OriginalWidth = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        } else {
            tex->m_OriginalWidth = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        return tex;
    }

    void DeleteTexture(HTexture t)
    {
        assert(t);
        if (t->m_Data != 0x0)
            delete [] (char*)t->m_Data;
        delete t;
    }

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap)
    {
        assert(texture);
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {
        assert(texture);
        if (texture->m_Data != 0x0)
            delete [] (char*)texture->m_Data;
        texture->m_Format = params.m_Format;
        // Allocate even for 0x0 size so that the rendertarget dummies will work.
        texture->m_Data = new char[params.m_DataSize];
        memcpy(texture->m_Data, params.m_Data, params.m_DataSize);
    }

    uint8_t* GetTextureData(HTexture texture) {
        return 0x0;
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
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        assert(texture->m_Data);
        context->m_Textures[unit] = texture;
    }

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        context->m_Textures[unit] = 0;
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        uint32_t w = dmGraphics::GetWidth(context);
        uint32_t h = dmGraphics::GetHeight(context);
        assert (buffer_size >= w * h * 4);
        memset(buffer, 0, w * h * 4);
    }

    void EnableState(HContext context, State state)
    {
        assert(context);
    }

    void DisableState(HContext context, State state)
    {
        assert(context);
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
    }

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        context->m_RedMask = red;
        context->m_GreenMask = green;
        context->m_BlueMask = blue;
        context->m_AlphaMask = alpha;
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        context->m_DepthMask = mask;
    }

    void SetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        context->m_DepthFunc = func;
    }

    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
        context->m_ScissorRect[0] = (int32_t) x;
        context->m_ScissorRect[1] = (int32_t) y;
        context->m_ScissorRect[2] = (int32_t) x+width;
        context->m_ScissorRect[3] = (int32_t) y+height;
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        context->m_StencilMask = mask;
    }

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        context->m_StencilFunc = func;
        context->m_StencilFuncRef = ref;
        context->m_StencilFuncMask = mask;
    }

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        context->m_StencilOpSFail = sfail;
        context->m_StencilOpDPFail = dpfail;
        context->m_StencilOpDPPass = dppass;
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
    }

    void SetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
    }

    bool AcquireSharedContext()
    {
        return false;
    }

    void UnacquireContext()
    {

    }

    void SetTextureAsync(HTexture texture, const TextureParams& params)
    {
        SetTexture(texture, params);
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    void SetForceFragmentReloadFail(bool should_fail)
    {
        g_ForceFragmentReloadFail = should_fail;
    }

    void SetForceVertexReloadFail(bool should_fail)
    {
        g_ForceVertexReloadFail = should_fail;
    }

}

