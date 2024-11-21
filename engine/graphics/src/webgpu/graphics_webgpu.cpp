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
#include "graphics_webgpu_private.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/html5.h>
#endif

#if 0
#define TRACE_CALL printf("Call %s:%d %s\n", __FILE__, __LINE__, __func__)
#else
#define TRACE_CALL
#endif

using namespace dmGraphics;
using namespace dmVMath;

static const WGPUAddressMode g_webgpu_address_mode[] = {
    WGPUAddressMode_ClampToEdge,
    WGPUAddressMode_ClampToEdge,
    WGPUAddressMode_MirrorRepeat,
    WGPUAddressMode_Repeat
};

static const WGPUStencilOperation g_webgpu_stencil_ops[] = {
    WGPUStencilOperation_Keep,
    WGPUStencilOperation_Zero,
    WGPUStencilOperation_Replace,
    WGPUStencilOperation_IncrementClamp,
    WGPUStencilOperation_IncrementWrap,
    WGPUStencilOperation_DecrementClamp,
    WGPUStencilOperation_DecrementWrap,
    WGPUStencilOperation_Invert
};

static const WGPUCompareFunction g_webgpu_compare_funcs[] = {
    WGPUCompareFunction_Never,
    WGPUCompareFunction_Less,
    WGPUCompareFunction_LessEqual,
    WGPUCompareFunction_Greater,
    WGPUCompareFunction_GreaterEqual,
    WGPUCompareFunction_Equal,
    WGPUCompareFunction_NotEqual,
    WGPUCompareFunction_Always
};

static const WGPUBlendFactor g_webgpu_blend_factors[] = {
    WGPUBlendFactor_Zero,
    WGPUBlendFactor_One,
    WGPUBlendFactor_Src,
    WGPUBlendFactor_OneMinusSrc,
    WGPUBlendFactor_Dst,
    WGPUBlendFactor_OneMinusDst,
    WGPUBlendFactor_SrcAlpha,
    WGPUBlendFactor_OneMinusSrcAlpha,
    WGPUBlendFactor_DstAlpha,
    WGPUBlendFactor_OneMinusDstAlpha,
    WGPUBlendFactor_SrcAlphaSaturated
};

static GraphicsAdapterFunctionTable WebGPURegisterFunctionTable();
static bool WebGPUIsSupported();
static HContext WebGPUGetContext();
static GraphicsAdapter g_webgpu_adapter(ADAPTER_FAMILY_WEBGPU);
static const int8_t g_webgpu_adapter_priority = 0;
static WebGPUContext* g_WebGPUContext         = NULL;

DM_REGISTER_GRAPHICS_ADAPTER(GraphicsAdapterWebGPU, &g_webgpu_adapter, WebGPUIsSupported, WebGPURegisterFunctionTable, WebGPUGetContext, g_webgpu_adapter_priority);

