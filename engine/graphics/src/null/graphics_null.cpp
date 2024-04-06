// Copyright 2020-2024 The Defold Foundation
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
#include <dlib/thread.h>
#include <dlib/hash.h>

#include <platform/platform_window.h>

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

    static GraphicsAdapterFunctionTable NullRegisterFunctionTable();
    static bool                         NullIsSupported();
    static const int8_t    g_null_adapter_priority = 2;
    static GraphicsAdapter g_null_adapter(ADAPTER_FAMILY_NULL);
    static NullContext*    g_NullContext = 0x0;

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterNull, &g_null_adapter, NullIsSupported, NullRegisterFunctionTable, g_null_adapter_priority);

    static bool NullInitialize(HContext context);
    static void PostDeleteTextures(NullContext* context, bool force_delete);

    static void NullFinalize()
    {
        // nop
    }

    NullContext::NullContext(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_Width                   = params.m_Width;
        m_Height                  = params.m_Height;
        m_Window                  = params.m_Window;
        m_PrintDeviceInfo         = params.m_PrintDeviceInfo;
        m_JobThread               = params.m_JobThread;

        // We need to have some sort of valid default filtering
        if (m_DefaultTextureMinFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR;
        if (m_DefaultTextureMagFilter == TEXTURE_FILTER_DEFAULT)
            m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;

        for (int i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            m_Samplers[i].m_MinFilter = m_DefaultTextureMinFilter;
            m_Samplers[i].m_MagFilter = m_DefaultTextureMagFilter;
        }

        assert(dmPlatform::GetWindowStateParam(m_Window, dmPlatform::WINDOW_STATE_OPENED));

        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;

        m_ShaderClassLanguage[(int) ShaderDesc::SHADER_CLASS_GRAPHICS] = ShaderDesc::LANGUAGE_GLSL_SM140;
        m_ShaderClassLanguage[(int) ShaderDesc::SHADER_CLASS_COMPUTE]  = ShaderDesc::LANGUAGE_GLSL_SM430;
    }

    static HContext NullNewContext(const ContextParams& params)
    {
        if (!g_NullContext)
        {
            g_NullContext = new NullContext(params);

            if (NullInitialize(g_NullContext)){
                return g_NullContext;
            }

            DeleteContext(g_NullContext);
        }
        return 0x0;
    }

    static bool NullIsSupported()
    {
        return true;
    }

    static void NullDeleteContext(HContext _context)
    {
        assert(_context);
        if (g_NullContext)
        {
            NullContext* context = (NullContext*) _context;

            if (context->m_AssetContainerMutex)
            {
                dmMutex::Delete(context->m_AssetContainerMutex);
            }

            ResetSetTextureAsyncState(context->m_SetTextureAsyncState);
            delete (NullContext*) context;
            g_NullContext = 0x0;
        }
    }

    static bool NullInitialize(HContext _context)
    {
        assert(_context);

        NullContext* context = (NullContext*) _context;
        uint32_t buffer_size = 4 * dmPlatform::GetWindowWidth(context->m_Window) * dmPlatform::GetWindowHeight(context->m_Window);

        context->m_MainFrameBuffer.m_ColorBuffer[0]     = new char[buffer_size];
        context->m_MainFrameBuffer.m_ColorBufferSize[0] = buffer_size;
        context->m_MainFrameBuffer.m_DepthBuffer        = new char[buffer_size];
        context->m_MainFrameBuffer.m_StencilBuffer      = new char[buffer_size];
        context->m_MainFrameBuffer.m_DepthBufferSize    = buffer_size;
        context->m_MainFrameBuffer.m_StencilBufferSize  = buffer_size;
        context->m_CurrentFrameBuffer                   = &context->m_MainFrameBuffer;
        context->m_Program                              = 0x0;
        context->m_PipelineState                        = GetDefaultPipelineState();
        context->m_AsyncProcessingSupport               = context->m_JobThread && dmThread::PlatformHasThreadSupport();

        context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_MULTI_TARGET_RENDERING;
        context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_TEXTURE_ARRAY;
        context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_COMPUTE_SHADER;

        if (context->m_AsyncProcessingSupport)
        {
            context->m_AssetContainerMutex = dmMutex::New();
            InitializeSetTextureAsyncState(context->m_SetTextureAsyncState);
        }

        if (context->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: null");
        }
        return true;
    }

    static void NullCloseWindow(HContext _context)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;

        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            PostDeleteTextures(context, true);
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer[0];
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_Width = 0;
            context->m_Height = 0;
            dmPlatform::CloseWindow(context->m_Window);
        }
    }

    static void NullRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    static dmPlatform::HWindow NullGetWindow(HContext _context)
    {
        NullContext* context = (NullContext*) _context;
        return context->m_Window;
    }

    static uint32_t NullGetDisplayDpi(HContext context)
    {
        assert(context);
        return 0;
    }

    static uint32_t NullGetWidth(HContext _context)
    {
        NullContext* context = (NullContext*) _context;
        return context->m_Width;
    }

    static uint32_t NullGetHeight(HContext _context)
    {
        NullContext* context = (NullContext*) _context;
        return context->m_Height;
    }

    static void NullSetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            FrameBuffer& main = context->m_MainFrameBuffer;
            delete [] (char*)main.m_ColorBuffer[0];
            delete [] (char*)main.m_DepthBuffer;
            delete [] (char*)main.m_StencilBuffer;
            context->m_Width = width;
            context->m_Height = height;
            uint32_t buffer_size = 4 * width * height;
            main.m_ColorBuffer[0] = new char[buffer_size];
            main.m_ColorBufferSize[0] = buffer_size;
            main.m_DepthBuffer = new char[buffer_size];
            main.m_DepthBufferSize = buffer_size;
            main.m_StencilBuffer = new char[buffer_size];
            main.m_StencilBufferSize = buffer_size;

            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void NullResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_Window, width, height);
        }
    }

    static void NullGetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        NullContext* context = (NullContext*) _context;
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    static void NullClear(HContext _context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(_context);

        NullContext* context = (NullContext*) _context;

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
                uint32_t color = (red << 24) | (green << 16) | (blue << 8) | alpha;
                uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_ColorBuffer[i];
                uint32_t count = context->m_CurrentFrameBuffer->m_ColorBufferSize[i] / sizeof(uint32_t);
                for (uint32_t i = 0; i < count; ++i)
                    buffer[i] = color;
            }
        }

        if (flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT)
        {
            if (context->m_CurrentFrameBuffer->m_DepthTextureBuffer)
            {
                float* buffer = (float*)context->m_CurrentFrameBuffer->m_DepthTextureBuffer;
                uint32_t count = context->m_CurrentFrameBuffer->m_DepthTextureBufferSize / sizeof(uint32_t);
                for (uint32_t i = 0; i < count; ++i)
                    buffer[i] = depth;
            }
            else
            {
                float* buffer = (float*)context->m_CurrentFrameBuffer->m_DepthBuffer;
                uint32_t count = context->m_CurrentFrameBuffer->m_DepthBufferSize / sizeof(uint32_t);
                for (uint32_t i = 0; i < count; ++i)
                    buffer[i] = depth;
            }
        }

        if (flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            if (context->m_CurrentFrameBuffer->m_StencilTextureBuffer)
            {
                uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_StencilTextureBuffer;
                uint32_t count = context->m_CurrentFrameBuffer->m_StencilTextureBufferSize / sizeof(uint32_t);
                for (uint32_t i = 0; i < count; ++i)
                    buffer[i] = stencil;
            }
            else
            {
                uint32_t* buffer = (uint32_t*)context->m_CurrentFrameBuffer->m_StencilBuffer;
                uint32_t count = context->m_CurrentFrameBuffer->m_StencilBufferSize / sizeof(uint32_t);
                for (uint32_t i = 0; i < count; ++i)
                    buffer[i] = stencil;
            }
        }
    }

    static void NullBeginFrame(HContext context)
    {
        // NOP
    }

    static void NullFlip(HContext _context)
    {
        NullContext* context = (NullContext*) _context;
        PostDeleteTextures(context, false);

        // Mimick glfw
        if (context->m_RequestWindowClose)
        {
            if (dmPlatform::TriggerCloseCallback(context->m_Window))
            {
                CloseWindow(context);
            }
        }

        g_Flipped = 1;
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

    static bool NullIsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return true;
    }

    static uint32_t NullGetMaxElementsIndices(HContext context)
    {
        return 65536;
    }

    static HVertexDeclaration NullNewVertexDeclarationStride(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        return NewVertexDeclaration(context, stream_declaration);
    }

    static HVertexDeclaration NullNewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
    {
        VertexDeclaration* vd = new VertexDeclaration;
        memset(vd, 0, sizeof(VertexDeclaration));
        if (stream_declaration)
        {
            for (uint32_t i=0; i<stream_declaration->m_StreamCount; i++)
            {
                VertexStream& stream = stream_declaration->m_Streams[i];
                vd->m_Streams[i].m_NameHash  = stream.m_NameHash;
                vd->m_Streams[i].m_Location  = -1;
                vd->m_Streams[i].m_Size      = stream.m_Size;
                vd->m_Streams[i].m_Type      = stream.m_Type;
                vd->m_Streams[i].m_Normalize = stream.m_Normalize;
                vd->m_Streams[i].m_Offset    = vd->m_Stride;

                vd->m_Stride += stream.m_Size * GetTypeSize(stream.m_Type);
            }
            vd->m_StreamCount = stream_declaration->m_StreamCount;
        }
        return vd;
    }

    static void EnableVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        VertexStreamBuffer& s = ((NullContext*) context)->m_VertexStreams[stream];
        assert(s.m_Source == 0x0);
        assert(s.m_Buffer == 0x0);
        s.m_Source = vertex_buffer;
        s.m_Size   = size * TYPE_SIZE[type - dmGraphics::TYPE_BYTE];
        s.m_Stride = stride;
    }

    static void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);
        VertexStreamBuffer& s = ((NullContext*) context)->m_VertexStreams[stream];
        s.m_Size = 0;
        if (s.m_Buffer != 0x0)
        {
            delete [] (char*)s.m_Buffer;
            s.m_Buffer = 0x0;
        }
        s.m_Source = 0x0;
    }

    static void NullEnableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer, uint32_t binding_index)
    {
        NullContext* context = (NullContext*) _context;
        context->m_VertexBuffer = vertex_buffer;
    }

    static void NullDisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
        NullContext* context = (NullContext*) _context;
        context->m_VertexBuffer = 0;
    }

    void EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index)
    {
        assert(_context);
        assert(vertex_declaration);

        NullContext* context = (NullContext*) _context;

        VertexBuffer* vb = (VertexBuffer*) context->m_VertexBuffer;
        assert(vb);

        uint16_t stride = 0;

        for (uint32_t i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            stride += vertex_declaration->m_Streams[i].m_Size * TYPE_SIZE[vertex_declaration->m_Streams[i].m_Type - dmGraphics::TYPE_BYTE];
        }

        uint32_t offset = 0;
        for (uint16_t i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            VertexDeclaration::Stream& stream = vertex_declaration->m_Streams[i];
            if (stream.m_Size > 0)
            {
                stream.m_Location = i;
                EnableVertexStream(context, i, stream.m_Size, stream.m_Type, stride, &vb->m_Buffer[offset]);
                offset += stream.m_Size * TYPE_SIZE[stream.m_Type - dmGraphics::TYPE_BYTE];
            }
        }
    }

    static void NullEnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, HProgram program)
    {
        EnableVertexDeclaration(context, vertex_declaration, binding_index);
    }

    static void NullDisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
        assert(vertex_declaration);
        for (uint32_t i = 0; i < vertex_declaration->m_StreamCount; ++i)
            if (vertex_declaration->m_Streams[i].m_Size > 0)
                DisableVertexStream(context, i);
    }

    static uint32_t GetIndex(Type type, HIndexBuffer ib, uint32_t index)
    {
        const void* index_buffer = ((IndexBuffer*) ib)->m_Buffer;

        if (type == dmGraphics::TYPE_UNSIGNED_SHORT)
        {
            return ((unsigned short*)index_buffer)[index/2];
        }
        else if (type == dmGraphics::TYPE_UNSIGNED_INT)
        {
            return ((unsigned int*)index_buffer)[index/4];
        }

        assert(0);
        return ~0;
    }

    static void NullDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(_context);
        assert(index_buffer);
        NullContext* context = (NullContext*) _context;
        for (uint32_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
        {
            VertexStreamBuffer& vs = context->m_VertexStreams[i];
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
                VertexStreamBuffer& vs = context->m_VertexStreams[j];
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
    void ResetDrawCount()
    {
        g_DrawCount = 0;
    }

    uint64_t GetDrawCount()
    {
        return g_DrawCount;
    }

    static void ProgramShaderResourceCallback(dmGraphics::GLSLUniformParserBindingType binding_type, const char* name, uint32_t name_length, dmGraphics::Type type, uint32_t size, uintptr_t userdata);

    struct ShaderBinding
    {
        ShaderBinding() : m_Name(0) {};
        char*    m_Name;
        uint32_t m_Index;
        uint32_t m_Size;
        uint32_t m_Stride;
        Type     m_Type;
    };

    struct ShaderProgram
    {
        char*                  m_Data;
        ShaderDesc::Language   m_Language;
        dmArray<ShaderBinding> m_Uniforms;
    };

    static void PushBinding(dmArray<ShaderBinding>* binding_array, const char* name, uint32_t name_length, dmGraphics::Type type, uint32_t size)
    {
        if(binding_array->Full())
        {
            binding_array->OffsetCapacity(8);
        }

        ShaderBinding binding;
        name_length++;
        binding.m_Name   = new char[name_length];
        binding.m_Index  = binding_array->Size();
        binding.m_Type   = type;
        binding.m_Size   = size;
        binding.m_Stride = GetTypeSize(type);

        dmStrlCpy(binding.m_Name, name, name_length);
        binding_array->Push(binding);
    }

    struct Program
    {
        Program(ShaderProgram* vp, ShaderProgram* fp)
        {
            m_Uniforms.SetCapacity(16);
            m_Compute = 0;
            m_VP      = vp;
            m_FP      = fp;
            if (m_VP != 0x0)
            {
                GLSLAttributeParse(m_VP->m_Language, m_VP->m_Data, ProgramShaderResourceCallback, (uintptr_t)this);
                GLSLUniformParse(m_VP->m_Language, m_VP->m_Data, ProgramShaderResourceCallback, (uintptr_t)this);
                TransferUniforms(m_VP);
                m_Language = m_VP->m_Language;
            }
            if (m_FP != 0x0)
            {
                GLSLUniformParse(m_FP->m_Language, m_FP->m_Data, ProgramShaderResourceCallback, (uintptr_t)this);
                TransferUniforms(m_FP);
                m_Language = m_FP->m_Language;
            }
        }

        Program(ShaderProgram* compute)
        {
            m_Uniforms.SetCapacity(16);
            m_VP      = 0;
            m_FP      = 0;
            m_Compute = compute;

            if (m_Compute != 0x0)
            {
                GLSLUniformParse(m_Compute->m_Language, m_Compute->m_Data, ProgramShaderResourceCallback, (uintptr_t) this);
                TransferUniforms(m_Compute);
                m_Language = compute->m_Language;
            }
        }

        void TransferUniforms(ShaderProgram* p)
        {
            for (int i = 0; i < p->m_Uniforms.Size(); ++i)
            {
                PushBinding(&m_Uniforms, p->m_Uniforms[i].m_Name, strlen(p->m_Uniforms[i].m_Name), p->m_Uniforms[i].m_Type, p->m_Uniforms[i].m_Size);
            }
        }

        ~Program()
        {
            for(uint32_t i = 0; i < m_Uniforms.Size(); ++i)
                delete[] m_Uniforms[i].m_Name;
            for(uint32_t i = 0; i < m_Attributes.Size(); ++i)
                delete[] m_Attributes[i].m_Name;
        }

        ShaderDesc::Language   m_Language;

        ShaderProgram*         m_VP;
        ShaderProgram*         m_FP;
        ShaderProgram*         m_Compute;

        dmArray<ShaderBinding> m_Uniforms;
        dmArray<ShaderBinding> m_Attributes;
    };

    static void ProgramShaderResourceCallback(dmGraphics::GLSLUniformParserBindingType binding_type, const char* name, uint32_t name_length, dmGraphics::Type type, uint32_t size, uintptr_t userdata)
    {
        Program* program = (Program*) userdata;

        dmArray<ShaderBinding>* binding_array = binding_type == GLSLUniformParserBindingType::UNIFORM ?
            &program->m_Uniforms : &program->m_Attributes;

        PushBinding(binding_array, name, name_length, type, size);
    }

    static ShaderProgram* NewShaderProgramFromDDF(ShaderDesc::Shader* ddf)
    {
        assert(ddf);
        ShaderProgram* p = new ShaderProgram();
        p->m_Data = new char[ddf->m_Source.m_Count+1];
        memcpy(p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        p->m_Data[ddf->m_Source.m_Count] = '\0';
        p->m_Language = ddf->m_Language;

        for (int i = 0; i < ddf->m_UniformBuffers.m_Count; ++i)
        {
            ShaderDesc::ResourceBinding& res = ddf->m_UniformBuffers[i];

            ShaderDesc::ResourceTypeInfo& type = ddf->m_Types[res.m_Type.m_Type.m_TypeIndex];

            for (int j = 0; j < type.m_Members.m_Count; ++j)
            {
                PushBinding(&p->m_Uniforms, type.m_Members[j].m_Name, strlen(type.m_Members[j].m_Name), ShaderDataTypeToGraphicsType(type.m_Members[j].m_Type.m_Type.m_ShaderType), 1);
            }
        }

        for (int i = 0; i < ddf->m_StorageBuffers.m_Count; ++i)
        {
            ShaderDesc::ResourceBinding& res = ddf->m_StorageBuffers[i];
            PushBinding(&p->m_Uniforms, res.m_Name, strlen(res.m_Name), ShaderDataTypeToGraphicsType(res.m_Type.m_Type.m_ShaderType), 1);
        }

        for (int i = 0; i < ddf->m_Textures.m_Count; ++i)
        {
            ShaderDesc::ResourceBinding& res = ddf->m_Textures[i];
            PushBinding(&p->m_Uniforms, res.m_Name, strlen(res.m_Name), ShaderDataTypeToGraphicsType(res.m_Type.m_Type.m_ShaderType), 1);
        }

        return p;
    }

    static HComputeProgram NullNewComputeProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return (HComputeProgram) NewShaderProgramFromDDF(ddf);
    }

    static HProgram NullNewProgramFromCompute(HContext context, HComputeProgram compute_program)
    {
        return (HProgram) new Program((ShaderProgram*) compute_program);
    }

    static void NullDeleteComputeProgram(HComputeProgram prog)
    {
        ShaderProgram* p = (ShaderProgram*) prog;
        delete [] (char*)p->m_Data;
        for(uint32_t i = 0; i < p->m_Uniforms.Size(); ++i)
            delete[] p->m_Uniforms[i].m_Name;
        delete p;
    }

    static HProgram NullNewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        ShaderProgram* vertex   = 0x0;
        ShaderProgram* fragment = 0x0;
        if (vertex_program != INVALID_VERTEX_PROGRAM_HANDLE)
        {
            vertex = (ShaderProgram*) vertex_program;
        }
        if (fragment_program != INVALID_FRAGMENT_PROGRAM_HANDLE)
        {
            fragment = (ShaderProgram*) fragment_program;
        }
        return (HProgram) new Program(vertex, fragment);
    }

    static void NullDeleteProgram(HContext context, HProgram program)
    {
        delete (Program*) program;
    }

    static HVertexProgram NullNewVertexProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return (HVertexProgram) NewShaderProgramFromDDF(ddf);
    }

    static HFragmentProgram NullNewFragmentProgram(HContext context, ShaderDesc::Shader* ddf)
    {
        return (HFragmentProgram) NewShaderProgramFromDDF(ddf);
    }

    static bool NullReloadVertexProgram(HVertexProgram prog, ShaderDesc::Shader* ddf)
    {
        assert(prog);
        assert(ddf);
        ShaderProgram* p = (ShaderProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[ddf->m_Source.m_Count];
        memcpy((char*)p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        return !g_ForceVertexReloadFail;
    }

    static bool NullReloadFragmentProgram(HFragmentProgram prog, ShaderDesc::Shader* ddf)
    {
        assert(prog);
        assert(ddf);
        ShaderProgram* p = (ShaderProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[ddf->m_Source.m_Count];
        memcpy((char*)p->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        return !g_ForceFragmentReloadFail;
    }

    static void NullDeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        ShaderProgram* p = (ShaderProgram*)program;
        delete [] (char*)p->m_Data;
        for(uint32_t i = 0; i < p->m_Uniforms.Size(); ++i)
            delete[] p->m_Uniforms[i].m_Name;
        delete p;
    }

    static void NullDeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        ShaderProgram* p = (ShaderProgram*)program;
        delete [] (char*)p->m_Data;
        for(uint32_t i = 0; i < p->m_Uniforms.Size(); ++i)
            delete[] p->m_Uniforms[i].m_Name;
        delete p;
    }

    void SetOverrideShaderLanguage(HContext context, ShaderDesc::ShaderClass shader_class, ShaderDesc::Language language)
    {
        ((NullContext*) context)->m_ShaderClassLanguage[(int) shader_class] = language;
    }

    static ShaderDesc::Language NullGetProgramLanguage(HProgram program)
    {
        return ((ShaderProgram*) program)->m_Language;
    }

    static ShaderDesc::Language NullGetShaderProgramLanguage(HContext context, ShaderDesc::ShaderClass shader_class)
    {
#if defined(DM_PLATFORM_VENDOR)
        #if defined(DM_GRAPHICS_NULL_SHADER_LANGUAGE)
            return ShaderDesc:: DM_GRAPHICS_NULL_SHADER_LANGUAGE ;
        #else
            #error "You must define the platform default shader language using DM_GRAPHICS_NULL_SHADER_LANGUAGE"
        #endif
#else
        return ((NullContext*) context)->m_ShaderClassLanguage[(int) shader_class];
#endif
    }

    static void NullEnableProgram(HContext context, HProgram program)
    {
        assert(context);
        ((NullContext*) context)->m_Program = (void*)program;
    }

    static void NullDisableProgram(HContext context)
    {
        assert(context);
        ((NullContext*) context)->m_Program = 0x0;
    }

    static bool NullReloadProgramGraphics(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        (void) context;
        (void) program;

        return true;
    }

    static bool NullReloadProgramCompute(HContext context, HProgram program, HComputeProgram compute_program)
    {
        (void) context;
        (void) program;
        return true;
    }

    static bool NullReloadComputeProgram(HComputeProgram prog, ShaderDesc::Shader* ddf)
    {
        (void)prog;
        return true;
    }

    static uint32_t NullGetAttributeCount(HProgram prog)
    {
        return ((Program*) prog)->m_Attributes.Size();
    }

    static uint32_t GetElementCount(Type type)
    {
        switch(type)
        {
            case TYPE_BYTE:
            case TYPE_UNSIGNED_BYTE:
            case TYPE_SHORT:
            case TYPE_UNSIGNED_SHORT:
            case TYPE_INT:
            case TYPE_UNSIGNED_INT:
            case TYPE_FLOAT:
                return 1;
                break;
            case TYPE_FLOAT_VEC4:       return 4;
            case TYPE_FLOAT_MAT4:       return 16;
            case TYPE_SAMPLER_2D:       return 1;
            case TYPE_SAMPLER_CUBE:     return 1;
            case TYPE_SAMPLER_2D_ARRAY: return 1;
            case TYPE_IMAGE_2D:         return 1;
            case TYPE_FLOAT_VEC2:       return 2;
            case TYPE_FLOAT_VEC3:       return 3;
            case TYPE_FLOAT_MAT2:       return 4;
            case TYPE_FLOAT_MAT3:       return 9;
        }
        assert(0);
        return 0;
    }

    static void NullGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        Program* program       = (Program*) prog;
        ShaderBinding& binding = program->m_Attributes[index];
        *name_hash             = dmHashString64(binding.m_Name);
        *type                  = binding.m_Type;
        *element_count         = GetElementCount(binding.m_Type);
        *num_values            = binding.m_Size;
        *location              = binding.m_Index;
    }

    static uint32_t NullGetUniformCount(HProgram prog)
    {
        return ((Program*)prog)->m_Uniforms.Size();
    }

    static uint32_t NullGetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        Program* program = (Program*)prog;
        assert(index < program->m_Uniforms.Size());
        ShaderBinding& uniform = program->m_Uniforms[index];
        *buffer = '\0';
        dmStrlCat(buffer, uniform.m_Name, buffer_size);
        *type = uniform.m_Type;
        *size = uniform.m_Size;
        return (uint32_t)strlen(buffer);
    }

    static HUniformLocation NullGetUniformLocation(HProgram prog, const char* name)
    {
        Program* program = (Program*)prog;
        uint32_t count = program->m_Uniforms.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            ShaderBinding& uniform = program->m_Uniforms[i];
            if (dmStrCaseCmp(uniform.m_Name, name)==0)
            {
                return (int32_t)uniform.m_Index;
            }
        }
        return INVALID_UNIFORM_LOCATION;
    }

    static void NullSetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
    }

    // Tests Only
    const Vector4& GetConstantV4Ptr(HContext _context, HUniformLocation base_location)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        assert(context->m_Program != 0x0);
        return context->m_ProgramRegisters[base_location];
    }

    static void NullSetConstantV4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_location], data, sizeof(Vector4) * count);
    }

    static void NullSetConstantM4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        assert(context->m_Program != 0x0);
        memcpy(&context->m_ProgramRegisters[base_location], data, sizeof(Vector4) * 4 * count);
    }

    static void NullSetSampler(HContext context, HUniformLocation location, int32_t unit)
    {
    }

    static inline uint32_t GetBufferSize(const TextureParams& params)
    {
        uint32_t bytes_per_pixel = dmGraphics::GetTextureFormatBitsPerPixel(params.m_Format) / 3;
        return sizeof(uint32_t) * params.m_Width * params.m_Height * bytes_per_pixel;
    }

    static HRenderTarget NullNewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        NullContext* context      = (NullContext*) _context;
        RenderTarget* rt          = new RenderTarget();

        BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        void** color_buffers[MAX_BUFFER_COLOR_ATTACHMENTS] = {
            &rt->m_FrameBuffer.m_ColorBuffer[0],
            &rt->m_FrameBuffer.m_ColorBuffer[1],
            &rt->m_FrameBuffer.m_ColorBuffer[2],
            &rt->m_FrameBuffer.m_ColorBuffer[3]
        };

        uint32_t* color_buffer_sizes[MAX_BUFFER_COLOR_ATTACHMENTS] = {
            &rt->m_FrameBuffer.m_ColorBufferSize[0],
            &rt->m_FrameBuffer.m_ColorBufferSize[1],
            &rt->m_FrameBuffer.m_ColorBufferSize[2],
            &rt->m_FrameBuffer.m_ColorBufferSize[3]
        };

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (buffer_type_flags & color_buffer_flags[i])
            {
                rt->m_ColorTextureParams[i] = params.m_ColorBufferParams[i];
                ClearTextureParamsData(rt->m_ColorTextureParams[i]);
                uint32_t buffer_size                    = GetBufferSize(params.m_ColorBufferParams[i]);
                rt->m_ColorTextureParams[i].m_DataSize  = buffer_size;
                rt->m_ColorBufferTexture[i]             = NewTexture(context, params.m_ColorBufferCreationParams[i]);
                Texture* attachment_tex                 = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, rt->m_ColorBufferTexture[i]);
                SetTexture(rt->m_ColorBufferTexture[i], rt->m_ColorTextureParams[i]);
                *(color_buffer_sizes[i]) = buffer_size;
                *(color_buffers[i])      = attachment_tex->m_Data;
            }
        }

        if (buffer_type_flags & BUFFER_TYPE_DEPTH_BIT)
        {
            rt->m_DepthBufferParams = params.m_DepthBufferParams;
            ClearTextureParamsData(rt->m_DepthBufferParams);
            if (params.m_DepthTexture)
            {
                uint32_t buffer_size               = sizeof(float) * params.m_DepthBufferParams.m_Width * params.m_DepthBufferParams.m_Height;
                rt->m_DepthBufferParams.m_DataSize = buffer_size;
                rt->m_DepthBufferTexture           = NewTexture(context, params.m_DepthBufferCreationParams);
                Texture* attachment_tex            = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, rt->m_DepthBufferTexture);
                SetTexture(rt->m_DepthBufferTexture, rt->m_DepthBufferParams);

                rt->m_FrameBuffer.m_DepthTextureBufferSize = buffer_size;
                rt->m_FrameBuffer.m_DepthTextureBuffer     = attachment_tex->m_Data;
            }
            else
            {
                rt->m_FrameBuffer.m_DepthBuffer = new char[GetBufferSize(params.m_DepthBufferParams)];
            }
        }
        if (buffer_type_flags & BUFFER_TYPE_STENCIL_BIT)
        {
            rt->m_StencilBufferParams = params.m_StencilBufferParams;
            ClearTextureParamsData(rt->m_StencilBufferParams);
            if (params.m_StencilTexture)
            {
                uint32_t buffer_size                 = sizeof(float) * params.m_StencilBufferParams.m_Width * params.m_StencilBufferParams.m_Height;
                rt->m_StencilBufferParams.m_DataSize = buffer_size;
                rt->m_StencilBufferTexture           = NewTexture(context, params.m_StencilBufferCreationParams);
                Texture* attachment_tex              = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, rt->m_StencilBufferTexture);
                SetTexture(rt->m_StencilBufferTexture, rt->m_StencilBufferParams);

                rt->m_FrameBuffer.m_StencilTextureBufferSize = buffer_size;
                rt->m_FrameBuffer.m_StencilTextureBuffer     = attachment_tex->m_Data;
            }
            else
            {
                rt->m_FrameBuffer.m_StencilBuffer = new char[GetBufferSize(params.m_StencilBufferParams)];
            }
        }

        return StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
    }

    static void NullDeleteRenderTarget(HRenderTarget render_target)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_NullContext->m_AssetHandleContainer, render_target);

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_ColorBufferTexture[i])
            {
                DeleteTexture(rt->m_ColorBufferTexture[i]);
            }
        }
        if (rt->m_DepthBufferTexture)
        {
            DeleteTexture(rt->m_DepthBufferTexture);
        }
        if (rt->m_StencilBufferTexture)
        {
            DeleteTexture(rt->m_StencilBufferTexture);
        }
        delete [] (char*)rt->m_FrameBuffer.m_DepthBuffer;
        delete [] (char*)rt->m_FrameBuffer.m_StencilBuffer;
        delete rt;

        g_NullContext->m_AssetHandleContainer.Release(render_target);
    }

    static void NullSetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        assert(_context);
        NullContext* context = (NullContext*) _context;

        if (render_target == 0)
        {
            context->m_CurrentFrameBuffer = &context->m_MainFrameBuffer;
        }
        else
        {
            assert(GetAssetType(render_target) == dmGraphics::ASSET_TYPE_RENDER_TARGET);
            RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);
            context->m_CurrentFrameBuffer = &rt->m_FrameBuffer;
        }
    }

    static HTexture NullGetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_NullContext->m_AssetHandleContainer, render_target);

        if (IsColorBufferType(buffer_type))
        {
            return rt->m_ColorBufferTexture[GetBufferTypeIndex(buffer_type)];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT)
        {
            return rt->m_DepthBufferTexture;
        }
        else if (buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            return rt->m_StencilBufferTexture;
        }
        return 0;
    }

    static void NullGetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        RenderTarget* rt      = GetAssetFromContainer<RenderTarget>(g_NullContext->m_AssetHandleContainer, render_target);
        TextureParams* params = 0;

        if (IsColorBufferType(buffer_type))
        {
            uint32_t i = GetBufferTypeIndex(buffer_type);
            assert(i < MAX_BUFFER_COLOR_ATTACHMENTS);
            params = &rt->m_ColorTextureParams[i];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT)
        {
            params = &rt->m_DepthBufferParams;
        }
        else if (buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            params = &rt->m_StencilBufferParams;
        }
        else
        {
            assert(0);
        }

        width  = params->m_Width;
        height = params->m_Height;
    }

    static void NullSetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_NullContext->m_AssetHandleContainer, render_target);

        uint32_t buffer_size = sizeof(uint32_t) * width * height;
        void** color_buffers[MAX_BUFFER_COLOR_ATTACHMENTS] = {
            &rt->m_FrameBuffer.m_ColorBuffer[0],
            &rt->m_FrameBuffer.m_ColorBuffer[1],
            &rt->m_FrameBuffer.m_ColorBuffer[2],
            &rt->m_FrameBuffer.m_ColorBuffer[3],
        };

        uint32_t* color_buffer_sizes[MAX_BUFFER_COLOR_ATTACHMENTS] = {
            &rt->m_FrameBuffer.m_ColorBufferSize[0],
            &rt->m_FrameBuffer.m_ColorBufferSize[1],
            &rt->m_FrameBuffer.m_ColorBufferSize[2],
            &rt->m_FrameBuffer.m_ColorBufferSize[3],
        };

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (color_buffers[i])
            {
                *(color_buffer_sizes[i])             = buffer_size;
                rt->m_ColorTextureParams[i].m_Width  = width;
                rt->m_ColorTextureParams[i].m_Height = height;

                if (rt->m_ColorBufferTexture[i])
                {
                    rt->m_ColorTextureParams[i].m_DataSize = buffer_size;
                    SetTexture(rt->m_ColorBufferTexture[i], rt->m_ColorTextureParams[i]);
                    Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, rt->m_ColorBufferTexture[i]);
                    *(color_buffers[i]) = tex->m_Data;
                }
            }
        }

        if (rt->m_DepthBufferTexture)
        {
            rt->m_FrameBuffer.m_DepthTextureBufferSize = buffer_size;
            rt->m_DepthBufferParams.m_Width            = width;
            rt->m_DepthBufferParams.m_Height           = height;
            rt->m_DepthBufferParams.m_DataSize         = buffer_size;
            SetTexture(rt->m_DepthBufferTexture, rt->m_DepthBufferParams);
            Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, rt->m_DepthBufferTexture);
            rt->m_FrameBuffer.m_DepthTextureBuffer = tex->m_Data;
        }
        else if (rt->m_FrameBuffer.m_DepthBuffer)
        {
            rt->m_FrameBuffer.m_DepthTextureBufferSize = buffer_size;
            rt->m_DepthBufferParams.m_Width            = width;
            rt->m_DepthBufferParams.m_Height           = height;
            rt->m_DepthBufferParams.m_DataSize         = buffer_size;
            delete [] (char*) rt->m_FrameBuffer.m_DepthBuffer;
            rt->m_FrameBuffer.m_DepthBuffer = new char[buffer_size];
        }

        if (rt->m_StencilBufferTexture)
        {
            rt->m_FrameBuffer.m_StencilTextureBufferSize = buffer_size;
            rt->m_StencilBufferParams.m_Width            = width;
            rt->m_StencilBufferParams.m_Height           = height;
            rt->m_StencilBufferParams.m_DataSize         = buffer_size;
            SetTexture(rt->m_StencilBufferTexture, rt->m_StencilBufferParams);
            Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, rt->m_StencilBufferTexture);
            rt->m_FrameBuffer.m_StencilTextureBuffer = tex->m_Data;
        }
        else if (rt->m_FrameBuffer.m_StencilBuffer)
        {
            rt->m_FrameBuffer.m_StencilTextureBufferSize = buffer_size;
            rt->m_StencilBufferParams.m_Width            = width;
            rt->m_StencilBufferParams.m_Height           = height;
            rt->m_StencilBufferParams.m_DataSize         = buffer_size;
            delete [] (char*) rt->m_FrameBuffer.m_StencilBuffer;
            rt->m_FrameBuffer.m_StencilBuffer = new char[buffer_size];
        }
    }

    static bool NullIsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (((NullContext*) context)->m_TextureFormatSupport & (1 << format)) != 0;
    }

    static uint32_t NullGetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    static HTexture NullNewTexture(HContext _context, const TextureCreationParams& params)
    {
        NullContext* context  = (NullContext*) _context;
        Texture* tex          = new Texture();

        uint16_t num_texture_ids = 1;
        TextureType texture_type = params.m_Type;

        if (params.m_Type == TEXTURE_TYPE_2D_ARRAY && !IsContextFeatureSupported(_context, CONTEXT_FEATURE_TEXTURE_ARRAY))
        {
            num_texture_ids = params.m_Depth;
            texture_type    = TEXTURE_TYPE_2D;
        }

        tex->m_Type          = texture_type;
        tex->m_Width         = params.m_Width;
        tex->m_Height        = params.m_Height;
        tex->m_Depth         = params.m_Depth;
        tex->m_MipMapCount   = 0;
        tex->m_Data          = 0;
        tex->m_DataState     = 0;
        tex->m_NumTextureIds = num_texture_ids;
        tex->m_LastBoundUnit = new int32_t[num_texture_ids];

        for (int i = 0; i < num_texture_ids; ++i)
        {
            tex->m_LastBoundUnit[i] = -1;
        }

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

        ASSET_CONTAINER_LOCK_BEGIN(context);

        HTexture handle = StoreAssetInContainer(context->m_AssetHandleContainer, tex, ASSET_TYPE_TEXTURE);

        ASSET_CONTAINER_LOCK_END(context);

        return handle;
    }

    static int DoDeleteTexture(void* _context, void* _h_texture)
    {
        NullContext* context = (NullContext*) _context;
        ASSET_CONTAINER_LOCK_BEGIN(context);
        HTexture texture = (HTexture) _h_texture;

        Texture* tex = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, texture);
        if (tex)
        {
            if (tex->m_Data != 0x0)
            {
                delete [] (char*)tex->m_Data;
            }

            delete [] tex->m_LastBoundUnit;
            delete tex;
        }
        ASSET_CONTAINER_LOCK_END(context);
        return 0;
    }

    static void DoDeleteTextureComplete(void* _context, void* _h_texture, int result)
    {
        NullContext* context = (NullContext*) _context;
        ASSET_CONTAINER_LOCK_BEGIN(context);
        HTexture texture = (HTexture) _h_texture;
        context->m_AssetHandleContainer.Release(texture);
        ASSET_CONTAINER_LOCK_END(context);
    }

    static void NullDeleteTextureAsync(NullContext* context, HTexture texture)
    {
        ASSET_CONTAINER_LOCK_BEGIN(context);
        dmJobThread::PushJob(context->m_JobThread, DoDeleteTexture, DoDeleteTextureComplete, context, (void*) texture);
        ASSET_CONTAINER_LOCK_END(context);
    }

    static void PostDeleteTextures(NullContext* context, bool force_delete)
    {
        if (force_delete)
        {
            uint32_t size = context->m_SetTextureAsyncState.m_PostDeleteTextures.Size();
            for (uint32_t i = 0; i < size; ++i)
            {
                void* texture = (void*) (size_t) context->m_SetTextureAsyncState.m_PostDeleteTextures[i];
                DoDeleteTexture(context, texture);
                DoDeleteTextureComplete(context, texture, 0);
            }
            context->m_SetTextureAsyncState.m_PostDeleteTextures.SetSize(0);
            return;
        }

        uint32_t i = 0;
        while(i < context->m_SetTextureAsyncState.m_PostDeleteTextures.Size())
        {
            HTexture texture = context->m_SetTextureAsyncState.m_PostDeleteTextures[i];
            if(!(dmGraphics::GetTextureStatusFlags(texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING))
            {
                NullDeleteTextureAsync(context, texture);
                context->m_SetTextureAsyncState.m_PostDeleteTextures.EraseSwap(i);
            }
            else
            {
                ++i;
            }
        }
    }

    static void NullDeleteTexture(HTexture texture)
    {
        if (g_NullContext->m_AsyncProcessingSupport && g_NullContext->m_UseAsyncTextureLoad)
        {
            // If they're not uploaded yet, we cannot delete them
            if(dmGraphics::GetTextureStatusFlags(texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
            {
                PushSetTextureAsyncDeleteTexture(g_NullContext->m_SetTextureAsyncState, texture);
            }
            else
            {
                NullDeleteTextureAsync(g_NullContext, texture);
            }
        }
        else
        {
            void* htexture = (void*) texture;
            DoDeleteTexture(g_NullContext, htexture);
            DoDeleteTextureComplete(g_NullContext, htexture, 0);
        }
    }

    static HandleResult NullGetTextureHandle(HTexture texture, void** out_handle)
    {
        *out_handle = 0x0;
        Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);

        if (!tex)
        {
            return HANDLE_RESULT_ERROR;
        }

        *out_handle = tex->m_Data;

        return HANDLE_RESULT_OK;
    }

    static void NullSetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        assert(texture);
        g_NullContext->m_Samplers[g_NullContext->m_TextureUnit].m_MinFilter = minfilter == TEXTURE_FILTER_DEFAULT ? g_NullContext->m_DefaultTextureMinFilter : minfilter;
        g_NullContext->m_Samplers[g_NullContext->m_TextureUnit].m_MagFilter = magfilter == TEXTURE_FILTER_DEFAULT ? g_NullContext->m_DefaultTextureMagFilter : magfilter;
    }

    static void NullSetTexture(HTexture texture, const TextureParams& params)
    {
        ASSET_CONTAINER_LOCK_BEGIN(g_NullContext);
        Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);
        assert(tex);
        assert(!params.m_SubUpdate || (params.m_X + params.m_Width <= tex->m_Width));
        assert(!params.m_SubUpdate || (params.m_Y + params.m_Height <= tex->m_Height));

        if (tex->m_Data != 0x0)
        {
            delete [] (char*)tex->m_Data;
        }

        tex->m_Format = params.m_Format;
        // Allocate even for 0x0 size so that the rendertarget dummies will work.
        tex->m_Data = new char[params.m_DataSize];
        if (params.m_Data != 0x0)
        {
            memcpy(tex->m_Data, params.m_Data, params.m_DataSize);
        }

        // The width/height of the texture can change from this function as well
        if (!params.m_SubUpdate && params.m_MipMap == 0)
        {
            tex->m_Width  = params.m_Width;
            tex->m_Height = params.m_Height;
        }

        tex->m_Depth               = dmMath::Max((uint16_t) 1, params.m_Depth);
        tex->m_MipMapCount         = dmMath::Max(tex->m_MipMapCount, (uint8_t) (params.m_MipMap+1));
        tex->m_MipMapCount         = dmMath::Clamp(tex->m_MipMapCount, (uint8_t) 0, GetMipmapCount(dmMath::Max(tex->m_Width, tex->m_Height)));
        tex->m_Sampler.m_MinFilter = params.m_MinFilter;
        tex->m_Sampler.m_MagFilter = params.m_MagFilter;
        tex->m_Sampler.m_UWrap     = params.m_UWrap;
        tex->m_Sampler.m_VWrap     = params.m_VWrap;

        ASSET_CONTAINER_LOCK_END(g_NullContext);
    }

    static uint32_t NullGetTextureResourceSize(HTexture texture)
    {
        Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);
        if (!tex)
        {
            return 0;
        }

        uint32_t size_total = 0;
        uint32_t size = tex->m_Width * tex->m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(tex->m_Format)/8);
        for(uint32_t i = 0; i < tex->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        if (tex->m_Type == TEXTURE_TYPE_CUBE_MAP)
        {
            size_total *= 6;
        }
        return size_total + sizeof(Texture);
    }

    static uint16_t NullGetTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Width;
    }

    static uint16_t NullGetTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Height;
    }

    static uint16_t NullGetOriginalTextureWidth(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_OriginalWidth;
    }

    static uint16_t NullGetOriginalTextureHeight(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_OriginalHeight;
    }

    static void NullEnableTexture(HContext _context, uint32_t unit, uint8_t id_index, HTexture texture)
    {
        assert(_context);
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        NullContext* context = (NullContext*) _context;
        Texture* tex = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, texture);
        assert(tex->m_Data);
        context->m_Textures[unit] = texture;
        context->m_TextureUnit = unit;
        NullSetTextureParams(texture, tex->m_Sampler.m_MinFilter, tex->m_Sampler.m_MagFilter, tex->m_Sampler.m_UWrap, tex->m_Sampler.m_VWrap, tex->m_Sampler.m_Anisotropy);

        tex->m_LastBoundUnit[id_index] = unit;
    }

    static void NullDisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        ((NullContext*) context)->m_Textures[unit] = 0;
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
        SetPipelineStateValue(((NullContext*) context)->m_PipelineState, state, 1);
    }

    static void NullDisableState(HContext context, State state)
    {
        assert(context);
        SetPipelineStateValue(((NullContext*) context)->m_PipelineState, state, 0);
    }

    static void NullSetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
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
        ((NullContext*) context)->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void NullSetDepthMask(HContext context, bool mask)
    {
        assert(context);
        ((NullContext*) context)->m_PipelineState.m_WriteDepth = mask;
    }

    static void NullSetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        ((NullContext*) context)->m_PipelineState.m_DepthTestFunc = func;
    }

    static void NullSetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        context->m_ScissorRect[0] = (int32_t) x;
        context->m_ScissorRect[1] = (int32_t) y;
        context->m_ScissorRect[2] = (int32_t) x+width;
        context->m_ScissorRect[3] = (int32_t) y+height;
    }

    static void NullSetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        ((NullContext*) context)->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void NullSetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void NullSetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        context->m_PipelineState.m_StencilBackOpFail       = sfail;
        context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void NullSetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        NullContext* context = (NullContext*) _context;

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

    static void NullSetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        NullContext* context = (NullContext*) _context;

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
        ((NullContext*) context)->m_PipelineState.m_FaceWinding = face_winding;
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
        return ((NullContext*) context)->m_PipelineState;
    }

    // Called on worker thread
    static int AsyncProcessCallback(void* _context, void* data)
    {
        NullContext* context       = (NullContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        {
            ASSET_CONTAINER_LOCK_BEGIN(context);
            Texture* tex = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, ap.m_Texture);
            if (tex)
            {
                SetTexture(ap.m_Texture, ap.m_Params);
                tex->m_DataState &= ~(1<<ap.m_Params.m_MipMap);
            }
            ASSET_CONTAINER_LOCK_END(context);
        }

        return 0;
    }

    // Called on thread where we update (which should be the main thread)
    static void AsyncCompleteCallback(void* _context, void* data, int result)
    {
        NullContext* context       = (NullContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        if (ap.m_Callback)
        {
            ap.m_Callback(ap.m_Texture, ap.m_UserData);
        }

        ReturnSetTextureAsyncIndex(context->m_SetTextureAsyncState, param_array_index);
    }

    static void NullSetTextureAsync(HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        if (g_NullContext->m_AsyncProcessingSupport && g_NullContext->m_UseAsyncTextureLoad)
        {
            ASSET_CONTAINER_LOCK_BEGIN(g_NullContext);

            Texture* tex               = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);
            tex->m_DataState          |= 1 << params.m_MipMap;
            uint16_t param_array_index = PushSetTextureAsyncState(g_NullContext->m_SetTextureAsyncState, texture, params, callback, user_data);

            dmJobThread::PushJob(g_NullContext->m_JobThread,
                AsyncProcessCallback,
                AsyncCompleteCallback,
                (void*) g_NullContext,
                (void*) (uintptr_t) param_array_index);

            ASSET_CONTAINER_LOCK_END(g_NullContext);
        }
        else
        {
            SetTexture(texture, params);
        }
    }

    static uint32_t NullGetTextureStatusFlags(HTexture texture)
    {
        ASSET_CONTAINER_LOCK_BEGIN(g_NullContext);
        Texture* tex   = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);
        uint32_t flags = TEXTURE_STATUS_OK;
        if(tex && tex->m_DataState)
        {
            flags |= TEXTURE_STATUS_DATA_PENDING;
        }
        ASSET_CONTAINER_LOCK_END(g_NullContext);
        return flags;
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

    static bool NullIsExtensionSupported(HContext context, const char* extension)
    {
        return true;
    }

    static TextureType NullGetTextureType(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Type;
    }

    static uint32_t NullGetNumSupportedExtensions(HContext context)
    {
        return 0;
    }

    static const char* NullGetSupportedExtension(HContext context, uint32_t index)
    {
        return "";
    }

    static uint8_t NullGetNumTextureHandles(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_NumTextureIds;
    }

    static bool NullIsContextFeatureSupported(HContext _context, ContextFeature feature)
    {
        NullContext* context = (NullContext*) _context;
        return (context->m_ContextFeatures & (1 << feature)) != 0;
    }

    static uint16_t NullGetTextureDepth(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Depth;
    }

    static uint8_t NullGetTextureMipmapCount(HTexture texture)
    {
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_MipMapCount;
    }

    static bool NullIsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
    {
        assert(_context);
        if (asset_handle == 0)
        {
            return false;
        }
        NullContext* context = (NullContext*) _context;
        AssetType type       = GetAssetType(asset_handle);
        if (type == ASSET_TYPE_TEXTURE)
        {
            return GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            return GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    bool UnmapIndexBuffer(HContext context, HIndexBuffer buffer)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        memcpy(ib->m_Buffer, ib->m_Copy, ib->m_Size);
        delete [] ib->m_Copy;
        ib->m_Copy = 0x0;
        return true;
    }

    void* MapVertexBuffer(HContext context, HVertexBuffer buffer, BufferAccess access)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        vb->m_Copy = new char[vb->m_Size];
        memcpy(vb->m_Copy, vb->m_Buffer, vb->m_Size);
        return vb->m_Copy;
    }

    bool UnmapVertexBuffer(HContext context, HVertexBuffer buffer)
    {
        VertexBuffer* vb = (VertexBuffer*)buffer;
        memcpy(vb->m_Buffer, vb->m_Copy, vb->m_Size);
        delete [] vb->m_Copy;
        vb->m_Copy = 0x0;
        return true;
    }

    void* MapIndexBuffer(HContext context, HIndexBuffer buffer, BufferAccess access)
    {
        IndexBuffer* ib = (IndexBuffer*)buffer;
        ib->m_Copy = new char[ib->m_Size];
        memcpy(ib->m_Copy, ib->m_Buffer, ib->m_Size);
        return ib->m_Copy;
    }

    void GetTextureFilters(HContext context, uint32_t unit, TextureFilter& min_filter, TextureFilter& mag_filter)
    {
        min_filter = ((NullContext*) context)->m_Samplers[unit].m_MinFilter;
        mag_filter = ((NullContext*) context)->m_Samplers[unit].m_MagFilter;
    }

    static GraphicsAdapterFunctionTable NullRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, Null);
        return fn_table;
    }
}

