#ifndef DMSDK_GRAPHICS_H
#define DMSDK_GRAPHICS_H

namespace dmGraphics
{
    typedef struct Texture*      HTexture;
    typedef struct RenderTarget* HRenderTarget;

    enum HandleResult
    {
        HANDLE_RESULT_OK = 0,
        HANDLE_RESULT_NOT_AVAILABLE = -1,
        HANDLE_RESULT_ERROR = -2
    };

    enum RenderTargetAttachment
    {
        ATTACHMENT_COLOR     = 0,
        ATTACHMENT_DEPTH     = 1,
        ATTACHMENT_STENCIL   = 2,
        MAX_ATTACHMENT_COUNT = 3
    };

    HTexture GetRenderTargetAttachment(HRenderTarget render_target, RenderTargetAttachment attachment_type);
    HandleResult GetTextureHandle(HTexture texture, void** out_handle);
}

#endif // DMSDK_GRAPHICS_H