static WGPUSampler WebGPUGetOrCreateSampler(WebGPUContext* context, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
{
    HashState64 sampler_hash_state;
    dmHashInit64(&sampler_hash_state, false);
    dmHashUpdateBuffer64(&sampler_hash_state, &minfilter, sizeof(minfilter));
    dmHashUpdateBuffer64(&sampler_hash_state, &magfilter, sizeof(magfilter));
    dmHashUpdateBuffer64(&sampler_hash_state, &uwrap, sizeof(uwrap));
    dmHashUpdateBuffer64(&sampler_hash_state, &vwrap, sizeof(vwrap));
    dmHashUpdateBuffer64(&sampler_hash_state, &max_anisotropy, sizeof(max_anisotropy));

    const uint64_t sampler_hash = dmHashFinal64(&sampler_hash_state);
    if (WGPUSampler* cached_sampler = context->m_SamplerCache.Get(sampler_hash))
        return *cached_sampler;

    WGPUSamplerDescriptor desc = {};
    desc.lodMinClamp           = 0;
    desc.lodMaxClamp           = 32;

    desc.maxAnisotropy = uint16_t(max_anisotropy);
    desc.addressModeU  = g_webgpu_address_mode[uwrap];
    desc.addressModeV  = g_webgpu_address_mode[vwrap];

    if (magfilter == TEXTURE_FILTER_DEFAULT)
        magfilter = context->m_DefaultTextureMagFilter;
    switch (magfilter)
    {
        case TEXTURE_FILTER_NEAREST:
            desc.magFilter = WGPUFilterMode_Nearest;
            break;
        case TEXTURE_FILTER_LINEAR:
            desc.magFilter = WGPUFilterMode_Linear;
            break;
        default:
            assert(false);
    }

    if (minfilter == TEXTURE_FILTER_DEFAULT)
        minfilter = context->m_DefaultTextureMinFilter;
    switch (minfilter)
    {
        case TEXTURE_FILTER_NEAREST:
            desc.magFilter   = WGPUFilterMode_Nearest;
            desc.lodMaxClamp = 0.25f;
            break;
        case TEXTURE_FILTER_LINEAR:
            desc.magFilter   = WGPUFilterMode_Linear;
            desc.lodMaxClamp = 0.25f;
            break;
        case TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
            desc.minFilter    = WGPUFilterMode_Nearest;
            desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
            break;
        case TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
            desc.minFilter    = WGPUFilterMode_Nearest;
            desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
            break;
        case TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
            desc.minFilter    = WGPUFilterMode_Linear;
            desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
            break;
        case TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
            desc.minFilter    = WGPUFilterMode_Linear;
            desc.mipmapFilter = WGPUMipmapFilterMode_Linear;
            break;
        default:
            assert(false);
    }

    WGPUSampler sampler = wgpuDeviceCreateSampler(context->m_Device, &desc);
    if (context->m_SamplerCache.Full())
        context->m_SamplerCache.SetCapacity(32, context->m_SamplerCache.Capacity() + 4);
    context->m_SamplerCache.Put(sampler_hash, sampler);
    return sampler;
}

static WebGPUTexture* WebGPUNewTextureInternal(const TextureCreationParams& params)
{
    TRACE_CALL;
    WebGPUTexture* texture = new WebGPUTexture;
    texture->m_Type        = params.m_Type;
    texture->m_Width       = params.m_Width;
    texture->m_Height      = params.m_Height;
    texture->m_Depth       = params.m_Depth;
    texture->m_MipMapCount = params.m_MipMapCount;
    if (params.m_UsageHintBits & TEXTURE_USAGE_FLAG_SAMPLE)
        texture->m_UsageFlags |= WGPUTextureUsage_TextureBinding;
    if (params.m_UsageHintBits & TEXTURE_USAGE_FLAG_STORAGE)
        texture->m_UsageFlags |= WGPUTextureUsage_StorageBinding;
    texture->m_UsageHintFlags = params.m_UsageHintBits;

    if (params.m_OriginalWidth == 0)
    {
        texture->m_OriginalWidth  = params.m_Width;
        texture->m_OriginalHeight = params.m_Height;
    }
    else
    {
        texture->m_OriginalWidth  = params.m_OriginalWidth;
        texture->m_OriginalHeight = params.m_OriginalHeight;
    }
    return texture;
}

static void WebGPUSetTextureParamsInternal(WebGPUTexture* texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
{
    TRACE_CALL;
    texture->m_Sampler = WebGPUGetOrCreateSampler(g_WebGPUContext, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
}

static WGPUTextureFormat WebGPUFormatFromTextureFormat(TextureFormat format)
{
    assert(format <= TEXTURE_FORMAT_COUNT);
    switch (format)
    {
        case TEXTURE_FORMAT_LUMINANCE:
            return WGPUTextureFormat_R8Unorm;
        case TEXTURE_FORMAT_LUMINANCE_ALPHA:
            return WGPUTextureFormat_RG8Unorm;
        case TEXTURE_FORMAT_RGB:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGBA:
            return WGPUTextureFormat_RGBA8Unorm;
        case TEXTURE_FORMAT_RGB_16BPP:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGBA_16BPP:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_DEPTH:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_STENCIL:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGB_ETC1:
            return WGPUTextureFormat_ETC2RGB8Unorm;
        case TEXTURE_FORMAT_RGBA_ETC2:
            return WGPUTextureFormat_ETC2RGBA8Unorm;
        case TEXTURE_FORMAT_RGBA_ASTC_4x4:
            return WGPUTextureFormat_ASTC4x4Unorm;
        case TEXTURE_FORMAT_RGB_BC1:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGBA_BC3:
            return WGPUTextureFormat_BC3RGBAUnorm;
        case TEXTURE_FORMAT_RGBA_BC7:
            return WGPUTextureFormat_BC7RGBAUnorm;
        case TEXTURE_FORMAT_R_BC4:
            return WGPUTextureFormat_BC4RUnorm;
        case TEXTURE_FORMAT_RG_BC5:
            return WGPUTextureFormat_BC5RGUnorm;
        case TEXTURE_FORMAT_RGB16F:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGB32F:
            return WGPUTextureFormat_Undefined;
        case TEXTURE_FORMAT_RGBA16F:
            return WGPUTextureFormat_RGBA16Float;
        case TEXTURE_FORMAT_RGBA32F:
            return WGPUTextureFormat_RGBA32Float;
        case TEXTURE_FORMAT_R16F:
            return WGPUTextureFormat_R16Float;
        case TEXTURE_FORMAT_RG16F:
            return WGPUTextureFormat_RG16Float;
        case TEXTURE_FORMAT_R32F:
            return WGPUTextureFormat_R32Float;
        case TEXTURE_FORMAT_RG32F:
            return WGPUTextureFormat_RG32Float;
        case TEXTURE_FORMAT_RGBA32UI:
            return WGPUTextureFormat_RGBA32Uint;
        case TEXTURE_FORMAT_BGRA8U:
            return WGPUTextureFormat_BGRA8Unorm;
        case TEXTURE_FORMAT_R32UI:
            return WGPUTextureFormat_R32Uint;
        default:
            return WGPUTextureFormat_Undefined;
    };
}

static size_t WebGPUCompressedBlockWidth(TextureFormat format)
{
    assert(format <= TEXTURE_FORMAT_COUNT);
    switch (format)
    {
    case TEXTURE_FORMAT_RGB_ETC1:           return 4;
    case TEXTURE_FORMAT_RGBA_ETC2:          return 4;
    case TEXTURE_FORMAT_RGBA_ASTC_4x4:      return 4;
    case TEXTURE_FORMAT_RGB_BC1:            return 4;
    case TEXTURE_FORMAT_RGBA_BC3:           return 4;
    case TEXTURE_FORMAT_RGBA_BC7:           return 4;
    default:                                return 0;
    };
}

static size_t WebGPUCompressedBlockByteSize(TextureFormat format)
{
    assert(format <= TEXTURE_FORMAT_COUNT);
    switch (format)
    {
    case TEXTURE_FORMAT_RGB_ETC1:           return 8;
    case TEXTURE_FORMAT_RGBA_ETC2:          return 8;
    case TEXTURE_FORMAT_RGBA_ASTC_4x4:      return 16;
    case TEXTURE_FORMAT_RGB_BC1:            return 8;
    case TEXTURE_FORMAT_RGBA_BC3:           return 16;
    case TEXTURE_FORMAT_RGBA_BC7:           return 16;
    default:                                return 0;
    };
}

static void WebGPURealizeTexture(WebGPUTexture* texture, WGPUTextureFormat format, uint8_t depth, uint32_t sampleCount, WGPUTextureUsage usage)
{
    if (texture->m_Depth > depth)
        depth = texture->m_Depth;

    assert(!texture->m_Texture && !texture->m_TextureView);
    texture->m_Format = format;

    {
        WGPUTextureDescriptor desc = {};
        desc.usage                 = texture->m_UsageFlags | usage;
        desc.dimension             = WGPUTextureDimension_2D;
        desc.size                  = { texture->m_Width, texture->m_Height, depth };
        desc.sampleCount           = sampleCount;
        desc.format                = texture->m_Format;
        desc.mipLevelCount         = texture->m_MipMapCount;
        texture->m_Texture         = wgpuDeviceCreateTexture(g_WebGPUContext->m_Device, &desc);
    }
    {
        WGPUTextureViewDescriptor desc = {};
        desc.format                    = texture->m_Format;
        switch (texture->m_Type)
        {
            case TEXTURE_TYPE_2D_ARRAY:
                desc.dimension = WGPUTextureViewDimension_2DArray;
                break;
            case TEXTURE_TYPE_CUBE_MAP:
            case TEXTURE_TYPE_TEXTURE_CUBE:
                desc.dimension = WGPUTextureViewDimension_Cube;
                break;
            default:
                desc.dimension = WGPUTextureViewDimension_2D;
                break;
        }
        desc.mipLevelCount     = texture->m_MipMapCount;
        desc.arrayLayerCount   = depth;
        desc.aspect            = WGPUTextureAspect_All;
        texture->m_TextureView = wgpuTextureCreateView(texture->m_Texture, &desc);
    }
}

static void WebGPUSetTextureInternal(WebGPUTexture* texture, const TextureParams& params)
{
    switch (params.m_Format)
    {
        case TEXTURE_FORMAT_DEPTH:
        case TEXTURE_FORMAT_STENCIL:
            dmLogError("Unable to upload texture data, unsupported type (%s).", GetTextureFormatLiteral(params.m_Format));
            return;
        default:
            break;
    }

    assert(params.m_Width <= g_WebGPUContext->m_DeviceLimits.limits.maxTextureDimension2D);
    assert(params.m_Height <= g_WebGPUContext->m_DeviceLimits.limits.maxTextureDimension2D);

    if (texture->m_MipMapCount == 1 && params.m_MipMap > 0)
        return;

    {
        WGPUImageCopyTexture dest = {};
        dest.mipLevel             = params.m_MipMap;
        dest.origin               = { params.m_X, params.m_Y, params.m_Z };
        dest.aspect               = WGPUTextureAspect_All;

        WGPUExtent3D extent = {};
        extent.width        = params.m_Width;
        extent.height       = params.m_Height;

        WGPUTextureDataLayout layout = {};
        layout.offset                = 0;
        layout.rowsPerImage          = extent.height;

        const uint8_t depth = dmMath::Max(texture->m_Depth, params.m_Depth);
        if (params.m_Format == TEXTURE_FORMAT_RGB) // Not really supported, so transcode it
        {
            if (!texture->m_Texture)
            {
                assert(!params.m_SubUpdate);
                WebGPURealizeTexture(texture, WGPUTextureFormat_RGBA8Unorm, params.m_Depth, 1, WGPUTextureUsage_CopyDst);
                assert(texture->m_Texture && texture->m_TextureView);
                texture->m_GraphicsFormat = TEXTURE_FORMAT_RGBA;
            }
            const uint8_t repackBPP     = 4;
            const uint32_t repackPixels = params.m_Width * params.m_Height * depth;
            uint8_t* repackData         = new uint8_t[repackPixels * repackBPP];
            RepackRGBToRGBA(repackPixels, (uint8_t*)params.m_Data, repackData);

            dest.texture              = texture->m_Texture;
            layout.bytesPerRow        = extent.width * repackBPP;
            extent.depthOrArrayLayers = depth;
            wgpuQueueWriteTexture(g_WebGPUContext->m_Queue, &dest, repackData, repackPixels * repackBPP, &layout, &extent);

            delete[] repackData;
        }
        else
        {
            if (!texture->m_Texture)
            {
                assert(!params.m_SubUpdate);
                WebGPURealizeTexture(texture, WebGPUFormatFromTextureFormat(params.m_Format), params.m_Depth, 1, WGPUTextureUsage_CopyDst);
                assert(texture->m_Texture && texture->m_TextureView);
                texture->m_GraphicsFormat = params.m_Format;
            }
            const uint8_t bpp     = ceil(GetTextureFormatBitsPerPixel(params.m_Format) / 8.0f);
            const size_t dataSize = bpp * params.m_Width * params.m_Height * depth;

            dest.texture = texture->m_Texture;
            layout.bytesPerRow = extent.width;
            if(IsTextureFormatCompressed(params.m_Format))
                layout.bytesPerRow = (layout.bytesPerRow / WebGPUCompressedBlockWidth(params.m_Format)) * WebGPUCompressedBlockByteSize(params.m_Format);
            else
                layout.bytesPerRow *= bpp;
            extent.depthOrArrayLayers = depth;
            wgpuQueueWriteTexture(g_WebGPUContext->m_Queue, &dest, params.m_Data, dataSize, &layout, &extent);
        }
    }

    WebGPUSetTextureParamsInternal(texture, params.m_MinFilter, params.m_MagFilter, params.m_UWrap, params.m_VWrap, 1.0f);
}

static inline WGPUVertexFormat WebGPUDeduceVertexAttributeFormat(Type type, uint16_t size, bool normalized)
{
    if (type == TYPE_FLOAT)
    {
        switch (size)
        {
            case 1:
                return WGPUVertexFormat_Float32;
            case 2:
                return WGPUVertexFormat_Float32x2;
            case 3:
                return WGPUVertexFormat_Float32x3;
            case 4:
                return WGPUVertexFormat_Float32x4;
            default:
                break;
        }
    }
    else if (type == TYPE_INT)
    {
        switch (size)
        {
            case 1:
                return WGPUVertexFormat_Sint32;
            case 2:
                return WGPUVertexFormat_Sint32x2;
            case 3:
                return WGPUVertexFormat_Sint32x3;
            case 4:
                return WGPUVertexFormat_Sint32x4;
            default:
                break;
        }
    }
    else if (type == TYPE_UNSIGNED_INT)
    {
        switch (size)
        {
            case 1:
                return WGPUVertexFormat_Uint32;
            case 2:
                return WGPUVertexFormat_Uint32x2;
            case 3:
                return WGPUVertexFormat_Uint32x3;
            case 4:
                return WGPUVertexFormat_Uint32x4;
            default:
                break;
        }
    }
    else if (type == TYPE_BYTE)
    {
        switch (size)
        {
            case 2:
                return normalized ? WGPUVertexFormat_Snorm8x2 : WGPUVertexFormat_Sint8x2;
            case 4:
                return normalized ? WGPUVertexFormat_Snorm8x4 : WGPUVertexFormat_Sint8x4;
            default:
                break;
        }
    }
    else if (type == TYPE_UNSIGNED_BYTE)
    {
        switch (size)
        {
            case 2:
                return normalized ? WGPUVertexFormat_Unorm8x2 : WGPUVertexFormat_Uint8x2;
            case 4:
                return normalized ? WGPUVertexFormat_Unorm8x4 : WGPUVertexFormat_Uint8x4;
            default:
                break;
        }
    }
    else if (type == TYPE_SHORT)
    {
        switch (size)
        {
            case 2:
                return normalized ? WGPUVertexFormat_Snorm16x2 : WGPUVertexFormat_Sint16x2;
            case 4:
                return normalized ? WGPUVertexFormat_Snorm16x4 : WGPUVertexFormat_Sint16x4;
            default:
                break;
        }
    }
    else if (type == TYPE_UNSIGNED_SHORT)
    {
        switch (size)
        {
            case 2:
                return normalized ? WGPUVertexFormat_Unorm16x2 : WGPUVertexFormat_Uint16x2;
            case 4:
                return normalized ? WGPUVertexFormat_Unorm16x4 : WGPUVertexFormat_Uint16x4;
            default:
                break;
        }
    }
    else if (type == TYPE_FLOAT_MAT4)
    {
        return WGPUVertexFormat_Float32;
    }
    else if (type == TYPE_FLOAT_VEC4)
    {
        return WGPUVertexFormat_Float32x4;
    }
    assert(0 && "Unable to deduce type from dmGraphics::Type");
    return WGPUVertexFormat_Undefined;
}

static WGPUComputePipeline WebGPUGetOrCreateComputePipeline(WebGPUContext* context)
{
    HashState64 pipeline_hash_state;
    dmHashInit64(&pipeline_hash_state, false);
    dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentProgram->m_Hash, sizeof(context->m_CurrentProgram->m_Hash));

    const uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);
    if (WGPUComputePipeline* cached_pipeline = context->m_ComputePipelineCache.Get(pipeline_hash))
        return *cached_pipeline;

    WGPUComputePipelineDescriptor desc = {};

    // layout
    desc.layout = context->m_CurrentProgram->m_PipelineLayout;

    // shader
    desc.compute.entryPoint = "main";
    desc.compute.module     = context->m_CurrentProgram->m_ComputeModule->m_Module;

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(context->m_Device, &desc);
    if (context->m_ComputePipelineCache.Full())
        context->m_ComputePipelineCache.SetCapacity(32, context->m_ComputePipelineCache.Capacity() + 4);
    context->m_ComputePipelineCache.Put(pipeline_hash, pipeline);
    return pipeline;
}

static WGPURenderPipeline WebGPUGetOrCreateRenderPipeline(WebGPUContext* context)
{
    HashState64 pipeline_hash_state;
    dmHashInit64(&pipeline_hash_state, false);
    dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentProgram->m_Hash, sizeof(context->m_CurrentProgram->m_Hash));
    dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentPipelineState, sizeof(context->m_CurrentPipelineState));
    dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentRenderPass.m_Target, sizeof(context->m_CurrentRenderPass.m_Target));
    for (int i = 0, d = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        if (context->m_CurrentVertexBuffers[i])
        {
            dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentVertexDeclaration[d]->m_PipelineHash, sizeof(context->m_CurrentVertexDeclaration[d]->m_PipelineHash));
            dmHashUpdateBuffer64(&pipeline_hash_state, &context->m_CurrentVertexDeclaration[d]->m_StepFunction, sizeof(context->m_CurrentVertexDeclaration[d]->m_StepFunction));
            ++d;
        }
    }

    const uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);
    if (WGPURenderPipeline* cached_pipeline = context->m_RenderPipelineCache.Get(pipeline_hash))
        return *cached_pipeline;

    WGPURenderPipelineDescriptor desc = {};

    // multisample
    desc.multisample.count = context->m_CurrentRenderPass.m_Target->m_Multisample;
    desc.multisample.mask  = 0xFFFFFFFF;

    // layout
    desc.layout = context->m_CurrentProgram->m_PipelineLayout;

    // vertex
    desc.vertex.entryPoint = "main";
    desc.vertex.module     = context->m_CurrentProgram->m_VertexModule->m_Module;
    WGPUVertexAttribute vertexAttributes[MAX_VERTEX_STREAM_COUNT];
    WGPUVertexBufferLayout vertexBuffers[MAX_VERTEX_BUFFERS];
    for (int i = 0, attributes = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        if (context->m_CurrentVertexBuffers[i])
        {
            VertexDeclaration* declaration                     = context->m_CurrentVertexDeclaration[desc.vertex.bufferCount];
            vertexBuffers[desc.vertex.bufferCount]             = {};
            vertexBuffers[desc.vertex.bufferCount].arrayStride = declaration->m_Stride;
            if (declaration->m_StepFunction == VERTEX_STEP_FUNCTION_VERTEX)
                vertexBuffers[desc.vertex.bufferCount].stepMode = WGPUVertexStepMode_Vertex;
            else
                vertexBuffers[desc.vertex.bufferCount].stepMode = WGPUVertexStepMode_Instance;
            if (declaration->m_StreamCount)
            {
                vertexBuffers[desc.vertex.bufferCount].attributeCount = declaration->m_StreamCount;
                vertexBuffers[desc.vertex.bufferCount].attributes     = vertexAttributes + attributes;
                for (uint16_t s = 0; s < declaration->m_StreamCount; ++s)
                {
                    vertexAttributes[attributes]                = {};
                    vertexAttributes[attributes].offset         = declaration->m_Streams[s].m_Offset;
                    vertexAttributes[attributes].shaderLocation = declaration->m_Streams[s].m_Location;
                    vertexAttributes[attributes].format         = WebGPUDeduceVertexAttributeFormat(declaration->m_Streams[s].m_Type,
                                                                                            declaration->m_Streams[s].m_Size,
                                                                                            declaration->m_Streams[s].m_Normalize);
                    ++attributes;
                }
                ++desc.vertex.bufferCount;
            }
        }
    }
    if (desc.vertex.bufferCount)
        desc.vertex.buffers = vertexBuffers;

    // primitives
    switch (context->m_CurrentPipelineState.m_PrimtiveType)
    {
        case PRIMITIVE_LINES:
            desc.primitive.topology = WGPUPrimitiveTopology_LineList;
            break;
        case PRIMITIVE_TRIANGLES:
            desc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
            break;
        case PRIMITIVE_TRIANGLE_STRIP:
            desc.primitive.topology = WGPUPrimitiveTopology_TriangleStrip;
            break;
    }
    if (!context->m_CurrentPipelineState.m_CullFaceEnabled)
        desc.primitive.cullMode = WGPUCullMode_None;
    else if (context->m_CurrentPipelineState.m_CullFaceType == FACE_TYPE_FRONT)
        desc.primitive.cullMode = WGPUCullMode_Front;
    else if (context->m_CurrentPipelineState.m_CullFaceType == FACE_TYPE_BACK)
        desc.primitive.cullMode = WGPUCullMode_Back;
    else if (context->m_CurrentPipelineState.m_CullFaceType == FACE_TYPE_FRONT_AND_BACK)
        desc.primitive.cullMode = WGPUCullMode(WGPUCullMode_Front | WGPUCullMode_Back);
    desc.primitive.frontFace = WGPUFrontFace_CCW;

    // depth stencil
    WGPUDepthStencilState depthstencil_desc = {};
    if (context->m_CurrentRenderPass.m_Target->m_TextureDepthStencil)
    {
        WebGPUTexture* texture              = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_CurrentRenderPass.m_Target->m_TextureDepthStencil);
        depthstencil_desc.format            = texture->m_Format;
        depthstencil_desc.depthWriteEnabled = context->m_CurrentPipelineState.m_WriteDepth;
        depthstencil_desc.depthCompare      = context->m_CurrentPipelineState.m_DepthTestEnabled ? g_webgpu_compare_funcs[context->m_CurrentPipelineState.m_DepthTestFunc] : WGPUCompareFunction_Always;
        if (context->m_CurrentPipelineState.m_StencilEnabled)
        {
            depthstencil_desc.stencilFront.compare     = g_webgpu_compare_funcs[context->m_CurrentPipelineState.m_StencilFrontTestFunc];
            depthstencil_desc.stencilFront.failOp      = g_webgpu_stencil_ops[context->m_CurrentPipelineState.m_StencilFrontOpFail];
            depthstencil_desc.stencilFront.depthFailOp = g_webgpu_stencil_ops[context->m_CurrentPipelineState.m_StencilFrontOpDepthFail];
            depthstencil_desc.stencilFront.passOp      = g_webgpu_stencil_ops[context->m_CurrentPipelineState.m_StencilFrontOpPass];
            depthstencil_desc.stencilBack.compare      = g_webgpu_compare_funcs[context->m_CurrentPipelineState.m_StencilFrontTestFunc];
            depthstencil_desc.stencilBack.failOp       = g_webgpu_stencil_ops[context->m_CurrentPipelineState.m_StencilBackOpFail];
            depthstencil_desc.stencilBack.depthFailOp  = g_webgpu_stencil_ops[context->m_CurrentPipelineState.m_StencilBackOpDepthFail];
            depthstencil_desc.stencilBack.passOp       = g_webgpu_stencil_ops[context->m_CurrentPipelineState.m_StencilFrontOpPass];
        }
        else
        {
            depthstencil_desc.stencilFront.compare     = WGPUCompareFunction_Always;
            depthstencil_desc.stencilFront.failOp      = WGPUStencilOperation_Keep;
            depthstencil_desc.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
            depthstencil_desc.stencilFront.passOp      = WGPUStencilOperation_Keep;
            depthstencil_desc.stencilBack.compare      = WGPUCompareFunction_Always;
            depthstencil_desc.stencilBack.failOp       = WGPUStencilOperation_Keep;
            depthstencil_desc.stencilBack.depthFailOp  = WGPUStencilOperation_Keep;
            depthstencil_desc.stencilBack.passOp       = WGPUStencilOperation_Keep;
        }
        depthstencil_desc.stencilWriteMask = context->m_CurrentPipelineState.m_StencilWriteMask;
        desc.depthStencil                  = &depthstencil_desc;
    }

    // fragment
    WGPUFragmentState fragment_desc = {};
    if (context->m_CurrentProgram->m_FragmentModule)
    {
        fragment_desc.entryPoint           = "main";
        fragment_desc.module               = context->m_CurrentProgram->m_FragmentModule->m_Module;
        WGPUColorWriteMaskFlags write_mask = {};
        if (context->m_CurrentPipelineState.m_WriteColorMask & DM_GRAPHICS_STATE_WRITE_R)
            write_mask |= WGPUColorWriteMask_Red;
        if (context->m_CurrentPipelineState.m_WriteColorMask & DM_GRAPHICS_STATE_WRITE_G)
            write_mask |= WGPUColorWriteMask_Green;
        if (context->m_CurrentPipelineState.m_WriteColorMask & DM_GRAPHICS_STATE_WRITE_B)
            write_mask |= WGPUColorWriteMask_Blue;
        if (context->m_CurrentPipelineState.m_WriteColorMask & DM_GRAPHICS_STATE_WRITE_A)
            write_mask |= WGPUColorWriteMask_Alpha;
        WGPUBlendState blend_state  = {};
        blend_state.color.operation = WGPUBlendOperation_Add;
        blend_state.color.srcFactor = g_webgpu_blend_factors[context->m_CurrentPipelineState.m_BlendSrcFactor];
        blend_state.color.dstFactor = g_webgpu_blend_factors[context->m_CurrentPipelineState.m_BlendDstFactor];
        blend_state.alpha.operation = WGPUBlendOperation_Add;
        blend_state.alpha.srcFactor = g_webgpu_blend_factors[context->m_CurrentPipelineState.m_BlendSrcFactor];
        blend_state.alpha.dstFactor = g_webgpu_blend_factors[context->m_CurrentPipelineState.m_BlendDstFactor];
        WGPUColorTargetState targets_desc[MAX_BUFFER_COLOR_ATTACHMENTS];
        for (int a = 0; a < context->m_CurrentRenderPass.m_Target->m_ColorBufferCount; ++a)
        {
            targets_desc[a]        = {};
            WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, context->m_CurrentRenderPass.m_Target->m_TextureColor[a]);
            targets_desc[a].format = texture->m_Format;
            if (context->m_CurrentPipelineState.m_BlendEnabled)
                targets_desc[a].blend = &blend_state;
            targets_desc[a].writeMask = write_mask;
        }
        fragment_desc.targetCount = context->m_CurrentRenderPass.m_Target->m_ColorBufferCount;
        fragment_desc.targets     = targets_desc;
        desc.fragment             = &fragment_desc;
    }
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(context->m_Device, &desc);
    if (context->m_RenderPipelineCache.Full())
        context->m_RenderPipelineCache.SetCapacity(32, context->m_RenderPipelineCache.Capacity() + 4);
    context->m_RenderPipelineCache.Put(pipeline_hash, pipeline);
    return pipeline;
}

