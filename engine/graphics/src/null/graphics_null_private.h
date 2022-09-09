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

#ifndef GRAPHICS_DEVICE_NULL
#define GRAPHICS_DEVICE_NULL

#include <dmsdk/dlib/vmath.h>

namespace dmGraphics
{
    struct Texture
    {
        void* m_Data;
        TextureFormat   m_Format;
        TextureType     m_Type;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_OriginalWidth;
        uint32_t m_OriginalHeight;
        uint16_t m_MipMapCount;
    };

    struct VertexStream
    {
        const void* m_Source;
        void* m_Buffer;
        uint16_t m_Size;
        uint16_t m_Stride;
    };

    const static uint32_t MAX_VERTEX_STREAM_COUNT = 8;
    const static uint32_t MAX_REGISTER_COUNT = 16;
    const static uint32_t MAX_TEXTURE_COUNT = 32;

    struct FrameBuffer
    {
        void*       m_ColorBuffer[MAX_BUFFER_COLOR_ATTACHMENTS];
        void*       m_DepthBuffer;
        void*       m_StencilBuffer;
        uint32_t    m_ColorBufferSize[MAX_BUFFER_COLOR_ATTACHMENTS];
        uint32_t    m_DepthBufferSize;
        uint32_t    m_StencilBufferSize;
    };

    struct VertexDeclaration
    {
        uint32_t        m_Count;
        VertexElement   m_Elements[MAX_VERTEX_STREAM_COUNT];
    };

    struct VertexBuffer
    {
        char* m_Buffer;
        char* m_Copy;
        uint32_t m_Size;
    };

    struct IndexBuffer
    {
        char* m_Buffer;
        char* m_Copy;
        uint32_t m_Size;
    };

    struct RenderTarget
    {
        TextureParams   m_BufferTextureParams[MAX_BUFFER_TYPE_COUNT];
        HTexture        m_ColorBufferTexture[MAX_BUFFER_COLOR_ATTACHMENTS];
        FrameBuffer     m_FrameBuffer;
    };

    struct Context
    {
        Context(const ContextParams& params);

        VertexStream                m_VertexStreams[MAX_VERTEX_STREAM_COUNT];
        dmVMath::Vector4            m_ProgramRegisters[MAX_REGISTER_COUNT];
        HTexture                    m_Textures[MAX_TEXTURE_COUNT];
        FrameBuffer                 m_MainFrameBuffer;
        FrameBuffer*                m_CurrentFrameBuffer;
        void*                       m_Program;
        WindowResizeCallback        m_WindowResizeCallback;
        void*                       m_WindowResizeCallbackUserData;
        WindowCloseCallback         m_WindowCloseCallback;
        void*                       m_WindowCloseCallbackUserData;
        WindowCloseCallback         m_WindowFocusCallback;
        void*                       m_WindowFocusCallbackUserData;
        PipelineState               m_PipelineState;
        TextureFilter               m_DefaultTextureMinFilter;
        TextureFilter               m_DefaultTextureMagFilter;
        uint32_t                    m_Width;
        uint32_t                    m_Height;
        uint32_t                    m_WindowWidth;
        uint32_t                    m_WindowHeight;
        uint32_t                    m_Dpi;
        int32_t                     m_ScissorRect[4];
        uint32_t                    m_TextureFormatSupport;
        uint32_t                    m_WindowOpened : 1;
        // Only use for testing
        uint32_t                    m_RequestWindowClose : 1;
    };
}

#endif // __GRAPHICS_DEVICE_NULL__
