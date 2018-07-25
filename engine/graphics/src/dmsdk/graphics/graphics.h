#ifndef DMSDK_GRAPHICS_H
#define DMSDK_GRAPHICS_H

namespace dmGraphics
{
    /*#
     * [file:<dmsdk/graphics/graphics.h>]
     *
     * @document
     * @name Graphics
     * @namespace dmGraphics
     */

    /*# HTexture type definition
     *
     * ```cpp
     * typedef struct Texture* HTexture;
     * ```
     *
     * @typedef
     * @name dmGraphics::HTexture
     *
     */
    typedef struct Texture*      HTexture;

    /*# HRenderTarget type definition
     *
     * ```cpp
     * typedef struct RenderTarget* HRenderTarget;
     * ```
     *
     * @typedef
     * @name dmGraphics::HRenderTarget
     *
     */
    typedef struct RenderTarget* HRenderTarget;

    /*# handleResult enumeration
     *
     * HandleResult enumeration.
     *
     * @enum
     * @name HandleResult
     * @member dmGraphics::HANDLE_RESULT_OK
     * @member dmGraphics::HANDLE_RESULT_NOT_AVAILABLE
     * @member dmGraphics::HANDLE_RESULT_ERROR
     *
     */
    enum HandleResult
    {
        HANDLE_RESULT_OK = 0,
        HANDLE_RESULT_NOT_AVAILABLE = -1,
        HANDLE_RESULT_ERROR = -2
    };

    /*# renderTargetAttachment enumeration
     *
     * RenderTargetAttachment enumeration.
     *
     * @enum
     * @name RenderTargetAttachment
     * @member dmGraphics::ATTACHMENT_COLOR
     * @member dmGraphics::ATTACHMENT_DEPTH
     * @member dmGraphics::ATTACHMENT_STENCIL
     * @member dmGraphics::MAX_ATTACHMENT_COUNT
     *
     */
    enum RenderTargetAttachment
    {
        ATTACHMENT_COLOR     = 0,
        ATTACHMENT_DEPTH     = 1,
        ATTACHMENT_STENCIL   = 2,
        MAX_ATTACHMENT_COUNT = 3
    };

    /*# get texture attachment for a render target
     *
     * Get texture attachment for a render target, returns zero if not available.
     *
     * ```cpp
     * dmGraphics::HTexture color_texture = dmGraphics::GetRenderTargetAttachment(render_target, dmGraphics::ATTACHMENT_COLOR);
     * if (color_texture == 0x0) {
     *    dmLogError("Could not get color attachment.");
     * }
     * ```
     * @name dmGraphics::GetRenderTargetAttachment
     * @param render_target [type:dmGraphics::HRenderTarget] Render target handle
     * @param attachment_type [type:dmGraphics::RenderTargetAttachment] Attachment type
     * @return value [type:dmGraphics::HTexture] Texture
     */
    HTexture GetRenderTargetAttachment(HRenderTarget render_target, RenderTargetAttachment attachment_type);

    /*# get native texture handle from a texture
     *
     * Get the native texture handle from a texture. On OpenGL platforms, `out_handle` will point to a `GLuint*`.
     *
     * ```cpp
     * GLuint* gl_texture = 0x0;
     * dmGraphics::HandleResult res = dmGraphics::GetTextureHandle(texture, (void**)&gl_texture);
     * if (res == dmGraphics::HANDLE_RESULT_OK) {
     *     // ... do something with (*gl_texture)
     * } else {
     *     dmLogError("Could not get native texture handle.");
     * }
     * ```
     * @name dmGraphics::GetTextureHandle
     * @param texture [type:dmGraphics::HTexture] Texture
     * @param out_handle [type:void**] Pointer to where native handle should be stored
     * @return value [type:dmGraphics::HandleResult] HANDLE_RESULT_OK on success
     */
    HandleResult GetTextureHandle(HTexture texture, void** out_handle);
}

#endif // DMSDK_GRAPHICS_H