static void WebGPUCreateSwapchain(WebGPUContext* context, uint32_t width, uint32_t height)
{
    // swapchain
    {
        if (context->m_SwapChain)
            wgpuSwapChainRelease(context->m_SwapChain);
        WGPUSwapChainDescriptor swap_chain_desc = {};
        swap_chain_desc.usage                   = WGPUTextureUsage_RenderAttachment;
        swap_chain_desc.format                  = context->m_Format;
        swap_chain_desc.width                   = width;
        swap_chain_desc.height                  = height;
        swap_chain_desc.presentMode             = WGPUPresentMode_Fifo;
        context->m_SwapChain                    = wgpuDeviceCreateSwapChain(context->m_Device, context->m_Surface, &swap_chain_desc);
    }

    // rendertarget
    if (!context->m_MainRenderTarget)
    {
        context->m_MainRenderTarget                = new WebGPURenderTarget();
        context->m_MainRenderTarget->m_Multisample = dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_SAMPLE_COUNT);
        if (!context->m_MainRenderTarget->m_Multisample)
            context->m_MainRenderTarget->m_Multisample = 1;
        else if (context->m_MainRenderTarget->m_Multisample != 1)
            context->m_MainRenderTarget->m_Multisample = 4; // only 1 and 4 are supported
        context->m_MainRenderTarget->m_ColorBufferCount       = 1;
        context->m_MainRenderTarget->m_ColorBufferStoreOps[0] = ATTACHMENT_OP_STORE;
        context->m_MainRenderTarget->m_ColorBufferLoadOps[0]  = ATTACHMENT_OP_LOAD;
    }
    context->m_MainRenderTarget->m_Width = context->m_Width = width;
    context->m_MainRenderTarget->m_Height = context->m_Height = height;
    // colorbuffer
    if (context->m_MainRenderTarget->m_Multisample == 1)
    {
        WebGPUTexture* textureColor = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureColor[0]);
        if (!textureColor)
        {
            textureColor                                   = new WebGPUTexture();
            context->m_MainRenderTarget->m_TextureColor[0] = StoreAssetInContainer(context->m_AssetHandleContainer, textureColor, ASSET_TYPE_TEXTURE);
        }
        textureColor->m_Width  = width;
        textureColor->m_Height = height;
        textureColor->m_Format = context->m_Format;
    }
    else
    {
        WebGPUTexture* textureColor = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureColor[0]);
        if (textureColor)
        {
            delete textureColor;
            context->m_AssetHandleContainer.Release(context->m_MainRenderTarget->m_TextureColor[0]);
        }
        {
            TextureCreationParams params;
            params.m_Width         = width;
            params.m_Height        = height;
            params.m_UsageHintBits = 0;
            textureColor           = WebGPUNewTextureInternal(params);
            WebGPURealizeTexture(textureColor, context->m_Format, 1, context->m_MainRenderTarget->m_Multisample, WGPUTextureUsage_RenderAttachment);
            context->m_MainRenderTarget->m_TextureColor[0] = StoreAssetInContainer(context->m_AssetHandleContainer, textureColor, ASSET_TYPE_TEXTURE);
        }

        WebGPUTexture* textureResolve = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureResolve[0]);
        if (!textureResolve)
        {
            textureResolve                                   = new WebGPUTexture();
            context->m_MainRenderTarget->m_TextureResolve[0] = StoreAssetInContainer(context->m_AssetHandleContainer, textureResolve, ASSET_TYPE_TEXTURE);
        }
        textureResolve->m_Width  = width;
        textureResolve->m_Height = height;
        textureResolve->m_Format = context->m_Format;
    }
    // depthstencil
    WebGPUTexture* textureDepthStencil = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureDepthStencil);
    if (textureDepthStencil)
    {
        delete textureDepthStencil;
        context->m_AssetHandleContainer.Release(context->m_MainRenderTarget->m_TextureDepthStencil);
    }
    {
        TextureCreationParams params;
        params.m_Width         = width;
        params.m_Height        = height;
        params.m_UsageHintBits = 0;
        textureDepthStencil    = WebGPUNewTextureInternal(params);
        WebGPURealizeTexture(textureDepthStencil, WGPUTextureFormat_Depth24PlusStencil8, 1, context->m_MainRenderTarget->m_Multisample, WGPUTextureUsage_RenderAttachment);
        context->m_MainRenderTarget->m_TextureDepthStencil = StoreAssetInContainer(context->m_AssetHandleContainer, textureDepthStencil, ASSET_TYPE_TEXTURE);
    }
}

static void requestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)userdata;
    if (device)
    {
        context->m_Device = device;
        wgpuDeviceGetLimits(context->m_Device, &context->m_DeviceLimits);
        context->m_Queue = wgpuDeviceGetQueue(context->m_Device);
        {
            WGPUSurfaceDescriptor surface_desc = {};
#if defined(__EMSCRIPTEN__)
            WGPUSurfaceDescriptorFromCanvasHTMLSelector selector = { { surface_desc.nextInChain, WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector }, "#canvas" };
            surface_desc.nextInChain                             = (WGPUChainedStruct*)&selector;
#endif
            context->m_Surface = wgpuInstanceCreateSurface(context->m_Instance, &surface_desc);
        }
        context->m_Format = wgpuSurfaceGetPreferredFormat(context->m_Surface, context->m_Adapter);
        WebGPUCreateSwapchain(context, context->m_OriginalWidth, context->m_OriginalHeight);
    }
    else
    {
        dmLogError("WebGPU: Unable to create device %s", message);
        context->m_InitComplete = true;
    }
}

static void instanceRequestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)userdata;
    if (adapter)
    {
        context->m_Adapter = adapter;
        wgpuAdapterGetLimits(context->m_Adapter, &context->m_AdapterLimits);

        WGPUDeviceDescriptor descriptor = {};
        WGPUFeatureName features[16];
        descriptor.requiredFeatures = features;
        if (wgpuAdapterHasFeature(context->m_Adapter, WGPUFeatureName_TextureCompressionBC))
            features[descriptor.requiredFeatureCount++] = WGPUFeatureName_TextureCompressionBC;
        if (wgpuAdapterHasFeature(context->m_Adapter, WGPUFeatureName_TextureCompressionASTC))
            features[descriptor.requiredFeatureCount++] = WGPUFeatureName_TextureCompressionASTC;
        wgpuAdapterRequestDevice(context->m_Adapter, &descriptor, requestDeviceCallback, userdata);
        context->m_InitComplete = true;
    }
    else
    {
        dmLogError("WebGPU: Unable to create adapter %s", message);
        context->m_InitComplete = true;
    }
}

static bool InitializeWebGPUContext(WebGPUContext* context, const ContextParams& params)
{
    TRACE_CALL;
    context->m_Window = params.m_Window;
    assert(dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED));
    context->m_OriginalWidth   = params.m_Width;
    context->m_OriginalHeight  = params.m_Height;
    context->m_PrintDeviceInfo = params.m_PrintDeviceInfo;
    context->m_CurrentUniforms.m_Allocs.SetCapacity(32);

    context->m_CurrentPipelineState = GetDefaultPipelineState();

    context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_MULTI_TARGET_RENDERING;
    context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_TEXTURE_ARRAY;
    context->m_ContextFeatures |= 1 << CONTEXT_FEATURE_COMPUTE_SHADER;

    if (context->m_PrintDeviceInfo)
    {
        dmLogInfo("Device: webgpu");
    }

    context->m_Instance = wgpuCreateInstance(nullptr);
    wgpuInstanceRequestAdapter(context->m_Instance, NULL, instanceRequestAdapterCallback, context);
#if defined(__EMSCRIPTEN__)
    while (!context->m_InitComplete)
        emscripten_sleep(100);
#endif
    context->m_SamplerCache.SetCapacity(32, 64);
    context->m_BindGroupCache.SetCapacity(32, 64);
    context->m_RenderPipelineCache.SetCapacity(32, 64);
    context->m_ComputePipelineCache.SetCapacity(32, 64);
    SetSwapInterval(context, params.m_SwapInterval);

    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGB; // Transcoded
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA16F;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA32F;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_R16F;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RG16F;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_R32F;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RG32F;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA32UI;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_R32UI;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA_16BPP;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_LUMINANCE;
    context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_LUMINANCE_ALPHA;
    if (wgpuAdapterHasFeature(context->m_Adapter, WGPUFeatureName_TextureCompressionASTC))
    {
        context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA_ASTC_4x4;
    }
    if (wgpuAdapterHasFeature(context->m_Adapter, WGPUFeatureName_TextureCompressionBC))
    {
        context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGB_BC1;
        context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA_BC3;
        context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RGBA_BC7;
        context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_R_BC4;
        context->m_TextureFormatSupport |= 1ULL << TEXTURE_FORMAT_RG_BC5;
    }

    context->m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
    if (context->m_DefaultTextureMinFilter == TEXTURE_FILTER_DEFAULT)
        context->m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR;
    context->m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
    if (context->m_DefaultTextureMagFilter == TEXTURE_FILTER_DEFAULT)
        context->m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;

    {
        // Create default texture sampler
        WGPUSampler sampler = WebGPUGetOrCreateSampler(context, TEXTURE_FILTER_LINEAR, TEXTURE_FILTER_LINEAR, TEXTURE_WRAP_REPEAT, TEXTURE_WRAP_REPEAT, 1.0f);

        // Create default dummy texture(s)
        TextureCreationParams default_texture_creation_params;
        default_texture_creation_params.m_Width          = 1;
        default_texture_creation_params.m_Height         = 1;
        default_texture_creation_params.m_Depth          = 1;
        default_texture_creation_params.m_UsageHintBits  = TEXTURE_USAGE_FLAG_SAMPLE;
        default_texture_creation_params.m_OriginalWidth  = default_texture_creation_params.m_Width;
        default_texture_creation_params.m_OriginalHeight = default_texture_creation_params.m_Height;

        const uint8_t default_texture_data[4 * 6] = {}; // RGBA * 6 (for cubemap)

        TextureParams default_texture_params;
        default_texture_params.m_Width  = 1;
        default_texture_params.m_Height = 1;
        default_texture_params.m_Depth  = 1;
        default_texture_params.m_Data   = default_texture_data;
        default_texture_params.m_Format = TEXTURE_FORMAT_RGBA;

        context->m_DefaultTexture2D = WebGPUNewTextureInternal(default_texture_creation_params);
        WebGPUSetTextureInternal(context->m_DefaultTexture2D, default_texture_params);

        default_texture_params.m_Format            = TEXTURE_FORMAT_RGBA32UI;
        context->m_DefaultTexture2D32UI            = WebGPUNewTextureInternal(default_texture_creation_params);
        context->m_DefaultTexture2D32UI->m_Sampler = sampler;
        WebGPUSetTextureInternal(context->m_DefaultTexture2D32UI, default_texture_params);

        default_texture_creation_params.m_Type      = TEXTURE_TYPE_2D_ARRAY;
        context->m_DefaultTexture2DArray            = WebGPUNewTextureInternal(default_texture_creation_params);
        context->m_DefaultTexture2DArray->m_Sampler = sampler;

        default_texture_params.m_Format                 = TEXTURE_FORMAT_RGBA;
        default_texture_creation_params.m_Type          = TEXTURE_TYPE_IMAGE_2D;
        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_STORAGE | TEXTURE_USAGE_FLAG_SAMPLE;
        context->m_DefaultStorageImage2D                = WebGPUNewTextureInternal(default_texture_creation_params);
        context->m_DefaultStorageImage2D->m_Sampler     = sampler;
        WebGPUSetTextureInternal(context->m_DefaultStorageImage2D, default_texture_params);

        default_texture_creation_params.m_Type          = TEXTURE_TYPE_CUBE_MAP;
        default_texture_creation_params.m_Depth         = 6;
        default_texture_creation_params.m_UsageHintBits = TEXTURE_USAGE_FLAG_SAMPLE;
        context->m_DefaultTextureCubeMap                = WebGPUNewTextureInternal(default_texture_creation_params);
        WebGPUSetTextureInternal(context->m_DefaultTextureCubeMap, default_texture_params);
        context->m_DefaultTextureCubeMap->m_Sampler = sampler;
    }

    return true;
}

static void DestroyWebGPUContext(WebGPUContext* context)
{
    if (context->m_Surface)
        wgpuSurfaceRelease(context->m_Surface);
    if (context->m_Adapter)
        wgpuAdapterRelease(context->m_Adapter);
    if (context->m_Queue)
        wgpuQueueRelease(context->m_Queue);
    if (context->m_Device)
        wgpuDeviceRelease(context->m_Device);
}

static HContext WebGPUNewContext(const ContextParams& params)
{
    TRACE_CALL;
    if (!g_WebGPUContext)
    {
        g_WebGPUContext = (WebGPUContext*)malloc(sizeof(WebGPUContext));
        memset(g_WebGPUContext, 0, sizeof(*g_WebGPUContext));
        if (InitializeWebGPUContext(g_WebGPUContext, params))
            return g_WebGPUContext;
        DeleteContext(g_WebGPUContext);
    }
    return NULL;
}

static bool WebGPUIsSupported()
{
    TRACE_CALL;
    return true;
}

static HContext WebGPUGetContext()
{
    TRACE_CALL;
    return g_WebGPUContext;
}

static void WebGPUFinalize()
{
}

static void WebGPUDeleteContext(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    assert(g_WebGPUContext == context);
    if (context)
    {
        DestroyWebGPUContext(context);
        free(context);
        g_WebGPUContext = NULL;
    }
}

static void WebGPURunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
{
    TRACE_CALL;
#ifdef __EMSCRIPTEN__
    while (0 != is_running(user_data))
    {
        // N.B. Beyond the first test, the above statement is essentially formal since set_main_loop will throw an exception.
        emscripten_set_main_loop_arg(step_method, user_data, 0, 1);
    }
#else
    while (0 != is_running(user_data))
    {
        step_method(user_data);
    }
#endif
}

static dmPlatform::HWindow WebGPUGetWindow(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    return context->m_Window;
}

static uint32_t WebGPUGetDisplayDpi(HContext context)
{
    TRACE_CALL;
    assert(context);
    return 0;
}

static uint32_t WebGPUGetWidth(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    return context->m_OriginalWidth;
}

static uint32_t WebGPUGetHeight(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    return context->m_OriginalHeight;
}

static void WebGPUSetWindowSize(HContext _context, uint32_t width, uint32_t height)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context = (WebGPUContext*)_context;
    if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
    {
        context->m_Width  = width;
        context->m_Height = height;
        dmPlatform::SetWindowSize(context->m_Window, width, height);
    }
}

static void WebGPUResizeWindow(HContext _context, uint32_t width, uint32_t height)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context = (WebGPUContext*)_context;
    if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
    {
        dmPlatform::SetWindowSize(context->m_Window, width, height);
        WebGPUCreateSwapchain(context, width, height);
    }
}

static void WebGPUGetDefaultTextureFilters(HContext _context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    out_min_filter         = context->m_DefaultTextureMinFilter;
    out_mag_filter         = context->m_DefaultTextureMagFilter;
}

static void WebGPUCreateCommandEncoder(WebGPUContext* context)
{
    if(!context->m_CommandEncoder)
    {
        context->m_CommandEncoder = wgpuDeviceCreateCommandEncoder(context->m_Device, NULL);
    }
}

static void WebGPUEndRenderPass(WebGPUContext* context);

static void WebGPUEndComputePass(WebGPUContext* context)
{
    TRACE_CALL;
    if (context->m_CurrentComputePass.m_Encoder)
    {
        wgpuComputePassEncoderEnd(context->m_CurrentComputePass.m_Encoder);
        wgpuComputePassEncoderRelease(context->m_CurrentComputePass.m_Encoder);
        memset(&context->m_CurrentComputePass, 0, sizeof(context->m_CurrentComputePass));
    }
}

static void WebGPUSubmitCommandEncoder(WebGPUContext* context)
{
    TRACE_CALL;
    if (context->m_CommandEncoder)
    {
        WebGPUEndComputePass(context);
        WebGPUEndRenderPass(context);

        const WGPUCommandBuffer buffer = wgpuCommandEncoderFinish(context->m_CommandEncoder, NULL);
        wgpuQueueSubmit(context->m_Queue, 1, &buffer);
        wgpuCommandBufferRelease(buffer);
        wgpuCommandEncoderRelease(context->m_CommandEncoder);
        context->m_CommandEncoder = NULL;
        context->m_LastSubmittedRenderPass = context->m_RenderPasses;
    }
}

