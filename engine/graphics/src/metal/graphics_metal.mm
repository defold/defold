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

#include <type_traits>

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.h>
// Metal.hpp is Taken from https://github.com/bkaradzic/metal-cpp/blob/metal-cpp_macOS15.2_iOS18.2/SingleHeader/Metal.hpp
#include <Metal.hpp>
#include <QuartzCore/QuartzCore.h>
#import <Cocoa/Cocoa.h>

#include <dlib/math.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include <dlib/log.h>
#include <dlib/thread.h>

#include <platform/window.hpp>

#include "../graphics_private.h"
#include "../graphics_native.h"
#include "../graphics_adapter.h"

#include "graphics_metal_private.h"

DM_PROPERTY_EXTERN(rmtp_DrawCalls);
DM_PROPERTY_EXTERN(rmtp_DispatchCalls);

namespace dmGraphics
{
    static GraphicsAdapterFunctionTable MetalRegisterFunctionTable();
    static bool                         MetalIsSupported();
    static HContext                     MetalGetContext();
    static bool                         MetalInitialize(MetalContext* context);
    static GraphicsAdapter g_Metal_adapter(ADAPTER_FAMILY_METAL);
    static MetalContext*   g_MetalContext = 0x0;

    DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterMetal, &g_Metal_adapter, MetalIsSupported, MetalRegisterFunctionTable, MetalGetContext, ADAPTER_FAMILY_PRIORITY_METAL);

    static MetalTexture* MetalNewTextureInternal(const TextureCreationParams& params);
    static void          MetalSetTextureInternal(MetalContext* context, MetalTexture* texture, const TextureParams& params);
    static void          CreateMetalTexture(MetalContext* context, MetalTexture* texture, const TextureParams& params, MTL::TextureUsage usage);
    static void          CreateMetalDepthStencilTexture(MetalContext* context, MetalTexture* texture, const TextureParams& params, MTL::TextureUsage usage);
    static int16_t       CreateTextureSampler(MetalContext* context, TextureFilter minFilter, TextureFilter magFilter, TextureWrap uWrap, TextureWrap vWrap, uint8_t maxLod, float maxAnisotropy);
    static void          FlushResourcesToDestroy(MetalContext* context, ResourcesToDestroyList* resource_list);
    static void          BeginRenderPass(MetalContext* context, HRenderTarget render_target);

    struct ClearParams
    {
        float    m_ClearColor[4];
        float    m_ClearDepth;
        uint32_t m_ClearStencil;
        uint32_t m_Padding[2]; // metal will pad this struct to 32 bytes
    };

    MetalContext::MetalContext(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));

        m_BaseContext.m_VerifyGraphicsCalls     = params.m_VerifyGraphicsCalls;
        m_BaseContext.m_PrintDeviceInfo         = params.m_PrintDeviceInfo;
        m_BaseContext.m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_BaseContext.m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_BaseContext.m_Width                   = params.m_Width;
        m_BaseContext.m_Height                  = params.m_Height;
        m_BaseContext.m_Window                  = params.m_Window;
        m_JobContext                            = params.m_JobContext;

        assert(dmPlatform::GetWindowStateParam(m_BaseContext.m_Window, WINDOW_STATE_OPENED));

        if (m_BaseContext.m_DefaultTextureMinFilter == TEXTURE_FILTER_DEFAULT)
            m_BaseContext.m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR;
        if (m_BaseContext.m_DefaultTextureMagFilter == TEXTURE_FILTER_DEFAULT)
            m_BaseContext.m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;

        DM_STATIC_ASSERT(sizeof(m_BaseContext.m_TextureFormatSupport) * 8 >= TEXTURE_FORMAT_COUNT, Invalid_Struct_Size );
    }

    static inline MTL::LoadAction MetalLoadAction(AttachmentOp op)
    {
        switch (op)
        {
            case ATTACHMENT_OP_LOAD:      return MTL::LoadActionLoad;
            case ATTACHMENT_OP_CLEAR:     return MTL::LoadActionClear;
            case ATTACHMENT_OP_DONT_CARE: return MTL::LoadActionDontCare;
            default:                      return MTL::LoadActionLoad;
        }
    }

    static inline MTL::StoreAction MetalStoreAction(AttachmentOp op)
    {
        return op == ATTACHMENT_OP_DONT_CARE ? MTL::StoreActionDontCare : MTL::StoreActionStore;
    }

    static HContext MetalNewContext(const ContextParams& params)
    {
        if (!g_MetalContext)
        {
            g_MetalContext = new MetalContext(params);

            if (MetalInitialize(g_MetalContext))
            {
                return (HContext) g_MetalContext;
            }

            DeleteContext((HContext) g_MetalContext);
        }
        return 0x0;
    }

    static void MetalDeleteContext(HContext _context)
    {
        assert(_context);
        if (g_MetalContext)
        {
            MetalContext* context = (MetalContext*) _context;

            for (uint32_t i = 0; i < context->m_NumFramesInFlight; ++i)
            {
                MetalFrameResource& frame = context->m_FrameResources[i];

                if (frame.m_CommandBuffer && frame.m_InFlight)
                {
                    frame.m_CommandBuffer->waitUntilCompleted();
                }

                FlushResourcesToDestroy(context, frame.m_ResourcesToDestroy);

                if (frame.m_MSAAColorTexture)
                    frame.m_MSAAColorTexture->release();
                if (frame.m_MSAADepthTexture)
                    frame.m_MSAADepthTexture->release();

                delete frame.m_ResourcesToDestroy;
                frame.m_ResourcesToDestroy = 0;

                if (frame.m_AutoReleasePool)
                {
                    frame.m_AutoReleasePool->release();
                    frame.m_AutoReleasePool = 0;
                }
            }

            context->m_Device->release();
            context->m_CommandQueue->release();

            delete (MetalContext*) context;
            g_MetalContext = 0x0;
        }
    }

    static HContext MetalGetContext()
    {
        return (HContext) g_MetalContext;
    }

    static inline MetalFrameResource& GetCurrentFrameResource(MetalContext* context)
    {
        return context->m_FrameResources[context->m_CurrentFrameInFlight];
    }

    static bool MetalIsSupported()
    {
        return true;
    }

    template <typename T>
    static void DestroyResourceDeferred(MetalContext* context, T* resource)
    {
        if (resource == 0x0 || resource->m_Destroyed)
        {
            return;
        }

        ResourceToDestroy resource_to_destroy;
        resource_to_destroy.m_ResourceType = resource->GetType();

        switch(resource_to_destroy.m_ResourceType)
        {
            case RESOURCE_TYPE_DEVICE_BUFFER:
                {
                    resource_to_destroy.m_DeviceBuffer = ((MetalDeviceBuffer*) resource)->m_Buffer;
                    if (resource_to_destroy.m_DeviceBuffer == 0x0)
                    {
                        return;
                    }
                } break;
            case RESOURCE_TYPE_TEXTURE:
                {
                    resource_to_destroy.m_Texture = ((MetalTexture*) resource)->m_Texture;
                    if (resource_to_destroy.m_Texture == 0x0)
                    {
                        return;
                    }
                } break;
            case RESOURCE_TYPE_PROGRAM:
            case RESOURCE_TYPE_RENDER_TARGET:
                break;
            default:
                assert(0);
                break;
        }

        MetalFrameResource& frame = GetCurrentFrameResource(context);

        if (frame.m_ResourcesToDestroy->Full())
        {
            frame.m_ResourcesToDestroy->OffsetCapacity(8);
        }

        frame.m_ResourcesToDestroy->Push(resource_to_destroy);
        resource->m_Destroyed = 1;
    }

    static void FlushResourcesToDestroy(MetalContext* context, ResourcesToDestroyList* resource_list)
    {
        if (resource_list->Size() > 0)
        {
            for (uint32_t i = 0; i < resource_list->Size(); ++i)
            {
                switch(resource_list->Begin()[i].m_ResourceType)
                {
                    case RESOURCE_TYPE_DEVICE_BUFFER:
                        resource_list->Begin()[i].m_DeviceBuffer->release();
                        break;
                    case RESOURCE_TYPE_TEXTURE:
                    case RESOURCE_TYPE_PROGRAM:
                    case RESOURCE_TYPE_RENDER_TARGET:
                        break;
                    default:


                        //assert(0);
                        break;
                }
            }

            resource_list->SetSize(0);
        }
    }

    static inline MTL::ResourceOptions GetResourceOptions(MTL::StorageMode storageMode)
    {
        switch (storageMode)
        {
            case MTL::StorageModePrivate:
                return MTL::ResourceStorageModePrivate | MTL::ResourceCPUCacheModeDefaultCache;
            case MTL::StorageModeManaged:
                return MTL::ResourceStorageModeManaged | MTL::ResourceCPUCacheModeDefaultCache;
            case MTL::StorageModeShared:
            default:
                return MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeDefaultCache;
        }
    }

    static void DeviceBufferUploadHelper(MetalContext* context, const void* data, uint32_t size, uint32_t offset, MetalDeviceBuffer* device_buffer)
    {
        if (size == 0)
            return;

        if (device_buffer->m_Destroyed || device_buffer->m_Buffer == 0x0)
        {
            device_buffer->m_Buffer = context->m_Device->newBuffer(size, GetResourceOptions(device_buffer->m_StorageMode));
            device_buffer->m_Size = size;
        }

        if (data == 0)
        {
            memset(device_buffer->m_Buffer->contents(), 0, (size_t) size);
        }
        else
        {
            memcpy(device_buffer->m_Buffer->contents(), data, size);
        }
    }

    void MetalConstantScratchBuffer::EnsureSize(const MetalContext* context, uint32_t size)
    {
        if (!CanAllocate(size))
        {
            const uint32_t SIZE_INCREASE = 1024 * 8;
            DestroyResourceDeferred((MetalContext*) context, &m_DeviceBuffer);
            DeviceBufferUploadHelper((MetalContext*) context, 0, m_DeviceBuffer.m_Size + SIZE_INCREASE, 0, &m_DeviceBuffer);
            Rewind();
        }
    }

    void MetalArgumentBufferPool::Initialize(const MetalContext* context, uint32_t size_per_buffer)
    {
        m_ScratchBufferIndex = 0;
        m_SizePerBuffer      = size_per_buffer;

        AddBuffer(context);
    }

    void MetalArgumentBufferPool::AddBuffer(const MetalContext* context)
    {
        MetalConstantScratchBuffer buffer = {};
        buffer.m_DeviceBuffer.m_StorageMode = MTL::StorageModeShared;
        buffer.EnsureSize(context, m_SizePerBuffer);
        m_ScratchBufferPool.OffsetCapacity(1);
        m_ScratchBufferPool.Push(buffer);
    }

    MetalConstantScratchBuffer* MetalArgumentBufferPool::Allocate(const MetalContext* context, uint32_t size)
    {
        MetalConstantScratchBuffer* current = Get();

        if (!current->CanAllocate(size))
        {
            m_ScratchBufferIndex++;
            if (m_ScratchBufferIndex >= m_ScratchBufferPool.Size())
            {
                AddBuffer(context);
            }
            current = Get();
        }

        assert(current->CanAllocate(size));
        return current;
    }

    MetalArgumentBinding MetalArgumentBufferPool::Bind(const MetalContext* context, MTL::ArgumentEncoder* encoder)
    {
        uint32_t encode_size_aligned = DM_ALIGN(encoder->encodedLength(), 16);
        assert(encode_size_aligned > 0);

        MetalConstantScratchBuffer* current = Allocate(context, encode_size_aligned);

        MetalArgumentBinding arg_binding = {};
        arg_binding.m_Buffer = current->m_DeviceBuffer.m_Buffer;
        arg_binding.m_Offset = current->m_MappedDataCursor;

        encoder->setArgumentBuffer(current->m_DeviceBuffer.m_Buffer, current->m_MappedDataCursor);
        current->Advance(encoder->encodedLength());

        return arg_binding;
    }

    static void SetupMainRenderTarget(MetalContext* context)
    {
        // Initialize the dummy rendertarget for the main framebuffer
        // The m_Framebuffer construct will be rotated sequentially
        // with the framebuffer objects created per swap chain.

        MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, context->m_MainRenderTarget);
        if (rt == 0x0)
        {
            rt                          = new MetalRenderTarget(DM_RENDERTARGET_BACKBUFFER_ID);
            context->m_MainRenderTarget = StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
            MetalTexture* color         = new MetalTexture();
            rt->m_TextureColor[0]       = StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, color, ASSET_TYPE_TEXTURE);
            MetalTexture* depth         = new MetalTexture();
            rt->m_TextureDepthStencil   = StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, depth, ASSET_TYPE_TEXTURE);
        }

        rt->m_ColorFormat[0]       = MTL::PixelFormatBGRA8Unorm;
        rt->m_DepthStencilFormat   = MTL::PixelFormatDepth32Float_Stencil8;
        rt->m_ColorAttachmentCount = 1;
        rt->m_ColorBufferLoadOps[0] = ATTACHMENT_OP_CLEAR;
        rt->m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
        rt->m_ColorAttachmentClearValue[0][0] = 0.0f;
        rt->m_ColorAttachmentClearValue[0][1] = 0.0f;
        rt->m_ColorAttachmentClearValue[0][2] = 0.0f;
        rt->m_ColorAttachmentClearValue[0][3] = 1.0f;
    }

    static void SetupSupportedTextureFormats(MetalContext* context)
    {
        // PVRTC is always supported on Apple GPUs
        context->m_BaseContext.m_TextureFormatSupport |= (1 << TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
        context->m_BaseContext.m_TextureFormatSupport |= (1 << TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
        context->m_BaseContext.m_TextureFormatSupport |= (1 << TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
        context->m_BaseContext.m_TextureFormatSupport |= (1 << TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);

        // ETC2 support
        if (context->m_Device->supportsFamily(MTL::GPUFamilyApple3))  // A8+ class
        {
            context->m_BaseContext.m_TextureFormatSupport |= (1 << TEXTURE_FORMAT_RGB_ETC1);
            context->m_BaseContext.m_TextureFormatSupport |= (1 << TEXTURE_FORMAT_RGBA_ETC2);
        }

        // ASTC support
        if (context->m_Device->supportsFamily(MTL::GPUFamilyApple3))
        {
            context->m_ASTCSupport = 1;
            context->m_ASTCArrayTextureSupport = 1;
        }

        // Common uncompressed formats
        TextureFormat base_formats[] = {
            TEXTURE_FORMAT_RGBA,
            TEXTURE_FORMAT_RGBA16F,
            TEXTURE_FORMAT_RGBA32F,
            TEXTURE_FORMAT_R16F,
            TEXTURE_FORMAT_R32F,
            TEXTURE_FORMAT_RG16F,
            TEXTURE_FORMAT_RG32F,
            TEXTURE_FORMAT_RGBA32UI,
            TEXTURE_FORMAT_R32UI,
        };

        for (uint32_t i = 0; i < DM_ARRAY_SIZE(base_formats); ++i)
        {
            context->m_BaseContext.m_TextureFormatSupport |= 1 << base_formats[i];
        }

        // RGB isn't supported as a texture format, but we still need to supply it to the engine
        // Later in the vulkan pipeline when the texture is created, we will convert it internally to RGBA
        context->m_BaseContext.m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
    }

    static MTL::Texture* CreateMSAATexture(MetalContext* ctx, uint32_t width, uint32_t height, MTL::PixelFormat fmt, uint32_t sampleCount)
    {
        assert(sampleCount > 1);
        MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
        desc->setTextureType(MTL::TextureType2DMultisample);
        desc->setPixelFormat(fmt);
        desc->setWidth(width);
        desc->setHeight(height);
        desc->setSampleCount(sampleCount);
        desc->setStorageMode(MTL::StorageModePrivate);
        desc->setUsage(MTL::TextureUsageRenderTarget);

        MTL::Texture* t = ctx->m_Device->newTexture(desc);
        desc->release();
        return t;
    }

    // TODO: implement a better sample counting
    static uint32_t MetalGetClosestSampleCount(uint32_t requested)
    {
        if (requested <= 1) return 1;
        if (requested >= 4) return 4;
        if (requested >= 2) return 2;
        return 1;
    }

    static const MetalClearData::ClearShader* GetOrCreateClearShader(MetalContext* context, bool clear_color, bool clear_depth, bool clear_stencil)
    {
        for (int i = 0; i < context->m_ClearData.m_ClearShaderPermutations.Size(); ++i)
        {
            MetalClearData::ClearShader* shader = &context->m_ClearData.m_ClearShaderPermutations[i];

            if (shader->m_ClearColor == clear_color &&
                shader->m_ClearDepth == clear_depth &&
                shader->m_ClearStencil == clear_stencil)
            {
                return shader;
            }
        }

        char header[512] = {};
        uint32_t header_offset = 0;

        if (clear_color)
        {
            header_offset += dmStrlCat(header, "#define CLEAR_COLOR\n", sizeof(header) - header_offset);
        }
        if (clear_depth)
        {
            header_offset += dmStrlCat(header, "#define CLEAR_DEPTH\n", sizeof(header) - header_offset);
        }
        if (clear_stencil)
        {
            header_offset += dmStrlCat(header, "#define CLEAR_STENCIL\n", sizeof(header) - header_offset);
        }

        static const char* body = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct VSOut {
                float4 position [[position]];
            };

            // Fullscreen triangle
            vertex VSOut ClearVS(uint vid [[vertex_id]])
            {
                float2 positions[3] = {
                    float2(-1.0, -1.0),
                    float2(-1.0,  3.0),
                    float2( 3.0, -1.0)
                };

                VSOut out;
                out.position = float4(positions[vid], 0.0, 1.0);
                return out;
            }

            struct ClearParams
            {
                float4 clearColor;
                float  clearDepth;
                uint   clearStencil;
            };

            struct ClearFSOutput
            {
            #ifdef CLEAR_COLOR
                float4 color [[color(0)]];
            #endif

            #ifdef CLEAR_DEPTH
                float depth [[depth(any)]];
            #endif

            #ifdef CLEAR_STENCIL
                uint stencil [[stencil]];
            #endif
            };

            fragment ClearFSOutput ClearFS(constant ClearParams& params [[buffer(0)]])
            {
                ClearFSOutput out;
            #ifdef CLEAR_COLOR
                out.color = params.clearColor;
            #endif

            #ifdef CLEAR_DEPTH
                out.depth = params.clearDepth;
            #endif

            #ifdef CLEAR_STENCIL
                out.stencil = params.clearStencil;
            #endif
                return out;
            }
            )";

        uint32_t header_size = strlen(header);
        uint32_t full_size = header_size + strlen(body) + 1;
        char* full_src = (char*)malloc(full_size);
        full_src[full_size - 1] = 0;
        memcpy(full_src, header, header_size);
        memcpy(full_src + header_size, body, strlen(body));

        NS::Error* error  = 0;
        MTL::Library* library = context->m_Device->newLibrary(NS::String::string(full_src, NS::StringEncoding::UTF8StringEncoding), 0, &error);

        if (error)
        {
            dmLogError("Failed to create Metal clear pipeline: %s", error ? error->localizedDescription()->utf8String() : "Unknown error");
            return 0;
        }
        MetalClearData::ClearShader shader = {};
        shader.m_ClearColor   = clear_color;
        shader.m_ClearDepth   = clear_depth;
        shader.m_ClearStencil = clear_stencil;
        shader.m_VsFunction   = library->newFunction(NS::String::string("ClearVS", NS::StringEncoding::UTF8StringEncoding));
        shader.m_FsFunction   = library->newFunction(NS::String::string("ClearFS", NS::StringEncoding::UTF8StringEncoding));
        library->release();

        context->m_ClearData.m_ClearShaderPermutations.OffsetCapacity(1);
        context->m_ClearData.m_ClearShaderPermutations.Push(shader);

        return &context->m_ClearData.m_ClearShaderPermutations[context->m_ClearData.m_ClearShaderPermutations.Size() - 1];
    }

    static MetalPipeline* GetOrCreateClearPipeline(MetalContext* context, const MetalClearData::CacheKey& key)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &key, sizeof(key));
        uint64_t hash = dmHashFinal64(&pipeline_hash_state);

        MetalPipeline* cached_pipeline = context->m_ClearData.m_PipelineCache.Get(hash);
        if (cached_pipeline)
        {
            return cached_pipeline;
        }

        const MetalClearData::ClearShader* clear_shader = GetOrCreateClearShader(context, key.m_ClearColor, key.m_ClearDepth, key.m_ClearStencil);
        assert(clear_shader);

        // create descriptor
        MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();

        // vertex + fragment
        desc->setVertexFunction(clear_shader->m_VsFunction);
        desc->setFragmentFunction(clear_shader->m_FsFunction);
        desc->setSampleCount(key.m_SampleCount);

        // color attachments
        for (size_t i = 0; i < key.m_ColorAttachmentCount; ++i)
        {
            MTL::RenderPipelineColorAttachmentDescriptor* ca = desc->colorAttachments()->object((uint32_t)i);
            ca->setPixelFormat(key.m_ColorFormats[i]);

            // apply write mask from key
            if (key.m_ColorWriteMaskBits & (1u << (uint32_t)i))
            {
                ca->setWriteMask(MTL::ColorWriteMaskAll);
            }
            else
            {
                ca->setWriteMask(MTL::ColorWriteMaskNone);
            }
        }

        // depth/stencil format (if RT has one)
        desc->setDepthAttachmentPixelFormat(key.m_DepthStencilFormat);
        desc->setStencilAttachmentPixelFormat(key.m_DepthStencilFormat);

        NS::Error* error = NULL;
        MetalPipeline pipeline = {};

        pipeline.m_RenderPipelineState = context->m_Device->newRenderPipelineState(desc, &error);
        desc->release();

        if (!pipeline.m_RenderPipelineState)
        {
            dmLogError("Failed to create Metal pipeline: %s", error ? error->localizedDescription()->utf8String() : "Unknown error");
            return 0;
        }

        MTL::DepthStencilDescriptor* desc_ds = MTL::DepthStencilDescriptor::alloc()->init();
        desc_ds->setDepthWriteEnabled(key.m_ClearDepth);
        desc_ds->setDepthCompareFunction(MTL::CompareFunctionAlways);

        if (key.m_ClearStencil)
        {
            MTL::StencilDescriptor* sd = MTL::StencilDescriptor::alloc()->init();
            sd->setStencilCompareFunction(MTL::CompareFunctionAlways);
            // Replace on pass so the reference value is written
            sd->setDepthFailureOperation(MTL::StencilOperationReplace);
            sd->setStencilFailureOperation(MTL::StencilOperationReplace);
            sd->setDepthStencilPassOperation(MTL::StencilOperationReplace);
            sd->setWriteMask(0xFF);
            desc_ds->setFrontFaceStencil(sd);
            desc_ds->setBackFaceStencil(sd);
            sd->release();
        }

        pipeline.m_DepthStencilState = context->m_Device->newDepthStencilState(desc_ds);
        desc_ds->release();

        if (!pipeline.m_DepthStencilState)
        {
            dmLogError("Failed to create Metal depth stencil state");
            return 0;
        }

        context->m_ClearData.m_PipelineCache.Put(hash, pipeline);
        return context->m_ClearData.m_PipelineCache.Get(hash);
    }

    /*
    static void SetupClearPipeline(MetalContext* context)
    {
        static const char* src = R"(
            #include <metal_stdlib>
            using namespace metal;

            struct VSOut {
                float4 position [[position]];
            };

            // Fullscreen triangle
            vertex VSOut ClearVS(uint vid [[vertex_id]])
            {
                float2 positions[3] = {
                    float2(-1.0, -1.0),
                    float2(-1.0,  3.0),
                    float2( 3.0, -1.0)
                };

                VSOut out;
                out.position = float4(positions[vid], 0.0, 1.0);
                return out;
            }

            struct ClearParams
            {
                float4 clearColor;
                float  clearDepth;
                uint   clearStencil;
            };

            // Always-returning outputs; actual writes controlled via pipeline write masks and DS state
            struct ClearFSOutput {
                float4 color [[color(0)]];
                float depth [[depth(any)]];
                uint stencil [[stencil]];
            };

            fragment ClearFSOutput ClearFS(constant ClearParams& params [[buffer(0)]])
            {
                ClearFSOutput out;
                out.color = params.clearColor;
                out.depth = params.clearDepth;
                out.stencil = params.clearStencil;
                return out;
            }
            )";

        NS::Error* error  = 0;
        MTL::Library* library = context->m_Device->newLibrary(NS::String::string(src, NS::StringEncoding::UTF8StringEncoding), 0, &error);

        if (error)
        {
            dmLogError("Failed to create Metal clear pipeline: %s", error ? error->localizedDescription()->utf8String() : "Unknown error");
        }
        else
        {
            context->m_ClearData.m_Library = library;
            context->m_ClearData.m_PipelineCache.SetCapacity(16,32);
        }
    }
    */

    static bool MetalInitialize(MetalContext* context)
    {
        context->m_Device            = MTL::CreateSystemDefaultDevice();
        context->m_CommandQueue      = context->m_Device->newCommandQueue();
        context->m_NumFramesInFlight = MAX_FRAMES_IN_FLIGHT;
        context->m_FrameBoundarySemaphore = dispatch_semaphore_create(context->m_NumFramesInFlight);
        context->m_PipelineState     = GetDefaultPipelineState();
        context->m_RenderTargetBound = 0;
        context->m_MainRTBegunThisFrame = 0;
        context->m_ViewportChanged   = true;
        context->m_CullFaceChanged   = true;
        context->m_MSAASampleCount   = MetalGetClosestSampleCount(dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_SAMPLE_COUNT));

        SetupMainRenderTarget(context);
        context->m_CurrentRenderTarget = context->m_MainRenderTarget;
        context->m_PipelineCache.SetCapacity(32,64);
        context->m_ClearData.m_PipelineCache.SetCapacity(16,32);

        SetupSupportedTextureFormats(context);

        uint32_t window_width = dmPlatform::GetWindowWidth(context->m_BaseContext.m_Window);
        uint32_t window_height = dmPlatform::GetWindowHeight(context->m_BaseContext.m_Window);

        // Create main resources-to-destroy lists, one for each command buffer
        for (uint32_t i = 0; i < context->m_NumFramesInFlight; ++i)
        {
            context->m_FrameResources[i].m_ResourcesToDestroy = new ResourcesToDestroyList;
            context->m_FrameResources[i].m_ResourcesToDestroy->SetCapacity(8);
            context->m_FrameResources[i].m_CommandBuffer = 0;
            context->m_FrameResources[i].m_Drawable = 0;
            context->m_FrameResources[i].m_AutoReleasePool = 0;
            context->m_FrameResources[i].m_RenderPassDescriptor = 0;
            context->m_FrameResources[i].m_RenderCommandEncoder = 0;
            context->m_FrameResources[i].m_InFlight = 0;

            if (context->m_MSAASampleCount > 1)
            {
                context->m_FrameResources[i].m_MSAAColorTexture = CreateMSAATexture(context, window_width, window_height, MTL::PixelFormatBGRA8Unorm, context->m_MSAASampleCount);
                context->m_FrameResources[i].m_MSAADepthTexture = CreateMSAATexture(context, window_width, window_height, MTL::PixelFormatDepth32Float_Stencil8, context->m_MSAASampleCount);
            }

            // This is just the starting point size for the constant scratch buffer,
            // the buffers can grow as needed.

            // Something is wrong here, I can't make larger buffer here?
            const uint32_t constant_buffer_size = 1024 * 8;
            context->m_FrameResources[i].m_ConstantScratchBuffer.m_DeviceBuffer.m_StorageMode = MTL::StorageModeShared;
            DeviceBufferUploadHelper(context, 0, constant_buffer_size, 0, &context->m_FrameResources[i].m_ConstantScratchBuffer.m_DeviceBuffer);

            // This is a fixed size per buffer in the pool
            const uint32_t argument_buffer_size = 1024 * 4;
            context->m_FrameResources[i].m_ArgumentBufferPool.Initialize(context, argument_buffer_size);
        }

        context->m_AsyncProcessingSupport = context->m_JobContext != 0x0 && dmThread::PlatformHasThreadSupport();
        if (context->m_AsyncProcessingSupport)
        {
            InitializeSetTextureAsyncState(context->m_SetTextureAsyncState);
            context->m_BaseContext.m_AssetHandleContainerMutex = dmMutex::New();
        }

        // Create default texture sampler
        CreateTextureSampler(context, TEXTURE_FILTER_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 1, 1.0f);

        // Create default dummy texture
        TextureCreationParams default_texture_creation_params;
        default_texture_creation_params.m_Width          = 1;
        default_texture_creation_params.m_Height         = 1;
        default_texture_creation_params.m_LayerCount     = 1;
        default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
        default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

        const uint8_t default_texture_data[4 * 6] = {}; // RGBA * 6 (for cubemap)

        TextureParams default_texture_params;
        default_texture_params.m_Width      = 1;
        default_texture_params.m_Height     = 1;
        default_texture_params.m_LayerCount = 1;
        default_texture_params.m_Data       = default_texture_data;
        default_texture_params.m_Format     = TEXTURE_FORMAT_RGBA;

        context->m_DefaultTexture2D = MetalNewTextureInternal(default_texture_creation_params);
        MetalSetTextureInternal(context, context->m_DefaultTexture2D, default_texture_params);

        default_texture_params.m_Format = TEXTURE_FORMAT_RGBA32UI;
        context->m_DefaultTexture2D32UI = MetalNewTextureInternal(default_texture_creation_params);
        MetalSetTextureInternal(context, context->m_DefaultTexture2D32UI, default_texture_params);

        default_texture_params.m_Format                 = TEXTURE_FORMAT_RGBA;
        default_texture_creation_params.m_LayerCount    = 1;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_IMAGE_2D;
        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_STORAGE | TEXTURE_USAGE_FLAG_SAMPLE;
        context->m_DefaultStorageImage2D                = MetalNewTextureInternal(default_texture_creation_params);
        MetalSetTextureInternal(context, context->m_DefaultStorageImage2D, default_texture_params);

        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_SAMPLE;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_2D_ARRAY;
        default_texture_creation_params.m_LayerCount    = 1;
        context->m_DefaultTexture2DArray                = MetalNewTextureInternal(default_texture_creation_params);
        MetalSetTextureInternal(context, context->m_DefaultTexture2DArray, default_texture_params);

        default_texture_creation_params.m_Type          = TEXTURE_TYPE_CUBE_MAP;
        default_texture_creation_params.m_Depth         = 1;
        default_texture_creation_params.m_LayerCount    = 6;
        context->m_DefaultTextureCubeMap = MetalNewTextureInternal(default_texture_creation_params);
        MetalSetTextureInternal(context, context->m_DefaultTextureCubeMap, default_texture_params);

        NSWindow* mative_window = (NSWindow*) dmGraphics::GetNativeOSXNSWindow();
        context->m_View         = [mative_window contentView];

        context->m_Layer               = [CAMetalLayer layer];
        context->m_Layer.device        = (__bridge id<MTLDevice>) context->m_Device;
        context->m_Layer.pixelFormat   = MTLPixelFormatBGRA8Unorm;
        context->m_Layer.drawableSize  = CGSizeMake(window_width, window_height);

        [context->m_View setLayer:context->m_Layer];
        [context->m_View setWantsLayer:YES];

        MTL::TextureDescriptor* depthDesc = MTL::TextureDescriptor::texture2DDescriptor(
            MTL::PixelFormatDepth32Float_Stencil8,
            window_width,
            window_height,
            false
        );

        depthDesc->setStorageMode(MTL::StorageModePrivate);
        depthDesc->setUsage(MTL::TextureUsageRenderTarget);
        context->m_MainDepthStencilTexture = context->m_Device->newTexture(depthDesc);
        depthDesc->release();

        return true;
    }

    static void MetalCloseWindow(HContext _context)
    {
        MetalContext* context = (MetalContext*) _context;

        if (dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_OPENED))
        {
        }
    }

    static void MetalFinalize()
    {

    }

    static HWindow MetalGetWindow(HContext _context)
    {
        MetalContext* context = (MetalContext*) _context;
        return context->m_BaseContext.m_Window;
    }

    static uint32_t MetalGetDisplayDpi(HContext _context)
    {
        return 0;
    }

    static uint32_t MetalGetWidth(HContext _context)
    {
        MetalContext* context = (MetalContext*) _context;
        return context->m_BaseContext.m_Width;
    }

    static uint32_t MetalGetHeight(HContext _context)
    {
        MetalContext* context = (MetalContext*) _context;
        return context->m_BaseContext.m_Height;
    }

    static void MetalSetWindowSize(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        MetalContext* context = (MetalContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_BaseContext.m_Window, width, height);
        }
    }

    static void MetalResizeWindow(HContext _context, uint32_t width, uint32_t height)
    {
        assert(_context);
        MetalContext* context = (MetalContext*) _context;
        if (dmPlatform::GetWindowStateParam(context->m_BaseContext.m_Window, WINDOW_STATE_OPENED))
        {
            dmPlatform::SetWindowSize(context->m_BaseContext.m_Window, width, height);
        }
    }

    static void MetalGetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        MetalContext* context = (MetalContext*) _context;
        out_min_filter = context->m_BaseContext.m_DefaultTextureMinFilter;
        out_mag_filter = context->m_BaseContext.m_DefaultTextureMagFilter;
    }

    static void EndRenderPass(MetalContext* context)
    {
        MetalFrameResource& frame = GetCurrentFrameResource(context);

        if (!frame.m_RenderCommandEncoder)
            return;

        frame.m_RenderCommandEncoder->endEncoding();
        frame.m_RenderCommandEncoder = 0;

        MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, context->m_CurrentRenderTarget);

        if (rt)
        {
            rt->m_IsBound = 0;
        }

        context->m_RenderTargetBound = 0;
    }

    static void FlushPendingRenderTargetClear(MetalContext* context, HRenderTarget render_target)
    {
        if (render_target == 0x0 || context->m_RenderTargetBound)
        {
            return;
        }

        bool has_pending_clear = false;
        {
            DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
            MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, render_target);
            has_pending_clear = rt && rt->m_HasPendingClearColor;
        }

        if (has_pending_clear)
        {
            BeginRenderPass(context, render_target);
            EndRenderPass(context);
        }
    }

    static void BeginRenderPass(MetalContext* context, HRenderTarget render_target)
    {
        if (context->m_CurrentRenderTarget == render_target && context->m_RenderTargetBound)
        {
            return;
        }

        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);

        MetalRenderTarget* current_rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, context->m_CurrentRenderTarget);
        MetalRenderTarget* rt         = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, render_target);

        if (!rt)
        {
            return;
        }

        if (current_rt && current_rt->m_Id == rt->m_Id && current_rt->m_IsBound)
            return;

        if (current_rt && current_rt->m_IsBound)
            EndRenderPass(context);

        // Use the frame’s command buffer
        MetalFrameResource& frame = GetCurrentFrameResource(context);
        MTL::CommandBuffer* commandBuffer = frame.m_CommandBuffer;
        assert(commandBuffer);

        // Build a render pass descriptor
        MTL::RenderPassDescriptor* rpDesc = MTL::RenderPassDescriptor::alloc()->init();
        const bool is_main_rt = render_target == context->m_MainRenderTarget;
        const bool has_pending_clear = rt->m_HasPendingClearColor;

        // --- Configure color attachments ---
        for (uint32_t i = 0; i < rt->m_ColorAttachmentCount; ++i)
        {
            if (!rt->m_TextureColor[i])
                continue;

            MetalTexture* tex = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, rt->m_TextureColor[i]);
            if (!tex || !tex->m_Texture)
                continue;

            MTL::RenderPassColorAttachmentDescriptor* colorAttachment = rpDesc->colorAttachments()->object(i);
            MTL::LoadAction load_action = MetalLoadAction(rt->m_ColorBufferLoadOps[i]);
            if (has_pending_clear)
            {
                load_action = MTL::LoadActionClear;
            }
            else if (is_main_rt && context->m_MainRTBegunThisFrame)
            {
                load_action = MTL::LoadActionLoad;
            }
            colorAttachment->setLoadAction(load_action);
            colorAttachment->setClearColor(MTL::ClearColor(
                rt->m_ColorAttachmentClearValue[i][0],
                rt->m_ColorAttachmentClearValue[i][1],
                rt->m_ColorAttachmentClearValue[i][2],
                rt->m_ColorAttachmentClearValue[i][3]));

            if (rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID && context->m_MSAASampleCount > 1)
            {
                colorAttachment->setTexture(frame.m_MSAAColorTexture);
                colorAttachment->setResolveTexture(tex->m_Texture);
                colorAttachment->setStoreAction(MTL::StoreActionMultisampleResolve);
            }
            else
            {
                colorAttachment->setTexture(tex->m_Texture);
                colorAttachment->setStoreAction(MetalStoreAction(rt->m_ColorBufferStoreOps[i]));
            }
        }

        // --- Configure depth/stencil attachment ---
        if (rt->m_TextureDepthStencil)
        {
            MetalTexture* tex = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, rt->m_TextureDepthStencil);
            if (tex && tex->m_Texture)
            {
                MTL::RenderPassDepthAttachmentDescriptor* depthAttachment = rpDesc->depthAttachment();
                depthAttachment->setLoadAction(MTL::LoadActionLoad);
                depthAttachment->setClearDepth(1.0);

                MTL::RenderPassStencilAttachmentDescriptor* stencilAttachment = rpDesc->stencilAttachment();
                stencilAttachment->setLoadAction(MTL::LoadActionLoad);
                stencilAttachment->setClearStencil(0);

                if (rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID && context->m_MSAASampleCount > 1)
                {
                    depthAttachment->setTexture(frame.m_MSAADepthTexture);
                    stencilAttachment->setTexture(frame.m_MSAADepthTexture);

                    depthAttachment->setStoreAction(MTL::StoreActionDontCare);
                    stencilAttachment->setStoreAction(MTL::StoreActionDontCare);
                }
                else
                {
                    depthAttachment->setTexture(tex->m_Texture);
                    stencilAttachment->setTexture(tex->m_Texture);

                    depthAttachment->setStoreAction(MTL::StoreActionStore);
                    stencilAttachment->setStoreAction(MTL::StoreActionStore);
                }
            }
        }

        // Create a render encoder for this render target
        MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(rpDesc);

        // Configure viewport/scissor
        const uint32_t width  = rt->m_ColorTextureParams[0].m_Width;
        const uint32_t height = rt->m_ColorTextureParams[0].m_Height;

        MTL::Viewport viewport = {0.0, 0.0, (double)width, (double)height, 0.0, 1.0};
        encoder->setViewport(viewport);

        MTL::ScissorRect scissor = {0, 0, width, height};
        encoder->setScissorRect(scissor);

        // Track the active encoder
        frame.m_RenderCommandEncoder = encoder;
        context->m_CurrentRenderTarget = render_target;
        context->m_RenderTargetBound = 1;
        if (is_main_rt)
        {
            context->m_MainRTBegunThisFrame = 1;
        }
        rt->m_HasPendingClearColor = 0;
        rt->m_IsBound = 1;

        rpDesc->release();
    }

    static void MetalBeginFrame(HContext _context)
    {
        MetalContext* context = (MetalContext*) _context;
        dispatch_semaphore_wait(context->m_FrameBoundarySemaphore, DISPATCH_TIME_FOREVER);

        MetalFrameResource& frame = GetCurrentFrameResource(context);
        assert(!frame.m_InFlight);

        frame.m_AutoReleasePool = 0;
        frame.m_Drawable = (__bridge CA::MetalDrawable*)[context->m_Layer nextDrawable];
        if (frame.m_Drawable)
        {
            frame.m_Drawable->retain();
        }
        frame.m_InFlight = 1;
        context->m_FrameBegun = 1;

        frame.m_CommandBuffer = context->m_CommandQueue->commandBuffer();
        if (frame.m_CommandBuffer)
        {
            frame.m_CommandBuffer->retain();
        }
        frame.m_ConstantScratchBuffer.Rewind();
        frame.m_ArgumentBufferPool.Rewind();
        context->m_RenderTargetBound = 0;
        context->m_MainRTBegunThisFrame = 0;

        // Setup the initial render pass state
        frame.m_RenderPassDescriptor = 0;
        frame.m_RenderCommandEncoder = 0;

        MetalRenderTarget* rt   = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, context->m_MainRenderTarget);
        MetalTexture* color_tex = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, rt->m_TextureColor[0]);
        MetalTexture* ds_tex    = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, rt->m_TextureDepthStencil);

        color_tex->m_Texture    = frame.m_Drawable->texture();
        ds_tex->m_Texture       = context->m_MainDepthStencilTexture;

        rt->m_ColorTextureParams[0].m_Width  = frame.m_Drawable->texture()->width();
        rt->m_ColorTextureParams[0].m_Height = frame.m_Drawable->texture()->height();
    }

    static void MetalCommandBufferCompleted(MetalContext* context, uint32_t frame_index)
    {
        MetalFrameResource& frame = context->m_FrameResources[frame_index];

        FlushResourcesToDestroy(context, frame.m_ResourcesToDestroy);

        if (frame.m_CommandBuffer)
        {
            frame.m_CommandBuffer->release();
        }

        if (frame.m_Drawable)
        {
            frame.m_Drawable->release();
        }

        frame.m_CommandBuffer = 0;
        frame.m_Drawable = 0;
        frame.m_RenderPassDescriptor = 0;
        frame.m_RenderCommandEncoder = 0;
        frame.m_InFlight = 0;

        dispatch_semaphore_signal(context->m_FrameBoundarySemaphore);
    }

    static void MetalFlip(HContext _context)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context->m_FrameBegun);

        FlushPendingRenderTargetClear(context, context->m_CurrentRenderTarget);

        // End the current render pass
        EndRenderPass(context);

        const uint32_t frame_index = context->m_CurrentFrameInFlight;
        MetalFrameResource& frame = context->m_FrameResources[frame_index];

        frame.m_CommandBuffer->presentDrawable(frame.m_Drawable);

        // Register completion callback
        frame.m_CommandBuffer->addCompletedHandler(^void(MTL::CommandBuffer* cb) {
            MetalCommandBufferCompleted(context, frame_index);
        });

        frame.m_CommandBuffer->commit();

        context->m_CurrentFrameInFlight = (context->m_CurrentFrameInFlight + 1) % context->m_NumFramesInFlight;
        context->m_FrameBegun = 0;
    }

    static void MetalClear(HContext _context, uint32_t flags,
                           uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha,
                           float depth, uint32_t stencil)
    {
        MetalContext* context = (MetalContext*) _context;
        MetalRenderTarget* current_rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, context->m_CurrentRenderTarget);
        assert(current_rt);

        // Determine which buffers to clear
        const BufferType color_buffers[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        const uint32_t any_color_clear_mask = BUFFER_TYPE_COLOR0_BIT | BUFFER_TYPE_COLOR1_BIT | BUFFER_TYPE_COLOR2_BIT | BUFFER_TYPE_COLOR3_BIT;
        bool want_color                     = (flags & any_color_clear_mask) != 0;
        bool want_depth                     = (flags & BUFFER_TYPE_DEPTH_BIT) != 0;
        bool want_stencil                   = (flags & BUFFER_TYPE_STENCIL_BIT) != 0;
        bool pass_not_bound                 = !context->m_RenderTargetBound;
        bool rt_has_ds                      = current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID || current_rt->m_TextureDepthStencil != 0;
        bool clear_ds                       = rt_has_ds && (want_depth || want_stencil);
        bool all_colors_in_flags            = current_rt->m_ColorAttachmentCount > 0;

        for (int i = 0; i < current_rt->m_ColorAttachmentCount; ++i)
        {
            if (!(flags & color_buffers[i]))
            {
                all_colors_in_flags = false;
                break;
            }
        }

        if (pass_not_bound && want_color && all_colors_in_flags && !clear_ds)
        {
            const float r = (float) red   / 255.0f;
            const float g = (float) green / 255.0f;
            const float b = (float) blue  / 255.0f;
            const float a = (float) alpha / 255.0f;

            for (int i = 0; i < current_rt->m_ColorAttachmentCount; ++i)
            {
                current_rt->m_ColorAttachmentClearValue[i][0] = r;
                current_rt->m_ColorAttachmentClearValue[i][1] = g;
                current_rt->m_ColorAttachmentClearValue[i][2] = b;
                current_rt->m_ColorAttachmentClearValue[i][3] = a;
            }
            current_rt->m_HasPendingClearColor = 1;
            return;
        }

        BeginRenderPass(context, context->m_CurrentRenderTarget);

        // BeginRenderPass must have already bound the RT and created a MTLRenderCommandEncoder
        MetalFrameResource& frame = GetCurrentFrameResource(context);
        MTL::RenderCommandEncoder* enc = frame.m_RenderCommandEncoder;
        assert(enc);

        // Build cache key
        MetalClearData::CacheKey key = {};
        key.m_ClearColor             = want_color;
        key.m_DepthStencilFormat     = current_rt->m_DepthStencilFormat;
        key.m_ColorAttachmentCount   = current_rt->m_ColorAttachmentCount;
        key.m_ClearDepth             = want_depth && current_rt->m_DepthStencilFormat != MTL::PixelFormatInvalid;
        key.m_ClearStencil           = want_stencil && current_rt->m_DepthStencilFormat != MTL::PixelFormatInvalid;
        key.m_ColorWriteMaskBits     = 0;
        key.m_SampleCount            = current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID ? context->m_MSAASampleCount : 1;

        for (uint32_t i = 0; i < key.m_ColorAttachmentCount; ++i)
        {
            key.m_ColorFormats[i] = current_rt->m_ColorFormat[i];

            if (flags & color_buffers[i])
            {
                key.m_ColorWriteMaskBits |= (1 << i);
            }
        }

        // Acquire pipeline
        MetalPipeline* pipeline = GetOrCreateClearPipeline(context, key);
        assert(pipeline);

        enc->setRenderPipelineState(pipeline->m_RenderPipelineState);
        enc->setDepthStencilState(pipeline->m_DepthStencilState);

        ClearParams clear_params = {};
        clear_params.m_ClearColor[0] = (float)red   / 255.0f;
        clear_params.m_ClearColor[1] = (float)green / 255.0f;
        clear_params.m_ClearColor[2] = (float)blue  / 255.0f;
        clear_params.m_ClearColor[3] = (float)alpha / 255.0f;
        clear_params.m_ClearDepth    = depth;
        clear_params.m_ClearStencil  = stencil;

        enc->setFragmentBytes(&clear_params, sizeof(clear_params), 0);

         // If writing stencil, set the stencil reference value to the desired replacement value
        if (want_stencil)
        {
            enc->setStencilReferenceValue((uint32_t) stencil);
        }
        else
        {
            // If not writing stencil, reference value doesn't matter; still set to 0 to be explicit
            enc->setStencilReferenceValue(0);
        }

        // Draw fullscreen triangle
        enc->setCullMode(MTL::CullModeNone);
        enc->drawPrimitives(MTL::PrimitiveTypeTriangle, (uint32_t) 0, (uint32_t) 3);

        // Refresh culling after this call
        context->m_CullFaceChanged = true;
    }

    static HVertexBuffer MetalNewVertexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        MetalContext* context = (MetalContext*) _context;
        MetalDeviceBuffer* buffer = new MetalDeviceBuffer();

        /*
        switch (buffer_usage)
        {
            case BUFFER_USAGE_STATIC_DRAW:
                buffer->m_StorageMode = MTL::StorageModePrivate;
                break;
            case BUFFER_USAGE_DYNAMIC_DRAW:
                buffer->m_StorageMode = MTL::StorageModeShared;
                break;
            default:
                buffer->m_StorageMode = MTL::StorageModeShared;
                break;
        }
        */
        buffer->m_StorageMode = MTL::StorageModeShared;

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, data, size, 0, buffer);
        }

        return (HVertexBuffer) buffer;
    }

    static void MetalDeleteVertexBuffer(HVertexBuffer _buffer)
    {
        MetalDeviceBuffer* buffer = (MetalDeviceBuffer*) _buffer;

        if (!buffer->m_Destroyed)
        {
            DestroyResourceDeferred(g_MetalContext, buffer);
        }
        delete buffer;
    }

    static void SetDeviceBuffer(MetalContext* context, MetalDeviceBuffer* buffer, uint32_t size, const void* data)
    {
        if (size == 0)
        {
            return;
        }
        if (size != buffer->m_Size)
        {
            DestroyResourceDeferred(context, buffer);
        }

        DeviceBufferUploadHelper(context, data, size, 0, buffer);
    }

    static HUniformBuffer MetalNewUniformBuffer(HContext _context, const UniformBufferLayout& layout)
    {
        MetalContext* context      = (MetalContext*) _context;
        MetalUniformBuffer* ubo    = new MetalUniformBuffer();
        memset(ubo, 0, sizeof(MetalUniformBuffer));
        ubo->m_BaseUniformBuffer.m_Layout       = layout;
        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;
        ubo->m_DeviceBuffer.m_StorageMode       = MTL::StorageModeShared;

        if (layout.m_Size > 0)
        {
            DeviceBufferUploadHelper(context, 0, layout.m_Size, 0, &ubo->m_DeviceBuffer);
        }

        return (HUniformBuffer) ubo;
    }

    static void MetalSetUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t offset, uint32_t size, const void* data)
    {
        MetalContext* context       = (MetalContext*) _context;
        MetalUniformBuffer* ubo     = (MetalUniformBuffer*) uniform_buffer;
        assert(offset + size <= ubo->m_BaseUniformBuffer.m_Layout.m_Size);

        if (ubo->m_DeviceBuffer.m_Buffer == 0x0)
        {
            ubo->m_DeviceBuffer.m_StorageMode = MTL::StorageModeShared;
            DeviceBufferUploadHelper(context, 0, ubo->m_BaseUniformBuffer.m_Layout.m_Size, 0, &ubo->m_DeviceBuffer);
        }

        memcpy(reinterpret_cast<uint8_t*>(ubo->m_DeviceBuffer.m_Buffer->contents()) + offset, data, size);
    }

    static void MetalDisableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        MetalContext* context       = (MetalContext*) _context;
        MetalUniformBuffer* ubo     = (MetalUniformBuffer*) uniform_buffer;

        if (ubo->m_BaseUniformBuffer.m_BoundSet == UNUSED_BINDING_OR_SET || ubo->m_BaseUniformBuffer.m_BoundBinding == UNUSED_BINDING_OR_SET)
        {
            return;
        }

        if (context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] == ubo)
        {
            context->m_CurrentUniformBuffers[ubo->m_BaseUniformBuffer.m_BoundSet][ubo->m_BaseUniformBuffer.m_BoundBinding] = 0;
        }

        ubo->m_BaseUniformBuffer.m_BoundSet     = UNUSED_BINDING_OR_SET;
        ubo->m_BaseUniformBuffer.m_BoundBinding = UNUSED_BINDING_OR_SET;
    }

    static void MetalEnableUniformBuffer(HContext _context, HUniformBuffer uniform_buffer, uint32_t binding, uint32_t set)
    {
        MetalContext* context       = (MetalContext*) _context;
        MetalUniformBuffer* ubo     = (MetalUniformBuffer*) uniform_buffer;

        ubo->m_BaseUniformBuffer.m_BoundBinding = binding;
        ubo->m_BaseUniformBuffer.m_BoundSet     = set;

        if (context->m_CurrentUniformBuffers[set][binding])
        {
            MetalDisableUniformBuffer(_context, (HUniformBuffer) context->m_CurrentUniformBuffers[set][binding]);
        }

        context->m_CurrentUniformBuffers[set][binding] = ubo;
    }

    static void MetalDeleteUniformBuffer(HContext _context, HUniformBuffer uniform_buffer)
    {
        MetalContext* context       = (MetalContext*) _context;
        MetalUniformBuffer* ubo     = (MetalUniformBuffer*) uniform_buffer;

        MetalDisableUniformBuffer(_context, uniform_buffer);

        if (!ubo->m_DeviceBuffer.m_Destroyed)
        {
            DestroyResourceDeferred(context, &ubo->m_DeviceBuffer);
        }

        delete ubo;
    }

    static void MetalSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);
        SetDeviceBuffer(g_MetalContext, (MetalDeviceBuffer*) buffer, size, data);
    }

    static void MetalSetVertexBufferSubData(HVertexBuffer _buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        assert(size > 0);
        MetalDeviceBuffer* buffer = (MetalDeviceBuffer*) _buffer;
        assert(offset + size <= buffer->m_Size);
        DeviceBufferUploadHelper(g_MetalContext, data, size, offset, buffer);
    }

    static uint32_t MetalGetVertexBufferSize(HVertexBuffer _buffer)
    {
        if (!_buffer)
        {
            return 0;
        }
        MetalDeviceBuffer* buffer = (MetalDeviceBuffer*) _buffer;
        return buffer->m_Size;
    }

    static uint32_t MetalGetMaxElementsVertices(HContext _context)
    {
        return 65536;
    }

    static HIndexBuffer MetalNewIndexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        MetalContext* context = (MetalContext*) _context;
        MetalDeviceBuffer* buffer = new MetalDeviceBuffer();

        /*
        switch (buffer_usage)
        {
            case BUFFER_USAGE_STATIC_DRAW:
                buffer->m_StorageMode = MTL::StorageModePrivate;
                break;
            case BUFFER_USAGE_DYNAMIC_DRAW:
                buffer->m_StorageMode = MTL::StorageModeShared;
                break;
            default:
                buffer->m_StorageMode = MTL::StorageModeShared;
                break;
        }
        */

        buffer->m_StorageMode = MTL::StorageModeShared;

        if (size > 0)
        {
            DeviceBufferUploadHelper(context, data, size, 0, buffer);
        }

        return (HIndexBuffer) buffer;
    }

    static void MetalDeleteIndexBuffer(HIndexBuffer _buffer)
    {
        MetalDeviceBuffer* buffer = (MetalDeviceBuffer*) _buffer;

        if (!buffer->m_Destroyed)
        {
            DestroyResourceDeferred(g_MetalContext, buffer);
        }
        delete buffer;
    }

    static void MetalSetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        DM_PROFILE(__FUNCTION__);
        SetDeviceBuffer(g_MetalContext, (MetalDeviceBuffer*) buffer, size, data);
    }

    static void MetalSetIndexBufferSubData(HIndexBuffer _buffer, uint32_t offset, uint32_t size, const void* data)
    {
        DM_PROFILE(__FUNCTION__);
        assert(size > 0);
        MetalDeviceBuffer* buffer = (MetalDeviceBuffer*) _buffer;
        assert(offset + size <= buffer->m_Size);
        DeviceBufferUploadHelper(g_MetalContext, data, size, offset, buffer);
    }

    static uint32_t MetalGetIndexBufferSize(HIndexBuffer _buffer)
    {
        if (!_buffer)
        {
            return 0;
        }
        MetalDeviceBuffer* buffer = (MetalDeviceBuffer*) _buffer;
        return buffer->m_Size;
    }

    static bool MetalIsIndexBufferFormatSupported(HContext _context, IndexBufferFormat format)
    {
        return true;
    }

    static uint32_t MetalGetMaxElementsIndices(HContext _context)
    {
        return -1;
    }

    static VertexDeclaration* CreateAndFillVertexDeclaration(HashState64* hash, HVertexStreamDeclaration stream_declaration)
    {
        VertexDeclaration* vd = new VertexDeclaration();
        memset(vd, 0, sizeof(VertexDeclaration));

        uint32_t stream_count = stream_declaration->m_Streams.Size();
        vd->m_StreamCount = stream_count;
        vd->m_Streams     = new VertexDeclaration::Stream[stream_count];

        for (uint32_t i = 0; i < stream_count; ++i)
        {
            VertexStream& stream = stream_declaration->m_Streams[i];

            if (stream.m_Type == TYPE_UNSIGNED_BYTE && !stream.m_Normalize)
            {
                dmLogWarning("Using the type '%s' for stream '%s' with normalize: false is not supported for vertex declarations. Defaulting to TYPE_BYTE.", GetGraphicsTypeLiteral(stream.m_Type), dmHashReverseSafe64(stream.m_NameHash));
                stream.m_Type = TYPE_BYTE;
            }
            else if (stream.m_Type == TYPE_UNSIGNED_SHORT && !stream.m_Normalize)
            {
                dmLogWarning("Using the type '%s' for stream '%s' with normalize: false is not supported for vertex declarations. Defaulting to TYPE_SHORT.", GetGraphicsTypeLiteral(stream.m_Type), dmHashReverseSafe64(stream.m_NameHash));
                stream.m_Type = TYPE_SHORT;
            }

            vd->m_Streams[i].m_NameHash  = stream.m_NameHash;
            vd->m_Streams[i].m_Type      = stream.m_Type;
            vd->m_Streams[i].m_Size      = stream.m_Size;
            vd->m_Streams[i].m_Normalize = stream.m_Normalize;
            vd->m_Streams[i].m_Offset    = vd->m_Stride;
            vd->m_Streams[i].m_Location  = -1;
            vd->m_Stride                += stream.m_Size * GetTypeSize(stream.m_Type);

            dmHashUpdateBuffer64(hash, &stream.m_Size, sizeof(stream.m_Size));
            dmHashUpdateBuffer64(hash, &stream.m_Type, sizeof(stream.m_Type));
            dmHashUpdateBuffer64(hash, &vd->m_Streams[i].m_Type, sizeof(vd->m_Streams[i].m_Type));
        }

        vd->m_Stride       = DM_ALIGN(vd->m_Stride, 4);
        vd->m_StepFunction = stream_declaration->m_StepFunction;

        return vd;
    }

    static HVertexDeclaration MetalNewVertexDeclaration(HContext _context, HVertexStreamDeclaration stream_declaration)
    {
        DM_PROFILE(__FUNCTION__);
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
        dmHashUpdateBuffer64(&decl_hash_state, &vd->m_Stride, sizeof(vd->m_Stride));
        vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
        return vd;
    }

    static HVertexDeclaration MetalNewVertexDeclarationStride(HContext _context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        HashState64 decl_hash_state;
        dmHashInit64(&decl_hash_state, false);
        VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
        dmHashUpdateBuffer64(&decl_hash_state, &stride, sizeof(stride));
        vd->m_Stride       = stride;
        vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
        return vd;
    }

    static void MetalEnableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        MetalContext* context     = (MetalContext*) _context;
        MetalProgram* program_ptr = (MetalProgram*) program;

        context->m_MainVertexDeclaration[binding_index]                = {};
        context->m_MainVertexDeclaration[binding_index].m_Stride       = vertex_declaration->m_Stride;
        context->m_MainVertexDeclaration[binding_index].m_StepFunction = vertex_declaration->m_StepFunction;
        context->m_MainVertexDeclaration[binding_index].m_PipelineHash = vertex_declaration->m_PipelineHash;

        context->m_CurrentVertexDeclaration[binding_index]             = &context->m_MainVertexDeclaration[binding_index];
        context->m_CurrentVertexBufferOffset[binding_index]            = base_offset;

        if (context->m_MainVertexDeclarationStreams[binding_index].Size() < vertex_declaration->m_StreamCount)
        {
            context->m_MainVertexDeclarationStreams[binding_index].SetCapacity(vertex_declaration->m_StreamCount);
            context->m_MainVertexDeclarationStreams[binding_index].SetSize(vertex_declaration->m_StreamCount);
        }

        memset(context->m_MainVertexDeclarationStreams[binding_index].Begin(), 0, sizeof(VertexDeclaration::Stream) * vertex_declaration->m_StreamCount);
        context->m_MainVertexDeclaration[binding_index].m_Streams = context->m_MainVertexDeclarationStreams[binding_index].Begin();

        uint32_t stream_ix = 0;
        uint32_t num_inputs = program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs.Size();

        for (int i = 0; i < vertex_declaration->m_StreamCount; ++i)
        {
            for (int j = 0; j < num_inputs; ++j)
            {
                ShaderResourceBinding& input = program_ptr->m_BaseProgram.m_ShaderMeta.m_Inputs[j];

                if (input.m_StageFlags & SHADER_STAGE_FLAG_VERTEX && input.m_NameHash == vertex_declaration->m_Streams[i].m_NameHash)
                {
                    VertexDeclaration::Stream& stream = context->m_MainVertexDeclaration[binding_index].m_Streams[stream_ix];
                    stream.m_NameHash  = input.m_NameHash;
                    stream.m_Location  = input.m_Binding;
                    stream.m_Type      = vertex_declaration->m_Streams[i].m_Type;
                    stream.m_Offset    = vertex_declaration->m_Streams[i].m_Offset;
                    stream.m_Size      = vertex_declaration->m_Streams[i].m_Size;
                    stream.m_Normalize = vertex_declaration->m_Streams[i].m_Normalize;
                    stream_ix++;

                    context->m_MainVertexDeclaration[binding_index].m_StreamCount++;
                    break;
                }
            }
        }
    }

    static void MetalDisableVertexDeclaration(HContext _context, HVertexDeclaration vertex_declaration)
    {
        MetalContext* context = (MetalContext*) _context;
        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexDeclaration[i] == vertex_declaration)
            {
                context->m_CurrentVertexDeclaration[i]  = 0;
                context->m_CurrentVertexBufferOffset[i] = 0;
            }
        }
    }

    static void MetalEnableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer, uint32_t binding_index)
    {
        MetalContext* context = (MetalContext*) _context;
        context->m_CurrentVertexBuffer[binding_index] = (MetalDeviceBuffer*) vertex_buffer;
    }

    static void MetalDisableVertexBuffer(HContext _context, HVertexBuffer vertex_buffer)
    {
        MetalContext* context = (MetalContext*) _context;
        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i] == (MetalDeviceBuffer*) vertex_buffer)
            {
                context->m_CurrentVertexBuffer[i] = 0;
            }
        }
    }

    static inline MTL::VertexStepFunction ConvertStepFunction(VertexStepFunction step)
    {
        switch (step)
        {
            case VERTEX_STEP_FUNCTION_VERTEX:   return MTL::VertexStepFunctionPerVertex;
            case VERTEX_STEP_FUNCTION_INSTANCE: return MTL::VertexStepFunctionPerInstance;
            default:                            return MTL::VertexStepFunctionPerVertex;
        }
    }

    static inline MTL::VertexFormat ConvertVertexFormat(Type type, uint16_t size, bool normalized)
    {
        if (type == TYPE_FLOAT)
        {
            switch (size)
            {
                case 1:  return MTL::VertexFormatFloat;
                case 2:  return MTL::VertexFormatFloat2;
                case 3:  return MTL::VertexFormatFloat3;
                case 4:  return MTL::VertexFormatFloat4;
                case 9:  return MTL::VertexFormatFloat3;  // Mat3 fallback
                case 16: return MTL::VertexFormatFloat4;  // Mat4 fallback
                default: break;
            }
        }
        else if (type == TYPE_INT)
        {
            switch (size)
            {
                case 1:  return MTL::VertexFormatInt;
                case 2:  return MTL::VertexFormatInt2;
                case 3:  return MTL::VertexFormatInt3;
                case 4:  return MTL::VertexFormatInt4;
                case 9:  return MTL::VertexFormatInt3;
                case 16: return MTL::VertexFormatInt4;
                default: break;
            }
        }
        else if (type == TYPE_UNSIGNED_INT)
        {
            switch (size)
            {
                case 1:  return MTL::VertexFormatUInt;
                case 2:  return MTL::VertexFormatUInt2;
                case 3:  return MTL::VertexFormatUInt3;
                case 4:  return MTL::VertexFormatUInt4;
                case 9:  return MTL::VertexFormatUInt3;
                case 16: return MTL::VertexFormatUInt4;
                default: break;
            }
        }
        else if (type == TYPE_BYTE)
        {
            switch (size)
            {
                case 1:  return normalized ? MTL::VertexFormatCharNormalized   : MTL::VertexFormatChar;
                case 2:  return normalized ? MTL::VertexFormatChar2Normalized  : MTL::VertexFormatChar2;
                case 3:  return normalized ? MTL::VertexFormatChar3Normalized  : MTL::VertexFormatChar3;
                case 4:  return normalized ? MTL::VertexFormatChar4Normalized  : MTL::VertexFormatChar4;
                case 9:  return normalized ? MTL::VertexFormatChar3Normalized  : MTL::VertexFormatChar3;
                case 16: return normalized ? MTL::VertexFormatChar4Normalized  : MTL::VertexFormatChar4;
                default: break;
            }
        }
        else if (type == TYPE_UNSIGNED_BYTE)
        {
            switch (size)
            {
                case 1:  return normalized ? MTL::VertexFormatUCharNormalized   : MTL::VertexFormatUChar;
                case 2:  return normalized ? MTL::VertexFormatUChar2Normalized  : MTL::VertexFormatUChar2;
                case 3:  return normalized ? MTL::VertexFormatUChar3Normalized  : MTL::VertexFormatUChar3;
                case 4:  return normalized ? MTL::VertexFormatUChar4Normalized  : MTL::VertexFormatUChar4;
                case 9:  return normalized ? MTL::VertexFormatUChar3Normalized  : MTL::VertexFormatUChar3;
                case 16: return normalized ? MTL::VertexFormatUChar4Normalized  : MTL::VertexFormatUChar4;
                default: break;
            }
        }
        else if (type == TYPE_SHORT)
        {
            switch (size)
            {
                case 1:  return normalized ? MTL::VertexFormatShortNormalized   : MTL::VertexFormatShort;
                case 2:  return normalized ? MTL::VertexFormatShort2Normalized  : MTL::VertexFormatShort2;
                case 3:  return normalized ? MTL::VertexFormatShort3Normalized  : MTL::VertexFormatShort3;
                case 4:  return normalized ? MTL::VertexFormatShort4Normalized  : MTL::VertexFormatShort4;
                case 9:  return normalized ? MTL::VertexFormatShort3Normalized  : MTL::VertexFormatShort3;
                case 16: return normalized ? MTL::VertexFormatShort4Normalized  : MTL::VertexFormatShort4;
                default: break;
            }
        }
        else if (type == TYPE_UNSIGNED_SHORT)
        {
            switch (size)
            {
                case 1:  return normalized ? MTL::VertexFormatUShortNormalized   : MTL::VertexFormatUShort;
                case 2:  return normalized ? MTL::VertexFormatUShort2Normalized  : MTL::VertexFormatUShort2;
                case 3:  return normalized ? MTL::VertexFormatUShort3Normalized  : MTL::VertexFormatUShort3;
                case 4:  return normalized ? MTL::VertexFormatUShort4Normalized  : MTL::VertexFormatUShort4;
                case 9:  return normalized ? MTL::VertexFormatUShort3Normalized  : MTL::VertexFormatUShort3;
                case 16: return normalized ? MTL::VertexFormatUShort4Normalized  : MTL::VertexFormatUShort4;
                default: break;
            }
        }
        else if (type == TYPE_FLOAT_MAT4 || type == TYPE_FLOAT_MAT3 || type == TYPE_FLOAT_MAT2)
        {
            // Metal doesn't have matrix vertex formats.
            // Typically you expand these into multiple attributes.
            return MTL::VertexFormatFloat4;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return MTL::VertexFormatFloat4;
        }

        assert(0 && "Unable to deduce Metal vertex format from dmGraphics::Type");
        return MTL::VertexFormatInvalid;
    }

    static inline MTL::BlendFactor GetMetalBlendFactor(BlendFactor factor)
    {
        const MTL::BlendFactor blend_factors[] = {
            MTL::BlendFactorZero,
            MTL::BlendFactorOne,
            MTL::BlendFactorSourceColor,
            MTL::BlendFactorOneMinusSourceColor,
            MTL::BlendFactorDestinationColor,
            MTL::BlendFactorOneMinusDestinationColor,
            MTL::BlendFactorSourceAlpha,
            MTL::BlendFactorOneMinusSourceAlpha,
            MTL::BlendFactorDestinationAlpha,
            MTL::BlendFactorOneMinusDestinationAlpha,
            MTL::BlendFactorSourceAlphaSaturated,
        };
        return blend_factors[(int)factor];
    }

    static inline MTL::BlendOperation GetMetalBlendOperation(BlendEquation equation)
    {
        switch (equation)
        {
            case BLEND_EQUATION_ADD:              return MTL::BlendOperationAdd;
            case BLEND_EQUATION_SUBTRACT:         return MTL::BlendOperationSubtract;
            case BLEND_EQUATION_REVERSE_SUBTRACT: return MTL::BlendOperationReverseSubtract;
            case BLEND_EQUATION_MIN:              return MTL::BlendOperationMin;
            case BLEND_EQUATION_MAX:              return MTL::BlendOperationMax;
            default: break;
        }

        return MTL::BlendOperationAdd;
    }

    static inline MTL::ColorWriteMask GetMetalColorWriteMask(uint8_t write_mask)
    {
        MTL::ColorWriteMask metal_write_mask = MTL::ColorWriteMaskNone;
        metal_write_mask |= (write_mask & DM_GRAPHICS_STATE_WRITE_R) ? MTL::ColorWriteMaskRed   : MTL::ColorWriteMaskNone;
        metal_write_mask |= (write_mask & DM_GRAPHICS_STATE_WRITE_G) ? MTL::ColorWriteMaskGreen : MTL::ColorWriteMaskNone;
        metal_write_mask |= (write_mask & DM_GRAPHICS_STATE_WRITE_B) ? MTL::ColorWriteMaskBlue  : MTL::ColorWriteMaskNone;
        metal_write_mask |= (write_mask & DM_GRAPHICS_STATE_WRITE_A) ? MTL::ColorWriteMaskAlpha : MTL::ColorWriteMaskNone;
        return metal_write_mask;
    }

    static inline MTL::CompareFunction GetMetalDepthTestFunc(CompareFunc func)
    {
        const MTL::CompareFunction compare_funcs[] = {
            MTL::CompareFunctionNever,
            MTL::CompareFunctionLess,
            MTL::CompareFunctionLessEqual,
            MTL::CompareFunctionGreater,
            MTL::CompareFunctionGreaterEqual,
            MTL::CompareFunctionEqual,
            MTL::CompareFunctionNotEqual,
            MTL::CompareFunctionAlways
        };
        return compare_funcs[(int)func];
    }

    static bool CreatePipeline(MetalContext* context, MetalRenderTarget* rt, const PipelineState pipeline_state,  MetalProgram* program, VertexDeclaration** vertexDeclaration, uint32_t vertexDeclarationCount, MetalPipeline* pipeline)
    {
        MTL::VertexDescriptor* vertex_desc = MTL::VertexDescriptor::alloc()->init();
        uint32_t vx_buffer_start_ix = program->m_BaseProgram.m_MaxBinding;

        uint32_t sample_count = rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID ? context->m_MSAASampleCount : 1;

        for (uint32_t buffer_index = 0; buffer_index < vertexDeclarationCount; ++buffer_index)
        {
            VertexDeclaration* vd = vertexDeclaration[buffer_index];

            for (uint32_t s = 0; s < vd->m_StreamCount; ++s)
            {
                const VertexDeclaration::Stream& stream = vd->m_Streams[s];

                // Use the shader location (stream.m_Location) as the attribute index
                uint32_t attrIndex = stream.m_Location;
                MTL::VertexAttributeDescriptor* attr = vertex_desc->attributes()->object(attrIndex);

                attr->setFormat(ConvertVertexFormat(stream.m_Type, stream.m_Size, stream.m_Normalize));
                attr->setOffset(stream.m_Offset);
                attr->setBufferIndex(buffer_index + vx_buffer_start_ix);
            }

            // One layout per vertex buffer
            MTL::VertexBufferLayoutDescriptor* layout = vertex_desc->layouts()->object(buffer_index + vx_buffer_start_ix);

            layout->setStride(vd->m_Stride);
            layout->setStepFunction(ConvertStepFunction(vd->m_StepFunction));
            layout->setStepRate(1);
        }

        MTL::RenderPipelineDescriptor* pipeline_desc = MTL::RenderPipelineDescriptor::alloc()->init();
        pipeline_desc->setVertexFunction(program->m_VertexModule->m_Function);
        pipeline_desc->setFragmentFunction(program->m_FragmentModule->m_Function);
        pipeline_desc->setVertexDescriptor(vertex_desc);
        pipeline_desc->setSampleCount(sample_count);

        for (uint32_t i = 0; i < rt->m_ColorAttachmentCount; ++i)
        {
            MTL::RenderPipelineColorAttachmentDescriptor* colorAttachment = pipeline_desc->colorAttachments()->object(i);
            colorAttachment->setPixelFormat(rt->m_ColorFormat[i]);
            colorAttachment->setWriteMask(GetMetalColorWriteMask(pipeline_state.m_WriteColorMask));
            colorAttachment->setBlendingEnabled(pipeline_state.m_BlendEnabled);

            if (pipeline_state.m_BlendEnabled)
            {
                colorAttachment->setSourceRGBBlendFactor(GetMetalBlendFactor((BlendFactor) pipeline_state.m_BlendSrcFactor));
                colorAttachment->setDestinationRGBBlendFactor(GetMetalBlendFactor((BlendFactor) pipeline_state.m_BlendDstFactor));
                colorAttachment->setRgbBlendOperation(GetMetalBlendOperation((BlendEquation) pipeline_state.m_BlendEquationColor));
                colorAttachment->setSourceAlphaBlendFactor(GetMetalBlendFactor((BlendFactor) pipeline_state.m_BlendSrcFactor));
                colorAttachment->setDestinationAlphaBlendFactor(GetMetalBlendFactor((BlendFactor) pipeline_state.m_BlendDstFactor));
                colorAttachment->setAlphaBlendOperation(GetMetalBlendOperation((BlendEquation) pipeline_state.m_BlendEquationAlpha));
            }
        }
        pipeline_desc->setDepthAttachmentPixelFormat(rt->m_DepthStencilFormat);
        pipeline_desc->setStencilAttachmentPixelFormat(rt->m_DepthStencilFormat);

        NS::Error* error = nullptr;
        pipeline->m_RenderPipelineState = context->m_Device->newRenderPipelineState(pipeline_desc, &error);

        if (!pipeline->m_RenderPipelineState)
        {
            dmLogError("Failed to create Metal pipeline: %s", error ? error->localizedDescription()->utf8String() : "Unknown error");
            pipeline_desc->release();
            vertex_desc->release();
            return false;
        }

        if (rt->m_DepthStencilFormat != MTL::PixelFormatInvalid)
        {
            MTL::DepthStencilDescriptor* ds = MTL::DepthStencilDescriptor::alloc()->init();

            if (pipeline_state.m_DepthTestEnabled)
            {
                ds->setDepthCompareFunction(GetMetalDepthTestFunc((CompareFunc) pipeline_state.m_DepthTestFunc));
                ds->setDepthWriteEnabled(pipeline_state.m_WriteDepth);   
            }
            else
            {
                ds->setDepthCompareFunction(MTL::CompareFunctionAlways);
                ds->setDepthWriteEnabled(false);
            }
            pipeline->m_DepthStencilState = context->m_Device->newDepthStencilState(ds);
            ds->release();
        }

        pipeline_desc->release();
        vertex_desc->release();

        return true;
    }

    static bool CreateComputePipeline(MetalContext* context, MetalProgram* program, MetalPipeline* pipeline)
    {
        NS::Error* error = NULL;
        pipeline->m_ComputePipelineState = context->m_Device->newComputePipelineState(program->m_ComputeModule->m_Function, &error);
        if (!pipeline->m_ComputePipelineState)
        {
            dmLogError("Failed to create Metal compute pipeline: %s", error ? error->localizedDescription()->utf8String() : "Unknown error");
            return false;
        }
        return true;
    }

    static MetalPipeline* GetOrCreateComputePipeline(MetalContext* context, MetalProgram* program)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_Hash, sizeof(program->m_Hash));
        uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);

        MetalPipeline* cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            MetalPipeline new_pipeline = {};

            if (!CreateComputePipeline(context, program, &new_pipeline))
            {
                return 0;
            }

            if (context->m_PipelineCache.Full())
            {
                context->m_PipelineCache.SetCapacity(32, context->m_PipelineCache.Capacity() + 4);
            }

            context->m_PipelineCache.Put(pipeline_hash, new_pipeline);
            cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);
        }

        return cached_pipeline;
    }

    static MetalPipeline* GetOrCreatePipeline(MetalContext* context, const PipelineState pipeline_state, MetalProgram* program, MetalRenderTarget* rt, VertexDeclaration** vertexDeclaration, uint32_t vertexDeclarationCount)
    {
        HashState64 pipeline_hash_state;
        dmHashInit64(&pipeline_hash_state, false);
        dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_Hash, sizeof(program->m_Hash));
        dmHashUpdateBuffer64(&pipeline_hash_state, &pipeline_state, sizeof(pipeline_state));
        dmHashUpdateBuffer64(&pipeline_hash_state, &rt->m_Id, sizeof(rt->m_Id));

        for (int i = 0; i < vertexDeclarationCount; ++i)
        {
            dmHashUpdateBuffer64(&pipeline_hash_state, &vertexDeclaration[i]->m_PipelineHash, sizeof(vertexDeclaration[i]->m_PipelineHash));
            dmHashUpdateBuffer64(&pipeline_hash_state, &vertexDeclaration[i]->m_StepFunction, sizeof(vertexDeclaration[i]->m_StepFunction));
        }

        uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);

        MetalPipeline* cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);

        if (!cached_pipeline)
        {
            MetalPipeline new_pipeline = {};

            if (!CreatePipeline(context, rt, pipeline_state, program, vertexDeclaration, vertexDeclarationCount, &new_pipeline))
            {
                return 0;
            }

            if (context->m_PipelineCache.Full())
            {
                context->m_PipelineCache.SetCapacity(32, context->m_PipelineCache.Capacity() + 4);
            }

            context->m_PipelineCache.Put(pipeline_hash, new_pipeline);
            cached_pipeline = context->m_PipelineCache.Get(pipeline_hash);
        }

        return cached_pipeline;
    }

    static inline MTL::PixelFormat GetMetalPixelFormat(TextureFormat format)
    {
        switch (format)
        {
            case TEXTURE_FORMAT_LUMINANCE:         return MTL::PixelFormatR8Unorm;
            case TEXTURE_FORMAT_LUMINANCE_ALPHA:   return MTL::PixelFormatRG8Unorm;
            case TEXTURE_FORMAT_RGB:               return MTL::PixelFormatRGBA8Unorm; // expand RGB to RGBA
            case TEXTURE_FORMAT_RGBA:              return MTL::PixelFormatRGBA8Unorm;
            case TEXTURE_FORMAT_RGB_16BPP:         return MTL::PixelFormatB5G6R5Unorm; // closest 16-bit
            //case TEXTURE_FORMAT_RGBA_16BPP:        return MTL::PixelFormatRGBA4Unorm;
            case TEXTURE_FORMAT_DEPTH:             return MTL::PixelFormatInvalid;
            case TEXTURE_FORMAT_STENCIL:           return MTL::PixelFormatInvalid;

            // PVRTC
            case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:  return MTL::PixelFormatPVRTC_RGB_2BPP;
            case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:  return MTL::PixelFormatPVRTC_RGB_4BPP;
            case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1: return MTL::PixelFormatPVRTC_RGBA_2BPP;
            case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1: return MTL::PixelFormatPVRTC_RGBA_4BPP;

            // ETC2
            case TEXTURE_FORMAT_RGB_ETC1:          return MTL::PixelFormatETC2_RGB8;
            //case TEXTURE_FORMAT_RGBA_ETC2:         return MTL::PixelFormatETC2_RGBA8;

            // BC / DXT (macOS only)
            case TEXTURE_FORMAT_RGB_BC1:           return MTL::PixelFormatBC1_RGBA;
            case TEXTURE_FORMAT_RGBA_BC3:          return MTL::PixelFormatBC3_RGBA;
            //case TEXTURE_FORMAT_RGBA_BC7:          return MTL::PixelFormatBC7_RGBA;
            case TEXTURE_FORMAT_R_BC4:             return MTL::PixelFormatBC4_RUnorm;
            case TEXTURE_FORMAT_RG_BC5:            return MTL::PixelFormatBC5_RGUnorm;

            // Floating point
            case TEXTURE_FORMAT_RGB16F:            return MTL::PixelFormatRGBA16Float; // expand RGB -> RGBA
            case TEXTURE_FORMAT_RGB32F:            return MTL::PixelFormatRGBA32Float; // expand RGB -> RGBA
            case TEXTURE_FORMAT_RGBA16F:           return MTL::PixelFormatRGBA16Float;
            case TEXTURE_FORMAT_RGBA32F:           return MTL::PixelFormatRGBA32Float;
            case TEXTURE_FORMAT_R16F:              return MTL::PixelFormatR16Float;
            case TEXTURE_FORMAT_RG16F:             return MTL::PixelFormatRG16Float;
            case TEXTURE_FORMAT_R32F:              return MTL::PixelFormatR32Float;
            case TEXTURE_FORMAT_RG32F:             return MTL::PixelFormatRG32Float;

            // Unsigned integer
            case TEXTURE_FORMAT_RGBA32UI:          return MTL::PixelFormatRGBA32Uint;
            case TEXTURE_FORMAT_R32UI:             return MTL::PixelFormatR32Uint;
            case TEXTURE_FORMAT_BGRA8U:            return MTL::PixelFormatBGRA8Unorm;

            // ASTC
            case TEXTURE_FORMAT_RGBA_ASTC_4X4:    return MTL::PixelFormatASTC_4x4_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_5X4:    return MTL::PixelFormatASTC_5x4_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_5X5:    return MTL::PixelFormatASTC_5x5_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_6X5:    return MTL::PixelFormatASTC_6x5_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_6X6:    return MTL::PixelFormatASTC_6x6_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_8X5:    return MTL::PixelFormatASTC_8x5_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_8X6:    return MTL::PixelFormatASTC_8x6_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_8X8:    return MTL::PixelFormatASTC_8x8_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_10X5:   return MTL::PixelFormatASTC_10x5_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_10X6:   return MTL::PixelFormatASTC_10x6_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_10X8:   return MTL::PixelFormatASTC_10x8_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_10X10:  return MTL::PixelFormatASTC_10x10_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_12X10:  return MTL::PixelFormatASTC_12x10_LDR;
            case TEXTURE_FORMAT_RGBA_ASTC_12X12:  return MTL::PixelFormatASTC_12x12_LDR;

            default: return MTL::PixelFormatInvalid;
        }
    }

    static inline MetalTexture* GetDefaultTexture(MetalContext* context, ShaderDesc::ShaderDataType type)
    {
        switch(type)
        {
            case ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT:
            case ShaderDesc::SHADER_TYPE_TEXTURE2D:
            case ShaderDesc::SHADER_TYPE_SAMPLER:
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:       return context->m_DefaultTexture2D;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY: return context->m_DefaultTexture2DArray;
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE:    return context->m_DefaultTextureCubeMap;
            case ShaderDesc::SHADER_TYPE_UTEXTURE2D:      return context->m_DefaultTexture2D32UI;
            case ShaderDesc::SHADER_TYPE_IMAGE2D:
            case ShaderDesc::SHADER_TYPE_UIMAGE2D:        return context->m_DefaultStorageImage2D;
            default:break;
        }
        return 0x0;
    }

    static inline bool RequiresSampler(ShaderDesc::ShaderDataType type)
    {
        return type == ShaderDesc::SHADER_TYPE_SAMPLER2D ||
               type == ShaderDesc::SHADER_TYPE_SAMPLER3D ||
               type == ShaderDesc::SHADER_TYPE_SAMPLER_CUBE ||
               type == ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY ||
               type == ShaderDesc::SHADER_TYPE_SAMPLER ||
               type == ShaderDesc::SHADER_TYPE_SAMPLER3D_ARRAY;
    }

    static void CommitUniforms(MetalContext* context,
        void* encoder_raw,
        MetalConstantScratchBuffer* scratch_buffer,
        MetalArgumentBufferPool* argument_buffer_pool,
        MetalProgram* program,
        uint32_t alignment,
        bool is_compute)
    {
        MTL::RenderCommandEncoder* renc = NULL;
        MTL::ComputeCommandEncoder* cenc = NULL;

        if (is_compute)
            cenc = (MTL::ComputeCommandEncoder*) encoder_raw;
        else
            renc = (MTL::RenderCommandEncoder*) encoder_raw;

        for (int i = 0; i < program->m_BaseProgram.m_MaxSet; ++i)
        {
            if (program->m_ArgumentEncoders[i])
            {
                program->m_ArgumentBufferBindings[i] = argument_buffer_pool->Bind(context, program->m_ArgumentEncoders[i]);
            }
        }

        ProgramResourceBindingIterator it(&program->m_BaseProgram);
        const ProgramResourceBinding* next;
        while ((next = it.Next()))
        {
            ShaderResourceBinding* res        = next->m_Res;
            MTL::ArgumentEncoder* arg_encoder = program->m_ArgumentEncoders[res->m_Set];

            uint32_t msl_index = program->m_ResourceToMslIndex[res->m_Set][res->m_Binding];

            switch (res->m_BindingFamily)
            {
                case BINDING_FAMILY_UNIFORM_BUFFER:
                {
                    MetalUniformBuffer* bound_ubo = context->m_CurrentUniformBuffers[res->m_Set][res->m_Binding];

                    if (bound_ubo)
                    {
                        UniformBufferLayout* pgm_layout = (UniformBufferLayout*) next->m_BindingUserData;
                        if (bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash != pgm_layout->m_Hash)
                        {
                            dmLogWarning("Uniform buffer with hash %d has an incompatible layout with the currently bound program at the shader binding '%s' (hash=%d)",
                                bound_ubo->m_BaseUniformBuffer.m_Layout.m_Hash,
                                res->m_Name,
                                pgm_layout->m_Hash);
                            bound_ubo = 0;
                        }
                    }

                    if (bound_ubo)
                    {
                        arg_encoder->setBuffer(bound_ubo->m_DeviceBuffer.m_Buffer, 0, (NSUInteger) msl_index);

                        if (is_compute)
                            cenc->useResource(bound_ubo->m_DeviceBuffer.m_Buffer, MTL::ResourceUsageRead);
                        else
                            renc->useResource(bound_ubo->m_DeviceBuffer.m_Buffer, MTL::ResourceUsageRead);
                    }
                    else
                    {
                        const uint32_t uniform_size = DM_ALIGN(res->m_BindingInfo.m_BlockSize, alignment);
                        uint32_t offset = DM_ALIGN(scratch_buffer->m_MappedDataCursor, alignment);

                        memcpy(reinterpret_cast<uint8_t*>(scratch_buffer->m_DeviceBuffer.m_Buffer->contents()) + offset,
                               &program->m_UniformData[next->m_UniformBufferOffset],
                               res->m_BindingInfo.m_BlockSize);

                        arg_encoder->setBuffer(scratch_buffer->m_DeviceBuffer.m_Buffer, (NSUInteger) offset, (NSUInteger) msl_index);
                        scratch_buffer->m_MappedDataCursor = offset + uniform_size;
                    }
                } break;

                case BINDING_FAMILY_TEXTURE:
                {
                    MetalTexture* texture = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, context->m_TextureUnits[next->m_TextureUnit]);

                    if (texture == 0x0)
                    {
                        texture = GetDefaultTexture(context, res->m_Type.m_ShaderType);
                    }

                    if (res->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
                    {
                        MetalTextureSampler* sampler = &context->m_TextureSamplers[texture->m_TextureSamplerIndex];
                        arg_encoder->setSamplerState(sampler->m_Sampler, msl_index);
                    }
                    else
                    {
                        arg_encoder->setTexture(texture->m_Texture, msl_index);

                        // Handle combined sampler types
                        if (RequiresSampler(res->m_Type.m_ShaderType))
                        {
                            MetalTextureSampler* sampler = &context->m_TextureSamplers[texture->m_TextureSamplerIndex];
                            arg_encoder->setSamplerState(sampler->m_Sampler, msl_index + 1);
                        }
                    }

                    if (is_compute)
                        cenc->useResource(texture->m_Texture, texture->m_Usage);
                    else
                        renc->useResource(texture->m_Texture, texture->m_Usage);
                } break;

                case BINDING_FAMILY_STORAGE_BUFFER:
                    // TODO
                    break;
                case BINDING_FAMILY_GENERIC:
                    break;

                default:
                    break;
            }
        }

        // Maybe move this call to a "prepare scratch buffer" function or something?
        if (is_compute)
            cenc->useResource(scratch_buffer->m_DeviceBuffer.m_Buffer, MTL::ResourceUsageRead);
        else
            renc->useResource(scratch_buffer->m_DeviceBuffer.m_Buffer, MTL::ResourceUsageRead);

        for (uint32_t set = 0; set < context->m_CurrentProgram->m_BaseProgram.m_MaxSet; ++set)
        {
            if (context->m_CurrentProgram->m_ArgumentEncoders[set])
            {
                MetalArgumentBinding& arg_binding = context->m_CurrentProgram->m_ArgumentBufferBindings[set];

                if (is_compute)
                {
                    // Compute encoder uses only one stage
                    cenc->useResource(arg_binding.m_Buffer, MTL::ResourceUsageRead);
                    cenc->setBuffer(arg_binding.m_Buffer, arg_binding.m_Offset, set);
                }
                else
                {
                    // Render encoder can bind to either vertex or fragment
                    renc->useResource(arg_binding.m_Buffer, MTL::ResourceUsageRead);

                    if (set == 0)
                        renc->setVertexBuffer(arg_binding.m_Buffer, arg_binding.m_Offset, set);
                    if (set == 1)
                        renc->setFragmentBuffer(arg_binding.m_Buffer, arg_binding.m_Offset, set);
                }
            }
        }
    }

    static void DrawSetupCompute(MetalContext* context, MTL::ComputeCommandEncoder* encoder)
    {
        MetalFrameResource& frame = GetCurrentFrameResource(context);

        frame.m_ConstantScratchBuffer.EnsureSize(context, context->m_CurrentProgram->m_UniformDataSizeAligned);

        MetalPipeline* pipeline = GetOrCreateComputePipeline(context, context->m_CurrentProgram);
        assert(pipeline);

        CommitUniforms(context, encoder, &frame.m_ConstantScratchBuffer, &frame.m_ArgumentBufferPool, context->m_CurrentProgram, UNIFORM_BUFFER_ALIGNMENT, true);

        encoder->setComputePipelineState(pipeline->m_ComputePipelineState);
    }

    static void DrawSetup(MetalContext* context)
    {
        MetalRenderTarget* current_rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, context->m_CurrentRenderTarget);
        BeginRenderPass(context, context->m_CurrentRenderTarget);

        MetalFrameResource& frame = GetCurrentFrameResource(context);
        MTL::RenderCommandEncoder* encoder = frame.m_RenderCommandEncoder;

        VertexDeclaration* vx_declarations[MAX_VERTEX_BUFFERS] = {};
        uint32_t num_vx_buffers = 0;
        uint32_t vx_buffer_start_ix = context->m_CurrentProgram->m_BaseProgram.m_MaxBinding;

        for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
        {
            if (context->m_CurrentVertexBuffer[i] && context->m_CurrentVertexDeclaration[i])
            {
                vx_declarations[num_vx_buffers] = context->m_CurrentVertexDeclaration[i];

                encoder->setVertexBuffer(context->m_CurrentVertexBuffer[i]->m_Buffer, 0, num_vx_buffers + vx_buffer_start_ix);

                num_vx_buffers++;
            }
        }

        frame.m_ConstantScratchBuffer.EnsureSize(context, context->m_CurrentProgram->m_UniformDataSizeAligned);

        PipelineState pipeline_state_draw = context->m_PipelineState;

        MetalPipeline* pipeline = GetOrCreatePipeline(context, pipeline_state_draw,
            context->m_CurrentProgram, current_rt, vx_declarations, num_vx_buffers);
        assert(pipeline);

        encoder->setRenderPipelineState(pipeline->m_RenderPipelineState);
        if (pipeline->m_DepthStencilState)
        {
            encoder->setDepthStencilState(pipeline->m_DepthStencilState);
        }
        MTL::Viewport metal_vp;
        metal_vp.originX = context->m_MainViewport.m_X;
        metal_vp.width   = context->m_MainViewport.m_W;
        metal_vp.znear   = 0.0;
        metal_vp.zfar    = 1.0;

        if (current_rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            metal_vp.originY = current_rt->m_ColorTextureParams[0].m_Height - context->m_MainViewport.m_Y;
            metal_vp.height  = -(double) context->m_MainViewport.m_H;
        }
        else
        {
            metal_vp.originY = context->m_MainViewport.m_Y;
            metal_vp.height  = context->m_MainViewport.m_H;
        }

        if (context->m_ViewportChanged)
        {
            encoder->setViewport(metal_vp);

            /*
            SetViewportHelper(context->m_MainCommandBuffers[context->m_CurrentFrameInFlight],
                    vp.m_X, (context->m_WindowHeight - vp.m_Y), vp.m_W, -vp.m_H);
            */

            // MTL::ScissorRect scissor;
            // scissor.x      = current_rt->m_Scissor.offset.x;
            // scissor.y      = current_rt->m_Scissor.offset.y;
            // scissor.width  = current_rt->m_Scissor.extent.width;
            // scissor.height = current_rt->m_Scissor.extent.height;
            // encoder->setScissorRect(scissor);
        }

        MTL::CullMode cull_mode = MTL::CullModeNone;
        PipelineState pipeline_state_cull = pipeline_state_draw;

        if (current_rt->m_Id != DM_RENDERTARGET_BACKBUFFER_ID)
        {
            if (pipeline_state_cull.m_CullFaceType == FACE_TYPE_BACK)
            {
                pipeline_state_cull.m_CullFaceType = FACE_TYPE_FRONT;
            }
            else if (pipeline_state_cull.m_CullFaceType == FACE_TYPE_FRONT)
            {
                pipeline_state_cull.m_CullFaceType = FACE_TYPE_BACK;
            }
        }

        if (pipeline_state_cull.m_CullFaceEnabled)
        {
            if (pipeline_state_cull.m_CullFaceType == FACE_TYPE_BACK)
            {
                cull_mode = MTL::CullModeBack;
            }
            else if (pipeline_state_cull.m_CullFaceType == FACE_TYPE_FRONT)
            {
                cull_mode = MTL::CullModeFront;
            }
        }

        encoder->setCullMode(cull_mode);
        encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);

        CommitUniforms(context, encoder, &frame.m_ConstantScratchBuffer, &frame.m_ArgumentBufferPool, context->m_CurrentProgram, UNIFORM_BUFFER_ALIGNMENT, false);
    }

    static MTL::PrimitiveType ConvertPrimitiveType(PrimitiveType prim_type)
    {
        switch (prim_type)
        {
            case PRIMITIVE_TRIANGLES:      return MTL::PrimitiveTypeTriangle;
            case PRIMITIVE_TRIANGLE_STRIP: return MTL::PrimitiveTypeTriangleStrip;
            case PRIMITIVE_LINES:          return MTL::PrimitiveTypeLine;
            //case PRIMITIVE_LINE_STRIP:     return MTL::PrimitiveTypeLineStrip;
            //case PRIMITIVE_POINTS:         return MTL::PrimitiveTypePoint;
            default: break;
        }
        return MTL::PrimitiveTypeTriangle;
    }

    static void MetalDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        MetalContext* context = (MetalContext*)_context;
        assert(context->m_FrameBegun);

        DrawSetup(context);

        // Index buffer setup
        MetalDeviceBuffer* ib = (MetalDeviceBuffer*) index_buffer;
        assert(ib);
        assert(ib->m_Buffer);

        MTL::IndexType metal_index_type = (type == TYPE_UNSIGNED_INT)
            ? MTL::IndexTypeUInt32
            : MTL::IndexTypeUInt16;

        // The `first` value is a byte offset, similar to Vulkan’s.
        NSUInteger index_offset = first;

        MTL::PrimitiveType metal_prim_type = ConvertPrimitiveType(prim_type);
        MetalFrameResource& frame = GetCurrentFrameResource(context);

        // Perform the draw
        if (instance_count > 1)
        {
            frame.m_RenderCommandEncoder->drawIndexedPrimitives(metal_prim_type, count, metal_index_type, ib->m_Buffer, index_offset, instance_count);
        }
        else
        {
            frame.m_RenderCommandEncoder->drawIndexedPrimitives(metal_prim_type, count, metal_index_type, ib->m_Buffer, index_offset);
        }
    }

    static void MetalDraw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DrawCalls, 1);

        MetalContext* context = (MetalContext*)_context;
        assert(context->m_FrameBegun);

        DrawSetup(context);

        MTL::PrimitiveType metal_prim_type = ConvertPrimitiveType(prim_type);
        MetalFrameResource& frame = GetCurrentFrameResource(context);

        if (instance_count > 1)
        {
            frame.m_RenderCommandEncoder->drawPrimitives(metal_prim_type, first, count, instance_count);
        }
        else
        {
            frame.m_RenderCommandEncoder->drawPrimitives(metal_prim_type, first, count);
        }
    }

    static inline bool IsRenderTargetbound(MetalContext* context, HRenderTarget rt)
    {
        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        MetalRenderTarget* current_rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, rt);
        return current_rt ? current_rt->m_IsBound : 0;
    }

    static void MetalDispatchCompute(HContext _context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        DM_PROFILE(__FUNCTION__);
        DM_PROPERTY_ADD_U32(rmtp_DispatchCalls, 1);
        MetalContext* context = (MetalContext*) _context;
        MetalProgram* program = context->m_CurrentProgram;
        assert(program);

        // We can't run compute if we have started a render pass
        // Perhaps it would work if we could run it in a separate command buffer or a dedicated compute queue?
        if (IsRenderTargetbound(context, context->m_CurrentRenderTarget))
        {
            EndRenderPass(context);
        }

        MetalFrameResource& frame = GetCurrentFrameResource(context);

        MTL::ComputeCommandEncoder* encoder = frame.m_CommandBuffer->computeCommandEncoder();

        DrawSetupCompute(context, encoder);

        encoder->dispatchThreadgroups(
            MTL::Size{ group_count_x, group_count_y, group_count_z },
            MTL::Size{ program->m_WorkGroupSize[0], program->m_WorkGroupSize[1], program->m_WorkGroupSize[2] }
        );

        encoder->endEncoding();
    }

    static MetalShaderModule* CreateShaderModule(MTL::Device* device, const char* src, uint32_t src_size, char* error_buffer, uint32_t error_buffer_size)
    {
        char* null_terminated_buffer = new char[src_size + 1];
        memcpy(null_terminated_buffer, src, src_size);
        null_terminated_buffer[src_size] = 0;

        NS::Error* error  = 0;
        MTL::Library* library = device->newLibrary(NS::String::string(null_terminated_buffer, NS::StringEncoding::UTF8StringEncoding), 0, &error);

        delete[] null_terminated_buffer;

        if (error)
        {
            dmSnPrintf(error_buffer, error_buffer_size, "%s", error->localizedDescription()->utf8String());
            return 0;
        }

        MetalShaderModule* module = new MetalShaderModule;
        module->m_Library = library;
        module->m_Function = library->newFunction(NS::String::string("main0", NS::StringEncoding::UTF8StringEncoding));

        HashState64 shader_hash_state;
        dmHashInit64(&shader_hash_state, false);
        dmHashUpdateBuffer64(&shader_hash_state, src, (uint32_t) src_size);
        module->m_Hash = dmHashFinal64(&shader_hash_state);

        return module;
    }

    static void CreateProgramResourceBindings(MetalProgram* program, ResourceBindingDesc bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT], MetalShaderModule** shaders, ShaderDesc::Shader** ddf_shaders, uint32_t num_shaders)
    {
        ProgramResourceBindingsInfo binding_info = {};
        FillProgramResourceBindings(&program->m_BaseProgram, bindings, UNIFORM_BUFFER_ALIGNMENT, STORAGE_BUFFER_ALIGNMENT, binding_info);

        for (int i = 0; i < num_shaders; ++i)
        {
            ShaderDesc::Shader* ddf = ddf_shaders[i];

            for (int j = 0; j < ddf->m_MslResourceMapping.m_Count; ++j)
            {
                ShaderDesc::MSLResourceMapping* entry = &ddf->m_MslResourceMapping[j];
                program->m_ResourceToMslIndex[entry->m_Set][entry->m_Binding] = entry->m_MslIndex;
            }
        }

        program->m_UniformData = new uint8_t[binding_info.m_UniformDataSize];
        memset(program->m_UniformData, 0, binding_info.m_UniformDataSize);

        program->m_UniformDataSizeAligned   = binding_info.m_UniformDataSizeAligned;
        program->m_UniformBufferCount       = binding_info.m_UniformBufferCount;
        program->m_StorageBufferCount       = binding_info.m_StorageBufferCount;
        program->m_TextureSamplerCount      = binding_info.m_TextureCount;
        program->m_BaseProgram.m_MaxSet     = binding_info.m_MaxSet;
        program->m_BaseProgram.m_MaxBinding = binding_info.m_MaxBinding;

        for (int i = 0; i < program->m_BaseProgram.m_ShaderMeta.m_Textures.Size(); ++i)
        {
            const ShaderResourceBinding& shader_res = program->m_BaseProgram.m_ShaderMeta.m_Textures[i];
            ProgramResourceBinding& shader_pgm_res = program->m_BaseProgram.m_ResourceBindings[shader_res.m_Set][shader_res.m_Binding];

            if (shader_res.m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
            {
                const ShaderResourceBinding& texture_shader_res = program->m_BaseProgram.m_ShaderMeta.m_Textures[shader_res.m_BindingInfo.m_SamplerTextureIndex];
                const ProgramResourceBinding& texture_pgm_res   = program->m_BaseProgram.m_ResourceBindings[texture_shader_res.m_Set][texture_shader_res.m_Binding];
                shader_pgm_res.m_TextureUnit                    = texture_pgm_res.m_TextureUnit;
            }
        }

        BuildUniforms(&program->m_BaseProgram);
    }

    static void CreateArgumentBuffers(MetalContext* context, MetalProgram* program)
    {
        uint8_t set_stage_flags[MAX_SET_COUNT] = {0};

        for (int i = 0; i < program->m_BaseProgram.m_MaxSet; ++i)
        {
            for (int j = 0; j < program->m_BaseProgram.m_MaxBinding; ++j)
            {
                ProgramResourceBinding* res = &program->m_BaseProgram.m_ResourceBindings[i][j];

                if (res->m_Res)
                {
                    set_stage_flags[i] |= res->m_Res->m_StageFlags;
                }
            }

            if (set_stage_flags[i] == 0)
            {
                continue;
            }

            if (set_stage_flags[i] & SHADER_STAGE_FLAG_VERTEX)
            {
                program->m_ArgumentEncoders[i] = program->m_VertexModule->m_Function->newArgumentEncoder(i);
            }
            else if (set_stage_flags[i] & SHADER_STAGE_FLAG_FRAGMENT)
            {
                program->m_ArgumentEncoders[i] = program->m_FragmentModule->m_Function->newArgumentEncoder(i);
            }
            else if (set_stage_flags[i] & SHADER_STAGE_FLAG_COMPUTE)
            {
                program->m_ArgumentEncoders[i] = program->m_ComputeModule->m_Function->newArgumentEncoder(i);
            }
        }
    }

    static HProgram MetalNewProgram(HContext _context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        ShaderDesc::Shader* ddf_vp = 0x0;
        ShaderDesc::Shader* ddf_fp = 0x0;
        ShaderDesc::Shader* ddf_cp = 0x0;

        if (!GetShaderProgram(_context, ddf, &ddf_vp, &ddf_fp, &ddf_cp))
        {
            return 0;
        }

        MetalContext* context = (MetalContext*) _context;
        MetalProgram* program = new MetalProgram;
        memset(program, 0, sizeof(MetalProgram));

        CreateShaderMeta(&ddf->m_Reflection, &program->m_BaseProgram.m_ShaderMeta);

        MetalShaderModule* shaders[] = { 0x0, 0x0 };
        ShaderDesc::Shader* ddf_shaders[] = { 0x0, 0x0 };
        uint32_t num_shaders = 0;

        if (ddf_cp)
        {
            program->m_ComputeModule = CreateShaderModule(context->m_Device, (const char*) ddf_cp->m_Source.m_Data, ddf_cp->m_Source.m_Count, error_buffer, error_buffer_size);

            if (!program->m_ComputeModule)
            {
                DeleteProgram(_context, (HProgram) program);
                return 0;
            }

            program->m_WorkGroupSize[0] = ddf_cp->m_WorkGroupSize.m_X;
            program->m_WorkGroupSize[1] = ddf_cp->m_WorkGroupSize.m_Y;
            program->m_WorkGroupSize[2] = ddf_cp->m_WorkGroupSize.m_Z;

            shaders[0]     = program->m_ComputeModule;
            ddf_shaders[0] = ddf_cp;
            num_shaders    = 1;
        }
        else
        {
            program->m_VertexModule = CreateShaderModule(context->m_Device, (const char*) ddf_vp->m_Source.m_Data, ddf_vp->m_Source.m_Count, error_buffer, error_buffer_size);
            if (!program->m_VertexModule)
            {
                DeleteProgram(_context, (HProgram) program);
                return 0;
            }

            program->m_FragmentModule = CreateShaderModule(context->m_Device, (const char*) ddf_fp->m_Source.m_Data, ddf_fp->m_Source.m_Count, error_buffer, error_buffer_size);
            if (!program->m_FragmentModule)
            {
                DeleteProgram(_context, (HProgram) program);
                return 0;
            }

            HashState64 program_hash;
            dmHashInit64(&program_hash, false);

            for (uint32_t i=0; i < program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size(); i++)
            {
                if (program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_StageFlags & SHADER_STAGE_FLAG_VERTEX)
                {
                    dmHashUpdateBuffer64(&program_hash, &program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_Binding, sizeof(program->m_BaseProgram.m_ShaderMeta.m_Inputs[i].m_Binding));
                }
            }

            dmHashUpdateBuffer64(&program_hash, &program->m_VertexModule->m_Hash, sizeof(program->m_VertexModule->m_Hash));
            dmHashUpdateBuffer64(&program_hash, &program->m_FragmentModule->m_Hash, sizeof(program->m_FragmentModule->m_Hash));
            program->m_Hash = dmHashFinal64(&program_hash);

            shaders[0]     = program->m_VertexModule;
            shaders[1]     = program->m_FragmentModule;
            ddf_shaders[0] = ddf_vp;
            ddf_shaders[1] = ddf_fp;
            num_shaders    = 2;
        }

        ResourceBindingDesc bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};
        CreateProgramResourceBindings(program, bindings, shaders, ddf_shaders, num_shaders);

        CreateArgumentBuffers(context, program);

        return (HProgram) program;
    }

    static void MetalDeleteProgram(HContext _context, HProgram _program)
    {
        MetalProgram* program = (MetalProgram*) _program;
        delete program;
    }

    static ShaderDesc::Language MetalGetProgramLanguage(HProgram _program)
    {
        return ShaderDesc::LANGUAGE_MSL_22;
    }

    static bool MetalIsShaderLanguageSupported(HContext _context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        return language == ShaderDesc::LANGUAGE_MSL_22;
    }

    static void MetalEnableProgram(HContext _context, HProgram program)
    {
        MetalContext* context = (MetalContext*)_context;
        context->m_CurrentProgram = (MetalProgram*) program;
    }

    static void MetalDisableProgram(HContext _context)
    {
        MetalContext* context = (MetalContext*)_context;
        context->m_CurrentProgram = 0;
    }

    static bool MetalReloadProgram(HContext _context, HProgram program, ShaderDesc* ddf)
    {
        return 0;
    }

    static uint32_t MetalGetAttributeCount(HProgram prog)
    {
        MetalProgram* program_ptr = (MetalProgram*) prog;
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

    static void MetalGetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        MetalProgram* program = (MetalProgram*) prog;
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

    static inline void WriteConstantData(uint32_t offset, uint8_t* uniform_data_ptr, uint8_t* data_ptr, uint32_t data_size)
    {
        memcpy(&uniform_data_ptr[offset], data_ptr, data_size);
    }

    static void MetalSetConstantV4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        MetalProgram* program_ptr = (MetalProgram*) context->m_CurrentProgram;
        uint32_t set               = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding           = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset     = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res = program_ptr->m_BaseProgram.m_ResourceBindings[set][binding];

        uint32_t offset = pgm_res.m_UniformBufferOffset + buffer_offset;
        WriteConstantData(offset, program_ptr->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * count);
    }

    static void MetalSetConstantM4(HContext _context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context->m_CurrentProgram);
        assert(base_location != INVALID_UNIFORM_LOCATION);

        MetalProgram* program_ptr    = (MetalProgram*) context->m_CurrentProgram;
        uint32_t set            = UNIFORM_LOCATION_GET_OP0(base_location);
        uint32_t binding        = UNIFORM_LOCATION_GET_OP1(base_location);
        uint32_t buffer_offset  = UNIFORM_LOCATION_GET_OP2(base_location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        ProgramResourceBinding& pgm_res = program_ptr->m_BaseProgram.m_ResourceBindings[set][binding];

        uint32_t offset = pgm_res.m_UniformBufferOffset + buffer_offset;
        WriteConstantData(offset, program_ptr->m_UniformData, (uint8_t*) data, sizeof(dmVMath::Vector4) * 4 * count);
    }

    static void MetalSetSampler(HContext _context, HUniformLocation location, int32_t unit)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context->m_CurrentProgram);
        assert(location != INVALID_UNIFORM_LOCATION);

        MetalProgram* program_ptr = (MetalProgram*) context->m_CurrentProgram;
        uint32_t set         = UNIFORM_LOCATION_GET_OP0(location);
        uint32_t binding     = UNIFORM_LOCATION_GET_OP1(location);
        assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

        assert(program_ptr->m_BaseProgram.m_ResourceBindings[set][binding].m_Res);
        program_ptr->m_BaseProgram.m_ResourceBindings[set][binding].m_TextureUnit = unit;
    }

    static void MetalSetViewport(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        MetalContext* context = (MetalContext*)_context;
        // Defer the update to when we actually draw, since we *might* need to invert the viewport
        // depending on wether or not we have set a different rendertarget from when
        // this call was made.
        MetalViewport& viewport = context->m_MainViewport;
        viewport.m_X            = (uint16_t) x;
        viewport.m_Y            = (uint16_t) y;
        viewport.m_W            = (uint16_t) width;
        viewport.m_H            = (uint16_t) height;

        context->m_ViewportChanged = 1;
    }

    static void MetalEnableState(HContext _context, State state)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 1);
    }

    static void MetalDisableState(HContext _context, State state)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context);
        SetPipelineStateValue(context->m_PipelineState, state, 0);
    }

    static void MetalSetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context);
        context->m_PipelineState.m_BlendSrcFactor      = source_factor;
        context->m_PipelineState.m_BlendDstFactor      = destinaton_factor;
        context->m_PipelineState.m_BlendSrcFactorAlpha = source_factor;
        context->m_PipelineState.m_BlendDstFactorAlpha = destinaton_factor;
        context->m_PipelineState.m_BlendEquationColor  = BLEND_EQUATION_ADD;
        context->m_PipelineState.m_BlendEquationAlpha  = BLEND_EQUATION_ADD;
    }

    static void MetalSetBlendFuncSeparate(HContext _context, BlendFactor src_factor_color, BlendFactor dst_factor_color, BlendFactor src_factor_alpha, BlendFactor dst_factor_alpha)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context);
        context->m_PipelineState.m_BlendSrcFactor      = src_factor_color;
        context->m_PipelineState.m_BlendDstFactor      = dst_factor_color;
        context->m_PipelineState.m_BlendSrcFactorAlpha = src_factor_alpha;
        context->m_PipelineState.m_BlendDstFactorAlpha = dst_factor_alpha;
    }

    static void MetalSetBlendEquationSeparate(HContext _context, BlendEquation equation_color, BlendEquation equation_alpha)
    {
        MetalContext* context = (MetalContext*) _context;
        assert(context);
        context->m_PipelineState.m_BlendEquationColor  = equation_color;
        context->m_PipelineState.m_BlendEquationAlpha  = equation_alpha;
    }

    static void MetalSetColorMask(HContext _context, bool red, bool green, bool blue, bool alpha)
    {
        MetalContext* context = (MetalContext*)_context;
        assert(context);
        uint8_t write_mask = red   ? DM_GRAPHICS_STATE_WRITE_R : 0;
        write_mask        |= green ? DM_GRAPHICS_STATE_WRITE_G : 0;
        write_mask        |= blue  ? DM_GRAPHICS_STATE_WRITE_B : 0;
        write_mask        |= alpha ? DM_GRAPHICS_STATE_WRITE_A : 0;

        context->m_PipelineState.m_WriteColorMask = write_mask;
    }

    static void MetalSetDepthMask(HContext _context, bool enable_mask)
    {
        MetalContext* context = (MetalContext*)_context;
        context->m_PipelineState.m_WriteDepth = enable_mask;
    }

    static void MetalSetDepthFunc(HContext _context, CompareFunc func)
    {
        MetalContext* context = (MetalContext*)_context;
        context->m_PipelineState.m_DepthTestFunc = func;
    }

    static void MetalSetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        //MetalContext* context = (MetalContext*)_context;
        // TODO
    }

    static void MetalSetStencilMask(HContext _context, uint32_t mask)
    {
        MetalContext* context = (MetalContext*)_context;
        context->m_PipelineState.m_StencilWriteMask = mask;
    }

    static void MetalSetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        MetalContext* context = (MetalContext*)_context;
        assert(context);
        context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void MetalSetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        MetalContext* context = (MetalContext*)_context;
        assert(context);
        if (face_type == FACE_TYPE_BACK)
        {
            context->m_PipelineState.m_StencilBackTestFunc  = (uint8_t) func;
        }
        else
        {
            context->m_PipelineState.m_StencilFrontTestFunc = (uint8_t) func;
        }
        context->m_PipelineState.m_StencilReference     = (uint8_t) ref;
        context->m_PipelineState.m_StencilCompareMask   = (uint8_t) mask;
    }

    static void MetalSetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        MetalContext* context = (MetalContext*)_context;
        assert(context);
        context->m_PipelineState.m_StencilFrontOpFail      = sfail;
        context->m_PipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_PipelineState.m_StencilFrontOpPass      = dppass;
        context->m_PipelineState.m_StencilBackOpFail       = sfail;
        context->m_PipelineState.m_StencilBackOpDepthFail  = dpfail;
        context->m_PipelineState.m_StencilBackOpPass       = dppass;
    }

    static void MetalSetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        MetalContext* context = (MetalContext*)_context;
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

    static void MetalSetCullFace(HContext _context, FaceType face_type)
    {
        MetalContext* context = (MetalContext*)_context;
        assert(context);
        context->m_PipelineState.m_CullFaceType = face_type;
        context->m_CullFaceChanged              = true;
    }

    static void MetalSetFaceWinding(HContext _context, FaceWinding face_winding)
    {

    }

    static void MetalSetPolygonOffset(HContext _context, float factor, float units)
    {

    }

    static HRenderTarget MetalNewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        MetalContext* context = (MetalContext*)_context;

        // allocate the render target object (ID helper reused from your Vulkan path)
        MetalRenderTarget* rt = new MetalRenderTarget(GetNextRenderTargetId());

        // copy params into RT object
        memcpy(rt->m_ColorBufferLoadOps, params.m_ColorBufferLoadOps, sizeof(AttachmentOp) * MAX_BUFFER_COLOR_ATTACHMENTS);
        memcpy(rt->m_ColorBufferStoreOps, params.m_ColorBufferStoreOps, sizeof(AttachmentOp) * MAX_BUFFER_COLOR_ATTACHMENTS);
        memcpy(rt->m_ColorAttachmentClearValue, params.m_ColorBufferClearValue, sizeof(float) * MAX_BUFFER_COLOR_ATTACHMENTS * 4);
        memcpy(rt->m_ColorTextureParams, params.m_ColorBufferParams, sizeof(TextureParams) * MAX_BUFFER_COLOR_ATTACHMENTS);
        // depth/stencil choice as in Vulkan
        rt->m_DepthStencilTextureParams = (buffer_type_flags & BUFFER_TYPE_DEPTH_BIT) ?
            params.m_DepthBufferParams :
            params.m_StencilBufferParams;

        // We don't want the engine to keep the pointer to raw data in the params stored on the RT,
        // so clear any pointers inside the stored TextureParams (same as Vulkan)
        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            ClearTextureParamsData(rt->m_ColorTextureParams[i]);
        }
        ClearTextureParamsData(rt->m_DepthStencilTextureParams);

        // local arrays for bookkeeping while creating
        BufferType buffer_types[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture texture_color[MAX_BUFFER_COLOR_ATTACHMENTS];
        HTexture texture_depth_stencil = 0;

        const bool has_depth   = (buffer_type_flags & BUFFER_TYPE_DEPTH_BIT) != 0;
        const bool has_stencil = (buffer_type_flags & BUFFER_TYPE_STENCIL_BIT) != 0;
        uint8_t color_index = 0;

        // color bits to check
        BufferType color_buffer_flags[] = {
            BUFFER_TYPE_COLOR0_BIT,
            BUFFER_TYPE_COLOR1_BIT,
            BUFFER_TYPE_COLOR2_BIT,
            BUFFER_TYPE_COLOR3_BIT,
        };

        // create color attachments
        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            BufferType buffer_type = color_buffer_flags[i];
            if ((buffer_type_flags & buffer_type) == 0)
                continue;

            TextureParams& color_buffer_params = rt->m_ColorTextureParams[i];

            // promote RGB -> RGBA (Metal doesn't support 3-channel render targets reliably)
            if (color_buffer_params.m_Format == TEXTURE_FORMAT_RGB)
            {
                color_buffer_params.m_Format = TEXTURE_FORMAT_RGBA;
            }

            // Create engine texture object using your NewTexture helper (keeps bookkeeping consistent)
            // The Vulkan path used params.m_ColorBufferCreationParams[i]; mirror that here.
            HTexture new_texture_color_handle = NewTexture(_context, params.m_ColorBufferCreationParams[i]);
            MetalTexture* new_texture_color = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, new_texture_color_handle);
            assert(new_texture_color);

            CreateMetalTexture(context, new_texture_color, color_buffer_params, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);

            texture_color[color_index] = new_texture_color_handle;
            buffer_types[color_index] = buffer_type;
            color_index++;
        }

        // create depth/stencil attachment if requested
        if (has_depth || has_stencil)
        {
            const TextureCreationParams& ds_create_params = has_depth ? params.m_DepthBufferCreationParams : params.m_StencilBufferCreationParams;
            const TextureParams& ds_params = has_depth ? params.m_DepthBufferParams : params.m_StencilBufferParams;

            // create engine texture wrapper for depth/stencil
            texture_depth_stencil = NewTexture(_context, ds_create_params);
            MetalTexture* depth_texture_ptr = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, texture_depth_stencil);
            assert(depth_texture_ptr);

            CreateMetalDepthStencilTexture(context, depth_texture_ptr, ds_params, MTL::TextureUsageRenderTarget);
        }

        // record info into the render target object
        rt->m_ColorAttachmentCount = color_index;
        for (uint32_t c = 0; c < (uint32_t)color_index; ++c)
        {
            rt->m_TextureColor[c] = texture_color[c];
            rt->m_ColorFormat[c]  = GetMetalPixelFormat(rt->m_ColorTextureParams[c].m_Format);
        }
        if (texture_depth_stencil)
        {
            rt->m_TextureDepthStencil = texture_depth_stencil;
            rt->m_DepthStencilFormat = MTL::PixelFormatDepth32Float_Stencil8; // or chosenDepthFormat
        }

        // store the RT in the asset container and return handle
        return StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
    }

    static void MetalDeleteRenderTarget(HContext _context, HRenderTarget render_target)
    {
        MetalContext* context = (MetalContext*) _context;
        MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, render_target);
        context->m_BaseContext.m_AssetHandleContainer.Release(render_target);

        for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            if (rt->m_TextureColor[i])
            {
                DeleteTexture(_context, rt->m_TextureColor[i]);
            }
        }

        if (rt->m_TextureDepthStencil)
        {
            DeleteTexture(_context, rt->m_TextureDepthStencil);
        }

        delete rt;
    }

    static void MetalSetRenderTarget(HContext _context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        MetalContext* context = (MetalContext*) _context;
        HRenderTarget new_rt = render_target != 0x0 ? render_target : context->m_MainRenderTarget;

        if (context->m_CurrentRenderTarget == new_rt)
        {
            return;
        }

        FlushPendingRenderTargetClear(context, context->m_CurrentRenderTarget);

        if (context->m_RenderTargetBound)
        {
            EndRenderPass(context);
        }

        context->m_CurrentRenderTarget = new_rt;
        context->m_ViewportChanged = 1;
    }

    static HTexture MetalGetRenderTargetTexture(HContext _context, HRenderTarget render_target, BufferType buffer_type)
    {
        MetalContext* context = (MetalContext*)_context;
        MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, render_target);

        if (IsColorBufferType(buffer_type))
        {
            return rt->m_TextureColor[GetBufferTypeIndex(buffer_type)];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            return rt->m_TextureDepthStencil;
        }
        return 0;
    }

    static void MetalGetRenderTargetSize(HContext _context, HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        MetalContext* context = (MetalContext*)_context;
        MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, render_target);
        TextureParams* params = 0;

        if (IsColorBufferType(buffer_type))
        {
            uint32_t i = GetBufferTypeIndex(buffer_type);
            assert(i < MAX_BUFFER_COLOR_ATTACHMENTS);
            params = &rt->m_ColorTextureParams[i];
        }
        else if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        {
            params = &rt->m_DepthStencilTextureParams;
        }
        else
        {
            assert(0);
        }

        width  = params->m_Width;
        height = params->m_Height;
    }

    static void MetalSetRenderTargetSize(HContext _context, HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        MetalContext* context = (MetalContext*)_context;
        MetalRenderTarget* rt = GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, render_target);
        if (!rt)
        {
            return;
        }

        // The backbuffer size is owned by the drawable/layer.
        if (rt->m_Id == DM_RENDERTARGET_BACKBUFFER_ID)
        {
            return;
        }

        if (rt->m_IsBound)
        {
            EndRenderPass(context);
        }

        for (uint32_t i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
        {
            rt->m_ColorTextureParams[i].m_Width  = width;
            rt->m_ColorTextureParams[i].m_Height = height;

            if (rt->m_TextureColor[i])
            {
                MetalTexture* texture_color = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, rt->m_TextureColor[i]);
                if (texture_color)
                {
                    CreateMetalTexture(context, texture_color, rt->m_ColorTextureParams[i], MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
                }
            }
        }

        rt->m_DepthStencilTextureParams.m_Width  = width;
        rt->m_DepthStencilTextureParams.m_Height = height;

        if (rt->m_TextureDepthStencil)
        {
            MetalTexture* depth_stencil_texture = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, rt->m_TextureDepthStencil);
            if (depth_stencil_texture)
            {
                CreateMetalDepthStencilTexture(context, depth_stencil_texture, rt->m_DepthStencilTextureParams, MTL::TextureUsageRenderTarget);
            }
        }

        context->m_ViewportChanged = 1;
    }

    static bool MetalIsTextureFormatSupported(HContext _context, TextureFormat format)
    {
        MetalContext* context = (MetalContext*) _context;
        return (context->m_BaseContext.m_TextureFormatSupport & (1 << format)) != 0 || (context->m_ASTCSupport && IsTextureFormatASTC(format));
    }

    static inline MTL::ResourceUsage GetMetalUsageFromHints(uint8_t hints)
    {
        MTL::ResourceUsage usage = MTL::ResourceUsageRead;
        if ((hints & TEXTURE_USAGE_FLAG_SAMPLE) != 0)
        {
            usage |= MTL::ResourceUsageSample;
        }
        if ((hints & TEXTURE_USAGE_FLAG_STORAGE) != 0)
        {
            usage |= MTL::ResourceUsageWrite;
        }

        return usage;
    }

    static MetalTexture* MetalNewTextureInternal(const TextureCreationParams& params)
    {
        MetalTexture* tex = new MetalTexture;
        tex->m_Base.m_Type           = params.m_Type;
        tex->m_Base.m_Format         = TEXTURE_FORMAT_RGBA;
        tex->m_Base.m_Width          = params.m_Width;
        tex->m_Base.m_Height         = params.m_Height;
        tex->m_Base.m_Depth          = dmMath::Max((uint16_t)1, params.m_Depth);
        tex->m_Base.m_MipMapCount    = params.m_MipMapCount;
        tex->m_Base.m_UsageHintFlags = params.m_UsageHintBits;
        tex->m_Base.m_PageCount      = params.m_LayerCount;
        tex->m_Base.m_DataState      = 0;
        tex->m_Base.m_NumTextureIds  = 1;
        tex->m_LayerCount            = dmMath::Max((uint8_t)1, params.m_LayerCount);

        // TODO
        // tex->m_PendingUpload  = INVALID_OPAQUE_HANDLE;
        tex->m_Usage = GetMetalUsageFromHints(params.m_UsageHintBits);

        if (params.m_OriginalWidth == 0)
        {
            tex->m_Base.m_OriginalWidth  = params.m_Width;
            tex->m_Base.m_OriginalHeight = params.m_Height;
            tex->m_Base.m_OriginalDepth  = params.m_Depth;
        }
        else
        {
            tex->m_Base.m_OriginalWidth  = params.m_OriginalWidth;
            tex->m_Base.m_OriginalHeight = params.m_OriginalHeight;
            tex->m_Base.m_OriginalDepth  = params.m_OriginalDepth;
        }
        return tex;
    }

    static HTexture MetalNewTexture(HContext _context, const TextureCreationParams& params)
    {
        MetalContext* context = (MetalContext*) _context;
        MetalTexture* texture = MetalNewTextureInternal(params);
        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        return StoreAssetInContainer(context->m_BaseContext.m_AssetHandleContainer, texture, ASSET_TYPE_TEXTURE);
    }

    static void MetalDeleteTextureInternal(MetalContext* context, MetalTexture* texture)
    {
        DestroyResourceDeferred(context, texture);
        delete texture;
    }

    static void MetalDeleteTexture(HContext _context, HTexture texture)
    {
        MetalContext* context = (MetalContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        MetalDeleteTextureInternal(context, GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, texture));
        context->m_BaseContext.m_AssetHandleContainer.Release(texture);
    }

    // Helper to compute aligned pitch
    static inline uint32_t AlignTo(uint32_t value, uint32_t align) {
        return (value + align - 1) & ~(align - 1);
    }

    // Metal version of "GetCopyableFootprints" behavior for one mip level.
    // It computes rows, rowSize (unpacked), paddedRowPitch and sliceSize for each slice.
    struct MetalPlacedSubresource
    {
        uint32_t rows;
        uint64_t rowSize;         // unpadded bytes per row
        uint64_t paddedRowPitch;  // bytesPerRow used for copyFromBuffer (aligned to 256)
        uint64_t sliceSize;       // bytes per image (paddedRowPitch * rows)
        uint64_t offset;          // offset inside contiguous upload buffer (computed later)
    };

    static void MetalCopyToTexture(MetalContext* context,
                                   MetalTexture* texture,
                                   TextureFormat format_src,
                                   const TextureParams& params,
                                   const uint8_t* pixels)
    {
        if (!texture || !texture->m_Texture || !pixels) return;

        MTL::Device* device = context->m_Device;

        const uint32_t target_mip = params.m_MipMap;
        const uint32_t layerCount = dmMath::Max((uint32_t)texture->m_LayerCount, (uint32_t)params.m_LayerCount);

        // Base dims (full-size level 0)
        const uint32_t baseWidth  = texture->m_Base.m_Width;
        const uint32_t baseHeight = texture->m_Base.m_Height;

        // Compute mip dims for the target mip
        auto MipDim = [] (uint32_t base, uint32_t mip) -> uint32_t {
            uint32_t v = base >> mip;
            return v ? v : 1u;
        };
        const uint32_t mipWidth  = MipDim(baseWidth, target_mip);
        const uint32_t mipHeight = MipDim(baseHeight, target_mip);

        // Copy rectangle size — use params.m_Width/Height if set (subupdate), otherwise full mip size
        const uint32_t copyWidth  = params.m_Width ? params.m_Width : mipWidth;
        const uint32_t copyHeight = params.m_Height ? params.m_Height : mipHeight;

        // Bytes per texel in source format
        const uint32_t bpp_src_bits = GetTextureFormatBitsPerPixel(format_src);
        const uint32_t bytesPerTexel = bpp_src_bits / 8;
        const uint64_t unpaddedRowSize = (uint64_t)copyWidth * bytesPerTexel;
        const uint64_t unpaddedSliceSize = unpaddedRowSize * (uint64_t)copyHeight;

        // Align row pitch to 256 bytes for Metal copyFromBuffer() — required on many GPUs
        const uint32_t rowAlignment = 256u;
        const uint64_t paddedRowPitch = (uint64_t)AlignTo((uint32_t)unpaddedRowSize, rowAlignment);
        const uint64_t paddedSliceSize = paddedRowPitch * (uint64_t)copyHeight;

        // Build per-slice placed footprints (for this helper we use same footprint for each slice)
        dmArray<MetalPlacedSubresource> placed;
        placed.SetCapacity(layerCount);
        placed.SetSize(layerCount);

        uint64_t accumOffset = 0;
        for (uint32_t i = 0; i < layerCount; ++i)
        {
            MetalPlacedSubresource& p = placed[i];
            p.rows = copyHeight;
            p.rowSize = unpaddedRowSize;
            p.paddedRowPitch = paddedRowPitch;
            p.sliceSize = paddedSliceSize;
            p.offset = accumOffset;
            accumOffset += p.sliceSize;
        }

        // Create one contiguous upload buffer (like DX12 upload heap)
        const uint64_t totalUploadSize = accumOffset;
        MTL::Buffer* uploadBuffer = device->newBuffer((NSUInteger)totalUploadSize, MTL::ResourceStorageModeShared);
        if (!uploadBuffer)
        {
            dmLogError("MetalTextureUploadWithFootprints: failed to create upload buffer size %llu", totalUploadSize);
            return;
        }

        // Map buffer and copy each slice into its place with row-pitch padding
        uint8_t* dstBase = reinterpret_cast<uint8_t*>(uploadBuffer->contents());
        const uint8_t* srcBase = pixels;

        // Source layout assumption: pixels holds `layerCount` slices contiguous for this mip,
        // each slice being exactly (copyWidth * copyHeight * bytesPerTexel) bytes (unpadded).
        const uint64_t srcSliceStride = unpaddedSliceSize;

        for (uint32_t slice = 0; slice < layerCount; ++slice)
        {
            uint8_t* dstSlice = dstBase + placed[slice].offset;
            const uint8_t* srcSlice = srcBase + (uint64_t)slice * srcSliceStride;

            // copy row by row into padded rows
            for (uint32_t y = 0; y < placed[slice].rows; ++y)
            {
                const uint8_t* srcRow = srcSlice + (uint64_t)y * unpaddedRowSize;
                uint8_t* dstRow = dstSlice + (uint64_t)y * placed[slice].paddedRowPitch;
                memcpy(dstRow, srcRow, (size_t)unpaddedRowSize);

                if (placed[slice].paddedRowPitch > placed[slice].rowSize)
                {
                    memset(dstRow + placed[slice].rowSize, 255, (size_t)(placed[slice].paddedRowPitch - placed[slice].rowSize));
                }
            }
        }

        // Create command buffer and blit encoder
        MTL::CommandBuffer* cmdBuf = context->m_CommandQueue->commandBuffer();

        // For each slice, issue copyFromBuffer with the placed footprint
        for (uint32_t slice = 0; slice < layerCount; ++slice)
        {
            const MetalPlacedSubresource& p = placed[slice];

            // destination slice (array index or cube face index)
            NSUInteger destSlice = (NSUInteger)(params.m_Slice + slice);

            // destination origin (subupdate offsets)
            MTL::Origin destOrigin = { (NSUInteger)params.m_X, (NSUInteger)params.m_Y, (NSUInteger)params.m_Z };

            // destination size
            MTL::Size copySize = { (NSUInteger)copyWidth, (NSUInteger)copyHeight, 1 };

            MTL::BlitCommandEncoder* blit = cmdBuf->blitCommandEncoder();

            blit->copyFromBuffer(
                uploadBuffer,
                (NSUInteger)p.offset,
                (NSUInteger)p.paddedRowPitch,
                (NSUInteger)p.sliceSize,
                copySize,
                texture->m_Texture,
                destSlice,
                (NSUInteger)target_mip,
                destOrigin
            );

            blit->endEncoding();
        }

        cmdBuf->commit();
        cmdBuf->waitUntilCompleted();

        // cleanup
        uploadBuffer->release();
    }

    static void CreateMetalDepthStencilTexture(MetalContext* context, MetalTexture* texture, const TextureParams& params, MTL::TextureUsage usage)
    {
        MTL::TextureDescriptor* desc = MTL::TextureDescriptor::texture2DDescriptor(
            MTL::PixelFormatDepth32Float_Stencil8,
            params.m_Width,
            params.m_Height,
            false
        );

        desc->setStorageMode(MTL::StorageModePrivate);
        desc->setUsage(usage);

        if (texture->m_Texture)
        {
            texture->m_Texture->release();
        }

        texture->m_Texture = context->m_Device->newTexture(desc);
        desc->release();

        texture->m_Base.m_Width       = params.m_Width;
        texture->m_Base.m_Height      = params.m_Height;
        texture->m_Base.m_Format      = params.m_Format;
        texture->m_Base.m_Depth       = 1;
        texture->m_Base.m_MipMapCount = 1;
        texture->m_LayerCount  = 1; // TODO: Move to base texture
    }

    static void CreateMetalTexture(MetalContext* context, MetalTexture* texture, const TextureParams& params, MTL::TextureUsage usage)
    {
        uint8_t tex_layer_count  = dmMath::Max(texture->m_LayerCount, params.m_LayerCount);
        uint8_t tex_array_length = tex_layer_count;
        uint16_t tex_depth       = dmMath::Max(texture->m_Base.m_Depth, params.m_Depth);
        uint8_t tex_mip_count    = 1;

        // Note:
        // If the texture has requested mipmaps and we need to recreate the texture, make sure to allocate enough mipmaps.
        // For vulkan this means that we can't cap a texture to a specific mipmap count since the engine expects
        // that setting texture data works like the OpenGL backend where we set the mipmap count to zero and then
        // update the mipmap count based on the params. If we recreate the texture when that is detected (i.e we have too few mipmaps in the texture)
        // we will lose all the data that was previously uploaded. We could copy that data, but for now this is the easiest way of dealing with this..

        if (texture->m_Base.m_MipMapCount > 1)
        {
            tex_mip_count = (uint16_t) GetMipmapCount(dmMath::Max(texture->m_Base.m_Width, texture->m_Base.m_Height));
        }

        MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();

        switch(texture->m_Base.m_Type)
        {
            case TEXTURE_TYPE_2D:
            case TEXTURE_TYPE_IMAGE_2D:
            case TEXTURE_TYPE_TEXTURE_2D:
                desc->setTextureType(MTL::TextureType2D);
                tex_depth = 1;
                break;
            case TEXTURE_TYPE_2D_ARRAY:
            case TEXTURE_TYPE_TEXTURE_2D_ARRAY:
                desc->setTextureType(MTL::TextureType2DArray);
                tex_depth = 1;
                break;
            case TEXTURE_TYPE_3D:
            case TEXTURE_TYPE_IMAGE_3D:
            case TEXTURE_TYPE_TEXTURE_3D:
                desc->setTextureType(MTL::TextureType3D);
                break;
            case TEXTURE_TYPE_CUBE_MAP:
            case TEXTURE_TYPE_TEXTURE_CUBE:
                desc->setTextureType(MTL::TextureTypeCube);
                tex_depth = 1;
                tex_array_length = 1;
                break;
            default:
                assert(0);
        }

        // Set pixel format
        desc->setPixelFormat(GetMetalPixelFormat(params.m_Format));

        // Set dimensions
        desc->setWidth(params.m_Width);
        desc->setHeight(params.m_Height);
        desc->setDepth(tex_depth);
        desc->setArrayLength(tex_array_length);
        desc->setMipmapLevelCount(tex_mip_count);
        desc->setSampleCount(1);
        desc->setStorageMode(MTL::StorageModePrivate);
        desc->setUsage(usage);

        // Create Metal texture
        if (texture->m_Texture)
        {
            texture->m_Texture->release();
        }
        texture->m_Texture = context->m_Device->newTexture(desc);
        desc->release();

        texture->m_Base.m_Width       = params.m_Width;
        texture->m_Base.m_Height      = params.m_Height;
        texture->m_Base.m_Depth       = tex_depth;
        texture->m_Base.m_Format      = params.m_Format;
        texture->m_Base.m_MipMapCount = tex_mip_count;
        texture->m_LayerCount         = tex_layer_count;
    }

    static void MetalSetTextureInternal(MetalContext* context, MetalTexture* texture, const TextureParams& params)
    {
        // Reject unsupported formats
        if (params.m_Format == TEXTURE_FORMAT_DEPTH || params.m_Format == TEXTURE_FORMAT_STENCIL)
        {
            dmLogError("Unable to upload texture data, unsupported type (%s).", GetTextureFormatLiteral(params.m_Format));
            return;
        }

        // Clamp size to Metal limits
        uint32_t maxSize = GetMaxTextureSize((HContext) context);
        assert(params.m_Width  <= maxSize);
        assert(params.m_Height <= maxSize);

        // Compute layer count, depth, and bits per pixel
        uint8_t tex_layer_count = dmMath::Max(texture->m_LayerCount, params.m_LayerCount);
        uint16_t tex_depth      = dmMath::Max(texture->m_Base.m_Depth, params.m_Depth);
        uint8_t tex_bpp         = GetTextureFormatBitsPerPixel(params.m_Format);
        size_t tex_data_size    = params.m_DataSize * tex_layer_count * 8; // bits
        void* tex_data_ptr      = (void*)params.m_Data;
        texture->m_Base.m_MipMapCount = dmMath::Max(texture->m_Base.m_MipMapCount, (uint8_t)(params.m_MipMap+1));

        // Expand RGB to RGBA if needed
        TextureFormat format_orig = params.m_Format;
        TextureFormat format_new = format_orig;

        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            uint32_t pixel_count = params.m_Width * params.m_Height * tex_layer_count;
            uint8_t* data_new = new uint8_t[pixel_count * 4]; // RGBA
            RepackRGBToRGBA(pixel_count, (uint8_t*)tex_data_ptr, data_new);
            tex_data_ptr = data_new;
            tex_bpp = 32;
            format_new = TEXTURE_FORMAT_RGBA;
        }

        // Compute tex_data_size in bytes
        tex_data_size = tex_bpp / 8 * params.m_Width * params.m_Height * tex_depth * tex_layer_count;

        if (params.m_SubUpdate)
        {
            // Same as vulkan
            tex_data_size = params.m_Width * params.m_Height * tex_bpp * tex_layer_count;
        }
        else if (params.m_MipMap == 0)
        {
            if (texture->m_Base.m_Format != params.m_Format ||
                texture->m_Base.m_Width != params.m_Width ||
                texture->m_Base.m_Height != params.m_Height ||
                (IsTextureType3D(texture->m_Base.m_Type) && (texture->m_Base.m_Depth != params.m_Depth)))
            {
                DestroyResourceDeferred(context, texture);
            }
        }

        if (texture->m_Destroyed || texture->m_Texture == 0x0)
        {
            CreateMetalTexture(context, texture, params, texture->m_Usage);
        }

        if (tex_data_ptr && tex_data_size > 0)
        {
            MetalCopyToTexture(context, texture, format_new, params, (const uint8_t*) tex_data_ptr);
        }

        // Clean up temporary RGB->RGBA conversion
        if (format_orig == TEXTURE_FORMAT_RGB)
        {
            delete[] (uint8_t*)tex_data_ptr;
        }
    }

    static void MetalSetTexture(HContext _context, HTexture texture, const TextureParams& params)
    {
        DM_PROFILE(__FUNCTION__);
        MetalContext* context = (MetalContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        MetalTexture* tex = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, texture);
        MetalSetTextureInternal(context, tex, params);
    }

    static int16_t GetTextureSamplerIndex(MetalContext* context, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, uint8_t maxLod, float max_anisotropy)
    {
        if (minfilter == TEXTURE_FILTER_DEFAULT)
        {
            minfilter = context->m_BaseContext.m_DefaultTextureMinFilter;
        }
        if (magfilter == TEXTURE_FILTER_DEFAULT)
        {
            magfilter = context->m_BaseContext.m_DefaultTextureMagFilter;
        }

        for (uint32_t i=0; i < context->m_TextureSamplers.Size(); i++)
        {
            const MetalTextureSampler& sampler = context->m_TextureSamplers[i];
            if (sampler.m_MagFilter     == magfilter &&
                sampler.m_MinFilter     == minfilter &&
                sampler.m_AddressModeU  == uwrap     &&
                sampler.m_AddressModeV  == vwrap     &&
                sampler.m_MaxLod        == maxLod    &&
                sampler.m_MaxAnisotropy == max_anisotropy)
            {
                return (uint8_t) i;
            }
        }

        return -1;
    }

    static inline float GetMaxAnisotrophyClamped(float requested)
    {
        // Metal does not expose a device query for this, but all Apple GPUs
        // and modern discrete GPUs support at least 16x anisotropy.
        const float MAX_SUPPORTED_ANISOTROPY = 16.0f;
        return (requested < MAX_SUPPORTED_ANISOTROPY) ? requested : MAX_SUPPORTED_ANISOTROPY;
    }

    static inline MTL::SamplerAddressMode GetMetalSamplerAddressMode(TextureWrap wrap)
    {
        switch (wrap)
        {
            case TEXTURE_WRAP_REPEAT:          return MTL::SamplerAddressModeRepeat;
            case TEXTURE_WRAP_MIRRORED_REPEAT: return MTL::SamplerAddressModeMirrorRepeat;
            case TEXTURE_WRAP_CLAMP_TO_EDGE:   return MTL::SamplerAddressModeClampToEdge;
            default:                           return MTL::SamplerAddressModeClampToEdge;
        }
    }

    static inline void GetMetalFilters(TextureFilter filter, MTL::SamplerMinMagFilter& outMin, MTL::SamplerMipFilter& outMip)
    {
        switch (filter)
        {
            case TEXTURE_FILTER_NEAREST:
                outMin = MTL::SamplerMinMagFilterNearest;
                outMip = MTL::SamplerMipFilterNotMipmapped;
                break;

            case TEXTURE_FILTER_LINEAR:
                outMin = MTL::SamplerMinMagFilterLinear;
                outMip = MTL::SamplerMipFilterNotMipmapped;
                break;

            case TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                outMin = MTL::SamplerMinMagFilterNearest;
                outMip = MTL::SamplerMipFilterNearest;
                break;

            case TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                outMin = MTL::SamplerMinMagFilterNearest;
                outMip = MTL::SamplerMipFilterLinear;
                break;

            case TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                outMin = MTL::SamplerMinMagFilterLinear;
                outMip = MTL::SamplerMipFilterNearest;
                break;

            case TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                outMin = MTL::SamplerMinMagFilterLinear;
                outMip = MTL::SamplerMipFilterLinear;
                break;

            default:
                outMin = MTL::SamplerMinMagFilterNearest;
                outMip = MTL::SamplerMipFilterNotMipmapped;
                break;
        }
    }

    static MTL::SamplerState* CreateMetalTextureSampler(
        MTL::Device* device,
        MTL::SamplerMinMagFilter minFilter,
        MTL::SamplerMinMagFilter magFilter,
        MTL::SamplerMipFilter mipFilter,
        MTL::SamplerAddressMode wrapU,
        MTL::SamplerAddressMode wrapV,
        float minLod,
        float maxLod,
        float maxAnisotropy)
    {
        using namespace MTL;

        // Create and configure the sampler descriptor
        SamplerDescriptor* desc = SamplerDescriptor::alloc()->init();
        desc->setMinFilter(minFilter);
        desc->setMagFilter(magFilter);
        desc->setMipFilter(mipFilter);

        desc->setSAddressMode(wrapU);
        desc->setTAddressMode(wrapV);
        desc->setRAddressMode(wrapU); // Metal allows 3D address mode too

        desc->setLodMinClamp(minLod);
        desc->setLodMaxClamp(maxLod);
        desc->setSupportArgumentBuffers(true);

        if (maxAnisotropy > 1.0f)
            desc->setMaxAnisotropy(maxAnisotropy);

        // Metal always normalizes texture coordinates
        // (no unnormalizedCoordinates option like Vulkan)

        // Metal doesn't support border color — it clamps or repeats instead
        // so you must pick the appropriate address mode for that

        // Create the sampler state
        SamplerState* sampler = device->newSamplerState(desc);
        desc->release();

        return sampler;
    }

    static int16_t CreateTextureSampler(
            MetalContext* context,
            TextureFilter minFilter,
            TextureFilter magFilter,
            TextureWrap uWrap,
            TextureWrap vWrap,
            uint8_t maxLod,
            float maxAnisotropy)
        {
            // Resolve defaults
            if (magFilter == TEXTURE_FILTER_DEFAULT)
                magFilter = context->m_BaseContext.m_DefaultTextureMagFilter;

            if (minFilter == TEXTURE_FILTER_DEFAULT)
                minFilter = context->m_BaseContext.m_DefaultTextureMinFilter;

            // Determine Metal min/mip filters (based on minFilter)
            MTL::SamplerMinMagFilter metalMinFilter;
            MTL::SamplerMipFilter metalMipFilter;
            GetMetalFilters(minFilter, metalMinFilter, metalMipFilter);

            // Magnification filter ignores the mip component completely
            MTL::SamplerMinMagFilter metalMagFilter =
                (magFilter == TEXTURE_FILTER_LINEAR)
                    ? MTL::SamplerMinMagFilterLinear
                    : MTL::SamplerMinMagFilterNearest;

            float maxLodFloat = static_cast<float>(maxLod);

            // Wrap modes
            MTL::SamplerAddressMode wrapU = GetMetalSamplerAddressMode(uWrap);
            MTL::SamplerAddressMode wrapV = GetMetalSamplerAddressMode(vWrap);

            // Build sampler data
            MetalTextureSampler newSampler = {};
            newSampler.m_MinFilter = minFilter;
            newSampler.m_MagFilter = magFilter;
            newSampler.m_AddressModeU = uWrap;
            newSampler.m_AddressModeV = vWrap;
            newSampler.m_MaxLod = maxLod;
            newSampler.m_MaxAnisotropy = maxAnisotropy;

            uint32_t samplerIndex = context->m_TextureSamplers.Size();
            if (context->m_TextureSamplers.Full())
                context->m_TextureSamplers.OffsetCapacity(1);

            // Create Metal sampler state object
            newSampler.m_Sampler = CreateMetalTextureSampler(
                context->m_Device,
                metalMinFilter,
                metalMagFilter,
                metalMipFilter,
                wrapU,
                wrapV,
                0.0f,
                maxLodFloat,
                maxAnisotropy
            );

            context->m_TextureSamplers.Push(newSampler);
            return (int16_t)samplerIndex;
        }

    static void MetalSetTextureParamsInternal(MetalContext* context, MetalTexture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        const MetalTextureSampler& sampler = context->m_TextureSamplers[texture->m_TextureSamplerIndex];
        float anisotropy_clamped = GetMaxAnisotrophyClamped(max_anisotropy);

        if (sampler.m_MinFilter     != minfilter                     ||
            sampler.m_MagFilter     != magfilter                     ||
            sampler.m_AddressModeU  != uwrap                         ||
            sampler.m_AddressModeV  != vwrap                         ||
            sampler.m_MaxLod        != texture->m_Base.m_MipMapCount ||
            sampler.m_MaxAnisotropy != anisotropy_clamped)
        {
            int16_t sampler_index = GetTextureSamplerIndex(context, minfilter, magfilter, uwrap, vwrap, texture->m_Base.m_MipMapCount, anisotropy_clamped);
            if (sampler_index < 0)
            {
                sampler_index = CreateTextureSampler(context, minfilter, magfilter, uwrap, vwrap, texture->m_Base.m_MipMapCount, anisotropy_clamped);
            }
            texture->m_TextureSamplerIndex = sampler_index;
        }
    }

    static void MetalSetTextureAsync(HContext _context, HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        // TODO
        SetTexture(_context, texture, params);
        if (callback)
        {
            callback(texture, user_data);
        }
    }

    static void MetalSetTextureParams(HContext _context, HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        MetalContext* context = (MetalContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        MetalTexture* tex = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, texture);
        MetalSetTextureParamsInternal(context, tex, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
    }

    static uint32_t MetalGetTextureResourceSize(HContext _context, HTexture texture)
    {
        MetalContext* context = (MetalContext*)_context;
        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        MetalTexture* tex = GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, texture);
        if (!tex)
        {
            return 0;
        }
        uint32_t size_total = 0;
        uint32_t size = tex->m_Base.m_Width * tex->m_Base.m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(tex->m_Base.m_Format)/8);
        for(uint32_t i = 0; i < tex->m_Base.m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        if (tex->m_Base.m_Type == TEXTURE_TYPE_CUBE_MAP)
        {
            size_total *= 6;
        }
        return size_total + sizeof(MetalTexture);
    }

    static void MetalEnableTexture(HContext _context, uint32_t unit, uint8_t id_index, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        ((MetalContext*)_context)->m_TextureUnits[unit] = texture;
    }

    static void MetalDisableTexture(HContext _context, uint32_t unit, HTexture texture)
    {
        assert(unit < DM_MAX_TEXTURE_UNITS);
        ((MetalContext*)_context)->m_TextureUnits[unit] = 0;
    }

    static uint32_t MetalGetMaxTextureSize(HContext _context)
    {
        MetalContext* context = (MetalContext*)_context;
        if (context->m_Device->supportsFamily(MTL::GPUFamilyApple5) ||
            context->m_Device->supportsFamily(MTL::GPUFamilyMac1))
        {
            return 16384;
        }
        return 8192;
    }

    static void MetalReadPixels(HContext _context, int32_t x, int32_t y, uint32_t width, uint32_t height, void* buffer, uint32_t buffer_size)
    {

    }

    static void MetalRunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {

    }

    static HandleResult MetalGetTextureHandle(HTexture texture, void** out_handle)
    {
        return HANDLE_RESULT_OK;
    }

    static bool MetalIsExtensionSupported(HContext _context, const char* extension)
    {
        return 0;
    }

    static uint32_t MetalGetNumSupportedExtensions(HContext _context)
    {
        return 0;
    }

    static const char* MetalGetSupportedExtension(HContext _context, uint32_t index)
    {
        return 0;
    }

    static bool MetalIsContextFeatureSupported(HContext _context, ContextFeature feature)
    {
        return true;
    }

    static PipelineState MetalGetPipelineState(HContext _context)
    {
        MetalContext* context = (MetalContext*)_context;
        return context->m_PipelineState;
    }

    static uint8_t MetalGetTexturePageCount(HTexture texture)
    {
        DM_MUTEX_SCOPED_LOCK(g_MetalContext->m_BaseContext.m_AssetHandleContainerMutex);
        MetalTexture* tex = GetAssetFromContainer<MetalTexture>(g_MetalContext->m_BaseContext.m_AssetHandleContainer, texture);
        return tex ? tex->m_Base.m_PageCount : 0;
    }

    static bool MetalIsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
    {
        if (asset_handle == 0)
        {
            return false;
        }

        MetalContext* context = (MetalContext*) _context;
        AssetType type         = GetAssetType(asset_handle);

        DM_MUTEX_SCOPED_LOCK(context->m_BaseContext.m_AssetHandleContainerMutex);
        if (type == ASSET_TYPE_TEXTURE)
        {
            return GetAssetFromContainer<MetalTexture>(context->m_BaseContext.m_AssetHandleContainer, asset_handle) != 0;
        }
        else if (type == ASSET_TYPE_RENDER_TARGET)
        {
            return GetAssetFromContainer<MetalRenderTarget>(context->m_BaseContext.m_AssetHandleContainer, asset_handle) != 0;
        }
        return false;
    }

    static void MetalInvalidateGraphicsHandles(HContext _context)
    {

    }

    static void MetalGetViewport(HContext _context, int32_t* x, int32_t* y, uint32_t* width, uint32_t* height)
    {
        MetalContext* context = (MetalContext*)_context;
        const MetalViewport& viewport = context->m_MainViewport;
        *x = viewport.m_X, *y = viewport.m_Y, *width = viewport.m_W, *height = viewport.m_H;
    }

    static GraphicsAdapterFunctionTable MetalRegisterFunctionTable()
    {
        GraphicsAdapterFunctionTable fn_table = {};
        DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, Metal);
        return fn_table;
    }
}
