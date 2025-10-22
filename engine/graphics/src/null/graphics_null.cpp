// Copyright 2020-2025 The Defold Foundation
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
    static HContext                     NullGetContext();

    static GraphicsAdapter g_null_adapter(ADAPTER_FAMILY_NULL);
    static NullContext*    g_NullContext = 0x0;

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterNull, &g_null_adapter, NullIsSupported, NullRegisterFunctionTable, NullGetContext, ADAPTER_FAMILY_PRIORITY_NULL);

    static bool NullInitialize(HContext context, const ContextParams& params);
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
    }

    static HContext NullNewContext(const ContextParams& params)
    {
        if (!g_NullContext)
        {
            g_NullContext = new NullContext(params);

            if (NullInitialize(g_NullContext, params))
            {
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

    static HContext NullGetContext()
    {
        return g_NullContext;
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

    static bool NullInitialize(HContext _context, const ContextParams& params)
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
        context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_INSTANCING;
        context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_3D_TEXTURES;

        if (context->m_AsyncProcessingSupport)
        {
            context->m_AssetContainerMutex = dmMutex::New();
            InitializeSetTextureAsyncState(context->m_SetTextureAsyncState);
        }

        if (context->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: null");
        }

        SetSwapInterval(context, params.m_SwapInterval);

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

    static uint32_t NullGetVertexBufferSize(HVertexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        VertexBuffer* buffer_ptr = (VertexBuffer*) buffer;
        return buffer_ptr->m_Size;
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

    static uint32_t NullGetIndexBufferSize(HIndexBuffer buffer)
    {
        if (!buffer)
        {
            return 0;
        }
        IndexBuffer* buffer_ptr = (IndexBuffer*) buffer;
        return buffer_ptr->m_Size;
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
            vd->m_StepFunction = stream_declaration->m_StepFunction;
        }
        return vd;
    }

    static void EnableVertexStream(HContext context, uint32_t binding_index, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
        VertexStreamBuffer& s = ((NullContext*) context)->m_VertexStreams[binding_index][stream];
        assert(s.m_Source == 0x0);
        assert(s.m_Buffer == 0x0);
        s.m_Source = vertex_buffer;
        s.m_Size   = size * TYPE_SIZE[type - dmGraphics::TYPE_BYTE];
        s.m_Stride = stride;
    }

    static void DisableVertexStream(HContext context, uint32_t binding_index, uint16_t stream)
    {
        assert(context);
        VertexStreamBuffer& s = ((NullContext*) context)->m_VertexStreams[binding_index][stream];
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
        context->m_VertexBuffers[binding_index] = vertex_buffer;
    }

    static void NullDisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
        NullContext* context = (NullContext*) _context;

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_VertexBuffers[i] == vertex_buffer)
            {
                context->m_VertexBuffers[i] = 0x0;
                break;
            }
        }
    }

    void EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index)
    {
        assert(_context);
        assert(vertex_declaration);

        NullContext* context = (NullContext*) _context;

        VertexBuffer* vb = (VertexBuffer*) context->m_VertexBuffers[binding_index];
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
                EnableVertexStream(context, binding_index, i, stream.m_Size, stream.m_Type, stride, &vb->m_Buffer[offset]);
                offset += stream.m_Size * TYPE_SIZE[stream.m_Type - dmGraphics::TYPE_BYTE];
            }
        }

        context->m_VertexDeclarations[binding_index] = vertex_declaration;
    }

    static void NullEnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        EnableVertexDeclaration(context, vertex_declaration, binding_index);
    }

    static void NullDisableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration)
    {
        NullContext* context = (NullContext*) _context;
        assert(context);
        assert(vertex_declaration);

        int32_t binding_index = -1;

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_VertexDeclarations[i] == vertex_declaration)
            {
                binding_index = i;
                break;
            }
        }

        if (binding_index == -1)
            return;

        for (uint32_t i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            if (vertex_declaration->m_Streams[i].m_Size > 0)
            {
                DisableVertexStream(context, binding_index, i);
            }
        }
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

    static void NullDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
    {
        assert(_context);
        assert(index_buffer);
        NullContext* context = (NullContext*) _context;

        uint32_t binding_index = 0;

        for (uint32_t i = 0; i < MAX_VERTEX_STREAM_COUNT; ++i)
        {
            VertexStreamBuffer& vs = context->m_VertexStreams[binding_index][i];
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
                VertexStreamBuffer& vs = context->m_VertexStreams[binding_index][j];
                if (vs.m_Size > 0)
                {
                    memcpy(&((char*)vs.m_Buffer)[i * vs.m_Size], &((char*)vs.m_Source)[index * vs.m_Stride], vs.m_Size);
                }
            }
        }

        if (g_Flipped)
        {
            g_Flipped = 0;
            g_DrawCount = 0;
        }
        g_DrawCount++;
    }

    static void NullDraw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        assert(context);

        if (g_Flipped)
        {
            g_Flipped = 0;
            g_DrawCount = 0;
        }
        g_DrawCount++;
    }

    static void NullDispatchCompute(HContext _context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        // Not supported
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

    static void CreateProgramResourceBindings(NullProgram* program, NullShaderModule* vertex_module, NullShaderModule* fragment_module, NullShaderModule* compute_module)
    {
        ResourceBindingDesc bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};

        uint32_t ubo_alignment = UNIFORM_BUFFERS_ALIGNMENT;
        uint32_t ssbo_alignment = 0;

        ProgramResourceBindingsInfo binding_info = {};
        FillProgramResourceBindings(&program->m_BaseProgram, bindings, ubo_alignment, ssbo_alignment, binding_info);

        program->m_BaseProgram.m_MaxSet     = binding_info.m_MaxSet;
        program->m_BaseProgram.m_MaxBinding = binding_info.m_MaxBinding;
        program->m_UniformDataSize          = binding_info.m_UniformDataSize;
        program->m_UniformData              = new uint8_t[binding_info.m_UniformDataSize];
        memset(program->m_UniformData, 0, binding_info.m_UniformDataSize);

        BuildUniforms(&program->m_BaseProgram);
    }

    static NullShaderModule* NewShaderModuleFromDDF(HContext context, ShaderDesc::Shader* ddf)
    {
        NullShaderModule* shader = new NullShaderModule();
        shader->m_Data           = new char[ddf->m_Source.m_Count+1];
        shader->m_Language       = ddf->m_Language;

        memcpy(shader->m_Data, ddf->m_Source.m_Data, ddf->m_Source.m_Count);
        shader->m_Data[ddf->m_Source.m_Count] = '\0';
        return shader;
    }

    static HProgram NullNewProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        NullProgram* p = new NullProgram();

        if (ddf_cp)
        {
            p->m_Compute = NewShaderModuleFromDDF(_context, ddf_cp);
        }
        else
        {
            p->m_VP = NewShaderModuleFromDDF(_context, ddf_vp);
            p->m_FP = NewShaderModuleFromDDF(_context, ddf_fp);
        }

        CreateShaderMeta(&ddf->m_Reflection, &p->m_BaseProgram.m_ShaderMeta);
        CreateProgramResourceBindings(p, p->m_VP, p->m_FP, p->m_Compute);

        return (HProgram) p;
    }

    static void DeleteShader(NullShaderModule* shader)
    {
        if (!shader)
            return;
        delete [] (char*) shader->m_Data;
        //DestroyShaderMeta(shader->m_ShaderMeta);
        delete shader;
    }

    static void NullDeleteProgram(HContext context, HProgram _program)
    {
        NullProgram* program = (NullProgram*) _program;

        DeleteShader(program->m_VP);
        DeleteShader(program->m_FP);
        DeleteShader(program->m_Compute);

        delete[] program->m_UniformData;
        delete program;
    }

    static bool NullReloadProgram(HContext _context, HProgram program, ShaderDesc* ddf)
    {
        NullProgram* p = (NullProgram*) program;

        if (g_ForceVertexReloadFail || g_ForceFragmentReloadFail)
            return false;

        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        if (ddf_cp)
        {
            delete [] (char*) p->m_Compute->m_Data;
            p->m_Compute->m_Data = new char[ddf_cp->m_Source.m_Count];
            memcpy((char*)p->m_Compute->m_Data, ddf_cp->m_Source.m_Data, ddf_cp->m_Source.m_Count);
        }
        else
        {
            // VP
            delete [] (char*) p->m_VP->m_Data;
            p->m_VP->m_Data = new char[ddf_vp->m_Source.m_Count];
            memcpy((char*)p->m_VP->m_Data, ddf_vp->m_Source.m_Data, ddf_vp->m_Source.m_Count);

            // FP
            delete [] (char*) p->m_FP->m_Data;
            p->m_FP->m_Data = new char[ddf_fp->m_Source.m_Count];
            memcpy((char*)p->m_FP->m_Data, ddf_fp->m_Source.m_Data, ddf_fp->m_Source.m_Count);
        }

        return true;
    }

    static ShaderDesc::Language NullGetProgramLanguage(HProgram program)
    {
        return ((NullShaderModule*) program)->m_Language;
    }

    static bool NullIsShaderLanguageSupported(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
    #if defined(DM_PLATFORM_VENDOR)
        #if defined(DM_GRAPHICS_NULL_SHADER_LANGUAGE)
            return language == ShaderDesc:: DM_GRAPHICS_NULL_SHADER_LANGUAGE ;
        #else
            #error "You must define the platform default shader language using DM_GRAPHICS_NULL_SHADER_LANGUAGE"
        #endif
    #else
        switch(shader_type)
        {
            case ShaderDesc::SHADER_TYPE_VERTEX:
                return language == ShaderDesc::LANGUAGE_GLSL_SM330 ||
                       language == ShaderDesc::LANGUAGE_GLES_SM300 ||
                       language == ShaderDesc::LANGUAGE_SPIRV;
            case ShaderDesc::SHADER_TYPE_FRAGMENT:
                return language == ShaderDesc::LANGUAGE_GLSL_SM330 ||
                       language == ShaderDesc::LANGUAGE_GLES_SM300 ||
                       language == ShaderDesc::LANGUAGE_SPIRV;
            case ShaderDesc::SHADER_TYPE_COMPUTE:
                return language == ShaderDesc::LANGUAGE_GLSL_SM430 ||
                       language == ShaderDesc::LANGUAGE_SPIRV;
        }
    #endif
        return false;
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

    static uint32_t NullGetAttributeCount(HProgram prog)
    {
        NullProgram* program_ptr = (NullProgram*) prog;

        uint32_t num_vx_inputs = 0;
        for (int i = 0; i < program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); ++i)
        {
            if (program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                num_vx_inputs++;
            }
        }
        return num_vx_inputs;
    }

    static void NullGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        NullProgram* program = (NullProgram*) prog;

        uint32_t input_ix = 0;
        for (int i = 0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); ++i)
        {
            if (program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
            {
                if (input_ix == index)
                {
                    ShaderResourceBinding& attr = program->m_BaseProgram.m_ShaderMeta.m_Inputs[i];
                    *name_hash                  = attr.m_NameHash;
                    *type                       = ShaderDataTypeToGraphicsType(attr.m_Type.m_ShaderType);
                    *num_values                 = 1;
                    *location                   = attr.m_Binding;
                    *element_count              = GetShaderTypeSize(attr.m_Type.m_ShaderType) / sizeof(float);
                }
                input_ix++;
            }
        }
    }

    const Uniform* GetUniform(HProgram prog, dmhash_t name_hash)
    {
        NullProgram* program = (NullProgram*)prog;
        uint32_t count = program->m_BaseProgram.m_Uniforms.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Uniform& uniform = program->m_BaseProgram.m_Uniforms[i];
            if (uniform.m_NameHash == name_hash)
            {
                return &uniform;
            }
        }
        return 0x0;
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

        uint32_t set           = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding       = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        NullProgram* program            = (NullProgram*) context->m_Program;
        ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[set][binding];
        uint32_t offset                 = pgm_res.m_DataOffset + buffer_offset;

        Vector4* ptr = (Vector4*) (program->m_UniformData + offset);
        return *ptr;
    }

    static inline void WriteConstantData(NullProgram* program, uint32_t offset, uint8_t* data_ptr, uint32_t data_size)
    {
        assert(offset + data_size <= program->m_UniformDataSize);
        memcpy(program->m_UniformData + offset, data_ptr, data_size);
    }

    static void NullSetConstantV4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        assert(context->m_Program != 0x0);

        uint32_t set           = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding       = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        NullProgram* program            = (NullProgram*) context->m_Program;
        ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[set][binding];
        uint32_t offset                 = pgm_res.m_DataOffset + buffer_offset;

        WriteConstantData(program, offset, (uint8_t*) data, sizeof(dmVMath::Vector4) * count);
    }

    static void NullSetConstantM4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
    {
        assert(_context);
        NullContext* context = (NullContext*) _context;
        assert(context->m_Program != 0x0);

        uint32_t set           = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding       = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        NullProgram* program            = (NullProgram*) context->m_Program;
        ProgramResourceBinding& pgm_res = program->m_BaseProgram.m_ResourceBindings[set][binding];
        uint32_t offset                 = pgm_res.m_DataOffset + buffer_offset;

        WriteConstantData(program, offset, (uint8_t*) data, sizeof(dmVMath::Vector4) * 4 * count);
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

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (buffer_type_flags & color_buffer_flags[i])
            {
                rt->m_ColorTextureParams[i] = params.m_ColorBufferParams[i];
                ClearTextureParamsData(rt->m_ColorTextureParams[i]);
                uint32_t buffer_size                    = GetBufferSize(params.m_ColorBufferParams[i]);
                rt->m_ColorTextureParams[i].m_DataSize  = buffer_size;
                rt->m_ColorBufferTexture[i]             = NewTexture(context, params.m_ColorBufferCreationParams[i]);
                Texture* attachment_tex = 0x0;
                {
                    attachment_tex                 = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, rt->m_ColorBufferTexture[i]);
                }
                SetTexture(_context, rt->m_ColorBufferTexture[i], rt->m_ColorTextureParams[i]);
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
                Texture* attachment_tex = 0x0;
                {
                    attachment_tex            = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, rt->m_DepthBufferTexture);
                }
                SetTexture(_context, rt->m_DepthBufferTexture, rt->m_DepthBufferParams);

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
                Texture* attachment_tex = 0x0;
                {
                    attachment_tex              = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, rt->m_StencilBufferTexture);
                }
                SetTexture(_context, rt->m_StencilBufferTexture, rt->m_StencilBufferParams);

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

    static void NullDeleteRenderTarget(HContext context, HRenderTarget render_target)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        RenderTarget* rt = GetAssetFromContainer<RenderTarget>(g_NullContext->m_AssetHandleContainer, render_target);

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_ColorBufferTexture[i])
            {
                DeleteTexture(context, rt->m_ColorBufferTexture[i]);
            }
        }
        if (rt->m_DepthBufferTexture)
        {
            DeleteTexture(context, rt->m_DepthBufferTexture);
        }
        if (rt->m_StencilBufferTexture)
        {
            DeleteTexture(context, rt->m_StencilBufferTexture);
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
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
            RenderTarget* rt = GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, render_target);
            context->m_CurrentFrameBuffer = &rt->m_FrameBuffer;
        }
    }

    static HTexture NullGetRenderTargetTexture(HContext context, HRenderTarget render_target, BufferType buffer_type)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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

    static void NullGetRenderTargetSize(HContext context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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

    static void NullSetRenderTargetSize(HContext context, HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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
                    SetTexture((HContext)g_NullContext, rt->m_ColorBufferTexture[i], rt->m_ColorTextureParams[i]);
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
            SetTexture((HContext)g_NullContext, rt->m_DepthBufferTexture, rt->m_DepthBufferParams);
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
            SetTexture((HContext)g_NullContext, rt->m_StencilBufferTexture, rt->m_StencilBufferParams);
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

        uint16_t num_texture_ids = 1;
        TextureType texture_type = params.m_Type;

        if (params.m_Type == TEXTURE_TYPE_2D_ARRAY && !IsContextFeatureSupported(_context, CONTEXT_FEATURE_TEXTURE_ARRAY))
        {
            num_texture_ids = params.m_Depth;
            texture_type    = TEXTURE_TYPE_2D;
        }
        else if (IsTextureType3D(params.m_Type) && !IsContextFeatureSupported(_context, CONTEXT_FEATURE_3D_TEXTURES))
        {
            return 0;
        }

        Texture* tex          = new Texture();
        tex->m_Type           = texture_type;
        tex->m_Width          = params.m_Width;
        tex->m_Height         = params.m_Height;
        tex->m_Depth          = params.m_Depth;
        tex->m_MipMapCount    = 0;
        tex->m_Data           = 0;
        tex->m_DataState      = 0;
        tex->m_NumTextureIds  = num_texture_ids;
        tex->m_LastBoundUnit  = new int32_t[num_texture_ids];
        tex->m_UsageHintFlags = params.m_UsageHintBits;
        tex->m_PageCount      = params.m_LayerCount;

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

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetContainerMutex);
        return StoreAssetInContainer(context->m_AssetHandleContainer, tex, ASSET_TYPE_TEXTURE);
    }

    static int DoDeleteTexture(dmJobThread::HContext, dmJobThread::HJob hjob, void* _context, void* _h_texture)
    {
        NullContext* context = (NullContext*) _context;
        HTexture texture = (HTexture) _h_texture;

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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
        return 0;
    }

    static void DoDeleteTextureComplete(dmJobThread::HContext, dmJobThread::HJob hjob, dmJobThread::JobStatus status, void* _context, void* _h_texture, int result)
    {
        NullContext* context = (NullContext*) _context;
        HTexture texture = (HTexture) _h_texture;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        context->m_AssetHandleContainer.Release(texture);
    }

    static void NullDeleteTextureAsync(NullContext* context, HTexture texture)
    {
        dmJobThread::Job job = {0};
        job.m_Process = DoDeleteTexture;
        job.m_Callback = DoDeleteTextureComplete;
        job.m_Context = context;
        job.m_Data = (void*)(uintptr_t)texture;

        dmJobThread::HJob hjob = dmJobThread::CreateJob(context->m_JobThread, &job);
        dmJobThread::PushJob(context->m_JobThread, hjob);
    }

    static void PostDeleteTextures(NullContext* context, bool force_delete)
    {
        if (force_delete)
        {
            uint32_t size = context->m_SetTextureAsyncState.m_PostDeleteTextures.Size();
            for (uint32_t i = 0; i < size; ++i)
            {
                void* texture = (void*) (size_t) context->m_SetTextureAsyncState.m_PostDeleteTextures[i];
                DoDeleteTexture(context->m_JobThread, 0, context, texture);
                DoDeleteTextureComplete(context->m_JobThread, 0, dmJobThread::JOB_STATUS_FINISHED, context, texture, 0);
            }
            context->m_SetTextureAsyncState.m_PostDeleteTextures.SetSize(0);
            return;
        }

        uint32_t i = 0;
        while(i < context->m_SetTextureAsyncState.m_PostDeleteTextures.Size())
        {
            HTexture texture = context->m_SetTextureAsyncState.m_PostDeleteTextures[i];
            if(!(dmGraphics::GetTextureStatusFlags(context, texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING))
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

    static void NullDeleteTexture(HContext _context, HTexture texture)
    {
        NullContext* context = (NullContext*)_context;
        if (g_NullContext->m_AsyncProcessingSupport && g_NullContext->m_UseAsyncTextureLoad)
        {
            // If they're not uploaded yet, we cannot delete them
            if(dmGraphics::GetTextureStatusFlags(g_NullContext, texture) & dmGraphics::TEXTURE_STATUS_DATA_PENDING)
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
            DoDeleteTexture(context->m_JobThread, 0, g_NullContext, htexture);
            DoDeleteTextureComplete(context->m_JobThread, 0, dmJobThread::JOB_STATUS_FINISHED, g_NullContext, htexture, 0);
        }
    }

    static HandleResult NullGetTextureHandle(HTexture texture, void** out_handle)
    {
        *out_handle = 0x0;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        Texture* tex = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);

        if (!tex)
        {
            return HANDLE_RESULT_ERROR;
        }

        *out_handle = tex->m_Data;

        return HANDLE_RESULT_OK;
    }

    static void NullSetTextureParams(HContext context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        assert(texture);
        g_NullContext->m_Samplers[g_NullContext->m_TextureUnit].m_MinFilter = minfilter == TEXTURE_FILTER_DEFAULT ? g_NullContext->m_DefaultTextureMinFilter : minfilter;
        g_NullContext->m_Samplers[g_NullContext->m_TextureUnit].m_MagFilter = magfilter == TEXTURE_FILTER_DEFAULT ? g_NullContext->m_DefaultTextureMagFilter : magfilter;
    }

    static void NullSetTexture(HContext context, HTexture texture, const TextureParams& params)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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
    }

    static uint32_t NullGetTextureResourceSize(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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

    static uint16_t NullGetTextureWidth(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Width;
    }

    static uint16_t NullGetTextureHeight(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Height;
    }

    static uint16_t NullGetOriginalTextureWidth(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_OriginalWidth;
    }

    static uint16_t NullGetOriginalTextureHeight(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_OriginalHeight;
    }

    static void NullEnableTexture(HContext _context, uint32_t unit, uint8_t id_index, HTexture texture)
    {
        assert(_context);
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        NullContext* context = (NullContext*) _context;
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        Texture* tex = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, texture);
        assert(tex->m_Data);
        context->m_Textures[unit] = texture;
        context->m_TextureUnit = unit;
        NullSetTextureParams(_context, texture, tex->m_Sampler.m_MinFilter, tex->m_Sampler.m_MagFilter, tex->m_Sampler.m_UWrap, tex->m_Sampler.m_VWrap, tex->m_Sampler.m_Anisotropy);

        tex->m_LastBoundUnit[id_index] = unit;
    }

    static void NullDisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        ((NullContext*) context)->m_Textures[unit] = 0;
    }

    static void NullReadPixels(HContext context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size)
    {
        assert (buffer_size >= width * height * 4);
        memset(buffer, 0, width * height * 4);
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

    static void NullSetDepthMask(HContext context, bool enable_mask)
    {
        assert(context);
        ((NullContext*) context)->m_PipelineState.m_WriteDepth = enable_mask;
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
    static int AsyncProcessCallback(dmJobThread::HContext, dmJobThread::HJob hjob, void* _context, void* data)
    {
        NullContext* context       = (NullContext*) _context;
        uint16_t param_array_index = (uint16_t) (size_t) data;
        SetTextureAsyncParams ap   = GetSetTextureAsyncParams(context->m_SetTextureAsyncState, param_array_index);

        DM_MUTEX_OPTIONAL_SCOPED_LOCK(context->m_AssetContainerMutex);
        Texture* tex = GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, ap.m_Texture);
        if (tex)
        {
            SetTexture(_context, ap.m_Texture, ap.m_Params);
            tex->m_DataState &= ~(1<<ap.m_Params.m_MipMap);
        }

        return 0;
    }

    // Called on thread where we update (which should be the main thread)
    static void AsyncCompleteCallback(dmJobThread::HContext, dmJobThread::HJob hjob, dmJobThread::JobStatus status, void* _context, void* data, int result)
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

    static void NullSetTextureAsync(HContext context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        if (g_NullContext->m_AsyncProcessingSupport && g_NullContext->m_UseAsyncTextureLoad)
        {
            {
                DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
                Texture* tex      = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);
                tex->m_DataState |= 1 << params.m_MipMap;
            }
            uint16_t param_array_index = PushSetTextureAsyncState(g_NullContext->m_SetTextureAsyncState, texture, params, callback, user_data);

            dmJobThread::Job job = {0};
            job.m_Process = AsyncProcessCallback;
            job.m_Callback = AsyncCompleteCallback;
            job.m_Context = (void*) g_NullContext;
            job.m_Data = (void*) (uintptr_t) param_array_index;

            dmJobThread::HJob hjob = dmJobThread::CreateJob(g_NullContext->m_JobThread, &job);
            dmJobThread::PushJob(g_NullContext->m_JobThread, hjob);
        }
        else
        {
            SetTexture(context, texture, params);
        }
    }

    static uint32_t NullGetTextureStatusFlags(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        Texture* tex   = GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture);
        uint32_t flags = TEXTURE_STATUS_OK;
        if(tex && tex->m_DataState)
        {
            flags |= TEXTURE_STATUS_DATA_PENDING;
        }
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

    static TextureType NullGetTextureType(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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

    static uint8_t NullGetNumTextureHandles(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_NumTextureIds;
    }

    static uint32_t NullGetTextureUsageHintFlags(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_UsageHintFlags;
    }

    static uint8_t NullGetTexturePageCount(HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_PageCount;
    }

    static bool NullIsContextFeatureSupported(HContext _context, ContextFeature feature)
    {
        NullContext* context = (NullContext*) _context;
        return (context->m_ContextFeatures & (1 << feature)) != 0;
    }

    static uint16_t NullGetTextureDepth(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
        return GetAssetFromContainer<Texture>(g_NullContext->m_AssetHandleContainer, texture)->m_Depth;
    }

    static uint8_t NullGetTextureMipmapCount(HContext context, HTexture texture)
    {
        DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
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
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
            return GetAssetFromContainer<Texture>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            DM_MUTEX_OPTIONAL_SCOPED_LOCK(g_NullContext->m_AssetContainerMutex);
            return GetAssetFromContainer<RenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    static void NullInvalidateGraphicsHandles(HContext context) { }

    static void NullGetViewport(HContext context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
    {
        assert(context);
        *x = 0, *y = 0, *width = 1000, *height = 1000;
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
