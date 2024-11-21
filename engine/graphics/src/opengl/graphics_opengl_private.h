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

#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include <dlib/atomic.h>
#include <dlib/math.h>
#include <dmsdk/dlib/atomic.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dlib/opaque_handle_container.h>
#include <platform/platform_window.h>

namespace dmGraphics
{
    enum AttachmentType
    {
        ATTACHMENT_TYPE_UNUSED  = 0,
        ATTACHMENT_TYPE_BUFFER  = 1,
        ATTACHMENT_TYPE_TEXTURE = 2,
    };

    enum DeviceBufferType
    {
        DEVICE_BUFFER_TYPE_INDEX   = 0,
        DEVICE_BUFFER_TYPE_VERTEX  = 1,
    };

    struct OpenGLTexture
    {
        TextureParams     m_Params;
        TextureType       m_Type;
        GLuint*           m_TextureIds;
        uint32_t          m_ResourceSize; // For Mip level 0. We approximate each mip level is 1/4th. Or MipSize0 * 1.33
        int32_atomic_t    m_DataState; // data state per mip-map (mipX = bitX). 0=ok, 1=pending
        uint16_t          m_NumTextureIds;
        uint16_t          m_Width;
        uint16_t          m_Height;
        uint16_t          m_Depth;
        uint16_t          m_OriginalWidth;
        uint16_t          m_OriginalHeight;
        uint16_t          m_MipMapCount;
        uint8_t           m_UsageHintFlags;
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

    struct OpenGLShader
    {
        GLuint               m_Id;
        ShaderMeta           m_ShaderMeta;
        ShaderDesc::Language m_Language;
    };

    struct OpenGLBuffer
    {
        GLuint           m_Id;
        DeviceBufferType m_Type;
        uint32_t         m_MemorySize;
    };

    struct OpenGLVertexAttribute
    {
        dmhash_t m_NameHash;
        int32_t  m_Location;
        GLint    m_Count;
        GLenum   m_Type;
    };

    struct OpenGLUniformBuffer
    {
        dmArray<GLint> m_Indices;
        dmArray<GLint> m_Offsets;
        uint8_t*       m_BlockMemory;
        GLuint         m_Id;
        GLint          m_Binding;
        GLint          m_BlockSize;
        GLint          m_ActiveUniforms;
        uint8_t        m_Dirty : 1;
    };

    struct OpenGLUniform
    {
        char*            m_Name;
        dmhash_t         m_NameHash;
        HUniformLocation m_Location;
        GLint            m_Count;
        GLenum           m_Type;
        uint8_t          m_TextureUnit   : 7;
        uint8_t          m_IsTextureType : 1;
    };

    struct OpenGLProgram
    {
        GLuint                         m_Id;
        ShaderDesc::Language           m_Language;
        dmArray<OpenGLVertexAttribute> m_Attributes;
        dmArray<OpenGLUniformBuffer>   m_UniformBuffers;
        dmArray<OpenGLUniform>         m_Uniforms;
    };

    struct OpenGLContext
    {
        OpenGLContext(const ContextParams& params);

        SetTextureAsyncState    m_SetTextureAsyncState;
        dmPlatform::HWindow     m_Window;
        dmJobThread::HContext   m_JobThread;
        dmArray<const char*>    m_Extensions; // pointers into m_ExtensionsString
        char*                   m_ExtensionsString;
        void*                   m_AuxContext;
        int32_atomic_t          m_AuxContextJobPending;
        int32_atomic_t          m_DeleteContextRequested;

        OpenGLProgram*          m_CurrentProgram;

        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;

        PipelineState           m_PipelineState;

        GLuint                  m_GlobalVAO;

        uint32_t                m_Width;
        uint32_t                m_Height;
        uint32_t                m_MaxTextureSize;
        TextureFilter           m_DefaultTextureMinFilter;
        TextureFilter           m_DefaultTextureMagFilter;
        uint32_t                m_MaxElementVertices;
        // Counter to keep track of various modifications. Used for cache flush etc
        // Version zero is never used
        uint32_t                m_ModificationVersion;
        uint32_t                m_IndexBufferFormatSupport;
        uint64_t                m_TextureFormatSupport;
        uint32_t                m_DepthBufferBits;
        uint32_t                m_FrameBufferInvalidateBits;
        float                   m_MaxAnisotropy;
        uint32_t                m_FrameBufferInvalidateAttachments : 1;
        uint32_t                m_VerifyGraphicsCalls              : 1;
        uint32_t                m_PrintDeviceInfo                  : 1;
        uint32_t                m_IsGles3Version                   : 1; // 0 == gles 2, 1 == gles 3
        uint32_t                m_IsShaderLanguageGles             : 1; // 0 == glsl, 1 == gles

        uint32_t                m_PackedDepthStencilSupport        : 1;
        uint32_t                m_AsyncProcessingSupport           : 1;
        uint32_t                m_AnisotropySupport                : 1;
        uint32_t                m_TextureArraySupport              : 1;
        uint32_t                m_MultiTargetRenderingSupport      : 1;
        uint32_t                m_ComputeSupport                   : 1;
        uint32_t                m_StorageBufferSupport             : 1;
        uint32_t                m_RenderDocSupport                 : 1;
        uint32_t                m_InstancingSupport                : 1;
    };
}
#endif // __GRAPHICS_DEVICE_OPENGL__