static void WebGPUBeginComputePass(WebGPUContext* context)
{
    TRACE_CALL;
    if (!context->m_CurrentComputePass.m_Encoder)
    {
        WebGPUEndRenderPass(context);
        WebGPUCreateCommandEncoder(context);
        WGPUComputePassDescriptor desc          = {};
        context->m_CurrentComputePass.m_Encoder = wgpuCommandEncoderBeginComputePass(context->m_CommandEncoder, &desc);
    }
}

static void WebGPUEndRenderPass(WebGPUContext* context)
{
    TRACE_CALL;
    if (context->m_CurrentRenderPass.m_Encoder)
    {
        assert(context->m_CurrentRenderPass.m_Target);
        wgpuRenderPassEncoderEnd(context->m_CurrentRenderPass.m_Encoder);
        wgpuRenderPassEncoderRelease(context->m_CurrentRenderPass.m_Encoder);
        memset(&context->m_CurrentRenderPass, 0, sizeof(context->m_CurrentRenderPass));
    }
}

static void WebGPUBeginRenderPass(WebGPUContext* context, const float* clearColor = NULL, float clearDepth = 1.0f, uint32_t clearStencil = 0)
{
    TRACE_CALL;
    WebGPUEndComputePass(context);
    if (context->m_CurrentRenderPass.m_Target != context->m_CurrentRenderTarget)
    {
        WebGPUEndRenderPass(context);
        WebGPUCreateCommandEncoder(context);
        ++context->m_RenderPasses;
        context->m_CurrentRenderPass.m_Target = context->m_CurrentRenderTarget;
        {
            WGPURenderPassDescriptor desc = {};

            // color
            WGPURenderPassColorAttachment colorAttachments[MAX_BUFFER_COLOR_ATTACHMENTS];
            for (int i = 0; i < context->m_CurrentRenderPass.m_Target->m_ColorBufferCount; ++i)
            {
                colorAttachments[i]            = {};
                colorAttachments[i].depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
                {
                    WebGPUTexture* texture   = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, context->m_CurrentRenderPass.m_Target->m_TextureColor[i]);
                    colorAttachments[i].view = texture->m_TextureView;
                }
                if (context->m_CurrentRenderPass.m_Target->m_TextureResolve[i])
                {
                    WebGPUTexture* texture            = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, context->m_CurrentRenderPass.m_Target->m_TextureResolve[i]);
                    colorAttachments[i].resolveTarget = texture->m_TextureView;
                }
                switch (context->m_CurrentRenderPass.m_Target->m_ColorBufferStoreOps[i])
                {
                    case ATTACHMENT_OP_STORE:
                        colorAttachments[i].storeOp = WGPUStoreOp_Store;
                        break;
                    default:
                        colorAttachments[i].storeOp = WGPUStoreOp_Undefined;
                        break;
                }
                if (clearColor)
                {
                    colorAttachments[i].loadOp       = WGPULoadOp_Clear;
                    colorAttachments[i].clearValue.r = clearColor[0];
                    colorAttachments[i].clearValue.g = clearColor[1];
                    colorAttachments[i].clearValue.b = clearColor[2];
                    colorAttachments[i].clearValue.a = clearColor[3];
                }
                else
                {
                    switch (context->m_CurrentRenderPass.m_Target->m_ColorBufferLoadOps[i])
                    {
                        case ATTACHMENT_OP_DONT_CARE:
                        case ATTACHMENT_OP_LOAD:
                            colorAttachments[i].loadOp = WGPULoadOp_Load;
                            break;
                        case ATTACHMENT_OP_CLEAR:
                            colorAttachments[i].loadOp       = WGPULoadOp_Clear;
                            colorAttachments[i].clearValue.r = context->m_CurrentRenderPass.m_Target->m_ColorBufferClearValue[i][0];
                            colorAttachments[i].clearValue.g = context->m_CurrentRenderPass.m_Target->m_ColorBufferClearValue[i][1];
                            colorAttachments[i].clearValue.b = context->m_CurrentRenderPass.m_Target->m_ColorBufferClearValue[i][2];
                            colorAttachments[i].clearValue.a = context->m_CurrentRenderPass.m_Target->m_ColorBufferClearValue[i][3];
                            break;
                        default:
                            colorAttachments[i].loadOp = WGPULoadOp_Undefined;
                            break;
                    }
                }
            }
            desc.colorAttachments     = colorAttachments;
            desc.colorAttachmentCount = context->m_CurrentRenderPass.m_Target->m_ColorBufferCount;

            // depth/stencil
            WGPURenderPassDepthStencilAttachment dsAttachment = {};
            if (context->m_CurrentRenderPass.m_Target->m_TextureDepthStencil)
            {
                WebGPUTexture* texture         = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, context->m_CurrentRenderPass.m_Target->m_TextureDepthStencil);
                dsAttachment.view              = texture->m_TextureView;
                dsAttachment.depthLoadOp       = WGPULoadOp_Clear;
                dsAttachment.depthStoreOp      = WGPUStoreOp_Store;
                dsAttachment.depthClearValue   = clearDepth;
                dsAttachment.stencilLoadOp     = WGPULoadOp_Clear;
                dsAttachment.stencilStoreOp    = WGPUStoreOp_Store;
                dsAttachment.stencilClearValue = clearStencil;
                desc.depthStencilAttachment    = &dsAttachment;
            }

            context->m_CurrentRenderPass.m_Encoder = wgpuCommandEncoderBeginRenderPass(context->m_CommandEncoder, &desc);
        }
        context->m_CurrentRenderPass.m_Target->m_Scissor[0] = 0;
        context->m_CurrentRenderPass.m_Target->m_Scissor[1] = 0;
        context->m_CurrentRenderPass.m_Target->m_Scissor[2] = context->m_CurrentRenderPass.m_Target->m_Width;
        context->m_CurrentRenderPass.m_Target->m_Scissor[3] = context->m_CurrentRenderPass.m_Target->m_Height;
    }

    if (context->m_ViewportChanged)
    {
        const int32_t vx1 = dmMath::Clamp(context->m_ViewportRect[0], 0, int32_t(context->m_CurrentRenderPass.m_Target->m_Width));
        const int32_t vy1 = dmMath::Clamp(context->m_ViewportRect[1], 0, int32_t(context->m_CurrentRenderPass.m_Target->m_Height));
        const int32_t vx2 = dmMath::Clamp(context->m_ViewportRect[2], 0, int32_t(context->m_CurrentRenderPass.m_Target->m_Width));
        const int32_t vy2 = dmMath::Clamp(context->m_ViewportRect[3], 0, int32_t(context->m_CurrentRenderPass.m_Target->m_Height));
        wgpuRenderPassEncoderSetViewport(context->m_CurrentRenderPass.m_Encoder, vx1, vy1, vx2 - vx1, vy2 - vy1, 0.0f, 1.0f);
        wgpuRenderPassEncoderSetScissorRect(context->m_CurrentRenderPass.m_Encoder, context->m_CurrentRenderPass.m_Target->m_Scissor[0], context->m_CurrentRenderPass.m_Target->m_Scissor[1], context->m_CurrentRenderPass.m_Target->m_Scissor[2], context->m_CurrentRenderPass.m_Target->m_Scissor[3]);
        context->m_ViewportChanged = 0;
    }
    if (context->m_CurrentPipelineState.m_StencilReference)
        wgpuRenderPassEncoderSetStencilReference(context->m_CurrentRenderPass.m_Encoder, context->m_CurrentPipelineState.m_StencilReference);
}

static void WebGPUClear(HContext _context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUEndRenderPass(context);
    const float clearColor[] = { red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f };
    WebGPUBeginRenderPass(context, clearColor, depth, stencil);
}

static void WebGPUBeginFrame(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    {
        const uint32_t windowWidth = GetWindowWidth(context->m_Window), windowHeight = GetWindowHeight(context->m_Window);
        if (!context->m_MainRenderTarget || windowWidth != context->m_Width || windowHeight != context->m_Height) // (re)create
            WebGPUCreateSwapchain(context, windowWidth, windowHeight);

        WebGPUTexture* texture;
        if (context->m_MainRenderTarget->m_Multisample == 1)
            texture = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureColor[0]);
        else
            texture = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureResolve[0]);
        texture->m_Texture     = wgpuSwapChainGetCurrentTexture(context->m_SwapChain);
        texture->m_TextureView = wgpuSwapChainGetCurrentTextureView(context->m_SwapChain);
    }
    context->m_CurrentRenderTarget = context->m_MainRenderTarget;
}

static void WebGPUFlip(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUSubmitCommandEncoder(context);
    context->m_CurrentRenderTarget = NULL;
    {
#if !defined(__EMSCRIPTEN__)
        wgpuSwapChainPresent(context->m_SwapChain);
#endif
        {
            WebGPUTexture* texture;
            if (context->m_MainRenderTarget->m_Multisample == 1)
                texture = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureColor[0]);
            else
                texture = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, context->m_MainRenderTarget->m_TextureResolve[0]);
            if (texture->m_Texture)
            {
                wgpuTextureRelease(texture->m_Texture);
                texture->m_Texture = NULL;
            }
            if (texture->m_TextureView)
            {
                wgpuTextureViewRelease(texture->m_TextureView);
                texture->m_TextureView = NULL;
            }
        }
        if (context->m_CurrentUniforms.m_Allocs.Size() > 0)
        {
            for (size_t a = 0; a <= context->m_CurrentUniforms.m_Alloc; ++a)
            {
                context->m_CurrentUniforms.m_Allocs[a]->m_Used = 0;
            }
            context->m_CurrentUniforms.m_Alloc = 0;
        }
    }
    dmPlatform::SwapBuffers(context->m_Window);
}

static void WebGPUWriteBuffer(WebGPUContext* context, WebGPUBuffer* buffer, size_t offset, void const* data, size_t size)
{
    TRACE_CALL;
    assert(size);
    if (!buffer->m_Buffer) // create it
    {
        WGPUBufferDescriptor desc = {};
        desc.usage                = buffer->m_Usage;
        desc.size                 = size;
        buffer->m_Buffer          = wgpuDeviceCreateBuffer(context->m_Device, &desc);
        buffer->m_Used = buffer->m_Size = desc.size;
    }
    else if (buffer->m_LastRenderPass && buffer->m_LastRenderPass > context->m_LastSubmittedRenderPass) // flush pipeline
    {
        //dmLogWarning("Deoptimization: Forcing pipeline flush due to buffer write");
        WebGPUSubmitCommandEncoder(context);
    }
    wgpuQueueWriteBuffer(context->m_Queue, buffer->m_Buffer, offset, data, size);
}

static HVertexBuffer WebGPUNewVertexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
{
    TRACE_CALL;
    WebGPUContext* context     = (WebGPUContext*)_context;
    WGPUBufferUsageFlags usage = WGPUBufferUsage(WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst);
    if (buffer_usage & BUFFER_USAGE_TRANSFER)
        usage |= WGPUBufferUsage_CopySrc;
    WebGPUBuffer* buffer = new WebGPUBuffer(usage);
    if (size > 0)
    {
        WebGPUWriteBuffer(context, buffer, 0, data, size);
    }
    return (HVertexBuffer)buffer;
}

static void WebGPUDeleteVertexBuffer(HVertexBuffer buffer)
{
    TRACE_CALL;
    WebGPUBuffer* gpu_buffer = (WebGPUBuffer*)buffer;
    if (gpu_buffer->m_Buffer)
    {
        wgpuBufferRelease(gpu_buffer->m_Buffer);
    }
    delete gpu_buffer;
}

static void WebGPUSetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
{
    if (size == 0)
    {
        return;
    }
    TRACE_CALL;
    WebGPUBuffer* gpu_buffer = (WebGPUBuffer*)buffer;
    if (gpu_buffer->m_Buffer && gpu_buffer->m_Size < size)
    {
        wgpuBufferRelease(gpu_buffer->m_Buffer);
        gpu_buffer->m_Buffer = NULL;
        gpu_buffer->m_Used = gpu_buffer->m_Size = 0;
        gpu_buffer->m_LastRenderPass = 0;
    }
    else
    {
        gpu_buffer->m_Used = size;
    }
    WebGPUWriteBuffer(g_WebGPUContext, gpu_buffer, 0, data, size);
}

static void WebGPUSetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
{
    TRACE_CALL;
    assert(size > 0);
    WebGPUBuffer* gpu_buffer = (WebGPUBuffer*)buffer;
    assert(gpu_buffer->m_Used >= offset + size);
    WebGPUWriteBuffer(g_WebGPUContext, gpu_buffer, offset, data, size);
}

static uint32_t WebGPUGetVertexBufferSize(HVertexBuffer buffer)
{
    if (!buffer)
    {
        return 0;
    }
    WebGPUBuffer* buffer_ptr = (WebGPUBuffer*) buffer;
    return buffer_ptr->m_Size;
}

static uint32_t WebGPUGetMaxElementsVertices(HContext context)
{
    TRACE_CALL;
    return 65536;
}

static HIndexBuffer WebGPUNewIndexBuffer(HContext _context, uint32_t size, const void* data, BufferUsage buffer_usage)
{
    TRACE_CALL;
    WebGPUContext* context     = (WebGPUContext*)_context;
    WGPUBufferUsageFlags usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    if (buffer_usage & BUFFER_USAGE_TRANSFER)
        usage |= WGPUBufferUsage_CopySrc;
    WebGPUBuffer* buffer = new WebGPUBuffer(usage);
    if (size)
        WebGPUWriteBuffer(context, buffer, 0, data, size);
    return (HIndexBuffer)buffer;
}

static void WebGPUDeleteIndexBuffer(HIndexBuffer _buffer)
{
    TRACE_CALL;
    WebGPUBuffer* buffer = (WebGPUBuffer*)_buffer;
    if (buffer->m_Buffer)
        wgpuBufferRelease(buffer->m_Buffer);
    delete buffer;
}

