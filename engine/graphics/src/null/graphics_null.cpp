// Copyright 2020-2022 The Defold Foundation
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
#include <assert.h>
#include <dmsdk/dlib/vmath.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"
#include "graphics_null_private.h"
#include "glsl_uniform_parser.h"

uint64_t g_DrawCount = 0;
uint64_t g_Flipped = 0;

// Used only for tests
bool g_ForceFragmentReloadFail = false;
bool g_ForceVertexReloadFail = false;

namespace dmGraphics
{
    using namespace dmVMath;

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

    static GraphicsAdapterFunctionTable NullRegisterFunctionTable();
    static bool                         NullIsSupported();
    static const int8_t    g_null_adapter_priority = 2;
    static GraphicsAdapter g_null_adapter(ADAPTER_TYPE_NULL);

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterNull, &g_null_adapter, NullIsSupported, NullRegisterFunctionTable, g_null_adapter_priority);

    static bool NullInitialize()
    {
        return true;
    }

    static void NullFinalize()
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

    static HContext NullNewContext(const ContextParams& params)
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

    static bool NullIsSupported()
    {
        return true;
    }

    static void NullDeleteContext(HContext context)
    {
        assert(context);
        if (g_ContextCreated)
        {
            delete context;
            g_ContextCreated = false;
        }
    }

    static WindowResult NullOpenWindow(HContext context, WindowParams* params)
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
        context->m_MainFrameBuffer.m_ColorBuffer[0] = new char[buffer_size];
        context->m_MainFrameBuffer.m_ColorBufferSize[0] = buffer_size;
        context->m_MainFrameBuffer.m_DepthBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_StencilBuffer = new char[buffer_size];
        context->m_MainFrameBuffer.m_DepthBufferSize = buffer_size;
        context->m_MainFrameBuffer.m_StencilBufferSize = buffer_size;
        context->m_CurrentFrameBuffer = &context->m_MainFrameBuffer;
        context->m_Program = 0x0;
        context->m_PipelineState = GetDefaultPipelineState();

        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: null");
        }
        return WINDOW_RESULT_OK;
    }

    static uint32_t NullGetWindowRefreshRate(HContext context)
    {
        assert(context);
        return 0;
    }

    static void NullCloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer[0];
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_WindowOpened = 0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
        }
    }

    static void NullIconifyWindow(HContext context)
    {
        assert(context);
    }

    static void NullRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    static uint32_t NullGetWindowState(HContext context, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
                return context->m_WindowOpened;
            default:
                return 0;
        }
    }

    static uint32_t NullGetDisplayDpi(HContext context)
    {
        assert(context);
        return context->m_Dpi;
    }

    static uint32_t NullGetWidth(HContext context)
    {
        return context->m_Width;
    }

    static uint32_t NullGetHeight(HContext context)
    {
        return context->m_Height;
    }

    static uint32_t NullGetWindowWidth(HContext context)
    {
        return context->m_WindowWidth;
    }

    static float NullGetDisplayScaleFactor(HContext context)
    {
        return 1.0f;
    }

    static uint32_t NullGetWindowHeight(HContext context)
    {
        return context->m_WindowHeight;
    }

    static void NullSetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer[0];
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_Width = width;
            context->m_Height = height;
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;
            uint32_t buffer_size = 4 * width * height;
            main.m_ColorBuffer[0] = new char[buffer_size];
            main.m_ColorBufferSize[0] = buffer_size;
            main.m_DepthBuffer = new char[buffer_size];
            main.m_DepthBufferSize = buffer_size;
            main.m_StencilBuffer = new char[buffer_size];
            main.m_StencilBufferSize = buffer_size;

            if (context->m_WindowResizeCallback)
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
        }
    }

    static void NullResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;

            if (context->m_WindowResizeCallback)
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
        }
    }

    static void NullGetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void NullClear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);

        BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (flags & color_buffer_flags[i])
            {
                uint32_t colour = (red << 24) | (green << 16) | (blue << 8) | alpha;
                uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_ColorBuffer[i];
                uint32_t count = context->m_CurrentFrameBuffer->m_ColorBufferSize[i] / sizeof(uint32_t);
                for (uint32_t i = 0; i < count; ++i)
                    buffer[i] = colour;
            }
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

    static void NullBeginFrame(HContext context)
    {
        // NOP
    }

    static void NullFlip(HContext context)
    {
        // Mimick glfw
        if (context->m_RequestWindowClose)
        {
            if (context->m_WindowCloseCallback != 0x0 && context->m_WindowCloseCallback(context->m_WindowCloseCallbackUserData))
            {
                CloseWindow(context);
            }
        }

        g_Flipped = 1;
    }

    static void NullSetSwapInterval(HContext /*context*/, uint32_t /*swap_interval*/)
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
    NATIVE_HANDLE_IMPL(android_app*, AndroidApp);
    NATIVE_HANDLE_IMPL(Window, X11Window);
    NATIVE_HANDLE_IMPL(GLXContext, X11GLXContext);

    #undef NATIVE_HANDLE_IMPL

    static HVertexBuffer NullNewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = new VertexBuffer();
        vb->m_Buffer = new char[size];
        vb->m_Copy = 0x0;
        vb->m_Size = size;
        if (size > 0 && data != 0x0)
            memcpy(vb->m_Buffer, data, size);
        return (uintptr_t)vb;
    }

    static void NullDeleteVertexBuffer(HVertexBuffer buffer)
    {
        if (!buffer)
            return;
        VertexBuffer* vb = (VertexBuffer*)buffer;
        assert(vb->m_Copy == 0x0);
        delete [] vb->m_Buffer;
        delete vb;
    }

    static void NullSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        assert(vb->m_Copy == 0x0);
        delete [] vb->m_Buffer;
        vb->m_Buffer = new char[size];
        vb->m_Size = size;
        if (data != 0x0)
            memcpy(vb->m_Buffer, data, size);
    }

    static void NullSetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        if (offset + size <= vb->m_Size && data != 0x0)
            memcpy(&(vb->m_Buffer)[offset], data, size);
    }

    static void* NullMapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        vb->m_Copy = new char[vb->m_Size];
        memcpy(vb->m_Copy, vb->m_Buffer, vb->m_Size);
        return vb->m_Copy;
    }

    static bool NullUnmapVertexBuffer(HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        memcpy(vb->m_Buffer, vb->m_Copy, vb->m_Size);
        delete [] vb->m_Copy;
        vb->m_Copy = 0x0;
        return true;
    }

    static uint32_t NullGetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    static HIndexBuffer NullNewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = new IndexBuffer();
        ib->m_Buffer = new char[size];
        ib->m_Copy = 0x0;
        ib->m_Size = size;
        memcpy(ib->m_Buffer, data, size);
        return (uintptr_t)ib;
    }

    static void NullDeleteIndexBuffer(HIndexBuffer buffer)
    {
        if (!buffer)
            return;
        IndexBuffer* ib = (IndexBuffer*)buffer;
        assert(ib->m_Copy == 0x0);
        delete [] ib->m_Buffer;
        delete ib;
    }

    static void NullSetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        assert(ib->m_Copy == 0x0);
        delete [] ib->m_Buffer;
        ib->m_Buffer = new char[size];
        ib->m_Size = size;
        if (data != 0x0)
            memcpy(ib->m_Buffer, data, size);
    }

    static void NullSetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        if (offset + size <= ib->m_Size && data != 0x0)
            memcpy(&(ib->m_Buffer)[offset], data, size);
    }

    static void* NullMapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        ib->m_Copy = new char[ib->m_Size];
        memcpy(ib->m_Copy, ib->m_Buffer, ib->m_Size);
        return ib->m_Copy;
    }

    static bool NullUnmapIndexBuffer(HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        memcpy(ib->m_Buffer, ib->m_Copy, ib->m_Size);
        delete [] ib->m_Copy;
        ib->m_Copy = 0x0;
        return true;
    }

    static bool NullIsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    static uint32_t NullGetMaxElementsIndices(HContext context)
    {
        return 65536;
    }

    static bool NullIsMultiTargetRenderingSupported(HContext context)
    {
        return true;
    }

    static HVertexDeclaration NullNewVertexDeclarationStride(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        return NewVertexDeclaration(context, element, count);
    }

    static HVertexDeclaration NullNewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
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

    bool NullSetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset)
    {
        if (stream_index > vertex_declaration->m_Count) {
            return false;
        }

        return true;
    }

    static void NullDeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
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

    static void NullEnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
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

    static void NullEnableVertexDeclarationProgram(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        NullEnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
    }

    static void NullDisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);
        for (uint32_t i = 0; i < vertex_declaration->m_Count; ++i)
            if (vertex_declaration->m_Elements[i].m_Size > 0)
                DisableVertexStream(context, i);
    }

    void NullHashVertexDeclaration(HashState32 *state, HVertexDeclaration vertex_declaration)
    {
        uint16_t stream_count = vertex_declaration->m_Count;
        for (int i = 0; i < stream_count; ++i)
        {
            VertexElement& vert_elem = vertex_declaration->m_Elements[i];
            dmHashUpdateBuffer32(state, vert_elem.m_Name, strlen(vert_elem.m_Name));
            dmHashUpdateBuffer32(state, &vert_elem.m_Stream, sizeof(vert_elem.m_Stream));
            dmHashUpdateBuffer32(state, &vert_elem.m_Size, sizeof(vert_elem.m_Size));
            dmHashUpdateBuffer32(state, &vert_elem.m_Type, sizeof(vert_elem.m_Type));
            dmHashUpdateBuffer32(state, &vert_elem.m_Normalize, sizeof(vert_elem.m_Normalize));
        }
    }

    static uint32_t GetIndex(Type type, HIndexBuffer ib, uint32_t index)
    {
        const void* index_buffer = ((IndexBuffer*) ib)->m_Buffer;

        if (type == dmGraphics::TYPE_BYTE)
        {
            return ((char*)index_buffer)[index];
        }
        else if (type == dmGraphics::TYPE_UNSIGNED_BYTE)
        {
            return ((unsigned char*)index_buffer)[index];
        }
        else if (type == dmGraphics::TYPE_SHORT)
        {
            return ((short*)index_buffer)[index];
        }
        else if (type == dmGraphics::TYPE_UNSIGNED_SHORT)
        {
            return ((unsigned short*)index_buffer)[index];
        }
        else if (type == dmGraphics::TYPE_INT)
        {
            return ((int*)index_buffer)[index];
        }
        else if (type == dmGraphics::TYPE_UNSIGNED_INT)
        {
            return ((unsigned int*)index_buffer)[index];
        }
        else if (type == dmGraphics::TYPE_FLOAT)
        {
            return (uint32_t)((float*)index_buffer)[index];
        }

        assert(0);
        return ~0;
    }

    static void NullDrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
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

        if (g_Flipped)
        {
            g_Flipped = 0;
            g_DrawCount = 0;
        }
        g_DrawCount++;
    }

    static void NullDraw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);

        if (g_Flipped)
        {
            g_Flipped = 0;
            g_DrawCount = 0;
        }
        g_DrawCount++;
    }

    // For tests
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

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uint32_t size, uintptr_t userdata);

    struct Uniform
    {
        Uniform() : m_Name(0) {};
        char* m_Name;
        uint32_t m_Index;
        uint32_t m_Size;
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

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uint32_t size, uintptr_t userdata)
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
        uniform.m_Size = size;
        program->m_Uniforms.Push(uniform);
    }

    static HProgram NullNewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        VertexProgram* vertex = 0x0;
        FragmentProgram* fragment = 0x0;
        if (vertex_program != INVALID_VERTEX_PROGRAM_HANDLE)
            vertex = (VertexProgram*) vertex_program;
        if (fragment_program != INVALID_FRAGMENT_PROGRAM_HANDLE)
            fragment = (FragmentProgram*) fragment_program;
        return (HProgram) new Program(vertex, fragment);
    }

    static void NullDeleteProgram(HContext context, HProgram program)
    {
        delete (Program*) program;
    }

    static HVertexProgram NullNewVertexProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        assert(ddf);
        VertexProgram* p = new VertexProgram();
        p->m_Data = new char[ddf->m_Source.m_Count+1];
        memcpy(p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        p->m_Data[ddf->m_Source.m_Count] = '\0';
        return (uintptr_t)p;
    }

    static HFragmentProgram NullNewFragmentProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        assert(ddf);
        FragmentProgram* p = new FragmentProgram();
        p->m_Data = new char[ddf->m_Source.m_Count+1];
        memcpy(p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        p->m_Data[ddf->m_Source.m_Count] = '\0';
        return (uintptr_t)p;
    }

    static bool NullReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        assert(prog);
        assert(ddf);
        VertexProgram* p = (VertexProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[ddf->m_Source.m_Count];
        memcpy((char*)p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        return !g_ForceVertexReloadFail;
    }

    static bool NullReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        assert(prog);
        assert(ddf);
        FragmentProgram* p = (FragmentProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[ddf->m_Source.m_Count];
        memcpy((char*)p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        return !g_ForceFragmentReloadFail;
    }

    static void NullDeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    static void NullDeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    static ShaderDesc::Language NullGetShaderProgramLanguage(HContext context)
    {
        return ShaderDesc::LANGUAGE_GLSL_SM140;
    }

    static void NullEnableProgram(HContext context, HProgram program)
    {
        assert(context);
        context->m_Program = (void*)program;
    }

    static void NullDisableProgram(HContext context)
    {
        assert(context);
        context->m_Program = 0x0;
    }

    static bool NullReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        (void) context;
        (void) program;

        return true;
    }

    static uint32_t NullGetUniformCount(HProgram prog)
    {
        return ((Program*)prog)->m_Uniforms.Size();
    }

    static uint32_t NullGetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        Program* program = (Program*)prog;
        assert(index < program->m_Uniforms.Size());
        Uniform& uniform = program->m_Uniforms[index];
        *buffer = '\0';
        dmStrlCat(buffer, uniform.m_Name, buffer_size);
        *type = uniform.m_Type;
        *size = uniform.m_Size;
        return (uint32_t)strlen(buffer);
    }

    static int32_t NullGetUniformLocation(HProgram prog, const char* name)
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

    static void NullSetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
    }

    // Tests Only
    const Vector4& GetConstantV4Ptr(HContext context, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        return context->m_ProgramRegisters[base_register];
    }

    static void NullSetConstantV4(HContext context, const Vector4* data, int count, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4) * count);
    }

    static void NullSetConstantM4(HContext context, const Vector4* data, int count, int base_register)
    {
        assert(context);
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4) * 4 * count);
    }

    static void NullSetSampler(HContext context, int32_t location, int32_t unit)
    {
    }

    static HRenderTarget NullNewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget();
        memset(rt, 0, sizeof(RenderTarget));

        void** buffers[MAX_BUFFER_TYPE_COUNT] = {
            &rt->m_FrameBuffer.m_ColorBuffer[0],
            &rt->m_FrameBuffer.m_ColorBuffer[1],
            &rt->m_FrameBuffer.m_ColorBuffer[2],
            &rt->m_FrameBuffer.m_ColorBuffer[3],
            &rt->m_FrameBuffer.m_DepthBuffer,
            &rt->m_FrameBuffer.m_StencilBuffer,
        };
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {
            &rt->m_FrameBuffer.m_ColorBufferSize[0],
            &rt->m_FrameBuffer.m_ColorBufferSize[1],
            &rt->m_FrameBuffer.m_ColorBufferSize[2],
            &rt->m_FrameBuffer.m_ColorBufferSize[3],
            &rt->m_FrameBuffer.m_DepthBufferSize,
            &rt->m_FrameBuffer.m_StencilBufferSize,
        };

        BufferType buffer_types[MAX_BUFFER_TYPE_COUNT] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
            BUFFER_TYPE_DEPTH_BIT,
            BUFFER_TYPE_STENCIL_BIT,
        };

        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            assert(GetBufferTypeIndex(buffer_types[i]) == i);

            if (buffer_type_flags & buffer_types[i])
            {
                uint32_t bytes_per_pixel                = dmGraphics::GetTextureFormatBitsPerPixel(params[i].m_Format) / 3;
                uint32_t buffer_size                    = sizeof(uint32_t) * params[i].m_Width * params[i].m_Height * bytes_per_pixel;
                *(buffer_sizes[i])                      = buffer_size;
                rt->m_BufferTextureParams[i]            = params[i];
                rt->m_BufferTextureParams[i].m_Data     = 0x0;
                rt->m_BufferTextureParams[i].m_DataSize = 0;

                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR0_BIT)  ||
                   i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR1_BIT) ||
                   i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR2_BIT) ||
                   i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR3_BIT))
                {
                    rt->m_BufferTextureParams[i].m_DataSize = buffer_size;
                    rt->m_ColorBufferTexture[i]   = NewTexture(context, creation_params[i]);
                    SetTexture(rt->m_ColorBufferTexture[i], rt->m_BufferTextureParams[i]);
                    *(buffers[i]) = rt->m_ColorBufferTexture[i]->m_Data;
                } else {
                    *(buffers[i]) = new char[buffer_size];
                }
            }
        }

        return rt;
    }

    static void NullDeleteRenderTarget(HRenderTarget rt)
    {
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_ColorBufferTexture[i])
            {
                DeleteTexture(rt->m_ColorBufferTexture[i]);
            }
        }
        delete [] (char*)rt->m_FrameBuffer.m_DepthBuffer;
        delete [] (char*)rt->m_FrameBuffer.m_StencilBuffer;
        delete rt;
    }

    static void NullSetRenderTarget(HContext context, HRenderTarget rendertarget, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        assert(context);
        context->m_CurrentFrameBuffer = &rendertarget->m_FrameBuffer;
    }

    static HTexture NullGetRenderTargetTexture(HRenderTarget rendertarget, BufferType buffer_type)
    {
        if(!(buffer_type == BUFFER_TYPE_COLOR0_BIT ||
             buffer_type == BUFFER_TYPE_COLOR1_BIT ||
             buffer_type == BUFFER_TYPE_COLOR2_BIT ||
             buffer_type == BUFFER_TYPE_COLOR3_BIT))
        {
            return 0;
        }
        return rendertarget->m_ColorBufferTexture[GetBufferTypeIndex(buffer_type)];
    }

    static void NullGetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        assert(render_target);
        uint32_t i = GetBufferTypeIndex(buffer_type);
        assert(i < MAX_BUFFER_TYPE_COUNT);
        width = render_target->m_BufferTextureParams[i].m_Width;
        height = render_target->m_BufferTextureParams[i].m_Height;
    }

    static void NullSetRenderTargetSize(HRenderTarget rt, uint32_t width, uint32_t height)
    {
        uint32_t buffer_size = sizeof(uint32_t) * width * height;
        void** buffers[MAX_BUFFER_TYPE_COUNT] = {
            &rt->m_FrameBuffer.m_ColorBuffer[0],
            &rt->m_FrameBuffer.m_ColorBuffer[1],
            &rt->m_FrameBuffer.m_ColorBuffer[2],
            &rt->m_FrameBuffer.m_ColorBuffer[3],
            &rt->m_FrameBuffer.m_DepthBuffer,
            &rt->m_FrameBuffer.m_StencilBuffer,
        };
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {
            &rt->m_FrameBuffer.m_ColorBufferSize[0],
            &rt->m_FrameBuffer.m_ColorBufferSize[1],
            &rt->m_FrameBuffer.m_ColorBufferSize[2],
            &rt->m_FrameBuffer.m_ColorBufferSize[3],
            &rt->m_FrameBuffer.m_DepthBufferSize,
            &rt->m_FrameBuffer.m_StencilBufferSize,
        };

        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            if (buffers[i])
            {
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i].m_Width = width;
                rt->m_BufferTextureParams[i].m_Height = height;
                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR0_BIT)  ||
                   i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR1_BIT) ||
                   i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR2_BIT) ||
                   i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR3_BIT))
                {
                    if (rt->m_ColorBufferTexture[i])
                    {
                        rt->m_BufferTextureParams[i].m_DataSize = buffer_size;
                        SetTexture(rt->m_ColorBufferTexture[i], rt->m_BufferTextureParams[i]);
                        *(buffers[i]) = rt->m_ColorBufferTexture[i]->m_Data;
                    }
                } else {
                    delete [] (char*)*(buffers[i]);
                    *(buffers[i]) = new char[buffer_size];
                }
            }
        }
    }

    static bool NullIsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    static uint32_t NullGetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    static HTexture NullNewTexture(HContext context, const TextureCreationParams& params)
    {
        Texture* tex = new Texture();

        tex->m_Type = params.m_Type;
        tex->m_Width = params.m_Width;
        tex->m_Height = params.m_Height;
        tex->m_MipMapCount = 0;
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

    static void NullDeleteTexture(HTexture t)
    {
        assert(t);
        if (t->m_Data != 0x0)
            delete [] (char*)t->m_Data;
        delete t;
    }

    static HandleResult NullGetTextureHandle(HTexture texture, void** out_handle)
    {
        *out_handle = 0x0;

        if (!texture) {
            return HANDLE_RESULT_ERROR;
        }

        *out_handle = texture->m_Data;

        return HANDLE_RESULT_OK;
    }

    static void NullSetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        assert(texture);
    }

    static void NullSetTexture(HTexture texture, const TextureParams& params)
    {
        assert(texture);
        assert(!params.m_SubUpdate || (params.m_X + params.m_Width <= texture->m_Width));
        assert(!params.m_SubUpdate || (params.m_Y + params.m_Height <= texture->m_Height));

        if (texture->m_Data != 0x0)
            delete [] (char*)texture->m_Data;
        texture->m_Format = params.m_Format;
        // Allocate even for 0x0 size so that the rendertarget dummies will work.
        texture->m_Data = new char[params.m_DataSize];
        if (params.m_Data != 0x0)
            memcpy(texture->m_Data, params.m_Data, params.m_DataSize);
        texture->m_MipMapCount = dmMath::Max(texture->m_MipMapCount, (uint16_t)(params.m_MipMap+1));
    }

    // Not used?
    uint8_t* GetTextureData(HTexture texture)
    {
        return 0x0;
    }

    static uint32_t NullGetTextureResourceSize(HTexture texture)
    {
        uint32_t size_total = 0;
        uint32_t size = texture->m_Width * texture->m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(texture->m_Format)/8);
        for(uint32_t i = 0; i < texture->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP)
        {
            size_total *= 6;
        }
        return size_total + sizeof(Texture);
    }

    static uint16_t NullGetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    static uint16_t NullGetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    static uint16_t NullGetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    static uint16_t NullGetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    static void NullEnableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        assert(texture->m_Data);
        context->m_Textures[unit] = texture;
    }

    static void NullDisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        context->m_Textures[unit] = 0;
    }

    static void NullReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        uint32_t w = dmGraphics::GetWidth(context);
        uint32_t h = dmGraphics::GetHeight(context);
        assert (buffer_size >= w * h * 4);
        memset(buffer, 0, w * h * 4);
    }

    static void NullEnableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 1);
    }

    static void NullDisableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 0);
    }

    static void NullSetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
        context->m_PipelineState.m_BlendSrcFactor = source_factor;
        context->m_PipelineState.m_BlendDstFactor = destinaton_factor;
    }

    static void NullSetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        // Replace above
        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;
        context->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void NullSetDepthMask(HContext context, bool mask)
    {
        assert(context);
        context->m_PipelineState.m_WriteDepth = mask;
    }

    static void NullSetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        context->m_PipelineState.m_DepthTestFunc = func;
    }

    static void NullSetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
        context->m_ScissorRect[0] = (int32_t) x;
        context->m_ScissorRect[1] = (int32_t) y;
        context->m_ScissorRect[2] = (int32_t) x+width;
        context->m_ScissorRect[3] = (int32_t) y+height;
    }

    static void NullSetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        context->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void NullSetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void NullSetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        context->m_PipelineState.m_StencilBackOpFail       = sfail;
        context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void NullSetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
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

    static void NullSetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
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

    static void NullSetFaceWinding(HContext context, FaceWinding face_winding)
    {
        context->m_PipelineState.m_FaceWinding = face_winding;
    }

    static void NullSetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
    }

    static void NullSetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
    }

    static PipelineState NullGetPipelineState(HContext context)
    {
        return context->m_PipelineState;
    }

    // Not used?
    bool AcquireSharedContext()
    {
        return false;
    }

    // Not used?
    void UnacquireContext()
    {

    }

    static void NullSetTextureAsync(HTexture texture, const TextureParams& params)
    {
        SetTexture(texture, params);
    }

    static uint32_t NullGetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    // Tests only
    void SetForceFragmentReloadFail(bool should_fail)
    {
        g_ForceFragmentReloadFail = should_fail;
    }

    // Tests only
    void SetForceVertexReloadFail(bool should_fail)
    {
        g_ForceVertexReloadFail = should_fail;
    }

    static GraphicsAdapterFunctionTable NullRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table;
        memset(&fn_table,0,sizeof(fn_table));
        fn_table.m_NewContext = NullNewContext;
        fn_table.m_DeleteContext = NullDeleteContext;
        fn_table.m_Initialize = NullInitialize;
        fn_table.m_Finalize = NullFinalize;
        fn_table.m_GetWindowRefreshRate = NullGetWindowRefreshRate;
        fn_table.m_OpenWindow = NullOpenWindow;
        fn_table.m_CloseWindow = NullCloseWindow;
        fn_table.m_IconifyWindow = NullIconifyWindow;
        fn_table.m_GetWindowState = NullGetWindowState;
        fn_table.m_GetDisplayDpi = NullGetDisplayDpi;
        fn_table.m_GetWidth = NullGetWidth;
        fn_table.m_GetHeight = NullGetHeight;
        fn_table.m_GetWindowWidth = NullGetWindowWidth;
        fn_table.m_GetWindowHeight = NullGetWindowHeight;
        fn_table.m_GetDisplayScaleFactor = NullGetDisplayScaleFactor;
        fn_table.m_SetWindowSize = NullSetWindowSize;
        fn_table.m_ResizeWindow = NullResizeWindow;
        fn_table.m_GetDefaultTextureFilters = NullGetDefaultTextureFilters;
        fn_table.m_BeginFrame = NullBeginFrame;
        fn_table.m_Flip = NullFlip;
        fn_table.m_SetSwapInterval = NullSetSwapInterval;
        fn_table.m_Clear = NullClear;
        fn_table.m_NewVertexBuffer = NullNewVertexBuffer;
        fn_table.m_DeleteVertexBuffer = NullDeleteVertexBuffer;
        fn_table.m_SetVertexBufferData = NullSetVertexBufferData;
        fn_table.m_SetVertexBufferSubData = NullSetVertexBufferSubData;
        fn_table.m_MapVertexBuffer = NullMapVertexBuffer;
        fn_table.m_UnmapVertexBuffer = NullUnmapVertexBuffer;
        fn_table.m_GetMaxElementsVertices = NullGetMaxElementsVertices;
        fn_table.m_NewIndexBuffer = NullNewIndexBuffer;
        fn_table.m_DeleteIndexBuffer = NullDeleteIndexBuffer;
        fn_table.m_SetIndexBufferData = NullSetIndexBufferData;
        fn_table.m_SetIndexBufferSubData = NullSetIndexBufferSubData;
        fn_table.m_MapIndexBuffer = NullMapIndexBuffer;
        fn_table.m_UnmapIndexBuffer = NullUnmapIndexBuffer;
        fn_table.m_IsIndexBufferFormatSupported = NullIsIndexBufferFormatSupported;
        fn_table.m_NewVertexDeclaration = NullNewVertexDeclaration;
        fn_table.m_NewVertexDeclarationStride = NullNewVertexDeclarationStride;
        fn_table.m_SetStreamOffset = NullSetStreamOffset;
        fn_table.m_DeleteVertexDeclaration = NullDeleteVertexDeclaration;
        fn_table.m_EnableVertexDeclaration = NullEnableVertexDeclaration;
        fn_table.m_EnableVertexDeclarationProgram = NullEnableVertexDeclarationProgram;
        fn_table.m_DisableVertexDeclaration = NullDisableVertexDeclaration;
        fn_table.m_HashVertexDeclaration = NullHashVertexDeclaration;
        fn_table.m_DrawElements = NullDrawElements;
        fn_table.m_Draw = NullDraw;
        fn_table.m_NewVertexProgram = NullNewVertexProgram;
        fn_table.m_NewFragmentProgram = NullNewFragmentProgram;
        fn_table.m_NewProgram = NullNewProgram;
        fn_table.m_DeleteProgram = NullDeleteProgram;
        fn_table.m_ReloadVertexProgram = NullReloadVertexProgram;
        fn_table.m_ReloadFragmentProgram = NullReloadFragmentProgram;
        fn_table.m_DeleteVertexProgram = NullDeleteVertexProgram;
        fn_table.m_DeleteFragmentProgram = NullDeleteFragmentProgram;
        fn_table.m_GetShaderProgramLanguage = NullGetShaderProgramLanguage;
        fn_table.m_EnableProgram = NullEnableProgram;
        fn_table.m_DisableProgram = NullDisableProgram;
        fn_table.m_ReloadProgram = NullReloadProgram;
        fn_table.m_GetUniformName = NullGetUniformName;
        fn_table.m_GetUniformCount = NullGetUniformCount;
        fn_table.m_GetUniformLocation = NullGetUniformLocation;
        fn_table.m_SetConstantV4 = NullSetConstantV4;
        fn_table.m_SetConstantM4 = NullSetConstantM4;
        fn_table.m_SetSampler = NullSetSampler;
        fn_table.m_SetViewport = NullSetViewport;
        fn_table.m_EnableState = NullEnableState;
        fn_table.m_DisableState = NullDisableState;
        fn_table.m_SetBlendFunc = NullSetBlendFunc;
        fn_table.m_SetColorMask = NullSetColorMask;
        fn_table.m_SetDepthMask = NullSetDepthMask;
        fn_table.m_SetDepthFunc = NullSetDepthFunc;
        fn_table.m_SetScissor = NullSetScissor;
        fn_table.m_SetStencilMask = NullSetStencilMask;
        fn_table.m_SetStencilFunc = NullSetStencilFunc;
        fn_table.m_SetStencilOp = NullSetStencilOp;
        fn_table.m_SetCullFace = NullSetCullFace;
        fn_table.m_SetPolygonOffset = NullSetPolygonOffset;
        fn_table.m_NewRenderTarget = NullNewRenderTarget;
        fn_table.m_DeleteRenderTarget = NullDeleteRenderTarget;
        fn_table.m_SetRenderTarget = NullSetRenderTarget;
        fn_table.m_GetRenderTargetTexture = NullGetRenderTargetTexture;
        fn_table.m_GetRenderTargetSize = NullGetRenderTargetSize;
        fn_table.m_SetRenderTargetSize = NullSetRenderTargetSize;
        fn_table.m_IsTextureFormatSupported = NullIsTextureFormatSupported;
        fn_table.m_NewTexture = NullNewTexture;
        fn_table.m_DeleteTexture = NullDeleteTexture;
        fn_table.m_SetTexture = NullSetTexture;
        fn_table.m_SetTextureAsync = NullSetTextureAsync;
        fn_table.m_SetTextureParams = NullSetTextureParams;
        fn_table.m_GetTextureResourceSize = NullGetTextureResourceSize;
        fn_table.m_GetTextureWidth = NullGetTextureWidth;
        fn_table.m_GetTextureHeight = NullGetTextureHeight;
        fn_table.m_GetOriginalTextureWidth = NullGetOriginalTextureWidth;
        fn_table.m_GetOriginalTextureHeight = NullGetOriginalTextureHeight;
        fn_table.m_EnableTexture = NullEnableTexture;
        fn_table.m_DisableTexture = NullDisableTexture;
        fn_table.m_GetMaxTextureSize = NullGetMaxTextureSize;
        fn_table.m_GetTextureStatusFlags = NullGetTextureStatusFlags;
        fn_table.m_ReadPixels = NullReadPixels;
        fn_table.m_RunApplicationLoop = NullRunApplicationLoop;
        fn_table.m_GetTextureHandle = NullGetTextureHandle;
        fn_table.m_GetMaxElementsIndices = NullGetMaxElementsIndices;
        fn_table.m_IsMultiTargetRenderingSupported = NullIsMultiTargetRenderingSupported;
        fn_table.m_GetPipelineState = NullGetPipelineState;
        fn_table.m_SetStencilFuncSeparate = NullSetStencilFuncSeparate;
        fn_table.m_SetStencilOpSeparate = NullSetStencilOpSeparate;
        fn_table.m_SetFaceWinding = NullSetFaceWinding;

        return fn_table;
    }
}

