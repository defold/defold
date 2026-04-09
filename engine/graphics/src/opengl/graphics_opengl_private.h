// Copyright 2020-2026 The Defold Foundation
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
#include <dlib/jobsystem.h>
#include <dlib/mutex.h>
#include <dmsdk/dlib/atomic.h>
#include <dmsdk/vectormath/cpp/vectormath_aos.h>
#include <dlib/opaque_handle_container.h>
#include <platform/window.h>

#include "../graphics_private.h"

namespace dmGraphics
{
    typedef uint32_t HOpenglID;

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

    // TODO: Merge with vulkan, dx12, webgpu!
    struct OpenGLSampler
    {
        TextureFilter m_MinFilter;
        TextureFilter m_MagFilter;
        TextureWrap   m_AddressModeU;
        TextureWrap   m_AddressModeV;
        float         m_MaxAnisotropy;
    };

    struct OpenGLTexture
    {
        Texture       m_Base;
        TextureParams     m_Params;
        HOpenglID*        m_TextureIds;
        OpenGLSampler     m_Sampler;
        OpenGLSampler     m_SamplerDirty;
        uint32_t          m_ResourceSize; // For Mip level 0. We approximate each mip level is 1/4th. Or MipSize0 * 1.33
    };

    struct OpenGLTextureBinding
    {
        HTexture m_Texture;
        uint8_t  m_TextureIdIndex;
    };

    struct OpenGLRenderTargetAttachment
    {
        TextureParams m_Params;
        union
        {
            HTexture  m_Texture;
            HOpenglID m_Buffer;
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
        HOpenglID                    m_Id;
        uint32_t                     m_BufferTypeFlags;
    };

    struct OpenGLShader
    {
        HOpenglID            m_Id;
        ShaderDesc::Language m_Language;
        ShaderStageFlag      m_Stage;
    };

    struct OpenGLBuffer
    {
        HOpenglID        m_Id;
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
        UniformBuffer m_BaseUniformBuffer;
        HOpenglID     m_Id;
    };

    struct OpenGLScratchUniformBuffer
    {
        const UniformBufferLayout* m_Layout;
        dmArray<GLint>             m_Indices;
        dmArray<GLint>             m_Offsets;
        uint8_t*                   m_BlockMemory;
        HOpenglID                  m_Id;
        GLint                      m_BindPoint;
        GLint                      m_BlockSize; // Total GL buffer size (whole instance array or one block)
        GLint                      m_ActiveUniforms;
        uint32_t                   m_FirstGlBlockIndex; // glGetUniformBlockIndex for [0] / sole block
        uint32_t                   m_SingleElementSize;  // GL_UNIFORM_BLOCK_DATA_SIZE for one array element
        uint32_t                   m_ElementStride;      // Byte stride between elements (UBO offset aligned)
        uint16_t                   m_ArrayElementCount;  // 1 for non-array UBOs
        uint8_t                    m_ResourceBinding;
        uint8_t                    m_ResourceSet : 7;
        uint8_t                    m_Dirty       : 1;
    };

    struct OpenGLProgram
    {
        Program                             m_BaseProgram;
        OpenGLShader*                       m_VertexShader;
        OpenGLShader*                       m_FragmentShader;
        OpenGLShader*                       m_ComputeShader;
        uint32_t                            m_Id;
        ShaderDesc::Language                m_Language;
        dmArray<OpenGLVertexAttribute>      m_Attributes;
        dmArray<OpenGLScratchUniformBuffer> m_UniformBuffers;
    };

    /*
    * Store all allocated OpenGL handles in one array.
    * All other abstractions should use index of handles inside that array instead of direct use of GL handle.
    * It helps to avoid changing relationship between resource/component and graphical handle.
    * But it enables to recreate all underlying handles without changes of external connection.
    */
    struct OpenGLHandlesData
    {
        dmMutex::HMutex    m_Mutex; /// Guards access to m_AllGLHandles and m_FreeIndexes
        dmArray<GLuint>    m_AllGLHandles;
        dmArray<HOpenglID> m_FreeIndexes; /// contains indexes that can be reused in m_AllGLHandles
    };

    struct OpenGLContext
    {
        OpenGLContext(const ContextParams& params);

        GraphicsContext         m_BaseContext;
        SetTextureAsyncState    m_SetTextureAsyncState;
        HJobContext             m_JobContext;
        dmArray<const char*>    m_Extensions; // pointers into m_ExtensionsString
        char*                   m_ExtensionsString;
        void*                   m_AuxContext;
        int32_atomic_t          m_AuxContextJobPending;
        int32_atomic_t          m_DeleteContextRequested;

        OpenGLProgram*          m_CurrentProgram;
        OpenGLUniformBuffer*    m_CurrentUniformBuffers[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT];
        OpenGLTextureBinding    m_CurrentTextures[DM_MAX_TEXTURE_UNITS];

        OpenGLHandlesData                  m_GLHandlesData;

        PipelineState           m_PipelineState;      // Last applied pipeline state
        PipelineState           m_PipelineStateDirty; // Current dirty state
        int32_t                 m_ViewportRect[4];
        int32_t                 m_ScissorRect[4];
        int32_t                 m_ScissorRectDirty[4];
        HOpenglID               m_GlobalVAO;
        uint32_t                m_MaxTextureSize;
        uint32_t                m_MaxElementVertices;
        // Counter to keep track of various modifications. Used for cache flush etc
        // Version zero is never used
        uint32_t                m_ModificationVersion;
        uint32_t                m_IndexBufferFormatSupport;
        uint32_t                m_DepthBufferBits;
        uint32_t                m_FrameBufferInvalidateBits;
        float                   m_MaxAnisotropy;
        uint32_t                m_FrameBufferInvalidateAttachments : 1;
        uint32_t                m_IsGles3Version                   : 1; // 0 == gles 2, 1 == gles 3
        uint32_t                m_IsShaderLanguageGles             : 1; // 0 == glsl, 1 == gles

        uint32_t                m_PackedDepthStencilSupport        : 1;
        uint32_t                m_AsyncProcessingSupport           : 1;
        uint32_t                m_AnisotropySupport                : 1;
        uint32_t                m_TextureArraySupport              : 1;
        uint32_t                m_MultiTargetRenderingSupport      : 1;
        uint32_t                m_ComputeSupport                   : 1;
        uint32_t                m_StorageBufferSupport             : 1;
        uint32_t                m_InstancingSupport                : 1;
        uint32_t                m_ASTCSupport                      : 1;
        // ASTC for 2D array textures (paged atlases). Some HTML5/GLES drivers
        // fail on ASTC uploads for array targets even if 2D works. This flag
        // allows us to disable ASTC only for array textures without disabling
        // ASTC entirely.
        uint32_t                m_ASTCArrayTextureSupport          : 1;
        uint32_t                m_3DTextureSupport                 : 1;
    };
}
#endif // __GRAPHICS_DEVICE_OPENGL__
