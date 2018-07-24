#include "graphics.h"
#include <string.h>
#include <assert.h>

namespace dmGraphics
{
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
}
