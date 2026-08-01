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

    DM_DLLEXPORT FontRendererResult FontRendererCreate(const char*               name,
                                                       const uint8_t*            font_bytes,
                                                       uint32_t                  font_byte_count,
                                                       const FontRendererParams* params,
                                                       HFontRenderer*            renderer);
    DM_DLLEXPORT void               FontRendererDestroy(HFontRenderer renderer);

    DM_DLLEXPORT FontRendererResult FontRendererMeasure(HFontRenderer       renderer,
                                                        const uint32_t*     codepoints,
                                                        uint32_t            codepoint_count,
                                                        uint32_t            line_break,
                                                        float               width,
                                                        float               leading,
                                                        float               tracking,
                                                        FontRendererLayout* layout);

    DM_DLLEXPORT FontRendererResult FontRendererGenerateGlyph(HFontRenderer      renderer,
                                                              uint32_t           codepoint,
                                                              FontRendererGlyph* glyph);
    DM_DLLEXPORT void               FontRendererFreeGlyph(FontRendererGlyph* glyph);
    DM_DLLEXPORT FontRendererResult FontRendererDecodeImage(const uint8_t*     image_bytes,
                                                            uint32_t           image_byte_count,
                                                            FontRendererImage* image);
    DM_DLLEXPORT void               FontRendererFreeImage(FontRendererImage* image);

    DM_DLLEXPORT FontRendererResult FontRendererSetProperties(HFontRenderer                 renderer,
                                                              const FontRendererProperties* properties);
    DM_DLLEXPORT FontRendererResult FontRendererSetText(HFontRenderer   renderer,
                                                        const uint32_t* codepoints,
                                                        uint32_t        codepoint_count);
    DM_DLLEXPORT uint64_t           FontRendererHash(HFontRenderer renderer);
    DM_DLLEXPORT FontRendererResult FontRendererBeginBatch(HFontRenderer renderer);
    DM_DLLEXPORT FontRendererResult FontRendererGenerateTexture(HFontRenderer renderer,
                                                                uint64_t      known_atlas_version,
                                                                FontTexture*  texture);
    DM_DLLEXPORT void               FontRendererFreeTexture(FontTexture* texture);
    DM_DLLEXPORT FontRendererResult FontRendererGetVertexBufferSize(HFontRenderer renderer,
                                                                    uint32_t*     vertex_count,
                                                                    uint32_t*     vertex_buffer_size);
    DM_DLLEXPORT FontRendererResult FontRendererGetVertices(HFontRenderer renderer,
                                                            const float*  world_transform,
                                                            uint8_t*      vertex_buffer,
                                                            uint32_t      vertex_buffer_size);

#ifdef __cplusplus
}
#endif

#endif // DM_FONT_RENDERER_H