static void WebGPUSetIndexBufferData(HIndexBuffer _buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
{
    if (!size)
        return;

    TRACE_CALL;
    WebGPUBuffer* buffer = (WebGPUBuffer*)_buffer;
    if (buffer->m_Buffer && buffer->m_Size < size)
    {
        wgpuBufferRelease(buffer->m_Buffer);
        buffer->m_Buffer = NULL;
        buffer->m_Used = buffer->m_Size = 0;
        buffer->m_LastRenderPass = 0;
    }
    else
    {
        buffer->m_Used = size;
    }
    WebGPUWriteBuffer(g_WebGPUContext, buffer, 0, data, size);
}

static void WebGPUSetIndexBufferSubData(HIndexBuffer _buffer, uint32_t offset, uint32_t size, const void* data)
{
    TRACE_CALL;
    assert(size > 0);
    WebGPUBuffer* buffer = (WebGPUBuffer*)_buffer;
    assert(buffer->m_Used >= offset + size);
    WebGPUWriteBuffer(g_WebGPUContext, buffer, offset, data, size);
}

static uint32_t WebGPUGetIndexBufferSize(HIndexBuffer buffer)
{
    if (!buffer)
    {
        return 0;
    }
    WebGPUBuffer* buffer_ptr = (WebGPUBuffer*) buffer;
    return buffer_ptr->m_Size;
}

static bool WebGPUIsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
{
    TRACE_CALL;
    return true;
}

static uint32_t WebGPUGetMaxElementsIndices(HContext context)
{
    TRACE_CALL;
    assert(false);
    return -1;
}

static VertexDeclaration* CreateAndFillVertexDeclaration(HashState64* hash, HVertexStreamDeclaration stream_declaration)
{
    VertexDeclaration* vd = new VertexDeclaration();
    memset(vd, 0, sizeof(VertexDeclaration));
    vd->m_StreamCount = stream_declaration->m_StreamCount;
    for (uint32_t i = 0; i < stream_declaration->m_StreamCount; ++i)
    {
        VertexStream& stream = stream_declaration->m_Streams[i];
        if ((stream.m_Type == TYPE_BYTE || stream.m_Type == TYPE_UNSIGNED_BYTE || stream.m_Type == TYPE_SHORT || stream.m_Type == TYPE_UNSIGNED_SHORT) && !stream.m_Normalize) // stolen from vulkan
        {
            dmLogWarning("Using the type '%s' for stream '%s' with normalize: false is not supported for vertex declarations. Defaulting to normalize:true.", GetGraphicsTypeLiteral(stream.m_Type), dmHashReverseSafe64(stream.m_NameHash));
            stream.m_Normalize = 1;
        }

        vd->m_Streams[i].m_NameHash  = stream.m_NameHash;
        vd->m_Streams[i].m_Type      = stream.m_Type;
        vd->m_Streams[i].m_Size      = stream.m_Size;
        vd->m_Streams[i].m_Normalize = stream.m_Normalize;
        vd->m_Streams[i].m_Offset    = vd->m_Stride;
        vd->m_Streams[i].m_Location  = -1;
        vd->m_Stride += DM_ALIGN(stream.m_Size * GetTypeSize(stream.m_Type), 4);

        dmHashUpdateBuffer64(hash, &stream.m_Size, sizeof(stream.m_Size));
        dmHashUpdateBuffer64(hash, &stream.m_Type, sizeof(stream.m_Type));
        dmHashUpdateBuffer64(hash, &vd->m_Streams[i].m_Type, sizeof(vd->m_Streams[i].m_Type));
    }
    return vd;
}

static HVertexDeclaration WebGPUNewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
{
    TRACE_CALL;
    HashState64 decl_hash_state;
    dmHashInit64(&decl_hash_state, false);
    VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
    dmHashUpdateBuffer64(&decl_hash_state, &vd->m_Stride, sizeof(vd->m_Stride));
    vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
    vd->m_StepFunction = VERTEX_STEP_FUNCTION_VERTEX;
    return vd;
}

static HVertexDeclaration WebGPUNewVertexDeclarationStride(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
{
    TRACE_CALL;
    HashState64 decl_hash_state;
    dmHashInit64(&decl_hash_state, false);
    VertexDeclaration* vd = CreateAndFillVertexDeclaration(&decl_hash_state, stream_declaration);
    dmHashUpdateBuffer64(&decl_hash_state, &stride, sizeof(stride));
    vd->m_Stride       = stride;
    vd->m_PipelineHash = dmHashFinal64(&decl_hash_state);
    vd->m_StepFunction = VERTEX_STEP_FUNCTION_VERTEX;
    return vd;
}

static void WebGPUEnableVertexBuffer(HContext _context, HVertexBuffer _buffer, uint32_t index)
{
    TRACE_CALL;
    WebGPUBuffer* buffer                   = (WebGPUBuffer*)_buffer;
    WebGPUContext* context                 = (WebGPUContext*)_context;
    context->m_CurrentVertexBuffers[index] = buffer;
}

static void WebGPUDisableVertexBuffer(HContext _context, HVertexBuffer _buffer)
{
    TRACE_CALL;
    WebGPUBuffer* buffer   = (WebGPUBuffer*)_buffer;
    WebGPUContext* context = (WebGPUContext*)_context;
    for (size_t i = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        if (context->m_CurrentVertexBuffers[i] == buffer)
            context->m_CurrentVertexBuffers[i] = NULL;
    }
}

static void WebGPUEnableVertexDeclaration(HContext _context, HVertexDeclaration _declaration, uint32_t binding_index, uint32_t base_offset, HProgram _program)
{
    TRACE_CALL;
    WebGPUContext* context         = (WebGPUContext*)_context;
    WebGPUProgram* program         = (WebGPUProgram*)_program;
    VertexDeclaration* declaration = (VertexDeclaration*)_declaration;

    // TODO: Instancing!

    context->m_VertexDeclaration[binding_index]                = {};
    context->m_VertexDeclaration[binding_index].m_Stride       = declaration->m_Stride;
    context->m_VertexDeclaration[binding_index].m_StepFunction = declaration->m_StepFunction;
    context->m_VertexDeclaration[binding_index].m_PipelineHash = declaration->m_PipelineHash;

    context->m_CurrentVertexDeclaration[binding_index] = &context->m_VertexDeclaration[binding_index];

    uint32_t stream_ix  = 0;
    uint32_t num_inputs = program->m_VertexModule->m_ShaderMeta.m_Inputs.Size();

    for (int i = 0; i < declaration->m_StreamCount; ++i)
    {
        for (int j = 0; j < num_inputs; ++j)
        {
            ShaderResourceBinding& input = program->m_VertexModule->m_ShaderMeta.m_Inputs[j];

            if (input.m_NameHash == declaration->m_Streams[i].m_NameHash)
            {
                VertexDeclaration::Stream& stream = context->m_VertexDeclaration[binding_index].m_Streams[stream_ix];
                stream.m_NameHash                 = input.m_NameHash;
                stream.m_Location                 = input.m_Binding;
                stream.m_Type                     = declaration->m_Streams[i].m_Type;
                stream.m_Offset                   = declaration->m_Streams[i].m_Offset;
                stream.m_Size                     = declaration->m_Streams[i].m_Size;
                stream.m_Normalize                = declaration->m_Streams[i].m_Normalize;
                stream_ix++;

                context->m_VertexDeclaration[binding_index].m_StreamCount++;
                break;
            }
        }
    }
}

static void WebGPUDisableVertexDeclaration(HContext _context, HVertexDeclaration declaration)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context = (WebGPUContext*)_context;
    for (int i = 0; i < MAX_VERTEX_BUFFERS; ++i)
    {
        if (context->m_CurrentVertexDeclaration[i] == ((VertexDeclaration*)declaration))
            context->m_CurrentVertexDeclaration[i] = 0;
    }
}

static void WebGPUUpdateBindGroups(WebGPUContext* context)
{
    for (int set = 0; set < context->m_CurrentProgram->m_MaxSet; ++set)
    {
        if (!context->m_CurrentProgram->m_BindGroupLayouts[set] || context->m_CurrentProgram->m_BindGroups[set])
            continue;

        HashState64 bindgroup_hash_state;
        dmHashInit64(&bindgroup_hash_state, false);
        dmHashUpdateBuffer64(&bindgroup_hash_state, &set, sizeof(set));
        dmHashUpdateBuffer64(&bindgroup_hash_state, &context->m_CurrentProgram->m_Hash, sizeof(context->m_CurrentProgram->m_Hash));

        WGPUBindGroupDescriptor desc = {};
        WGPUBindGroupEntry entries[MAX_BINDINGS_PER_SET_COUNT];
        for (int binding = 0; binding < context->m_CurrentProgram->m_MaxBinding; ++binding)
        {
            ProgramResourceBinding& pgm_res = context->m_CurrentProgram->m_ResourceBindings[set][binding];
            if (pgm_res.m_Res == NULL)
                continue;
            entries[desc.entryCount]         = {};
            entries[desc.entryCount].binding = pgm_res.m_Res->m_Binding;
            switch (pgm_res.m_Res->m_BindingFamily)
            {
                case ShaderResourceBinding::BINDING_FAMILY_TEXTURE: {
                    WebGPUTexture* texture = context->m_CurrentTextureUnits[pgm_res.m_TextureUnit];
                    if (!texture)
                    {
                        switch (pgm_res.m_Res->m_Type.m_ShaderType)
                        {
                            case ShaderDesc::SHADER_TYPE_RENDER_PASS_INPUT:
                            case ShaderDesc::SHADER_TYPE_TEXTURE2D:
                            case ShaderDesc::SHADER_TYPE_SAMPLER:
                            case ShaderDesc::SHADER_TYPE_SAMPLER2D:
                                texture = context->m_DefaultTexture2D;
                                break;
                            case ShaderDesc::SHADER_TYPE_TEXTURE2D_ARRAY:
                            case ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY:
                                texture = context->m_DefaultTexture2DArray;
                                break;
                            case ShaderDesc::SHADER_TYPE_TEXTURE_CUBE:
                            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE:
                                texture = context->m_DefaultTextureCubeMap;
                                break;
                            case ShaderDesc::SHADER_TYPE_UTEXTURE2D:
                                texture = context->m_DefaultTexture2D32UI;
                                break;
                            case ShaderDesc::SHADER_TYPE_IMAGE2D:
                            case ShaderDesc::SHADER_TYPE_UIMAGE2D:
                                texture = context->m_DefaultStorageImage2D;
                                break;
                            default:
                                assert(false);
                        }
                    }
                    if (pgm_res.m_Res->m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
                    {
                        assert(texture->m_Sampler);
                        entries[desc.entryCount].sampler = texture->m_Sampler;
                    }
                    else
                    {
                        assert(texture->m_TextureView);
                        entries[desc.entryCount].textureView = texture->m_TextureView;
                    }
                    break;
                }
                case ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER: {
                    // const uint32_t ssbo_alignment = context->m_DeviceLimits.limits.minStorageBufferOffsetAlignment;
                    assert(false);
                    break;
                }
                case ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER: {
                    const uint32_t ubo_alignment = context->m_DeviceLimits.limits.minUniformBufferOffsetAlignment;
                    if (context->m_CurrentUniforms.m_Allocs.Size() == 0 ||
                        context->m_CurrentUniforms.m_Allocs[context->m_CurrentUniforms.m_Alloc]->m_Size <
                        context->m_CurrentUniforms.m_Allocs[context->m_CurrentUniforms.m_Alloc]->m_Used + pgm_res.m_Res->m_BindingInfo.m_BlockSize)
                    {
                        if (context->m_CurrentUniforms.m_Allocs.Size() > context->m_CurrentUniforms.m_Alloc + 1)
                        {
                            ++context->m_CurrentUniforms.m_Alloc;
                            assert(context->m_CurrentUniforms.m_Allocs[context->m_CurrentUniforms.m_Alloc]->m_Size >= pgm_res.m_Res->m_BindingInfo.m_BlockSize);
                        }
                        else
                        {
                            WebGPUUniformBuffer::Alloc* alloc = new WebGPUUniformBuffer::Alloc();
                            {
                                WGPUBufferDescriptor desc = {};
                                desc.usage                = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
                                desc.size                 = std::max(uint16_t(16 * 1024), pgm_res.m_Res->m_BindingInfo.m_BlockSize);
                                alloc->m_Buffer           = wgpuDeviceCreateBuffer(context->m_Device, &desc);
                                alloc->m_Size             = desc.size;
                            }
                            if (context->m_CurrentUniforms.m_Allocs.Full())
                            {
                                context->m_CurrentUniforms.m_Allocs.OffsetCapacity(4);
                            }
                            context->m_CurrentUniforms.m_Alloc = context->m_CurrentUniforms.m_Allocs.Size();
                            context->m_CurrentUniforms.m_Allocs.Push(alloc);
                        }
                    }
                    entries[desc.entryCount].buffer = context->m_CurrentUniforms.m_Allocs[context->m_CurrentUniforms.m_Alloc]->m_Buffer;
                    entries[desc.entryCount].offset = context->m_CurrentUniforms.m_Allocs[context->m_CurrentUniforms.m_Alloc]->m_Used;
                    entries[desc.entryCount].size   = pgm_res.m_Res->m_BindingInfo.m_BlockSize;
                    wgpuQueueWriteBuffer(context->m_Queue, entries[desc.entryCount].buffer, entries[desc.entryCount].offset, context->m_CurrentProgram->m_UniformData + pgm_res.m_DataOffset, entries[desc.entryCount].size);
                    context->m_CurrentUniforms.m_Allocs[context->m_CurrentUniforms.m_Alloc]->m_Used += DM_ALIGN(pgm_res.m_Res->m_BindingInfo.m_BlockSize, ubo_alignment);
                    break;
                }
                case ShaderResourceBinding::BINDING_FAMILY_GENERIC:
                    assert(false);
                    break;
            }
            dmHashUpdateBuffer64(&bindgroup_hash_state, entries + desc.entryCount, sizeof(entries[desc.entryCount]));
            ++desc.entryCount;
        }

        const uint64_t bindgroup_hash = dmHashFinal64(&bindgroup_hash_state);
        if (WGPUBindGroup* cached_bindgroup = context->m_BindGroupCache.Get(bindgroup_hash))
        {
            context->m_CurrentProgram->m_BindGroups[set] = *cached_bindgroup;
        }
        else
        {
            desc.entries            = entries;
            desc.layout             = context->m_CurrentProgram->m_BindGroupLayouts[set];
            WGPUBindGroup bindgroup = wgpuDeviceCreateBindGroup(context->m_Device, &desc);
            if (context->m_BindGroupCache.Full())
                context->m_BindGroupCache.SetCapacity(32, context->m_BindGroupCache.Capacity() + 4);
            context->m_BindGroupCache.Put(bindgroup_hash, bindgroup);
            context->m_CurrentProgram->m_BindGroups[set] = bindgroup;
        }
    }
}

static void WebGPUSetupComputePipeline(WebGPUContext* context)
{
    TRACE_CALL;
    WebGPUBeginComputePass(context);
    WebGPUUpdateBindGroups(context);

    // Get the pipeline for the active draw state
    WGPUComputePipeline pipeline = WebGPUGetOrCreateComputePipeline(context);
    if (pipeline != context->m_CurrentComputePass.m_Pipeline)
    {
        wgpuComputePassEncoderSetPipeline(context->m_CurrentComputePass.m_Encoder, pipeline);
        context->m_CurrentComputePass.m_Pipeline = pipeline;
    }

    // Set the bind groups
    for (int set = 0; set < context->m_CurrentProgram->m_MaxSet; ++set)
    {
        if (context->m_CurrentProgram->m_BindGroups[set])
            wgpuComputePassEncoderSetBindGroup(context->m_CurrentComputePass.m_Encoder, set, context->m_CurrentProgram->m_BindGroups[set], 0, 0);
    }
}

static void WebGPUSetupRenderPipeline(WebGPUContext* context, WebGPUBuffer* indexBuffer, Type indexBufferType)
{
    TRACE_CALL;
    WebGPUBeginRenderPass(context);
    WebGPUUpdateBindGroups(context);

    // Get the pipeline for the active draw state
    WGPURenderPipeline pipeline = WebGPUGetOrCreateRenderPipeline(context);
    if (pipeline != context->m_CurrentRenderPass.m_Pipeline)
    {
        wgpuRenderPassEncoderSetPipeline(context->m_CurrentRenderPass.m_Encoder, pipeline);
        context->m_CurrentRenderPass.m_Pipeline = pipeline;
    }

    // Set the indexbuffer
    if (indexBuffer && indexBuffer->m_Buffer != context->m_CurrentRenderPass.m_IndexBuffer)
    {
        assert(indexBufferType == TYPE_UNSIGNED_SHORT || indexBufferType == TYPE_UNSIGNED_INT);
        wgpuRenderPassEncoderSetIndexBuffer(context->m_CurrentRenderPass.m_Encoder, indexBuffer->m_Buffer, indexBufferType == TYPE_UNSIGNED_INT ? WGPUIndexFormat_Uint32 : WGPUIndexFormat_Uint16, 0, WGPU_WHOLE_SIZE);
        context->m_CurrentRenderPass.m_IndexBuffer = indexBuffer->m_Buffer;
        indexBuffer->m_LastRenderPass = context->m_RenderPasses;
    }

    // Set the vertexbuffer(s)
    for (int slot = 0; slot < MAX_VERTEX_BUFFERS; ++slot)
    {
        if (context->m_CurrentVertexBuffers[slot] && context->m_CurrentVertexBuffers[slot]->m_Buffer != context->m_CurrentRenderPass.m_VertexBuffers[slot])
        {
            wgpuRenderPassEncoderSetVertexBuffer(context->m_CurrentRenderPass.m_Encoder, slot, context->m_CurrentVertexBuffers[slot]->m_Buffer, 0, context->m_CurrentVertexBuffers[slot]->m_Used);
            context->m_CurrentRenderPass.m_VertexBuffers[slot] = context->m_CurrentVertexBuffers[slot]->m_Buffer;
            context->m_CurrentVertexBuffers[slot]->m_LastRenderPass = context->m_RenderPasses;
        }
    }

    // Set the bind groups
    for (int set = 0; set < context->m_CurrentProgram->m_MaxSet; ++set)
    {
        if (context->m_CurrentProgram->m_BindGroups[set] && context->m_CurrentProgram->m_BindGroups[set] != context->m_CurrentRenderPass.m_BindGroups[set])
        {
            wgpuRenderPassEncoderSetBindGroup(context->m_CurrentRenderPass.m_Encoder, set, context->m_CurrentProgram->m_BindGroups[set], 0, 0);
            context->m_CurrentRenderPass.m_BindGroups[set] = context->m_CurrentProgram->m_BindGroups[set];
        }
    }
}

static void WebGPUDrawElements(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
{
    TRACE_CALL;
    assert(_context);
    assert(index_buffer);
    // TODO: Instancing!
    WebGPUContext* context                         = (WebGPUContext*)_context;
    context->m_CurrentPipelineState.m_PrimtiveType = prim_type;
    WebGPUSetupRenderPipeline(context, (WebGPUBuffer*)index_buffer, type);
    wgpuRenderPassEncoderDrawIndexed(context->m_CurrentRenderPass.m_Encoder, count, 1, first / (type == TYPE_UNSIGNED_SHORT ? 2 : 4), 0, 0);
}

static void WebGPUDraw(HContext _context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
{
    TRACE_CALL;
    assert(_context);
    // TODO: Instancing!
    WebGPUContext* context                         = (WebGPUContext*)_context;
    context->m_CurrentPipelineState.m_PrimtiveType = prim_type;
    WebGPUSetupRenderPipeline(context, NULL, TYPE_BYTE);
    wgpuRenderPassEncoderDraw(context->m_CurrentRenderPass.m_Encoder, count, 1, first, 0);
}

static void WebGPUDispatchCompute(HContext _context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUSetupComputePipeline(context);
    wgpuComputePassEncoderDispatchWorkgroups(context->m_CurrentComputePass.m_Encoder, group_count_x, group_count_y, group_count_z);
}

static bool WebGPUCreateShaderModuleFromDDF(WebGPUContext* context, WebGPUShaderModule* shader, ShaderDesc* ddf)
{
    TRACE_CALL;
    ShaderDesc::Shader* ddf_shader = GetShaderProgram(context, ddf);
    {
        WGPUShaderModuleDescriptor shader_desc = {};
        WGPUShaderModuleWGSLDescriptor wgsl    = { { shader_desc.nextInChain, WGPUSType_ShaderModuleWGSLDescriptor }, (const char*)ddf_shader->m_Source.m_Data };
        shader_desc.nextInChain                = (WGPUChainedStruct*)&wgsl;
        shader->m_Module                       = wgpuDeviceCreateShaderModule(context->m_Device, &shader_desc);
    }
    {
        HashState64 shader_hash_state;
        dmHashInit64(&shader_hash_state, false);
        dmHashUpdateBuffer64(&shader_hash_state, ddf_shader->m_Source.m_Data, (uint32_t)ddf_shader->m_Source.m_Count);
        shader->m_Hash = dmHashFinal64(&shader_hash_state);
    }
    CreateShaderMeta(&ddf->m_Reflection, &shader->m_ShaderMeta);
    return true;
}

static void WebGPUDestroyShader(WebGPUShaderModule* shader)
{
    TRACE_CALL;
    wgpuShaderModuleRelease(shader->m_Module);
    shader->m_Module = NULL;
    DestroyShaderMeta(shader->m_ShaderMeta);
}

static HComputeProgram WebGPUNewComputeProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
{
    TRACE_CALL;
    WebGPUShaderModule* shader = new WebGPUShaderModule;
    memset(shader, 0, sizeof(*shader));
    WebGPUCreateShaderModuleFromDDF((WebGPUContext*)context, shader, ddf);
    return (HComputeProgram)shader;
}

static bool WebGPUReloadShader(WebGPUShaderModule* shader, ShaderDesc* ddf)
{
    TRACE_CALL;
    ShaderDesc::Shader* ddf_shader = GetShaderProgram((HContext)g_WebGPUContext, ddf);
    if (ddf_shader == NULL)
        return false;

    WebGPUShaderModule tmp_shader;
    if (WebGPUCreateShaderModuleFromDDF(g_WebGPUContext, &tmp_shader, ddf))
    {
        WebGPUDestroyShader(shader);
        memcpy(shader, &tmp_shader, sizeof(*shader));
        return true;
    }
    return false;
}

static void WebGPUUpdateBindGroupLayouts(WebGPUContext* context, WebGPUProgram* program, dmArray<ShaderResourceBinding>& resources, dmArray<ShaderResourceTypeInfo>& stage_type_infos, WGPUBindGroupLayoutEntry bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT], WGPUShaderStage stage_flag, ProgramResourceBindingsInfo& info)
{
    TRACE_CALL;
    for (int i = 0; i < resources.Size(); ++i)
    {
        ShaderResourceBinding& res = resources[i];
        assert(res.m_Set < MAX_SET_COUNT);
        assert(res.m_Binding < MAX_BINDINGS_PER_SET_COUNT);
        WGPUBindGroupLayoutEntry& binding = bindings[res.m_Set][res.m_Binding];

        if (binding.visibility == WGPUShaderStage_None)
        {
            binding.binding = res.m_Binding;

            ProgramResourceBinding& program_resource_binding = program->m_ResourceBindings[res.m_Set][res.m_Binding];
            program_resource_binding.m_Res                   = &res;
            program_resource_binding.m_TypeInfos             = &stage_type_infos;

            switch (res.m_BindingFamily)
            {
                case ShaderResourceBinding::BINDING_FAMILY_TEXTURE:
                    switch (res.m_Type.m_ShaderType)
                    {
                        case ShaderDesc::SHADER_TYPE_SAMPLER:
                            if (stage_flag == WGPUShaderStage_Compute)
                                binding.sampler.type = WGPUSamplerBindingType_NonFiltering;
                            else
                                binding.sampler.type = WGPUSamplerBindingType_Filtering;
                            break;
                        case ShaderDesc::SHADER_TYPE_TEXTURE_CUBE:
                            binding.texture.viewDimension = WGPUTextureViewDimension_Cube;
                            if (stage_flag == WGPUShaderStage_Compute)
                                binding.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
                            else
                                binding.texture.sampleType = WGPUTextureSampleType_Float;
                            break;
                        case ShaderDesc::SHADER_TYPE_IMAGE2D:
                        case ShaderDesc::SHADER_TYPE_UIMAGE2D:
                            binding.storageTexture.access        = WGPUStorageTextureAccess_WriteOnly;
                            binding.storageTexture.format        = WGPUTextureFormat_RGBA32Float; // ###
                            binding.storageTexture.viewDimension = WGPUTextureViewDimension_2D;
                            break;
                        case ShaderDesc::SHADER_TYPE_TEXTURE2D_ARRAY:
                            binding.texture.viewDimension = WGPUTextureViewDimension_2DArray;
                            if (stage_flag == WGPUShaderStage_Compute)
                                binding.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
                            else
                                binding.texture.sampleType = WGPUTextureSampleType_Float;
                            break;
                        default:
                            if (stage_flag == WGPUShaderStage_Compute)
                                binding.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
                            else
                                binding.texture.sampleType = WGPUTextureSampleType_Float;
                            break;
                    }
                    program_resource_binding.m_TextureUnit = info.m_TextureCount;
                    info.m_TextureCount++;
                    info.m_TotalUniformCount++;
                    break;
                case ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER: {
                    assert(false);
                    // const uint32_t ssbo_alignment = context->m_DeviceLimits.limits.minStorageBufferOffsetAlignment;
                    binding.buffer.type = WGPUBufferBindingType_Storage;

                    program_resource_binding.m_StorageBufferUnit = info.m_StorageBufferCount;
                    info.m_StorageBufferCount++;
                    info.m_TotalUniformCount++;
                    break;
                }
                case ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER: {
                    const uint32_t ubo_alignment = context->m_DeviceLimits.limits.minUniformBufferOffsetAlignment;
                    binding.buffer.type          = WGPUBufferBindingType_Uniform;

                    assert(res.m_Type.m_UseTypeIndex);
                    program_resource_binding.m_DataOffset         = info.m_UniformDataSize;
                    program_resource_binding.m_DynamicOffsetIndex = info.m_UniformBufferCount;

                    info.m_UniformBufferCount++;
                    info.m_UniformDataSize += res.m_BindingInfo.m_BlockSize;
                    info.m_UniformDataSizeAligned += DM_ALIGN(res.m_BindingInfo.m_BlockSize, ubo_alignment);
                    info.m_TotalUniformCount += stage_type_infos[res.m_Type.m_TypeIndex].m_Members.Size();
                    break;
                }
                case ShaderResourceBinding::BINDING_FAMILY_GENERIC:
                    break;
            }

            info.m_MaxSet     = dmMath::Max(info.m_MaxSet, (uint32_t)(res.m_Set + 1));
            info.m_MaxBinding = dmMath::Max(info.m_MaxBinding, (uint32_t)(res.m_Binding + 1));
        }

        binding.visibility |= stage_flag;
    }
}

static void WebGPUUpdateBindGroupLayouts(WebGPUContext* context, WebGPUProgram* program, WebGPUShaderModule* module, WGPUBindGroupLayoutEntry bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT], WGPUShaderStage stage_flag, ProgramResourceBindingsInfo& info)
{
    TRACE_CALL;
    WebGPUUpdateBindGroupLayouts(context, program, module->m_ShaderMeta.m_UniformBuffers, module->m_ShaderMeta.m_TypeInfos, bindings, stage_flag, info);
    WebGPUUpdateBindGroupLayouts(context, program, module->m_ShaderMeta.m_StorageBuffers, module->m_ShaderMeta.m_TypeInfos, bindings, stage_flag, info);
    WebGPUUpdateBindGroupLayouts(context, program, module->m_ShaderMeta.m_Textures, module->m_ShaderMeta.m_TypeInfos, bindings, stage_flag, info);
}

static void WebGPUUpdateProgramLayouts(WebGPUContext* context, WebGPUProgram* program)
{
    TRACE_CALL;

    // update layouts
    ProgramResourceBindingsInfo binding_info                                     = {};
    WGPUBindGroupLayoutEntry bindings[MAX_SET_COUNT][MAX_BINDINGS_PER_SET_COUNT] = {};
    if (program->m_ComputeModule)
    {
        WebGPUUpdateBindGroupLayouts(context, program, program->m_ComputeModule, bindings, WGPUShaderStage_Compute, binding_info);
    }
    else
    {
        assert(program->m_VertexModule && program->m_FragmentModule);
        WebGPUUpdateBindGroupLayouts(context, program, program->m_VertexModule, bindings, WGPUShaderStage_Vertex, binding_info);
        WebGPUUpdateBindGroupLayouts(context, program, program->m_FragmentModule, bindings, WGPUShaderStage_Fragment, binding_info);
    }

    // fill in program
    program->m_UniformData = new uint8_t[binding_info.m_UniformDataSize];
    memset(program->m_UniformData, 0, binding_info.m_UniformDataSize);
    program->m_UniformDataSizeAligned = binding_info.m_UniformDataSizeAligned;
    program->m_UniformBufferCount     = binding_info.m_UniformBufferCount;
    program->m_StorageBufferCount     = binding_info.m_StorageBufferCount;
    program->m_TextureSamplerCount    = binding_info.m_TextureCount;
    program->m_TotalUniformCount      = binding_info.m_TotalUniformCount;
    program->m_TotalResourcesCount    = binding_info.m_UniformBufferCount + binding_info.m_TextureCount + binding_info.m_StorageBufferCount; // num actual descriptors
    program->m_MaxSet                 = binding_info.m_MaxSet;
    program->m_MaxBinding             = binding_info.m_MaxBinding;

    // create bind group layout
    for (int set = 0; set < program->m_MaxSet; ++set)
    {
        WGPUBindGroupLayoutDescriptor desc = {};
        WGPUBindGroupLayoutEntry entries[MAX_BINDINGS_PER_SET_COUNT];
        desc.entries = entries;
        for (int binding = 0; binding < MAX_BINDINGS_PER_SET_COUNT; ++binding)
        {
            if (bindings[set][binding].visibility != WGPUShaderStage_None)
                entries[desc.entryCount++] = bindings[set][binding];
        }
        program->m_BindGroupLayouts[set] = wgpuDeviceCreateBindGroupLayout(context->m_Device, &desc);
    }

    // create pipeline layout
    {
        WGPUPipelineLayoutDescriptor desc = {};
        desc.bindGroupLayouts             = program->m_BindGroupLayouts;
        desc.bindGroupLayoutCount         = binding_info.m_MaxSet;
        program->m_PipelineLayout         = wgpuDeviceCreatePipelineLayout(context->m_Device, &desc);
    }
}

static void WebGPUCreateComputeProgram(WebGPUContext* context, WebGPUProgram* program, WebGPUShaderModule* compute_module)
{
    program->m_ComputeModule = compute_module;
    program->m_Hash          = compute_module->m_Hash;
    WebGPUUpdateProgramLayouts(context, program);
}

static void WebGPUCreateGraphicsProgram(WebGPUContext* context, WebGPUProgram* program, WebGPUShaderModule* vertex_module, WebGPUShaderModule* fragment_module)
{
    TRACE_CALL;
    program->m_VertexModule   = vertex_module;
    program->m_FragmentModule = fragment_module;
    {
        HashState64 program_hash;
        dmHashInit64(&program_hash, false);
        for (uint32_t i = 0; i < vertex_module->m_ShaderMeta.m_Inputs.Size(); i++)
            dmHashUpdateBuffer64(&program_hash, &vertex_module->m_ShaderMeta.m_Inputs[i].m_Binding, sizeof(vertex_module->m_ShaderMeta.m_Inputs[i].m_Binding));
        dmHashUpdateBuffer64(&program_hash, &vertex_module->m_Hash, sizeof(vertex_module->m_Hash));
        dmHashUpdateBuffer64(&program_hash, &fragment_module->m_Hash, sizeof(fragment_module->m_Hash));
        program->m_Hash = dmHashFinal64(&program_hash);
    }
    WebGPUUpdateProgramLayouts(context, program);
}

static HProgram WebGPUNewProgramFromCompute(HContext context, HComputeProgram compute_program)
{
    TRACE_CALL;
    WebGPUProgram* program = new WebGPUProgram;
    WebGPUCreateComputeProgram((WebGPUContext*)context, program, (WebGPUShaderModule*)compute_program);
    return (HProgram)program;
}

static void WebGPUDeleteComputeProgram(HComputeProgram prog)
{
    TRACE_CALL;
    WebGPUShaderModule* shader = (WebGPUShaderModule*)prog;
    WebGPUDestroyShader(shader);
    delete shader;
}

static HProgram WebGPUNewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
{
    TRACE_CALL;
    WebGPUProgram* program = new WebGPUProgram;
    WebGPUCreateGraphicsProgram((WebGPUContext*)context, program, (WebGPUShaderModule*)vertex_program, (WebGPUShaderModule*)fragment_program);
    return (HProgram)program;
}

static void WebGPUDestroyProgram(WebGPUContext* context, WebGPUProgram* program)
{
    TRACE_CALL;
    if (program->m_UniformData)
    {
        delete[] program->m_UniformData;
        program->m_UniformData = NULL;
    }
    for (size_t i = 0; i < MAX_SET_COUNT; ++i)
    {
        if (program->m_BindGroupLayouts[i])
        {
            wgpuBindGroupLayoutRelease(program->m_BindGroupLayouts[i]);
            program->m_BindGroupLayouts[i] = NULL;
        }
        if (program->m_BindGroups[i])
            program->m_BindGroups[i] = NULL;
    }
    if (program->m_PipelineLayout)
    {
        wgpuPipelineLayoutRelease(program->m_PipelineLayout);
        program->m_PipelineLayout = NULL;
    }
}

static void WebGPUDeleteProgram(HContext context, HProgram _program)
{
    TRACE_CALL;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    WebGPUDestroyProgram((WebGPUContext*)context, program);
    delete program;
}

static HVertexProgram WebGPUNewVertexProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
{
    TRACE_CALL;
    WebGPUShaderModule* shader = new WebGPUShaderModule;
    memset(shader, 0, sizeof(*shader));
    WebGPUCreateShaderModuleFromDDF((WebGPUContext*)context, shader, ddf);
    return (HComputeProgram)shader;
}

static HFragmentProgram WebGPUNewFragmentProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
{
    TRACE_CALL;
    WebGPUShaderModule* shader = new WebGPUShaderModule;
    memset(shader, 0, sizeof(*shader));
    WebGPUCreateShaderModuleFromDDF((WebGPUContext*)context, shader, ddf);
    return (HComputeProgram)shader;
}

static bool WebGPUReloadVertexProgram(HVertexProgram prog, ShaderDesc* ddf)
{
    TRACE_CALL;
    return WebGPUReloadShader((WebGPUShaderModule*)prog, ddf);
}

static bool WebGPUReloadFragmentProgram(HFragmentProgram prog, ShaderDesc* ddf)
{
    TRACE_CALL;
    return WebGPUReloadShader((WebGPUShaderModule*)prog, ddf);
}

static void WebGPUDeleteVertexProgram(HVertexProgram program)
{
    TRACE_CALL;
    WebGPUShaderModule* shader = (WebGPUShaderModule*)program;
    WebGPUDestroyShader(shader);
    delete shader;
}

static void WebGPUDeleteFragmentProgram(HFragmentProgram program)
{
    TRACE_CALL;
    WebGPUShaderModule* shader = (WebGPUShaderModule*)program;
    WebGPUDestroyShader(shader);
    delete shader;
}

static ShaderDesc::Language WebGPUGetProgramLanguage(HProgram program)
{
    TRACE_CALL;
    (void)program;
    return ShaderDesc::LANGUAGE_WGSL;
}

static bool WebGPUIsShaderLanguageSupported(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
{
    TRACE_CALL;
    return language == ShaderDesc::LANGUAGE_WGSL;
}

static void WebGPUEnableProgram(HContext _context, HProgram program)
{
    TRACE_CALL;
    WebGPUContext* context    = (WebGPUContext*)_context;
    context->m_CurrentProgram = (WebGPUProgram*)program;
}

static void WebGPUDisableProgram(HContext _context)
{
    TRACE_CALL;
    WebGPUContext* context    = (WebGPUContext*)_context;
    context->m_CurrentProgram = NULL;
}

static bool WebGPUReloadProgramGraphics(HContext _context, HProgram _program, HVertexProgram vertex_program, HFragmentProgram fragment_program)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    WebGPUDestroyProgram(context, program);
    WebGPUCreateGraphicsProgram(context, program, (WebGPUShaderModule*)vertex_program, (WebGPUShaderModule*)fragment_program);
    return true;
}

static bool WebGPUReloadProgramCompute(HContext _context, HProgram _program, HComputeProgram compute_program)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    WebGPUDestroyProgram(context, program);
    WebGPUCreateComputeProgram(context, program, (WebGPUShaderModule*)compute_program);
    return true;
}

static bool WebGPUReloadComputeProgram(HComputeProgram prog, ShaderDesc* ddf)
{
    TRACE_CALL;
    return WebGPUReloadShader((WebGPUShaderModule*)prog, ddf);
}

static uint32_t WebGPUGetAttributeCount(HProgram _program)
{
    TRACE_CALL;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    return program->m_VertexModule->m_ShaderMeta.m_Inputs.Size();
}

static void WebGPUGetAttribute(HProgram _program, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
{
    TRACE_CALL;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    assert(index < program->m_VertexModule->m_ShaderMeta.m_Inputs.Size());
    ShaderResourceBinding& attr = program->m_VertexModule->m_ShaderMeta.m_Inputs[index];

    *name_hash     = attr.m_NameHash;
    *type          = ShaderDataTypeToGraphicsType(attr.m_Type.m_ShaderType);
    *num_values    = 1;
    *location      = attr.m_Binding;
    *element_count = GetShaderTypeSize(attr.m_Type.m_ShaderType) / sizeof(float);
}

static uint32_t WebGPUGetUniformCount(HProgram _program)
{
    TRACE_CALL;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    return program->m_TotalUniformCount;
}

static uint32_t WebGPUGetUniformName(HProgram _program, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
{
    TRACE_CALL;
    WebGPUProgram* program = (WebGPUProgram*)_program;
    uint32_t search_index  = 0;
    for (int set = 0; set < program->m_MaxSet; ++set)
    {
        for (int binding = 0; binding < program->m_MaxBinding; ++binding)
        {
            ProgramResourceBinding& pgm_res = program->m_ResourceBindings[set][binding];
            if (pgm_res.m_Res == NULL)
                continue;

            if (pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_TEXTURE ||
                pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER)
            {
                if (search_index == index)
                {
                    ShaderResourceBinding* res = pgm_res.m_Res;
                    *type                      = ShaderDataTypeToGraphicsType(res->m_Type.m_ShaderType);
                    *size                      = 1;
                    return (uint32_t)dmStrlCpy(buffer, res->m_Name, buffer_size);
                }
                search_index++;
            }
            else if (pgm_res.m_Res->m_BindingFamily == ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER)
            {
                // TODO: Generic type lookup is not supported yet!
                // We can only support one level of indirection here right now
                assert(pgm_res.m_Res->m_Type.m_UseTypeIndex);
                const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
                const ShaderResourceTypeInfo& type_info           = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];

                const uint32_t num_members = type_info.m_Members.Size();
                for (int i = 0; i < num_members; ++i)
                {
                    if (search_index == index)
                    {
                        const ShaderResourceMember& member = type_info.m_Members[i];
                        *type                              = ShaderDataTypeToGraphicsType(member.m_Type.m_ShaderType);
                        *size                              = dmMath::Max((uint32_t)1, member.m_ElementCount);
                        return (uint32_t)dmStrlCpy(buffer, member.m_Name, buffer_size);
                    }
                    search_index++;
                }
            }
        }
    }
    return 0;
}

static HUniformLocation WebGPUGetUniformLocation(HProgram _program, const char* name)
{
    TRACE_CALL;
    WebGPUProgram* program   = (WebGPUProgram*)_program;
    const dmhash_t name_hash = dmHashString64(name);
    for (int set = 0; set < program->m_MaxSet; ++set)
    {
        for (int binding = 0; binding < program->m_MaxBinding; ++binding)
        {
            ProgramResourceBinding& pgm_res = program->m_ResourceBindings[set][binding];
            if (pgm_res.m_Res == NULL)
                continue;

            if (pgm_res.m_Res->m_NameHash == name_hash)
            {
                return set | binding << 16;
            }
            if (pgm_res.m_Res->m_Type.m_UseTypeIndex)
            {
                // TODO: Generic type lookup is not supported yet!
                // We can only support one level of indirection here right now
                const dmArray<ShaderResourceTypeInfo>& type_infos = *pgm_res.m_TypeInfos;
                const ShaderResourceTypeInfo& type_info           = type_infos[pgm_res.m_Res->m_Type.m_TypeIndex];
                const uint32_t num_members                        = type_info.m_Members.Size();
                for (int i = 0; i < num_members; ++i)
                {
                    const ShaderResourceMember& member = type_info.m_Members[i];

                    if (member.m_NameHash == name_hash)
                    {
                        return set | binding << 16 | ((uint64_t)i) << 32;
                    }
                }
            }
        }
    }

    return INVALID_UNIFORM_LOCATION;
}

static void WebGPUSetConstantV4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context = (WebGPUContext*)_context;
    assert(context->m_CurrentProgram);
    assert(base_location != INVALID_UNIFORM_LOCATION);

    const uint32_t set     = UNIFORM_LOCATION_GET_VS(base_location);
    const uint32_t binding = UNIFORM_LOCATION_GET_VS_MEMBER(base_location);
    const uint32_t member  = UNIFORM_LOCATION_GET_FS(base_location);
    assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

    const ProgramResourceBinding& pgm_res   = context->m_CurrentProgram->m_ResourceBindings[set][binding];
    const ShaderResourceTypeInfo& type_info = (*pgm_res.m_TypeInfos)[pgm_res.m_Res->m_Type.m_TypeIndex];
    if (memcpy(context->m_CurrentProgram->m_UniformData + pgm_res.m_DataOffset + type_info.m_Members[member].m_Offset,
               (uint8_t*)data,
               sizeof(dmVMath::Vector4) * count))
    {
        memcpy(context->m_CurrentProgram->m_UniformData + pgm_res.m_DataOffset + type_info.m_Members[member].m_Offset,
               (uint8_t*)data,
               sizeof(dmVMath::Vector4) * count);
        context->m_CurrentProgram->m_BindGroups[set] = NULL;
    }
}

static void WebGPUSetConstantM4(HContext _context, const Vector4* data, int count, HUniformLocation base_location)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context = (WebGPUContext*)_context;
    assert(context->m_CurrentProgram);
    assert(base_location != INVALID_UNIFORM_LOCATION);

    const uint32_t set     = UNIFORM_LOCATION_GET_VS(base_location);
    const uint32_t binding = UNIFORM_LOCATION_GET_VS_MEMBER(base_location);
    const uint32_t member  = UNIFORM_LOCATION_GET_FS(base_location);
    assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

    const ProgramResourceBinding& pgm_res   = context->m_CurrentProgram->m_ResourceBindings[set][binding];
    const ShaderResourceTypeInfo& type_info = (*pgm_res.m_TypeInfos)[pgm_res.m_Res->m_Type.m_TypeIndex];
    if (memcmp(context->m_CurrentProgram->m_UniformData + pgm_res.m_DataOffset + type_info.m_Members[member].m_Offset,
               (uint8_t*)data,
               sizeof(dmVMath::Vector4) * 4 * count))
    {
        memcpy(context->m_CurrentProgram->m_UniformData + pgm_res.m_DataOffset + type_info.m_Members[member].m_Offset,
               (uint8_t*)data,
               sizeof(dmVMath::Vector4) * 4 * count);
        context->m_CurrentProgram->m_BindGroups[set] = NULL;
    }
}

