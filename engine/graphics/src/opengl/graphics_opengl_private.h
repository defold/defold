// Copyright 2020-2023 The Defold Foundation
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

#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dlib/opaque_handle_container.h>

namespace dmGraphics
{
    enum AttachmentType
    {
        ATTACHMENT_TYPE_UNUSED  = 0,
        ATTACHMENT_TYPE_BUFFER  = 1,
        ATTACHMENT_TYPE_TEXTURE = 2,
    };

    struct OpenGLTexture
    {
        TextureType       m_Type;
        GLuint*           m_TextureIds;
        uint32_t          m_ResourceSize; // For Mip level 0. We approximate each mip level is 1/4th. Or MipSize0 * 1.33
        uint16_t          m_NumTextureIds;
        uint16_t          m_Width;
        uint16_t          m_Height;
        uint16_t          m_Depth;
        uint16_t          m_OriginalWidth;
        uint16_t          m_OriginalHeight;
        uint16_t          m_MipMapCount;
        volatile uint16_t m_DataState; // data state per mip-map (mipX = bitX). 0=ok, 1=pending
        TextureParams     m_Params;
    };

    struct OpenGLRenderTargetAttachment
    {
        TextureParams m_Params;
        union
        {
            HTexture m_Texture;
            GLuint   m_Buffer;
        };
        AttachmentType m_Type;
        bool           m_Attached;
    };

    struct OpenGLRenderTarget
    {
        OpenGLRenderTargetAttachment m_ColorAttachments[MAX_BUFFER_COLOR_ATTACHMENTS];
        OpenGLRenderTargetAttachment m_DepthAttachment;
        OpenGLRenderTargetAttachment m_StencilAttachment;
        OpenGLRenderTargetAttachment m_DepthStencilAttachment;
        GLuint                       m_Id;
        uint32_t                     m_BufferTypeFlags;
    };

    struct OpenglVertexAttribute
    {
        dmhash_t m_NameHash;
        int32_t  m_Location;
        GLint    m_Count;
        GLenum   m_Type;
    };

    struct OpenGLConstantValue
    {
        bool    m_ValueSet;
        // The maximum size of a constant is 64 bytes (4x4 matrix is the largest).
        uint8_t m_Value[64];
    };

    struct OpenGLProgram
    {
        GLuint                         m_Id;
        dmArray<OpenglVertexAttribute> m_Attributes;
        dmArray<OpenGLConstantValue>   m_ConstantValues;
    };

    struct OpenGLContext
    {
        OpenGLContext(const ContextParams& params);

        // Async queue data and synchronization objects
        dmMutex::HMutex         m_AsyncMutex;
        dmArray<const char*>    m_Extensions; // pointers into m_ExtensionsString
        char*                   m_ExtensionsString;

        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;

        WindowResizeCallback    m_WindowResizeCallback;
        void*                   m_WindowResizeCallbackUserData;
        WindowCloseCallback     m_WindowCloseCallback;
        void*                   m_WindowCloseCallbackUserData;
        WindowFocusCallback     m_WindowFocusCallback;
        void*                   m_WindowFocusCallbackUserData;
        WindowIconifyCallback   m_WindowIconifyCallback;
        void*                   m_WindowIconifyCallbackUserData;
        PipelineState           m_PipelineState;
        OpenGLProgram*          m_ActiveProgram;
        uint32_t                m_Width;
        uint32_t                m_Height;
        uint32_t                m_WindowWidth;
        uint32_t                m_WindowHeight;
        uint32_t                m_Dpi;
        uint32_t                m_MaxTextureSize;
        TextureFilter           m_DefaultTextureMinFilter;
        TextureFilter           m_DefaultTextureMagFilter;
        uint32_t                m_MaxElementVertices;
        uint32_t                m_MaxElementIndices;
        // Counter to keep track of various modifications. Used for cache flush etc
        // Version zero is never used
        uint32_t                m_ModificationVersion;
        uint32_t                m_IndexBufferFormatSupport;
        uint64_t                m_TextureFormatSupport;
        uint32_t                m_DepthBufferBits;
        uint32_t                m_FrameBufferInvalidateBits;
        float                   m_MaxAnisotropy;
        uint32_t                m_AnisotropySupport                : 1;
        uint32_t                m_TextureArraySupport              : 1;
        uint32_t                m_MultiTargetRenderingSupport      : 1;
        uint32_t                m_FrameBufferInvalidateAttachments : 1;
        uint32_t                m_PackedDepthStencilSupport        : 1;
        uint32_t                m_WindowOpened                     : 1;
        uint32_t                m_VerifyGraphicsCalls              : 1;
        uint32_t                m_RenderDocSupport                 : 1;
        uint32_t                m_IsGles3Version                   : 1; // 0 == gles 2, 1 == gles 3
        uint32_t                m_IsShaderLanguageGles             : 1; // 0 == glsl, 1 == gles
    };

    // JG: dmsdk/graphics.h defines this as a struct ptr so don't want to rename it yet..
    struct VertexDeclaration
    {
        struct Stream
        {
            dmhash_t m_NameHash;
            uint16_t m_LogicalIndex;
            int16_t  m_PhysicalIndex;
            uint16_t m_Size;
            uint16_t m_Offset;
            Type     m_Type;
            bool     m_Normalize;
        };

        Stream      m_Streams[MAX_VERTEX_STREAM_COUNT];
        uint16_t    m_StreamCount;
        uint16_t    m_Stride;
        HProgram    m_BoundForProgram;
        uint32_t    m_ModificationVersion;
    };

    struct OpenGLShader
    {
        GLuint m_Id;
    };
}
#endif // __GRAPHICS_DEVICE_OPENGL__
