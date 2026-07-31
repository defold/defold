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

#ifndef DM_GRAPHICS_PRIVATE_H
#define DM_GRAPHICS_PRIVATE_H

#include <stdint.h>
#include <dlib/mutex.h>
#include <dlib/index_pool.h>
#include <dmsdk/dlib/atomic.h>
#include "graphics.h"

namespace dmGraphics
{
    // Shared texture metadata embedded as m_Base in each backend texture struct (OpenGLTexture, VulkanTexture, etc.).
    // Each adapter keeps Texture as the first non-vtable member so the asset pointer aliases m_Base and
    // GetAssetFromContainer<Texture>(container, handle) is valid alongside GetAssetFromContainer<BackendTexture>(...).
    struct Texture
    {
        TextureType    m_Type;
        TextureFormat  m_Format;
        uint16_t       m_Width;
        uint16_t       m_Height;
        uint16_t       m_Depth;
        uint16_t       m_OriginalWidth;
        uint16_t       m_OriginalHeight;
        uint16_t       m_OriginalDepth;
        uint8_t        m_MipMapCount;
        uint8_t        m_PageCount;
        uint8_t        m_UsageHintFlags;
        uint16_t       m_NumTextureIds;
        uint32_t       m_ResourceSize;
        uint32_t       m_Mip0ResourceSize;
        int32_atomic_t m_DataState; // mip bits for upload pending; mutable so const Texture* can pass &m_DataState to atomics
    };

    struct Buffer
    {
        uint32_t m_Size;
    };

