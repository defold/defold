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

#include "graphics.h"
#include "graphics_private.h"
#include "graphics_adapter.h"

#if defined(DM_PLATFORM_IOS)
#include  <glfw/glfw_native.h> // for glfwAppBootstrap
#endif
#include <string.h>
#include <assert.h>
#include <dlib/profile.h>
#include <dlib/math.h>

DM_PROPERTY_GROUP(rmtp_Graphics, "Graphics");
DM_PROPERTY_U32(rmtp_DrawCalls, 0, FrameReset, "# vertices", &rmtp_Graphics);
DM_PROPERTY_U32(rmtp_DispatchCalls, 0, FrameReset, "# dispatches", &rmtp_Graphics);

#include <dlib/log.h>
#include <dlib/dstrings.h>

namespace dmGraphics
{
    static GraphicsAdapter*             g_adapter_list = 0;
    static GraphicsAdapter*             g_adapter = 0;
    static GraphicsAdapterFunctionTable g_functions;

    void RegisterGraphicsAdapter(GraphicsAdapter* adapter,
        GraphicsAdapterIsSupportedCb              is_supported_cb,
        GraphicsAdapterRegisterFunctionsCb        register_functions_cb,
        GraphicsAdapterGetContextCb               get_context_cb,
        int8_t                                    priority)
    {
        adapter->m_Next          = g_adapter_list;
        adapter->m_IsSupportedCb = is_supported_cb;
        adapter->m_RegisterCb    = register_functions_cb;
        adapter->m_GetContextCb  = get_context_cb;
        adapter->m_Priority      = priority;
        g_adapter_list           = adapter;
    }

    static bool SelectAdapterByFamily(AdapterFamily family)
    {
        if (family != ADAPTER_FAMILY_NONE)
        {
            GraphicsAdapter* next = g_adapter_list;

            while(next)
            {
                if (next->m_Family == family && next->m_IsSupportedCb())
                {
                    g_functions = next->m_RegisterCb();
                    g_adapter   = next;
                    return true;
                }
                next = next->m_Next;
            }
        }

        return false;
    }

    static bool SelectAdapterByPriority()
    {
        GraphicsAdapter* next     = g_adapter_list;
        GraphicsAdapter* selected = next;

        while(next)
        {
            bool is_supported = next->m_IsSupportedCb();
            if (next->m_Priority < selected->m_Priority && is_supported)
            {
                selected = next;
            }

            next = next->m_Next;
        }

        if (!selected)
        {
            return false;
        }

        g_functions = selected->m_RegisterCb();
        g_adapter   = selected;
        return true;
    }

    AdapterFamily GetAdapterFamily(const char* adapter_name)
    {
        if (adapter_name == 0)
            return ADAPTER_FAMILY_NONE;
        if (dmStrCaseCmp("null", adapter_name) == 0)
            return ADAPTER_FAMILY_NULL;
        if (dmStrCaseCmp("opengl", adapter_name) == 0)
            return ADAPTER_FAMILY_OPENGL;
        if (dmStrCaseCmp("vulkan", adapter_name) == 0)
            return ADAPTER_FAMILY_VULKAN;
        if (dmStrCaseCmp("webgpu", adapter_name) == 0)
            return ADAPTER_FAMILY_WEBGPU;
        if (dmStrCaseCmp("vendor", adapter_name) == 0)
            return ADAPTER_FAMILY_VENDOR;
        assert(0 && "Adapter type not supported?");
        return ADAPTER_FAMILY_NONE;
    }

    #define GRAPHICS_ENUM_TO_STR_CASE(x) case x: return #x;

    const char* GetAdapterFamilyLiteral(AdapterFamily family)
    {
        switch(family)
        {
            GRAPHICS_ENUM_TO_STR_CASE(ADAPTER_FAMILY_NONE);
            GRAPHICS_ENUM_TO_STR_CASE(ADAPTER_FAMILY_NULL);
            GRAPHICS_ENUM_TO_STR_CASE(ADAPTER_FAMILY_OPENGL);
            GRAPHICS_ENUM_TO_STR_CASE(ADAPTER_FAMILY_VULKAN);
            GRAPHICS_ENUM_TO_STR_CASE(ADAPTER_FAMILY_VENDOR);
            GRAPHICS_ENUM_TO_STR_CASE(ADAPTER_FAMILY_WEBGPU);
            default:break;
        }
        return "<unknown dmGraphics::AdapterFamily>";
    }

