#include "graphics.h"
#include "graphics_adapter.h"
#include <string.h>
#include <assert.h>

namespace dmGraphics
{
    GraphicsAdapter* g_adapter_list = 0;
    static GraphicsAdapterFunctionTable g_functions;

    GraphicsAdapter::GraphicsAdapter(GraphicsAdapterIsSupportedCb is_supported_cb, GraphicsAdapterRegisterFunctionsCb register_functions_cb, int priority)
    : m_Next(g_adapter_list)
    , m_RegisterCb(register_functions_cb)
    , m_IsSupportedCb(is_supported_cb)
    , m_Priority(priority)
    {
        g_adapter_list = this;
    }

    WindowParams::WindowParams()
    : m_ResizeCallback(0x0)
    , m_ResizeCallbackUserData(0x0)
    , m_CloseCallback(0x0)
    , m_CloseCallbackUserData(0x0)
    , m_Width(640)
    , m_Height(480)
    , m_Samples(1)
    , m_Title("Dynamo App")
    , m_Fullscreen(false)
    , m_PrintDeviceInfo(false)
    , m_HighDPI(false)
    {

    }

    ContextParams::ContextParams()
    : m_DefaultTextureMinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
    , m_DefaultTextureMagFilter(TEXTURE_FILTER_LINEAR)
    , m_VerifyGraphicsCalls(false)
    {

    }

    TextureFormatToBPP::TextureFormatToBPP()
    {
        memset(m_FormatToBPP, 0x0, sizeof(m_FormatToBPP));
        m_FormatToBPP[TEXTURE_FORMAT_LUMINANCE]          = 8;
        m_FormatToBPP[TEXTURE_FORMAT_LUMINANCE_ALPHA]    = 16;
        m_FormatToBPP[TEXTURE_FORMAT_RGB]                = 24;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA]               = 32;
        m_FormatToBPP[TEXTURE_FORMAT_RGB_16BPP]          = 16;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA_16BPP]         = 16;
        m_FormatToBPP[TEXTURE_FORMAT_RGB_DXT1]           = 4;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA_DXT1]          = 4;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA_DXT3]          = 8;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA_DXT5]          = 8;
        m_FormatToBPP[TEXTURE_FORMAT_DEPTH]              = 24;
        m_FormatToBPP[TEXTURE_FORMAT_STENCIL]            = 8;
        m_FormatToBPP[TEXTURE_FORMAT_RGB_PVRTC_2BPPV1]   = 2;
        m_FormatToBPP[TEXTURE_FORMAT_RGB_PVRTC_4BPPV1]   = 4;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1]  = 2;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1]  = 4;
        m_FormatToBPP[TEXTURE_FORMAT_RGB_ETC1]           = 4;
        m_FormatToBPP[TEXTURE_FORMAT_RGB16F]             = 48;
        m_FormatToBPP[TEXTURE_FORMAT_RGB32F]             = 96;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA16F]            = 64;
        m_FormatToBPP[TEXTURE_FORMAT_RGBA32F]            = 128;
        m_FormatToBPP[TEXTURE_FORMAT_R16F]               = 16;
        m_FormatToBPP[TEXTURE_FORMAT_RG16F]              = 32;
        m_FormatToBPP[TEXTURE_FORMAT_R32F]               = 32;
        m_FormatToBPP[TEXTURE_FORMAT_RG32F]              = 64;
    }

    AttachmentToBufferType::AttachmentToBufferType()
    {
        memset(m_AttachmentToBufferType, 0x0, sizeof(m_AttachmentToBufferType));
        m_AttachmentToBufferType[ATTACHMENT_COLOR] = BUFFER_TYPE_COLOR_BIT;
        m_AttachmentToBufferType[ATTACHMENT_DEPTH] = BUFFER_TYPE_DEPTH_BIT;
        m_AttachmentToBufferType[ATTACHMENT_STENCIL] = BUFFER_TYPE_STENCIL_BIT;
    }

    HContext NewContext(const ContextParams& params)
    {
        GraphicsAdapter* next     = g_adapter_list;
        GraphicsAdapter* selected = next;
        while(next)
        {
            if (next->m_Priority < selected->m_Priority && next->m_IsSupportedCb())
            {
                selected = next;
            }

            next = next->m_Next;
        }

        assert(selected);
        g_functions = selected->m_RegisterCb();

        return g_functions.m_NewContext(params);
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
        ShaderDesc::Language language = GetShaderProgramLanguage(context);
        assert(shader_desc);
        for(uint32_t i = 0; i < shader_desc->m_Shaders.m_Count; ++i)
        {
            ShaderDesc::Shader* shader = &shader_desc->m_Shaders.m_Data[i];
            if(shader->m_Language == language)
            {
                return shader;
            }
        }
        return 0x0;
    }

    uint32_t GetTextureFormatBPP(TextureFormat format)
    {
        static TextureFormatToBPP g_TextureFormatToBPP;
        assert(format < TEXTURE_FORMAT_COUNT);
        return g_TextureFormatToBPP.m_FormatToBPP[format];
    }

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return g_functions.m_NewVertexBuffer(context, size, data, buffer_usage);
    }
}
