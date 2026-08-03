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

#ifndef DM_FONT_RENDERER_H
#define DM_FONT_RENDERER_H

#include <stdint.h>

#include <dlib/shared_library.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct FontRendererContext* HFontRenderer;

    typedef enum FontRendererResult
    {
        FONT_RENDERER_RESULT_OK = 0,
        FONT_RENDERER_RESULT_INVALID_ARGUMENT = -1,
        FONT_RENDERER_RESULT_FONT_ERROR = -2,
        FONT_RENDERER_RESULT_TEXT_ERROR = -3,
        FONT_RENDERER_RESULT_GLYPH_ERROR = -4,
        FONT_RENDERER_RESULT_OUT_OF_MEMORY = -5,
    } FontRendererResult;

    typedef enum FontRendererLayer
    {
        FONT_RENDERER_LAYER_FACE = 1,
        FONT_RENDERER_LAYER_OUTLINE = 2,
        FONT_RENDERER_LAYER_SHADOW = 4,
    } FontRendererLayer;

    typedef struct FontRendererParams
    {
        float    m_Size;
        uint32_t m_AtlasWidth;
        uint32_t m_AtlasHeight;
        uint32_t m_CellPadding;
        float    m_SdfBasePadding;
        uint32_t m_SdfEdgeValue;
        float    m_SdfSpread;
        float    m_SdfOutline;
        float    m_SdfShadow;
        float    m_OutlineWidth;
        float    m_ShadowBlur;
        float    m_ShadowX;
        float    m_ShadowY;
        uint32_t m_LayerMask;
        uint32_t m_OutputBitmap;
        uint32_t m_Antialias;
        uint32_t m_HasOutline;
        uint32_t m_HasShadow;
        uint32_t m_UseTextShaping;
    } FontRendererParams;

    typedef struct FontRendererLayout
    {
        float    m_Width;
        float    m_Height;
        uint32_t m_LineCount;
        float    m_MaxAscent;
        float    m_MaxDescent;
    } FontRendererLayout;

    typedef struct FontRendererGlyph
    {
        uint32_t m_GlyphIndex;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Channels;
        float    m_Advance;
        float    m_LeftBearing;
        float    m_Ascent;
        float    m_Descent;
        uint8_t* m_Pixels;
        uint32_t m_PixelCount;
    } FontRendererGlyph;

    typedef struct FontRendererProperties
    {
        float    m_FaceColor[4];
        float    m_OutlineColor[4];
        float    m_ShadowColor[4];
        float    m_Width;
        float    m_Height;
        float    m_Leading;
        float    m_Tracking;
        float    m_SdfScale;
        uint32_t m_LineBreak;
        uint32_t m_Align;
        uint32_t m_VerticalAlign;
    } FontRendererProperties;

    typedef struct FontTexture
    {
        uint8_t* m_Pixels;
        uint64_t m_AtlasVersion;
        uint32_t m_PixelCount;
        uint32_t m_X;
        uint32_t m_Y;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Channels;
    } FontTexture;

    typedef struct FontRendererImage
    {
        uint8_t* m_Pixels;
        uint32_t m_PixelCount;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Channels;
    } FontRendererImage;

    /*#
     * Creates a persistent font renderer context.
     *
     * The context owns the loaded font, glyph cache, and texture atlas. The input
     * name and font data remain owned by the caller and only need to be valid for
     * the duration of this call. The parameters are retained for the lifetime of
     * the context.
     *
     * @name FontRendererCreate
     * @param name [type: const char*] Font resource name, used for diagnostics.
     * @param font_bytes [type: const uint8_t*] Font file contents.
     * @param font_byte_count [type: uint32_t] Size of the font file contents in bytes.
     * @param params [type: const FontRendererParams*] Immutable renderer configuration.
     * @param renderer [type: HFontRenderer*] Receives the created context on success.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererCreate(const char*               name,
                                                       const uint8_t*            font_bytes,
                                                       uint32_t                  font_byte_count,
                                                       const FontRendererParams* params,
                                                       HFontRenderer*            renderer);

    /*#
     * Destroys a font renderer context and all resources owned by it.
     * Passing a null handle is allowed.
     *
     * @name FontRendererDestroy
     * @param renderer [type: HFontRenderer] Context to destroy.
     */
    DM_DLLEXPORT void               FontRendererDestroy(HFontRenderer renderer);

    /*#
     * Measures UTF-32 text using the renderer's font and text-layout mode.
     * This operation does not modify the retained properties, text, glyph cache,
     * or texture atlas. A null codepoint pointer is allowed when the count is zero.
     *
     * @name FontRendererMeasure
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param codepoints [type: const uint32_t*] UTF-32 text to measure.
     * @param codepoint_count [type: uint32_t] Number of UTF-32 codepoints.
     * @param line_break [type: uint32_t] Non-zero to wrap text to the supplied width.
     * @param width [type: float] Maximum line width when line breaking is enabled.
     * @param leading [type: float] Line spacing used by the runtime text layout.
     * @param tracking [type: float] Character spacing used by the runtime text layout.
     * @param layout [type: FontRendererLayout*] Receives the measured layout.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererMeasure(HFontRenderer       renderer,
                                                        const uint32_t*     codepoints,
                                                        uint32_t            codepoint_count,
                                                        uint32_t            line_break,
                                                        float               width,
                                                        float               leading,
                                                        float               tracking,
                                                        FontRendererLayout* layout);

    /*#
     * Generates a standalone bitmap and metrics for one Unicode codepoint.
     * The generated glyph is not inserted into the context's glyph cache or atlas.
     * The returned pixel buffer is owned by the caller and must be released with
     * FontRendererFreeGlyph.
     *
     * @name FontRendererGenerateGlyph
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param codepoint [type: uint32_t] Unicode codepoint to generate.
     * @param glyph [type: FontRendererGlyph*] Receives the glyph bitmap and metrics.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererGenerateGlyph(HFontRenderer      renderer,
                                                              uint32_t           codepoint,
                                                              FontRendererGlyph* glyph);

    /*#
     * Releases the pixel buffer in a generated glyph and clears the structure.
     * Passing a null pointer is allowed.
     *
     * @name FontRendererFreeGlyph
     * @param glyph [type: FontRendererGlyph*] Glyph returned by FontRendererGenerateGlyph.
     */
    DM_DLLEXPORT void               FontRendererFreeGlyph(FontRendererGlyph* glyph);

    /*#
     * Decodes encoded image data into an uncompressed pixel buffer.
     * This utility does not require a renderer context. The returned pixel buffer
     * is owned by the caller and must be released with FontRendererFreeImage.
     *
     * @name FontRendererDecodeImage
     * @param image_bytes [type: const uint8_t*] Encoded image contents.
     * @param image_byte_count [type: uint32_t] Size of the encoded image in bytes.
     * @param image [type: FontRendererImage*] Receives the decoded image.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererDecodeImage(const uint8_t*     image_bytes,
                                                            uint32_t           image_byte_count,
                                                            FontRendererImage* image);

    /*#
     * Releases the pixel buffer in a decoded image and clears the structure.
     * Passing a null pointer is allowed.
     *
     * @name FontRendererFreeImage
     * @param image [type: FontRendererImage*] Image returned by FontRendererDecodeImage.
     */
    DM_DLLEXPORT void               FontRendererFreeImage(FontRendererImage* image);

    /*#
     * Replaces the retained render and layout properties.
     * The values are copied into the context and immediately included in
     * FontRendererHash. No texture or vertex data is generated by this call.
     *
     * @name FontRendererSetProperties
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param properties [type: const FontRendererProperties*] Properties to copy.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererSetProperties(HFontRenderer                 renderer,
                                                              const FontRendererProperties* properties);

    /*#
     * Replaces the retained UTF-32 text.
     * The codepoints are copied into the context and immediately included in
     * FontRendererHash. A null pointer is allowed when the count is zero. No
     * texture or vertex data is generated by this call.
     *
     * @name FontRendererSetText
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param codepoints [type: const uint32_t*] UTF-32 text to copy.
     * @param codepoint_count [type: uint32_t] Number of UTF-32 codepoints.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererSetText(HFontRenderer   renderer,
                                                        const uint32_t* codepoints,
                                                        uint32_t        codepoint_count);

    /*#
     * Returns a hash of the retained properties and text.
     * The hash changes when either value is replaced and can be used by a caller
     * to decide whether layout or vertex data needs to be regenerated. Atlas state
     * is tracked separately by FontTexture.m_AtlasVersion.
     *
     * @name FontRendererHash
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @return hash [type: uint64_t] Current retained-state hash, or zero for a null context.
     */
    DM_DLLEXPORT uint64_t           FontRendererHash(HFontRenderer renderer);

    /*#
     * Starts preparation of a render batch.
     * Glyphs touched after this call are protected from atlas eviction for the
     * remainder of the batch. Call this once before preparing all text entries
     * whose generated texture and vertices will be rendered together.
     *
     * @name FontRendererBeginBatch
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererBeginBatch(HFontRenderer renderer);

    /*#
     * Ensures the retained text's glyphs are present in the atlas and returns the
     * texture update needed by the caller.
     *
     * Pass zero for the first known atlas version. If the supplied version already
     * matches the resulting atlas, the function succeeds with no pixel data. The
     * returned atlas version must be retained by the caller for subsequent calls.
     * The returned pixel buffer, when present, is owned by the caller and must be
     * released with FontRendererFreeTexture. Generate the texture before querying
     * or writing vertices so their glyphs are available in the atlas.
     *
     * @name FontRendererGenerateTexture
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param known_atlas_version [type: uint64_t] Atlas version currently held by the caller.
     * @param texture [type: FontTexture*] Receives an atlas update and the resulting version.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererGenerateTexture(HFontRenderer renderer,
                                                                uint64_t      known_atlas_version,
                                                                FontTexture*  texture);

    /*#
     * Releases the pixel buffer in a texture update and clears the structure.
     * Passing a null pointer is allowed.
     *
     * @name FontRendererFreeTexture
     * @param texture [type: FontTexture*] Texture returned by FontRendererGenerateTexture.
     */
    DM_DLLEXPORT void               FontRendererFreeTexture(FontTexture* texture);

    /*#
     * Calculates the storage required for vertices of the retained text.
     * Call FontRendererGenerateTexture first so all drawable glyphs are present in
     * the atlas. This function does not allocate vertex storage. The size includes
     * all enabled face, outline, and shadow layers.
     *
     * @name FontRendererGetVertexBufferSize
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param vertex_count [type: uint32_t*] Receives the number of FontGlyphVertex elements.
     * @param vertex_buffer_size [type: uint32_t*] Receives the required size in bytes.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererGetVertexBufferSize(HFontRenderer renderer,
                                                                    uint32_t*     vertex_count,
                                                                    uint32_t*     vertex_buffer_size);

    /*#
     * Writes vertices for the retained text into caller-owned memory.
     * The buffer must be at least the size returned by
     * FontRendererGetVertexBufferSize for the current retained state and atlas.
     * A null buffer is allowed only when the required size is zero. Positions are
     * transformed by the supplied column-major 4x4 world transform.
     *
     * @name FontRendererGetVertices
     * @param renderer [type: HFontRenderer] Font renderer context.
     * @param world_transform [type: const float*] Column-major 4x4 world transform.
     * @param vertex_buffer [type: uint8_t*] Caller-owned output buffer.
     * @param vertex_buffer_size [type: uint32_t] Available output-buffer size in bytes.
     * @return result [type: FontRendererResult] Result of the operation.
     */
    DM_DLLEXPORT FontRendererResult FontRendererGetVertices(HFontRenderer renderer,
                                                            const float*  world_transform,
                                                            uint8_t*      vertex_buffer,
                                                            uint32_t      vertex_buffer_size);

#ifdef __cplusplus
}
#endif

#endif // DM_FONT_RENDERER_H