    const char* GetTextureTypeLiteral(TextureType texture_type)
    {
        switch(texture_type)
        {
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_TYPE_2D);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_TYPE_2D_ARRAY);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_TYPE_CUBE_MAP);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_TYPE_IMAGE_2D);
            default:break;
        }
        return "<unknown dmGraphics::TextureType>";
    }

    const char* GetBufferTypeLiteral(BufferType buffer_type)
    {
        switch(buffer_type)
        {
            GRAPHICS_ENUM_TO_STR_CASE(BUFFER_TYPE_COLOR0_BIT);
            GRAPHICS_ENUM_TO_STR_CASE(BUFFER_TYPE_COLOR1_BIT);
            GRAPHICS_ENUM_TO_STR_CASE(BUFFER_TYPE_COLOR2_BIT);
            GRAPHICS_ENUM_TO_STR_CASE(BUFFER_TYPE_COLOR3_BIT);
            GRAPHICS_ENUM_TO_STR_CASE(BUFFER_TYPE_DEPTH_BIT);
            GRAPHICS_ENUM_TO_STR_CASE(BUFFER_TYPE_STENCIL_BIT);
            default:break;
        }
        return "<unknown dmGraphics::BufferType>";
    }

    const char* GetGraphicsTypeLiteral(Type type)
    {
        switch(type)
        {
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_BYTE);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_UNSIGNED_BYTE);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_SHORT);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_UNSIGNED_SHORT);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_INT);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_UNSIGNED_INT);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT_VEC4);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT_MAT4);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_SAMPLER_2D);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_SAMPLER_CUBE);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_SAMPLER_2D_ARRAY);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT_VEC2);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT_VEC3);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT_MAT2);
            GRAPHICS_ENUM_TO_STR_CASE(TYPE_FLOAT_MAT3);
            default:break;
        }
        return "<unknown dmGraphics::Type>";
    }

    const char* GetAssetTypeLiteral(AssetType type)
    {
        switch(type)
        {
            GRAPHICS_ENUM_TO_STR_CASE(ASSET_TYPE_NONE);
            GRAPHICS_ENUM_TO_STR_CASE(ASSET_TYPE_TEXTURE);
            GRAPHICS_ENUM_TO_STR_CASE(ASSET_TYPE_RENDER_TARGET);
            default:break;
        }
        return "<unknown dmGraphics::AssetType>";
    }

    const char* GetTextureFormatLiteral(TextureFormat format)
    {
        switch(format)
        {
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_LUMINANCE);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_LUMINANCE_ALPHA);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB_16BPP);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_16BPP);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_DEPTH);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_STENCIL);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB_PVRTC_2BPPV1);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB_ETC1);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_ETC2);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_ASTC_4x4);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB_BC1);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_BC3);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_R_BC4);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RG_BC5);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA_BC7);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB16F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGB32F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA16F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA32F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_R16F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RG16F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_R32F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RG32F);
            GRAPHICS_ENUM_TO_STR_CASE(TEXTURE_FORMAT_RGBA32UI);
            default:break;
        }
        return "<unknown dmGraphics::TextureFormat>";
    }

    #undef GRAPHICS_ENUM_TO_STR_CASE

    #define SHADERDESC_ENUM_TO_STR_CASE(x) case ShaderDesc::x: return #x;

    const char* GetShaderProgramLanguageLiteral(ShaderDesc::Language language)
    {
        switch(language)
        {
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_GLSL_SM120);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_GLSL_SM140);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_GLES_SM100);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_GLES_SM300);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_GLSL_SM430);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_GLSL_SM330);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_SPIRV);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_PSSL);
            SHADERDESC_ENUM_TO_STR_CASE(LANGUAGE_WGSL);
            default:break;
        }
        return "<unknown ShaderDesc::Language>";
    }

    #undef SHADERDESC_ENUM_TO_STR_CASE

    ContextParams::ContextParams()
    {
        memset(this, 0x0, sizeof(*this));
        m_DefaultTextureMinFilter = TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
        m_DefaultTextureMagFilter = TEXTURE_FILTER_LINEAR;
        m_SwapInterval            = 1;
    }

    AttachmentToBufferType::AttachmentToBufferType()
    {
        memset(m_AttachmentToBufferType, 0x0, sizeof(m_AttachmentToBufferType));
        m_AttachmentToBufferType[ATTACHMENT_COLOR] = BUFFER_TYPE_COLOR0_BIT;
        m_AttachmentToBufferType[ATTACHMENT_DEPTH] = BUFFER_TYPE_DEPTH_BIT;
        m_AttachmentToBufferType[ATTACHMENT_STENCIL] = BUFFER_TYPE_STENCIL_BIT;
    }

    HContext NewContext(const ContextParams& params)
    {
        return g_functions.m_NewContext(params);
    }

    HContext GetInstalledContext()
    {
        assert(g_adapter && "No graphics adapter installed");
        return g_adapter->m_GetContextCb();
    }

    static inline BufferType GetAttachmentBufferType(RenderTargetAttachment attachment)
    {
        static AttachmentToBufferType g_AttachmentToBufferType;
        assert(attachment < MAX_ATTACHMENT_COUNT);
        return g_AttachmentToBufferType.m_AttachmentToBufferType[attachment];
    }

    HTexture GetRenderTargetAttachment(HRenderTarget render_target, RenderTargetAttachment attachment)
    {
        return GetRenderTargetTexture(render_target, GetAttachmentBufferType(attachment));
    }

    ShaderDesc::Shader* GetShaderProgram(HContext context, ShaderDesc* shader_desc)
    {
        assert(shader_desc);
        ShaderDesc::Shader* selected_shader = 0x0;

        for(uint32_t i = 0; i < shader_desc->m_Shaders.m_Count; ++i)
        {
            ShaderDesc::Shader* shader = &shader_desc->m_Shaders.m_Data[i];
            if(IsShaderLanguageSupported(context, shader->m_Language, shader_desc->m_ShaderType))
            {
                if (shader->m_VariantTextureArray)
                {
                    // Only select this variant if we don't support texture arrays natively
                    if (!IsContextFeatureSupported(context, CONTEXT_FEATURE_TEXTURE_ARRAY))
                    {
                        return shader;
                    }
                }
                else
                {
                    selected_shader = shader;
                }
            }
        }

        if (selected_shader == 0)
        {
            dmLogError("Unable to get a valid shader from a ShaderDesc for this context.");
        }

        return selected_shader;
    }

    uint32_t GetBufferTypeIndex(BufferType buffer_type)
    {
        switch(buffer_type)
        {
            case BUFFER_TYPE_COLOR0_BIT:  return 0;
            case BUFFER_TYPE_COLOR1_BIT:  return 1;
            case BUFFER_TYPE_COLOR2_BIT:  return 2;
            case BUFFER_TYPE_COLOR3_BIT:  return 3;
            case BUFFER_TYPE_DEPTH_BIT:   return 4;
            case BUFFER_TYPE_STENCIL_BIT: return 5;
            default: break;
        }
        return ~0u;
    }

    BufferType GetBufferTypeFromIndex(uint32_t index)
    {
        switch(index)
        {
            case 0: return BUFFER_TYPE_COLOR0_BIT;
            case 1: return BUFFER_TYPE_COLOR1_BIT;
            case 2: return BUFFER_TYPE_COLOR2_BIT;
            case 3: return BUFFER_TYPE_COLOR3_BIT;
            case 4: return BUFFER_TYPE_DEPTH_BIT;
            case 5: return BUFFER_TYPE_STENCIL_BIT;
            default: break;
        }
        return (BufferType) ~0u;
    }

    uint32_t GetTypeSize(dmGraphics::Type type)
    {
        if (type == dmGraphics::TYPE_BYTE || type == dmGraphics::TYPE_UNSIGNED_BYTE)
        {
            return 1;
        }
        else if (type == dmGraphics::TYPE_SHORT || type == dmGraphics::TYPE_UNSIGNED_SHORT)
        {
            return 2;
        }
        else if (type == dmGraphics::TYPE_INT || type == dmGraphics::TYPE_UNSIGNED_INT || type == dmGraphics::TYPE_FLOAT)
        {
             return 4;
        }
        else if (type == dmGraphics::TYPE_FLOAT_VEC2)
        {
            return 2 * 4;
        }
        else if (type == dmGraphics::TYPE_FLOAT_VEC3)
        {
            return 3 * 4;
        }
        else if (type == dmGraphics::TYPE_FLOAT_VEC4)
        {
            return 4 * 4;
        }
        else if (type == dmGraphics::TYPE_FLOAT_MAT2)
        {
            return 2 * 4 * 4;
        }
        else if (type == dmGraphics::TYPE_FLOAT_MAT3)
        {
            return 3 * 4 * 4;
        }
        else if (type == dmGraphics::TYPE_FLOAT_MAT4)
        {
            return 4 * 4 * 4;
        }
        return 0;
    }

    uint32_t GetShaderTypeSize(ShaderDesc::ShaderDataType type)
    {
        switch(type)
        {
            case ShaderDesc::SHADER_TYPE_INT:     return 4;
            case ShaderDesc::SHADER_TYPE_UINT:    return 4;
            case ShaderDesc::SHADER_TYPE_FLOAT:   return 4;
            case ShaderDesc::SHADER_TYPE_VEC2:    return 8;
            case ShaderDesc::SHADER_TYPE_VEC3:    return 12;
            case ShaderDesc::SHADER_TYPE_VEC4:    return 16;
            case ShaderDesc::SHADER_TYPE_MAT2:    return 16;
            case ShaderDesc::SHADER_TYPE_MAT3:    return 36;
            case ShaderDesc::SHADER_TYPE_MAT4:    return 64;
            case ShaderDesc::SHADER_TYPE_UVEC2:   return 16;
            case ShaderDesc::SHADER_TYPE_UVEC3:   return 36;
            case ShaderDesc::SHADER_TYPE_UVEC4:   return 64;
            default: break;
        }
        return 0;
    }

    dmGraphics::Type GetGraphicsType(dmGraphics::VertexAttribute::DataType data_type)
    {
        switch(data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:           return dmGraphics::TYPE_BYTE;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:  return dmGraphics::TYPE_UNSIGNED_BYTE;
            case dmGraphics::VertexAttribute::TYPE_SHORT:          return dmGraphics::TYPE_SHORT;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT: return dmGraphics::TYPE_UNSIGNED_SHORT;
            case dmGraphics::VertexAttribute::TYPE_INT:            return dmGraphics::TYPE_INT;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:   return dmGraphics::TYPE_UNSIGNED_INT;
            case dmGraphics::VertexAttribute::TYPE_FLOAT:          return dmGraphics::TYPE_FLOAT;
            default: assert(0 && "Unsupported dmGraphics::VertexAttribute::DataType");
        }
        return (dmGraphics::Type) -1;
    }

    Type ShaderDataTypeToGraphicsType(ShaderDesc::ShaderDataType shader_type)
    {
        switch(shader_type)
        {
            case ShaderDesc::SHADER_TYPE_INT:             return TYPE_INT;
            case ShaderDesc::SHADER_TYPE_UINT:            return TYPE_UNSIGNED_INT;
            case ShaderDesc::SHADER_TYPE_FLOAT:           return TYPE_FLOAT;
            case ShaderDesc::SHADER_TYPE_VEC2:            return TYPE_FLOAT_VEC2;
            case ShaderDesc::SHADER_TYPE_VEC3:            return TYPE_FLOAT_VEC3;
            case ShaderDesc::SHADER_TYPE_VEC4:            return TYPE_FLOAT_VEC4;
            case ShaderDesc::SHADER_TYPE_MAT2:            return TYPE_FLOAT_MAT2;
            case ShaderDesc::SHADER_TYPE_MAT3:            return TYPE_FLOAT_MAT3;
            case ShaderDesc::SHADER_TYPE_MAT4:            return TYPE_FLOAT_MAT4;
            case ShaderDesc::SHADER_TYPE_SAMPLER:         return TYPE_SAMPLER;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:       return TYPE_SAMPLER_2D;
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE:    return TYPE_SAMPLER_CUBE;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY: return TYPE_SAMPLER_2D_ARRAY;
            case ShaderDesc::SHADER_TYPE_IMAGE2D:         return TYPE_IMAGE_2D;
            case ShaderDesc::SHADER_TYPE_TEXTURE2D:       return TYPE_TEXTURE_2D;
            case ShaderDesc::SHADER_TYPE_TEXTURE2D_ARRAY: return TYPE_TEXTURE_2D_ARRAY;
            case ShaderDesc::SHADER_TYPE_TEXTURE_CUBE:    return TYPE_TEXTURE_CUBE;
            default: break;
        }

        // Not supported
        return (Type) 0xffffffff;
    }

    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context)
    {
        VertexStreamDeclaration* sd = new VertexStreamDeclaration();
        memset(sd, 0, sizeof(*sd));
        return sd;
    }

    HVertexStreamDeclaration NewVertexStreamDeclaration(HContext context, VertexStepFunction step_function)
    {
        VertexStreamDeclaration* sd = NewVertexStreamDeclaration(context);
        sd->m_StepFunction = step_function;
        return sd;
    }

    void AddVertexStream(HVertexStreamDeclaration stream_declaration, const char* name, uint32_t size, Type type, bool normalize)
    {
        AddVertexStream(stream_declaration, dmHashString64(name), size, type, normalize);
    }

    void AddVertexStream(HVertexStreamDeclaration stream_declaration, dmhash_t name_hash, uint32_t size, Type type, bool normalize)
    {
        if (stream_declaration->m_StreamCount >= MAX_VERTEX_STREAM_COUNT)
        {
            dmLogError("Unable to add vertex stream '%s', stream declaration has no slots left (max: %d)",
                dmHashReverseSafe64(name_hash), MAX_VERTEX_STREAM_COUNT);
            return;
        }

        uint8_t stream_index = stream_declaration->m_StreamCount;
        stream_declaration->m_Streams[stream_index].m_NameHash  = name_hash;
        stream_declaration->m_Streams[stream_index].m_Size      = size;
        stream_declaration->m_Streams[stream_index].m_Type      = type;
        stream_declaration->m_Streams[stream_index].m_Normalize = normalize;
        stream_declaration->m_Streams[stream_index].m_Stream    = stream_index;
        stream_declaration->m_StreamCount++;
    }

    void DeleteVertexStreamDeclaration(HVertexStreamDeclaration stream_declaration)
    {
        delete stream_declaration;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    void HashVertexDeclaration(HashState32* state, HVertexDeclaration vertex_declaration)
    {
        dmHashUpdateBuffer32(state, vertex_declaration->m_Streams, sizeof(VertexDeclaration::Stream) * vertex_declaration->m_StreamCount);
    }

    bool SetStreamOffset(HVertexDeclaration vertex_declaration, uint32_t stream_index, uint16_t offset)
    {
        if (stream_index >= vertex_declaration->m_StreamCount) {
            return false;
        }
        vertex_declaration->m_Streams[stream_index].m_Offset = offset;
        return true;
    }

    uint32_t GetVertexStreamOffset(HVertexDeclaration vertex_declaration, dmhash_t name_hash)
    {
        uint32_t count = vertex_declaration->m_StreamCount;
        VertexDeclaration::Stream* streams = vertex_declaration->m_Streams;
        for (int i = 0; i < count; ++i)
        {
            if (streams[i].m_NameHash == name_hash)
            {
                return streams[i].m_Offset;
            }
        }
        return dmGraphics::INVALID_STREAM_OFFSET;
    }

    uint32_t GetVertexDeclarationStride(HVertexDeclaration vertex_declaration)
    {
        return vertex_declaration ? vertex_declaration->m_Stride : 0;
    }

    uint32_t GetVertexDeclarationStreamCount(HVertexDeclaration vertex_declaration)
    {
        return vertex_declaration ? vertex_declaration->m_StreamCount : 0;
    }

    #define DM_TEXTURE_FORMAT_TO_STR_CASE(x) case TEXTURE_FORMAT_##x: return #x;
    const char* TextureFormatToString(TextureFormat format)
    {
        switch(format)
        {
            DM_TEXTURE_FORMAT_TO_STR_CASE(LUMINANCE);
            DM_TEXTURE_FORMAT_TO_STR_CASE(LUMINANCE_ALPHA);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_16BPP);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_16BPP);
            DM_TEXTURE_FORMAT_TO_STR_CASE(DEPTH);
            DM_TEXTURE_FORMAT_TO_STR_CASE(STENCIL);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_PVRTC_2BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_PVRTC_4BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_PVRTC_2BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_PVRTC_4BPPV1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_ETC1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_ETC2);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_ASTC_4x4);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB_BC1);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_BC3);
            DM_TEXTURE_FORMAT_TO_STR_CASE(R_BC4);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RG_BC5);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA_BC7);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGB32F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RGBA32F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(R16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RG16F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(R32F);
            DM_TEXTURE_FORMAT_TO_STR_CASE(RG32F);
            default:break;
        }
        return "UNKNOWN_FORMAT";
    }
    #undef DM_TEXTURE_FORMAT_TO_STR_CASE

    // For estimating resource size
    uint32_t GetTextureFormatBitsPerPixel(TextureFormat format)
    {
        switch(format)
        {
        case TEXTURE_FORMAT_LUMINANCE:          return 8;
        case TEXTURE_FORMAT_LUMINANCE_ALPHA:    return 16;
        case TEXTURE_FORMAT_RGB:                return 24;
        case TEXTURE_FORMAT_RGBA:               return 32;
        case TEXTURE_FORMAT_RGB_16BPP:          return 16;
        case TEXTURE_FORMAT_RGBA_16BPP:         return 16;
        case TEXTURE_FORMAT_RGB_ETC1:           return 4;
        case TEXTURE_FORMAT_R_ETC2:             return 8;
        case TEXTURE_FORMAT_RG_ETC2:            return 8;
        case TEXTURE_FORMAT_RGBA_ETC2:          return 8;
        case TEXTURE_FORMAT_RGBA_ASTC_4x4:      return 8;
        case TEXTURE_FORMAT_RGB_BC1:            return 4;
        case TEXTURE_FORMAT_RGBA_BC3:           return 4;
        case TEXTURE_FORMAT_R_BC4:              return 8;
        case TEXTURE_FORMAT_RG_BC5:             return 8;
        case TEXTURE_FORMAT_RGBA_BC7:           return 8;
        case TEXTURE_FORMAT_DEPTH:              return 24;
        case TEXTURE_FORMAT_STENCIL:            return 8;
        case TEXTURE_FORMAT_RGB_PVRTC_2BPPV1:   return 2;
        case TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:   return 4;
        case TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1:  return 2;
        case TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:  return 4;
        case TEXTURE_FORMAT_RGB16F:             return 48;
        case TEXTURE_FORMAT_RGB32F:             return 96;
        case TEXTURE_FORMAT_RGBA16F:            return 64;
        case TEXTURE_FORMAT_RGBA32F:            return 128;
        case TEXTURE_FORMAT_R16F:               return 16;
        case TEXTURE_FORMAT_RG16F:              return 32;
        case TEXTURE_FORMAT_R32F:               return 32;
        case TEXTURE_FORMAT_RG32F:              return 64;
        case TEXTURE_FORMAT_RGBA32UI:           return 128;
        case TEXTURE_FORMAT_BGRA8U:             return 32;
        case TEXTURE_FORMAT_R32UI:              return 32;
        default:
            assert(false && "Unknown texture format");
            return TEXTURE_FORMAT_COUNT;
        }
    }

    uint32_t GetGraphicsTypeDataSize(Type type)
    {
        if (type == TYPE_BYTE || type == TYPE_UNSIGNED_BYTE)
        {
            return 1;
        }
        else if (type == TYPE_SHORT || type == TYPE_UNSIGNED_SHORT)
        {
            return 2;
        }
        else if (type == TYPE_INT || type == TYPE_UNSIGNED_INT || type == TYPE_FLOAT)
        {
            return 4;
        }
        else if (type == TYPE_FLOAT_VEC4)
        {
            return 16;
        }
        else if (type == TYPE_FLOAT_MAT4)
        {
            return 64;
        }
        assert(0 && "Unsupported data type");
        return 0;
    }

    Type GetGraphicsTypeFromShaderDataType(ShaderDesc::ShaderDataType shader_type)
    {
        switch(shader_type)
        {
            case ShaderDesc::SHADER_TYPE_INT:             return TYPE_INT;
            case ShaderDesc::SHADER_TYPE_UINT:            return TYPE_UNSIGNED_INT;
            case ShaderDesc::SHADER_TYPE_FLOAT:           return TYPE_FLOAT;
            case ShaderDesc::SHADER_TYPE_VEC2:            return TYPE_FLOAT_VEC2;
            case ShaderDesc::SHADER_TYPE_VEC3:            return TYPE_FLOAT_VEC3;
            case ShaderDesc::SHADER_TYPE_VEC4:            return TYPE_FLOAT_VEC4;
            case ShaderDesc::SHADER_TYPE_MAT2:            return TYPE_FLOAT_MAT2;
            case ShaderDesc::SHADER_TYPE_MAT3:            return TYPE_FLOAT_MAT3;
            case ShaderDesc::SHADER_TYPE_MAT4:            return TYPE_FLOAT_MAT4;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D:       return TYPE_SAMPLER_2D;
            case ShaderDesc::SHADER_TYPE_SAMPLER_CUBE:    return TYPE_SAMPLER_CUBE;
            case ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY: return TYPE_SAMPLER_2D_ARRAY;
            default: break;
        }

        // Not supported
        return (Type) 0xffffffff;
    }

    bool IsUniformTextureSampler(ShaderDesc::ShaderDataType uniform_type)
    {
        return uniform_type == ShaderDesc::SHADER_TYPE_SAMPLER2D       ||
               uniform_type == ShaderDesc::SHADER_TYPE_SAMPLER2D_ARRAY ||
               uniform_type == ShaderDesc::SHADER_TYPE_SAMPLER3D       ||
               uniform_type == ShaderDesc::SHADER_TYPE_SAMPLER_CUBE;
    }

    bool IsUniformStorageBuffer(ShaderDesc::ShaderDataType uniform_type)
    {
        return uniform_type == ShaderDesc::SHADER_TYPE_STORAGE_BUFFER;
    }

    bool IsTextureFormatCompressed(dmGraphics::TextureFormat format)
    {
        switch(format)
        {
            case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            case dmGraphics::TEXTURE_FORMAT_RGB_ETC1:
            case dmGraphics::TEXTURE_FORMAT_RGBA_ETC2:
            case dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4x4:
            case dmGraphics::TEXTURE_FORMAT_RGB_BC1:
            case dmGraphics::TEXTURE_FORMAT_RGBA_BC3:
            case dmGraphics::TEXTURE_FORMAT_R_BC4:
            case dmGraphics::TEXTURE_FORMAT_RG_BC5:
            case dmGraphics::TEXTURE_FORMAT_RGBA_BC7:
                return true;
            default:
                return false;
        }
    }

    static bool IsFormatRGBA(dmGraphics::TextureFormat format)
    {
        switch(format)
        {
            case dmGraphics::TEXTURE_FORMAT_RGBA:
            case dmGraphics::TEXTURE_FORMAT_RGBA32UI:
            case dmGraphics::TEXTURE_FORMAT_RGBA_BC7:
            case dmGraphics::TEXTURE_FORMAT_RGBA_BC3:
            case dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4x4:
            case dmGraphics::TEXTURE_FORMAT_RGBA_ETC2:
            case dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1:
            case dmGraphics::TEXTURE_FORMAT_RGBA_16BPP:
                return true;
            default:
                return false;
        }
    }

    static bool IsFormatRGB(dmGraphics::TextureFormat format)
    {
        switch(format)
        {
            case dmGraphics::TEXTURE_FORMAT_RGB:
            case dmGraphics::TEXTURE_FORMAT_RGB_BC1:
            case dmGraphics::TEXTURE_FORMAT_RGB_ETC1:
            case dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1:
            case dmGraphics::TEXTURE_FORMAT_RGB_16BPP:
                return true;
            default:
                return false;
        }
    }

    static bool IsFormatRG(dmGraphics::TextureFormat format)
    {
        switch(format)
        {
            case dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA:
            case dmGraphics::TEXTURE_FORMAT_RG_BC5:
                return true;
            default:
                return false;
        }
    }

    static bool IsFormatR(dmGraphics::TextureFormat format)
    {
        switch(format)
        {
            case dmGraphics::TEXTURE_FORMAT_LUMINANCE:
            case dmGraphics::TEXTURE_FORMAT_R_BC4:
                return true;
            default:
                return false;
        }
    }

    uint16_t GetMipmapSize(uint16_t size_0, uint8_t mipmap)
    {
        for (uint32_t i = 0; i < mipmap; ++i)
        {
            size_0 /= 2;
        }
        return size_0;
    }

    uint8_t GetMipmapCount(uint16_t size)
    {
        return (uint8_t) floor(log2f(size)) + 1;
    }

    PipelineState GetDefaultPipelineState()
    {
        PipelineState ps;
        ps.m_WriteColorMask           = DM_GRAPHICS_STATE_WRITE_R | DM_GRAPHICS_STATE_WRITE_G | DM_GRAPHICS_STATE_WRITE_B | DM_GRAPHICS_STATE_WRITE_A;
        ps.m_WriteDepth               = 1;
        ps.m_PrimtiveType             = PRIMITIVE_TRIANGLES;
        ps.m_DepthTestEnabled         = 1;
        ps.m_DepthTestFunc            = COMPARE_FUNC_LEQUAL;
        ps.m_BlendEnabled             = 0;
        ps.m_BlendSrcFactor           = BLEND_FACTOR_ZERO;
        ps.m_BlendDstFactor           = BLEND_FACTOR_ZERO;
        ps.m_StencilEnabled           = 0;
        ps.m_StencilFrontOpFail       = STENCIL_OP_KEEP;
        ps.m_StencilFrontOpDepthFail  = STENCIL_OP_KEEP;
        ps.m_StencilFrontOpPass       = STENCIL_OP_KEEP;
        ps.m_StencilFrontTestFunc     = COMPARE_FUNC_ALWAYS;
        ps.m_StencilBackOpFail        = STENCIL_OP_KEEP;
        ps.m_StencilBackOpDepthFail   = STENCIL_OP_KEEP;
        ps.m_StencilBackOpPass        = STENCIL_OP_KEEP;
        ps.m_StencilBackTestFunc      = COMPARE_FUNC_ALWAYS;
        ps.m_StencilWriteMask         = 0xff;
        ps.m_StencilCompareMask       = 0xff;
        ps.m_StencilReference         = 0x0;
        ps.m_CullFaceEnabled          = 0;
        ps.m_CullFaceType             = FACE_TYPE_BACK;
        ps.m_PolygonOffsetFillEnabled = 0;
        return ps;
    }

    // The goal is to find a supported compression format, since they're smaller than the uncompressed ones
    // The user can also choose RGB(a) 16BPP as the fallback if they wish to have smaller size than full RGB(a)
    dmGraphics::TextureFormat GetSupportedCompressionFormat(dmGraphics::HContext context, dmGraphics::TextureFormat format, uint32_t width, uint32_t height)
    {
        #define TEST_AND_RETURN(_TYPEN_ENUM) if (dmGraphics::IsTextureFormatSupported(context, (_TYPEN_ENUM))) return (_TYPEN_ENUM);

        if (IsFormatRGBA(format))
        {
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGBA_BC7);
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGBA_ASTC_4x4);
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGBA_ETC2);
            if (width == height) {
                TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1);
            }
            TEST_AND_RETURN(format);
            return dmGraphics::TEXTURE_FORMAT_RGBA;
        }

        if (IsFormatRGB(format))
        {
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGB_BC1);
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGB_ETC1);
            if (width == height) {
                TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RGB_PVRTC_4BPPV1);
            }
            TEST_AND_RETURN(format);
            return dmGraphics::TEXTURE_FORMAT_RGB;
        }

        if (IsFormatRG(format))
        {
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RG_BC5);
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_RG_ETC2);
            TEST_AND_RETURN(format);
            return dmGraphics::TEXTURE_FORMAT_LUMINANCE_ALPHA;
        }

        if (IsFormatR(format))
        {
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_R_BC4);
            TEST_AND_RETURN(dmGraphics::TEXTURE_FORMAT_R_ETC2);
            TEST_AND_RETURN(format);
            return dmGraphics::TEXTURE_FORMAT_LUMINANCE;
        }

        #undef TEST_AND_RETURN
        return format;
    }

    void SetPipelineStateValue(dmGraphics::PipelineState& pipeline_state, State state, uint8_t value)
    {
        switch(state)
        {
            case STATE_DEPTH_TEST:
                pipeline_state.m_DepthTestEnabled = value;
            break;
            case STATE_STENCIL_TEST:
                pipeline_state.m_StencilEnabled = value;
            break;
            case STATE_BLEND:
                pipeline_state.m_BlendEnabled = value;
            break;
            case STATE_CULL_FACE:
                pipeline_state.m_CullFaceEnabled = value;
            break;
            case STATE_POLYGON_OFFSET_FILL:
                pipeline_state.m_PolygonOffsetFillEnabled = value;
            break;
            default:
                assert(0 && "EnableState: State not supported");
            break;
        }
    }

    void RepackRGBToRGBA(uint32_t num_pixels, uint8_t* rgb, uint8_t* rgba)
    {
        for(uint32_t px=0; px < num_pixels; px++)
        {
            rgba[0] = rgb[0];
            rgba[1] = rgb[1];
            rgba[2] = rgb[2];
            rgba[3] = 255;
            rgba+=4;
            rgb+=3;
        }
    }

    static void PutShaderResourceBindings(const ShaderDesc::ResourceBinding* bindings, uint32_t bindings_count, dmArray<ShaderResourceBinding>& bindings_out, ShaderResourceBinding::BindingFamily family)
    {
        bindings_out.SetCapacity(bindings_count);
        bindings_out.SetSize(bindings_count);

        for (int i = 0; i < bindings_count; ++i)
        {
            ShaderResourceBinding& res = bindings_out[i];
            res.m_Name                 = strdup(bindings[i].m_Name);
            res.m_NameHash             = bindings[i].m_NameHash;
            res.m_Binding              = bindings[i].m_Binding;
            res.m_Set                  = bindings[i].m_Set;
            res.m_Type.m_UseTypeIndex  = bindings[i].m_Type.m_UseTypeIndex;
            res.m_BindingFamily        = family;

            if (res.m_Type.m_ShaderType == ShaderDesc::SHADER_TYPE_SAMPLER)
                res.m_BindingInfo.m_SamplerTextureIndex = bindings[i].m_Bindinginfo.m_SamplerTextureIndex;
            else
                res.m_BindingInfo.m_BlockSize = bindings[i].m_Bindinginfo.m_BlockSize;

            if (res.m_Type.m_UseTypeIndex)
                res.m_Type.m_TypeIndex = bindings[i].m_Type.m_Type.m_TypeIndex;
            else
                res.m_Type.m_ShaderType = bindings[i].m_Type.m_Type.m_ShaderType;
        }
    }

    void CreateShaderMeta(ShaderDesc::ShaderReflection* ddf, ShaderMeta* meta)
    {
        PutShaderResourceBindings(ddf->m_UniformBuffers.m_Data, ddf->m_UniformBuffers.m_Count, meta->m_UniformBuffers, ShaderResourceBinding::BINDING_FAMILY_UNIFORM_BUFFER);
        PutShaderResourceBindings(ddf->m_StorageBuffers.m_Data, ddf->m_StorageBuffers.m_Count, meta->m_StorageBuffers, ShaderResourceBinding::BINDING_FAMILY_STORAGE_BUFFER);
        PutShaderResourceBindings(ddf->m_Textures.m_Data, ddf->m_Textures.m_Count, meta->m_Textures, ShaderResourceBinding::BINDING_FAMILY_TEXTURE);
        PutShaderResourceBindings(ddf->m_Inputs.m_Data, ddf->m_Inputs.m_Count, meta->m_Inputs, ShaderResourceBinding::BINDING_FAMILY_GENERIC);

        meta->m_TypeInfos.SetCapacity(ddf->m_Types.m_Count);
        meta->m_TypeInfos.SetSize(ddf->m_Types.m_Count);

        memset(meta->m_TypeInfos.Begin(), 0, ddf->m_Types.m_Count * sizeof(meta->m_TypeInfos[0]));

        for (int i = 0; i < ddf->m_Types.m_Count; ++i)
        {
            ShaderResourceTypeInfo& info = meta->m_TypeInfos[i];
            info.m_Name     = strdup(ddf->m_Types[i].m_Name);
            info.m_NameHash = ddf->m_Types[i].m_NameHash;
            info.m_Members.SetCapacity(ddf->m_Types[i].m_Members.m_Count);
            info.m_Members.SetSize(ddf->m_Types[i].m_Members.m_Count);

            for (int j = 0; j < ddf->m_Types[i].m_Members.m_Count; ++j)
            {
                ShaderResourceMember& member = info.m_Members[j];
                member.m_Name                = strdup(ddf->m_Types[i].m_Members[j].m_Name);
                member.m_NameHash            = ddf->m_Types[i].m_Members[j].m_NameHash;
                member.m_ElementCount        = ddf->m_Types[i].m_Members[j].m_ElementCount;
                member.m_Offset              = ddf->m_Types[i].m_Members[j].m_Offset;
                member.m_Type.m_UseTypeIndex = ddf->m_Types[i].m_Members[j].m_Type.m_UseTypeIndex;

                if (member.m_Type.m_UseTypeIndex)
                    member.m_Type.m_TypeIndex = ddf->m_Types[i].m_Members[j].m_Type.m_Type.m_TypeIndex;
                else
                    member.m_Type.m_ShaderType = ddf->m_Types[i].m_Members[j].m_Type.m_Type.m_ShaderType;
            }
        }
    }

    static void FreeShaderResourceBindings(dmArray<ShaderResourceBinding>& bindings)
    {
        for (int i = 0; i < bindings.Size(); ++i)
        {
            free(bindings[i].m_Name);
        }
    }

    void DestroyShaderMeta(ShaderMeta& meta)
    {
        FreeShaderResourceBindings(meta.m_UniformBuffers);
        FreeShaderResourceBindings(meta.m_StorageBuffers);
        FreeShaderResourceBindings(meta.m_Textures);
        FreeShaderResourceBindings(meta.m_Inputs);

        for (int i = 0; i < meta.m_TypeInfos.Size(); ++i)
        {
            free(meta.m_TypeInfos[i].m_Name);
            for (int j = 0; j < meta.m_TypeInfos[i].m_Members.Size(); ++j)
            {
                free(meta.m_TypeInfos[i].m_Members[j].m_Name);
            }
        }
    }

    // TODO, comment from the PR (#4544):
    //   "These frequent lookups could be improved by sorting on the key beforehand,
    //   and during lookup, do a lower_bound, to find the item (or not).
    //   E.g see: engine/render/src/render/material.cpp#L446"
    bool GetUniformIndices(const dmArray<ShaderResourceTypeInfo> types, const dmArray<ShaderResourceBinding>& bindings, dmhash_t name_hash, uint64_t* index_out, uint64_t* index_member_out)
    {
        // JG: WHERE IS THIS FUNCTION USED
        assert(bindings.Size() < UNIFORM_LOCATION_MAX);
        for (uint32_t i = 0; i < bindings.Size(); ++i)
        {
            if (bindings[i].m_NameHash == name_hash)
            {
                *index_out = i;
                *index_member_out = 0;
                return true;
            }
            else if (bindings[i].m_Type.m_UseTypeIndex)
            {
                const ShaderResourceTypeInfo& type_info = types[bindings[i].m_Type.m_TypeIndex];
                const uint32_t num_members = type_info.m_Members.Size();
                for (int j = 0; j < num_members; ++j)
                {
                    if (type_info.m_NameHash == name_hash)
                    {
                        *index_out = i;
                        *index_member_out = j;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void InitializeSetTextureAsyncState(SetTextureAsyncState& state)
    {
        state.m_Mutex = dmMutex::New();
    }

    void ResetSetTextureAsyncState(SetTextureAsyncState& state)
    {
        if(state.m_Mutex)
        {
            dmMutex::Delete(state.m_Mutex);
        }
    }

    uint16_t PushSetTextureAsyncState(SetTextureAsyncState& state, HTexture texture, TextureParams params, SetTextureAsyncCallback callback, void* user_data)
    {
        DM_MUTEX_SCOPED_LOCK(state.m_Mutex);
        if (state.m_Indices.Remaining() == 0)
        {
            state.m_Indices.SetCapacity(state.m_Indices.Capacity()+64);
            state.m_Params.SetCapacity(state.m_Indices.Capacity());
            state.m_Params.SetSize(state.m_Params.Capacity());
        }
        uint16_t param_array_index = state.m_Indices.Pop();
        SetTextureAsyncParams& ap  = state.m_Params[param_array_index];
        ap.m_Texture               = texture;
        ap.m_Params                = params;
        ap.m_Callback              = callback;
        ap.m_UserData              = user_data;
        return param_array_index;
    }

    void PushSetTextureAsyncDeleteTexture(SetTextureAsyncState& state, HTexture texture)
    {
        DM_MUTEX_SCOPED_LOCK(state.m_Mutex);
        if (state.m_PostDeleteTextures.Full())
        {
            state.m_PostDeleteTextures.OffsetCapacity(64);
        }
        state.m_PostDeleteTextures.Push(texture);
    }

    SetTextureAsyncParams GetSetTextureAsyncParams(SetTextureAsyncState& state, uint16_t index)
    {
        DM_MUTEX_SCOPED_LOCK(state.m_Mutex);
        return state.m_Params[index];
    }

    void ReturnSetTextureAsyncIndex(SetTextureAsyncState& state, uint16_t index)
    {
        DM_MUTEX_SCOPED_LOCK(state.m_Mutex);
        state.m_Indices.Push(index);
    }

    void DeleteContext(HContext context)
    {
        g_functions.m_DeleteContext(context);
    }

    bool InstallAdapter(AdapterFamily family)
    {
        if (g_adapter)
        {
            return true;
        }

        bool result = SelectAdapterByFamily(family);

        if (!result)
        {
            result = SelectAdapterByPriority();
        }

        if (result)
        {
            dmLogInfo("Installed graphics device '%s'", GetAdapterFamilyLiteral(g_adapter->m_Family));
            return true;
        }

        dmLogError("Could not install a graphics adapter. No compatible adapter was found.");
        return false;
    }

    AdapterFamily GetInstalledAdapterFamily()
    {
        if (g_adapter)
        {
            return g_adapter->m_Family;
        }
        return ADAPTER_FAMILY_NONE;
    }

    void Finalize()
    {
        if (g_functions.m_Finalize)
            g_functions.m_Finalize();
    }

    ///////////////////////////////////////////////////
    ////// PLATFORM / WINDOWS SPECIFIC FUNCTIONS //////

    dmPlatform::HWindow GetWindow(HContext context)
    {
        return g_functions.m_GetWindow(context);
    }
    uint32_t GetWindowRefreshRate(HContext context)
    {
        return dmPlatform::GetWindowStateParam(g_functions.m_GetWindow(context), dmPlatform::WINDOW_STATE_REFRESH_RATE);
    }
    uint32_t GetWindowStateParam(HContext context, dmPlatform::WindowState state)
    {
        return dmPlatform::GetWindowStateParam(g_functions.m_GetWindow(context), state);
    }
    uint32_t GetWindowWidth(HContext context)
    {
        return dmPlatform::GetWindowWidth(g_functions.m_GetWindow(context));
    }
    uint32_t GetWindowHeight(HContext context)
    {
        return dmPlatform::GetWindowHeight(g_functions.m_GetWindow(context));
    }
    float GetDisplayScaleFactor(HContext context)
    {
        return dmPlatform::GetDisplayScaleFactor(g_functions.m_GetWindow(context));
    }
    void IconifyWindow(HContext context)
    {
        dmPlatform::IconifyWindow(g_functions.m_GetWindow(context));
    }
    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {
        dmPlatform::SetSwapInterval(g_functions.m_GetWindow(context), swap_interval);
    }
    ///////////////////////////////////////////////////
    void CloseWindow(HContext context)
    {
        g_functions.m_CloseWindow(context);
    }
    uint32_t GetDisplayDpi(HContext context)
    {
        return g_functions.m_GetDisplayDpi(context);
    }
    uint32_t GetWidth(HContext context)
    {
        return g_functions.m_GetWidth(context);
    }
    uint32_t GetHeight(HContext context)
    {
        return g_functions.m_GetHeight(context);
    }
    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        g_functions.m_SetWindowSize(context, width, height);
    }
    void ResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        g_functions.m_ResizeWindow(context, width, height);
    }
    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        g_functions.m_GetDefaultTextureFilters(context, out_min_filter, out_mag_filter);
    }
    void BeginFrame(HContext context)
    {
        g_functions.m_BeginFrame(context);
    }
    void Flip(HContext context)
    {
        g_functions.m_Flip(context);
    }
    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        g_functions.m_Clear(context, flags, red, green, blue, alpha, depth, stencil);
    }
    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return g_functions.m_NewVertexBuffer(context, size, data, buffer_usage);
    }
    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        g_functions.m_DeleteVertexBuffer(buffer);
    }
    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        g_functions.m_SetVertexBufferData(buffer, size, data, buffer_usage);
    }
    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        g_functions.m_SetVertexBufferSubData(buffer, offset, size, data);
    }
    uint32_t GetVertexBufferSize(HVertexBuffer buffer)
    {
        return g_functions.m_GetVertexBufferSize(buffer);
    }
    uint32_t GetMaxElementsVertices(HContext context)
    {
        return g_functions.m_GetMaxElementsVertices(context);
    }
    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return g_functions.m_NewIndexBuffer(context, size, data, buffer_usage);
    }
    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        g_functions.m_DeleteIndexBuffer(buffer);
    }
    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        g_functions.m_SetIndexBufferData(buffer, size, data, buffer_usage);
    }
    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        g_functions.m_SetIndexBufferSubData(buffer, offset, size, data);
    }
    uint32_t GetIndexBufferSize(HIndexBuffer buffer)
    {
        return g_functions.m_GetIndexBufferSize(buffer);
    }
    bool IsIndexBufferFormatSupported(HContext context, IndexBufferFormat format)
    {
        return g_functions.m_IsIndexBufferFormatSupported(context, format);
    }
    uint32_t GetMaxElementsIndices(HContext context)
    {
        return g_functions.m_GetMaxElementsIndices(context);
    }
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration)
    {
        return g_functions.m_NewVertexDeclaration(context, stream_declaration);
    }
    HVertexDeclaration NewVertexDeclaration(HContext context, HVertexStreamDeclaration stream_declaration, uint32_t stride)
    {
        return g_functions.m_NewVertexDeclarationStride(context, stream_declaration, stride);
    }
    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, uint32_t binding_index, uint32_t base_offset, HProgram program)
    {
        g_functions.m_EnableVertexDeclaration(context, vertex_declaration, binding_index, base_offset, program);
    }
    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        g_functions.m_DisableVertexDeclaration(context, vertex_declaration);
    }
    void EnableVertexBuffer(HContext context, HVertexBuffer vertex_buffer, uint32_t binding_index)
    {
        return g_functions.m_EnableVertexBuffer(context, vertex_buffer, binding_index);
    }
    void DisableVertexBuffer(HContext context, HVertexBuffer vertex_buffer)
    {
        g_functions.m_DisableVertexBuffer(context, vertex_buffer);
    }
    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer, uint32_t instance_count)
    {
        g_functions.m_DrawElements(context, prim_type, first, count, type, index_buffer, instance_count);
    }
    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, uint32_t instance_count)
    {
        g_functions.m_Draw(context, prim_type, first, count, instance_count);
    }
    void DispatchCompute(HContext context, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
    {
        g_functions.m_DispatchCompute(context, group_count_x, group_count_y, group_count_z);
    }
    HVertexProgram NewVertexProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        assert(ddf->m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX);
        return g_functions.m_NewVertexProgram(context, ddf, error_buffer, error_buffer_size);
    }
    HFragmentProgram NewFragmentProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        assert(ddf->m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT);
        return g_functions.m_NewFragmentProgram(context, ddf, error_buffer, error_buffer_size);
    }
    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        return g_functions.m_NewProgram(context, vertex_program, fragment_program);
    }
    void DeleteProgram(HContext context, HProgram program)
    {
        g_functions.m_DeleteProgram(context, program);
    }
    bool ReloadVertexProgram(HVertexProgram prog, ShaderDesc* ddf)
    {
        assert(ddf->m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX);
        return g_functions.m_ReloadVertexProgram(prog, ddf);
    }
    bool ReloadFragmentProgram(HFragmentProgram prog, ShaderDesc* ddf)
    {
        assert(ddf->m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT);
        return g_functions.m_ReloadFragmentProgram(prog, ddf);
    }
    void DeleteVertexProgram(HVertexProgram prog)
    {
        g_functions.m_DeleteVertexProgram(prog);
    }
    void DeleteFragmentProgram(HFragmentProgram prog)
    {
        g_functions.m_DeleteFragmentProgram(prog);
    }
    ShaderDesc::Language GetProgramLanguage(HProgram program)
    {
        return g_functions.m_GetProgramLanguage(program);
    }
    bool IsShaderLanguageSupported(HContext context, ShaderDesc::Language language, ShaderDesc::ShaderType shader_type)
    {
        return g_functions.m_IsShaderLanguageSupported(context, language, shader_type);
    }
    void EnableProgram(HContext context, HProgram program)
    {
        g_functions.m_EnableProgram(context, program);
    }
    void DisableProgram(HContext context)
    {
        g_functions.m_DisableProgram(context);
    }
    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        return g_functions.m_ReloadProgramGraphics(context, program, vert_program, frag_program);
    }
    bool ReloadProgram(HContext context, HProgram program, HComputeProgram compute_program)
    {
        return g_functions.m_ReloadProgramCompute(context, program, compute_program);
    }
    bool ReloadComputeProgram(HComputeProgram prog, ShaderDesc* ddf)
    {
        assert(ddf->m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_COMPUTE);
        return g_functions.m_ReloadComputeProgram(prog, ddf);
    }
    uint32_t GetAttributeCount(HProgram prog)
    {
        return g_functions.m_GetAttributeCount(prog);
    }
    void GetAttribute(HProgram prog, uint32_t index, dmhash_t* name_hash, Type* type, uint32_t* element_count, uint32_t* num_values, int32_t* location)
    {
        return g_functions.m_GetAttribute(prog, index, name_hash, type, element_count, num_values, location);
    }
    uint32_t GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type, int32_t* size)
    {
        return g_functions.m_GetUniformName(prog, index, buffer, buffer_size, type, size);
    }
    uint32_t GetUniformCount(HProgram prog)
    {
        return g_functions.m_GetUniformCount(prog);
    }
    HUniformLocation GetUniformLocation(HProgram prog, const char* name)
    {
        return g_functions.m_GetUniformLocation(prog, name);
    }
    void SetConstantV4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        g_functions.m_SetConstantV4(context, data, count, base_location);
    }
    void SetConstantM4(HContext context, const dmVMath::Vector4* data, int count, HUniformLocation base_location)
    {
        g_functions.m_SetConstantM4(context, data, count, base_location);
    }
    void SetSampler(HContext context, HUniformLocation location, int32_t unit)
    {
        g_functions.m_SetSampler(context, location, unit);
    }
    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        g_functions.m_SetViewport(context, x, y, width, height);
    }
    void EnableState(HContext context, State state)
    {
        g_functions.m_EnableState(context, state);
    }
    void DisableState(HContext context, State state)
    {
        g_functions.m_DisableState(context, state);
    }
    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        g_functions.m_SetBlendFunc(context, source_factor, destinaton_factor);
    }
    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        g_functions.m_SetColorMask(context, red, green, blue, alpha);
    }
    void SetDepthMask(HContext context, bool mask)
    {
        g_functions.m_SetDepthMask(context, mask);
    }
    void SetDepthFunc(HContext context, CompareFunc func)
    {
        g_functions.m_SetDepthFunc(context, func);
    }
    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        g_functions.m_SetScissor(context, x, y, width, height);
    }
    void SetStencilMask(HContext context, uint32_t mask)
    {
        g_functions.m_SetStencilMask(context, mask);
    }
    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        g_functions.m_SetStencilFunc(context, func, ref, mask);
    }
    void SetStencilFuncSeparate(HContext context, FaceType face_type, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        g_functions.m_SetStencilFuncSeparate(context, face_type, func, ref, mask);
    }
    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        g_functions.m_SetStencilOp(context, sfail, dpfail, dppass);
    }
    void SetStencilOpSeparate(HContext context, FaceType face_type, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        g_functions.m_SetStencilOpSeparate(context, face_type, sfail, dpfail, dppass);
    }
    void SetCullFace(HContext context, FaceType face_type)
    {
        g_functions.m_SetCullFace(context, face_type);
    }
    void SetFaceWinding(HContext context, FaceWinding face_winding)
    {
        g_functions.m_SetFaceWinding(context, face_winding);
    }
    void SetPolygonOffset(HContext context, float factor, float units)
    {
        g_functions.m_SetPolygonOffset(context, factor, units);
    }
    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const RenderTargetCreationParams params)
    {
        return g_functions.m_NewRenderTarget(context, buffer_type_flags, params);
    }
    void DeleteRenderTarget(HRenderTarget render_target)
    {
        g_functions.m_DeleteRenderTarget(render_target);
    }
    void SetRenderTarget(HContext context, HRenderTarget render_target, uint32_t transient_buffer_types)
    {
        g_functions.m_SetRenderTarget(context, render_target, transient_buffer_types);
    }
    HTexture GetRenderTargetTexture(HRenderTarget render_target, BufferType buffer_type)
    {
        return g_functions.m_GetRenderTargetTexture(render_target, buffer_type);
    }
    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        g_functions.m_GetRenderTargetSize(render_target, buffer_type, width, height);
    }
    void SetRenderTargetSize(HRenderTarget render_target, uint32_t width, uint32_t height)
    {
        g_functions.m_SetRenderTargetSize(render_target, width, height);
    }
    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return g_functions.m_IsTextureFormatSupported(context, format);
    }
    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        return g_functions.m_NewTexture(context, params);
    }
    void DeleteTexture(HTexture t)
    {
        g_functions.m_DeleteTexture(t);
    }
    void SetTexture(HTexture texture, const TextureParams& params)
    {
        g_functions.m_SetTexture(texture, params);
    }
    void SetTextureAsync(HTexture texture, const TextureParams& params, SetTextureAsyncCallback callback, void* user_data)
    {
        g_functions.m_SetTextureAsync(texture, params, callback, user_data);
    }
    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap, float max_anisotropy)
    {
        g_functions.m_SetTextureParams(texture, minfilter, magfilter, uwrap, vwrap, max_anisotropy);
    }
    uint32_t GetTextureResourceSize(HTexture texture)
    {
        return g_functions.m_GetTextureResourceSize(texture);
    }
    uint16_t GetTextureWidth(HTexture texture)
    {
        return g_functions.m_GetTextureWidth(texture);
    }
    uint16_t GetTextureHeight(HTexture texture)
    {
        return g_functions.m_GetTextureHeight(texture);
    }
    uint16_t GetTextureDepth(HTexture texture)
    {
        return g_functions.m_GetTextureDepth(texture);
    }
    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return g_functions.m_GetOriginalTextureWidth(texture);
    }
    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return g_functions.m_GetOriginalTextureHeight(texture);
    }
    uint8_t GetTextureMipmapCount(HTexture texture)
    {
        return g_functions.m_GetTextureMipmapCount(texture);
    }
    TextureType GetTextureType(HTexture texture)
    {
        return g_functions.m_GetTextureType(texture);
    }
    void EnableTexture(HContext context, uint32_t unit, uint8_t id_index, HTexture texture)
    {
        g_functions.m_EnableTexture(context, unit, id_index, texture);
    }
    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        g_functions.m_DisableTexture(context, unit, texture);
    }
    uint32_t GetMaxTextureSize(HContext context)
    {
        return g_functions.m_GetMaxTextureSize(context);
    }
    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return g_functions.m_GetTextureStatusFlags(texture);
    }
    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        g_functions.m_ReadPixels(context, buffer, buffer_size);
    }
    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        g_functions.m_RunApplicationLoop(user_data, step_method, is_running);
    }
    HandleResult GetTextureHandle(HTexture texture, void** out_handle)
    {
        return g_functions.m_GetTextureHandle(texture, out_handle);
    }
    bool IsExtensionSupported(HContext context, const char* extension)
    {
        return g_functions.m_IsExtensionSupported(context, extension);
    }
    uint32_t GetNumSupportedExtensions(HContext context)
    {
        return g_functions.m_GetNumSupportedExtensions(context);
    }
    const char* GetSupportedExtension(HContext context, uint32_t index)
    {
        return g_functions.m_GetSupportedExtension(context, index);
    }
    bool IsContextFeatureSupported(HContext context, ContextFeature feature)
    {
        if (CONTEXT_FEATURE_VSYNC == feature)
        {
            AdapterFamily family = GetInstalledAdapterFamily();
            return !(family == ADAPTER_FAMILY_NULL || family == ADAPTER_FAMILY_NONE);
        }
        return g_functions.m_IsContextFeatureSupported(context, feature);
    }
    PipelineState GetPipelineState(HContext context)
    {
        return g_functions.m_GetPipelineState(context);
    }
    uint8_t GetNumTextureHandles(HTexture texture)
    {
        return g_functions.m_GetNumTextureHandles(texture);
    }
    uint32_t GetTextureUsageHintFlags(HTexture texture)
    {
        return g_functions.m_GetTextureUsageHintFlags(texture);
    }
    bool IsAssetHandleValid(HContext context, HAssetHandle asset_handle)
    {
        assert(asset_handle <= MAX_ASSET_HANDLE_VALUE);
        return g_functions.m_IsAssetHandleValid(context, asset_handle);
    }
    HComputeProgram NewComputeProgram(HContext context, ShaderDesc* ddf, char* error_buffer, uint32_t error_buffer_size)
    {
        assert(ddf->m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_COMPUTE);
        return g_functions.m_NewComputeProgram(context, ddf, error_buffer, error_buffer_size);
    }
    HProgram NewProgram(HContext context, HComputeProgram compute_program)
    {
        return g_functions.m_NewProgramFromCompute(context, compute_program);
    }
    void DeleteComputeProgram(HComputeProgram prog)
    {
        return g_functions.m_DeleteComputeProgram(prog);
    }

#if defined(DM_PLATFORM_IOS)
    void AppBootstrap(int argc, char** argv, void* init_ctx, EngineInit init_fn, EngineExit exit_fn, EngineCreate create_fn, EngineDestroy destroy_fn, EngineUpdate update_fn, EngineGetResult result_fn)
    {
        glfwAppBootstrap(argc, argv, init_ctx, init_fn, exit_fn, create_fn, destroy_fn, update_fn, result_fn);
    }
#endif
}
