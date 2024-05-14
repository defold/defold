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

#ifndef GRAPHICS_DEVICE_NULL
#define GRAPHICS_DEVICE_NULL

#include <dmsdk/dlib/vmath.h>
#include <dlib/opaque_handle_container.h>

#include "../graphics_private.h"


namespace dmGraphics
{
    const static uint32_t MAX_REGISTER_COUNT = 16;
    const static uint32_t MAX_TEXTURE_COUNT  = 32;

    struct TextureSampler
    {
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_UWrap;
        TextureWrap   m_VWrap;
        float         m_Anisotropy;
    };

    struct Texture
    {
        void*             m_Data;
        TextureFormat     m_Format;
        TextureType       m_Type;
        TextureSampler    m_Sampler;
        uint32_t          m_Width;
        uint32_t          m_Height;
        uint32_t          m_Depth;
        uint32_t          m_OriginalWidth;
        uint32_t          m_OriginalHeight;
        uint16_t          m_NumTextureIds;
        int32_t*          m_LastBoundUnit; // testing
        volatile uint16_t m_DataState; // data state per mip-map (mipX = bitX). 0=ok, 1=pending
        uint8_t           m_MipMapCount;
    };

    struct VertexStreamBuffer
    {
        const void* m_Source;
        void* m_Buffer;
        uint16_t m_Size;
        uint16_t m_Stride;
    };

    struct FrameBuffer
    {
        void*       m_ColorBuffer[MAX_BUFFER_COLOR_ATTACHMENTS];
        void*       m_DepthBuffer;
        void*       m_StencilBuffer;
        void*       m_DepthTextureBuffer;
        void*       m_StencilTextureBuffer;

        uint32_t    m_ColorBufferSize[MAX_BUFFER_COLOR_ATTACHMENTS];
        uint32_t    m_DepthBufferSize;
        uint32_t    m_StencilBufferSize;
        uint32_t    m_DepthTextureBufferSize;
        uint32_t    m_StencilTextureBufferSize;
    };

    struct VertexBuffer
    {
        char*    m_Buffer;
        char*    m_Copy;
        uint32_t m_Size;
    };

    struct IndexBuffer
    {
        char*    m_Buffer;
        char*    m_Copy;
        uint32_t m_Size;
    };

    struct RenderTarget
    {
        TextureParams   m_ColorTextureParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams   m_DepthBufferParams;
        TextureParams   m_StencilBufferParams;
        HTexture        m_ColorBufferTexture[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture        m_DepthBufferTexture;
        HTexture        m_StencilBufferTexture;
        FrameBuffer     m_FrameBuffer;
    };

    struct NullContext
    {
        NullContext(const ContextParams& params);

        dmJobThread::HContext              m_JobThread;
        dmMutex::HMutex                    m_AssetContainerMutex;

        dmPlatform::HWindow                m_Window;
        SetTextureAsyncState               m_SetTextureAsyncState;
        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;
        VertexStreamBuffer                 m_VertexStreams[MAX_VERTEX_STREAM_COUNT];
        dmVMath::Vector4                   m_ProgramRegisters[MAX_REGISTER_COUNT];
        TextureSampler                     m_Samplers[MAX_TEXTURE_COUNT];
        HTexture                           m_Textures[MAX_TEXTURE_COUNT];
        HVertexBuffer                      m_VertexBuffer;
        FrameBuffer                        m_MainFrameBuffer;
        FrameBuffer*                       m_CurrentFrameBuffer;
        void*                              m_Program;
        PipelineState                      m_PipelineState;
        TextureFilter                      m_DefaultTextureMinFilter;
        TextureFilter                      m_DefaultTextureMagFilter;
        ShaderDesc::Language               m_ShaderClassLanguage[2];

        uint32_t                           m_Width;
        uint32_t                           m_Height;
        int32_t                            m_ScissorRect[4];
        uint32_t                           m_TextureFormatSupport;
        uint32_t                           m_TextureUnit;
        // Only use for testing
        uint32_t                           m_AsyncProcessingSupport : 1;
        uint32_t                           m_UseAsyncTextureLoad    : 1;
        uint32_t                           m_RequestWindowClose     : 1;
        uint32_t                           m_PrintDeviceInfo        : 1;
        uint32_t                           m_ContextFeatures        : 3;
    };
}

#endif // __GRAPHICS_DEVICE_NULL__