static void WebGPUSetSampler(HContext _context, HUniformLocation location, int32_t unit)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    assert(context->m_CurrentProgram);
    assert(location != INVALID_UNIFORM_LOCATION);

    const uint32_t set     = UNIFORM_LOCATION_GET_VS(location);
    const uint32_t binding = UNIFORM_LOCATION_GET_VS_MEMBER(location);
    assert(!(set == UNIFORM_LOCATION_MAX && binding == UNIFORM_LOCATION_MAX));

    if (context->m_CurrentProgram->m_ResourceBindings[set][binding].m_TextureUnit != unit)
    {
        context->m_CurrentProgram->m_ResourceBindings[set][binding].m_TextureUnit = unit;
        context->m_CurrentProgram->m_BindGroups[set]                              = NULL;
    }
}

static bool WebGPUIsTextureFormatSupported(HContext context, TextureFormat format)
{
    TRACE_CALL;
    return (((WebGPUContext*)context)->m_TextureFormatSupport & (1 << format)) != 0;
}

static uint32_t WebGPUGetMaxTextureSize(HContext context)
{
    TRACE_CALL;
    return ((WebGPUContext*)context)->m_DeviceLimits.limits.maxTextureDimension2D;
}

static HTexture WebGPUNewTexture(HContext _context, const TextureCreationParams& params)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUTexture* texture = WebGPUNewTextureInternal(params);
    return StoreAssetInContainer(context->m_AssetHandleContainer, texture, ASSET_TYPE_TEXTURE);
}