    static inline uint32_t EstimateTextureResourceDataSize(const Texture* texture, uint32_t mip0_size, bool multiply_depth)
    {
        uint32_t size_total = 0;
        uint32_t size       = mip0_size;
        if (size == 0)
        {
            size = texture->m_Mip0ResourceSize;
            if (size == 0)
            {
                size = texture->m_Width * texture->m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(texture->m_Format) / 8);
            }
        }
        for (uint32_t i = 0; i < texture->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP || texture->m_Type == TEXTURE_TYPE_TEXTURE_CUBE)
        {
            size_total *= 6;
        }
        else if (multiply_depth)
        {
            size_total *= dmMath::Max((uint16_t) 1, texture->m_Depth);
        }
        return size_total;
    }

    static inline void SetTextureResourceSize(Texture* texture, uint32_t backend_texture_size, uint32_t mip0_size = 0, bool multiply_depth = false)
    {
        if (mip0_size != 0 || texture->m_Mip0ResourceSize == 0)
        {
            texture->m_Mip0ResourceSize = mip0_size;
        }
        texture->m_ResourceSize = EstimateTextureResourceDataSize(texture, mip0_size, multiply_depth) + backend_texture_size;
    }

    static inline void SetTextureResourceSizeExact(Texture* texture, uint32_t backend_texture_size, uint32_t data_size)
    {
        texture->m_ResourceSize = data_size + backend_texture_size;
    }

    // Shared render target metadata embedded as m_Base in each backend render target struct.
    // Each adapter keeps RenderTarget as the first non-vtable member so the asset pointer aliases m_Base and
    // GetAssetFromContainer<RenderTarget>(container, handle) is valid alongside GetAssetFromContainer<BackendRenderTarget>(...).
    struct RenderTarget
    {
        TextureParams m_ColorTextureParams[MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureParams m_DepthBufferParams;
        TextureParams m_StencilBufferParams;
        TextureParams m_DepthStencilTextureParams;
        HTexture      m_TextureColor[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture      m_TextureDepth;
        HTexture      m_TextureStencil;
        HTexture      m_TextureDepthStencil;
        uint16_t      m_Id;
        uint8_t       m_ColorAttachmentCount;
        uint8_t       m_IsBound;
    };

    const static uint8_t DM_RENDERTARGET_BACKBUFFER_ID = 0;
    const static uint8_t MAX_VERTEX_BUFFERS            = 3;
    const static uint8_t MAX_BINDINGS_PER_SET_COUNT    = 32;
    const static uint8_t MAX_SET_COUNT                 = 4;
    const static uint8_t MAX_STORAGE_BUFFERS           = 4;
    const static uint8_t DM_MAX_TEXTURE_UNITS          = 32;
    const static uint8_t UNUSED_BINDING_OR_SET         = 0xFF;

    // In OpenGL, there is a single global resource identifier between
    // fragment and vertex uniforms for a single program. In Vulkan,
    // a uniform can be present in both shaders so we have to keep track
    // of this ourselves. Because of this we pack resource locations
    // for uniforms in a single base register with 15 bits
    // per shader location. If uniform is not found, we return -1 as usual.
    #define UNIFORM_LOCATION_MAX          ((uint64_t) 0xFFFF)
    #define UNIFORM_LOCATION_GET_OP0(loc) (loc & UNIFORM_LOCATION_MAX)
    #define UNIFORM_LOCATION_GET_OP1(loc) ((loc & (UNIFORM_LOCATION_MAX << 16)) >> 16)
    #define UNIFORM_LOCATION_GET_OP2(loc) ((loc & (UNIFORM_LOCATION_MAX << 32)) >> 32)
    #define UNIFORM_LOCATION_GET_OP3(loc) ((loc & (UNIFORM_LOCATION_MAX << 48)) >> 48)

    struct ProgramResourceBinding;
    struct ShaderResourceMember;

    struct CreateUniformLeafMembersCallbackParams
    {
        // Temporary char*, don't count on them existing outside of the callback
        char*                         m_CanonicalName;
        char*                         m_Namespace;
        char*                         m_InstanceName;
        const ProgramResourceBinding* m_Resource;
        const ShaderResourceMember*   m_Member;
        uint32_t                      m_BaseOffset;
    };

    typedef void (*IterateUniformsCallback)(const CreateUniformLeafMembersCallbackParams& params, void* user_data);

    // Fence values to indicate frame ready or in-use state
    enum RenderContextState
    {
        RENDER_CONTEXT_STATE_FREE   = 0,
        RENDER_CONTEXT_STATE_IN_USE = 1,
    };

    enum ShaderStageFlag
    {
        SHADER_STAGE_FLAG_VERTEX   = 0x1,
        SHADER_STAGE_FLAG_FRAGMENT = 0x2,
        SHADER_STAGE_FLAG_COMPUTE  = 0x4,
    };

    struct VertexStream
    {
        dmhash_t m_NameHash;
        uint32_t m_Stream;
        uint32_t m_Size;
        Type     m_Type;
        bool     m_Normalize;
    };

    struct VertexStreamDeclaration
    {
        dmArray<VertexStream> m_Streams;
        VertexStepFunction    m_StepFunction;
    };

    struct ShaderResourceBinding
    {
        union BindingInfo
        {
            uint16_t m_BlockSize;
            uint16_t m_SamplerTextureIndex;
        };

        char*                       m_Name;
        dmhash_t                    m_NameHash;
        char*                       m_InstanceName;
        dmhash_t                    m_InstanceNameHash;
        ShaderResourceType          m_Type;
        ShaderResourceBindingFamily m_BindingFamily;
        BindingInfo                 m_BindingInfo;
        uint16_t                    m_Set;
        uint16_t                    m_Binding;
        uint16_t                    m_ElementCount;
        uint8_t                     m_StageFlags;
    };

    struct ShaderMeta
    {
        dmArray<ShaderResourceBinding>  m_UniformBuffers;
        dmArray<ShaderResourceBinding>  m_StorageBuffers;
        dmArray<ShaderResourceBinding>  m_Textures;
        dmArray<ShaderResourceBinding>  m_Inputs;
        dmArray<ShaderResourceTypeInfo> m_TypeInfos;
    };

    struct SetTextureAsyncParams
    {
        HTexture                m_Texture;
        TextureParams           m_Params;
        SetTextureAsyncCallback m_Callback;
        void*                   m_UserData;
    };

    struct SetTextureAsyncState
    {
        dmMutex::HMutex                m_Mutex;
        dmArray<SetTextureAsyncParams> m_Params;
        dmIndexPool16                  m_Indices;
        dmArray<HTexture>              m_PostDeleteTextures;
    };

    // Shared fields embedded as m_BaseContext (first member) in each backend context struct.
    struct GraphicsContext
    {
        GraphicsContextLimits              m_Limits;
        uint16_t                           m_AdapterVersionMajor;
        uint16_t                           m_AdapterVersionMinor;
        HWindow                            m_Window;
        dmOpaqueHandleContainer<uintptr_t> m_AssetHandleContainer;
        dmMutex::HMutex                    m_AssetHandleContainerMutex;
        uint64_t                           m_TextureFormatSupport;
        TextureFilter                      m_DefaultTextureMinFilter;
        TextureFilter                      m_DefaultTextureMagFilter;
        uint32_t                           m_ContextFeatureSupport;
        uint32_t                           m_Width;
        uint32_t                           m_Height;
        // Every live render target, maintained by the NewRenderTarget/DeleteRenderTarget dispatchers.
        // Render targets are not resource-backed in general (render scripts create them at runtime),
        // so RecreateGraphicsHandles walks this list to restore them in place after a lost context.
        dmArray<HRenderTarget>             m_RenderTargets;
        // Bumped by the InvalidateGraphicsHandles dispatcher on every context loss, see
        // GetInvalidationGeneration.
        uint32_t                           m_InvalidationGeneration;
        uint32_t                           m_VerifyGraphicsCalls : 1;
        uint32_t                           m_PrintDeviceInfo     : 1;
    };

    static inline void SetContextFeatureSupported(GraphicsContext* context, ContextFeature feature)
    {
        assert(feature < MAX_CONTEXT_FEATURE_COUNT);
        context->m_ContextFeatureSupport |= 1 << feature;
    }

    static inline void SetAllContextFeaturesSupported(GraphicsContext* context)
    {
        for (uint32_t i = 0; i < MAX_CONTEXT_FEATURE_COUNT; ++i)
        {
            SetContextFeatureSupported(context, (ContextFeature) i);
        }
    }

    static inline void SetContextTextureFormatSupported(GraphicsContext* context, TextureFormat format)
    {
        context->m_TextureFormatSupport |= 1ULL << format;
    }

    static inline void SetContextASTCTextureFormatsSupported(GraphicsContext* context)
    {
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_4X4);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_5X4);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_5X5);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_6X5);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_6X6);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_8X5);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_8X6);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_8X8);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_10X5);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_10X6);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_10X8);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_10X10);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_12X10);
        SetContextTextureFormatSupported(context, TEXTURE_FORMAT_RGBA_ASTC_12X12);
    }

    struct ProgramResourceBindingsInfo
    {
        uint32_t m_UniformBufferCount;
        uint32_t m_StorageBufferCount;
        uint32_t m_TextureCount;
        uint32_t m_SamplerCount;
        uint32_t m_UniformDataSize;
        uint32_t m_UniformDataSizeAligned;
        uint32_t m_MaxSet;
        uint32_t m_MaxBinding;
    };

    struct ResourceBindingDesc
    {
        uint16_t m_Binding;
        uint8_t  m_Taken;
    };

    struct ProgramResourceBinding
    {
        ShaderResourceBinding*           m_Res;
        dmArray<ShaderResourceTypeInfo>* m_TypeInfos;
        void*                            m_BindingUserData;

        union
        {
            uint32_t m_TextureUnit;
            uint32_t m_StorageBufferUnit;
            uint32_t m_UniformBufferOffset; // Offset into scratch space typically
        };

        uint8_t m_StageFlags;
    };

    struct UniformBuffer
    {
        UniformBufferLayout m_Layout;
        uint32_t            m_Size;
        uint8_t             m_BoundBinding;
        uint8_t             m_BoundSet;
    };

    struct Program
    {
        ProgramResourceBinding       m_ResourceBindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT];
        ShaderMeta                   m_ShaderMeta;
        dmArray<Uniform>             m_Uniforms;
        dmArray<UniformBufferLayout> m_UniformBufferLayouts;
        uint8_t                      m_MaxSet;
        uint8_t                      m_MaxBinding;
    };

    struct ProgramResourceBindingIterator
    {
        const Program* m_Program;
        uint32_t       m_CurrentSet;
        uint32_t       m_CurrentBinding;

        ProgramResourceBindingIterator(const Program* pgm)
        : m_Program(pgm)
        , m_CurrentSet(0)
        , m_CurrentBinding(0)
        {}

        const ProgramResourceBinding* Next()
        {
            for (; m_CurrentSet < m_Program->m_MaxSet; ++m_CurrentSet)
            {
                for (; m_CurrentBinding < m_Program->m_MaxBinding; ++m_CurrentBinding)
                {
                    if (m_Program->m_ResourceBindings[m_CurrentSet][m_CurrentBinding].m_Res != 0x0)
                    {
                        const ProgramResourceBinding* res = &m_Program->m_ResourceBindings[m_CurrentSet][m_CurrentBinding];
                        m_CurrentBinding++;
                        return res;
                    }
                }
                m_CurrentBinding = 0;  // Reset binding index when moving to the next set
            }
            return 0x0;
        }

        void Reset()
        {
            m_CurrentSet = 0;
            m_CurrentBinding = 0;
        }
    };

    struct TextureFormatCompressedBlockSize
    {
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_ByteSize;
    };

    uint32_t                   GetTextureFormatBitsPerPixel(TextureFormat format); // Gets the bits per pixel from uncompressed formats
    bool                       GetTextureFormatCompressedBlockSize(TextureFormat format, TextureFormatCompressedBlockSize* out);
    uint32_t                   GetGraphicsTypeDataSize(Type type);
    void                       InstallAdapterVendor();
    PipelineState              GetDefaultPipelineState();
    Type                       GetGraphicsTypeFromShaderDataType(ShaderDesc::ShaderDataType shader_type);
    void                       SetForceFragmentReloadFail(bool should_fail);
    void                       SetForceVertexReloadFail(bool should_fail);
    void                       SetPipelineStateValue(PipelineState& pipeline_state, State state, uint8_t value);
    bool                       IsTextureFormatCompressed(TextureFormat format);
    bool                       IsTextureFormatASTC(TextureFormat format);
    const char*                TextureFormatToString(TextureFormat format);
    ShaderDesc::Language       GetShaderProgramLanguage(HContext context);
    uint32_t                   GetShaderTypeSize(ShaderDesc::ShaderDataType type);
    Type                       ShaderDataTypeToGraphicsType(ShaderDesc::ShaderDataType shader_type);
    ShaderDesc::ShaderDataType GraphicsTypeToShaderDataType(Type graphics_type);
    bool                       GetShaderProgram(HContext context, ShaderDesc* shader_desc, ShaderDesc::Shader** vp, ShaderDesc::Shader** fp, ShaderDesc::Shader** cp);

    void                       CreateShaderMeta(ShaderDesc::ShaderReflection* ddf, ShaderMeta* meta);
    void                       DestroyShaderMeta(ShaderMeta& meta);
    bool                       GetUniformIndices(const dmArray<ShaderResourceBinding>& uniforms, dmhash_t name_hash, uint64_t* index_out, uint64_t* index_member_out);
    uint32_t                   CountShaderResourceLeafMembers(const dmArray<ShaderResourceTypeInfo>& type_infos, ShaderResourceType type, uint32_t count = 0);
    void                       BuildUniforms(Program* program);
    void                       IterateUniforms(Program* program, bool prepend_instance_name, IterateUniformsCallback callback, void* user_data);
    UniformBufferLayout*       AddUniformBufferLayout(Program* program, const ShaderResourceBinding* res, const ShaderResourceTypeInfo* type_infos, uint32_t num_type_infos);

    void FillProgramResourceBindings(Program& program,
        dmArray<ShaderResourceBinding>&       resources,
        dmArray<ShaderResourceTypeInfo>&      stage_type_infos,
        ResourceBindingDesc                   bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                              ubo_alignment,
        uint32_t                              ssbo_alignment,
        ProgramResourceBindingsInfo&          info);

    void FillProgramResourceBindings(
        Program*                     program,
        ResourceBindingDesc          bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT],
        uint32_t                     ubo_alignment,
        uint32_t                     ssbo_alignment,
        ProgramResourceBindingsInfo& info);

    void                  InitializeSetTextureAsyncState(SetTextureAsyncState& state);
    void                  ResetSetTextureAsyncState(SetTextureAsyncState& state);
    SetTextureAsyncParams GetSetTextureAsyncParams(SetTextureAsyncState& state, uint16_t index);
    uint16_t              PushSetTextureAsyncState(SetTextureAsyncState& state, HTexture texture, TextureParams params, SetTextureAsyncCallback callback, void* user_data);
    void                  ReturnSetTextureAsyncIndex(SetTextureAsyncState& state, uint16_t index);
    void                  PushSetTextureAsyncDeleteTexture(SetTextureAsyncState& state, HTexture texture);

    struct ScopedLock
    {
        ScopedLock(dmMutex::HMutex mutex)
        : m_Mutex(mutex)
        {
            if (m_Mutex)
            {
                dmMutex::Lock(m_Mutex);
            }
        }
        ~ScopedLock()
        {
            if (m_Mutex)
            {
                dmMutex::Unlock(m_Mutex);
            }
        }
        dmMutex::HMutex m_Mutex;
    };

    static inline void ClearTextureParamsData(TextureParams& params)
    {
        params.m_Data     = 0x0;
        params.m_DataSize = 0;
    }

    static inline uint32_t GetNextRenderTargetId()
    {
        static uint32_t next_id = 1;

        // DM_RENDERTARGET_BACKBUFFER_ID is taken for the main framebuffer
        if (next_id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            next_id = DM_RENDERTARGET_BACKBUFFER_ID + 1;
        }
        return next_id++;
    }

    template <typename T>
    static inline HAssetHandle StoreAssetInContainer(dmOpaqueHandleContainer<uintptr_t>& container, T* asset, AssetType type)
    {
        if (container.Full())
        {
            container.Allocate(8);
        }
        HOpaqueHandle opaque_handle = container.Put((uintptr_t*) asset);
        HAssetHandle asset_handle   = MakeAssetHandle(opaque_handle, type);
        return asset_handle;
    }

    template <typename T>
    static inline T* GetAssetFromContainer(dmOpaqueHandleContainer<uintptr_t>& container, HAssetHandle asset_handle)
    {
        assert(asset_handle <= MAX_ASSET_HANDLE_VALUE);
        HOpaqueHandle opaque_handle = GetOpaqueHandle(asset_handle);
        return (T*) container.Get(opaque_handle);
    }

    // Test only functions:
    void                ResetDrawCount();
    uint64_t            GetDrawCount();
    void                GetTextureFilters(HContext context, uint32_t unit, TextureFilter& min_filter, TextureFilter& mag_filter);
    void                EnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index);
    void                SetOverrideShaderLanguage(HContext context, ShaderDesc::ShaderType shader_class, ShaderDesc::Language language);
    const Uniform*      GetUniform(HProgram prog, dmhash_t name_hash);
    const ShaderMeta*   GetShaderMeta(HProgram prog);
}

#endif // #ifndef DM_GRAPHICS_PRIVATE_H