static void WebGPUDeleteTexture(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    if (texture)
    {
        if (texture->m_Texture)
            wgpuTextureRelease(texture->m_Texture);
        if (texture->m_TextureView)
            wgpuTextureViewRelease(texture->m_TextureView);
        delete texture;
        g_WebGPUContext->m_AssetHandleContainer.Release(_texture);
    }
}

static HandleResult WebGPUGetTextureHandle(HTexture texture, void** out_handle)
{
    TRACE_CALL;
    *out_handle = NULL;
    return HANDLE_RESULT_NOT_AVAILABLE;
}

static void WebGPUSetTextureParams(HTexture _texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    WebGPUSetTextureParamsInternal(texture, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
}

static void WebGPUSetTexture(HTexture _texture, const TextureParams& params)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    WebGPUSetTextureInternal(texture, params);
}

static uint32_t WebGPUGetTextureResourceSize(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    uint32_t size_total    = 0;
    uint32_t size          = texture->m_Width * texture->m_Height * dmMath::Max(1U, GetTextureFormatBitsPerPixel(texture->m_GraphicsFormat) / 8);
    for (uint32_t i = 0; i < texture->m_MipMapCount; ++i)
    {
        size_total += size;
        size >>= 2;
    }
    if (texture->m_Type == TEXTURE_TYPE_CUBE_MAP || texture->m_Type == TEXTURE_TYPE_TEXTURE_CUBE)
    {
        size_total *= 6;
    }
    return size_total + sizeof(*texture);
}

static uint16_t WebGPUGetTextureWidth(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_Width;
}

static uint16_t WebGPUGetTextureHeight(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_Height;
}

static uint16_t WebGPUGetOriginalTextureWidth(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_OriginalWidth;
}

static uint16_t WebGPUGetOriginalTextureHeight(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_OriginalHeight;
}

static void WebGPUEnableTexture(HContext _context, uint32_t unit, uint8_t id_index, HTexture _texture)
{
    TRACE_CALL;
    assert(unit < MAX_TEXTURE_COUNT);
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, _texture);
    if (context->m_CurrentTextureUnits[unit] != texture)
    {
        context->m_CurrentTextureUnits[unit] = texture;
        if (context->m_CurrentProgram)
        {
            for (int set = 0; set < context->m_CurrentProgram->m_MaxSet; ++set)
            {
                if (!context->m_CurrentProgram->m_BindGroups[set])
                    continue;
                for (int binding = 0; binding < context->m_CurrentProgram->m_MaxBinding; ++binding)
                {
                    ProgramResourceBinding& pgm_res = context->m_CurrentProgram->m_ResourceBindings[set][binding];
                    if (pgm_res.m_Res == NULL)
                        continue;
                    if (context->m_CurrentProgram->m_ResourceBindings[set][binding].m_TextureUnit == unit)
                    {
                        context->m_CurrentProgram->m_BindGroups[set] = NULL;
                        break;
                    }
                }
            }
        }
    }
}

static void WebGPUDisableTexture(HContext context, uint32_t unit, HTexture texture)
{
    TRACE_CALL;
    assert(unit < MAX_TEXTURE_COUNT);
    ((WebGPUContext*)context)->m_CurrentTextureUnits[unit] = NULL;
}

static void WebGPUReadPixels(HContext context, void* buffer, uint32_t buffer_size)
{
    TRACE_CALL;
    assert(false);
}

static HRenderTarget WebGPUNewRenderTarget(HContext _context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    WebGPURenderTarget* rt = new WebGPURenderTarget();
    rt->m_Multisample      = 1;
    rt->m_Width = rt->m_Height = 0;

    // colors
    const BufferType color_buffer_flags[] = {
        BUFFER_TYPE_COLOR0_BIT,
        BUFFER_TYPE_COLOR1_BIT,
        BUFFER_TYPE_COLOR2_BIT,
        BUFFER_TYPE_COLOR3_BIT,
    };
    for (int i = 0; i < MAX_BUFFER_COLOR_ATTACHMENTS; ++i)
    {
        const BufferType buffer_type = color_buffer_flags[i];
        if (buffer_type_flags & buffer_type)
        {
            rt->m_ColorBufferLoadOps[rt->m_ColorBufferCount]  = params.m_ColorBufferLoadOps[i];
            rt->m_ColorBufferStoreOps[rt->m_ColorBufferCount] = params.m_ColorBufferStoreOps[i];
            memcpy(rt->m_ColorBufferClearValue + rt->m_ColorBufferCount, params.m_ColorBufferClearValue + i, sizeof(params.m_ColorBufferClearValue[i]));
            {
                WebGPUTexture* texture    = WebGPUNewTextureInternal(params.m_ColorBufferCreationParams[i]);
                texture->m_GraphicsFormat = params.m_ColorBufferParams[i].m_Format;
                WebGPURealizeTexture(texture, WebGPUFormatFromTextureFormat(params.m_ColorBufferParams[i].m_Format), 1, 1, WGPUTextureUsage_RenderAttachment);
                rt->m_TextureColor[rt->m_ColorBufferCount] = StoreAssetInContainer(context->m_AssetHandleContainer, texture, ASSET_TYPE_TEXTURE);
                if (!rt->m_Width)
                {
                    rt->m_Width  = texture->m_Width;
                    rt->m_Height = texture->m_Height;
                }
                assert(rt->m_Width == texture->m_Width && rt->m_Height == texture->m_Height);
            }
            ++rt->m_ColorBufferCount;
        }
    }

    // depth/stencil
    const bool has_depth = buffer_type_flags & dmGraphics::BUFFER_TYPE_DEPTH_BIT, has_stencil = buffer_type_flags & dmGraphics::BUFFER_TYPE_STENCIL_BIT;
    if (has_depth || has_stencil)
    {
        WGPUTextureFormat format;
        WebGPUTexture* texture = NULL;
        if (has_depth)
        {
            texture                   = WebGPUNewTextureInternal(params.m_DepthBufferCreationParams);
            texture->m_GraphicsFormat = params.m_DepthBufferParams.m_Format;
            if (has_stencil)
                format = WGPUTextureFormat_Depth24PlusStencil8;
            else
                format = WGPUTextureFormat_Depth24Plus;
        }
        else if (has_stencil)
        {
            texture                   = WebGPUNewTextureInternal(params.m_StencilBufferCreationParams);
            texture->m_GraphicsFormat = params.m_StencilBufferParams.m_Format;
            format                    = WGPUTextureFormat_Stencil8;
        }
        assert(texture);
        WebGPURealizeTexture(texture, format, 1, 1, WGPUTextureUsage_RenderAttachment);
        rt->m_TextureDepthStencil = StoreAssetInContainer(context->m_AssetHandleContainer, texture, ASSET_TYPE_TEXTURE);
        if (!rt->m_Width)
        {
            rt->m_Width  = texture->m_Width;
            rt->m_Height = texture->m_Height;
        }
        assert(rt->m_Width == texture->m_Width && rt->m_Height == texture->m_Height);
    }

    assert(rt->m_Width && rt->m_Height);
    return StoreAssetInContainer(context->m_AssetHandleContainer, rt, ASSET_TYPE_RENDER_TARGET);
}

static void WebGPUDeleteRenderTarget(HRenderTarget _rt)
{
    TRACE_CALL;
    WebGPURenderTarget* rt = GetAssetFromContainer<WebGPURenderTarget>(g_WebGPUContext->m_AssetHandleContainer, _rt);
    for (size_t i = 0; i < rt->m_ColorBufferCount; ++i)
    {
        if (rt->m_TextureColor[i])
            WebGPUDeleteTexture(rt->m_TextureColor[i]);
        if (rt->m_TextureResolve[i])
            WebGPUDeleteTexture(rt->m_TextureResolve[i]);
    }
    if (rt->m_TextureDepthStencil)
        WebGPUDeleteTexture(rt->m_TextureDepthStencil);
    delete rt;
    g_WebGPUContext->m_AssetHandleContainer.Release(_rt);
}

static void WebGPUSetRenderTarget(HContext _context, HRenderTarget _rt, uint32_t transient_buffer_types)
{
    TRACE_CALL;
    (void)transient_buffer_types;
    assert(_context);
    WebGPUContext* context         = (WebGPUContext*)_context;
    WebGPURenderTarget* rt         = GetAssetFromContainer<WebGPURenderTarget>(context->m_AssetHandleContainer, _rt);
    context->m_ViewportChanged     = 1;
    context->m_CurrentRenderTarget = rt ? rt : context->m_MainRenderTarget;
}

static HTexture WebGPUGetRenderTargetTexture(HRenderTarget _rt, BufferType buffer_type)
{
    TRACE_CALL;
    WebGPURenderTarget* rt = GetAssetFromContainer<WebGPURenderTarget>(g_WebGPUContext->m_AssetHandleContainer, _rt);
    if (IsColorBufferType(buffer_type))
        return rt->m_TextureColor[GetBufferTypeIndex(buffer_type)];
    if (buffer_type == BUFFER_TYPE_DEPTH_BIT || buffer_type == BUFFER_TYPE_STENCIL_BIT)
        return rt->m_TextureDepthStencil;
    return NULL;
}

static void WebGPUGetRenderTargetSize(HRenderTarget _rt, BufferType buffer_type, uint32_t& width, uint32_t& height)
{
    TRACE_CALL;
    WebGPURenderTarget* rt = GetAssetFromContainer<WebGPURenderTarget>(g_WebGPUContext->m_AssetHandleContainer, _rt);
    width                  = rt->m_Width;
    height                 = rt->m_Height;
}

static void WebGPUSetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
{
    TRACE_CALL;
    assert(false);
}

static void WebGPUEnableState(HContext context, State state)
{
    TRACE_CALL;
    assert(context);
    SetPipelineStateValue(((WebGPUContext*)context)->m_CurrentPipelineState, state, 1);
}

static void WebGPUDisableState(HContext context, State state)
{
    TRACE_CALL;
    assert(context);
    SetPipelineStateValue(((WebGPUContext*)context)->m_CurrentPipelineState, state, 0);
}

static void WebGPUSetBlendFunc(HContext _context, BlendFactor source_factor, BlendFactor destinaton_factor)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context                           = (WebGPUContext*)_context;
    context->m_CurrentPipelineState.m_BlendSrcFactor = source_factor;
    context->m_CurrentPipelineState.m_BlendDstFactor = destinaton_factor;
}

static void WebGPUSetColorMask(HContext _context, bool red, bool green, bool blue, bool alpha)
{
    TRACE_CALL;
    WebGPUContext* context                           = (WebGPUContext*)_context;
    context->m_CurrentPipelineState.m_WriteColorMask = 0;
    if (red)
        context->m_CurrentPipelineState.m_WriteColorMask |= DM_GRAPHICS_STATE_WRITE_R;
    if (green)
        context->m_CurrentPipelineState.m_WriteColorMask |= DM_GRAPHICS_STATE_WRITE_G;
    if (blue)
        context->m_CurrentPipelineState.m_WriteColorMask |= DM_GRAPHICS_STATE_WRITE_B;
    if (alpha)
        context->m_CurrentPipelineState.m_WriteColorMask |= DM_GRAPHICS_STATE_WRITE_A;
}

static void WebGPUSetDepthMask(HContext context, bool mask)
{
    TRACE_CALL;
    assert(context);
    ((WebGPUContext*)context)->m_CurrentPipelineState.m_WriteDepth = mask;
}

static void WebGPUSetDepthFunc(HContext context, CompareFunc func)
{
    TRACE_CALL;
    assert(context);
    ((WebGPUContext*)context)->m_CurrentPipelineState.m_DepthTestFunc = func;
}

static void WebGPUSetViewport(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
{
    TRACE_CALL;
    WebGPUContext* context     = (WebGPUContext*)_context;
    context->m_ViewportChanged = 1;
    context->m_ViewportRect[0] = x;
    context->m_ViewportRect[1] = y;
    context->m_ViewportRect[2] = width;
    context->m_ViewportRect[3] = height;
}

static void WebGPUSetScissor(HContext _context, int32_t x, int32_t y, int32_t width, int32_t height)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context     = (WebGPUContext*)_context;
    context->m_ViewportChanged = 1;
    context->m_ScissorRect[0]  = x;
    context->m_ScissorRect[1]  = y;
    context->m_ScissorRect[2]  = width;
    context->m_ScissorRect[3]  = height;
}

static void WebGPUSetStencilMask(HContext context, uint32_t mask)
{
    TRACE_CALL;
    assert(context);
    ((WebGPUContext*)context)->m_CurrentPipelineState.m_StencilWriteMask = mask;
}

static void WebGPUSetStencilFunc(HContext _context, CompareFunc func, uint32_t ref, uint32_t mask)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context                                 = (WebGPUContext*)_context;
    context->m_CurrentPipelineState.m_StencilFrontTestFunc = (uint8_t)func;
    context->m_CurrentPipelineState.m_StencilBackTestFunc  = (uint8_t)func;
    context->m_CurrentPipelineState.m_StencilReference     = (uint8_t)ref;
    context->m_CurrentPipelineState.m_StencilCompareMask   = (uint8_t)mask;
}

static void WebGPUSetStencilOp(HContext _context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context                                    = (WebGPUContext*)_context;
    context->m_CurrentPipelineState.m_StencilFrontOpFail      = sfail;
    context->m_CurrentPipelineState.m_StencilFrontOpDepthFail = dpfail;
    context->m_CurrentPipelineState.m_StencilFrontOpPass      = dppass;
    context->m_CurrentPipelineState.m_StencilBackOpFail       = sfail;
    context->m_CurrentPipelineState.m_StencilBackOpDepthFail  = dpfail;
    context->m_CurrentPipelineState.m_StencilBackOpPass       = dppass;
}

static void WebGPUSetStencilFuncSeparate(HContext _context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;

    if (face_type == FACE_TYPE_BACK)
    {
        context->m_CurrentPipelineState.m_StencilBackTestFunc = (uint8_t)func;
    }
    else
    {
        context->m_CurrentPipelineState.m_StencilFrontTestFunc = (uint8_t)func;
    }
    context->m_CurrentPipelineState.m_StencilReference   = (uint8_t)ref;
    context->m_CurrentPipelineState.m_StencilCompareMask = (uint8_t)mask;
}

static void WebGPUSetStencilOpSeparate(HContext _context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;

    if (face_type == FACE_TYPE_BACK)
    {
        context->m_CurrentPipelineState.m_StencilBackOpFail      = sfail;
        context->m_CurrentPipelineState.m_StencilBackOpDepthFail = dpfail;
        context->m_CurrentPipelineState.m_StencilBackOpPass      = dppass;
    }
    else
    {
        context->m_CurrentPipelineState.m_StencilFrontOpFail      = sfail;
        context->m_CurrentPipelineState.m_StencilFrontOpDepthFail = dpfail;
        context->m_CurrentPipelineState.m_StencilFrontOpPass      = dppass;
    }
}

static void WebGPUSetFaceWinding(HContext context, FaceWinding face_winding)
{
    TRACE_CALL;
    ((WebGPUContext*)context)->m_CurrentPipelineState.m_FaceWinding = face_winding;
}

static void WebGPUSetCullFace(HContext context, FaceType face_type)
{
    TRACE_CALL;
    assert(context);
}

static void WebGPUSetPolygonOffset(HContext context, float factor, float units)
{
    TRACE_CALL;
    assert(context);
}

static PipelineState WebGPUGetPipelineState(HContext context)
{
    TRACE_CALL;
    return ((WebGPUContext*)context)->m_CurrentPipelineState;
}

static void WebGPUSetTextureAsync(HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
{
    TRACE_CALL;
    WebGPUSetTexture(texture, params);
    if (callback)
        callback(texture, user_data);
}

static uint32_t WebGPUGetTextureStatusFlags(HTexture texture)
{
    TRACE_CALL;
    return 0;
}

static bool WebGPUIsExtensionSupported(HContext context, const char* extension)
{
    TRACE_CALL;
    return true;
}

static TextureType WebGPUGetTextureType(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_Type;
}

static uint32_t WebGPUGetNumSupportedExtensions(HContext context)
{
    TRACE_CALL;
    return 0;
}

static const char* WebGPUGetSupportedExtension(HContext context, uint32_t index)
{
    TRACE_CALL;
    return "";
}

static uint8_t WebGPUGetNumTextureHandles(HTexture texture)
{
    TRACE_CALL;
    return 1;
}

static uint32_t WebGPUGetTextureUsageHintFlags(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_UsageHintFlags;
}

static bool WebGPUIsContextFeatureSupported(HContext _context, ContextFeature feature)
{
    TRACE_CALL;
    WebGPUContext* context = (WebGPUContext*)_context;
    return (context->m_ContextFeatures & (1 << feature)) != 0;
}

static uint16_t WebGPUGetTextureDepth(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_Depth;
}

static uint8_t WebGPUGetTextureMipmapCount(HTexture _texture)
{
    TRACE_CALL;
    WebGPUTexture* texture = GetAssetFromContainer<WebGPUTexture>(g_WebGPUContext->m_AssetHandleContainer, _texture);
    return texture->m_MipMapCount;
}

static bool WebGPUIsAssetHandleValid(HContext _context, HAssetHandle asset_handle)
{
    TRACE_CALL;
    assert(_context);
    if (asset_handle == 0)
    {
        return false;
    }
    WebGPUContext* context = (WebGPUContext*)_context;
    AssetType type         = GetAssetType(asset_handle);
    if (type == ASSET_TYPE_TEXTURE)
        return GetAssetFromContainer<WebGPUTexture>(context->m_AssetHandleContainer, asset_handle) != 0;
    if (type == ASSET_TYPE_RENDER_TARGET)
        return GetAssetFromContainer<WebGPURenderTarget>(context->m_AssetHandleContainer, asset_handle) != 0;
    return false;
}

void WebGPUCleanupRenderPipelineCache(WebGPUContext* context, const uint64_t* key, WGPURenderPipeline* value)
{
    wgpuRenderPipelineRelease(*value);
}

void WebGPUCleanupComputePipelineCache(WebGPUContext* context, const uint64_t* key, WGPUComputePipeline* value)
{
    wgpuComputePipelineRelease(*value);
}

void WebGPUCleanupBindGroupCache(WebGPUContext* context, const uint64_t* key, WGPUBindGroup* value)
{
    wgpuBindGroupRelease(*value);
}

void WebGPUCleanupSamplerCache(WebGPUContext* context, const uint64_t* key, WGPUSampler* value)
{
    wgpuSamplerRelease(*value);
}

static void WebGPUCloseWindow(HContext _context)
{
    TRACE_CALL;
    assert(_context);
    WebGPUContext* context = (WebGPUContext*)_context;
    if (dmPlatform::GetWindowStateParam(context->m_Window, dmPlatform::WINDOW_STATE_OPENED))
    {
        context->m_RenderPipelineCache.Iterate(WebGPUCleanupRenderPipelineCache, context);
        context->m_ComputePipelineCache.Iterate(WebGPUCleanupComputePipelineCache, context);
        context->m_BindGroupCache.Iterate(WebGPUCleanupBindGroupCache, context);
        context->m_SamplerCache.Iterate(WebGPUCleanupSamplerCache, context);

        if (context->m_MainRenderTarget)
        {
            WebGPUDeleteTexture(context->m_MainRenderTarget->m_TextureResolve[0]);
            WebGPUDeleteTexture(context->m_MainRenderTarget->m_TextureColor[0]);
            WebGPUDeleteTexture(context->m_MainRenderTarget->m_TextureDepthStencil);
            delete context->m_MainRenderTarget;
            context->m_MainRenderTarget = NULL;
        }

        context->m_Width  = 0;
        context->m_Height = 0;
        dmPlatform::CloseWindow(context->m_Window);
    }
}

static GraphicsAdapterFunctionTable WebGPURegisterFunctionTable()
{
    GraphicsAdapterFunctionTable fn_table = {};
    DM_REGISTER_GRAPHICS_FUNCTION_TABLE(fn_table, WebGPU);
    return fn_table;
}
